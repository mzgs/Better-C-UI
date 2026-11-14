/* cmake -S . -B build -DCUI_VIDEO=ON; ./build/examples/video [file-or-URL] */
#include <cui/cui_mpv.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef struct { const char **items; int capacity; } video_track_menu;

static void video_track_menu_free(cui_ctx *ui, void *data) {
    (void)ui;
    free(((video_track_menu *)data)->items);
}

static int video_track_select(cui_ctx *ui, const char *id, cui_mpv *player, cui_mpv_track_type type) {
    int count = 0, offset = type == CUI_MPV_SUBTITLE ? 2 : 1;
    const cui_mpv_track *tracks = cui_mpv_tracks(player, type, &count);
    cui_push_id(ui, id);
    cui_id state_id = cui_make_id(ui, "menu-storage");
    cui_pop_id(ui);
    video_track_menu *menu = cui_state(ui, state_id, sizeof *menu);
    if (!menu || count > INT_MAX - offset || (size_t)(count + offset) > SIZE_MAX / sizeof *menu->items) {
        cui_secondary(ui, "Track controls unavailable: out of memory."); return 0;
    }
    cui_state_on_free(ui, state_id, video_track_menu_free);
    if (menu->capacity < count + offset) {
        void *items = realloc(menu->items, (size_t)(count + offset) * sizeof *menu->items);
        if (!items) { cui_secondary(ui, "Track controls unavailable: out of memory."); return 0; }
        menu->items = items; menu->capacity = count + offset;
    }
    menu->items[0] = type == CUI_MPV_SUBTITLE ? "Off" : "Automatic";
    if (type == CUI_MPV_SUBTITLE) menu->items[1] = "Automatic";
    int selected = 0;
    for (int i = 0; i < count; i++) {
        menu->items[i + offset] = tracks[i].label;
        if (tracks[i].selected) selected = i + offset;
    }
    if (!cui_select(ui, id, menu->items, count + offset, &selected, .width = CUI_FILL,
        .label = type == CUI_MPV_AUDIO ? "Audio language / track" : "Subtitles", .disabled = !count)) return 0;
    int64_t track = selected >= offset ? tracks[selected - offset].id :
        type == CUI_MPV_SUBTITLE && selected == 0 ? CUI_MPV_TRACK_OFF : CUI_MPV_TRACK_AUTO;
    return cui_mpv_select_track(player, type, track);
}

int main(int argc, char **argv) {
    cui_app *app = cui_app_create(&(cui_app_desc){ .title = "Video player", .width = 880, .height = 700 });
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
        float height = fmaxf(120, viewport.y - 350), scale = cui_dpi_scale(ui);
        float aspect = cui_mpv_state(player)->aspect;
        if (!(aspect > 0)) aspect = 16.0f / 9;
        float width = fminf(fmaxf(1, viewport.x - 32), height * aspect);
        height = fminf(height, width / aspect);
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
            cui_row(ui, .width = CUI_FILL, .gap = 12, .align = CUI_ALIGN_END, .wrap = true) {
                cui_box(ui, .width = CUI_GROW(1), .min_width = 180) {
                    int result = video_track_select(ui, "audio-track", player, CUI_MPV_AUDIO);
                    if (result < 0) error = result;
                }
                cui_box(ui, .width = CUI_GROW(1), .min_width = 180) {
                    int result = video_track_select(ui, "subtitle-track", player, CUI_MPV_SUBTITLE);
                    if (result < 0) error = result;
                }
                if (cui_button(ui, "Open subtitle", .disabled = !video.frame)) {
                    char subtitle[4096];
                    if (cui_app_file_dialog(app, &(cui_file_dialog_opts){ .title = "Open subtitle",
                        .filter_name = "Subtitles", .filter = "srt;ass;ssa;vtt;sub;idx" }, subtitle, sizeof subtitle))
                        error = cui_mpv_load_subtitle(player, subtitle);
                }
            }
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
