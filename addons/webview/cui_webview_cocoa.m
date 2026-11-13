/* WKWebView is supplied by macOS; no browser SDK is bundled with CUI. */
#include "cui_webview.h"
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@interface CUIWebView : WKWebView <WKNavigationDelegate, WKScriptMessageHandler>
@property (nonatomic, assign) cui__webview *owner;
@property (nonatomic, strong) WKNavigation *activeNavigation;
@end

@implementation CUIWebView
- (void)webView:(WKWebView *)web didStartProvisionalNavigation:(WKNavigation *)navigation {
    self.activeNavigation = navigation;
    if (self.owner) cui__webview_emit(self.owner, CUI_WEBVIEW_LOAD_STARTED, web.URL.absoluteString.UTF8String, NULL, 0, 0);
}
- (void)webView:(WKWebView *)web didFinishNavigation:(WKNavigation *)navigation {
    if (self.owner && navigation == self.activeNavigation)
        cui__webview_emit(self.owner, CUI_WEBVIEW_LOAD_FINISHED, web.URL.absoluteString.UTF8String, NULL, 1, 0);
}
- (void)webView:(WKWebView *)web didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    if (self.owner && navigation == self.activeNavigation)
        cui__webview_emit(self.owner, CUI_WEBVIEW_LOAD_ERROR, web.URL.absoluteString.UTF8String,
            error.localizedDescription.UTF8String, (float)web.estimatedProgress, (int)error.code);
}
- (void)webView:(WKWebView *)web didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    if (self.owner && navigation == self.activeNavigation) {
        NSString *url = error.userInfo[NSURLErrorFailingURLStringErrorKey] ?: web.URL.absoluteString;
        cui__webview_emit(self.owner, CUI_WEBVIEW_LOAD_ERROR, url.UTF8String,
            error.localizedDescription.UTF8String, (float)web.estimatedProgress, (int)error.code);
    }
}
- (void)observeValueForKeyPath:(NSString *)path ofObject:(id)object change:(NSDictionary *)change context:(void *)context {
    (void)object; (void)change; (void)context;
    if (!self.owner) return;
    if ([path isEqualToString:@"URL"])
        cui__webview_emit(self.owner, CUI_WEBVIEW_URL_CHANGED, self.URL.absoluteString.UTF8String, NULL, (float)self.estimatedProgress, 0);
    else if ([path isEqualToString:@"estimatedProgress"] && self.owner->load.state == CUI_WEBVIEW_LOADING)
        cui__webview_emit(self.owner, CUI_WEBVIEW_LOAD_PROGRESS, self.URL.absoluteString.UTF8String, NULL, (float)self.estimatedProgress, 0);
}
- (void)userContentController:(WKUserContentController *)controller didReceiveScriptMessage:(WKScriptMessage *)message {
    (void)controller;
    if (self.owner && message.frameInfo.mainFrame && [message.body isKindOfClass:[NSString class]])
        cui__webview_emit(self.owner, CUI_WEBVIEW_MESSAGE, message.frameInfo.request.URL.absoluteString.UTF8String,
            [(NSString *)message.body UTF8String], (float)self.estimatedProgress, 0);
}
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
    web.navigationDelegate = web;
    NSString *bridge = @"window.cui=Object.freeze({postMessage:function(message){if(typeof message!=='string')throw new TypeError('Expected a string');window.webkit.messageHandlers.cui.postMessage(message);}});";
    WKUserContentController *controller = web.configuration.userContentController;
    [controller addScriptMessageHandler:web name:@"cui"];
    [controller addUserScript:[[WKUserScript alloc] initWithSource:bridge injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]];
    [web addObserver:web forKeyPath:@"URL" options:0 context:NULL];
    [web addObserver:web forKeyPath:@"estimatedProgress" options:0 context:NULL];
    web.hidden = YES;
    [window.contentView addSubview:web];
    v->surface.status = CUI_WEBVIEW_READY;
    return (__bridge_retained void *)web;
}

void cui__webview_native_destroy(cui__webview *v) {
    CUIWebView *web = (__bridge_transfer CUIWebView *)v->native;
    web.owner = NULL;
    web.navigationDelegate = nil;
    [web removeObserver:web forKeyPath:@"URL"];
    [web removeObserver:web forKeyPath:@"estimatedProgress"];
    [web.configuration.userContentController removeScriptMessageHandlerForName:@"cui"];
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
    if (!url) return false;
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
    for (int i = 0; i < v->header_count; i++)
        [request setValue:[NSString stringWithUTF8String:v->headers[i].value] forHTTPHeaderField:[NSString stringWithUTF8String:v->headers[i].name]];
    return [web loadRequest:request] != nil;
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

bool cui__webview_native_eval(cui__webview *v, const char *script, cui_webview_result_fn callback, void *user) {
    CUIWebView *web = (__bridge CUIWebView *)v->native;
    [web evaluateJavaScript:[NSString stringWithUTF8String:script] completionHandler:^(id result, NSError *error) {
        if (!web.owner || web.owner->surface.dead || !callback) return;
        if (error) { cui__webview_result(web.owner, callback, NULL, error.localizedDescription.UTF8String, user); return; }
        NSData *data = [NSJSONSerialization dataWithJSONObject:result ?: NSNull.null options:NSJSONWritingFragmentsAllowed error:NULL];
        NSString *json = data ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : @"null";
        cui__webview_result(web.owner, callback, json.UTF8String, NULL, user);
    }];
    return true;
}

bool cui__webview_native_user_agent(cui__webview *v) {
    CUIWebView *web = (__bridge CUIWebView *)v->native;
    web.customUserAgent = v->user_agent ? [NSString stringWithUTF8String:v->user_agent] : nil;
    return true;
}

bool cui__webview_native_control(cui__webview *v, bool stop) {
    CUIWebView *web = (__bridge CUIWebView *)v->native;
    if (stop) { [web stopLoading]; return true; }
    return [web reload] != nil;
}

bool cui__webview_native_clear_cookies(cui__webview *v, cui_webview_result_fn callback, void *user) {
    CUIWebView *web = (__bridge CUIWebView *)v->native;
    [web.configuration.websiteDataStore removeDataOfTypes:[NSSet setWithObject:WKWebsiteDataTypeCookies]
        modifiedSince:[NSDate distantPast] completionHandler:^{
            if (web.owner) cui__webview_result(web.owner, callback, "null", NULL, user);
        }];
    return true;
}

void cui__webview_native_wake(cui__webview *v) {
    CUIWebView *web = (__bridge CUIWebView *)v->native;
    NSEvent *event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint modifierFlags:0
        timestamp:0 windowNumber:web.window.windowNumber context:nil subtype:0 data1:0 data2:0];
    [NSApp postEvent:event atStart:NO];
}
