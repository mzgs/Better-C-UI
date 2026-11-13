/* Media controls only; the host app supplies playback and a frame texture. */
#include <cui/cui_app.h>

typedef struct {
    cui_video video;
    cui_now_playing track;
    bool show_controls;
    const char *status;
} media_demo;

static void media_demo_frame(cui_ctx *ui, void *user) {
    media_demo *demo = user;
    cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 24, .gap = 16, .scroll_y = true,
               .bg = CUI_COLOR_BACKGROUND) {
        cui_heading(ui, "Media UI", 2);
        cui_secondary(ui, "Interactive controls only. Your app supplies video frames and audio.");
        cui_checkbox(ui, "Show video controls", &demo->show_controls);
        int events = cui_video_player(ui, "video", &demo->video, .width = CUI_FILL, .height = 240,
                                      .no_controls = !demo->show_controls);
        if (events) demo->status = events & CUI_PLAYER_FULLSCREEN ? "Fullscreen requested; the host handles window changes." : "Video control state updated.";
        cui_now_playing(ui, "track", &demo->track, .width = CUI_FILL);
        cui_secondary(ui, demo->status);
    }
}

int main(void) {
    media_demo demo = {
        .video = { .position = 76, .duration = 222, .volume = 0.7f },
        .track = { .title = "Field notes", .artist = "Now playing card preview", .art_color = CUI_COLOR_ACCENT, .position = 42, .duration = 180 },
        .show_controls = true,
        .status = "The preview clock stays still; seeking updates the displayed position."
    };
    return cui_app_run(&(cui_app_desc){ .title = "Media UI", .width = 760, .height = 700, .accent = CUI_ACCENT_BLUE }, media_demo_frame, &demo);
}
