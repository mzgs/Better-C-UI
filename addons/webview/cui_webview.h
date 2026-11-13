/* Optional native engines, called only from the app thread. */
#ifndef CUI_WEBVIEW_INTERNAL_H
#define CUI_WEBVIEW_INTERNAL_H
#include "cui/cui_webview.h"
#include "platform/cui_platform.h"

typedef struct cui__webview {
    cui_webview_handle surface; /* first: public opaque handle */
    struct cui__webview *next;
    cui_app *app;
    void *native;
    char *content;
    bool html;
    char *user_agent;
    cui_webview_header *headers;
    int header_count;
    cui_webview_event_fn handler;
    void *handler_user;
    cui_webview_load_info load;
} cui__webview;

void *cui__webview_native_create(cui__webview *view);
void cui__webview_native_destroy(cui__webview *view);
bool cui__webview_native_load(cui__webview *view);
bool cui__webview_native_history(cui__webview *view, bool forward, bool move);
void cui__webview_native_place(cui__webview *view, bool visible);
void cui__webview_native_pump(void);
void cui__webview_return_focus(cui__webview *view, bool backward);
void cui__webview_claim_focus(cui__webview *view);
bool cui__webview_native_eval(cui__webview *view, const char *script, cui_webview_result_fn callback, void *user);
bool cui__webview_native_user_agent(cui__webview *view);
bool cui__webview_native_control(cui__webview *view, bool stop);
bool cui__webview_native_clear_cookies(cui__webview *view, cui_webview_result_fn callback, void *user);
void cui__webview_native_wake(cui__webview *view);
void cui__webview_result(cui__webview *view, cui_webview_result_fn callback, const char *json, const char *error, void *user);
void cui__webview_emit(cui__webview *view, cui_webview_event_type type, const char *url,
    const char *message, float progress, int error_code);
#endif
