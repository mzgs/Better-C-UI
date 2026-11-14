/* Real window/GL check; run explicitly with a top-red, bottom-green video. */
#include <cui/cui_mpv.h>
#include <cui/cui_render.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: cui_mpv_smoke media [screenshot.png] [external.srt]\n"); return 1; }
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
    int subtitle_phase = 0;
    unsigned char *baseline = NULL;
    size_t baseline_size = 0;
    bool ok = false;
    while (error >= 0 && frames++ < 300 && cui_app_frame(app)) {
        cui_ctx *ui = cui_app_ui(app);
        renders += cui_mpv_update(p, 320, 192);
        cui_video video = *cui_mpv_state(p);
        if (!paused && video.position > 0.15f && renders >= 3) {
            video.playing = false;
            error = cui_mpv_apply(p, &video, CUI_PLAYER_TOGGLE);
            if (error >= 0) error = cui_mpv_select_track(p, CUI_MPV_SUBTITLE, CUI_MPV_TRACK_OFF);
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
                if (!subtitle_phase) printf("video pixels: top %u,%u,%u; bottom %u,%u,%u\n",
                    top[0], top[1], top[2], bottom[0], bottom[1], bottom[2]);
                if (argc > 2) ok &= cui_write_png(argv[2], pixels, w, h, w * 4);
            }
            bool finished = true;
            if (argc > 3 && ok) {
                /* Track changes can refresh the clock/seek bar; compare only the video. */
                size_t size = (size_t)w * (size_t)ceilf(192 * scale) * 4;
                if (!subtitle_phase) {
                    baseline = pixels; pixels = NULL; baseline_size = size;
                    error = cui_mpv_load_subtitle(p, argv[3]);
                    subtitle_phase = 1;
                    finished = false;
                } else if (subtitle_phase == 1) {
                    if (size == baseline_size && memcmp(pixels, baseline, size)) {
                        error = cui_mpv_select_track(p, CUI_MPV_SUBTITLE, CUI_MPV_TRACK_OFF);
                        subtitle_phase = 2;
                    }
                    finished = false;
                } else {
                    finished = size == baseline_size && !memcmp(pixels, baseline, size);
                    if (finished) subtitle_phase = 3;
                }
            }
            free(pixels);
            if (finished) { ok &= !cui_mpv_update(p, 320, 192); break; }
            ok = false;
        }
        /* A watchdog also makes the pause/idle checks finish without input. */
        cui_request_wakeup(ui, 0.05f);
    }
    printf("video smoke: %s, frames=%d renders=%d paused=%d error=%d renderer=%s\n",
        ok ? "PASS" : "FAIL", frames, renders, paused, error, cui_app_is_software(app) ? "software" : "OpenGL");
    if (argc > 3) printf("external subtitles rendered then disabled: %s\n", subtitle_phase == 3 ? "PASS" : "FAIL");
    free(baseline);
    cui_mpv_destroy(p); cui_app_destroy(app);
    return ok && error >= 0 && (argc <= 3 || subtitle_phase == 3) ? 0 : 1;
}
