#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
/* Measures wall time per frame (build + layout + paint) for gallery pages,
 * and software rasterization time. */
#include <cui/cui.h>
#include <cui/cui_render.h>
#include "../examples/gallery_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
static double now(void) { LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c); return (double)c.QuadPart / f.QuadPart; }
#else
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (double)t.tv_sec + (double)t.tv_nsec * 1e-9; }
#endif

enum { BENCH_FRAMES = 120 };

static int bench_compare(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void bench_report(const char *name, const char *phase, double *samples, int count) {
    double sum = 0;
    for (int i = 0; i < count; i++) sum += samples[i];
    qsort(samples, (size_t)count, sizeof *samples, bench_compare);
    printf("%-24s %-6s %9.3f %9.3f %9.3f %9.3f\n", name, phase, sum / count,
        samples[(count * 95 + 99) / 100 - 1], samples[(count * 99 + 99) / 100 - 1], samples[count - 1]);
}

static void bench_gallery(void) {
    cui_sw_renderer *sw = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw) });
    cui_set_viewport(ui, 1240, 820, 2);
    gallery g;
    gallery_init(&g);
    unsigned char *px = malloc(2480 * 1640 * 4);
    for (int p = 0; p < gallery_page_total(); p++) {
        g.page = p;
        for (int i = 0; i < 30; i++) { cui_begin_frame(ui, 1 / 60.0f); gallery_frame(ui, &g); cui_end_frame(ui); }
        double build[BENCH_FRAMES], raster[30];
        for (int i = 0; i < BENCH_FRAMES; i++) {
            cui_input_mouse_move(ui, 300 + (i % 50) * 10.0f, 400);
            double t0 = now();
            cui_begin_frame(ui, 1 / 60.0f); gallery_frame(ui, &g); cui_end_frame(ui);
            build[i] = (now() - t0) * 1000;
        }
        const cui_draw_data *dd = cui_get_draw_data(ui);
        for (int i = 0; i < 30; i++) {
            double t0 = now();
            cui_sw_render(sw, dd, px, dd->width_px, dd->height_px, dd->width_px * 4, CUI_SW_BGRA);
            raster[i] = (now() - t0) * 1000;
        }
        bench_report(gallery_page_name(p), "build", build, BENCH_FRAMES);
        bench_report(gallery_page_name(p), "raster", raster, 30);
    }
    free(px);
    cui_destroy(ui);
    cui_sw_destroy(sw);
}

static void bench_highlights(int len, bool marks) {
    char *text = malloc((size_t)len + 1);
    cui_text_mark *ranges = marks ? malloc(sizeof *ranges * (size_t)len) : NULL;
    if (!text || (marks && !ranges)) { fprintf(stderr, "benchmark: out of memory\n"); exit(1); }
    memset(text, 'a', (size_t)len); text[len] = 0;
    if (marks) for (int i = 0; i < len; i++) ranges[i] = (cui_text_mark){ i, i + 1, 0 };
    cui_ctx *ui = cui_create(NULL);
    cui_set_viewport(ui, 1000, 700, 2);
    double samples[BENCH_FRAMES];
    for (int i = -5; i < BENCH_FRAMES; i++) {
        if (i == 0 && !marks) cui_code_editor_find(ui, "code", "a");
        cui_input_mouse_move(ui, 50 + i, 50);
        double t0 = now();
        cui_begin_frame(ui, 1.0f / 60);
        cui_code_editor(ui, "code", text, (size_t)len + 1, .lang = "text", .height = 600,
            .marks = ranges, .mark_count = marks ? len : 0);
        cui_end_frame(ui);
        if (i >= 0) samples[i] = (now() - t0) * 1000;
    }
    char name[48]; snprintf(name, sizeof name, "%s %d bytes", marks ? "Marks" : "Find", len);
    bench_report(name, "build", samples, BENCH_FRAMES);
    cui_destroy(ui); free(ranges); free(text);
}

static void bench_dialog(int scale) {
    cui_sw_renderer *sw = cui_sw_create();
    cui_sw_set_incremental(sw, true);
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw) });
    cui_set_viewport(ui, 1280, 800, (float)scale);
    int w = 1280 * scale, h = 800 * scale;
    unsigned char *px = malloc((size_t)w * (size_t)h * 4);
    if (!px) { fprintf(stderr, "benchmark: out of memory\n"); exit(1); }
    double build[BENCH_FRAMES], raster[BENCH_FRAMES], total[BENCH_FRAMES];
    for (int i = -30; i < BENCH_FRAMES; i++) {
        bool open = i >= 0 && (i / 30) % 2 == 0;
        double start = now();
        cui_begin_frame(ui, 1.0f / 60);
        cui_text(ui, "Background content");
        cui_dialog(ui, "Dialog", &open) {
            cui_text(ui, "Save the changes before closing?");
            cui_button(ui, "Save changes");
        }
        cui_end_frame(ui);
        double built = now();
        const cui_draw_data *dd = cui_get_draw_data(ui);
        cui_sw_render(sw, dd, px, w, h, w * 4, CUI_SW_BGRA);
        double end = now();
        if (i >= 0) { build[i] = (built - start) * 1000; raster[i] = (end - built) * 1000; total[i] = (end - start) * 1000; }
    }
    char name[48]; snprintf(name, sizeof name, "Dialog %dx incremental", scale);
    bench_report(name, "build", build, BENCH_FRAMES);
    bench_report(name, "raster", raster, BENCH_FRAMES);
    bench_report(name, "total", total, BENCH_FRAMES);
    cui_destroy(ui); cui_sw_destroy(sw); free(px);
}

int main(int argc, char **argv) {
    printf("%-24s %-6s %9s %9s %9s %9s\n", "case", "phase", "mean ms", "p95 ms", "p99 ms", "max ms");
    if (argc < 2 || strcmp(argv[1], "--interactions")) bench_gallery();
    bench_highlights(2000, false); bench_highlights(10000, false); bench_highlights(20000, false);
    bench_highlights(20000, true);
    bench_dialog(2); bench_dialog(3);
    return 0;
}
