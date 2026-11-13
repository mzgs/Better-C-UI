/* Ownership and content submission shared by the native browser engines. */
#include "cui_webview.h"

#ifdef CUI_WEBVIEW
static void cui__webview_free(cui__webview *v) {
    cui__webview_native_destroy(v);
    free(v->content);
    free(v);
}

static void cui__webviews_sync(cui_app *app) {
    cui__webview **link = &app->webviews.head;
    while (*link) {
        cui__webview *v = *link;
        if (v->surface.dead) { *link = v->next; cui__webview_free(v); continue; }
        bool visible = v->surface.seen && !app->ui->float_count && v->surface.status == CUI_WEBVIEW_READY;
        if (visible && v->surface.focus_requested) {
            cui_input_focus(app->ui, false);
            cui_input_focus(app->ui, true);
        }
        cui__webview_native_place(v, visible);
        v->surface.seen = false;
        if (v->surface.status != CUI_WEBVIEW_STARTING || app->ui->focus != v->surface.id)
            v->surface.focus_requested = false;
        link = &v->next;
    }
    if (!app->webviews.head) app->webviews.pump = NULL;
}

static void cui__webviews_destroy(cui_app *app) {
    cui__webview *v = app->webviews.head;
    while (v) { cui__webview *next = v->next; cui__webview_free(v); v = next; }
    memset(&app->webviews, 0, sizeof app->webviews);
}

void cui__webview_return_focus(cui__webview *v, bool backward) {
    cui_ctx *ui = v->surface.ui;
    cui_input_focus(ui, false);
    cui_input_focus(ui, true);
    ui->focus = v->surface.id;
    cui_input_key(ui, CUI_KEY_TAB, true, backward ? CUI_MOD_SHIFT : 0);
    cui_input_key(ui, CUI_KEY_TAB, false, backward ? CUI_MOD_SHIFT : 0);
    cui_request_redraw(ui);
}

void cui__webview_claim_focus(cui__webview *v) {
    cui_ctx *ui = v->surface.ui;
    if (ui->focus == v->surface.id) return;
    cui_input_focus(ui, false);
    cui_input_focus(ui, true);
    ui->focus = v->surface.id;
    ui->focus_visible = false;
    cui_request_redraw(ui);
}
#endif

cui_webview_handle *cui_app_webview_create(cui_app *app) {
#ifdef CUI_WEBVIEW
    app = cui__app_or_current(app);
    if (!app) return NULL;
    cui__webview *v = calloc(1, sizeof *v);
    if (!v) return NULL;
    v->app = app; v->surface.ui = app->ui; v->surface.status = CUI_WEBVIEW_STARTING;
    v->native = cui__webview_native_create(v);
    if (!v->native) { free(v); return NULL; }
    v->next = app->webviews.head;
    app->webviews.head = v;
    app->webviews.sync = cui__webviews_sync;
    app->webviews.destroy = cui__webviews_destroy;
    if (!strcmp(cui__plat_name(), "x11")) app->webviews.pump = cui__webview_native_pump;
    return &v->surface;
#else
    (void)app;
    return NULL;
#endif
}

void cui_webview_destroy(cui_webview_handle *view) {
    if (view) { view->dead = true; cui_request_redraw(view->ui); }
}

cui_webview_status cui_webview_get_status(const cui_webview_handle *view) {
    return view && !view->dead ? view->status : CUI_WEBVIEW_UNAVAILABLE;
}

static bool cui__webview_submit(cui_webview_handle *view, const char *text, bool html) {
#ifdef CUI_WEBVIEW
    if (!view || view->dead || !text) return false;
    cui__webview *v = (cui__webview *)view;
    size_t len = strlen(text);
    /* Reject malformed UTF-8 before passing it to platform string APIs. */
    for (size_t i = 0; i < len;) {
        uint32_t cp;
        int n = cui__utf8_decode(text + i, (int)CUI_MIN(len - i, 4), &cp);
        if ((cp == 0xFFFD && !(n == 3 && !memcmp(text + i, "\xef\xbf\xbd", 3))) ||
            (n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) ||
            cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
        i += (size_t)n;
    }
    char *copy = malloc(len + 1);
    if (!copy) return false;
    memcpy(copy, text, len + 1);
    free(v->content); v->content = copy; v->html = html;
    if (view->status == CUI_WEBVIEW_STARTING) return true;
    bool ok = cui__webview_native_load(v);
    view->status = ok ? CUI_WEBVIEW_READY : CUI_WEBVIEW_ERROR;
    cui_request_redraw(view->ui);
    return ok;
#else
    (void)view; (void)text; (void)html;
    return false;
#endif
}

bool cui_webview_navigate(cui_webview_handle *view, const char *url) {
    if (!url) return false;
    bool file = !strncmp(url, "file:///", 8);
    if (file && (!url[8] || url[8] == '/' || strchr(url, '\\'))) return false;
    if (!file && strncmp(url, "https://", 8) && strncmp(url, "http://", 7) && strcmp(url, "about:blank")) return false;
    if (strcmp(url, "about:blank")) {
        const char *host = strstr(url, "://") + 3;
        if (!file && (!*host || *host == '/' || *host == '?' || *host == '#')) return false;
        for (const unsigned char *p = (const unsigned char *)url; *p; p++)
            if (*p <= 0x20 || *p == 0x7f) return false;
    }
    return cui__webview_submit(view, url, false);
}

bool cui_webview_set_html(cui_webview_handle *view, const char *html) {
    return cui__webview_submit(view, html, true);
}

static bool cui__webview_history(cui_webview_handle *view, bool forward, bool move) {
#ifdef CUI_WEBVIEW
    if (cui_webview_get_status(view) != CUI_WEBVIEW_READY) return false;
    bool ok = cui__webview_native_history((cui__webview *)view, forward, move);
    if (ok && move) cui_request_redraw(view->ui);
    return ok;
#else
    (void)view; (void)forward; (void)move;
    return false;
#endif
}

bool cui_webview_can_go_back(cui_webview_handle *view) { return cui__webview_history(view, false, false); }
bool cui_webview_can_go_forward(cui_webview_handle *view) { return cui__webview_history(view, true, false); }
bool cui_webview_go_back(cui_webview_handle *view) { return cui__webview_history(view, false, true); }
bool cui_webview_go_forward(cui_webview_handle *view) { return cui__webview_history(view, true, true); }
