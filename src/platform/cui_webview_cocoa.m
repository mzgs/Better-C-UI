/* WKWebView is supplied by macOS; no browser SDK is bundled with CUI. */
#include "cui_webview.h"
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@interface CUIWebView : WKWebView
@property (nonatomic, assign) cui__webview *owner;
@end

@implementation CUIWebView
- (BOOL)performKeyEquivalent:(NSEvent *)event {
    NSResponder *responder = self.window.firstResponder;
    bool inside = [responder isKindOfClass:[NSView class]] && [(NSView *)responder isDescendantOf:self];
    if (inside && self.owner && event.keyCode == 48 && (event.modifierFlags & NSEventModifierFlagControl)) {
        [self.window makeFirstResponder:self.window.contentView];
        cui__webview_return_focus(self.owner, (event.modifierFlags & NSEventModifierFlagShift) != 0);
        return YES;
    }
    return [super performKeyEquivalent:event];
}
@end

void *cui__webview_native_create(cui__webview *v) {
    if (![NSThread isMainThread]) return NULL;
    NSWindow *window = (__bridge NSWindow *)cui_app_native_window(v->app);
    CUIWebView *web = [[CUIWebView alloc] initWithFrame:NSZeroRect];
    if (!web || !window.contentView) return NULL;
    web.owner = v;
    web.hidden = YES;
    [window.contentView addSubview:web];
    v->surface.status = CUI_WEBVIEW_READY;
    return (__bridge_retained void *)web;
}

void cui__webview_native_destroy(cui__webview *v) {
    CUIWebView *web = (__bridge_transfer CUIWebView *)v->native;
    web.owner = NULL;
    [web stopLoading];
    if ([web.window.firstResponder isKindOfClass:[NSView class]] &&
        [(NSView *)web.window.firstResponder isDescendantOf:web])
        [web.window makeFirstResponder:web.window.contentView];
    [web removeFromSuperview];
    v->native = NULL;
}

bool cui__webview_native_load(cui__webview *v) {
    CUIWebView *web = (__bridge CUIWebView *)v->native;
    NSString *text = [NSString stringWithUTF8String:v->content];
    if (!text) return false;
    if (v->html) return [web loadHTMLString:text baseURL:nil] != nil;
    NSURL *url = [NSURL URLWithString:text];
    /* Local HTML needs read access to sibling media, not the whole filesystem. */
    if (url.isFileURL) return [web loadFileURL:url allowingReadAccessToURL:url.URLByDeletingLastPathComponent] != nil;
    return url && [web loadRequest:[NSURLRequest requestWithURL:url]] != nil;
}

bool cui__webview_native_history(cui__webview *v, bool forward, bool move) {
    CUIWebView *web = (__bridge CUIWebView *)v->native;
    if (!(forward ? web.canGoForward : web.canGoBack)) return false;
    if (!move) return true;
    return (forward ? [web goForward] : [web goBack]) != nil;
}

void cui__webview_native_place(cui__webview *v, bool visible) {
    CUIWebView *web = (__bridge CUIWebView *)v->native;
    NSView *parent = web.superview;
    cui_rect r = v->surface.rect;
    if (!parent.isFlipped) r.y = (float)parent.bounds.size.height - r.y - r.h;
    web.frame = NSMakeRect(r.x, r.y, r.w, r.h);
    if (!visible && !web.hidden && [web.window.firstResponder isKindOfClass:[NSView class]] &&
        [(NSView *)web.window.firstResponder isDescendantOf:web]) {
        [web.window makeFirstResponder:parent];
        cui_input_focus(v->surface.ui, false);
        cui_input_focus(v->surface.ui, true);
    }
    web.hidden = !visible;
    if (visible && v->surface.focus_requested) [web.window makeFirstResponder:web];
    if (visible && [web.window.firstResponder isKindOfClass:[NSView class]] &&
        [(NSView *)web.window.firstResponder isDescendantOf:web]) cui__webview_claim_focus(v);
}

void cui__webview_native_pump(void) {}
