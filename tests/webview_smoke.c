/* Real-window smoke test; run explicitly on a desktop with its browser runtime. */
#include "platform/cui_webview.h"
#include <assert.h>
#if defined(__APPLE__)
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#endif
#ifdef CUI_WEBVIEW_GTK_SMOKE
#include <gtk/gtkx.h>
#include <gdk/gdkx.h>
#include <webkit2/webkit2.h>
typedef struct {
    bool dom_ok, pending, snapshot_started, snapshot_done, back_ok, forward_ok;
    int history_page;
    const char *path;
} wv_gtk_check;
static void wv_gtk_script_done(GObject *source, GAsyncResult *result, gpointer data) {
    wv_gtk_check *check = data;
    GError *error = NULL;
    JSCValue *value = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(source), result, &error);
    if (value) {
        char *text = jsc_value_to_string(value);
        check->dom_ok = !error && !strcmp(text, "42");
        g_free(text); g_object_unref(value);
    }
    g_clear_error(&error);
    check->pending = false;
}
static void wv_gtk_snapshot_done(GObject *source, GAsyncResult *result, gpointer data) {
    wv_gtk_check *check = data;
    GError *error = NULL;
    cairo_surface_t *surface = webkit_web_view_get_snapshot_finish(WEBKIT_WEB_VIEW(source), result, &error);
    if (!surface) fprintf(stderr, "WebKitGTK snapshot: %s (allocation %dx%d, visible %d)\n", error ? error->message : "unknown error",
        gtk_widget_get_allocated_width(GTK_WIDGET(source)), gtk_widget_get_allocated_height(GTK_WIDGET(source)), gtk_widget_get_visible(GTK_WIDGET(source)));
    assert(surface && !error);
    assert(cairo_surface_write_to_png(surface, check->path) == CAIRO_STATUS_SUCCESS);
    cairo_surface_destroy(surface);
    check->snapshot_done = true;
}
static void wv_gtk_history_done(GObject *source, GAsyncResult *result, gpointer data) {
    wv_gtk_check *check = data;
    GError *error = NULL;
    JSCValue *value = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(source), result, &error);
    assert(value && !error);
    char *text = jsc_value_to_string(value);
    check->history_page = !strcmp(text, "/first") ? 1 : !strcmp(text, "/second") ? 2 : 0;
    check->pending = false;
    g_free(text); g_object_unref(value);
}
#endif

int main(int argc, char **argv) {
    cui_app *app = cui_app_create(&(cui_app_desc){ .title = "CUI WebView smoke", .width = 480, .height = 400, .continuous = true });
    assert(app);
    cui_webview_handle *view = cui_app_webview_create(app);
    assert(view);
    assert(cui_webview_set_html(view, "<!doctype html><meta charset='utf-8'><style>:root{color-scheme:light dark}body{font:18px system-ui;margin:24px}</style><h1>WebView smoke</h1><input id='value' aria-label='Value'><button id='go' onclick=\"document.title=document.getElementById('value').value\">Apply</button>"));
    bool ready = false;
    int phase = 0;
#if defined(__APPLE__)
    __block bool dom_done = false, dom_ok = false, dom_pending = false, snapshot_done = argc < 2;
    __block bool back_ok = false, forward_ok = false;
    __block bool history_pending = false;
    __block int history_page = 0;
    bool snapshot_started = false;
    WKWebView *native = nil;
#else
    (void)argc; (void)argv;
#endif
#ifdef CUI_WEBVIEW_GTK_SMOKE
    wv_gtk_check check = { .path = argc > 1 ? argv[1] : NULL, .snapshot_done = argc < 2 };
    GtkWidget *plug = NULL;
    WebKitWebView *native = NULL;
    GList *windows = gtk_window_list_toplevels();
    for (GList *item = windows; item; item = item->next) if (GTK_IS_PLUG(item->data)) plug = item->data;
    g_list_free(windows);
    assert(plug);
    native = WEBKIT_WEB_VIEW(gtk_bin_get_child(GTK_BIN(plug)));
#endif
    for (int frame = 0; frame < 1200 && cui_app_frame(app); frame++) {
        cui_ctx *ui = cui_app_ui(app);
        ready |= cui_webview_get_status(view) == CUI_WEBVIEW_READY;
        assert(cui_webview_get_status(view) != CUI_WEBVIEW_ERROR);
        cui_id after = 0;
        cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 16, .gap = 8, .bg = CUI_COLOR_BACKGROUND) {
            cui_button(ui, "Before");
            if (phase < 200) cui_webview(ui, "page", view, .height = -1, .disabled = phase >= 160 && phase < 180);
            cui_button(ui, "After");
            after = cui_last_id(ui);
        }
#if defined(__APPLE__) || defined(CUI_WEBVIEW_GTK_SMOKE)
        if (argc > 2 && (phase == 20 || phase == 30)) {
            char url[512];
            int length = snprintf(url, sizeof url, "%s/%s", argv[2], phase == 20 ? "first" : "second");
            assert(length > 0 && length < (int)sizeof url);
            assert(cui_webview_navigate(view, url));
        }
        if (argc > 2 && phase == 40) assert(cui_webview_can_go_back(view) && cui_webview_go_back(view));
        if (argc > 2 && phase == 50) assert(cui_webview_can_go_forward(view) && cui_webview_go_forward(view));
#endif
#if defined(__APPLE__)
        NSWindow *window = (__bridge NSWindow *)cui_app_native_window(app);
        if (frame == 20) {
            for (NSView *child in window.contentView.subviews) if ([child isKindOfClass:[WKWebView class]]) native = (WKWebView *)child;
            assert(native && !native.hidden);
        }
        if (native && !dom_ok && !dom_pending) {
            dom_pending = true;
            [native evaluateJavaScript:@"document.getElementById('value').value='42'; document.getElementById('go').click(); document.title"
                completionHandler:^(id result, NSError *error) { dom_ok = !error && [result isEqual:@"42"]; dom_done = true; dom_pending = false; }];
        }
        if (dom_ok && !snapshot_started && argc > 1) {
            snapshot_started = true;
            NSString *path = [NSString stringWithUTF8String:argv[1]];
            [native takeSnapshotWithConfiguration:nil completionHandler:^(NSImage *image, NSError *error) {
                assert(image && !error);
                NSBitmapImageRep *rep = [NSBitmapImageRep imageRepWithData:image.TIFFRepresentation];
                assert([[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:path atomically:YES]);
                snapshot_done = true;
            }];
        }
        if (argc > 2 && (phase == 20 || phase == 30 || phase == 40 || phase == 50)) history_page = 0;
        if (argc > 2 && (phase == 21 || phase == 31 || phase == 41 || phase == 51)) {
            if (!history_pending) {
                history_pending = true;
                [native evaluateJavaScript:@"location.pathname" completionHandler:^(id result, NSError *error) {
                    history_page = !error && [result isEqual:@"/first"] ? 1 : !error && [result isEqual:@"/second"] ? 2 : 0;
                    history_pending = false;
                }];
            }
            if (history_page != (phase == 21 || phase == 41 ? 1 : 2)) continue;
            if (phase == 31 && !cui_webview_can_go_back(view)) continue;
            if (phase == 41) back_ok = true;
            if (phase == 51) forward_ok = true;
        }
        if (phase == 80) { ui->focus = view->id; view->focus_requested = true; }
        if (phase == 90) {
            cui__app_render(app); /* native input arrives between CUI frames */
            NSResponder *focused = window.firstResponder;
            assert([focused isKindOfClass:[NSView class]] && [(NSView *)focused isDescendantOf:native]);
            NSEvent *tab = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:NSEventModifierFlagControl
                timestamp:0 windowNumber:window.windowNumber context:nil characters:@"\t" charactersIgnoringModifiers:@"\t" isARepeat:NO keyCode:48];
            assert([native performKeyEquivalent:tab]);
        }
        if (phase == 92) assert(ui->focus == after);
        if (phase == 170 || phase == 210) assert(native.hidden);
        if (phase == 190) assert(!native.hidden);
        if (dom_ok && snapshot_done) phase++;
#endif
#ifdef CUI_WEBVIEW_GTK_SMOKE
        if (argc > 2 && (phase == 20 || phase == 30 || phase == 40 || phase == 50)) check.history_page = 0;
        if (argc > 2 && (phase == 21 || phase == 31 || phase == 41 || phase == 51)) {
            if (!check.pending) {
                check.pending = true;
                webkit_web_view_evaluate_javascript(native, "location.pathname", -1, NULL, NULL, NULL, wv_gtk_history_done, &check);
            }
            if (check.history_page != (phase == 21 || phase == 41 ? 1 : 2)) continue;
            if (phase == 31 && !cui_webview_can_go_back(view)) continue;
            if (phase == 41) check.back_ok = true;
            if (phase == 51) check.forward_ok = true;
        }
        if (phase == 60) {
            XWindowAttributes attributes;
            Display *display = gdk_x11_display_get_xdisplay(gtk_widget_get_display(plug));
            Window window = gdk_x11_window_get_xid(gtk_widget_get_window(plug));
            assert(XGetWindowAttributes(display, window, &attributes) && attributes.map_state == IsViewable);
        }
        if (phase == 80) { ui->focus = view->id; view->focus_requested = true; }
        if (phase == 90) {
            cui__app_render(app);
            GdkEventKey tab = { .type = GDK_KEY_PRESS, .state = GDK_CONTROL_MASK, .keyval = GDK_KEY_Tab };
            gboolean handled = FALSE;
            g_signal_emit_by_name(plug, "key-press-event", &tab, &handled);
            assert(handled);
        }
        if (phase == 92) assert(ui->focus == after);
        if (!check.dom_ok && !check.pending) {
            check.pending = true;
            webkit_web_view_evaluate_javascript(native,
                "document.getElementById('value').value='42'; document.getElementById('go').click(); document.title",
                -1, NULL, NULL, NULL, wv_gtk_script_done, &check);
        }
        if (check.dom_ok && !check.snapshot_started && check.path) {
            check.snapshot_started = true;
            webkit_web_view_get_snapshot(native, WEBKIT_SNAPSHOT_REGION_VISIBLE, WEBKIT_SNAPSHOT_OPTIONS_NONE,
                NULL, wv_gtk_snapshot_done, &check);
        }
        if (phase == 170 || phase == 210) assert(!gtk_widget_get_visible(plug));
        if (phase == 190) assert(gtk_widget_get_visible(plug));
        if (check.dom_ok && check.snapshot_done) phase++;
#endif
#if !defined(__APPLE__) && !defined(CUI_WEBVIEW_GTK_SMOKE)
        if (ready) phase++;
#endif
        if (phase >= 240) break;
    }
    assert(ready);
#if defined(__APPLE__)
    assert(dom_done && dom_ok && snapshot_done && (argc < 3 || (back_ok && forward_ok)));
    puts("webview native: WKWebView DOM, JavaScript, keyboard exit, disable, omission and snapshot checks passed");
    if (argc > 2) puts("webview history: back and forward between local pages passed");
#elif defined(CUI_WEBVIEW_GTK_SMOKE)
    assert(check.dom_ok && check.snapshot_done && (argc < 3 || (check.back_ok && check.forward_ok)));
    puts("webview native: WebKitGTK DOM, JavaScript, keyboard exit, disable, omission and snapshot checks passed");
    if (argc > 2) puts("webview history: back and forward between local pages passed");
#else
    puts("webview native: engine startup and content submission passed");
#endif
    cui_webview_destroy(view);
    /* Also exercise shutdown while a second engine may still be starting. */
    assert(cui_app_webview_create(app));
    cui_app_destroy(app);
    return 0;
}
