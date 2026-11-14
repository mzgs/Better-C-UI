/* Real parser integration: cached/incremental edits, query filtering and UI snapshots. */
#include "cui_internal.h"
#include "cui/cui_tree_sitter.h"
#include <tree_sitter/api.h>

const TSLanguage *tree_sitter_json(void);
static int ts_checks, ts_failed;
#define CHECK(c) do { ts_checks++; if (!(c)) { ts_failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static const uint8_t *ts_update(cui_tree_sitter *d, const char *source, int len) {
    const cui_code_highlighter *h = cui_tree_sitter_highlighter(d);
    return h ? h->update(h->user, source, len) : NULL;
}

static const char ts_json_query[] =
    "(string) @string\n"
    "(number) @number\n"
    "[(true) (false) (null)] @constant.builtin\n"
    "(pair key: (string) @string.special.key)\n"
    "(comment) @comment\n"
    "[\"{\" \"}\" \"[\" \"]\" \":\" \",\"] @punctuation\n";

static cui_tree_sitter *ts_document(const char *query) {
    cui_tree_sitter_error error;
    cui_tree_sitter *d = cui_tree_sitter_create(tree_sitter_json(), query, &error);
    if (!d) fprintf(stderr, "query error at %u: %s\n", error.offset, error.message);
    CHECK(d != NULL && error.message == NULL);
    return d;
}

static void ts_expect(cui_tree_sitter *d, const char *source, const char *word, int kind) {
    const char *at = strstr(source, word);
    const uint8_t *colors = ts_update(d, source, (int)strlen(source));
    CHECK(at && colors);
    if (!at || !colors) return;
    for (int i = 0; i < (int)strlen(word); i++) {
        if (colors[at - source + i] != kind) fprintf(stderr, "%s in %s: kind %u, expected %d\n", word, source, colors[at - source + i], kind);
        CHECK(colors[at - source + i] == kind);
    }
    CHECK(cui_tree_sitter_last_error(d).message == NULL);
}

static void test_ts_creation(void) {
    cui_tree_sitter_error error;
    CHECK(cui_tree_sitter_create(NULL, "", &error) == NULL && error.message);
    CHECK(cui_tree_sitter_create(tree_sitter_json(), NULL, &error) == NULL && error.message);
    CHECK(cui_tree_sitter_create(tree_sitter_json(), "(missing_node) @string", &error) == NULL && error.message);
    CHECK(cui_tree_sitter_create(tree_sitter_json(), "(string", &error) == NULL && error.message);
    CHECK(cui_tree_sitter_create(tree_sitter_json(), "((string) @x (#unknown? @x))", &error) == NULL && error.message);
    CHECK(cui_tree_sitter_create(tree_sitter_json(), "((string) @x (#set! priority \"100\"))", &error) == NULL && error.message);
    CHECK(cui_tree_sitter_create(tree_sitter_json(), "((string) @x (#match? @x \"[\"))", &error) == NULL && error.message);
    CHECK(cui_tree_sitter_create(tree_sitter_json(), "((string)+ @x (#eq? @x \"x\"))", &error) == NULL && error.message);
    CHECK(cui_tree_sitter_create(tree_sitter_json(), "((string) @x (#eq? @x))", &error) == NULL && error.message);
    CHECK(cui_tree_sitter_last_error(NULL).message != NULL);
    CHECK(cui_tree_sitter_highlighter(NULL) == NULL);
    cui_tree_sitter_destroy(NULL);
    cui_tree_sitter *d = ts_document(ts_json_query);
    if (!d) return;
    CHECK(ts_update(d, NULL, 0) == NULL);
    CHECK(ts_update(d, "", -1) == NULL);
    CHECK(cui_tree_sitter_last_error(d).message != NULL);
    ts_expect(d, "{\"name\": \"river\", \"count\": 42, \"ready\": true}", "\"name\"", CUI__TK_FUNC);
    ts_expect(d, "{\"name\": \"river\", \"count\": 42, \"ready\": true}", "\"river\"", CUI__TK_STRING);
    ts_expect(d, "{\"name\": \"river\", \"count\": 42, \"ready\": true}", "42", CUI__TK_NUMBER);
    ts_expect(d, "{\"name\": \"river\", \"count\": 42, \"ready\": true}", "true", CUI__TK_CONST);
    ts_expect(d, "{\"items\": [1, 2", "\"items\"", CUI__TK_STRING);  /* incomplete source is normal */
    cui_tree_sitter_destroy(d);
}

static void test_ts_predicates(void) {
    static const struct { const char *query, *yes, *no; } cases[] = {
        { "((string) @keyword (#eq? @keyword \"\\\"river\\\"\"))", "\"river\"", "\"hill\"" },
        { "((string) @keyword (#not-eq? @keyword \"\\\"hill\\\"\"))", "\"river\"", "\"hill\"" },
        { "((string) @keyword (#match? @keyword \"river\"))", "\"river\"", "\"hill\"" },
        { "((string) @keyword (#not-match? @keyword \"hill\"))", "\"river\"", "\"hill\"" },
        { "((string) @keyword (#any-of? @keyword \"\\\"river\\\"\" \"\\\"lake\\\"\"))", "\"river\"", "\"hill\"" },
        { "((string) @keyword (#not-any-of? @keyword \"\\\"hill\\\"\" \"\\\"ridge\\\"\"))", "\"river\"", "\"hill\"" },
    };
    for (int i = 0; i < CUI_COUNTOF(cases); i++) {
        cui_tree_sitter *d = ts_document(cases[i].query);
        if (!d) continue;
        ts_expect(d, "[\"river\", \"hill\"]", cases[i].yes, CUI__TK_KEYWORD);
        ts_expect(d, "[\"river\", \"hill\"]", cases[i].no, CUI__TK_TEXT);
        cui_tree_sitter_destroy(d);
    }
    cui_tree_sitter *d = ts_document("(pair key: (string) @_key value: (string) @keyword (#eq? @_key @keyword))");
    if (!d) return;
    ts_expect(d, "{\"river\": \"river\", \"hill\": \"lake\"}", "\"lake\"", CUI__TK_TEXT);
    const uint8_t *colors = ts_update(d, "{\"river\": \"river\"}", 18);
    CHECK(colors && colors[10] == CUI__TK_KEYWORD);
    cui_tree_sitter_destroy(d);
}

static void test_ts_edits(void) {
    cui_tree_sitter *d = ts_document(ts_json_query);
    if (!d) return;
    const char *edits[] = {
        "", "{}", "{\"name\": \"river\"}", "{\"name\": \"r\xc3\xa9ver\"}",
        "{\"name\": \"r\xc3\xaaver\"}", "{\"name\": \"r\xf0\x9f\x8c\x8aver\"}",
        "{\n\"name\": \"river\",\n\"count\": 125.5e-2\n}", "{\"unfinished\": [true, false,",
        "{\"name\": \"r\xc3\xa9ver\"}", "{}", "", "[null]",
    };
    for (int i = 0; i < CUI_COUNTOF(edits); i++) {
        int len = (int)strlen(edits[i]);
        const uint8_t *incremental = ts_update(d, edits[i], len);
        CHECK(incremental != NULL);
        CHECK(ts_update(d, edits[i], len) == incremental);   /* unchanged buffers keep their cache */
        cui_tree_sitter *fresh = ts_document(ts_json_query);
        const uint8_t *expected = ts_update(fresh, edits[i], len);
        CHECK(incremental && expected && !memcmp(incremental, expected, (size_t)len));
        cui_tree_sitter_destroy(fresh);
    }
    /* Many edits, including broken intermediate UTF-8 and arbitrary buffer replacement. */
    unsigned seed = 7;
    char text[128] = "{\"name\": \"river\", \"count\": 42}";
    for (int i = 0; i < 150; i++) {
        seed = seed * 1664525u + 1013904223u;
        int len = (int)strlen(text), at = (int)(seed % (unsigned)(len + 1));
        if (i % 3 == 0 && len < 120) {
            memmove(text + at + 1, text + at, (size_t)(len - at + 1));
            text[at] = "{}[]\"\\ \n09,\xc3\xa9"[(seed >> 8) % 14];
            if (!text[at]) text[at] = 'x';
        } else if (len && at < len) memmove(text + at, text + at + 1, (size_t)(len - at));
        len = (int)strlen(text);
        const uint8_t *incremental = ts_update(d, text, len);
        cui_tree_sitter *fresh = ts_document(ts_json_query);
        const uint8_t *expected = ts_update(fresh, text, len);
        CHECK(incremental && expected && !memcmp(incremental, expected, (size_t)len));
        cui_tree_sitter_destroy(fresh);
    }
    cui_tree_sitter_destroy(d);
}

static bool ts_render_has_color(cui_ctx *ui, int kind) {
    cui_color colors[CUI__TK_COUNT];
    cui__syntax_colors(ui, colors);
    const cui_draw_data *data = cui_get_draw_data(ui);
    for (int i = 0; i < data->vertex_count; i++) {
        const cui_vertex *v = &data->vertices[i];
        if (v->mode == CUI_SHADE_TEXT && !memcmp(&v->color, &colors[kind], sizeof v->color)) return true;
    }
    return false;
}

static void test_ts_fallback(void) {
    cui_tree_sitter *d = ts_document("((string) @keyword (#match? @keyword \"^\\\"(a+)+\\\"$\"))");
    if (!d) return;
    const char *hard = "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaab\"";
    CHECK(ts_update(d, hard, (int)strlen(hard)) == NULL);
    CHECK(cui_tree_sitter_last_error(d).message != NULL);
    cui_ctx *ui = cui_create(NULL);
    cui_set_viewport(ui, 700, 300, 1);
    cui_set_reduced_motion(ui, true);
    for (int i = 0; i < 2; i++) {
        cui_begin_frame(ui, 1.0f / 60);
        cui_code_block(ui, "fallback", hard, .lang = "json", .highlighter = cui_tree_sitter_highlighter(d), .no_header = true);
        cui_end_frame(ui);
    }
    CHECK(ts_render_has_color(ui, CUI__TK_STRING));
    CHECK(!ts_render_has_color(ui, CUI__TK_KEYWORD));
    ts_expect(d, "\"aa\"", "\"aa\"", CUI__TK_KEYWORD);
    cui_destroy(ui);
    cui_tree_sitter_destroy(d);
}

static void test_ts_editor(void) {
    cui_ctx *ui = cui_create(NULL);
    cui_set_reduced_motion(ui, true);
    cui_set_viewport(ui, 700, 300, 1);
    for (int dark = 0; dark < 2; dark++) {
        cui_set_theme(ui, dark ? CUI_THEME_DARK : CUI_THEME_LIGHT);
        cui_tree_sitter *d = ts_document("(number) @keyword");
        char text[128] = "42";
        for (int frame = 0; frame < 3; frame++) {
            cui_begin_frame(ui, 1.0f / 60);
            cui_code_editor(ui, "editor", text, sizeof text, .lang = "text", .highlighter = cui_tree_sitter_highlighter(d), .autofocus = true, .no_line_numbers = true);
            cui_end_frame(ui);
        }
        CHECK(ts_render_has_color(ui, CUI__TK_KEYWORD));
        cui_input_key(ui, CUI_KEY_A, true, CUI_MOD_PRIMARY);
        cui_input_key(ui, CUI_KEY_A, false, CUI_MOD_PRIMARY);
        cui_begin_frame(ui, 1.0f / 60);
        cui_code_editor(ui, "editor", text, sizeof text, .lang = "text", .highlighter = cui_tree_sitter_highlighter(d), .no_line_numbers = true);
        cui_end_frame(ui);
        cui_input_key(ui, CUI_KEY_NONE, false, 0);
        cui_input_text(ui, "\"river\"");
        cui_begin_frame(ui, 1.0f / 60);
        cui_code_editor_result result = cui_code_editor(ui, "editor", text, sizeof text, .lang = "text", .highlighter = cui_tree_sitter_highlighter(d), .no_line_numbers = true);
        cui_end_frame(ui);
        CHECK(result.changed && !strcmp(text, "\"river\""));
        CHECK(!ts_render_has_color(ui, CUI__TK_KEYWORD));  /* no stale colors on the edit frame */
        cui_begin_frame(ui, 1.0f / 60);
        cui_code_block(ui, "block", "42", .lang = "text", .highlighter = cui_tree_sitter_highlighter(d), .no_header = true);
        cui_tree_sitter_destroy(d);   /* paint owns a snapshot, not the backend */
        cui_end_frame(ui);
        CHECK(ts_render_has_color(ui, CUI__TK_KEYWORD));
    }
    cui_destroy(ui);
}

int main(void) {
    test_ts_creation();
    test_ts_predicates();
    test_ts_edits();
    test_ts_editor();
    test_ts_fallback();
    printf("tree-sitter: %d checks, %d failures\n", ts_checks, ts_failed);
    return ts_failed ? 1 : 0;
}
