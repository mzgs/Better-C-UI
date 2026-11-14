/* cmake -S . -B build -DCUI_VIDEO=ON; ./build/examples/video [file-or-URL] */
#include <cui/cui_mpv.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    cui_app *app = cui_app_create(&(cui_app_desc){ .title = "Video player", .width = 880, .height = 620 });
    if (!app) return 1;
    int error = 0;
    cui_mpv *player = cui_mpv_create(app, &error);
    if (!player) {
        fprintf(stderr, "Video initialization: %s\n", cui_mpv_error_string(error));
        cui_app_destroy(app);
        return 1;
    }
    char path[4096] = {0};
    if (argc > 1) {
        if (strlen(argv[1]) >= sizeof path) error = -4; /* libmpv: invalid parameter */
        else { memcpy(path, argv[1], strlen(argv[1]) + 1); error = cui_mpv_load(player, path); }
    }
    while (cui_app_frame(app)) {
        cui_ctx *ui = cui_app_ui(app);
        cui_vec2 viewport = cui_viewport_size(ui);
        float height = fmaxf(120, viewport.y - 270), scale = cui_dpi_scale(ui);
        float aspect = cui_mpv_state(player)->aspect;
        if (!(aspect > 0)) aspect = 16.0f / 9;
        float width = fminf(fmaxf(1, viewport.x - 32), height * aspect);
        cui_mpv_update(player, (int)ceilf(width * scale), (int)ceilf(width / aspect * scale));
        cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 16, .gap = 12,
                   .scroll_y = true, .bg = CUI_COLOR_BACKGROUND) {
            cui_heading(ui, "Video player", 2);
            cui_row(ui, .width = CUI_FILL, .gap = 8, .align = CUI_ALIGN_END, .wrap = true) {
                cui_input(ui, "source", path, sizeof path, .label = "File or URL", .width = CUI_FILL);
                if (cui_button(ui, "Play source", .disabled = !*path)) error = cui_mpv_load(player, path);
                if (cui_button(ui, "Open file")) {
                    if (cui_app_file_dialog(app, &(cui_file_dialog_opts){ .title = "Open video" }, path, sizeof path))
                        error = cui_mpv_load(player, path);
                }
            }
            cui_video video = *cui_mpv_state(player);
            int events = cui_video_player(ui, "video", &video, .width = CUI_FILL, .height = height);
            int applied = cui_mpv_apply(player, &video, events);
            if (applied < 0) error = applied;
            if (error >= 0) error = cui_mpv_last_error(player);
            if (error < 0) cui_text(ui, cui_mpv_error_string(error), .color = CUI_COLOR_DANGER);
            else cui_secondary(ui, events & CUI_PLAYER_FULLSCREEN ? "Fullscreen is a host-window request." :
                "Space: play/pause. Left/Right: seek. Up/Down: volume. M: mute.");
        }
    }
    cui_mpv_destroy(player);
    cui_app_destroy(app);
    return 0;
}
