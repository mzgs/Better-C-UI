/* Optional editor with the GPUI Kit grammar bundle. Grammars belong to this demo, not the core. */
#include <cui/cui_app.h>
#include "languages.h"
#include <tree_sitter/api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cui_demo_check_languages(void) {
    int failed = 0;
    for (int i = 0; i < CUI_DEMO_LANGUAGE_COUNT; i++) {
        const cui_demo_language *language = &cui_demo_languages[i];
        cui_tree_sitter_error error;
        cui_tree_sitter *document = cui_tree_sitter_create(language->language(), language->query, &error);
        if (!document) {
            fprintf(stderr, "%s query at byte %u: %s\n", language->name, error.offset, error.message);
            failed++;
            continue;
        }
        int len = (int)strlen(language->sample), colored = 0;
        const cui_code_highlighter *highlighter = cui_tree_sitter_highlighter(document);
        const uint8_t *colors = highlighter->update(highlighter->user, language->sample, len);
        if (colors) for (int j = 0; j < len; j++) {
            if (colors[j] >= CUI_SYNTAX_COUNT) failed++;
            if (colors[j] != CUI_SYNTAX_TEXT && colors[j] != CUI_SYNTAX_PUNCTUATION) colored++;
        }
        if (!colors || !colored || cui_tree_sitter_last_error(document).message) {
            fprintf(stderr, "%s: sample did not highlight\n", language->name);
            failed++;
        }
        if (highlighter->update(highlighter->user, language->sample, len) != colors) failed++;
        char edited[4096];
        uint8_t expected[4096];
        bool had_colors = colors != NULL;
        if ((size_t)len + 2 > sizeof edited) { failed++; }
        else {
            if (colors) memcpy(expected, colors, (size_t)len);
            memcpy(edited, language->sample, (size_t)len);
            edited[len] = '\n'; edited[len + 1] = '\0';
            bool edited_ok = highlighter->update(highlighter->user, edited, len + 1) != NULL;
            const uint8_t *restored = highlighter->update(highlighter->user, language->sample, len);
            if (!edited_ok || !restored || !had_colors || memcmp(expected, restored, (size_t)len) != 0 ||
                cui_tree_sitter_last_error(document).message) {
                fprintf(stderr, "%s: edit/restore changed highlighting\n", language->name);
                failed++;
            }
        }
        TSParser *parser = ts_parser_new();
        TSTree *tree = NULL;
        if (parser && ts_parser_set_language(parser, language->language()))
            tree = ts_parser_parse_string(parser, NULL, language->sample, (uint32_t)len);
        if (!tree || ts_node_has_error(ts_tree_root_node(tree))) {
            fprintf(stderr, "%s: sample has parse errors\n", language->name);
            if (tree) {
                char *sexp = ts_node_string(ts_tree_root_node(tree));
                if (sexp) { fprintf(stderr, "%s\n", sexp); free(sexp); }
            }
            failed++;
        }
        ts_tree_delete(tree);
        ts_parser_delete(parser);
        cui_tree_sitter_destroy(document);
    }
    printf("Tree-sitter: %d languages checked, %d failures\n", CUI_DEMO_LANGUAGE_COUNT, failed);
    return failed ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--check") == 0) return cui_demo_check_languages();
    static char drafts[CUI_DEMO_LANGUAGE_COUNT][64 * 1024];
    const char *names[CUI_DEMO_LANGUAGE_COUNT];
    for (int i = 0; i < CUI_DEMO_LANGUAGE_COUNT; i++) {
        names[i] = cui_demo_languages[i].name;
        snprintf(drafts[i], sizeof drafts[i], "%s", cui_demo_languages[i].sample);
    }
    char description[128];
    snprintf(description, sizeof description, "%d language entries. Each keeps its draft until you close the app.", CUI_DEMO_LANGUAGE_COUNT);
    cui_app *app = cui_app_create(&(cui_app_desc){ .title = "Tree-sitter editor", .width = 800, .height = 640 });
    if (!app) return 1;
    int selected = 0, active = -1;
    cui_tree_sitter *document = NULL;
    cui_tree_sitter_error error = {0};
    while (cui_app_frame(app)) {
        cui_ctx *ui = cui_app_ui(app);
        cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 16, .gap = 12) {
            cui_heading(ui, "Tree-sitter editor", 2);
            cui_select(ui, "language", names, CUI_DEMO_LANGUAGE_COUNT, &selected, .label = "Language", .searchable = true);
            cui_secondary(ui, description);
            if (selected != active) {
                cui_tree_sitter_destroy(document);
                document = cui_tree_sitter_create(cui_demo_languages[selected].language(),
                    cui_demo_languages[selected].query, &error);
                active = selected;
            }
            cui_push_id(ui, cui_demo_languages[selected].id);
            cui_code_editor_result result = cui_code_editor(ui, "source", drafts[selected], sizeof drafts[selected],
                .lang = cui_demo_languages[selected].id, .highlighter = cui_tree_sitter_highlighter(document), .height = -1);
            cui_pop_id(ui);
            if (document) error = cui_tree_sitter_last_error(document);
            if (error.message) cui_text(ui, error.message, .color = CUI_COLOR_DANGER);
            else cui_textf(ui, "Tree-sitter · %s · Ln %d, Col %d", names[selected], result.line, result.column);
        }
    }
    cui_app_destroy(app);
    cui_tree_sitter_destroy(document);
    return 0;
}
