/* Build with -DCUI_WEBVIEW=ON to display the native browser. */
#include <cui/cui_webview.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    cui_webview_handle *view;
    bool attempted, hidden, disabled;
    char url[512], script[512], user_agent[256], result[512];
} webview_demo;

static const char webview_html[] =
    "<!doctype html><html lang='en'><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<style>:root{color-scheme:light dark}body{font:16px system-ui;margin:24px;line-height:1.6}"
    "input,button{font:inherit;padding:8px;max-width:100%;box-sizing:border-box}</style>"
    "<h1>Embedded web content</h1><p>This page runs in the system browser engine.</p>"
    "<label>Your name <input placeholder='Type here'></label> "
    "<button onclick='document.getElementById(\"result\").textContent=\"JavaScript works.\"'>Try JavaScript</button>"
    "<button onclick='window.cui.postMessage(\"Hello from the page\")'>Send message to C</button>"
    "<p id='result' aria-live='polite'>Tab moves between the page controls.</p>"
    "<p>Ctrl+Tab returns to the surrounding CUI controls.</p></html>";

static void webview_event(cui_webview_handle *view, const cui_webview_event *event, void *user) {
    (void)view;
    webview_demo *demo = user;
    if (event->type == CUI_WEBVIEW_LOAD_STARTED) demo->result[0] = 0;
    if (event->type == CUI_WEBVIEW_URL_CHANGED && *event->url) snprintf(demo->url, sizeof demo->url, "%s", event->url);
    if (event->type == CUI_WEBVIEW_MESSAGE) snprintf(demo->result, sizeof demo->result, "Page message: %s", event->message);
    if (event->type == CUI_WEBVIEW_LOAD_ERROR) snprintf(demo->result, sizeof demo->result, "Load error %d: %s", event->error_code, event->message);
}
static void webview_result(cui_webview_handle *view, const char *json, const char *error, void *user) {
    (void)view;
    webview_demo *demo = user;
    snprintf(demo->result, sizeof demo->result, "%s: %s", error ? "Script error" : "Script result", error ? error : json);
}
static void webview_cookies(cui_webview_handle *view, const char *json, const char *error, void *user) {
    (void)view; (void)json;
    webview_demo *demo = user;
    snprintf(demo->result, sizeof demo->result, "%s", error ? error : "Cookies cleared");
}

static void webview_frame(cui_ctx *ui, void *user) {
    webview_demo *demo = user;
    if (!demo->attempted) {
        demo->attempted = true;
        demo->view = cui_app_webview_create(NULL);
        cui_webview_set_event_handler(demo->view, webview_event, demo);
        cui_webview_navigate(demo->view, demo->url);
    }
    cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 16, .gap = 12, .bg = CUI_COLOR_BACKGROUND) {
        cui_heading(ui, "Web view", 3);
        bool ready = cui_webview_get_status(demo->view) == CUI_WEBVIEW_READY;
        cui_webview_load_info load = cui_webview_get_load_info(demo->view);
        cui_row(ui, .width = CUI_FILL, .gap = 8, .align = CUI_ALIGN_END) {
            if (cui_button(ui, "Back", .disabled = !cui_webview_can_go_back(demo->view))) cui_webview_go_back(demo->view);
            if (cui_button(ui, "Forward", .disabled = !cui_webview_can_go_forward(demo->view))) cui_webview_go_forward(demo->view);
            cui_input(ui, "url", demo->url, sizeof demo->url, .width = CUI_FILL, .label = "URL");
            if (cui_button(ui, "Go")) cui_webview_navigate(demo->view, demo->url);
        }
        cui_row(ui, .gap = 16, .wrap = true) {
            if (cui_button(ui, "Local page")) cui_webview_set_html(demo->view, webview_html);
            if (cui_button(ui, "Reload", .disabled = !ready)) cui_webview_reload(demo->view);
            if (cui_button(ui, "Stop", .disabled = !ready || load.state != CUI_WEBVIEW_LOADING)) cui_webview_stop(demo->view);
            if (cui_button(ui, "Clear cookies", .disabled = !ready)) cui_webview_clear_cookies(demo->view, webview_cookies, demo);
            cui_checkbox(ui, "Hide browser", &demo->hidden);
            cui_checkbox(ui, "Disable browser", &demo->disabled);
        }
        cui_row(ui, .width = CUI_FILL, .gap = 8, .align = CUI_ALIGN_END) {
            cui_input(ui, "script", demo->script, sizeof demo->script, .width = CUI_FILL, .label = "JavaScript");
            if (cui_button(ui, "Run JavaScript", .disabled = !ready))
                cui_webview_eval(demo->view, demo->script, webview_result, demo);
        }
        cui_row(ui, .width = CUI_FILL, .gap = 8, .align = CUI_ALIGN_END) {
            cui_input(ui, "user-agent", demo->user_agent, sizeof demo->user_agent, .width = CUI_FILL, .label = "User agent (empty resets)");
            if (cui_button(ui, "Apply user agent", .disabled = !demo->view) && !cui_webview_set_user_agent(demo->view, demo->user_agent))
                snprintf(demo->result, sizeof demo->result, "Could not apply user agent");
        }
        if (load.state == CUI_WEBVIEW_LOADING) cui_progress(ui, load.progress, .label = "Loading page", .show_value = true);
        if (*demo->result) cui_text(ui, demo->result);
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
    webview_demo demo = { .url = "https://www.google.com", .script = "document.title" };
    return cui_app_run(&(cui_app_desc){ .title = "CUI Web view", .width = 840, .height = 640 }, webview_frame, &demo);
}
