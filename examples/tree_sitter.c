/* Optional JSON editor; this demo links the JSON test grammar, not the core. */
#include <cui/cui_app.h>
#include <cui/cui_tree_sitter.h>
#include <stdio.h>

const struct TSLanguage *tree_sitter_json(void);

int main(void) {
    const char *query = "(string) @string (number) @number [(true) (false) (null)] @constant "
                        "(pair key: (string) @string.special.key)";
    cui_tree_sitter_error error;
    cui_tree_sitter *document = cui_tree_sitter_create(tree_sitter_json(), query, &error);
    if (!document) { fprintf(stderr, "Highlight query at byte %u: %s\n", error.offset, error.message); return 1; }
    cui_app *app = cui_app_create(&(cui_app_desc){ .title = "Tree-sitter editor", .width = 800, .height = 560 });
    if (!app) { cui_tree_sitter_destroy(document); return 1; }
    char text[64 * 1024] = "{\n  \"region\": \"north\",\n  \"cache_size\": 256,\n  \"prefetch\": true,\n  \"levels\": [2, 4, 8, 16]\n}\n";
    while (cui_app_frame(app)) {
        cui_ctx *ui = cui_app_ui(app);
        cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 24, .gap = 12) {
            cui_heading(ui, "Cache configuration", 2);
            cui_secondary(ui, "Tree-sitter JSON highlighting. Changes are parsed as you type.");
            cui_code_editor_result r = cui_code_editor(ui, "config", text, sizeof text,
                .lang = "json", .highlighter = cui_tree_sitter_highlighter(document), .height = -1);
            error = cui_tree_sitter_last_error(document);
            if (error.message) cui_text(ui, error.message, .color = CUI_COLOR_DANGER);
            else cui_textf(ui, "Ln %d, Col %d", r.line, r.column);
        }
    }
    cui_app_destroy(app);
    cui_tree_sitter_destroy(document);
    return 0;
}
