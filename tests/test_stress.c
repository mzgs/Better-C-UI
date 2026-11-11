/* Exercise the stress app's counters, sparse updates, row virtualization and input.
 * Optional argument: render light, dark and narrow screenshots into that directory. */
#define main stress_example_main
#include "../examples/stress.c"
#undef main
#include <cui/cui_render.h>
#include <stdlib.h>

static int stress_checks, stress_failures;
#define CHECK(c) do { stress_checks++; if (!(c)) { stress_failures++; fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static void stress_test_frame(cui_ctx *ui, app_state *a) {
    cui_begin_frame(ui, 1.0f / 60.0f);
    frame(ui, a);
    cui_end_frame(ui);
}

static void stress_test_click(cui_ctx *ui, app_state *a, cui_rect r) {
    cui_input_mouse_move(ui, r.x + r.w * 0.5f, r.y + r.h * 0.5f);
    stress_test_frame(ui, a);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
    stress_test_frame(ui, a);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
    stress_test_frame(ui, a);
}

static void stress_test_shot(cui_ctx *ui, cui_sw_renderer *sw, const char *dir, const char *name) {
    if (!dir) return;
    const cui_draw_data *dd = cui_get_draw_data(ui);
    unsigned char *pixels = malloc((size_t)dd->width_px * dd->height_px * 4);
    CHECK(pixels != NULL);
    if (!pixels) return;
    cui_sw_render(sw, dd, pixels, dd->width_px, dd->height_px, dd->width_px * 4, CUI_SW_RGBA);
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    CHECK(cui_write_png(path, pixels, dd->width_px, dd->height_px, dd->width_px * 4));
    free(pixels);
}

int main(int argc, char **argv) {
    stress_metrics metrics = { .sample_at = 1, .cpu_at = 0.25, .frames = 120 };
    stress_sample(&metrics, 3, 1.25, 64);
    CHECK(metrics.ready && metrics.fps == 60);
    CHECK(fabs(metrics.frame_ms - 1000.0 / 60) < 1e-9);
    CHECK(metrics.cpu_percent == 50 && metrics.rss_mib == 64);
    metrics.frames = 120;
    stress_sample(&metrics, 4, 3.25, 80);
    CHECK(metrics.cpu_percent == 200); /* multiple busy cores may exceed 100% */
    metrics.frames = 1;
    stress_sample(&metrics, 5, -1, -1);
    CHECK(metrics.cpu_percent == -1 && metrics.rss_mib == -1);
    stress_metrics before = metrics;
    stress_sample(&metrics, 5, 0, 0);
    CHECK(metrics.sample_at == before.sample_at && metrics.frames == before.frames);
    CHECK(stress_now() > 0 && stress_cpu_seconds() >= 0);
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
    CHECK(stress_rss_mib() > 0);
#endif

    static app_state state;
    stress_init(&state);
    state.selected = 2;
    stress_record unchanged = state.records[1];
    float old_load = state.records[2].load;
    stress_advance(&state, 0.2f);
    CHECK(state.records[2].load != old_load);
    CHECK(unchanged.load == state.records[1].load &&
          memcmp(unchanged.history, state.records[1].history, sizeof unchanged.history) == 0);
    state.changes = 1;
    stress_advance(&state, 0.2f);
    CHECK(unchanged.load != state.records[1].load);
    state.changes = 2;
    unchanged = state.records[1];
    stress_advance(&state, 1);
    CHECK(unchanged.load == state.records[1].load &&
          memcmp(unchanged.history, state.records[1].history, sizeof unchanged.history) == 0);

    cui_sw_renderer *sw = cui_sw_create();
    CHECK(sw != NULL);
    if (!sw) return 1;
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw), .theme = CUI_THEME_DARK, .accent = CUI_ACCENT_GREEN });
    CHECK(ui != NULL);
    if (!ui) { cui_sw_destroy(sw); return 1; }
    cui_set_viewport(ui, 1600, 1000, 1);
    cui_set_reduced_motion(ui, true);
    for (int i = 0; i < 3; i++) stress_test_frame(ui, &state);
    CHECK(state.built_rows == 3000);
    CHECK(cui_get_draw_data(ui)->vertex_count > 1000);
    const char *shots = argc > 1 ? argv[1] : NULL;
    stress_test_shot(ui, sw, shots, "stress-dark");

    cui_push_id_int(ui, 0);
    cui_rect input = cui_get_rect(ui, cui_make_id(ui, "name"));
    cui_pop_id(ui);
    CHECK(input.w > 50 && input.h > 0);
    stress_test_click(ui, &state, input);
    cui_input_text(ui, "x");
    stress_test_frame(ui, &state);
    CHECK(strchr(state.records[0].name, 'x') != NULL);

    cui_rect toggle = cui_get_rect(ui, cui_make_id(ui, "Virtualize rows") ^ 0x5D17C002u);
    CHECK(toggle.w > 0 && toggle.h > 0);
    stress_test_click(ui, &state, toggle);
    CHECK(state.virtualized);
    stress_test_frame(ui, &state);
    CHECK(state.built_rows > 0 && state.built_rows < 40);

    for (int load = 0; load < 4; load++) {
        state.workload = load;
        stress_test_frame(ui, &state);
        CHECK(state.built_rows > 0 && state.built_rows < 40);
    }
    state.virtualized = false;
    stress_test_frame(ui, &state);
    CHECK(state.built_rows == STRESS_RECORDS);
    state.workload = 1;
    cui_set_theme(ui, CUI_THEME_LIGHT);
    stress_test_frame(ui, &state);
    stress_test_frame(ui, &state);
    stress_test_shot(ui, sw, shots, "stress-light");

    cui_set_viewport(ui, 640, 900, 1);
    state.virtualized = true;
    stress_test_frame(ui, &state);
    stress_test_frame(ui, &state);
    CHECK(state.built_rows > 0 && state.built_rows < 40);
    stress_test_shot(ui, sw, shots, "stress-narrow");
    cui_destroy(ui);
    ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw), .direction = CUI_DIRECTION_RTL });
    CHECK(ui != NULL);
    if (ui) {
        cui_set_viewport(ui, 640, 900, 1);
        stress_test_frame(ui, &state);
        stress_test_frame(ui, &state);
        CHECK(state.built_rows > 0 && state.built_rows < 40);
        cui_destroy(ui);
    }
    cui_sw_destroy(sw);
    printf("%d checks, %d failed\n", stress_checks, stress_failures);
    return stress_failures ? 1 : 0;
}
