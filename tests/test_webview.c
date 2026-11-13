/* Exercise the real shared lifecycle with a small native-engine substitute. */
#include "platform/cui_webview.h"
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
    char html[] = "<p>First</p>";
    assert(cui_webview_set_html(v, html));
    html[3] = 'X';
    assert(!strcmp(((cui__webview *)v)->content, "<p>First</p>"));
    assert(cui_webview_navigate(v, "https://example.com/"));
    assert(!((cui__webview *)v)->html && wv_loaded == 0);
    const char *bad[] = { "", "javascript:alert(1)", "file:///etc/passwd", "https://", "http:///x", "https://?x", "https://a/\n", "https://a/\xc0\xaf", "https://a/\xed\xa0\x80", "https://a/\xf4\x90\x80\x80", "https://a/\xff" };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) assert(!cui_webview_navigate(v, bad[i]));
    assert(!cui_webview_set_html(v, "\xf0\x80\x80\x80"));
    assert(!cui_webview_set_html(v, "\xe2\x82"));
    assert(!cui_webview_navigate(NULL, "https://example.com"));
    assert(!cui_webview_set_html(v, NULL));
    assert(cui_webview_get_status(NULL) == CUI_WEBVIEW_UNAVAILABLE);
    wv_frame(&app, v, false, false, false);
    assert(!wv_visible); /* asynchronous startup */
    v->status = CUI_WEBVIEW_READY;
    assert(!cui_webview_can_go_back(v) && !cui_webview_go_back(v));
    assert(!cui_webview_can_go_forward(v) && !cui_webview_go_forward(v));
    wv_history_at = wv_history_last = 1;
    assert(cui_webview_can_go_back(v) && !cui_webview_can_go_forward(v));
    assert(cui_webview_go_back(v) && wv_history_at == 0);
    assert(!cui_webview_can_go_back(v) && cui_webview_can_go_forward(v));
    assert(cui_webview_go_forward(v) && wv_history_at == 1);
    assert(cui_webview_set_html(v, "<p>Merhaba \xef\xbf\xbd \xf0\x9f\x8c\x8d</p>"));
    assert(wv_loaded == 1);
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
    assert(!cui_webview_set_html(v, ""));
    assert(cui_webview_get_status(v) == CUI_WEBVIEW_ERROR);
    assert(!cui_webview_can_go_back(v) && !cui_webview_go_back(v));
    wv_fail = false;
    assert(cui_webview_navigate(v, "about:blank"));
    assert(cui_webview_get_status(v) == CUI_WEBVIEW_READY);
    cui_begin_frame(app.ui, 1.0f / 60);
    cui_webview(app.ui, "page", v);
    cui_webview_destroy(v); /* queued paint data must remain valid until frame end */
    assert(cui_webview_get_status(v) == CUI_WEBVIEW_UNAVAILABLE);
    assert(!cui_webview_can_go_back(v) && !cui_webview_go_forward(v));
    cui_end_frame(app.ui); app.webviews.sync(&app);
    assert(wv_destroyed == 1 && !app.webviews.head);
    assert(cui_app_webview_create(&app));
    app.webviews.destroy(&app);
    assert(wv_destroyed == 2);
    cui_destroy(app.ui);
    puts("webview: content, UTF-8, placement, visibility and lifecycle checks passed");
    return 0;
}
