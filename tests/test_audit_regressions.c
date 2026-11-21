/* Regression cases from the large-data and input-boundary audit. Runs headless. */
#include "cui_internal.h"
#ifdef CUI_TEXT_SHAPING
#include "cui_shape.h"
#endif
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

static void code_scroll_frame(cui_ctx *ui, float max_height) {
    cui_begin_frame(ui, 1.0f / 60);
    cui_scroll(ui, .id = "page", .width = CUI_PX(300), .height = CUI_PX(240)) {
        cui_code_block(ui, "code",
            "int main(void) { return a_long_function_name_with_arguments(123, 456, 789); }\n"
            "// second line\n// third line\n// fourth line\n",
            .lang = "c", .max_height = max_height);
        cui_box(ui, .height = CUI_PX(1000)) {}
    }
    cui_end_frame(ui);
}

static void test_code_scroll_routing(void) {
    static const struct { float dx, dy; int mods; bool horizontal; } cases[] = {
        { 0, -1, 0, false }, { -0.01f, -1, 0, false }, { 0.01f, -1, 0, false }, { -1, -1, 0, false },
        { -1, -0.01f, 0, true }, { -1, 0, 0, true }, { 0, -1, CUI_MOD_SHIFT, true }
    };
    for (int rtl = 0; rtl < 2; rtl++) for (int capped = 0; capped < 2; capped++) {
        for (int k = 0; k < CUI_COUNTOF(cases); k++) {
            cui_ctx *ui = context(rtl != 0);
            float height = capped ? 40 : 0;
            code_scroll_frame(ui, height); code_scroll_frame(ui, height);
            cui_id page_id = cui_make_id(ui, "page"), code_id = cui_make_id(ui, "code") ^ 0x5C0Du;
            cui_scroll_state *page = cui_state(ui, page_id, sizeof *page);
            cui_scroll_state *code = cui_state(ui, code_id, sizeof *code);
            CHECK(code->content_w > code->view_w && page->content_h > page->view_h);
            cui_rect r = cui_get_rect(ui, code_id);
            cui_input_mouse_move(ui, r.x + r.w / 2, r.y + 10);
            ui->mods = cases[k].mods;
            cui_input_scroll(ui, cases[k].dx, cases[k].dy);
            code_scroll_frame(ui, height);
            bool inner = capped || cases[k].horizontal;
            CHECK(ui->scroll_target == (inner ? code_id : page_id));
            CHECK((page->ty > 0) == !inner);
            CHECK((code->ty > 0.001f) == (capped && cases[k].dy < 0 && !cases[k].mods));
            CHECK((code->tx > 0) == (inner && (cases[k].dx < 0 || cases[k].mods)));
            cui_destroy(ui);
        }
    }
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

static void test_deep_audit_fixes(void) {
    /* 1. Regex UTF-8 and lone continuation byte greedy backtracking (no buffer underflow) */
    {
        cui_regex_match m;
        const char *err = NULL;
        char *heap_utf8 = malloc(4);
        memcpy(heap_utf8, "\xc3\xa9\xc3\xa9", 4);
        CHECK(!cui_regex_search("\\S+z", 0, heap_utf8, 4, 0, &m, &err));
        CHECK(cui_regex_search("\\S+", 0, heap_utf8, 4, 0, &m, &err) && m.start == 0 && m.end == 4);
        free(heap_utf8);
        char *heap_bad = malloc(2);
        heap_bad[0] = (char)0x80; heap_bad[1] = (char)0x80;
        CHECK(!cui_regex_search(".*z", 0, heap_bad, 2, 0, &m, &err));
        CHECK(cui_regex_search(".*", 0, heap_bad, 2, 0, &m, &err) && m.start == 0 && m.end == 2);
        free(heap_bad);
    }

    /* 2. Terminal scrollback double-wrap integrity */
    {
        cui_terminal_buffer tb;
        char text_buf[100];
        cui_terminal_line line_buf[16];
        cui_terminal_init(&tb, text_buf, sizeof text_buf, line_buf, 16);
        char chunk[90];
        memset(chunk, 'a', 80); chunk[80] = '\n';
        cui_terminal_write(&tb, chunk, 81);
        memset(chunk, 'b', 10); chunk[10] = '\n';
        cui_terminal_write(&tb, chunk, 11);
        memset(chunk, 'c', 15); chunk[15] = '\n';
        cui_terminal_write(&tb, chunk, 16);
        memset(chunk, 'd', 15); chunk[15] = '\n';
        cui_terminal_write(&tb, chunk, 16);
        memset(chunk, 'e', 75);
        cui_terminal_write(&tb, chunk, 75);
        CHECK(tb.count == 1);
        const cui_terminal_line *cur = &tb.lines[tb.first];
        CHECK(cur->len == 75 && cur->offset == 0);
        for (uint32_t i = 0; i < cur->len; i++) CHECK(tb.text[cur->offset + i] == 'e');
    }

    /* 3. Textarea trailing newline does not duplicate empty line and Up arrow escapes it */
    {
        cui_ctx *ui = context(false);
        char buf[64] = "a\n";
        cui_id tid = 0;
        for (int f = 0; f < 2; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            cui_textarea(ui, "ta", buf, sizeof buf, .width = CUI_PX(200));
            tid = cui_last_id(ui);
            cui_end_frame(ui);
        }
        cui_edit_state *es = cui_state(ui, tid, sizeof *es);
        es->cursor = es->anchor = 2;
        ui->focus = tid;
        cui_input_key(ui, CUI_KEY_UP, true, 0);
        cui_begin_frame(ui, 1.0f / 60);
        cui_textarea(ui, "ta", buf, sizeof buf, .width = CUI_PX(200));
        cui_end_frame(ui);
        CHECK(es->cursor < 2);
        cui_destroy(ui);
    }

    /* 4. Covered base-layer button must not activate on Enter/Space while popup is open */
    {
        cui_ctx *ui = context(false);
        bool clicked = false;
        cui_id btn_id = 0;
        for (int f = 0; f < 2; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            if (cui_button(ui, "Behind")) clicked = true;
            btn_id = cui_last_id(ui);
            cui_end_frame(ui);
        }
        ui->focus = btn_id;
        ui->focus_visible = true;
        cui_begin_frame(ui, 1.0f / 60);
        if (cui_button(ui, "Behind")) clicked = true;
        cui_popover_open(ui, "pop");
        if (cui_popover_begin(ui, "pop", NULL)) {
            cui_text(ui, "Open");
            cui_popover_end(ui);
        }
        cui_end_frame(ui);
        clicked = false;
        cui_input_key(ui, CUI_KEY_ENTER, true, 0);
        cui_begin_frame(ui, 1.0f / 60);
        if (cui_button(ui, "Behind")) clicked = true;
        if (cui_popover_begin(ui, "pop", NULL)) {
            cui_text(ui, "Open");
            cui_popover_end(ui);
        }
        cui_end_frame(ui);
        CHECK(!clicked);
        cui_destroy(ui);
    }

    /* 5. Zero-duration spring, narrow 18px slider, invalid calendar month, frame_alloc(0), zero-bucket histogram, 0-height viewer, negative-track trace */
    {
        cui_ctx *ui = cui_create(NULL);
        cui_set_viewport(ui, 800, 600, 1);
        cui_begin_frame(ui, 1.0f / 60);
        CHECK(cui__frame_alloc(ui, 0) != NULL);
        float sp = cui_animate_spring(ui, 42, 1.0f, 0.0f, 0.0f);
        CHECK(isfinite(sp) && sp == 1.0f);
        cui_end_frame(ui);
        cui_begin_frame(ui, 1.0f / 60);
        sp = cui_animate_spring(ui, 42, 5.0f, 0.0f, 0.0f);
        CHECK(isfinite(sp) && sp == 5.0f && !ui->animating);
        float sval = 0.5f;
        cui_slider(ui, "narrow", &sval, 0.0f, 1.0f, .width = CUI_PX(18));
        cui_rect sr = cui_last_rect(ui);
        cui_date bad_lo = { 2026, 0, 1 }, bad_hi = { 2026, 13, 1 };
        cui_calendar(ui, "cal0", &bad_lo);
        cui_calendar(ui, "cal13", &bad_hi);
        double sel0 = 0, sel1 = 10;
        cui_histogram_series ts = { .name = "s", .counts = NULL };
        cui_time_histogram_ex(ui, "th0", 0.0, 60.0, 0, &ts, 1, &sel0, &sel1, NULL);
        cui_image_view iv = { 0 };
        cui_image_viewer_ex(ui, "iv0", 0, 100, 100, &iv, &(cui_image_viewer_opts){ .height = CUI_PX(0) });
        CHECK(isfinite(iv.zoom) && isfinite(iv.cx) && isfinite(iv.cy));
        cui_trace_track trk = { .name = "T0" };
        cui_trace_span spans[2] = {
            { .name = "bad", .start = 0, .end = 1, .track = -1, .depth = 0 },
            { .name = "ok", .start = 1, .end = 2, .track = 0, .depth = 0 },
        };
        int tsel = 0;
        cui_trace_view_ex(ui, "tv", &trk, 1, spans, 2, NULL, &tsel, NULL);
        CHECK(tsel == -1);
        cui_end_frame(ui);
        /* drag the 18px slider right at its midpoint (x0 == x1) */
        cui_input_mouse_move(ui, sr.x + 9.0f, sr.y + sr.h * 0.5f);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
        cui_begin_frame(ui, 1.0f / 60);
        cui_slider(ui, "narrow", &sval, 0.0f, 1.0f, .width = CUI_PX(18));
        cui_end_frame(ui);
        CHECK(isfinite(sval));
        cui_destroy(ui);
    }

    /* 6. Software renderer NULL/invalid-argument safety */
    {
        cui_sw_set_threads(NULL, 4);
        cui_sw_renderer *sw = cui_sw_create();
        cui_draw_data dd = { 0 };
        unsigned char px[16] = { 0 };
        cui_sw_render(sw, &dd, px, 0, 4, 16, CUI_SW_RGBA);
        cui_sw_render(sw, &dd, px, 4, 4, -4, CUI_SW_RGBA);
        cui_sw_destroy(sw);
    }
}

static void test_pass2_component_fixes(void) {
    /* 1. cui__utc_offset across non-leap-year New Year boundary and out-of-range timestamps */
    {
        int off_dec = cui__utc_offset(1767223800LL); /* 2025-12-31 23:30:00 UTC */
        int off_jan = cui__utc_offset(1767227400LL); /* 2026-01-01 00:30:00 UTC */
        CHECK(off_dec >= -720 && off_dec <= 840);
        CHECK(off_jan >= -720 && off_jan <= 840);
        CHECK(cui__utc_offset(INT64_MAX) == 0);
    }

    /* 2. cui_url_params / cui_url_set_params ignore '?' inside '#fragment' */
    {
        cui_kv_pair kv[4];
        memset(kv, 0, sizeof kv);
        CHECK(cui_url_params("https://example.com/p#tab?x=1", kv, 4) == 0);
        char url[128] = "https://example.com/p#tab?x=1";
        cui_kv_pair out = { "a", "1", true };
        CHECK(cui_url_set_params(url, sizeof url, &out, 1));
        CHECK(strcmp(url, "https://example.com/p?a=1#tab?x=1") == 0);
    }

    /* 3. Speed dial preserves disabled stack depth when action clicked */
    {
        cui_ctx *ui = context(false);
        cui_speed_action acts[2] = { { "+", "One" }, { "-", "Two" } };
        cui_id fab_id = 0;
        for (int f = 0; f < 2; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            cui_push_disabled(ui, false);
            cui_speed_dial_ex(ui, "sd", acts, 2, NULL);
            fab_id = cui_make_id(ui, "sd");
            cui_pop_disabled(ui);
            cui_end_frame(ui);
        }
        /* Click FAB to open */
        cui_rect fr = cui_get_rect(ui, fab_id);
        cui_input_mouse_move(ui, fr.x + fr.w * 0.5f, fr.y + fr.h * 0.5f);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
        cui_begin_frame(ui, 1.0f / 60);
        cui_push_disabled(ui, false);
        cui_speed_dial_ex(ui, "sd", acts, 2, NULL);
        CHECK(ui->disabled_n == 1);
        cui_pop_disabled(ui);
        cui_end_frame(ui);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
        cui_begin_frame(ui, 1.0f / 60);
        cui_push_disabled(ui, false);
        cui_speed_dial_ex(ui, "sd", acts, 2, NULL);
        CHECK(ui->disabled_n == 1);
        cui_pop_disabled(ui);
        cui_end_frame(ui);
        cui_destroy(ui);
    }

    /* 4. Scrub rejects non-finite user text ("nan") */
    {
        cui_ctx *ui = context(false);
        float val = 5.0f;
        for (int f = 0; f < 2; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            cui_scrub_ex(ui, "sc", &val, 0.0f, 10.0f, NULL);
            cui_end_frame(ui);
        }
        cui_rect sr = cui_get_rect(ui, cui_make_id(ui, "sc"));
        /* Click to enter edit mode, type "nan", press Enter */
        cui_input_mouse_move(ui, sr.x + sr.w * 0.5f, sr.y + sr.h * 0.5f);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
        cui_begin_frame(ui, 1.0f / 60);
        cui_scrub_ex(ui, "sc", &val, 0.0f, 10.0f, NULL);
        cui_end_frame(ui);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
        cui_begin_frame(ui, 1.0f / 60);
        cui_scrub_ex(ui, "sc", &val, 0.0f, 10.0f, NULL);
        cui_end_frame(ui);
        cui_input_text(ui, "nan");
        cui_input_key(ui, CUI_KEY_ENTER, true, 0);
        cui_begin_frame(ui, 1.0f / 60);
        cui_scrub_ex(ui, "sc", &val, 0.0f, 10.0f, NULL);
        cui_end_frame(ui);
        CHECK(isfinite(val) && val == 5.0f);
        cui_destroy(ui);
    }

    /* 5. Tab bar single Right Arrow advances by exactly 1 tab */
    {
        cui_ctx *ui = context(false);
        cui_tab_bar_item tabs[4] = {
            { "A", "One", 0 }, { "B", "Two", 0 }, { "C", "Three", 0 }, { "D", "Four", 0 }
        };
        int sel = 0;
        for (int f = 0; f < 2; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            cui_tab_bar_ex(ui, "tb", tabs, 4, &sel, NULL);
            cui_end_frame(ui);
        }
        int k0 = 0;
        cui_id tab0_id = cui__hash(&k0, sizeof k0, cui_make_id(ui, "tb") ^ 0x7AB);
        ui->focus = tab0_id;
        ui->focus_visible = true;
        cui_input_key(ui, CUI_KEY_RIGHT, true, 0);
        cui_begin_frame(ui, 1.0f / 60);
        bool changed = cui_tab_bar_ex(ui, "tb", tabs, 4, &sel, NULL);
        cui_end_frame(ui);
        CHECK(changed && sel == 1);
        cui_destroy(ui);
    }

    /* 6. Month calendar pre-1970 dates (1969-12-31 has days == -1) */
    {
        cui_ctx *ui = context(false);
        cui_date month = { 1969, 12, 1 };
        cui_date sel = { 1969, 12, 31 };
        cui_month_opts mo = { .selected = &sel, .today = { 1969, 12, 31 } };
        for (int f = 0; f < 2; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            cui_month_calendar_ex(ui, "mc69", &month, NULL, 0, &mo);
            cui_end_frame(ui);
        }
        /* Focus the month calendar grid and press Left Arrow: 1969-12-31 -> 1969-12-30 */
        ui->focus = cui_make_id(ui, "mc69") ^ 0x6A1Du;
        cui_input_key(ui, CUI_KEY_LEFT, true, 0);
        cui_begin_frame(ui, 1.0f / 60);
        cui_month_result mr = cui_month_calendar_ex(ui, "mc69", &month, NULL, 0, &mo);
        cui_end_frame(ui);
        CHECK(mr.day_clicked && sel.year == 1969 && sel.month == 12 && sel.day == 30);
        cui_destroy(ui);
    }

    /* 7. Ruler slider high-magnitude range terminates, histogram/multi_select/signature/table/donut/viz/planning edge cases */
    {
        cui_ctx *ui = context(false);
        float rv = 17000000.0f;
        float hlo = 0.2f, hhi = 0.8f;
        bool msel[2] = { false, false };
        float dvals[3] = { 10, 20, 30 };
        const char *dlabels[3] = { "A", "B", "C" };
        cui_viz_item bad_waffle[2] = { { "Inf", INFINITY, { 0 } }, { "Ok", 10, { 0 } } };
        float bad_hm[4] = { NAN, NAN, 1.0f, 2.0f };
        cui_candle candles[2] = {
            { 10, 12, 9, 11, 100 }, { 11, 13, 10, 12, 200 }
        };
        cui_series ov = { .values = NULL, .count = 2 };
        cui_dot_series ds = { .values = bad_hm, .name = "s" };
        cui_presence_user pu[2] = { { NULL, { 0 }, 0, true }, { "Ada", { 0 }, 0, false } };
        cui_table_column tcols[2] = { { "A", CUI_PX(80), 0, false }, { "B", CUI_PX(80), 0, false } };
        cui_cal_event wev = { "Meet", NULL, 0, 9.0f, 10.0f, CUI_TONE_ACCENT };

        /* Frame 1: exercise extreme/NULL/shrinking inputs cleanly under ASan/UBSan */
        cui_begin_frame(ui, 1.0f / 60);
        cui_ruler_slider_ex(ui, "rl", &rv, 0.0f, 20000000.0f, &(cui_ruler_opts){ .step = 1.0f });
        cui_histogram_slider_ex(ui, "hs1", NULL, 4, &hlo, &hhi, 0.0f, 1.0f, NULL);
        cui_histogram_slider_ex(ui, "hs2", bad_hm, -1, &hlo, &hhi, 0.0f, 1.0f, NULL);
        cui_multi_select_ex(ui, "ms", NULL, 2, msel, NULL);
        cui_donut_chart_ex(ui, "dn", dvals, dlabels, 3, &(cui_chart_opts){ .height = 8 });
        if (cui_table_begin_ex(ui, "tb_early", tcols, 2, NULL)) {
            cui_table_cell_text(ui, "before_row");
            cui_table_end(ui);
        }
        cui_waffle_ex(ui, "wf", bad_waffle, 2, NULL);
        cui_heatmap_ex(ui, "hm1", bad_hm, 2, 2, NULL);
        cui_heatmap_ex(ui, "hm2", NULL, 2, 2, NULL);
        cui_calendar_heatmap_ex(ui, "chm", (cui_date){ 2026, 1, 1 }, NULL, 10, NULL);
        cui_candlestick_ex(ui, "cs", candles, 2, &(cui_candlestick_opts){ .height = 50, .overlays = &ov, .overlay_count = 1 });
        cui_dot_chart_ex(ui, "dc", &ds, 1, 2, &(cui_dot_chart_opts){ .annotations = NULL, .annotation_count = 2 });
        cui_presence(ui, "pr", pu, 2, 4, 28);
        cui_presence(ui, "pr_null", NULL, 2, 4, 28);
        cui_gantt_ex(ui, "gt1", NULL, 2, (cui_date){ 2026, 1, 1 }, 7, NULL);
        cui_gantt_ex(ui, "gt2", NULL, -1, (cui_date){ 2026, 1, 1 }, 7, NULL);
        cui_week_calendar_ex(ui, "wk_null", (cui_date){ 2026, 1, 5 }, NULL, 2, NULL);
        cui_week_calendar_ex(ui, "wk_ro", (cui_date){ 2026, 1, 5 }, &wev, 1, &(cui_week_opts){ .read_only = true, .height = CUI_PX(240) });
        cui_signature_pad_ex(ui, "sig", NULL);
        cui_end_frame(ui);

        /* Shrink donut slice count while potentially hovered, and test signature clear while drawing */
        cui_rect sig_r = cui_get_rect(ui, cui_make_id(ui, "sig"));
        cui_input_mouse_move(ui, sig_r.x + 20, sig_r.y + 20);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
        cui_begin_frame(ui, 1.0f / 60);
        cui_donut_chart_ex(ui, "dn", dvals, dlabels, 1, &(cui_chart_opts){ .height = 8 });
        cui_signature_pad_ex(ui, "sig", NULL);
        cui_signature_clear(ui, "sig");
        cui_end_frame(ui);
        /* Keep mouse held and drag after clear: must not read pts[-1] */
        cui_input_mouse_move(ui, sig_r.x + 30, sig_r.y + 25);
        cui_begin_frame(ui, 1.0f / 60);
        cui_signature_pad_ex(ui, "sig", NULL);
        cui_end_frame(ui);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);

        /* Reorder pending drag with shrinking count */
        int order[3] = { 0, 1, 2 };
        cui_begin_frame(ui, 1.0f / 60);
        cui_reorder_begin(ui, "ro", order, 3);
        for (int k = 0; k < 3; k++) {
            cui_reorder_item_begin(ui, k);
            cui_text(ui, "row");
            cui_reorder_item_end(ui);
        }
        cui_reorder_end(ui);
        cui_end_frame(ui);
        cui_begin_frame(ui, 1.0f / 60);
        cui_reorder_begin(ui, "ro", order, 1);
        cui_reorder_item_begin(ui, 0);
        cui_text(ui, "row");
        cui_reorder_item_end(ui);
        cui_reorder_end(ui);
        cui_end_frame(ui);

        CHECK(cui_get_draw_data(ui)->vertex_count > 0);
        cui_destroy(ui);
    }
}

static void test_wants_keyboard_start_of_frame(void) {
    cui_ctx *ui = context(false);
    char buf[64] = "";
    cui_id input_id = 0;

    /* Initial frame: no focused input -> false at start and end of frame. */
    cui_begin_frame(ui, 1.0f / 60);
    CHECK(!cui_wants_keyboard(ui));
    cui_input(ui, "kb-input", buf, sizeof buf);
    input_id = ui->last_id;
    CHECK(!cui_wants_keyboard(ui));
    cui_end_frame(ui);
    CHECK(!cui_wants_keyboard(ui));

    /* Click to focus the input. */
    cui_rect r = cui_get_rect(ui, input_id);
    cui_input_mouse_move(ui, r.x + r.w * 0.5f, r.y + r.h * 0.5f);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
    cui_begin_frame(ui, 1.0f / 60);
    cui_input(ui, "kb-input", buf, sizeof buf);
    CHECK(cui_wants_keyboard(ui));
    cui_end_frame(ui);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
    CHECK(cui_wants_keyboard(ui));

    /* Next frame: cui_wants_keyboard must already be true at the top of frame()
     * before cui_input() is called, so top-of-frame app shortcuts are suppressed. */
    cui_input_key(ui, CUI_KEY_K, true, CUI_MOD_PRIMARY);
    cui_begin_frame(ui, 1.0f / 60);
    CHECK(cui_wants_keyboard(ui));
    cui_input(ui, "kb-input", buf, sizeof buf);
    CHECK(cui_wants_keyboard(ui));
    cui_end_frame(ui);
    cui_input_key(ui, CUI_KEY_K, false, 0);

    /* Press Escape to blur: suppressed on the Escape frame, then clears on the next. */
    cui_input_key(ui, CUI_KEY_ESCAPE, true, 0);
    cui_begin_frame(ui, 1.0f / 60);
    CHECK(cui_wants_keyboard(ui));
    cui_input(ui, "kb-input", buf, sizeof buf);
    cui_end_frame(ui);
    cui_input_key(ui, CUI_KEY_ESCAPE, false, 0);

    /* Unfocused frame: cui_input sees focus == 0 and clears ui->edit/edit_prev. */
    cui_begin_frame(ui, 1.0f / 60);
    cui_input(ui, "kb-input", buf, sizeof buf);
    cui_end_frame(ui);
    CHECK(!cui_wants_keyboard(ui));

    /* Subsequent frame: top of frame() sees !cui_wants_keyboard(ui). */
    cui_begin_frame(ui, 1.0f / 60);
    CHECK(!cui_wants_keyboard(ui));
    cui_input(ui, "kb-input", buf, sizeof buf);
    cui_end_frame(ui);
    cui_destroy(ui);
}

static void test_pass3_audit_fixes(void) {
    /* 1. State store rehash past 1024 entries must preserve all slots without corruption */
    {
        cui_ctx *ui = context(false);
        cui_begin_frame(ui, 1.0f / 60);
        for (uint32_t i = 1; i <= 1500; i++) {
            uint32_t *slot = cui_state(ui, (cui_id)(i * 2654435761u | 1u), sizeof *slot);
            *slot = i;
        }
        bool ok = true;
        for (uint32_t i = 1; i <= 1500; i++) {
            uint32_t *slot = cui_state(ui, (cui_id)(i * 2654435761u | 1u), sizeof *slot);
            if (*slot != i) { ok = false; break; }
        }
        CHECK(ok);
        cui_end_frame(ui);
        cui_destroy(ui);
    }

    /* 2. cui_url_set_params rejects base >= cap and NULL pairs safely */
    {
        char short_url[16] = "https://a.test";
        cui_kv_pair kv = { "k", "v", true };
        CHECK(!cui_url_set_params(short_url, 4, &kv, 1));
        CHECK(cui_url_set_params(short_url, sizeof short_url, NULL, 2));
        CHECK(strcmp(short_url, "https://a.test") == 0);
    }

    /* 3. cui_time_histogram_ex with +INFINITY bucket does not hang */
    {
        cui_ctx *ui = context(false);
        float counts[4] = { 1.0f, INFINITY, 2.0f, 3.0f };
        cui_histogram_series s = { .name = "s", .counts = counts };
        double sel0 = 0, sel1 = 10;
        cui_begin_frame(ui, 1.0f / 60);
        cui_time_histogram_ex(ui, "th_inf", 0.0, 60.0, 4, &s, 1, &sel0, &sel1, NULL);
        cui_end_frame(ui);
        CHECK(cui_get_draw_data(ui)->vertex_count > 0);
        cui_destroy(ui);
    }

    /* 4. Code block, code editor and diff view render leading-space/tab lines */
    {
        cui_ctx *ui = context(false);
        cui_begin_frame(ui, 1.0f / 60);
        cui_code_block(ui, "cb_empty", "", .lang = "text");
        cui_end_frame(ui);
        int empty_v = cui_get_draw_data(ui)->vertex_count;

        cui_begin_frame(ui, 1.0f / 60);
        cui_code_block(ui, "cb_ws", "    indented\n\ttabbed\n", .lang = "text");
        cui_end_frame(ui);
        CHECK(cui_get_draw_data(ui)->vertex_count > empty_v);

        cui_begin_frame(ui, 1.0f / 60);
        cui_code_block(ui, "cb_c", "/* start\n    still comment\n*/\n", .lang = "c");
        cui_end_frame(ui);
        CHECK(cui_get_draw_data(ui)->vertex_count > empty_v);

        char ed_empty[64] = "";
        cui_begin_frame(ui, 1.0f / 60);
        cui_code_editor_ex(ui, "ed_empty", ed_empty, sizeof ed_empty, &(cui_code_editor_opts){ .lang = "text", .height = 120 });
        cui_end_frame(ui);
        int ed_empty_v = cui_get_draw_data(ui)->vertex_count;

        char ed_ws[64] = "    hello\n\tworld";
        cui_begin_frame(ui, 1.0f / 60);
        cui_code_editor_ex(ui, "ed_ws", ed_ws, sizeof ed_ws, &(cui_code_editor_opts){ .lang = "text", .height = 120 });
        cui_end_frame(ui);
        CHECK(cui_get_draw_data(ui)->vertex_count > ed_empty_v);

        cui_begin_frame(ui, 1.0f / 60);
        cui_diff_view_ex(ui, "dv_empty", "", "", NULL);
        cui_end_frame(ui);
        int dv_empty_v = cui_get_draw_data(ui)->vertex_count;

        cui_begin_frame(ui, 1.0f / 60);
        cui_diff_view_ex(ui, "dv_ws", "    old_line\n", "    new_line\n", NULL);
        cui_end_frame(ui);
        CHECK(cui_get_draw_data(ui)->vertex_count > dv_empty_v);
        cui_destroy(ui);
    }

    /* 5. Money input >= 1,000 keeps value when clicked and blurred */
    {
        cui_ctx *ui = context(false);
        double amount = 1234.56;
        cui_id mid = 0;
        for (int f = 0; f < 2; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            cui_money_input_ex(ui, "amt", &amount, NULL);
            mid = cui_last_id(ui);
            cui_end_frame(ui);
        }
        cui_rect mr = cui_get_rect(ui, mid);
        cui_input_mouse_move(ui, mr.x + mr.w * 0.5f, mr.y + mr.h * 0.5f);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
        cui_begin_frame(ui, 1.0f / 60);
        cui_money_input_ex(ui, "amt", &amount, NULL);
        cui_end_frame(ui);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
        cui_begin_frame(ui, 1.0f / 60);
        cui_money_input_ex(ui, "amt", &amount, NULL);
        cui_end_frame(ui);
        /* Click outside to blur */
        cui_input_mouse_move(ui, 790, 590);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
        cui_begin_frame(ui, 1.0f / 60);
        cui_money_input_ex(ui, "amt", &amount, NULL);
        cui_end_frame(ui);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
        CHECK(fabs(amount - 1234.56) < 0.001);
        cui_destroy(ui);
    }

    /* 6. Candlestick shrinking count re-clamps cursor and span safely */
    {
        cui_ctx *ui = context(false);
        cui_candle candles[20];
        for (int i = 0; i < 20; i++) candles[i] = (cui_candle){ 10 + i, 12 + i, 9 + i, 11 + i, 100 };
        cui_id cid = 0;
        for (int f = 0; f < 2; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            cui_candlestick_ex(ui, "cs_shrink", candles, 20, NULL);
            cid = cui_last_id(ui);
            cui_end_frame(ui);
        }
        ui->focus = cid;
        ui->focus_visible = true;
        cui_input_key(ui, CUI_KEY_END, true, 0);
        cui_begin_frame(ui, 1.0f / 60);
        cui_candlestick_ex(ui, "cs_shrink", candles, 20, NULL);
        cui_end_frame(ui);
        cui_input_key(ui, CUI_KEY_END, false, 0);
        /* Now shrink to 3 candles while cursor was at 19 */
        cui_begin_frame(ui, 1.0f / 60);
        cui_candlestick_ex(ui, "cs_shrink", candles, 3, NULL);
        cui_end_frame(ui);
        CHECK(cui_get_draw_data(ui)->vertex_count > 0);
        cui_destroy(ui);
    }

    /* 7. Modal layer ID matches inside modal and blocks click-focused background button */
    {
        cui_ctx *ui = context(false);
        bool bg_clicked = false, modal_open = false, modal_btn_clicked = false;
        cui_id bg_id = 0, mb_id = 0;
        for (int f = 0; f < 2; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            if (cui_button(ui, "BgBtn")) bg_clicked = true;
            bg_id = cui_last_id(ui);
            cui_end_frame(ui);
        }
        /* Click-focus BgBtn (focus_visible == false), then open modal */
        ui->focus = bg_id;
        ui->focus_visible = false;
        modal_open = true;
        for (int f = 0; f < 2; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            if (cui_button(ui, "BgBtn")) bg_clicked = true;
            if (cui_dialog_begin(ui, "Dialog", &modal_open, NULL)) {
                if (cui_button(ui, "InsideBtn")) modal_btn_clicked = true;
                mb_id = cui_last_id(ui);
                cui_dialog_end(ui);
            }
            cui_end_frame(ui);
        }
        /* Focus was cleared on modal open; even if re-forced onto bg_id, Enter must not click BgBtn */
        CHECK(ui->focus != bg_id);
        ui->focus = bg_id;
        bg_clicked = false;
        cui_input_key(ui, CUI_KEY_ENTER, true, 0);
        cui_begin_frame(ui, 1.0f / 60);
        if (cui_button(ui, "BgBtn")) bg_clicked = true;
        if (cui_dialog_begin(ui, "Dialog", &modal_open, NULL)) {
            if (cui_button(ui, "InsideBtn")) modal_btn_clicked = true;
            cui_dialog_end(ui);
        }
        cui_end_frame(ui);
        CHECK(!bg_clicked);
        cui_input_key(ui, CUI_KEY_ENTER, false, 0);
        /* Clicking InsideBtn works cleanly */
        cui_rect mbr = cui_get_rect(ui, mb_id);
        CHECK(mbr.w > 0);
        cui_input_mouse_move(ui, mbr.x + mbr.w * 0.5f, mbr.y + mbr.h * 0.5f);
        cui_begin_frame(ui, 1.0f / 60);
        cui_button(ui, "BgBtn");
        if (cui_dialog_begin(ui, "Dialog", &modal_open, NULL)) {
            if (cui_button(ui, "InsideBtn")) modal_btn_clicked = true;
            cui_dialog_end(ui);
        }
        cui_end_frame(ui);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
        cui_begin_frame(ui, 1.0f / 60);
        cui_button(ui, "BgBtn");
        if (cui_dialog_begin(ui, "Dialog", &modal_open, NULL)) {
            if (cui_button(ui, "InsideBtn")) modal_btn_clicked = true;
            cui_dialog_end(ui);
        }
        cui_end_frame(ui);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
        cui_begin_frame(ui, 1.0f / 60);
        cui_button(ui, "BgBtn");
        if (cui_dialog_begin(ui, "Dialog", &modal_open, NULL)) {
            if (cui_button(ui, "InsideBtn")) modal_btn_clicked = true;
            cui_dialog_end(ui);
        }
        cui_end_frame(ui);
        CHECK(modal_btn_clicked);
        cui_destroy(ui);
    }

    /* 8. Rapid release+press across two buttons clears stale ui->active */
    {
        cui_ctx *ui = context(false);
        cui_id id_a = 0, id_b = 0;
        bool b_clicked = false;
        for (int f = 0; f < 2; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            cui_button(ui, "BtnA"); id_a = cui_last_id(ui);
            if (cui_button(ui, "BtnB")) b_clicked = true;
            id_b = cui_last_id(ui);
            cui_end_frame(ui);
        }
        cui_rect ra = cui_get_rect(ui, id_a), rb = cui_get_rect(ui, id_b);
        cui_input_mouse_move(ui, ra.x + ra.w * 0.5f, ra.y + ra.h * 0.5f);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
        cui_begin_frame(ui, 1.0f / 60);
        cui_button(ui, "BtnA"); cui_button(ui, "BtnB");
        cui_end_frame(ui);
        CHECK(ui->active == id_a);
        /* In one inter-frame burst: release on A, move to B, press on B */
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
        cui_input_mouse_move(ui, rb.x + rb.w * 0.5f, rb.y + rb.h * 0.5f);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
        cui_begin_frame(ui, 1.0f / 60);
        cui_button(ui, "BtnA");
        if (cui_button(ui, "BtnB")) b_clicked = true;
        cui_end_frame(ui);
        CHECK(ui->active == id_b);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
        cui_begin_frame(ui, 1.0f / 60);
        cui_button(ui, "BtnA");
        if (cui_button(ui, "BtnB")) b_clicked = true;
        cui_end_frame(ui);
        CHECK(b_clicked);
        cui_destroy(ui);
    }

    /* 9. Spring numerical stability for extreme duration/bounce & theme shadow_resting retarget */
    {
        cui_ctx *ui = cui_create(NULL);
        cui_set_viewport(ui, 800, 600, 1);
        cui_begin_frame(ui, 1.0f / 60);
        float s0 = cui_animate_spring(ui, 99, 0.0f, 0.01f, 1.5f);
        CHECK(isfinite(s0));
        cui_end_frame(ui);
        for (int f = 0; f < 10; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            float v = cui_animate_spring(ui, 99, 100.0f, 0.01f, 1.5f);
            CHECK(isfinite(v));
            cui_end_frame(ui);
        }
        float light_shadow_a = ui->theme.shadow_resting[0].alpha;
        cui_set_reduced_motion(ui, true);
        cui_set_theme(ui, CUI_THEME_DARK);
        cui_begin_frame(ui, 1.0f / 60);
        cui_end_frame(ui);
        CHECK(ui->theme.shadow_resting[0].alpha > light_shadow_a);
        cui_destroy(ui);
    }

    /* 10. Keyboard Enter opening select / confirm popover must not close/confirm same frame */
    {
        cui_ctx *ui = context(false);
        const char *items[2] = { "Alpha", "Beta" };
        int sel = 0;
        cui_id sel_id = 0, trig_id = 0;
        for (int f = 0; f < 2; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            cui_select_ex(ui, "sel_kb", items, 2, &sel, NULL);
            sel_id = cui_last_id(ui);
            if (cui_button(ui, "Delete")) cui_popover_open(ui, "cp_kb");
            trig_id = cui_last_id(ui);
            cui_confirm_popover_ex(ui, "cp_kb", &(cui_confirm_popover_opts){ .title = "Really?" });
            cui_end_frame(ui);
        }
        ui->focus = sel_id;
        ui->focus_visible = true;
        cui_input_key(ui, CUI_KEY_ENTER, true, 0);
        cui_begin_frame(ui, 1.0f / 60);
        cui_select_ex(ui, "sel_kb", items, 2, &sel, NULL);
        cui_end_frame(ui);
        cui_input_key(ui, CUI_KEY_ENTER, false, 0);
        bool any_open = false;
        for (int i = 0; i < ui->popup_count; i++) if (ui->popups[i].open) any_open = true;
        CHECK(any_open);
        cui_destroy(ui);

        /* Confirm popover opened via Enter on trigger button must not confirm same frame */
        ui = context(false);
        for (int f = 0; f < 2; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            if (cui_button(ui, "Delete")) cui_popover_open(ui, "cp_kb");
            trig_id = cui_last_id(ui);
            cui_confirm_popover_ex(ui, "cp_kb", &(cui_confirm_popover_opts){ .title = "Really?" });
            cui_end_frame(ui);
        }
        ui->focus = trig_id;
        ui->focus_visible = true;
        cui_input_key(ui, CUI_KEY_ENTER, true, 0);
        cui_begin_frame(ui, 1.0f / 60);
        if (cui_button(ui, "Delete")) cui_popover_open(ui, "cp_kb");
        bool confirmed = cui_confirm_popover_ex(ui, "cp_kb", &(cui_confirm_popover_opts){ .title = "Really?" });
        cui_end_frame(ui);
        cui_input_key(ui, CUI_KEY_ENTER, false, 0);
        CHECK(!confirmed && cui_popover_is_open(ui, "cp_kb"));
        cui_destroy(ui);
    }

    /* 11. Untouched/disabled value controls and non-finite number input */
    {
        cui_ctx *ui = context(false);
        float sv = 0.33333334f, rlo = 0.25f, rhi = 0.75f, kv = 0.42f, fv = 0.67f;
        double nv = 42.0;
        cui_image_view iv = { 0 };
        cui_begin_frame(ui, 1.0f / 60);
        cui_push_disabled(ui, true);
        CHECK(!cui_slider_ex(ui, "sl", &sv, 0.0f, 1.0f, NULL));
        CHECK(!cui_range_slider(ui, "rs", &rlo, &rhi, 0.0f, 1.0f));
        CHECK(!cui_knob_ex(ui, "kn", &kv, 0.0f, 1.0f, NULL));
        CHECK(!cui_fader_ex(ui, "fd", &fv, 0.0f, 1.0f, NULL));
        CHECK(!cui_image_viewer_ex(ui, "iv", 0, 200, 100, &iv, NULL));
        cui_pop_disabled(ui);
        CHECK(!cui_image_viewer_ex(ui, "iv_en", 0, 200, 100, &iv, NULL));
        cui_end_frame(ui);
        CHECK(sv == 0.33333334f && rlo == 0.25f && rhi == 0.75f && kv == 0.42f && fv == 0.67f);

        /* cui_number rejects "nan" */
        cui_id nid = 0;
        for (int f = 0; f < 2; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            cui_number_ex(ui, "num", &nv, 0.0, 100.0, NULL);
            nid = cui_last_id(ui);
            cui_end_frame(ui);
        }
        ui->focus = nid;
        cui_input_key(ui, CUI_KEY_A, true, CUI_MOD_PRIMARY);
        cui_begin_frame(ui, 1.0f / 60);
        cui_number_ex(ui, "num", &nv, 0.0, 100.0, NULL);
        cui_end_frame(ui);
        cui_input_key(ui, CUI_KEY_A, false, 0);
        cui_input_text(ui, "nan");
        cui_input_key(ui, CUI_KEY_ENTER, true, 0);
        cui_begin_frame(ui, 1.0f / 60);
        cui_number_ex(ui, "num", &nv, 0.0, 100.0, NULL);
        cui_end_frame(ui);
        CHECK(isfinite(nv) && nv == 42.0);
        cui_destroy(ui);
    }

    /* 12. Shadow negative spread, 16-item speed dial, snippet-only source list,
       negative donut, 1-pt line chart, query builder NULL choices, tiny hex/log, month NULL */
    {
        cui_ctx *ui = context(false);
        cui_begin_frame(ui, 1.0f / 60);
        cui_draw_shadow(ui, (cui_rect){ 10, 10, 80, 40 }, CUI_CORNERS(0), 6, -4, (cui_vec2){ 0, 2 }, cui__rgba(0, 0, 0, 80));
        cui_speed_action acts[16];
        for (int i = 0; i < 16; i++) acts[i] = (cui_speed_action){ "+", "Item" };
        cui_speed_dial_ex(ui, "sd16", acts, 16, NULL);
        cui_source src = { .title = "Doc", .url = NULL, .snippet = "Summary text" };
        cui_source_list(ui, "srcs", &src, 1);
        float neg_donut[3] = { -5.0f, 10.0f, 20.0f };
        const char *dlab[3] = { "Neg", "A", "B" };
        cui_donut_chart_ex(ui, "dn_neg", neg_donut, dlab, 3, NULL);
        float one_pt[1] = { 7.0f };
        cui_series one_ser = { .values = one_pt, .count = 1 };
        cui_line_chart_ex(ui, "lc1", &one_ser, 1, NULL);
        cui_query_field qf = { .name = "Status", .type = CUI_QUERY_CHOICE, .choices = NULL, .choice_count = 2 };
        cui_query_node qn[2] = { { .group = true, .depth = 0 }, { .depth = 1, .field = 0 } };
        int qcnt = 2;
        CHECK(!cui_query_builder_ex(ui, "qb_ro", &qf, 1, qn, &qcnt, 2, &(cui_query_opts){ .read_only = true }));
        uint8_t hbytes[4] = { 1, 2, 3, 4 };
        cui_hex_view_ex(ui, "hx_tiny", hbytes, 4, NULL, &(cui_hex_view_opts){ .size = 0.1f });
        cui_log_view_ex(ui, "lv_tiny", NULL, &(cui_log_view_opts){ .size = 0.1f });
        cui_month_calendar_ex(ui, "mc_a", NULL, NULL, 0, NULL);
        cui_month_calendar_ex(ui, "mc_b", NULL, NULL, 0, NULL);
        cui_end_frame(ui);
        const cui_draw_data *dd = cui_get_draw_data(ui);
        bool all_finite = true;
        for (int v = 0; v < dd->vertex_count; v++) {
            if (!isfinite(dd->vertices[v].x) || !isfinite(dd->vertices[v].y)) { all_finite = false; break; }
        }
        CHECK(all_finite && dd->vertex_count > 0);
        cui_destroy(ui);
    }

    /* 13. Cropper CUI__CR_NEW drag up-left is not clamped to previous crop */
    {
        cui_ctx *ui = context(false);
        cui_rect crop = { 50, 50, 100, 100 };
        cui_id cr_id = 0;
        for (int f = 0; f < 2; f++) {
            cui_begin_frame(ui, 1.0f / 60);
            cui_cropper_ex(ui, "cr", 0, 400, 300, &crop, &(cui_cropper_opts){ .width = CUI_PX(428), .height = CUI_PX(328) });
            cr_id = cui_last_id(ui);
            cui_end_frame(ui);
        }
        cui_rect pr = cui_get_rect(ui, cr_id);
        /* pad is 14 -> scale z = 1, ox = pr.x + 14, oy = pr.y + 14 */
        float ox = pr.x + 14, oy = pr.y + 14;
        cui_input_mouse_move(ui, ox + 300, oy + 250);
        cui_begin_frame(ui, 1.0f / 60);
        cui_cropper_ex(ui, "cr", 0, 400, 300, &crop, &(cui_cropper_opts){ .width = CUI_PX(428), .height = CUI_PX(328) });
        cui_end_frame(ui);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
        cui_begin_frame(ui, 1.0f / 60);
        cui_cropper_ex(ui, "cr", 0, 400, 300, &crop, &(cui_cropper_opts){ .width = CUI_PX(428), .height = CUI_PX(328) });
        cui_end_frame(ui);
        cui_input_mouse_move(ui, ox + 100, oy + 100);
        cui_begin_frame(ui, 1.0f / 60);
        cui_cropper_ex(ui, "cr", 0, 400, 300, &crop, &(cui_cropper_opts){ .width = CUI_PX(428), .height = CUI_PX(328) });
        cui_end_frame(ui);
        cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
        CHECK(fabsf(crop.x - 100.0f) < 2.0f && fabsf(crop.w - 200.0f) < 2.0f);
        CHECK(fabsf(crop.y - 100.0f) < 2.0f && fabsf(crop.h - 150.0f) < 2.0f);
        cui_destroy(ui);
    }

    /* 14. Flexbox grow distribution with mixed min/max constraints conserves full container width */
    {
        cui_ctx *ui = context(false);
        cui_id ids[3] = { 0 };
        cui_begin_frame(ui, 1.0f / 60);
        cui_row(ui, .width = CUI_PX(300)) {
            ids[0] = cui_box_begin(ui, &(cui_box){ .id = "g0", .width = CUI_GROW(1), .max_width = 50, .height = CUI_PX(20) });
            cui_box_end(ui);
            ids[1] = cui_box_begin(ui, &(cui_box){ .id = "g1", .width = CUI_GROW(1), .min_width = 160, .height = CUI_PX(20) });
            cui_box_end(ui);
            ids[2] = cui_box_begin(ui, &(cui_box){ .id = "g2", .width = CUI_GROW(1), .height = CUI_PX(20) });
            cui_box_end(ui);
        }
        cui_end_frame(ui);
        cui_rect r0 = cui_get_rect(ui, ids[0]), r1 = cui_get_rect(ui, ids[1]), r2 = cui_get_rect(ui, ids[2]);
        CHECK(fabsf((r0.w + r1.w + r2.w) - 300.0f) < 1.0f);
        CHECK(fabsf(r0.w - 50.0f) < 1.0f && fabsf(r1.w - 160.0f) < 1.0f && fabsf(r2.w - 90.0f) < 1.0f);
        cui_destroy(ui);
    }

#ifdef CUI_TEXT_SHAPING
    /* 15. OpenType cmap format 12 out-of-range startGlyphID returns 0 */
    {
        uint8_t buf[64];
        memset(buf, 0, sizeof buf);
        /* format 12 at offset 4: format=12, length=28, numGroups=1, group: start=0x41, end=0x42, startGlyphID=0x80000000 */
        buf[4] = 0; buf[5] = 12;
        buf[15] = 28;
        buf[19] = 1;
        buf[23] = 0x41; buf[27] = 0x42;
        buf[28] = 0x80; buf[29] = 0; buf[30] = 0; buf[31] = 0;
        cui_shape_font sf;
        memset(&sf, 0, sizeof sf);
        sf.data = buf;
        sf.size = sizeof buf;
        sf.cmap = 4;
        CHECK(cui__shape_map(&sf, 0x41) == 0);
    }
#endif
}

int main(void) {
    test_ascii_helpers(); test_scopes();
    test_shrinking_list(); test_table_controls(); test_horizontal_drag(); test_code_scroll_routing();
    test_parsers(); test_file_names(); test_picker_capacity();
    test_large_row_ranges(); test_json_validation(); test_blame_without_commits();
    test_coverage_scale(); test_texture_bounds();
    test_file_globs(); test_chart_ranges();
    test_deep_audit_fixes();
    test_pass2_component_fixes();
    test_wants_keyboard_start_of_frame();
    test_pass3_audit_fixes();
    printf("audit regressions: %d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
