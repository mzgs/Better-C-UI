/* Exercise the shared app code without creating a native window. */
#include "platform/cui_platform.h"
#include <limits.h>

static int screenshot_failed, screenshot_checks;
#define CHECK(c) do { screenshot_checks++; if (!(c)) { screenshot_failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

double cui_test_now(void) { return 0; }

void cui_test_present(cui_app *app, const void *pixels, int w, int h, int stride) {
    (void)app; (void)pixels; (void)w; (void)h; (void)stride;
}

bool cui_test_present_part(cui_app *app, const void *pixels, int w, int h, int stride, int x, int y, int pw, int ph) {
    (void)app; (void)pixels; (void)w; (void)h; (void)stride; (void)x; (void)y; (void)pw; (void)ph;
    return true;
}

void cui_test_gl_render(cui_gl_renderer *renderer, const cui_draw_data *data, bool clear) {
    (void)renderer; (void)data; (void)clear;
}

void cui_test_read_pixels(cui_gl_renderer *renderer, int w, int h, void *pixels) {
    (void)renderer;
    for (int y = 0; y < h; y++) memset((unsigned char *)pixels + (size_t)y * w * 4, y + 1, (size_t)w * 4);
}

int main(void) {
    cui_app app = {0};
    app.sw = cui_sw_create();
    app.ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(app.sw) });
    cui_set_viewport(app.ui, 32, 32, 1);
    cui_begin_frame(app.ui, 1.0f / 60); cui_end_frame(app.ui);
    cui_set_viewport(app.ui, 64, 64, 1);
    cui_begin_frame(app.ui, 1.0f / 60);
    app.in_frame = true;
    unsigned char pixels[64 * 64 * 4], untouched[sizeof pixels];
    memset(pixels, 0x5a, sizeof pixels);
    memcpy(untouched, pixels, sizeof pixels);
    CHECK(!cui_app_screenshot(&app, pixels, 32, 32));
    CHECK(memcmp(pixels, untouched, sizeof pixels) == 0);
    CHECK(cui_app_screenshot(&app, pixels, 64, 64));
    CHECK(!cui_app_screenshot(&app, pixels, INT_MAX, 64));
    unsigned char padded[80 * 64 * 4];
    memset(padded, 0x5a, sizeof padded);
    app.gl = (cui_gl_renderer *)(uintptr_t)1;
    CHECK(cui_app_screenshot(&app, padded, 80, 64));
    for (int y = 0; y < 64; y++) CHECK(padded[y * 80 * 4] == y + 1);
    cui_destroy(app.ui); cui_sw_destroy(app.sw); free(app.sw_pixels);
    printf("app screenshot: %d checks, %d failed\n", screenshot_checks, screenshot_failed);
    return screenshot_failed ? 1 : 0;
}
