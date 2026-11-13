/* Ownership and content submission shared by the native browser engines. */
#include "cui_webview.h"

#ifdef CUI_WEBVIEW
static bool cui__webview_utf8(const char *text) {
    if (!text) return false;
    size_t len = strlen(text);
    for (size_t i = 0; i < len;) {
        uint32_t cp;
        int n = cui__utf8_decode(text + i, (int)CUI_MIN(len - i, 4), &cp);
        if ((cp == 0xFFFD && !(n == 3 && !memcmp(text + i, "\xef\xbf\xbd", 3))) ||
            (n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) ||
            cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
        i += (size_t)n;
    }
    return true;
}

static char *cui__webview_copy(const char *text) {
    size_t len = strlen(text);
    char *copy = malloc(len + 1);
    if (copy) memcpy(copy, text, len + 1);
    return copy;
}

static void cui__webview_headers_free(cui_webview_header *headers, int count) {
    for (int i = 0; i < count; i++) { free((void *)headers[i].name); free((void *)headers[i].value); }
    free(headers);
}

static void cui__webview_free(cui__webview *v) {
    v->surface.dead = true;
    cui__webview_native_destroy(v);
    cui__webview_headers_free(v->headers, v->header_count);
    free(v->user_agent);
    free((void *)v->load.url); free((void *)v->load.error);
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
    if (!strcmp(cui_app_backend(app), "x11")) app->webviews.pump = cui__webview_native_pump;
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

static bool cui__webview_submit(cui_webview_handle *view, const char *text, bool html,
    const cui_webview_request_opts *opts) {
#ifdef CUI_WEBVIEW
    if (!view || view->dead || !cui__webview_utf8(text)) return false;
    int count = opts ? opts->header_count : 0;
    if (count < 0 || (count && !opts->headers) || (size_t)count > SIZE_MAX / sizeof(cui_webview_header)) return false;
    cui_webview_header *headers = count ? calloc((size_t)count, sizeof *headers) : NULL;
    if (count && !headers) return false;
    for (int i = 0; i < count; i++) {
        const char *name = opts->headers[i].name, *value = opts->headers[i].value;
        if (!name || !*name || !cui__webview_utf8(value)) goto invalid_headers;
        for (const unsigned char *p = (const unsigned char *)name; *p; p++)
            if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
                strchr("!#$%&'*+-.^_`|~", *p))) goto invalid_headers;
        for (const unsigned char *p = (const unsigned char *)value; *p; p++)
            if ((*p < 0x20 && *p != '\t') || *p == 0x7f) goto invalid_headers;
        headers[i].name = cui__webview_copy(name); headers[i].value = cui__webview_copy(value);
        if (!headers[i].name || !headers[i].value) goto invalid_headers;
    }
    char *copy = cui__webview_copy(text);
    if (!copy) goto invalid_headers;
    cui__webview *v = (cui__webview *)view;
    free(v->content); v->content = copy; v->html = html;
    cui__webview_headers_free(v->headers, v->header_count);
    v->headers = headers; v->header_count = count;
    if (view->status == CUI_WEBVIEW_STARTING) return true;
    bool ok = cui__webview_native_load(v);
    view->status = ok ? CUI_WEBVIEW_READY : CUI_WEBVIEW_ERROR;
    cui_request_redraw(view->ui);
    return ok;
invalid_headers:
    cui__webview_headers_free(headers, count);
    return false;
#else
    (void)view; (void)text; (void)html; (void)opts;
    return false;
#endif
}

bool cui_webview_navigate(cui_webview_handle *view, const char *url) {
    return cui_webview_navigate_ex(view, url, NULL);
}

bool cui_webview_navigate_ex(cui_webview_handle *view, const char *url, const cui_webview_request_opts *opts) {
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
    if (opts && opts->header_count && strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) return false;
    return cui__webview_submit(view, url, false, opts);
}

bool cui_webview_set_html(cui_webview_handle *view, const char *html) {
    return cui__webview_submit(view, html, true, NULL);
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

void cui_webview_set_event_handler(cui_webview_handle *view, cui_webview_event_fn handler, void *user) {
#ifdef CUI_WEBVIEW
    if (view && !view->dead) { cui__webview *v = (cui__webview *)view; v->handler = handler; v->handler_user = user; }
#else
    (void)view; (void)handler; (void)user;
#endif
}

cui_webview_load_info cui_webview_get_load_info(const cui_webview_handle *view) {
    cui_webview_load_info info = { .url = "", .error = "" };
#ifdef CUI_WEBVIEW
    if (view && !view->dead) info = ((const cui__webview *)view)->load;
#else
    (void)view;
#endif
    if (!info.url) info.url = "";
    if (!info.error) info.error = "";
    return info;
}

#ifdef CUI_WEBVIEW
static void cui__webview_redraw(cui__webview *v) {
    bool wake = !cui_needs_redraw(v->surface.ui);
    cui_request_redraw(v->surface.ui);
    if (wake) cui__webview_native_wake(v);
}

void cui__webview_result(cui__webview *v, cui_webview_result_fn callback, const char *json, const char *error, void *user) {
    if (!v->surface.dead && callback) {
        cui__webview_redraw(v);
        callback(&v->surface, json, error, user);
    }
}

void cui__webview_emit(cui__webview *v, cui_webview_event_type type, const char *url,
    const char *message, float progress, int error_code) {
    if (v->surface.dead) return;
    char *event_url = cui__webview_copy(url ? url : "");
    char *event_message = cui__webview_copy(message ? message : "");
    if (!event_url || !event_message) { free(event_url); free(event_message); return; }
    if (type != CUI_WEBVIEW_MESSAGE) {
        char *copy = cui__webview_copy(event_url);
        bool terminal = type == CUI_WEBVIEW_LOAD_ERROR || type == CUI_WEBVIEW_LOAD_STARTED || type == CUI_WEBVIEW_LOAD_FINISHED;
        char *error = terminal ? cui__webview_copy(type == CUI_WEBVIEW_LOAD_ERROR ? event_message : "") : NULL;
        if (!copy || (terminal && !error)) { free(copy); free(error); free(event_url); free(event_message); return; }
        free((void *)v->load.url); v->load.url = copy;
        if (terminal) { free((void *)v->load.error); v->load.error = error; v->load.error_code = error_code; }
        v->load.progress = CUI_CLAMP(progress, 0, 1);
        if (type == CUI_WEBVIEW_LOAD_STARTED) v->load.state = CUI_WEBVIEW_LOADING;
        if (type == CUI_WEBVIEW_LOAD_FINISHED) v->load.state = CUI_WEBVIEW_LOADED;
        if (type == CUI_WEBVIEW_LOAD_ERROR) v->load.state = CUI_WEBVIEW_LOAD_FAILED;
    }
    cui_webview_event event = {type, event_url, event_message, CUI_CLAMP(progress, 0, 1), error_code};
    cui__webview_redraw(v);
    if (v->handler) v->handler(&v->surface, &event, v->handler_user);
    free(event_url); free(event_message);
}
#endif

bool cui_webview_eval(cui_webview_handle *view, const char *script, cui_webview_result_fn callback, void *user) {
#ifdef CUI_WEBVIEW
    if (cui_webview_get_status(view) != CUI_WEBVIEW_READY || !cui__webview_utf8(script)) return false;
    return cui__webview_native_eval((cui__webview *)view, script, callback, user);
#else
    (void)view; (void)script; (void)callback; (void)user;
    return false;
#endif
}

bool cui_webview_set_user_agent(cui_webview_handle *view, const char *user_agent) {
#ifdef CUI_WEBVIEW
    if (user_agent && !*user_agent) user_agent = NULL;
    if (!view || view->dead || (user_agent && !cui__webview_utf8(user_agent))) return false;
    for (const unsigned char *p = (const unsigned char *)user_agent; p && *p; p++)
        if (*p < 0x20 || *p == 0x7f) return false;
    char *copy = user_agent ? cui__webview_copy(user_agent) : NULL;
    if (user_agent && !copy) return false;
    cui__webview *v = (cui__webview *)view;
    char *old = v->user_agent;
    v->user_agent = copy;
    bool ok = view->status == CUI_WEBVIEW_STARTING || cui__webview_native_user_agent(v);
    if (!ok) { v->user_agent = old; free(copy); }
    else free(old);
    return ok;
#else
    (void)view; (void)user_agent;
    return false;
#endif
}

static bool cui__webview_control(cui_webview_handle *view, bool stop) {
#ifdef CUI_WEBVIEW
    return cui_webview_get_status(view) == CUI_WEBVIEW_READY && cui__webview_native_control((cui__webview *)view, stop);
#else
    (void)view; (void)stop;
    return false;
#endif
}
bool cui_webview_reload(cui_webview_handle *view) { return cui__webview_control(view, false); }
bool cui_webview_stop(cui_webview_handle *view) { return cui__webview_control(view, true); }

bool cui_webview_clear_cookies(cui_webview_handle *view, cui_webview_result_fn callback, void *user) {
#ifdef CUI_WEBVIEW
    return cui_webview_get_status(view) == CUI_WEBVIEW_READY && cui__webview_native_clear_cookies((cui__webview *)view, callback, user);
#else
    (void)view; (void)callback; (void)user;
    return false;
#endif
}
