/* Explicit desktop check of the example's selected-file playback path. */
#define main video_smoke_example_main
#include "../examples/webview_video.c"
#undef main
#include <assert.h>
#include <stdio.h>
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

int main(int argc, char **argv) {
    assert(argc == 2); /* path to an external test video; no bundled media */
    cui_app *app = cui_app_create(&(cui_app_desc){
        .title = "CUI selected video smoke", .width = 960, .height = 680, .continuous = true });
    assert(app);
    video_demo demo = { .attempted = true, .loaded = true, .view = cui_app_webview_create(app) };
    assert(demo.view && video_open(demo.view, argv[1]));
    WKWebView *native = nil;
    __block bool pending = false, done = false;
    int step = 0;
    const char *checks[] = {
        "(()=>{const v=document.querySelector('video');return !!v && v.readyState>=1 && v.duration>0 && v.videoWidth>0 && v.controls})()",
        "(()=>{const v=document.querySelector('video');v.muted=true;v.play().catch(()=>{});return true})()",
        "(()=>{const v=document.querySelector('video');return !v.paused && v.currentTime>0.1})()",
        "(()=>{const v=document.querySelector('video');v.pause();v.currentTime=Math.min(4,v.duration/2);return true})()",
        "(()=>{const v=document.querySelector('video');return v.paused && !v.seeking && Math.abs(v.currentTime-Math.min(4,v.duration/2))<0.1})()"
    };
    double deadline = [NSDate timeIntervalSinceReferenceDate] + 30;
    while (step < 5 && [NSDate timeIntervalSinceReferenceDate] < deadline && cui_app_frame(app)) {
        video_frame(cui_app_ui(app), &demo);
        if (!native) {
            NSWindow *window = (__bridge NSWindow *)cui_app_native_window(app);
            for (NSView *child in window.contentView.subviews)
                if ([child isKindOfClass:[WKWebView class]]) native = (WKWebView *)child;
        }
        assert(cui_webview_get_status(demo.view) != CUI_WEBVIEW_ERROR);
        if (!native || pending) continue;
        if (done) { done = false; step++; }
        if (step == 5) break;
        pending = true;
        [native evaluateJavaScript:[NSString stringWithUTF8String:checks[step]] completionHandler:^(id result, NSError *error) {
            done = !error && [result isEqual:@YES]; pending = false;
        }];
    }
    if (step != 5) fprintf(stderr, "selected video smoke timed out at step %d\n", step);
    assert(step == 5);
    cui_app_destroy(app);
    puts("webview video: selected local file, native controls, play, pause and seek passed");
    return 0;
}
