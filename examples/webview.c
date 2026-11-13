/* Build with -DCUI_WEBVIEW=ON to display the native browser. */
#include <cui/cui_app.h>

typedef struct {
    cui_webview_handle *view;
    bool attempted, hidden, disabled;
    char url[512];
} webview_demo;

static const char webview_html[] =
    "<!doctype html><html lang='en'><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<style>:root{color-scheme:light dark}body{font:16px system-ui;margin:24px;line-height:1.6}"
    "input,button{font:inherit;padding:8px;max-width:100%;box-sizing:border-box}</style>"
    "<h1>Embedded web content</h1><p>This page runs in the system browser engine.</p>"
    "<label>Your name <input placeholder='Type here'></label> "
    "<button onclick='document.getElementById(\"result\").textContent=\"JavaScript works.\"'>Try JavaScript</button>"
    "<p id='result' aria-live='polite'>Tab moves between the page controls.</p>"
    "<p>Ctrl+Tab returns to the surrounding CUI controls.</p></html>";

static void webview_frame(cui_ctx *ui, void *user) {
    webview_demo *demo = user;
    if (!demo->attempted) {
        demo->attempted = true;
        demo->view = cui_app_webview_create(NULL);
        cui_webview_navigate(demo->view, demo->url);
    }
    cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 16, .gap = 12, .bg = CUI_COLOR_BACKGROUND) {
        cui_heading(ui, "Web view", 3);
        cui_row(ui, .width = CUI_FILL, .gap = 8, .align = CUI_ALIGN_END) {
            if (cui_button(ui, "Back", .disabled = !cui_webview_can_go_back(demo->view))) cui_webview_go_back(demo->view);
            if (cui_button(ui, "Forward", .disabled = !cui_webview_can_go_forward(demo->view))) cui_webview_go_forward(demo->view);
            cui_input(ui, "url", demo->url, sizeof demo->url, .width = CUI_FILL, .label = "URL");
            if (cui_button(ui, "Go")) cui_webview_navigate(demo->view, demo->url);
        }
        cui_row(ui, .gap = 16, .wrap = true) {
            if (cui_button(ui, "Local page")) cui_webview_set_html(demo->view, webview_html);
            cui_checkbox(ui, "Hide browser", &demo->hidden);
            cui_checkbox(ui, "Disable browser", &demo->disabled);
        }
        cui_secondary(ui, cui_webview_get_status(demo->view) == CUI_WEBVIEW_UNAVAILABLE ?
            "Native browser unavailable. Build with CUI_WEBVIEW=ON and install the platform runtime." :
            cui_webview_get_status(demo->view) == CUI_WEBVIEW_ERROR ? "Browser engine failed." :
            "Tab enters the page. Ctrl+Tab returns; Ctrl+Shift+Tab goes back.");
        if (!demo->hidden) cui_webview(ui, "browser", demo->view, .height = -1, .disabled = demo->disabled);
    }
    /* Page scripts and navigation can change history without CUI input. */
    if (demo->view) cui_request_wakeup(ui, 0.1f);
}

int main(void) {
    webview_demo demo = { .url = "https://www.google.com" };
    return cui_app_run(&(cui_app_desc){ .title = "CUI Web view", .width = 840, .height = 640 }, webview_frame, &demo);
}
