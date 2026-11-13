/* Exercise the example's chooser without opening a native dialog. */
#include "platform/cui_webview.h"
#include <cui/cui_render.h>
#include <assert.h>

static cui__webview video_test_view;
static const char *video_test_path;
static char video_test_url[16384];
static int video_test_dialogs, video_test_loads;
static bool video_test_accept = true;

static cui_webview_handle *video_test_create(cui_app *app) { (void)app; return &video_test_view.surface; }
static bool video_test_navigate(cui_webview_handle *view, const char *url) {
    assert(view == &video_test_view.surface);
    assert(strlen(url) < sizeof video_test_url);
    strcpy(video_test_url, url); video_test_loads++;
    return video_test_accept;
}
static bool video_test_dialog(cui_app *app, const cui_file_dialog_opts *opts, char *out, size_t cap) {
    (void)app;
    assert(opts->kind == CUI_FILE_OPEN && !opts->filter && !opts->filter_name);
    video_test_dialogs++;
    if (!video_test_path) return false;
    assert(strlen(video_test_path) < cap);
    strcpy(out, video_test_path);
    return true;
}

#define cui_app_webview_create video_test_create
#define cui_webview_navigate video_test_navigate
#define cui_app_file_dialog video_test_dialog
#define main video_test_example_main
#include "../examples/webview_video.c"
#undef main
#undef cui_app_webview_create
#undef cui_webview_navigate
#undef cui_app_file_dialog

static void video_test_frame(cui_ctx *ui, video_demo *demo) {
    cui_begin_frame(ui, 1.0f / 60); video_frame(ui, demo); cui_end_frame(ui);
}
static void video_test_click(cui_ctx *ui, video_demo *demo) {
    cui_rect r = cui_get_rect(ui, cui_make_id(ui, "Choose video"));
    assert(r.w > 0 && r.h > 0);
    cui_input_mouse_move(ui, r.x + r.w / 2, r.y + r.h / 2);
    video_test_frame(ui, demo);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true); video_test_frame(ui, demo);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false); video_test_frame(ui, demo);
}

int main(int argc, char **argv) {
    cui_sw_renderer *sw = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw), .theme = CUI_THEME_LIGHT });
    video_test_view.surface.ui = ui; video_test_view.surface.status = CUI_WEBVIEW_READY;
    cui_set_viewport(ui, 960, 680, 1);
    video_demo demo = {0};
    video_test_frame(ui, &demo); video_test_frame(ui, &demo);
    assert(demo.view && !demo.loaded && video_test_loads == 0);
    if (argc == 2) for (int dark = 0; dark < 2; dark++) {
        cui_set_theme(ui, dark ? CUI_THEME_DARK : CUI_THEME_LIGHT);
        cui_set_reduced_motion(ui, true); video_test_frame(ui, &demo);
        const cui_draw_data *draw = cui_get_draw_data(ui);
        unsigned char *pixels = malloc(960 * 680 * 4);
        assert(pixels);
        cui_sw_render(sw, draw, pixels, 960, 680, 960 * 4, CUI_SW_RGBA);
        char path[1024];
        int n = snprintf(path, sizeof path, "%s/webview-video%s.png", argv[1], dark ? "-dark" : "");
        assert(n > 0 && n < (int)sizeof path && cui_write_png(path, pixels, 960, 680, 960 * 4));
        free(pixels);
    }
    video_test_click(ui, &demo); /* cancel leaves the empty state intact */
    assert(video_test_dialogs == 1 && !demo.loaded && video_test_loads == 0);
    video_test_path = "/tmp/a b#%?\xc3\xb6.mp4";
    video_test_click(ui, &demo);
    assert(demo.loaded && !demo.failed && !strcmp(video_test_url, "file:///tmp/a%20b%23%25%3F%C3%B6.mp4"));
    video_test_path = NULL; video_test_click(ui, &demo);
    assert(demo.loaded && video_test_loads == 1);
    video_test_path = "/tmp/second.mkv"; video_test_click(ui, &demo);
    assert(video_test_loads == 2 && !strcmp(video_test_url, "file:///tmp/second.mkv"));
    video_test_accept = false; video_test_click(ui, &demo);
    assert(demo.loaded && demo.failed);
    video_test_accept = true;
    assert(video_open(demo.view, "C:\\Videos\\a b.mp4") && !strcmp(video_test_url, "file:///C:/Videos/a%20b.mp4"));
    assert(video_open(demo.view, "/tmp/a\\b.mp4") && !strcmp(video_test_url, "file:///tmp/a%5Cb.mp4"));
    assert(!video_open(demo.view, NULL) && !video_open(demo.view, "") && !video_open(demo.view, "relative.mp4"));
    char long_path[VIDEO_PATH_CAP + 1]; memset(long_path, '%', sizeof long_path); long_path[0] = '/'; long_path[VIDEO_PATH_CAP] = 0;
    assert(!video_open(demo.view, long_path));
    long_path[VIDEO_PATH_CAP - 1] = 0; assert(video_open(demo.view, long_path));
    cui_destroy(ui); cui_sw_destroy(sw);
    puts("video chooser: cancel, selection, replacement, errors and escaped paths passed");
    return 0;
}
