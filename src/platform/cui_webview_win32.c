/* WebView2's SDK exposes a C COM API; the library remains C99. */
#include "cui_webview.h"
#ifndef COBJMACROS
#define COBJMACROS
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <stddef.h>
#include <wchar.h>
#include <WebView2.h>

typedef struct {
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler environment_handler;
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler controller_handler;
    ICoreWebView2AcceleratorKeyPressedEventHandler key_handler;
    LONG refs;
    cui__webview *owner;
    HWND host, parent;
    ICoreWebView2Controller *controller;
    ICoreWebView2 *web;
    EventRegistrationToken key_token;
    bool key_registered;
} cui__webview_win32;

/* Pending callbacks retain this allocation, never the app or public handle. */
static ULONG cui__webview_win_release(cui__webview_win32 *w) {
    ULONG refs = (ULONG)InterlockedDecrement(&w->refs);
    if (!refs) free(w);
    return refs;
}

static HRESULT cui__webview_win_query(cui__webview_win32 *w, REFIID id, void **out) {
    static const GUID unknown = {0, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(id, &unknown) || IsEqualGUID(id, &IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler))
        *out = &w->environment_handler;
    else if (IsEqualGUID(id, &IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) *out = &w->controller_handler;
    else if (IsEqualGUID(id, &IID_ICoreWebView2AcceleratorKeyPressedEventHandler)) *out = &w->key_handler;
    else return E_NOINTERFACE;
    InterlockedIncrement(&w->refs);
    return S_OK;
}

#define CUI__WEBVIEW_HANDLER(name, type, member) \
    static cui__webview_win32 *cui__wv_##name##_owner(type *self) { \
        return (cui__webview_win32 *)((char *)self - offsetof(cui__webview_win32, member)); \
    } \
    static HRESULT STDMETHODCALLTYPE cui__wv_##name##_query(type *self, REFIID id, void **out) { \
        return cui__webview_win_query(cui__wv_##name##_owner(self), id, out); \
    } \
    static ULONG STDMETHODCALLTYPE cui__wv_##name##_addref(type *self) { \
        return (ULONG)InterlockedIncrement(&cui__wv_##name##_owner(self)->refs); \
    } \
    static ULONG STDMETHODCALLTYPE cui__wv_##name##_release(type *self) { \
        return cui__webview_win_release(cui__wv_##name##_owner(self)); \
    }

CUI__WEBVIEW_HANDLER(env, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
    environment_handler)
CUI__WEBVIEW_HANDLER(ctrl, ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
    controller_handler)
CUI__WEBVIEW_HANDLER(key, ICoreWebView2AcceleratorKeyPressedEventHandler,
    key_handler)
#undef CUI__WEBVIEW_HANDLER

static HRESULT STDMETHODCALLTYPE cui__wv_key_invoke(ICoreWebView2AcceleratorKeyPressedEventHandler *self,
    ICoreWebView2Controller *sender, ICoreWebView2AcceleratorKeyPressedEventArgs *args) {
    (void)sender;
    cui__webview_win32 *w = cui__wv_key_owner(self);
    UINT key = 0;
    COREWEBVIEW2_KEY_EVENT_KIND kind;
    if (!w->owner || FAILED(ICoreWebView2AcceleratorKeyPressedEventArgs_get_KeyEventKind(args, &kind)) ||
        FAILED(ICoreWebView2AcceleratorKeyPressedEventArgs_get_VirtualKey(args, &key))) return S_OK;
    if ((kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN || kind == COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN) &&
        key == VK_TAB && (GetKeyState(VK_CONTROL) & 0x8000)) {
        ICoreWebView2AcceleratorKeyPressedEventArgs_put_Handled(args, TRUE);
        SetFocus(w->parent);
        cui__webview_return_focus(w->owner, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE cui__wv_ctrl_invoke(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self,
    HRESULT error, ICoreWebView2Controller *controller) {
    cui__webview_win32 *w = cui__wv_ctrl_owner(self);
    if (!w->owner) { if (controller) ICoreWebView2Controller_Close(controller); return S_OK; }
    cui__webview *v = w->owner;
    v->surface.status = CUI_WEBVIEW_ERROR;
    if (SUCCEEDED(error) && controller) {
        w->controller = controller;
        ICoreWebView2Controller_AddRef(controller);
        ICoreWebView2Controller_put_IsVisible(controller, FALSE);
        if (SUCCEEDED(ICoreWebView2Controller_get_CoreWebView2(controller, &w->web)) && w->web &&
            SUCCEEDED(ICoreWebView2Controller_add_AcceleratorKeyPressed(controller, &w->key_handler, &w->key_token))) {
            w->key_registered = true;
            v->surface.status = CUI_WEBVIEW_READY;
            if (v->content && !cui__webview_native_load(v)) v->surface.status = CUI_WEBVIEW_ERROR;
        }
    }
    cui_request_redraw(v->surface.ui);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE cui__wv_env_invoke(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self,
    HRESULT error, ICoreWebView2Environment *environment) {
    cui__webview_win32 *w = cui__wv_env_owner(self);
    if (!w->owner) return S_OK;
    if (FAILED(error) || !environment ||
        FAILED(ICoreWebView2Environment_CreateCoreWebView2Controller(environment, w->host, &w->controller_handler))) {
        w->owner->surface.status = CUI_WEBVIEW_ERROR;
        cui_request_redraw(w->owner->surface.ui);
    }
    return S_OK;
}

static CONST_VTBL ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl cui__wv_env_vtable = {
    cui__wv_env_query, cui__wv_env_addref, cui__wv_env_release, cui__wv_env_invoke
};
static CONST_VTBL ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl cui__wv_ctrl_vtable = {
    cui__wv_ctrl_query, cui__wv_ctrl_addref, cui__wv_ctrl_release, cui__wv_ctrl_invoke
};
static CONST_VTBL ICoreWebView2AcceleratorKeyPressedEventHandlerVtbl cui__wv_key_vtable = {
    cui__wv_key_query, cui__wv_key_addref, cui__wv_key_release, cui__wv_key_invoke
};

void *cui__webview_native_create(cui__webview *v) {
    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) return NULL;
    cui__webview_win32 *w = calloc(1, sizeof *w);
    if (!w) { CoUninitialize(); return NULL; }
    w->refs = 1; w->owner = v;
    w->environment_handler.lpVtbl = &cui__wv_env_vtable;
    w->controller_handler.lpVtbl = &cui__wv_ctrl_vtable;
    w->key_handler.lpVtbl = &cui__wv_key_vtable;
    w->parent = (HWND)cui_app_native_window(v->app);
    w->host = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_CLIPCHILDREN, 0, 0, 1, 1,
        w->parent, NULL, GetModuleHandleW(NULL), NULL);
    /* Keep browser data out of the (often read-only) executable directory. */
    WCHAR profile[MAX_PATH], executable[MAX_PATH];
    HRESULT error = E_FAIL;
    DWORD count = GetModuleFileNameW(NULL, executable, MAX_PATH);
    if (w->host && count && count < MAX_PATH &&
        SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, profile)) &&
        wcslen(profile) + 48 < MAX_PATH) {
        wcscat(profile, L"\\Better-C-UI");
        if (CreateDirectoryW(profile, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
            size_t at = wcslen(profile);
            swprintf(profile + at, MAX_PATH - at, L"\\WebView2-%08x", (unsigned)cui__hash(executable, count * sizeof(WCHAR), 0));
            error = CreateCoreWebView2EnvironmentWithOptions(NULL, profile, NULL, &w->environment_handler);
        }
    }
    if (FAILED(error)) {
        w->owner = NULL;
        if (w->host) DestroyWindow(w->host);
        cui__webview_win_release(w);
        CoUninitialize();
        return NULL;
    }
    return w;
}

void cui__webview_native_destroy(cui__webview *v) {
    cui__webview_win32 *w = v->native;
    w->owner = NULL;
    if (w->controller) {
        if (w->key_registered) ICoreWebView2Controller_remove_AcceleratorKeyPressed(w->controller, w->key_token);
        ICoreWebView2Controller_Close(w->controller);
    }
    if (w->web) ICoreWebView2_Release(w->web);
    if (w->controller) ICoreWebView2Controller_Release(w->controller);
    if (IsChild(w->host, GetFocus())) SetFocus(w->parent);
    DestroyWindow(w->host);
    cui__webview_win_release(w);
    CoUninitialize();
    v->native = NULL;
}

bool cui__webview_native_load(cui__webview *v) {
    cui__webview_win32 *w = v->native;
    if (!w->web || !w->key_registered) return false;
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, v->content, -1, NULL, 0);
    if (!count) return false;
    WCHAR *text = malloc((size_t)count * sizeof *text);
    if (!text) return false;
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, v->content, -1, text, count);
    HRESULT error = v->html ? ICoreWebView2_NavigateToString(w->web, text) : ICoreWebView2_Navigate(w->web, text);
    free(text);
    return SUCCEEDED(error);
}

bool cui__webview_native_history(cui__webview *v, bool forward, bool move) {
    cui__webview_win32 *w = v->native;
    if (!w->web) return false;
    BOOL can = FALSE;
    HRESULT error = forward ? ICoreWebView2_get_CanGoForward(w->web, &can) : ICoreWebView2_get_CanGoBack(w->web, &can);
    if (FAILED(error) || !can) return false;
    if (!move) return true;
    return SUCCEEDED(forward ? ICoreWebView2_GoForward(w->web) : ICoreWebView2_GoBack(w->web));
}

void cui__webview_native_place(cui__webview *v, bool visible) {
    cui__webview_win32 *w = v->native;
    cui_rect r = v->surface.rect;
    float scale = v->surface.ui->scale;
    int width = CUI_MAX(1, (int)lroundf(r.w * scale)), height = CUI_MAX(1, (int)lroundf(r.h * scale));
    SetWindowPos(w->host, NULL, (int)lroundf(r.x * scale), (int)lroundf(r.y * scale), width, height, SWP_NOACTIVATE | SWP_NOZORDER);
    if (w->controller) {
        RECT bounds = {0, 0, width, height};
        ICoreWebView2Controller_put_Bounds(w->controller, bounds);
        ICoreWebView2Controller_NotifyParentWindowPositionChanged(w->controller);
        ICoreWebView2Controller_put_IsVisible(w->controller, visible);
        if (visible && v->surface.focus_requested) ICoreWebView2Controller_MoveFocus(w->controller, COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }
    if (!visible && IsChild(w->host, GetFocus())) SetFocus(w->parent);
    ShowWindow(w->host, visible ? SW_SHOWNA : SW_HIDE);
    if (visible && IsChild(w->host, GetFocus())) cui__webview_claim_focus(v);
}

void cui__webview_native_pump(void) {}
