/* Interaction regressions: long lines, idle wakeups, large repaints and folder loading. */
#include "cui_internal.h"
#include "cui/cui_render.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <time.h>
#endif

static int latency_failed, latency_checks;
#define CHECK(c) do { latency_checks++; if (!(c)) { latency_failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static cui_ctx *latency_context(void) {
    cui_ctx *ui = cui_create(NULL);
    cui_set_viewport(ui, 800, 600, 1);
    cui_begin_frame(ui, 1.0f / 60); cui_end_frame(ui);
    return ui;
}

static void test_idle_clock(void) {
    cui_ctx *ui = latency_context();
    double before = cui_time(ui);
    cui_begin_frame(ui, 2);
    CHECK(fabs(cui_time(ui) - before - 2) < 1e-6);
    CHECK(cui_delta_time(ui) == 2);
    float hover = 0;
    cui_spring dialog = {0};
    cui__tween(ui, &hover, 1, CUI__T_HOVER);
    cui__spring(ui, &dialog, 1, CUI__SPRING_DIALOG);
    CHECK(hover > 0 && hover < 0.5f);
    CHECK(dialog.x > 0 && dialog.x < 0.1f);
    cui_toast(ui, "Saved", .duration = 4);
    cui_end_frame(ui);
    CHECK(ui->toast_count == 1 && ui->toasts[0].age == 0);
    double elapsed = 0;
    for (int frames = 0; ui->toast_count && frames < 1000; frames++) {
        float dt = cui_needs_redraw(ui) ? 1.0f / 60 : cui_next_wakeup(ui);
        CHECK(dt >= 0);
        if (dt < 0) break;
        elapsed += dt;
        cui_begin_frame(ui, dt); cui_end_frame(ui);
    }
    CHECK(ui->toast_count == 0 && elapsed >= 4 && elapsed < 4.6);
    before = cui_time(ui);
    cui_begin_frame(ui, NAN); cui_end_frame(ui);
    cui_begin_frame(ui, INFINITY); cui_end_frame(ui);
    cui_begin_frame(ui, -1); cui_end_frame(ui);
    CHECK(cui_time(ui) == before);
    cui_set_reduced_motion(ui, true);
    cui_begin_frame(ui, 2);
    hover = 0; dialog = (cui_spring){0};
    cui__tween(ui, &hover, 1, CUI__T_HOVER);
    cui__spring(ui, &dialog, 1, CUI__SPRING_DIALOG);
    CHECK(hover == 1 && dialog.x == 1 && dialog.v == 0);
    cui_end_frame(ui);
    cui_destroy(ui);
}

static void test_hold_after_idle(void) {
    cui_ctx *ui = latency_context();
    cui_id id = cui_make_id(ui, "Hold");
    ui->focus = id; ui->focus_visible = true;
    cui_input_key(ui, CUI_KEY_SPACE, true, 0);
    cui_begin_frame(ui, 5);
    CHECK(!cui_hold_button(ui, "Hold", .duration = 1));
    cui_end_frame(ui);
    bool fired = false;
    for (int i = 0; i < 61; i++) {
        cui_input_key(ui, CUI_KEY_SPACE, true, 0);   /* repeat must not restart the hold */
        cui_begin_frame(ui, 1.0f / 60);
        fired |= cui_hold_button(ui, "Hold", .duration = 1);
        cui_end_frame(ui);
    }
    CHECK(fired);
    cui_destroy(ui);
}

static void test_long_line_highlights(void) {
    enum { LEN = 20000 };
    char *text = malloc(LEN + 1);
    cui_text_mark *marks = malloc(sizeof *marks * LEN);
    CHECK(text && marks);
    if (!text || !marks) { free(text); free(marks); return; }
    memset(text, 'a', LEN); text[LEN] = 0;
    for (int i = 0; i < LEN; i++) marks[i] = (cui_text_mark){ i, i + 1, 0 };
    for (int rtl = 0; rtl < 2; rtl++) {
        cui_ctx *ui = latency_context();
        ui->direction = rtl ? CUI_DIRECTION_RTL : CUI_DIRECTION_LTR;
        cui_code_editor_find(ui, "code", "a");
        for (int frame = 0; frame < 3; frame++) {
            cui_begin_frame(ui, 1.0f / 60);
            if (frame == 2) cui_code_editor_select(ui, "code", LEN - 1, LEN);
            cui_code_editor_result r = cui_code_editor(ui, "code", text, LEN + 1,
                .lang = "text", .marks = marks, .mark_count = LEN);
            cui_end_frame(ui);
            CHECK(r.length == LEN && r.find_matches == LEN);
            CHECK(cui_get_draw_data(ui)->vertex_count < 5000);
            if (frame == 2) CHECK(r.cursor == LEN && r.find_index == LEN);
        }
        cui_destroy(ui);
    }
    free(text); free(marks);
}

static void test_parallel_repaint(void) {
    enum { W = 800, H = 600, BYTES = W * H * 4 };
    uint8_t *one = malloc(BYTES), *many = malloc(BYTES);
    CHECK(one && many);
    if (!one || !many) { free(one); free(many); return; }
    cui_sw_renderer *sw = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .renderer = cui_sw_iface(sw) });
    cui_set_viewport(ui, W, H, 1);
    for (int frame = 0; frame < 3; frame++) {
        cui_begin_frame(ui, 1.0f / 60);
        cui_box(ui, .width = CUI_FILL, .height = CUI_FILL, .bg = CUI_RGB(0x405060)) {
            cui_draw_rect(ui, (cui_rect){ 100, 100, 600, 300 }, CUI_CORNERS(18), CUI_RGBA(0x80a0c080));
        }
        cui_end_frame(ui);
        const cui_draw_data *dd = cui_get_draw_data(ui);
        CHECK(dd->index_count < 600);
        cui_sw_set_threads(sw, 1);
        cui_sw_render(sw, dd, one, W, H, W * 4, CUI_SW_RGBA);
        cui_sw_set_threads(sw, 4);
        cui_sw_render(sw, dd, many, W, H, W * 4, CUI_SW_RGBA);
        CHECK(memcmp(one, many, BYTES) == 0);
    }
    cui_destroy(ui); cui_sw_destroy(sw); free(one); free(many);
}

static int latency_listing_ready;
static int latency_listing(void *user, const char *path, cui_file_entry *out, int cap, char *names, size_t size, bool hidden) {
    (void)user; (void)path; (void)hidden;
    if (!latency_listing_ready) return CUI_FILE_LIST_PENDING;
    if (latency_listing_ready < 0) return -1;
    if (cap < 1 || size < 6) return CUI_FILE_LIST_NAMES_FULL;
    memcpy(names, "a.txt", 6);
    out[0] = (cui_file_entry){ .name = names };
    return 1;
}

static cui_file_browser_result latency_folder(cui_ctx *ui, char *path, cui_file_list_fn list) {
    cui_begin_frame(ui, 1.0f / 60);
    cui_file_browser_result r = cui_file_browser(ui, "files", path, 256, .list = list, .no_places = true);
    cui_end_frame(ui);
    return r;
}

static void test_pending_listing(void) {
    cui_ctx *ui = latency_context();
    char path[256] = "/files";
    latency_listing_ready = 0;
    cui_file_browser_result r = latency_folder(ui, path, latency_listing);
    CHECK(r.loading && !r.error && !r.selected && cui_next_wakeup(ui) >= 0);
    cui_rect before = cui_get_rect(ui, cui_make_id(ui, "files"));
    latency_listing_ready = 1;
    r = latency_folder(ui, path, latency_listing);
    CHECK(!r.loading && !r.error);
    cui_rect after = cui_get_rect(ui, cui_make_id(ui, "files"));
    CHECK(before.h == after.h && before.w == after.w);
    snprintf(path, sizeof path, "/missing");
    latency_listing_ready = -1;
    r = latency_folder(ui, path, latency_listing);
    CHECK(r.error && !r.loading);
    cui_destroy(ui);
}

#if !defined(_WIN32) && !defined(__wasm__) && !defined(__EMSCRIPTEN__) && !defined(CUI_NO_FILESYSTEM)
static void test_native_listing(void) {
    char dir[] = "/tmp/cui-latency-XXXXXX";
    char *created = mkdtemp(dir);
    CHECK(created != NULL);
    if (!created) return;
    char path[256], file[256];
    snprintf(file, sizeof file, "%s/a.txt", dir);
    FILE *f = fopen(file, "w"); CHECK(f != NULL); if (f) fclose(f);
    for (int close_early = 0; close_early < 8; close_early++) {
        cui_ctx *ui = latency_context();
        snprintf(path, sizeof path, "%s", dir);
        cui_file_browser_result r = latency_folder(ui, path, NULL);
        CHECK(r.loading && !r.error);
        if (close_early == 0) {
            /* Change paths while the previous read is still owned by the browser. */
            snprintf(path, sizeof path, "%s/missing", dir);
            r = latency_folder(ui, path, NULL);
            CHECK(r.loading && !r.selected);
            for (int i = 0; r.loading && i < 1000; i++) {
                struct timespec delay = {0, 1000000}; nanosleep(&delay, NULL);
                r = latency_folder(ui, path, NULL);
            }
            CHECK(!r.loading && r.error);
            snprintf(path, sizeof path, "%s", dir);
            r = latency_folder(ui, path, NULL);
            for (int i = 0; r.loading && i < 1000; i++) {
                struct timespec delay = {0, 1000000}; nanosleep(&delay, NULL);
                r = latency_folder(ui, path, NULL);
            }
            CHECK(!r.loading && !r.error);
            cui_push_id(ui, "files");
            cui_scroll_state *rows = cui_state(ui, cui_make_id(ui, "rows"), sizeof *rows);
            CHECK(rows->content_h == 30);
            cui_pop_id(ui);
        }
        cui_destroy(ui);
    }
    remove(file); rmdir(dir);
}
#endif

int main(void) {
    test_idle_clock(); test_hold_after_idle(); test_long_line_highlights(); test_parallel_repaint(); test_pending_listing();
#if !defined(_WIN32) && !defined(__wasm__) && !defined(__EMSCRIPTEN__) && !defined(CUI_NO_FILESYSTEM)
    test_native_listing();
#endif
    printf("latency: %d checks, %d failed\n", latency_checks, latency_failed);
    return latency_failed ? 1 : 0;
}
