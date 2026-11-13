/* WebKitGTK 4.1 / GTK3, embedded as an X11 child of the CUI window. */
#include "cui_webview.h"
#include <gtk/gtkx.h>
#include <gdk/gdkx.h>
#include <webkit2/webkit2.h>

typedef struct {
    GtkWidget *plug, *web;
    Display *display;
    Window parent, window;
    bool visible, failed;
} cui__webview_gtk;

static void cui__webview_gtk_focus(cui__webview_gtk *g, Window window) {
    XWindowAttributes attributes;
    if (XGetWindowAttributes(g->display, window, &attributes) && attributes.map_state == IsViewable)
        XSetInputFocus(g->display, window, RevertToParent, CurrentTime);
}

static gboolean cui__webview_gtk_key(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget;
    cui__webview *v = data;
    if ((event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_ISO_Left_Tab) && (event->state & GDK_CONTROL_MASK)) {
        cui__webview_gtk *g = v->native;
        cui__webview_gtk_focus(g, g->parent);
        XFlush(g->display);
        cui__webview_return_focus(v, (event->state & GDK_SHIFT_MASK) != 0);
        return TRUE;
    }
    return FALSE;
}

static void cui__webview_gtk_load(WebKitWebView *web, WebKitLoadEvent event, gpointer data) {
    cui__webview *v = data;
    cui__webview_gtk *g = v->native;
    if (event == WEBKIT_LOAD_STARTED) {
        g->failed = false;
        cui__webview_emit(v, CUI_WEBVIEW_LOAD_STARTED, webkit_web_view_get_uri(web), NULL, 0, 0);
    } else if (event == WEBKIT_LOAD_FINISHED && !g->failed)
        cui__webview_emit(v, CUI_WEBVIEW_LOAD_FINISHED, webkit_web_view_get_uri(web), NULL, 1, 0);
}
static gboolean cui__webview_gtk_failed(WebKitWebView *web, WebKitLoadEvent event, const gchar *url, GError *error, gpointer data) {
    (void)event;
    cui__webview *v = data;
    ((cui__webview_gtk *)v->native)->failed = true;
    cui__webview_emit(v, CUI_WEBVIEW_LOAD_ERROR, url, error->message, (float)webkit_web_view_get_estimated_load_progress(web), error->code);
    return FALSE;
}
static void cui__webview_gtk_changed(GObject *object, GParamSpec *property, gpointer data) {
    WebKitWebView *web = WEBKIT_WEB_VIEW(object);
    cui__webview *v = data;
    bool uri = !strcmp(property->name, "uri");
    if (uri || v->load.state == CUI_WEBVIEW_LOADING)
        cui__webview_emit(v, uri ? CUI_WEBVIEW_URL_CHANGED : CUI_WEBVIEW_LOAD_PROGRESS,
            webkit_web_view_get_uri(web), NULL, (float)webkit_web_view_get_estimated_load_progress(web), 0);
}
static void cui__webview_gtk_message(WebKitUserContentManager *manager, WebKitJavascriptResult *result, gpointer data) {
    (void)manager;
    cui__webview *v = data;
    JSCValue *value = webkit_javascript_result_get_js_value(result);
    if (!jsc_value_is_string(value)) return;
    char *message = jsc_value_to_string(value);
    cui__webview_gtk *g = v->native;
    cui__webview_emit(v, CUI_WEBVIEW_MESSAGE, webkit_web_view_get_uri(WEBKIT_WEB_VIEW(g->web)), message, v->load.progress, 0);
    g_free(message);
}

static gboolean cui__webview_gtk_policy(WebKitWebView *web, WebKitPolicyDecision *decision,
    WebKitPolicyDecisionType type, gpointer data) {
    (void)web;
    const char *url;
    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
        WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(decision));
        url = webkit_uri_request_get_uri(webkit_navigation_action_get_request(action));
    } else if (type == WEBKIT_POLICY_DECISION_TYPE_RESPONSE) {
        url = webkit_uri_response_get_uri(webkit_response_policy_decision_get_response(WEBKIT_RESPONSE_POLICY_DECISION(decision)));
    } else return FALSE;
    if (cui__webview_decide_navigation(data, url)) return FALSE;
    webkit_policy_decision_ignore(decision);
    return TRUE;
}

void *cui__webview_native_create(cui__webview *v) {
    if (!gdk_display_get_default()) gdk_set_allowed_backends("x11");
    if (!gtk_init_check(NULL, NULL) || !GDK_IS_X11_DISPLAY(gdk_display_get_default())) return NULL;
    cui__webview_gtk *g = calloc(1, sizeof *g);
    if (!g) return NULL;
    g->display = gdk_x11_display_get_xdisplay(gdk_display_get_default());
    g->parent = (Window)(uintptr_t)cui_app_native_window(v->app);
    g->plug = gtk_plug_new(0);
    g->web = webkit_web_view_new();
    g_object_set_data(G_OBJECT(g->web), "cui-owner", v);
    WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(g->web));
    /* The isolated relay rejects iframe messages; its native handler is not in the page world. */
    if (!webkit_user_content_manager_register_script_message_handler_in_world(manager, "cui", "cui")) {
        gtk_widget_destroy(g->web); gtk_widget_destroy(g->plug); free(g); return NULL;
    }
    WebKitUserScript *relay = webkit_user_script_new_for_world(
        "window.addEventListener('message',function(e){if(e.source===window&&e.data&&typeof e.data.__cui==='string')window.webkit.messageHandlers.cui.postMessage(e.data.__cui);});",
        WEBKIT_USER_CONTENT_INJECT_TOP_FRAME, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, "cui", NULL, NULL);
    WebKitUserScript *bridge = webkit_user_script_new(
        "window.cui=Object.freeze({postMessage:function(message){if(typeof message!=='string')throw new TypeError('Expected a string');window.postMessage({__cui:message},'*');}});",
        WEBKIT_USER_CONTENT_INJECT_TOP_FRAME, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, NULL, NULL);
    webkit_user_content_manager_add_script(manager, relay); webkit_user_script_unref(relay);
    webkit_user_content_manager_add_script(manager, bridge); webkit_user_script_unref(bridge);
    g_signal_connect(manager, "script-message-received::cui", G_CALLBACK(cui__webview_gtk_message), v);
    g_signal_connect(g->web, "load-changed", G_CALLBACK(cui__webview_gtk_load), v);
    g_signal_connect(g->web, "decide-policy", G_CALLBACK(cui__webview_gtk_policy), v);
    g_signal_connect(g->web, "load-failed", G_CALLBACK(cui__webview_gtk_failed), v);
    g_signal_connect(g->web, "notify::uri", G_CALLBACK(cui__webview_gtk_changed), v);
    g_signal_connect(g->web, "notify::estimated-load-progress", G_CALLBACK(cui__webview_gtk_changed), v);
    gtk_container_add(GTK_CONTAINER(g->plug), g->web);
    gtk_widget_realize(g->plug);
    g->window = gdk_x11_window_get_xid(gtk_widget_get_window(g->plug));
    XReparentWindow(g->display, g->window, g->parent, 0, 0);
    XFlush(g->display);
    g_signal_connect(g->plug, "key-press-event", G_CALLBACK(cui__webview_gtk_key), v);
    v->surface.status = CUI_WEBVIEW_READY;
    return g;
}

void cui__webview_native_destroy(cui__webview *v) {
    cui__webview_gtk *g = v->native;
    if (g->visible) {
        Window focused; int revert;
        XGetInputFocus(g->display, &focused, &revert);
        if (focused == g->window) cui__webview_gtk_focus(g, g->parent);
    }
    g_object_set_data(G_OBJECT(g->web), "cui-owner", NULL);
    g_signal_handlers_disconnect_by_data(g->web, v);
    WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(g->web));
    g_signal_handlers_disconnect_by_data(manager, v);
    gtk_widget_destroy(g->plug);
    free(g);
    v->native = NULL;
}

bool cui__webview_native_load(cui__webview *v) {
    cui__webview_gtk *g = v->native;
    if (v->html) webkit_web_view_load_html(WEBKIT_WEB_VIEW(g->web), v->content, NULL);
    else if (v->header_count) {
        WebKitURIRequest *request = webkit_uri_request_new(v->content);
        SoupMessageHeaders *headers = webkit_uri_request_get_http_headers(request);
        if (!headers) { g_object_unref(request); return false; }
        for (int i = 0; i < v->header_count; i++) soup_message_headers_replace(headers, v->headers[i].name, v->headers[i].value);
        webkit_web_view_load_request(WEBKIT_WEB_VIEW(g->web), request);
        g_object_unref(request);
    } else webkit_web_view_load_uri(WEBKIT_WEB_VIEW(g->web), v->content);
    return true;
}

bool cui__webview_native_startup_script(cui__webview *v, const char *script, cui_webview_result_fn callback, void *user) {
    cui__webview_gtk *g = v->native;
    WebKitUserScript *startup = webkit_user_script_new(script, WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, NULL, NULL);
    if (!startup) return false;
    webkit_user_content_manager_add_script(webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(g->web)), startup);
    webkit_user_script_unref(startup);
    cui__webview_result(v, callback, "null", NULL, user);
    return true;
}

bool cui__webview_native_history(cui__webview *v, bool forward, bool move) {
    cui__webview_gtk *g = v->native;
    WebKitWebView *web = WEBKIT_WEB_VIEW(g->web);
    if (!(forward ? webkit_web_view_can_go_forward(web) : webkit_web_view_can_go_back(web))) return false;
    if (move) {
        if (forward) webkit_web_view_go_forward(web);
        else webkit_web_view_go_back(web);
    }
    return true;
}

void cui__webview_native_place(cui__webview *v, bool visible) {
    cui__webview_gtk *g = v->native;
    cui_rect r = v->surface.rect;
    float scale = v->surface.ui->scale;
    int w = CUI_MAX(1, (int)lroundf(r.w * scale)), h = CUI_MAX(1, (int)lroundf(r.h * scale));
    int gtk_scale = gtk_widget_get_scale_factor(g->plug);
    if (visible != g->visible) {
        if (visible) gtk_widget_show_all(g->plug);
        else {
            Window focused; int revert;
            XGetInputFocus(g->display, &focused, &revert);
            if (focused == g->window) cui__webview_gtk_focus(g, g->parent);
            gtk_widget_hide(g->plug);
        }
        g->visible = visible;
    }
    /* A CUI parent has no GtkSocket to allocate the plug's children. */
    GtkAllocation allocation = {0, 0, CUI_MAX(1, w / gtk_scale), CUI_MAX(1, h / gtk_scale)};
    gtk_widget_size_allocate(g->plug, &allocation);
    XMoveResizeWindow(g->display, g->window, (int)lroundf(r.x * scale), (int)lroundf(r.y * scale), (unsigned)w, (unsigned)h);
    /* GtkPlug asks its socket to map it; CUI is that native parent. */
    if (visible) XMapWindow(g->display, g->window);
    else XUnmapWindow(g->display, g->window);
    if (visible && v->surface.focus_requested) {
        gtk_widget_grab_focus(g->web);
        cui__webview_gtk_focus(g, g->window);
    }
    if (visible) {
        Window focused; int revert;
        XGetInputFocus(g->display, &focused, &revert);
        if (focused == g->window) cui__webview_claim_focus(v);
    }
    XFlush(g->display);
}

void cui__webview_native_pump(void) {
    /* Bound work per CUI frame even while a page continuously posts events. */
    for (int i = 0; i < 64 && g_main_context_pending(NULL); i++) g_main_context_iteration(NULL, FALSE);
}

typedef struct { cui_webview_result_fn callback; void *user; GWeakRef web; } cui__webview_gtk_result;
static void cui__webview_gtk_eval_done(GObject *source, GAsyncResult *result, gpointer data) {
    cui__webview_gtk_result *job = data;
    GError *error = NULL;
    JSCValue *value = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(source), result, &error);
    char *json = value ? jsc_value_to_json(value, 0) : NULL;
    cui__webview *v = g_object_get_data(source, "cui-owner");
    if (v) cui__webview_result(v, job->callback, error ? NULL : json ? json : "null", error ? error->message : NULL, job->user);
    g_free(json);
    if (value) g_object_unref(value);
    g_clear_error(&error); free(job);
}
bool cui__webview_native_eval(cui__webview *v, const char *script, cui_webview_result_fn callback, void *user) {
    cui__webview_gtk_result *job = calloc(1, sizeof *job);
    if (!job) return false;
    job->callback = callback; job->user = user;
    cui__webview_gtk *g = v->native;
    webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(g->web), script, -1, NULL, NULL, NULL, cui__webview_gtk_eval_done, job);
    return true;
}
bool cui__webview_native_user_agent(cui__webview *v) {
    cui__webview_gtk *g = v->native;
    webkit_settings_set_user_agent(webkit_web_view_get_settings(WEBKIT_WEB_VIEW(g->web)), v->user_agent);
    return true;
}
bool cui__webview_native_control(cui__webview *v, bool stop) {
    cui__webview_gtk *g = v->native;
    if (stop) webkit_web_view_stop_loading(WEBKIT_WEB_VIEW(g->web));
    else webkit_web_view_reload(WEBKIT_WEB_VIEW(g->web));
    return true;
}
static void cui__webview_gtk_cookies_done(GObject *source, GAsyncResult *result, gpointer data) {
    cui__webview_gtk_result *job = data;
    GError *error = NULL;
    webkit_website_data_manager_clear_finish(WEBKIT_WEBSITE_DATA_MANAGER(source), result, &error);
    GObject *web = g_weak_ref_get(&job->web);
    cui__webview *v = web ? g_object_get_data(web, "cui-owner") : NULL;
    if (v) cui__webview_result(v, job->callback, error ? NULL : "null", error ? error->message : NULL, job->user);
    if (web) g_object_unref(web);
    g_weak_ref_clear(&job->web); g_clear_error(&error); free(job);
}
bool cui__webview_native_clear_cookies(cui__webview *v, cui_webview_result_fn callback, void *user) {
    cui__webview_gtk *g = v->native;
    cui__webview_gtk_result *job = calloc(1, sizeof *job);
    if (!job) return false;
    job->callback = callback; job->user = user; g_weak_ref_init(&job->web, G_OBJECT(g->web));
    WebKitWebsiteDataManager *manager = webkit_web_context_get_website_data_manager(webkit_web_view_get_context(WEBKIT_WEB_VIEW(g->web)));
    webkit_website_data_manager_clear(manager, WEBKIT_WEBSITE_DATA_COOKIES, 0, NULL, cui__webview_gtk_cookies_done, job);
    return true;
}

/* The shared app loop already pumps GTK with a bounded idle wait. */
void cui__webview_native_wake(cui__webview *v) { (void)v; }
