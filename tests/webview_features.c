/* Real-engine checks, driven by webview_features.py's loopback fixtures. */
#include <cui/cui_webview.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cui/cui_render.h>
#define main features_example_main
#include "../examples/webview.c"
#undef main

typedef struct {
    bool loaded, result_done, result_ok;
    int started, progress, finished, errors, messages, canceled;
    const char *expected;
} features_check;
static bool features_idle;

static void features_event(cui_webview_handle *view, const cui_webview_event *event, void *user) {
    (void)view;
    features_check *check = user;
    assert(event->url && event->message && event->progress >= 0 && event->progress <= 1);
    if (event->type == CUI_WEBVIEW_LOAD_STARTED) check->started++;
    if (event->type == CUI_WEBVIEW_LOAD_PROGRESS) check->progress++;
    if (event->type == CUI_WEBVIEW_LOAD_FINISHED) { check->finished++; check->loaded = true; }
    if (event->type == CUI_WEBVIEW_LOAD_ERROR) { assert(*event->message); check->errors++; check->loaded = true; }
    if (event->type == CUI_WEBVIEW_MESSAGE) {
        assert(strstr(event->url, "/features") && !strcmp(event->message, "from-page"));
        check->messages++;
    }
}
static void features_result(cui_webview_handle *view, const char *json, const char *error, void *user) {
    (void)view;
    features_check *check = user;
    check->result_done = true;
    check->result_ok = check->expected ? !error && json && !strcmp(json, check->expected) : error || (json && !strcmp(json, "null"));
    if (!check->result_ok) fprintf(stderr, "script result: %s; error: %s; expected: %s\n", json ? json : "(null)", error ? error : "(none)", check->expected ? check->expected : "error");
}
static void features_canceled(cui_webview_handle *view, const char *json, const char *error, void *user) {
    (void)view; (void)json; (void)error;
    ((features_check *)user)->canceled++;
}
static void features_idle_event(cui_webview_handle *view, const cui_webview_event *event, void *user) {
    (void)view;
    if (event->type == CUI_WEBVIEW_MESSAGE) {
        assert(!strcmp(event->message, "wake"));
        *(bool *)user = true;
    }
}
static bool features_frame(cui_app *app, cui_webview_handle *view) {
    if (!cui_app_frame(app)) return false;
    cui_ctx *ui = cui_app_ui(app);
    if (!features_idle) cui_request_wakeup(ui, 1.0f / 60);
    cui_column(ui, .width = CUI_FILL, .height = CUI_FILL) { cui_webview(ui, "page", view, .height = -1); }
    return true;
}
static void features_wait(cui_app *app, cui_webview_handle *view, bool *done) {
    for (int frame = 0; frame < 1800 && !*done && features_frame(app, view); frame++) {}
    assert(*done);
}
static void features_eval(cui_app *app, cui_webview_handle *view, features_check *check, const char *script, const char *expected) {
    check->result_done = check->result_ok = false; check->expected = expected;
    assert(cui_webview_eval(view, script, features_result, check));
    features_wait(app, view, &check->result_done);
    assert(check->result_ok);
}

static int features_shots(const char *dir, int width) {
    assert(width >= 360 && width <= 1920);
    for (int dark = 0; dark < 2; dark++) {
        cui_sw_renderer *sw = cui_sw_create();
        cui_ctx *ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw), .theme = dark ? CUI_THEME_DARK : CUI_THEME_LIGHT });
        cui_set_viewport(ui, (float)width, 700, 1); cui_set_reduced_motion(ui, true);
        webview_demo demo = { .attempted = true, .url = "about:blank", .script = "document.title" };
        for (int i = 0; i < 3; i++) { cui_begin_frame(ui, 1.0f / 60); webview_frame(ui, &demo); cui_end_frame(ui); }
        const cui_draw_data *draw = cui_get_draw_data(ui);
        unsigned char *pixels = malloc((size_t)width * 700 * 4);
        assert(pixels);
        cui_sw_render(sw, draw, pixels, width, 700, width * 4, CUI_SW_RGBA);
        char path[1024];
        int n = snprintf(path, sizeof path, "%s/webview-features%s.png", dir, dark ? "-dark" : "");
        assert(n > 0 && n < (int)sizeof path && cui_write_png(path, pixels, width, 700, width * 4));
        free(pixels); cui_destroy(ui); cui_sw_destroy(sw);
    }
    return 0;
}

int main(int argc, char **argv) {
    if ((argc == 3 || argc == 4) && !strcmp(argv[1], "--shots")) return features_shots(argv[2], argc == 4 ? atoi(argv[3]) : 840);
    assert(argc == 2);
    cui_app *app = cui_app_create(&(cui_app_desc){ .title = "CUI WebView features", .width = 640, .height = 480 });
    assert(app);
    cui_webview_handle *view = cui_app_webview_create(app);
    assert(view);
    features_check check = {0};
    cui_webview_set_event_handler(view, features_event, &check);
    assert(cui_webview_set_user_agent(view, "CUI-WebView-Test/1"));
    cui_webview_header header = { "X-CUI-Test", "cui-headers" };
    cui_webview_request_opts request = { .headers = &header, .header_count = 1 };
    char url[1024];
    snprintf(url, sizeof url, "%s/features", argv[1]);
    assert(cui_webview_navigate_ex(view, url, &request));
    features_wait(app, view, &check.loaded);
    assert(check.started && check.progress && check.finished && !check.errors);
    assert(cui_webview_get_status(view) == CUI_WEBVIEW_READY);
    cui_webview_load_info info = cui_webview_get_load_info(view);
    assert(info.state == CUI_WEBVIEW_LOADED && info.progress == 1 && strstr(info.url, "/features"));
    features_eval(app, view, &check, "window.testHeaders && navigator.userAgent==='CUI-WebView-Test/1'", "true");
    features_eval(app, view, &check, "1+2", "3");
    features_eval(app, view, &check, "document.body.dataset.reply='from-C';document.body.dataset.reply==='from-C'", "true");
    features_eval(app, view, &check, "throw new Error('intentional test')", NULL);
    for (int i = 0; i < 30 && features_frame(app, view); i++) {}
    assert(check.messages == 1); /* iframe bridge attempts must not reach C */
    features_eval(app, view, &check, "document.cookie.includes('cui_test=present')", "true");
    check.result_done = check.result_ok = false; check.expected = "null";
    assert(cui_webview_clear_cookies(view, features_result, &check));
    features_wait(app, view, &check.result_done); assert(check.result_ok);
    features_eval(app, view, &check, "document.cookie.includes('cui_test=present')", "false");
    features_eval(app, view, &check, "history.pushState({},'', '#changed');true", "true");
    for (int i = 0; i < 5 && features_frame(app, view); i++) {}
    assert(strstr(cui_webview_get_load_info(view).url, "#changed"));
    assert(cui_webview_set_user_agent(view, NULL));
    features_eval(app, view, &check, "navigator.userAgent!=='CUI-WebView-Test/1'", "true");
    check.loaded = false; assert(cui_webview_reload(view));
    features_wait(app, view, &check.loaded); assert(check.finished >= 2);
    assert(cui_webview_stop(view));
    check.loaded = false;
    snprintf(url, sizeof url, "%s/broken", argv[1]);
    assert(cui_webview_navigate(view, url)); features_wait(app, view, &check.loaded);
    info = cui_webview_get_load_info(view);
    assert(check.errors && info.state == CUI_WEBVIEW_LOAD_FAILED && info.error_code && *info.error);
    assert(cui_webview_get_status(view) == CUI_WEBVIEW_READY); /* page errors do not kill the engine */
    assert(cui_webview_eval(view, "42", features_canceled, &check));
    cui_webview_destroy(view);
    for (int i = 0; i < 30 && features_frame(app, NULL); i++) {}
    assert(check.canceled == 0);
    /* Reuse the host window while testing successive browser views. */
    features_idle = true;
    view = cui_app_webview_create(app);
    assert(view);
    bool awake = false;
    cui_webview_set_event_handler(view, features_idle_event, &awake);
    assert(cui_webview_set_html(view, "<!doctype html><script>setTimeout(function(){window.cui.postMessage('wake')},1500)</script>"));
    features_wait(app, view, &awake);
    cui_app_destroy(app);
    puts("webview features: scripts/results, main-frame messages, load/progress/URL/errors, user agent, headers, cookies, reload/stop, pending destruction and idle wake passed");
    return 0;
}
