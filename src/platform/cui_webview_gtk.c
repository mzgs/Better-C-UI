/* WebKitGTK 4.1 / GTK3, embedded as an X11 child of the CUI window. */
#include "cui_webview.h"
#include <gtk/gtkx.h>
#include <gdk/gdkx.h>
#include <webkit2/webkit2.h>

typedef struct {
    GtkWidget *plug, *web;
    Display *display;
    Window parent, window;
    bool visible;
} cui__webview_gtk;

static void cui__webview_gtk_focus(cui__webview_gtk *g, Window window) {
    XWindowAttributes attributes;
    if (XGetWindowAttributes(g->display, window, &attributes) && attributes.map_state == IsViewable)
        XSetInputFocus(g->display, window, RevertToParent, CurrentTime);
}

static gboolean cui__webview_gtk_key(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget;
    cui__webview *v = data;
    if ((event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_ISO_Left_Tab) && (event->state & GDK_CONTROL_MASK)) {
        cui__webview_gtk *g = v->native;
        cui__webview_gtk_focus(g, g->parent);
        XFlush(g->display);
        cui__webview_return_focus(v, (event->state & GDK_SHIFT_MASK) != 0);
        return TRUE;
    }
    return FALSE;
}

void *cui__webview_native_create(cui__webview *v) {
    if (!gdk_display_get_default()) gdk_set_allowed_backends("x11");
    if (!gtk_init_check(NULL, NULL) || !GDK_IS_X11_DISPLAY(gdk_display_get_default())) return NULL;
    cui__webview_gtk *g = calloc(1, sizeof *g);
    if (!g) return NULL;
    g->display = gdk_x11_display_get_xdisplay(gdk_display_get_default());
    g->parent = (Window)(uintptr_t)cui_app_native_window(v->app);
    g->plug = gtk_plug_new(0);
    g->web = webkit_web_view_new();
    gtk_container_add(GTK_CONTAINER(g->plug), g->web);
    gtk_widget_realize(g->plug);
    g->window = gdk_x11_window_get_xid(gtk_widget_get_window(g->plug));
    XReparentWindow(g->display, g->window, g->parent, 0, 0);
    XFlush(g->display);
    g_signal_connect(g->plug, "key-press-event", G_CALLBACK(cui__webview_gtk_key), v);
    v->surface.status = CUI_WEBVIEW_READY;
    return g;
}

void cui__webview_native_destroy(cui__webview *v) {
    cui__webview_gtk *g = v->native;
    if (g->visible) {
        Window focused; int revert;
        XGetInputFocus(g->display, &focused, &revert);
        if (focused == g->window) cui__webview_gtk_focus(g, g->parent);
    }
    gtk_widget_destroy(g->plug);
    free(g);
    v->native = NULL;
}

bool cui__webview_native_load(cui__webview *v) {
    cui__webview_gtk *g = v->native;
    if (v->html) webkit_web_view_load_html(WEBKIT_WEB_VIEW(g->web), v->content, NULL);
    else webkit_web_view_load_uri(WEBKIT_WEB_VIEW(g->web), v->content);
    return true;
}

bool cui__webview_native_history(cui__webview *v, bool forward, bool move) {
    cui__webview_gtk *g = v->native;
    WebKitWebView *web = WEBKIT_WEB_VIEW(g->web);
    if (!(forward ? webkit_web_view_can_go_forward(web) : webkit_web_view_can_go_back(web))) return false;
    if (move) {
        if (forward) webkit_web_view_go_forward(web);
        else webkit_web_view_go_back(web);
    }
    return true;
}

void cui__webview_native_place(cui__webview *v, bool visible) {
    cui__webview_gtk *g = v->native;
    cui_rect r = v->surface.rect;
    float scale = v->surface.ui->scale;
    int w = CUI_MAX(1, (int)lroundf(r.w * scale)), h = CUI_MAX(1, (int)lroundf(r.h * scale));
    int gtk_scale = gtk_widget_get_scale_factor(g->plug);
    if (visible != g->visible) {
        if (visible) gtk_widget_show_all(g->plug);
        else {
            Window focused; int revert;
            XGetInputFocus(g->display, &focused, &revert);
            if (focused == g->window) cui__webview_gtk_focus(g, g->parent);
            gtk_widget_hide(g->plug);
        }
        g->visible = visible;
    }
    /* A CUI parent has no GtkSocket to allocate the plug's children. */
    GtkAllocation allocation = {0, 0, CUI_MAX(1, w / gtk_scale), CUI_MAX(1, h / gtk_scale)};
    gtk_widget_size_allocate(g->plug, &allocation);
    XMoveResizeWindow(g->display, g->window, (int)lroundf(r.x * scale), (int)lroundf(r.y * scale), (unsigned)w, (unsigned)h);
    /* GtkPlug asks its socket to map it; CUI is that native parent. */
    if (visible) XMapWindow(g->display, g->window);
    else XUnmapWindow(g->display, g->window);
    if (visible && v->surface.focus_requested) {
        gtk_widget_grab_focus(g->web);
        cui__webview_gtk_focus(g, g->window);
    }
    if (visible) {
        Window focused; int revert;
        XGetInputFocus(g->display, &focused, &revert);
        if (focused == g->window) cui__webview_claim_focus(v);
    }
    XFlush(g->display);
}

void cui__webview_native_pump(void) {
    /* Bound work per CUI frame even while a page continuously posts events. */
    for (int i = 0; i < 64 && g_main_context_pending(NULL); i++) g_main_context_iteration(NULL, FALSE);
}
