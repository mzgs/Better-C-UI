/* Optional native browser add-on. Include this header and link cui::webview. */
#ifndef CUI_WEBVIEW_H
#define CUI_WEBVIEW_H
#include "cui_app.h"

/* The add-on is static, including when the core uses CUI_SHARED. */
#ifndef CUI_WEBVIEW_API
#define CUI_WEBVIEW_API
#endif
#ifdef __cplusplus
extern "C" {
#endif

/* Optional embedded browser (CUI_WEBVIEW=ON): Cocoa/WKWebView,
 * Win32/WebView2, X11/WebKitGTK. Other backends return NULL. Call on the app
 * thread. The app owns handles and destroys them before closing its window.
 * NULL app means the current app. Creation is asynchronous on Windows. */
CUI_WEBVIEW_API cui_webview_handle *cui_app_webview_create(cui_app *app);
/* Invalidates the handle immediately; native cleanup occurs at frame end. */
CUI_WEBVIEW_API void cui_webview_destroy(cui_webview_handle *view);
CUI_WEBVIEW_API cui_webview_status cui_webview_get_status(const cui_webview_handle *view);
typedef enum cui_webview_load_state {
    CUI_WEBVIEW_LOAD_IDLE = 0, CUI_WEBVIEW_LOADING, CUI_WEBVIEW_LOADED, CUI_WEBVIEW_LOAD_FAILED
} cui_webview_load_state;
typedef enum cui_webview_event_type {
    CUI_WEBVIEW_LOAD_STARTED = 0, CUI_WEBVIEW_LOAD_PROGRESS, CUI_WEBVIEW_URL_CHANGED,
    CUI_WEBVIEW_LOAD_FINISHED, CUI_WEBVIEW_LOAD_ERROR, CUI_WEBVIEW_MESSAGE
} cui_webview_event_type;
typedef struct cui_webview_event {
    cui_webview_event_type type;
    const char *url;           /* main-frame URL supplied by the engine */
    const char *message;       /* page message or error description; otherwise empty */
    float progress;           /* 0..1; Windows reports coarse navigation milestones */
    int error_code;            /* platform error code on LOAD_ERROR */
} cui_webview_event;
typedef struct cui_webview_load_info {
    cui_webview_load_state state;
    float progress;
    const char *url, *error;   /* borrowed until the next event or destruction */
    int error_code;
} cui_webview_load_info;
typedef void (*cui_webview_event_fn)(cui_webview_handle *view, const cui_webview_event *event, void *user);
typedef void (*cui_webview_result_fn)(cui_webview_handle *view, const char *json, const char *error, void *user);
/* Callbacks run on the app thread, outside or inside a CUI frame. Event/result
 * strings live only during the callback. A callback may destroy its handle;
 * use cui_app_quit rather than destroying the app inside a callback.
 * Page messages use window.cui.postMessage(string), from the main frame only.
 * Treat messages as untrusted; validate event->url before invoking app actions.
 * NULL handler detaches the callback. */
CUI_WEBVIEW_API void cui_webview_set_event_handler(cui_webview_handle *view, cui_webview_event_fn handler, void *user);
CUI_WEBVIEW_API cui_webview_load_info cui_webview_get_load_info(const cui_webview_handle *view);
/* Executes JavaScript in the main page. Requires READY; wait for LOAD_FINISHED
 * when accessing the DOM. Results are JSON; undefined becomes "null". Unsupported
 * values can produce "null" or a native serialization error. WebView2 also returns
 * "null" for JavaScript exceptions. Promises are not awaited.
 * False means rejected and no callback. Accepted callbacks are suppressed after
 * destruction. NULL callback is allowed for fire-and-forget execution. */
CUI_WEBVIEW_API bool cui_webview_eval(cui_webview_handle *view, const char *script, cui_webview_result_fn callback, void *user);
/* Copies the UTF-8 user agent; NULL/empty resets the engine default. May be set while
 * STARTING and applies to subsequent requests. Other controls require READY. */
CUI_WEBVIEW_API bool cui_webview_set_user_agent(cui_webview_handle *view, const char *user_agent);
CUI_WEBVIEW_API bool cui_webview_reload(cui_webview_handle *view);
CUI_WEBVIEW_API bool cui_webview_stop(cui_webview_handle *view);
/* Clears all cookies in the view's native browser store, which may be shared
 * with other views. Success passes json="null", error=NULL. Callback may run
 * before the function returns; callbacks are suppressed after destruction. */
CUI_WEBVIEW_API bool cui_webview_clear_cookies(cui_webview_handle *view, cui_webview_result_fn callback, void *user);
typedef struct cui_webview_header { const char *name, *value; } cui_webview_header;
typedef struct cui_webview_request_opts {
    int _;
    const cui_webview_header *headers;
    int header_count;
} cui_webview_request_opts;
/* Copies UTF-8 content; while starting, the latest request is queued.
 * navigate accepts absolute http://, https://, file:/// and about:blank URLs.
 * File URLs use forward slashes and URL-escaped paths, with no host name.
 * On Cocoa, local pages may read assets in their containing directory.
 * set_html uses no local-file base URL.
 * true means accepted, not successfully loaded; the engine handles network
 * and certificate errors. Never bypass certificate validation. */
CUI_WEBVIEW_API bool cui_webview_navigate(cui_webview_handle *view, const char *url);
/* Copies additional headers for this HTTP(S) navigation only. Header names
 * must be HTTP tokens; values cannot contain newlines or other controls.
 * No global header injection is installed for subresources or future links. */
CUI_WEBVIEW_API bool cui_webview_navigate_ex(cui_webview_handle *view, const char *url, const cui_webview_request_opts *opts);
CUI_WEBVIEW_API bool cui_webview_set_html(cui_webview_handle *view, const char *html);
/* Native navigation history. All return false for NULL, starting, failed or
 * destroyed handles. go_* returns whether navigation was accepted; it does
 * not wait for the page to load. An empty direction returns false. */
CUI_WEBVIEW_API bool cui_webview_can_go_back(cui_webview_handle *view);
CUI_WEBVIEW_API bool cui_webview_can_go_forward(cui_webview_handle *view);
CUI_WEBVIEW_API bool cui_webview_go_back(cui_webview_handle *view);
CUI_WEBVIEW_API bool cui_webview_go_forward(cui_webview_handle *view);

#ifdef __cplusplus
}
#endif
#endif
