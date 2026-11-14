/* Native desktop regression: exercise the actual video example's button and Escape. */
#include <cui/cui_mpv.h>
#include <cui/cui_render.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int fullscreen_phase, fullscreen_frames;
static double fullscreen_since, fullscreen_started;
static cui_vec2 fullscreen_windowed;
static bool fullscreen_ok, fullscreen_captured;
static const char *fullscreen_shot;

static bool fullscreen_test_capture(cui_app *app) {
    if (!fullscreen_shot) return true;
    const cui_draw_data *draw = cui_get_draw_data(cui_app_ui(app));
    int w = draw->width_px, h = draw->height_px;
    unsigned char *pixels = malloc((size_t)w * h * 4);
    bool ok = pixels && cui_app_screenshot(app, pixels, w, h) &&
        cui_write_png(fullscreen_shot, pixels, w, h, w * 4);
    free(pixels);
    return ok;
}

static bool fullscreen_test_frame(cui_app *app) {
    cui_ctx *ui = cui_app_ui(app);
    if (fullscreen_phase == 2 && cui_app_is_fullscreen(app) &&
        cui_time(ui) - fullscreen_since > 1 && !fullscreen_captured) {
        if (!fullscreen_test_capture(app)) return false;
        fullscreen_captured = true;
    }
    if (!cui_app_frame(app)) return false;
    double now = cui_time(ui);
    if (!fullscreen_frames++) { fullscreen_started = now; fullscreen_windowed = cui_app_size(app); }
    if (now - fullscreen_started > 15) return false;
    bool active = cui_app_is_fullscreen(app);
    cui_vec2 size = cui_app_size(app);
    bool restored = !active && fabsf(size.x - fullscreen_windowed.x) < 1 && fabsf(size.y - fullscreen_windowed.y) < 1;
    cui_push_id(ui, "video");
    cui_rect button = cui_get_rect(ui, cui_make_id(ui, "##fullscreen"));
    cui_pop_id(ui);
    if (button.w > 0 && button.h > 0)
        cui_input_mouse_move(ui, button.x + button.w / 2, button.y + button.h / 2);
    bool click = (fullscreen_phase == 0 && fullscreen_frames > 10) ||
        (fullscreen_phase == 2 && active && fullscreen_captured) ||
        (fullscreen_phase == 4 && restored && now - fullscreen_since > 1);
    if (click) {
        if (button.w <= 0 || button.h <= 0) return false;
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
        fullscreen_phase++; fullscreen_since = now;
    } else if (fullscreen_phase == 1 || fullscreen_phase == 3 || fullscreen_phase == 5) {
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
        fullscreen_phase++; fullscreen_since = now;
    } else if (fullscreen_phase == 6 && active && now - fullscreen_since > 1) {
        cui_input_key(ui, CUI_KEY_ESCAPE, true, 0);
        fullscreen_phase++; fullscreen_since = now;
    } else if (fullscreen_phase == 7) {
        cui_input_key(ui, CUI_KEY_ESCAPE, false, 0);
        fullscreen_phase++;
    } else if (fullscreen_phase == 8 && restored && now - fullscreen_since > 1) {
        fullscreen_ok = true;
        return false;
    }
    cui_request_wakeup(ui, 0.02f);
    return true;
}

#define cui_app_frame fullscreen_test_frame
#define main fullscreen_example_main
#include "../examples/video.c"
#undef main
#undef cui_app_frame

int main(int argc, char **argv) {
    fullscreen_shot = argc > 2 ? argv[2] : NULL;
    char *args[] = { argv[0], argc > 1 ? argv[1] : CUI_TEST_VIDEO_TRACKS, NULL };
    int result = fullscreen_example_main(2, args);
    printf("video fullscreen button, exit button, Escape and window restoration: %s (phase=%d)\n",
        fullscreen_ok ? "PASS" : "FAIL", fullscreen_phase);
    return result || !fullscreen_ok;
}
