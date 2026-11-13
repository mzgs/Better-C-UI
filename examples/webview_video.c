#include <cui/cui_app.h>
#include <string.h>

enum { VIDEO_PATH_CAP = 4096 };

typedef struct {
    cui_webview_handle *view;
    bool attempted, loaded, failed;
} video_demo;

static bool video_open(cui_webview_handle *view, const char *path) {
    if (!path || !*path || strlen(path) >= VIDEO_PATH_CAP) return false;
    bool drive = ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' && (path[2] == '/' || path[2] == '\\');
    if (!drive && path[0] != '/') return false;
    char url[3 * VIDEO_PATH_CAP + 9];
    strcpy(url, drive ? "file:///" : "file://");
    size_t at = strlen(url);
    static const char hex[] = "0123456789ABCDEF";
    for (const unsigned char *p = (const unsigned char *)path; *p; p++) {
        unsigned char c = drive && *p == '\\' ? '/' : *p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~' || c == '/' || (drive && p == (const unsigned char *)path + 1))
            url[at++] = (char)c;
        else { url[at++] = '%'; url[at++] = hex[c >> 4]; url[at++] = hex[c & 15]; }
    }
    url[at] = 0;
    return cui_webview_navigate(view, url);
}

static void video_frame(cui_ctx *ui, void *user) {
    video_demo *demo = user;
    if (!demo->attempted) {
        demo->attempted = true;
        demo->view = cui_app_webview_create(NULL);
    }
    cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 16, .gap = 12, .bg = CUI_COLOR_BACKGROUND) {
        cui_heading(ui, "HTML5 video", 3);
        if (cui_button(ui, "Choose video", .disabled = !demo->view)) {
            char path[VIDEO_PATH_CAP];
            if (cui_app_file_dialog(NULL, &(cui_file_dialog_opts){ .title = "Choose video" }, path, sizeof path)) {
                demo->failed = !video_open(demo->view, path);
                if (!demo->failed) demo->loaded = true;
            }
        }
        cui_secondary(ui, !demo->view ? "Build with CUI_WEBVIEW=ON and install the platform browser runtime." :
            demo->failed || cui_webview_get_status(demo->view) == CUI_WEBVIEW_ERROR ? "The selected video could not be opened." :
            "Choose a local video, then use the player's controls. Ctrl+Tab returns to CUI.");
        if (demo->loaded || !demo->view) cui_webview(ui, "video", demo->view, .height = -1);
        else cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .justify = CUI_JUSTIFY_CENTER) {
            cui_empty_state(ui, "No video selected", .icon = CUI_ICON_VIDEO,
                .description = "Choose a video from your computer to play it here.");
        }
    }
}

int main(void) {
    video_demo demo = {0};
    return cui_app_run(&(cui_app_desc){ .title = "CUI HTML5 video", .width = 960, .height = 680 }, video_frame, &demo);
}
