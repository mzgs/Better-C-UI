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

enum {
    CUI__WV_WIN_START = 1 << 0, CUI__WV_WIN_FINISH = 1 << 1, CUI__WV_WIN_SOURCE = 1 << 2,
    CUI__WV_WIN_PROGRESS = 1 << 3, CUI__WV_WIN_MESSAGE = 1 << 4, CUI__WV_WIN_FRAME = 1 << 5
};

typedef struct {
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler environment_handler;
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler controller_handler;
    ICoreWebView2AcceleratorKeyPressedEventHandler key_handler;
    ICoreWebView2NavigationStartingEventHandler start_handler;
    ICoreWebView2NavigationStartingEventHandler frame_handler;
    ICoreWebView2NavigationCompletedEventHandler finish_handler;
    ICoreWebView2SourceChangedEventHandler source_handler;
    ICoreWebView2ContentLoadingEventHandler progress_handler;
    ICoreWebView2WebMessageReceivedEventHandler message_handler;
    ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler script_handler;
    LONG refs;
    cui__webview *owner;
    HWND host, parent;
    ICoreWebView2Controller *controller;
    ICoreWebView2 *web;
    ICoreWebView2Environment2 *environment;
    UINT64 navigation;
    LPWSTR default_user_agent;
    char *navigation_url;
    unsigned events;
    EventRegistrationToken start_token, finish_token, source_token, progress_token, message_token;
    EventRegistrationToken key_token, frame_token;
    bool key_registered;
} cui__webview_win32;

static WCHAR *cui__webview_win_wide(const char *text) {
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (!count) return NULL;
    WCHAR *wide = malloc((size_t)count * sizeof *wide);
    if (wide) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, count);
    return wide;
}
static char *cui__webview_win_utf8(LPCWSTR wide) {
    if (!wide) return NULL;
    int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, NULL, 0, NULL, NULL);
    if (!count) return NULL;
    char *text = malloc((size_t)count);
    if (text) WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, text, count, NULL, NULL);
    return text;
}

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
    else if (IsEqualGUID(id, &IID_ICoreWebView2NavigationStartingEventHandler)) *out = &w->start_handler;
    else if (IsEqualGUID(id, &IID_ICoreWebView2NavigationCompletedEventHandler)) *out = &w->finish_handler;
    else if (IsEqualGUID(id, &IID_ICoreWebView2SourceChangedEventHandler)) *out = &w->source_handler;
    else if (IsEqualGUID(id, &IID_ICoreWebView2ContentLoadingEventHandler)) *out = &w->progress_handler;
    else if (IsEqualGUID(id, &IID_ICoreWebView2WebMessageReceivedEventHandler)) *out = &w->message_handler;
    else if (IsEqualGUID(id, &IID_ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler)) *out = &w->script_handler;
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
CUI__WEBVIEW_HANDLER(start, ICoreWebView2NavigationStartingEventHandler, start_handler)
CUI__WEBVIEW_HANDLER(finish, ICoreWebView2NavigationCompletedEventHandler, finish_handler)
CUI__WEBVIEW_HANDLER(source, ICoreWebView2SourceChangedEventHandler, source_handler)
CUI__WEBVIEW_HANDLER(progress, ICoreWebView2ContentLoadingEventHandler, progress_handler)
CUI__WEBVIEW_HANDLER(message, ICoreWebView2WebMessageReceivedEventHandler, message_handler)
CUI__WEBVIEW_HANDLER(script, ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler, script_handler)
#undef CUI__WEBVIEW_HANDLER

/* Same COM interface as start_handler, but a separate callback identity. */
static cui__webview_win32 *cui__wv_frame_owner(ICoreWebView2NavigationStartingEventHandler *self) {
    return (cui__webview_win32 *)((char *)self - offsetof(cui__webview_win32, frame_handler));
}
static HRESULT STDMETHODCALLTYPE cui__wv_frame_query(ICoreWebView2NavigationStartingEventHandler *self, REFIID id, void **out) {
    static const GUID unknown = {0, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(id, &unknown) && !IsEqualGUID(id, &IID_ICoreWebView2NavigationStartingEventHandler)) return E_NOINTERFACE;
    *out = self; InterlockedIncrement(&cui__wv_frame_owner(self)->refs); return S_OK;
}
static ULONG STDMETHODCALLTYPE cui__wv_frame_addref(ICoreWebView2NavigationStartingEventHandler *self) {
    return (ULONG)InterlockedIncrement(&cui__wv_frame_owner(self)->refs);
}
static ULONG STDMETHODCALLTYPE cui__wv_frame_release(ICoreWebView2NavigationStartingEventHandler *self) {
    return cui__webview_win_release(cui__wv_frame_owner(self));
}

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

static void cui__webview_win_event(cui__webview_win32 *w, cui_webview_event_type type, const char *message, float progress, int code) {
    if (!w->owner) return;
    LPWSTR wide = NULL;
    ICoreWebView2_get_Source(w->web, &wide);
    char *url = cui__webview_win_utf8(wide);
    CoTaskMemFree(wide);
    cui__webview_emit(w->owner, type, type == CUI_WEBVIEW_LOAD_ERROR ? w->navigation_url : url, message, progress, code);
    free(url);
}
static HRESULT STDMETHODCALLTYPE cui__wv_start_invoke(ICoreWebView2NavigationStartingEventHandler *self,
    ICoreWebView2 *sender, ICoreWebView2NavigationStartingEventArgs *args) {
    (void)sender;
    cui__webview_win32 *w = cui__wv_start_owner(self);
    if (!w->owner) return S_OK;
    LPWSTR wide = NULL;
    ICoreWebView2NavigationStartingEventArgs_get_Uri(args, &wide);
    char *url = cui__webview_win_utf8(wide); CoTaskMemFree(wide);
    if (!cui__webview_decide_navigation(w->owner, url)) {
        ICoreWebView2NavigationStartingEventArgs_put_Cancel(args, TRUE);
        free(url); return S_OK;
    }
    ICoreWebView2NavigationStartingEventArgs_get_NavigationId(args, &w->navigation);
    free(w->navigation_url); w->navigation_url = url;
    cui__webview_emit(w->owner, CUI_WEBVIEW_LOAD_STARTED, url, NULL, 0, 0);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE cui__wv_frame_invoke(ICoreWebView2NavigationStartingEventHandler *self,
    ICoreWebView2 *sender, ICoreWebView2NavigationStartingEventArgs *args) {
    (void)sender;
    cui__webview_win32 *w = cui__wv_frame_owner(self);
    LPWSTR wide = NULL;
    ICoreWebView2NavigationStartingEventArgs_get_Uri(args, &wide);
    char *url = cui__webview_win_utf8(wide); CoTaskMemFree(wide);
    if (!w->owner || !cui__webview_decide_navigation(w->owner, url))
        ICoreWebView2NavigationStartingEventArgs_put_Cancel(args, TRUE);
    free(url);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE cui__wv_finish_invoke(ICoreWebView2NavigationCompletedEventHandler *self,
    ICoreWebView2 *sender, ICoreWebView2NavigationCompletedEventArgs *args) {
    (void)sender;
    cui__webview_win32 *w = cui__wv_finish_owner(self);
    UINT64 navigation = 0;
    ICoreWebView2NavigationCompletedEventArgs_get_NavigationId(args, &navigation);
    if (!w->owner || navigation != w->navigation) return S_OK;
    BOOL success = FALSE;
    COREWEBVIEW2_WEB_ERROR_STATUS code = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
    ICoreWebView2NavigationCompletedEventArgs_get_IsSuccess(args, &success);
    ICoreWebView2NavigationCompletedEventArgs_get_WebErrorStatus(args, &code);
    char message[100]; snprintf(message, sizeof message, "WebView2 navigation failed (WebErrorStatus %d)", (int)code);
    cui__webview_win_event(w, success ? CUI_WEBVIEW_LOAD_FINISHED : CUI_WEBVIEW_LOAD_ERROR, success ? NULL : message, success ? 1 : w->owner->load.progress, success ? 0 : (int)code);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE cui__wv_source_invoke(ICoreWebView2SourceChangedEventHandler *self,
    ICoreWebView2 *sender, ICoreWebView2SourceChangedEventArgs *args) {
    (void)sender; (void)args;
    cui__webview_win32 *w = cui__wv_source_owner(self);
    if (w->owner) cui__webview_win_event(w, CUI_WEBVIEW_URL_CHANGED, NULL, w->owner->load.progress, 0);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE cui__wv_progress_invoke(ICoreWebView2ContentLoadingEventHandler *self,
    ICoreWebView2 *sender, ICoreWebView2ContentLoadingEventArgs *args) {
    (void)sender; (void)args;
    cui__webview_win32 *w = cui__wv_progress_owner(self);
    if (w->owner && w->owner->load.state == CUI_WEBVIEW_LOADING) cui__webview_win_event(w, CUI_WEBVIEW_LOAD_PROGRESS, NULL, 0.5f, 0);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE cui__wv_message_invoke(ICoreWebView2WebMessageReceivedEventHandler *self,
    ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) {
    (void)sender;
    cui__webview_win32 *w = cui__wv_message_owner(self);
    if (!w->owner) return S_OK;
    LPWSTR source = NULL, body = NULL;
    if (FAILED(ICoreWebView2WebMessageReceivedEventArgs_TryGetWebMessageAsString(args, &body))) return S_OK;
    ICoreWebView2WebMessageReceivedEventArgs_get_Source(args, &source);
    char *url = cui__webview_win_utf8(source), *message = cui__webview_win_utf8(body);
    CoTaskMemFree(source); CoTaskMemFree(body);
    if (message) cui__webview_emit(w->owner, CUI_WEBVIEW_MESSAGE, url, message, w->owner->load.progress, 0);
    free(url); free(message);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE cui__wv_script_invoke(ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler *self,
    HRESULT error, LPCWSTR id) {
    (void)id;
    cui__webview_win32 *w = cui__wv_script_owner(self);
    if (!w->owner) return S_OK;
    cui__webview *v = w->owner;
    v->surface.status = SUCCEEDED(error) ? CUI_WEBVIEW_READY : CUI_WEBVIEW_ERROR;
    if (SUCCEEDED(error) && ((v->user_agent && !cui__webview_native_user_agent(v)) || (v->content && !cui__webview_native_load(v)))) v->surface.status = CUI_WEBVIEW_ERROR;
    cui_request_redraw(v->surface.ui);
    return S_OK;
}

static bool cui__webview_win_events(cui__webview_win32 *w) {
    if (FAILED(ICoreWebView2_add_NavigationStarting(w->web, &w->start_handler, &w->start_token))) return false;
    w->events |= CUI__WV_WIN_START;
    if (FAILED(ICoreWebView2_add_FrameNavigationStarting(w->web, &w->frame_handler, &w->frame_token))) return false;
    w->events |= CUI__WV_WIN_FRAME;
    if (FAILED(ICoreWebView2_add_NavigationCompleted(w->web, &w->finish_handler, &w->finish_token))) return false;
    w->events |= CUI__WV_WIN_FINISH;
    if (FAILED(ICoreWebView2_add_SourceChanged(w->web, &w->source_handler, &w->source_token))) return false;
    w->events |= CUI__WV_WIN_SOURCE;
    if (FAILED(ICoreWebView2_add_ContentLoading(w->web, &w->progress_handler, &w->progress_token))) return false;
    w->events |= CUI__WV_WIN_PROGRESS;
    if (FAILED(ICoreWebView2_add_WebMessageReceived(w->web, &w->message_handler, &w->message_token))) return false;
    w->events |= CUI__WV_WIN_MESSAGE;
    return SUCCEEDED(ICoreWebView2_AddScriptToExecuteOnDocumentCreated(w->web,
        L"if(window===window.top)window.cui=Object.freeze({postMessage:function(message){if(typeof message!=='string')throw new TypeError('Expected a string');window.chrome.webview.postMessage(message);}});", &w->script_handler));
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
            v->surface.status = CUI_WEBVIEW_STARTING;
            if (!cui__webview_win_events(w)) v->surface.status = CUI_WEBVIEW_ERROR;
        }
    }
    cui_request_redraw(v->surface.ui);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE cui__wv_env_invoke(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self,
    HRESULT error, ICoreWebView2Environment *environment) {
    cui__webview_win32 *w = cui__wv_env_owner(self);
    if (!w->owner) return S_OK;
    if (SUCCEEDED(error) && environment)
        ICoreWebView2Environment_QueryInterface(environment, &IID_ICoreWebView2Environment2, (void **)&w->environment);
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

static CONST_VTBL ICoreWebView2NavigationStartingEventHandlerVtbl cui__wv_start_vtable = {
    cui__wv_start_query, cui__wv_start_addref, cui__wv_start_release, cui__wv_start_invoke };
static CONST_VTBL ICoreWebView2NavigationStartingEventHandlerVtbl cui__wv_frame_vtable = {
    cui__wv_frame_query, cui__wv_frame_addref, cui__wv_frame_release, cui__wv_frame_invoke };
static CONST_VTBL ICoreWebView2NavigationCompletedEventHandlerVtbl cui__wv_finish_vtable = {
    cui__wv_finish_query, cui__wv_finish_addref, cui__wv_finish_release, cui__wv_finish_invoke };
static CONST_VTBL ICoreWebView2SourceChangedEventHandlerVtbl cui__wv_source_vtable = {
    cui__wv_source_query, cui__wv_source_addref, cui__wv_source_release, cui__wv_source_invoke };
static CONST_VTBL ICoreWebView2ContentLoadingEventHandlerVtbl cui__wv_progress_vtable = {
    cui__wv_progress_query, cui__wv_progress_addref, cui__wv_progress_release, cui__wv_progress_invoke };
static CONST_VTBL ICoreWebView2WebMessageReceivedEventHandlerVtbl cui__wv_message_vtable = {
    cui__wv_message_query, cui__wv_message_addref, cui__wv_message_release, cui__wv_message_invoke };
static CONST_VTBL ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandlerVtbl cui__wv_script_vtable = {
    cui__wv_script_query, cui__wv_script_addref, cui__wv_script_release, cui__wv_script_invoke };

void *cui__webview_native_create(cui__webview *v) {
    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) return NULL;
    cui__webview_win32 *w = calloc(1, sizeof *w);
    if (!w) { CoUninitialize(); return NULL; }
    w->refs = 1; w->owner = v;
    w->environment_handler.lpVtbl = &cui__wv_env_vtable;
    w->controller_handler.lpVtbl = &cui__wv_ctrl_vtable;
    w->key_handler.lpVtbl = &cui__wv_key_vtable;
    w->start_handler.lpVtbl = &cui__wv_start_vtable;
    w->frame_handler.lpVtbl = &cui__wv_frame_vtable;
    w->finish_handler.lpVtbl = &cui__wv_finish_vtable;
    w->source_handler.lpVtbl = &cui__wv_source_vtable;
    w->progress_handler.lpVtbl = &cui__wv_progress_vtable;
    w->message_handler.lpVtbl = &cui__wv_message_vtable;
    w->script_handler.lpVtbl = &cui__wv_script_vtable;
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
    if (w->web) {
        if (w->events & CUI__WV_WIN_START) ICoreWebView2_remove_NavigationStarting(w->web, w->start_token);
        if (w->events & CUI__WV_WIN_FRAME) ICoreWebView2_remove_FrameNavigationStarting(w->web, w->frame_token);
        if (w->events & CUI__WV_WIN_FINISH) ICoreWebView2_remove_NavigationCompleted(w->web, w->finish_token);
        if (w->events & CUI__WV_WIN_SOURCE) ICoreWebView2_remove_SourceChanged(w->web, w->source_token);
        if (w->events & CUI__WV_WIN_PROGRESS) ICoreWebView2_remove_ContentLoading(w->web, w->progress_token);
        if (w->events & CUI__WV_WIN_MESSAGE) ICoreWebView2_remove_WebMessageReceived(w->web, w->message_token);
        ICoreWebView2_Release(w->web);
    }
    if (w->environment) ICoreWebView2Environment2_Release(w->environment);
    CoTaskMemFree(w->default_user_agent);
    free(w->navigation_url);
    if (w->controller) ICoreWebView2Controller_Release(w->controller);
    if (IsChild(w->host, GetFocus())) SetFocus(w->parent);
    DestroyWindow(w->host);
    cui__webview_win_release(w);
    CoUninitialize();
    v->native = NULL;
}

static HRESULT cui__webview_win_request(cui__webview *v, LPCWSTR url) {
    cui__webview_win32 *w = v->native;
    ICoreWebView2_2 *web = NULL;
    ICoreWebView2WebResourceRequest *request = NULL;
    ICoreWebView2HttpRequestHeaders *headers = NULL;
    HRESULT error = E_NOINTERFACE;
    if (w->environment && SUCCEEDED(ICoreWebView2_QueryInterface(w->web, &IID_ICoreWebView2_2, (void **)&web))) {
        error = ICoreWebView2Environment2_CreateWebResourceRequest(w->environment, url, L"GET", NULL, L"", &request);
        if (SUCCEEDED(error)) error = ICoreWebView2WebResourceRequest_get_Headers(request, &headers);
        for (int i = 0; SUCCEEDED(error) && i < v->header_count; i++) {
            WCHAR *name = cui__webview_win_wide(v->headers[i].name), *value = cui__webview_win_wide(v->headers[i].value);
            error = name && value ? ICoreWebView2HttpRequestHeaders_SetHeader(headers, name, value) : E_OUTOFMEMORY;
            free(name); free(value);
        }
        if (SUCCEEDED(error)) error = ICoreWebView2_2_NavigateWithWebResourceRequest(web, request);
    }
    if (headers) ICoreWebView2HttpRequestHeaders_Release(headers);
    if (request) ICoreWebView2WebResourceRequest_Release(request);
    if (web) ICoreWebView2_2_Release(web);
    return error;
}
bool cui__webview_native_load(cui__webview *v) {
    cui__webview_win32 *w = v->native;
    if (!w->web || !w->key_registered) return false;
    WCHAR *text = cui__webview_win_wide(v->content);
    if (!text) return false;
    HRESULT error = v->html ? ICoreWebView2_NavigateToString(w->web, text) :
        v->header_count ? cui__webview_win_request(v, text) : ICoreWebView2_Navigate(w->web, text);
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

typedef struct {
    ICoreWebView2ExecuteScriptCompletedHandler handler;
    ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler startup_handler;
    LONG refs;
    cui__webview_win32 *host;
    cui_webview_result_fn callback;
    void *user;
} cui__webview_win_eval;
static HRESULT STDMETHODCALLTYPE cui__wv_eval_query(ICoreWebView2ExecuteScriptCompletedHandler *self, REFIID id, void **out) {
    static const GUID unknown = {0, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(id, &unknown) || IsEqualGUID(id, &IID_ICoreWebView2ExecuteScriptCompletedHandler)) *out = self;
    else if (IsEqualGUID(id, &IID_ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler))
        *out = &((cui__webview_win_eval *)self)->startup_handler;
    else return E_NOINTERFACE;
    InterlockedIncrement(&((cui__webview_win_eval *)self)->refs); return S_OK;
}
static ULONG STDMETHODCALLTYPE cui__wv_eval_addref(ICoreWebView2ExecuteScriptCompletedHandler *self) {
    return (ULONG)InterlockedIncrement(&((cui__webview_win_eval *)self)->refs);
}
static ULONG STDMETHODCALLTYPE cui__wv_eval_release(ICoreWebView2ExecuteScriptCompletedHandler *self) {
    cui__webview_win_eval *job = (cui__webview_win_eval *)self;
    ULONG refs = (ULONG)InterlockedDecrement(&job->refs);
    if (!refs) { cui__webview_win_release(job->host); free(job); }
    return refs;
}
static HRESULT STDMETHODCALLTYPE cui__wv_eval_invoke(ICoreWebView2ExecuteScriptCompletedHandler *self, HRESULT error, LPCWSTR result) {
    cui__webview_win_eval *job = (cui__webview_win_eval *)self;
    cui__webview *v = job->host->owner;
    if (!v || v->surface.dead || !job->callback) return S_OK;
    char *json = cui__webview_win_utf8(result);
    char message[100]; snprintf(message, sizeof message, "WebView2 script error (HRESULT 0x%08lx)", (unsigned long)error);
    cui__webview_result(v, job->callback, FAILED(error) ? NULL : json ? json : "null", FAILED(error) ? message : NULL, job->user);
    free(json);
    return S_OK;
}
static CONST_VTBL ICoreWebView2ExecuteScriptCompletedHandlerVtbl cui__wv_eval_vtable = {
    cui__wv_eval_query, cui__wv_eval_addref, cui__wv_eval_release, cui__wv_eval_invoke };

static cui__webview_win_eval *cui__wv_startup_owner(ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler *self) {
    return (cui__webview_win_eval *)((char *)self - offsetof(cui__webview_win_eval, startup_handler));
}
static HRESULT STDMETHODCALLTYPE cui__wv_startup_query(ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler *self, REFIID id, void **out) {
    return cui__wv_eval_query(&cui__wv_startup_owner(self)->handler, id, out);
}
static ULONG STDMETHODCALLTYPE cui__wv_startup_addref(ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler *self) {
    return cui__wv_eval_addref(&cui__wv_startup_owner(self)->handler);
}
static ULONG STDMETHODCALLTYPE cui__wv_startup_release(ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler *self) {
    return cui__wv_eval_release(&cui__wv_startup_owner(self)->handler);
}
static HRESULT STDMETHODCALLTYPE cui__wv_startup_invoke(ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler *self, HRESULT error, LPCWSTR id) {
    (void)id;
    return cui__wv_eval_invoke(&cui__wv_startup_owner(self)->handler, error, L"null");
}
static CONST_VTBL ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandlerVtbl cui__wv_startup_vtable = {
    cui__wv_startup_query, cui__wv_startup_addref, cui__wv_startup_release, cui__wv_startup_invoke };

static bool cui__webview_win_script(cui__webview *v, const char *script, cui_webview_result_fn callback, void *user, bool startup) {
    cui__webview_win32 *w = v->native;
    WCHAR *text = cui__webview_win_wide(script);
    cui__webview_win_eval *job = calloc(1, sizeof *job);
    if (!text || !job) { free(text); free(job); return false; }
    job->handler.lpVtbl = &cui__wv_eval_vtable; job->refs = 1; job->host = w; job->callback = callback; job->user = user;
    job->startup_handler.lpVtbl = &cui__wv_startup_vtable;
    InterlockedIncrement(&w->refs);
    HRESULT error = startup ? ICoreWebView2_AddScriptToExecuteOnDocumentCreated(w->web, text, &job->startup_handler) :
        ICoreWebView2_ExecuteScript(w->web, text, &job->handler);
    free(text); cui__wv_eval_release(&job->handler);
    return SUCCEEDED(error);
}
bool cui__webview_native_eval(cui__webview *v, const char *script, cui_webview_result_fn callback, void *user) {
    return cui__webview_win_script(v, script, callback, user, false);
}
bool cui__webview_native_startup_script(cui__webview *v, const char *script, cui_webview_result_fn callback, void *user) {
    return cui__webview_win_script(v, script, callback, user, true);
}
bool cui__webview_native_user_agent(cui__webview *v) {
    cui__webview_win32 *w = v->native;
    ICoreWebView2Settings *settings = NULL;
    ICoreWebView2Settings2 *settings2 = NULL;
    HRESULT error = ICoreWebView2_get_Settings(w->web, &settings);
    if (SUCCEEDED(error)) error = ICoreWebView2Settings_QueryInterface(settings, &IID_ICoreWebView2Settings2, (void **)&settings2);
    if (SUCCEEDED(error) && !w->default_user_agent) error = ICoreWebView2Settings2_get_UserAgent(settings2, &w->default_user_agent);
    WCHAR *text = v->user_agent ? cui__webview_win_wide(v->user_agent) : NULL;
    if (SUCCEEDED(error)) error = !v->user_agent || text ? ICoreWebView2Settings2_put_UserAgent(settings2, text ? text : w->default_user_agent) : E_OUTOFMEMORY;
    free(text);
    if (settings2) ICoreWebView2Settings2_Release(settings2);
    if (settings) ICoreWebView2Settings_Release(settings);
    return SUCCEEDED(error);
}
bool cui__webview_native_control(cui__webview *v, bool stop) {
    cui__webview_win32 *w = v->native;
    return SUCCEEDED(stop ? ICoreWebView2_Stop(w->web) : ICoreWebView2_Reload(w->web));
}
bool cui__webview_native_clear_cookies(cui__webview *v, cui_webview_result_fn callback, void *user) {
    cui__webview_win32 *w = v->native;
    ICoreWebView2_2 *web = NULL;
    ICoreWebView2CookieManager *cookies = NULL;
    HRESULT error = ICoreWebView2_QueryInterface(w->web, &IID_ICoreWebView2_2, (void **)&web);
    if (SUCCEEDED(error)) error = ICoreWebView2_2_get_CookieManager(web, &cookies);
    if (SUCCEEDED(error)) error = ICoreWebView2CookieManager_DeleteAllCookies(cookies);
    if (cookies) ICoreWebView2CookieManager_Release(cookies);
    if (web) ICoreWebView2_2_Release(web);
    if (SUCCEEDED(error)) cui__webview_result(v, callback, "null", NULL, user);
    return SUCCEEDED(error);
}

void cui__webview_native_wake(cui__webview *v) {
    cui__webview_win32 *w = v->native;
    PostMessageW(w->parent, WM_NULL, 0, 0);
}
