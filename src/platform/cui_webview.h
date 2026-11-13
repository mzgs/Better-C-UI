/* Optional native engines, called only from the app thread. */
#ifndef CUI_WEBVIEW_INTERNAL_H
#define CUI_WEBVIEW_INTERNAL_H
#include "cui_platform.h"

typedef struct cui__webview {
    cui_webview_handle surface; /* first: public opaque handle */
    struct cui__webview *next;
    cui_app *app;
    void *native;
    char *content;
    bool html;
} cui__webview;

void *cui__webview_native_create(cui__webview *view);
void cui__webview_native_destroy(cui__webview *view);
bool cui__webview_native_load(cui__webview *view);
bool cui__webview_native_history(cui__webview *view, bool forward, bool move);
void cui__webview_native_place(cui__webview *view, bool visible);
void cui__webview_native_pump(void);
void cui__webview_return_focus(cui__webview *view, bool backward);
void cui__webview_claim_focus(cui__webview *view);
#endif
