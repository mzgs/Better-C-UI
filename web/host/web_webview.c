/* Native WebView add-on fallback for browser-hosted example apps. */
#include <cui/cui_webview.h>

cui_webview_handle *cui_app_webview_create(cui_app *app) { (void)app; return NULL; }
void cui_webview_destroy(cui_webview_handle *view) { (void)view; }
cui_webview_status cui_webview_get_status(const cui_webview_handle *view) { (void)view; return CUI_WEBVIEW_UNAVAILABLE; }
bool cui_webview_navigate(cui_webview_handle *view, const char *url) { (void)view; (void)url; return false; }
bool cui_webview_set_html(cui_webview_handle *view, const char *html) { (void)view; (void)html; return false; }
bool cui_webview_navigate_ex(cui_webview_handle *view, const char *url, const cui_webview_request_opts *opts) { (void)view; (void)url; (void)opts; return false; }
void cui_webview_set_event_handler(cui_webview_handle *view, cui_webview_event_fn handler, void *user) { (void)view; (void)handler; (void)user; }
cui_webview_load_info cui_webview_get_load_info(const cui_webview_handle *view) { (void)view; return (cui_webview_load_info){ .url = "", .error = "" }; }
bool cui_webview_eval(cui_webview_handle *view, const char *script, cui_webview_result_fn callback, void *user) { (void)view; (void)script; (void)callback; (void)user; return false; }
void cui_webview_set_navigation_handler(cui_webview_handle *view, cui_webview_navigation_fn handler, void *user) { (void)view; (void)handler; (void)user; }
bool cui_webview_add_startup_script(cui_webview_handle *view, const char *script, cui_webview_result_fn callback, void *user) { (void)view; (void)script; (void)callback; (void)user; return false; }
bool cui_webview_set_user_agent(cui_webview_handle *view, const char *user_agent) { (void)view; (void)user_agent; return false; }
bool cui_webview_reload(cui_webview_handle *view) { (void)view; return false; }
bool cui_webview_stop(cui_webview_handle *view) { (void)view; return false; }
bool cui_webview_clear_cookies(cui_webview_handle *view, cui_webview_result_fn callback, void *user) { (void)view; (void)callback; (void)user; return false; }
bool cui_webview_can_go_back(cui_webview_handle *view) { (void)view; return false; }
bool cui_webview_can_go_forward(cui_webview_handle *view) { (void)view; return false; }
bool cui_webview_go_back(cui_webview_handle *view) { (void)view; return false; }
bool cui_webview_go_forward(cui_webview_handle *view) { (void)view; return false; }
