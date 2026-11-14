/* Real window/GL check; run explicitly with a top-red, bottom-green video. */
#include <cui/cui_mpv.h>
#include <cui/cui_render.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: cui_mpv_smoke media [screenshot.png]\n"); return 1; }
    cui_app *app = cui_app_create(&(cui_app_desc){ .title = "Video smoke", .width = 320, .height = 260 });
    if (!app) return 1;
    int error = 0;
    cui_mpv *p = cui_mpv_create(app, &error);
    if (!p) { fprintf(stderr, "init: %s\n", cui_mpv_error_string(error)); cui_app_destroy(app); return 1; }
    cui_video edited = *cui_mpv_state(p);
    edited.muted = true;
    error = cui_mpv_apply(p, &edited, CUI_PLAYER_MUTE);
    if (error >= 0) error = cui_mpv_load(p, argv[1]);
    int frames = 0, renders = 0, paused = 0;
    bool ok = false;
    while (error >= 0 && frames++ < 300 && cui_app_frame(app)) {
        cui_ctx *ui = cui_app_ui(app);
        renders += cui_mpv_update(p, 160, 96);
        cui_video video = *cui_mpv_state(p);
        if (!paused && video.position > 0.15f && renders >= 3) {
            video.playing = false;
            error = cui_mpv_apply(p, &video, CUI_PLAYER_TOGGLE);
            paused = 1;
        } else if (paused && !video.playing) paused++;
        cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .bg = CUI_COLOR_BACKGROUND) {
            cui_video_player(ui, "video", &video, .width = CUI_FILL, .height = 192);
        }
        error = cui_mpv_last_error(p);
        if (paused >= 8) {
            float scale = cui_dpi_scale(ui);
            cui_vec2 viewport = cui_viewport_size(ui);
            int w = (int)ceilf(viewport.x * scale), h = (int)ceilf(viewport.y * scale);
            unsigned char *pixels = malloc((size_t)w * (size_t)h * 4);
            ok = pixels && cui_app_screenshot(app, pixels, w, h);
            if (ok) {
                const unsigned char *top = pixels + ((int)(40 * scale) * w + w / 2) * 4;
                const unsigned char *bottom = pixels + ((int)(140 * scale) * w + w / 2) * 4;
                ok = top[0] > 150 && top[1] < 100 && bottom[1] > 80 && bottom[0] < 100;
                printf("video pixels: top %u,%u,%u; bottom %u,%u,%u\n", top[0], top[1], top[2], bottom[0], bottom[1], bottom[2]);
                if (argc > 2) ok &= cui_write_png(argv[2], pixels, w, h, w * 4);
            }
            free(pixels);
            ok &= !cui_mpv_update(p, 160, 96);
            break;
        }
        /* A watchdog also makes the pause/idle checks finish without input. */
        cui_request_wakeup(ui, 0.05f);
    }
    printf("video smoke: %s, frames=%d renders=%d paused=%d error=%d renderer=%s\n",
        ok ? "PASS" : "FAIL", frames, renders, paused, error, cui_app_is_software(app) ? "software" : "OpenGL");
    cui_mpv_destroy(p); cui_app_destroy(app);
    return ok && error >= 0 ? 0 : 1;
}
