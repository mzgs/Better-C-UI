/* Regression cases from the large-data and input-boundary audit. Runs headless. */
#include "cui_internal.h"
#include "cui/cui_render.h"
#include <limits.h>
#include <float.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

static int failed, checks;
#define CHECK(c) do { checks++; if (!(c)) { failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void test_ascii_helpers(void) {
    const char *lower = "0123456789abcdef", *upper = "0123456789ABCDEF";
    for (int c = -128; c <= 256; c++) {
        int expected = -1;
        for (int digit = 0; digit < 16; digit++) {
            if (c == lower[digit] || c == upper[digit]) expected = digit;
        }
        CHECK(cui__hex_digit(c) == expected);
    }
    CHECK(cui__contains_ci("Mixed CASE", "case"));
    CHECK(cui__contains_ci("prefix suffix", "fix suf"));
    CHECK(cui__contains_ci("", ""));
    CHECK(cui__contains_ci("", NULL));
    CHECK(!cui__contains_ci("short", "shorter"));
    CHECK(!cui__contains_ci("", "x"));
    CHECK(cui__contains_ci("\xc3\xa9" "clair", "\xc3\xa9" "CL"));
    CHECK(!cui__contains_ci("\xc3\xa9", "\xc3\x89"));
}

static cui_ctx *context(bool rtl) {
    cui_ctx *ui = cui_create(&(cui_ctx_desc){ .direction = rtl ? CUI_DIRECTION_RTL : CUI_DIRECTION_LTR });
    cui_set_viewport(ui, 800, 600, 1);
    cui_set_reduced_motion(ui, true);
    return ui;
}

static void test_scopes(void) {
    cui_ctx *ui = context(false);
    cui_id expected[3] = {0};
    for (int frame = 0; frame < 3; frame++) {
        cui_begin_frame(ui, 1.0f / 60);
        for (int i = 0; i < 128; i++) cui_push_disabled(ui, i % 3 == 0);
        for (int i = 127; i >= 0; i--) {
            CHECK(cui_is_disabled(ui));
            cui_pop_disabled(ui);
        }
        CHECK(!cui_is_disabled(ui));
        cui_push_id(ui, "group");
        for (int i = 0; i < 3; i++) {
            if (i == 1) cui_push_id_int(ui, 7);
            cui_id id = cui_box_begin(ui, &(cui_box){ .scroll_y = true });
            cui_box_end(ui);
            if (i == 1) cui_pop_id(ui);
            if (!frame) expected[i] = id;
            CHECK(id == expected[i]);
        }
        CHECK(expected[0] != expected[1] && expected[0] != expected[2]);
        cui_pop_id(ui);
        cui_end_frame(ui);
    }
    cui_destroy(ui);
}

static int list_count, first, last;
static void list_frame(cui_ctx *ui) {
    cui_begin_frame(ui, 1.0f / 60);
    cui_virtual_list(ui, "rows", list_count, 32, CUI_PX(300), &first, &last) {
        for (int row = first; row < last; row++) {
            cui_box(ui, .height = CUI_PX(32)) cui_text(ui, "row");
        }
    }
    cui_end_frame(ui);
}

static void test_shrinking_list(void) {
    cui_ctx *ui = context(false);
    list_count = 1000000;
    list_frame(ui); list_frame(ui);
    cui_scroll_to(ui, "rows", 16000000);
    list_frame(ui);
    CHECK(first > 400000);
    list_count = 3;
    list_frame(ui);
    CHECK(first == 0 && last == 3);
    cui_scroll_state *s = cui_state(ui, cui_make_id(ui, "rows"), sizeof *s);
    CHECK(s->y == 0 && s->ty == 0 && s->content_h == 96);
    list_count = 0;
    list_frame(ui);
    CHECK(first == 0 && last == 0);
    list_count = -1;
    list_frame(ui);
    CHECK(first == 0 && last == 0);
    cui_destroy(ui);
}

static void test_large_row_ranges(void) {
    cui_ctx *ui = context(false);
    list_count = INT_MAX;
    list_frame(ui); list_frame(ui);
    cui_scroll_to(ui, "rows", 1e12f);
    list_frame(ui);
    CHECK(first >= 0 && first <= last && last == INT_MAX);
    static const cui_grid_column cols[] = { { .title = "Value" } };
    for (int frame = 0; frame < 3; frame++) {
        cui_begin_frame(ui, 1.0f / 60);
        if (frame == 2) {
            cui_scroll_state *s = cui_state(ui, cui_make_id(ui, "grid") ^ 0x5C011u, sizeof *s);
            s->y = s->ty = 1e12f;
        }
        cui_data_grid_begin_ex(ui, "grid", cols, 1, INT_MAX, &first, &last, NULL);
        CHECK(first >= 0 && first <= last && last <= INT_MAX);
        cui_data_grid_end(ui);
        cui_end_frame(ui);
    }
    cui_destroy(ui);
}

static void test_json_validation(void) {
    static const char *const invalid[] = { "-", "01", "-01", "1.", "1e", "1e+", "1e--2", "[1+2]", "{\"a\":--1}",
        "\"\\q\"", "\"\\uZZZZ\"", "\"\\u123\"", "\"\\", "\"unterminated" };
    static const char *const valid[] = { "0", "-0", "123", "-12.34e+56", "1E-9", "[true,false,null]", "{\"x\":\"\\u0041\\n\\t\\\\\\\"\\/\\b\\f\\r\"}" };
    cui_ctx *ui = context(false);
    for (int i = 0; i < CUI_COUNTOF(invalid) + CUI_COUNTOF(valid); i++) {
        cui_begin_frame(ui, 1.0f / 60);
        bool good = i >= CUI_COUNTOF(invalid);
        const char *json = good ? valid[i - CUI_COUNTOF(invalid)] : invalid[i];
        CHECK(cui_json_view(ui, "json", json) == good);
        cui_end_frame(ui);
    }
    cui_destroy(ui);
}

static void test_blame_without_commits(void) {
    cui_ctx *ui = context(false);
    const char *lines[] = { "first", "second", "third" };
    cui_begin_frame(ui, 1.0f / 60);
    cui_blame_result r = cui_blame(ui, "blame", lines, NULL, 3, NULL, 0);
    cui_end_frame(ui);
    CHECK(r.clicked == -1 && r.hovered == -1);
    CHECK(cui_get_draw_data(ui)->vertex_count > 0);
    cui_destroy(ui);
}

static void test_coverage_scale(void) {
    enum { N = 10000 };
    cui_coverage_file *files = calloc(N, sizeof *files);
    char (*names)[32] = malloc(N * sizeof *names);
    CHECK(files && names);
    if (!files || !names) { free(files); free(names); return; }
    for (int i = 0; i < N; i++) {
        snprintf(names[i], sizeof names[i], "file-%05d.c", N - i);
        files[i] = (cui_coverage_file){ .path = names[i], .lines = 10, .lines_hit = 5 };
    }
    cui_ctx *ui = context(false);
    cui_begin_frame(ui, 1.0f / 60);
    cui_coverage_result r = cui_coverage(ui, "coverage", files, N);
    cui_end_frame(ui);
    CHECK(r.lines == 50);
    cui_push_id(ui, "coverage");
    cui_scroll_state *s = cui_state(ui, cui_make_id(ui, "rows"), sizeof *s);
    CHECK(s->content_h == (N + 1) * 30);
    cui_pop_id(ui);
    /* Cached paths must survive the caller replacing its strings next frame. */
    char (*copy)[32] = malloc(N * sizeof *copy);
    CHECK(copy != NULL);
    if (copy) {
        memcpy(copy, names, N * sizeof *copy);
        free(names); names = copy;
        for (int i = 0; i < N; i++) files[i].path = names[i];
        cui_begin_frame(ui, 1.0f / 60);
        cui_coverage(ui, "coverage", files, N);
        cui_end_frame(ui);
    }
    char deep[4002];
    for (int i = 0; i < 2000; i++) { deep[i * 2] = 'a'; deep[i * 2 + 1] = '/'; }
    deep[4000] = 'x'; deep[4001] = 0;
    cui_coverage_file file = { .path = deep, .lines = 1, .lines_hit = 1 };
    cui_begin_frame(ui, 1.0f / 60);
    cui_coverage(ui, "deep", &file, 1);
    cui_end_frame(ui);
    cui_push_id(ui, "deep");
    s = cui_state(ui, cui_make_id(ui, "rows"), sizeof *s);
    CHECK(s->content_h == 2002 * 30);
    cui_pop_id(ui);
    cui_destroy(ui);
    free(names); free(files);
}

static void test_texture_bounds(void) {
    cui_sw_renderer *sw = cui_sw_create();
    cui_renderer_iface r = cui_sw_iface(sw);
    unsigned char pixels[64] = {0};
    CHECK(r.texture_create(r.user, 0, 4, CUI_TEXTURE_RGBA8, pixels) == 0);
    CHECK(r.texture_create(r.user, -1, 4, CUI_TEXTURE_RGBA8, pixels) == 0);
    cui_texture t = r.texture_create(r.user, 4, 4, CUI_TEXTURE_RGBA8, pixels);
    CHECK(t != 0);
    r.texture_update(r.user, t, -1, 0, 4, 4, pixels, 16);
    r.texture_update(r.user, t, 0, 0, 5, 4, pixels, 20);
    r.texture_update(r.user, t, 0, 0, 4, 4, pixels, 1);
    r.texture_update(r.user, t, INT_MAX, 0, 4, 4, pixels, 16);
    r.texture_update(r.user, t, 0, 0, 4, 4, NULL, 16);
    r.texture_update(r.user, t, 0, 0, 4, 4, pixels, 16);
    r.texture_destroy(r.user, t);
    cui_sw_destroy(sw);
}

static void test_chart_ranges(void) {
    const float ranges[][2] = { { 1e8f, 1e8f + 8 }, { 1e8f, 1e8f }, { 1e-44f, 2e-44f },
        { FLT_MAX, FLT_MAX }, { -FLT_MAX, FLT_MAX }, { NAN, 1 } };
    for (int i = 0; i < CUI_COUNTOF(ranges); i++) {
        float lo, hi, step;
        cui__nice_range(ranges[i][0], ranges[i][1], 4, &lo, &hi, &step);
        CHECK(isfinite(lo) && isfinite(hi) && isfinite(step));
        CHECK(hi > lo && step > 0 && lo + step > lo);
    }
    cui_ctx *ui = context(false);
    float values[] = { 1e8f, 1e8f + 8 };
    cui_series series = { .values = values, .count = 2 };
    for (int frame = 0; frame < 2; frame++) {
        cui_begin_frame(ui, 1.0f / 60);
        cui_line_chart(ui, "auto", &series, 1);
        cui_line_chart(ui, "fixed", &series, 1, .min = values[0], .max = values[1]);
        cui_end_frame(ui);
        CHECK(cui_get_draw_data(ui)->vertex_count > 0);
    }
    cui_vec2 points[] = { { 1e8f, 1e8f }, { 1e8f + 8, 1e8f + 8 } };
    cui_scatter_series scatter = { .points = points, .count = 2 };
    cui_begin_frame(ui, 1.0f / 60);
    cui_scatter_chart(ui, "scatter", &scatter, 1);
    cui_end_frame(ui);
    cui_dot_series dots = { .values = values };
    for (int frame = 0; frame < 2; frame++) {
        cui_begin_frame(ui, 1.0f / 60);
        cui_dot_chart(ui, "dots", &dots, 1, 2, .x_min = frame ? 1e16 : 0,
            .x_max = frame ? 1e16 + 2 : 1, .x_step = 1e-12);
        cui_end_frame(ui);
        CHECK(cui_get_draw_data(ui)->vertex_count > 0);
    }
    cui_destroy(ui);
}

static cui_id buttons[2];
static int clicked[2];
static void table_frame(cui_ctx *ui) {
    static const cui_table_column cols[] = { { "Action", CUI_PX(200) } };
    cui_begin_frame(ui, 1.0f / 60);
    cui_table(ui, "table", cols, 1) {
        for (int row = 0; row < 2; row++) {
            cui_table_row(ui, false);
            cui_table_cell(ui);
            if (cui_button(ui, "Delete")) clicked[row]++;
            buttons[row] = cui_last_id(ui);
        }
    }
    cui_end_frame(ui);
}

static void test_table_controls(void) {
    cui_ctx *ui = context(false);
    table_frame(ui); table_frame(ui);
    CHECK(buttons[0] != buttons[1]);
    cui_rect r = cui_get_rect(ui, buttons[1]);
    cui_input_mouse_move(ui, r.x + r.w / 2, r.y + r.h / 2); table_frame(ui);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true); table_frame(ui);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false); table_frame(ui);
    CHECK(clicked[0] == 0 && clicked[1] == 1);
    cui_destroy(ui);
}

static void horizontal_frame(cui_ctx *ui) {
    cui_begin_frame(ui, 1.0f / 60);
    cui_scroll(ui, .id = "scroll", .height = CUI_PX(300), .horizontal = true) {
        cui_box(ui, .width = CUI_PX(3000), .height = CUI_PX(50)) cui_text(ui, "wide");
    }
    cui_end_frame(ui);
}

static void test_horizontal_drag(void) {
    for (int rtl = 0; rtl < 2; rtl++) {
        cui_ctx *ui = context(rtl != 0);
        horizontal_frame(ui); horizontal_frame(ui);
        cui_id id = cui_make_id(ui, "scroll");
        cui_rect r = cui_get_rect(ui, id);
        cui_scroll_state *s = cui_state(ui, id, sizeof *s);
        float start = rtl ? r.x + r.w - 12 : r.x + 12;
        cui_input_mouse_move(ui, start, r.y + r.h - 6); horizontal_frame(ui);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true); horizontal_frame(ui);
        cui_input_mouse_move(ui, r.x + r.w * (rtl ? 0.2f : 0.8f), r.y + r.h - 6); horizontal_frame(ui);
        CHECK(s->x > (s->content_w - s->view_w) / 2);
        CHECK(s->drag_x && !s->drag_y);
        cui_input_mouse_move(ui, start, r.y + r.h - 6); horizontal_frame(ui);
        CHECK(s->x < 1);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false); horizontal_frame(ui);
        CHECK(!s->drag_x);
        cui_destroy(ui);
    }
}

static void test_parsers(void) {
    const char *error = NULL;
    int depth = 10000;
    char *pattern = malloc((size_t)depth * 4 + 2), *p = pattern;
    CHECK(pattern != NULL);
    if (!pattern) return;
    for (int i = 0; i < depth; i++) { memcpy(p, "(?:", 3); p += 3; }
    *p++ = 'a';
    for (int i = 0; i < depth; i++) *p++ = ')';
    *p = 0;
    CHECK(cui_regex_search(pattern, 0, "a", 1, 0, NULL, &error) == -1 && error != NULL);
    CHECK(cui_regex_search("(?:(?:a))", 0, "a", 1, 0, NULL, &error) == 1 && error == NULL);
    cui_regex_match match;
    CHECK(cui_regex_search("(?:a|ab)c", 0, "abc", 3, 0, &match, NULL) == 1 && match.end == 3 && match.groups == 0);
    CHECK(cui_regex_search("(?:ab)+", 0, "abab", 4, 0, &match, NULL) == 1 && match.end == 4);
    CHECK(cui_regex_search("(?:(a))", 0, "a", 1, 0, &match, NULL) == 1 && match.groups == 1 && match.group_end[1] == 1);
    free(pattern);
    cui_ctx *ui = context(false);
    char text[4096];
    cui_terminal_line lines[16];
    cui_terminal_buffer buffer;
    cui_terminal_init(&buffer, text, sizeof text, lines, 16);
    cui_terminal_write(&buffer, "\033[999999999999999mhello\033[31m red\033[0m\n", -1);
    cui_begin_frame(ui, 1.0f / 60);
    cui_terminal(ui, "term", &buffer, .height = 300, .no_input = true);
    cui_end_frame(ui);
    CHECK(cui_get_draw_data(ui)->vertex_count > 0);
    cui_destroy(ui);
}

static int list_calls, names_listed;
static int long_names(void *user, const char *path, cui_file_entry *out, int cap, char *names, size_t names_cap, bool hidden) {
    (void)user; (void)path; (void)hidden;
    list_calls++;
    for (int i = 0; i < 3 && i < cap; i++) {
        if ((size_t)(i + 1) * 81 > names_cap) return CUI_FILE_LIST_NAMES_FULL;
        char *name = names + i * 81;
        memset(name, 'x', 80); name[0] = (char)('0' + i); name[80] = 0;
        out[i] = (cui_file_entry){ .name = name };
    }
    names_listed = CUI_MIN(cap, 3);
    return names_listed;
}

static int glob_names(void *user, const char *path, cui_file_entry *out, int cap, char *names, size_t names_cap, bool hidden) {
    (void)user; (void)path; (void)hidden;
    static const char *const files[] = { "Report.TXT", "data.csv", "a.txt", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" };
    size_t used = 0;
    for (int i = 0; i < CUI_COUNTOF(files); i++) {
        size_t bytes = strlen(files[i]) + 1;
        if (i >= cap || used + bytes > names_cap) return CUI_FILE_LIST_NAMES_FULL;
        memcpy(names + used, files[i], bytes);
        out[i] = (cui_file_entry){ .name = names + used };
        used += bytes;
    }
    return CUI_COUNTOF(files);
}

static void test_file_globs(void) {
    static const struct { const char *pattern; int count; } cases[] = {
        { "*.txt", 2 }, { "?.txt", 1 }, { "**", 4 }, { "*.csv; *.TXT", 3 },
        { "*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*b", 0 },
        { "*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*", 1 }, { "", 4 }
    };
    cui_ctx *ui = context(false);
    char path[32] = "/files";
    for (int i = 0; i < CUI_COUNTOF(cases); i++) {
        cui_begin_frame(ui, 1.0f / 60);
        cui_file_browser_result r = cui_file_browser(ui, "files", path, sizeof path,
            .filter = cases[i].pattern, .list = glob_names, .no_places = true);
        cui_end_frame(ui);
        CHECK(!r.error);
        if (cases[i].count) {
            cui_push_id(ui, "files");
            cui_scroll_state *s = cui_state(ui, cui_make_id(ui, "rows"), sizeof *s);
            CHECK(s->content_h == cases[i].count * 30);
            cui_pop_id(ui);
        }
    }
    cui_destroy(ui);
}

static void test_file_names(void) {
    cui_ctx *ui = context(false);
    char path[256] = "/files";
    cui_begin_frame(ui, 1.0f / 60);
    cui_file_browser_result r = cui_file_browser(ui, "files", path, sizeof path,
        .height = CUI_PX(300), .max_entries = 3, .no_places = true, .list = long_names);
    cui_end_frame(ui);
    CHECK(!r.error && names_listed == 3 && list_calls > 1);
    cui_destroy(ui);
#if !defined(_WIN32)
    char directory[] = "/tmp/cui-names-XXXXXX";
    if (!mkdtemp(directory)) { CHECK(false); return; }
    char file[256], name[81];
    memset(name, 'x', 80); name[80] = 0;
    for (int i = 0; i < 3; i++) {
        name[0] = (char)('0' + i);
        snprintf(file, sizeof file, "%s/%s", directory, name);
        FILE *f = fopen(file, "w"); CHECK(f != NULL); if (f) fclose(f);
    }
    cui_file_entry entries[3]; char small[120], large[243];
    CHECK(cui_fs_list(NULL, directory, entries, 3, small, sizeof small, false) == CUI_FILE_LIST_NAMES_FULL);
    CHECK(cui_fs_list(NULL, directory, entries, 3, large, sizeof large, false) == 3);
    for (int i = 0; i < 3; i++) {
        name[0] = (char)('0' + i); snprintf(file, sizeof file, "%s/%s", directory, name); remove(file);
    }
    rmdir(directory);
#endif
}

static bool picker_open, picker_done;
static char picked[64];
static size_t picked_cap;
static cui_rect save_button;
static void picker_frame(cui_ctx *ui) {
    cui_begin_frame(ui, 1.0f / 60);
    picker_done = cui_file_picker(ui, "pick", &picker_open, picked, picked_cap,
        .start = "/files", .name = "report.txt", .save = true, .list = long_names);
    save_button = cui_last_rect(ui);
    cui_end_frame(ui);
}

static void test_picker_capacity(void) {
    cui_ctx *ui = context(false);
    picker_open = true; picked_cap = 8;
    picker_frame(ui); picker_frame(ui); picker_frame(ui);
    CHECK(save_button.w > 0);
    cui_input_mouse_move(ui, save_button.x + save_button.w / 2, save_button.y + save_button.h / 2); picker_frame(ui);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true); picker_frame(ui);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false); picker_frame(ui);
    CHECK(!picker_done && picker_open && picked[0] == 0);
    picked_cap = sizeof picked;
    picker_frame(ui);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true); picker_frame(ui);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false); picker_frame(ui);
    CHECK(picker_done && !picker_open && !strcmp(picked, "/files/report.txt"));
    cui_destroy(ui);
}

int main(void) {
    test_ascii_helpers(); test_scopes();
    test_shrinking_list(); test_table_controls(); test_horizontal_drag();
    test_parsers(); test_file_names(); test_picker_capacity();
    test_large_row_ranges(); test_json_validation(); test_blame_without_commits();
    test_coverage_scale(); test_texture_bounds();
    test_file_globs(); test_chart_ranges();
    printf("audit regressions: %d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
