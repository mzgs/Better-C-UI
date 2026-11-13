/* Exercise the real shared lifecycle with a small native-engine substitute. */
#include "../addons/webview/cui_webview.h"
#include <assert.h>

static int wv_created, wv_destroyed, wv_loaded;
static bool wv_visible, wv_fail;
static int wv_history_at, wv_history_last;
void *cui__webview_native_create(cui__webview *v) { wv_created++; return v; }
void cui__webview_native_destroy(cui__webview *v) { (void)v; wv_destroyed++; }
bool cui__webview_native_load(cui__webview *v) { assert(v->content); wv_loaded++; return !wv_fail; }
bool cui__webview_native_history(cui__webview *v, bool forward, bool move) {
    (void)v;
    if (forward ? wv_history_at == wv_history_last : wv_history_at == 0) return false;
    if (move) wv_history_at += forward ? 1 : -1;
    return true;
}
void cui__webview_native_place(cui__webview *v, bool visible) { (void)v; wv_visible = visible; }
void cui__webview_native_pump(void) {}
void cui__webview_native_wake(cui__webview *v) { (void)v; }

static int wv_scripts, wv_agents, wv_controls, wv_results, wv_events;
static bool wv_stopped, wv_destroy_on_event;
bool cui__webview_native_eval(cui__webview *v, const char *script, cui_webview_result_fn callback, void *user) {
    assert(!strcmp(script, "1+2")); wv_scripts++;
    cui__webview_result(v, callback, "3", NULL, user);
    return true;
}
bool cui__webview_native_user_agent(cui__webview *v) { (void)v; wv_agents++; return !wv_fail; }
bool cui__webview_native_control(cui__webview *v, bool stop) { (void)v; wv_controls++; wv_stopped = stop; return !wv_fail; }
bool cui__webview_native_clear_cookies(cui__webview *v, cui_webview_result_fn callback, void *user) {
    if (wv_fail) return false;
    cui__webview_result(v, callback, "null", NULL, user);
    return true;
}
static void wv_result(cui_webview_handle *view, const char *json, const char *error, void *user) {
    assert(view && !error && !strcmp(json, user)); wv_results++;
}
static void wv_event(cui_webview_handle *view, const cui_webview_event *event, void *user) {
    assert(user == &wv_events && event->url && event->message);
    assert(strlen(event->url) < 4096 && strlen(event->message) < 4096);
    wv_events++;
    if (wv_destroy_on_event) cui_webview_destroy(view);
}

static void wv_frame(cui_app *app, cui_webview_handle *view, bool disabled, bool clipped, bool floating) {
    cui_ctx *ui = app->ui;
    cui_begin_frame(ui, 1.0f / 60);
    cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 12) {
        if (clipped) {
            cui_box(ui, .width = CUI_FILL, .height = CUI_PX(50), .clip = true) {
                cui_webview(ui, "page", view, .height = 120);
            }
        } else cui_webview(ui, "page", view, .height = 120, .disabled = disabled);
    }
    cui_end_frame(ui);
    if (floating) ui->float_count = 1; /* sync must also check overlays added after the component */
    app->webviews.sync(app);
    if (floating) ui->float_count = 0;
}

int main(void) {
    cui_app app = {0};
    app.ui = cui_create(NULL);
    cui_set_viewport(app.ui, 360, 240, 2);
    cui_webview_handle *v = cui_app_webview_create(&app);
    assert(v && cui_webview_get_status(v) == CUI_WEBVIEW_STARTING && wv_created == 1);
    assert(!cui_webview_can_go_back(NULL) && !cui_webview_can_go_forward(NULL));
    assert(!cui_webview_go_back(NULL) && !cui_webview_go_forward(NULL));
    assert(!cui_webview_go_back(v) && !cui_webview_go_forward(v));
    assert(cui_webview_get_load_info(v).state == CUI_WEBVIEW_LOAD_IDLE);
    assert(!cui_webview_eval(v, "1+2", wv_result, "3") && !cui_webview_reload(v) && !cui_webview_stop(v));
    assert(!cui_webview_clear_cookies(v, wv_result, "null"));
    assert(cui_webview_set_user_agent(v, "CUI-Test/1") && wv_agents == 0);
    assert(!strcmp(((cui__webview *)v)->user_agent, "CUI-Test/1"));
    assert(!cui_webview_set_user_agent(v, "bad\r\nHeader: yes"));
    assert(!cui_webview_set_user_agent(v, "\xff"));
    cui_webview_set_event_handler(v, wv_event, &wv_events);
    char name[] = "X-Test", value[] = "first";
    cui_webview_header header = {name, value};
    cui_webview_request_opts request = { .headers = &header, .header_count = 1 };
    assert(cui_webview_navigate_ex(v, "https://example.com", &request));
    value[0] = 'X'; name[0] = 'Y';
    assert(!strcmp(((cui__webview *)v)->headers[0].name, "X-Test"));
    assert(!strcmp(((cui__webview *)v)->headers[0].value, "first"));
    header.name = "Bad:Name"; assert(!cui_webview_navigate_ex(v, "https://example.com", &request));
    header.name = "X-Test"; header.value = "x\nInjected: x";
    assert(!cui_webview_navigate_ex(v, "https://example.com", &request));
    header.value = "\x7f"; assert(!cui_webview_navigate_ex(v, "https://example.com", &request));
    header.value = "\xff"; assert(!cui_webview_navigate_ex(v, "https://example.com", &request));
    header.value = "okay";
    assert(!cui_webview_navigate_ex(v, "file:///tmp/x.html", &request));
    request.header_count = -1; assert(!cui_webview_navigate_ex(v, "https://example.com", &request));
    request.header_count = 1; request.headers = NULL;
    assert(!cui_webview_navigate_ex(v, "https://example.com", &request));
    char html[] = "<p>First</p>";
    assert(cui_webview_set_html(v, html));
    html[3] = 'X';
    assert(!strcmp(((cui__webview *)v)->content, "<p>First</p>"));
    assert(cui_webview_navigate(v, "https://example.com/"));
    assert(!((cui__webview *)v)->html && wv_loaded == 0);
    const char *files[] = { "file:///tmp/video/index.html", "file:///C:/Video%20demo/index.html", "file:///tmp/\xc3\xb6rnek.html" };
    for (size_t i = 0; i < sizeof files / sizeof *files; i++) {
        assert(cui_webview_navigate(v, files[i]));
        assert(!strcmp(((cui__webview *)v)->content, files[i]) && !((cui__webview *)v)->html);
    }
    assert(wv_loaded == 0); /* file navigation is also queued during startup */
    const char *bad[] = { "", "javascript:alert(1)", "file://host/video.html", "file://relative", "file:///", "file:////host/video.html", "file:///C:\\video.html", "file:///tmp/\n", "file:///tmp/\xff", "https://", "http:///x", "https://?x", "https://a/\n", "https://a/\xc0\xaf", "https://a/\xed\xa0\x80", "https://a/\xf4\x90\x80\x80", "https://a/\xff" };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) assert(!cui_webview_navigate(v, bad[i]));
    assert(!cui_webview_set_html(v, "\xf0\x80\x80\x80"));
    assert(!cui_webview_set_html(v, "\xe2\x82"));
    assert(!cui_webview_navigate(NULL, "https://example.com"));
    assert(!cui_webview_set_html(v, NULL));
    assert(cui_webview_get_status(NULL) == CUI_WEBVIEW_UNAVAILABLE);
    wv_frame(&app, v, false, false, false);
    assert(!wv_visible); /* asynchronous startup */
    v->status = CUI_WEBVIEW_READY;
    assert(cui_webview_eval(v, "1+2", wv_result, "3") && wv_scripts == 1 && wv_results == 1);
    assert(!cui_webview_eval(v, "\xff", wv_result, "3"));
    assert(cui_webview_clear_cookies(v, wv_result, "null") && wv_results == 2);
    assert(cui_webview_set_user_agent(v, NULL) && !((cui__webview *)v)->user_agent && wv_agents == 1);
    assert(cui_webview_reload(v) && !wv_stopped && cui_webview_stop(v) && wv_stopped && wv_controls == 2);
    cui__webview_emit((cui__webview *)v, CUI_WEBVIEW_LOAD_STARTED, "https://example.com/a", NULL, 0, 0);
    cui__webview_emit((cui__webview *)v, CUI_WEBVIEW_LOAD_PROGRESS, "https://example.com/a", NULL, 0.4f, 0);
    cui_webview_load_info info = cui_webview_get_load_info(v);
    assert(info.state == CUI_WEBVIEW_LOADING && info.progress == 0.4f && !strcmp(info.url, "https://example.com/a"));
    cui__webview_emit((cui__webview *)v, CUI_WEBVIEW_MESSAGE, "https://example.com/a", "hello", 0.4f, 0);
    assert(wv_events == 3 && cui_webview_get_load_info(v).progress == 0.4f);
    cui__webview_emit((cui__webview *)v, CUI_WEBVIEW_LOAD_ERROR, "https://example.com/a", "test error", 0.4f, 42);
    info = cui_webview_get_load_info(v);
    assert(info.state == CUI_WEBVIEW_LOAD_FAILED && info.error_code == 42 && !strcmp(info.error, "test error"));
    cui__webview_emit((cui__webview *)v, CUI_WEBVIEW_LOAD_STARTED, "about:blank", NULL, 0, 0);
    cui__webview_emit((cui__webview *)v, CUI_WEBVIEW_LOAD_FINISHED, "about:blank", NULL, 1, 0);
    info = cui_webview_get_load_info(v);
    assert(info.state == CUI_WEBVIEW_LOADED && info.progress == 1 && !*info.error);
    cui_webview_set_event_handler(v, NULL, NULL);
    cui__webview_emit((cui__webview *)v, CUI_WEBVIEW_URL_CHANGED, "about:blank#hash", NULL, 1, 0);
    assert(wv_events == 6 && !strcmp(cui_webview_get_load_info(v).url, "about:blank#hash"));
    info = cui_webview_get_load_info(v);
    cui__webview_emit((cui__webview *)v, CUI_WEBVIEW_URL_CHANGED, info.url, NULL, info.progress, 0);
    assert(!strcmp(cui_webview_get_load_info(v).url, "about:blank#hash"));
    assert(!cui_webview_can_go_back(v) && !cui_webview_go_back(v));
    assert(!cui_webview_can_go_forward(v) && !cui_webview_go_forward(v));
    wv_history_at = wv_history_last = 1;
    assert(cui_webview_can_go_back(v) && !cui_webview_can_go_forward(v));
    assert(cui_webview_go_back(v) && wv_history_at == 0);
    assert(!cui_webview_can_go_back(v) && cui_webview_can_go_forward(v));
    assert(cui_webview_go_forward(v) && wv_history_at == 1);
    assert(cui_webview_set_html(v, "<p>Merhaba \xef\xbf\xbd \xf0\x9f\x8c\x8d</p>"));
    assert(wv_loaded == 1);
    assert(cui_webview_navigate(v, "file:///tmp/video/index.html"));
    assert(wv_loaded == 2 && cui_webview_get_status(v) == CUI_WEBVIEW_READY);
    wv_frame(&app, v, false, false, false);
    assert(wv_visible && v->rect.w == 334 && v->rect.h == 118);
    for (int height = 240; height <= 480; height += 240) {
        cui_set_viewport(app.ui, 360, (float)height, 2);
        cui_begin_frame(app.ui, 1.0f / 60);
        cui_column(app.ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 12, .gap = 8) {
            cui_box(app.ui, .height = CUI_PX(36), .width = CUI_FILL) {}
            cui_webview(app.ui, "page", v, .height = -1);
        }
        cui_end_frame(app.ui); app.webviews.sync(&app);
        assert(wv_visible && v->rect.h == height - 70);
        assert(v->rect.y + v->rect.h == height - 13);
    }
    cui_set_viewport(app.ui, 360, 240, 2);
    wv_frame(&app, v, true, false, false); assert(!wv_visible);
    wv_frame(&app, v, false, true, false); assert(!wv_visible);
    wv_frame(&app, v, false, false, true); assert(!wv_visible);
    wv_frame(&app, NULL, false, false, false); assert(!wv_visible); /* omission */
    wv_frame(&app, v, false, false, false); assert(wv_visible);
    app.ui->focus = 0;
    cui__webview_claim_focus((cui__webview *)v);
    assert(app.ui->focus == v->id && !app.ui->focus_visible);
    wv_fail = true;
    assert(!cui_webview_set_user_agent(v, "Rejected") && !((cui__webview *)v)->user_agent);
    assert(!cui_webview_reload(v) && !cui_webview_stop(v));
    assert(!cui_webview_clear_cookies(v, wv_result, "null") && wv_results == 2);
    assert(!cui_webview_set_html(v, ""));
    assert(cui_webview_get_status(v) == CUI_WEBVIEW_ERROR);
    assert(!cui_webview_can_go_back(v) && !cui_webview_go_back(v));
    wv_fail = false;
    assert(cui_webview_navigate(v, "about:blank"));
    assert(cui_webview_get_status(v) == CUI_WEBVIEW_READY);
    cui_begin_frame(app.ui, 1.0f / 60);
    cui_webview(app.ui, "page", v);
    cui_webview_set_event_handler(v, wv_event, &wv_events); wv_destroy_on_event = true;
    cui__webview_emit((cui__webview *)v, CUI_WEBVIEW_MESSAGE, "about:blank", "destroy", 1, 0);
    assert(v->dead); /* event callbacks may invalidate their handle */
    assert(!cui_webview_eval(v, "1+2", wv_result, "3") && !cui_webview_set_user_agent(v, NULL));
    assert(cui_webview_get_status(v) == CUI_WEBVIEW_UNAVAILABLE);
    assert(!cui_webview_can_go_back(v) && !cui_webview_go_forward(v));
    cui_end_frame(app.ui); app.webviews.sync(&app);
    assert(wv_destroyed == 1 && !app.webviews.head);
    assert(cui_app_webview_create(&app));
    app.webviews.destroy(&app);
    assert(wv_destroyed == 2);
    cui_destroy(app.ui);
    puts("webview: content, UTF-8, scripts, headers, settings, events, placement and lifecycle checks passed");
    return 0;
}
