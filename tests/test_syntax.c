/* Shared highlighter: language samples, aliases, bounded input and viewport state. */
#include "cui_internal.h"

static int syntax_failed, syntax_checks;
#define CHECK(c) do { syntax_checks++; if (!(c)) { syntax_failed++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

typedef struct { const char *lang, *source, *word; int kind; const char *aliases; } syntax_case;
static const syntax_case syntax_cases[] = {
    { "json", "{\"count\": 42, \"ready\": true}", "\"count\"", CUI__TK_FUNC, "" },
    { "astro", "<Tile name=\"river\" />", "Tile", CUI__TK_KEYWORD, "" },
    { "bash", "if test -f config; then echo 'ready'; fi", "if", CUI__TK_KEYWORD, "sh|shell|zsh" },
    { "c", "int count = 42; // count", "int", CUI__TK_TYPE, "h" },
    { "cmake", "add_executable(tiles main.c)", "add_executable", CUI__TK_KEYWORD, "" },
    { "cpp", "constexpr int count = 42;", "constexpr", CUI__TK_KEYWORD, "c++|cxx|cc|hpp" },
    { "csharp", "namespace Tiles { public record Tile(int Size); }", "namespace", CUI__TK_KEYWORD, "cs|c#" },
    { "css", ".tile { background-color: green; }", "background-color", CUI__TK_FUNC, "" },
    { "diff", "+added entry", "+added entry", CUI__TK_STRING, "patch" },
    { "ejs", "<h1><%= name %></h1>", "<%= name %>", CUI__TK_PREPROC, "" },
    { "elixir", "defmodule Tiles do # main", "defmodule", CUI__TK_KEYWORD, "ex|exs" },
    { "erb", "<p><%= @name %></p>", "<%= @name %>", CUI__TK_PREPROC, "rhtml" },
    { "go", "func main() { fmt.Println(`river`) }", "func", CUI__TK_KEYWORD, "golang" },
    { "graphql", "query Tiles { tiles { name } }", "query", CUI__TK_KEYWORD, "" },
    { "html", "<section class=\"tiles\">River</section>", "section", CUI__TK_KEYWORD, "htm" },
    { "java", "public class Tile { int size = 42; }", "public", CUI__TK_KEYWORD, "" },
    { "javascript", "const name = `river`;", "const", CUI__TK_KEYWORD, "js|jsx" },
    { "jsdoc", "@param {string} name Description", "param", CUI__TK_KEYWORD, "" },
    { "kotlin", "fun total(items: List<Int>) = items.sum()", "fun", CUI__TK_KEYWORD, "kt|kts" },
    { "lua", "local count = 42 -- total", "local", CUI__TK_KEYWORD, "" },
    { "make", "include config.mk", "include", CUI__TK_KEYWORD, "makefile" },
    { "markdown", "## Tile cache", "## Tile cache", CUI__TK_KEYWORD, "md" },
    { "markdown_inline", "Read `tile_get()` here", "`tile_get()`", CUI__TK_STRING, "md_inline" },
    { "php", "function total($items) { return 42; } # total", "function", CUI__TK_KEYWORD, "" },
    { "proto", "message Tile { int32 size = 1; }", "message", CUI__TK_KEYWORD, "protobuf" },
    { "python", "def total(items): # sum", "def", CUI__TK_KEYWORD, "py" },
    { "ruby", "unless ready then puts 'waiting' end", "unless", CUI__TK_KEYWORD, "rb" },
    { "rust", "fn name<'a>(s: &'a str) {}", "fn", CUI__TK_KEYWORD, "rs" },
    { "scala", "given count: Int = 42", "given", CUI__TK_KEYWORD, "sc" },
    { "sql", "SELECT name FROM tiles WHERE count > 42;", "SELECT", CUI__TK_KEYWORD, "" },
    { "svelte", "<h1 class=\"tile\">{name}</h1>", "h1", CUI__TK_KEYWORD, "" },
    { "swift", "guard let name = value else { return }", "guard", CUI__TK_KEYWORD, "" },
    { "toml", "cache_size = 42 # entries", "cache_size", CUI__TK_FUNC, "" },
    { "tsx", "const tile: Tile = <Tile size={42} />;", "const", CUI__TK_KEYWORD, "" },
    { "typescript", "interface Tile { size: number; }", "interface", CUI__TK_KEYWORD, "ts" },
    { "yaml", "cache-size: 42 # entries", "cache-size", CUI__TK_FUNC, "yml" },
    { "zig", "pub fn main() void {}", "pub", CUI__TK_KEYWORD, "" },
};

static void syntax_expect(const char *lang, const char *source, const char *word, int kind) {
    cui__tok tokens[256];
    bool block = false, found = false;
    const char *at = strstr(source, word);
    CHECK(at != NULL);
    if (!at) return;
    int count = cui__tokenize_lang(lang, source, (int)strlen(source), &block, tokens, CUI_COUNTOF(tokens));
    for (int t = 0; t < count; t++) {
        if (tokens[t].start == at - source && tokens[t].len == (int)strlen(word) && tokens[t].kind == kind) found = true;
    }
    if (!found) fprintf(stderr, "  %s: expected '%s' to have kind %d in %s\n", lang, word, kind, source);
    CHECK(found);
}

static void syntax_bounds(const char *lang, const char *source) {
    cui__lang rules = cui__lang_for(lang);
    for (int n = 0; n <= (int)strlen(source); n++) {
        char *bytes = malloc((size_t)n + (n == 0));   /* deliberately no terminator */
        CHECK(bytes != NULL);
        if (!bytes) return;
        memcpy(bytes, source, (size_t)n);
        cui__tok tokens[256];
        bool block = false;
        int count = cui__tokenize_lang(lang, bytes, n, &block, tokens, CUI_COUNTOF(tokens)), end = 0;
        CHECK(count >= 0 && count <= CUI_COUNTOF(tokens));
        for (int t = 0; t < count; t++) {
            CHECK(tokens[t].start == end && tokens[t].len > 0);
            CHECK(tokens[t].kind < CUI__TK_COUNT);
            end = tokens[t].start + tokens[t].len;
            CHECK(end <= n);
        }
        CHECK(end == n);
        CHECK(cui__syntax_block_state(&rules, bytes, n, false) == block);
        free(bytes);
    }
}

static void test_syntax_languages(void) {
    CHECK(CUI_COUNTOF(syntax_cases) == 37);
    for (int i = 0; i < CUI_COUNTOF(syntax_cases); i++) {
        const syntax_case *c = &syntax_cases[i];
        syntax_expect(c->lang, c->source, c->word, c->kind);
        syntax_bounds(c->lang, c->source);
        const char *a = c->aliases;
        while (*a) {
            const char *end = strchr(a, '|');
            int n = end ? (int)(end - a) : (int)strlen(a);
            char alias[32];
            CHECK(n < (int)sizeof alias);
            if (n >= (int)sizeof alias) break;
            memcpy(alias, a, (size_t)n); alias[n] = 0;
            syntax_expect(alias, c->source, c->word, c->kind);
            if (!end) break;
            a = end + 1;
        }
        cui__lang rules = cui__lang_for(c->lang);
        const char *comments[] = { rules.line_comment, rules.line_comment2 };
        for (int k = 0; k < CUI_COUNTOF(comments); k++) if (comments[k]) {
            char line[128];
            snprintf(line, sizeof line, "%s comment", comments[k]);
            syntax_expect(c->lang, line, line, CUI__TK_COMMENT);
            syntax_bounds(c->lang, line);
        }
        if (rules.block_start) {
            char open[128], close[128], quoted[128];
            snprintf(open, sizeof open, "%s open comment", rules.block_start);
            snprintf(close, sizeof close, "comment closes %s", rules.block_end);
            snprintf(quoted, sizeof quoted, "\"%s\"", rules.block_start);
            cui__tok tokens[1];
            bool block = false;
            CHECK(cui__tokenize_lang(c->lang, open, (int)strlen(open), &block, tokens, 1) == 1);
            CHECK(block && tokens[0].kind == CUI__TK_COMMENT);
            CHECK(cui__syntax_block_state(&rules, open, (int)strlen(open), false));
            CHECK(cui__tokenize_lang(c->lang, close, (int)strlen(close), &block, tokens, 1) == 1);
            CHECK(!block && tokens[0].kind == CUI__TK_COMMENT);
            CHECK(!cui__syntax_block_state(&rules, close, (int)strlen(close), true));
            CHECK(!cui__syntax_block_state(&rules, quoted, (int)strlen(quoted), false));
            syntax_bounds(c->lang, open);
            syntax_bounds(c->lang, quoted);
            char long_line[2048];
            memset(long_line, ';', 1500);
            snprintf(long_line + 1500, sizeof long_line - 1500, "%s tail", rules.block_start);
            block = false;
            cui__tokenize_lang(c->lang, long_line, (int)strlen(long_line), &block, tokens, 1);
            /* The token buffer may fill before a comment that continues offscreen. */
            CHECK(block == cui__syntax_block_state(&rules, long_line, (int)strlen(long_line), false));
        }
    }
}

static void test_syntax_rules(void) {
    syntax_expect("ejs", "<%= \"%>\" %> tail", "<%= \"%>\" %>", CUI__TK_PREPROC);
    syntax_expect("erb", "<%# cached tiles %>", "<%# cached tiles %>", CUI__TK_COMMENT);
    syntax_bounds("ejs", "<%= \"<!-- %>\" %> tail");
    syntax_bounds("erb", "<%# ' %> tail");
    syntax_expect("markdown_inline", "# Heading", "Heading", CUI__TK_TEXT);
    syntax_expect("graphql", "query Tiles($limit: Int)", "Int", CUI__TK_TYPE);
    syntax_expect("proto", "int32 size = 1;", "int32", CUI__TK_TYPE);
    syntax_expect("jsdoc", "@param {string} name", "string", CUI__TK_TYPE);
    syntax_expect("sql", "select 'it''s ready' FROM tiles", "'it''s ready'", CUI__TK_STRING);
    syntax_expect("sql", "SeLeCt name", "SeLeCt", CUI__TK_KEYWORD);
    syntax_expect("sql", "SELECT 'C:\\' -- path", "-- path", CUI__TK_COMMENT);
    syntax_expect("markdown", "It's ready", "ready", CUI__TK_TEXT);
    syntax_expect("html", "<section data-id=\"42\">Text</section>", "data-id", CUI__TK_FUNC);
    syntax_expect("toml", "[cache] # settings", "[cache]", CUI__TK_TYPE);
    syntax_expect("yaml", "\"cache-size\": 42", "\"cache-size\"", CUI__TK_FUNC);
    syntax_expect("yaml", "ready: true", "true", CUI__TK_CONST);
    syntax_expect("markdown", "Read `tile_get()` here", "`tile_get()`", CUI__TK_STRING);
    syntax_expect("diff", "-removed", "-removed", CUI__TK_PREPROC);
    syntax_expect("diff", "@@ -1 +1 @@", "@@ -1 +1 @@", CUI__TK_KEYWORD);
    syntax_expect("make", "tiles: main.c", "tiles", CUI__TK_FUNC);
    syntax_expect("rust", "fn name<'a>(s: &'a str)", "str", CUI__TK_TYPE);
    syntax_expect("rust", "let letter = 'x';", "'x'", CUI__TK_STRING);
    syntax_expect("c", "printf(\"// string \\\" end\");", "\"// string \\\" end\"", CUI__TK_STRING);
    syntax_expect("c", "double size = 1.5e-3;", "1.5e-3", CUI__TK_NUMBER);
    syntax_expect("c", "ifx = 42;", "ifx", CUI__TK_TEXT);
    syntax_expect("text", "if 42 \"string\" // comment", "if 42 \"string\" // comment", CUI__TK_TEXT);
    syntax_expect("plain", "<section>", "<section>", CUI__TK_TEXT);
    syntax_expect("unknown", "42", "42", CUI__TK_NUMBER);

    const char *removed[] = { "dart", "perl", "r", "julia", "objc", "xml", "powershell", "batch",
        "dockerfile", "ini", "asm", "fortran", "pascal", "vb", "matlab", "groovy", "haskell",
        "erlang", "clojure", "fsharp", "ocaml", "glsl" };
    cui__lang generic = cui__lang_for(NULL);
    for (int i = 0; i < CUI_COUNTOF(removed); i++) {
        cui__lang rules = cui__lang_for(removed[i]);
        CHECK(rules.kw == generic.kw && rules.types == generic.types);
        CHECK(!rules.line_comment && !rules.block_start && rules.mode == CUI__LEX_CODE);
    }
}

static bool syntax_has_color(cui_ctx *ui, int kind) {
    cui_color colors[CUI__TK_COUNT];
    cui__syntax_colors(ui, colors);
    const cui_draw_data *draw = cui_get_draw_data(ui);
    for (int v = 0; v < draw->vertex_count; v++) {
        const cui_vertex *vertex = &draw->vertices[v];
        if (vertex->mode == CUI_SHADE_TEXT && memcmp(&vertex->color, &colors[kind], sizeof(cui_color)) == 0) return true;
    }
    return false;
}

typedef struct { uint8_t kinds[64]; int calls, mode; } syntax_provider;
static const uint8_t *syntax_provider_update(void *user, const char *source, int len) {
    syntax_provider *p = user;
    CHECK(source != NULL && len == 2 && !memcmp(source, "42", 2));
    p->calls++;
    memset(p->kinds, p->mode == 2 ? 255 : CUI_SYNTAX_KEYWORD, (size_t)len);
    return p->mode == 1 ? NULL : p->kinds;
}

static void test_syntax_provider(void) {
    cui_ctx *ui = cui_create(NULL);
    cui_set_reduced_motion(ui, true);
    cui_set_viewport(ui, 700, 300, 1);
    syntax_provider state = {0};
    cui_code_highlighter highlighter = { &state, syntax_provider_update };
    for (int dark = 0; dark < 2; dark++) {
        cui_set_theme(ui, dark ? CUI_THEME_DARK : CUI_THEME_LIGHT);
        for (int component = 0; component < 2; component++) {
            for (int mode = 0; mode < 3; mode++) {
                state.mode = mode;
                int before = state.calls;
                for (int frame = 0; frame < 2; frame++) {
                    char text[8] = "42";
                    cui_begin_frame(ui, 1.0f / 60);
                    if (component == 0) cui_code_block(ui, "custom-block", text, .lang = "json", .highlighter = &highlighter, .no_header = true);
                    else cui_code_editor(ui, "custom-editor", text, sizeof text, .lang = "json", .highlighter = &highlighter, .no_line_numbers = true, .height = 180);
                    memset(state.kinds, CUI_SYNTAX_STRING, sizeof state.kinds);   /* frame paint has its own copy */
                    cui_end_frame(ui);
                }
                CHECK(state.calls == before + 2);
                CHECK(syntax_has_color(ui, mode == 0 ? CUI__TK_KEYWORD : CUI__TK_NUMBER));
                CHECK(!syntax_has_color(ui, CUI__TK_STRING));
            }
        }
    }
    highlighter.update = NULL;
    cui_begin_frame(ui, 1.0f / 60);
    cui_code_block(ui, "no-callback", "42", .lang = "json", .highlighter = &highlighter, .no_header = true);
    cui_end_frame(ui);
    CHECK(syntax_has_color(ui, CUI__TK_NUMBER));
    cui_destroy(ui);
}

static void test_syntax_components(void) {
    cui_ctx *ui = cui_create(NULL);
    CHECK(ui != NULL);
    if (!ui) return;
    cui_set_reduced_motion(ui, true);
    for (int dark = 0; dark < 2; dark++) {
        cui_set_theme(ui, dark ? CUI_THEME_DARK : CUI_THEME_LIGHT);
        for (int i = 0; i < CUI_COUNTOF(syntax_cases); i++) {
            const syntax_case *c = &syntax_cases[i];
            char buffer[256];
            snprintf(buffer, sizeof buffer, "%s", c->source);
            for (int component = 0; component < 3; component++) {
                cui_set_viewport(ui, 700, 300, 1);
                for (int frame = 0; frame < 2; frame++) {
                    cui_begin_frame(ui, 1.0f / 60);
                    if (component == 0) cui_code_block(ui, "block", buffer, .lang = c->lang, .no_header = true);
                    if (component == 1) cui_code_editor(ui, "editor", buffer, sizeof buffer, .lang = c->lang, .height = 200);
                    if (component == 2) cui_diff_view(ui, "diff", "", buffer, .language = c->lang, .height = 200);
                    cui_end_frame(ui);
                }
                if (!syntax_has_color(ui, c->kind)) fprintf(stderr, "  %s: component %d, dark %d missing syntax color\n", c->lang, component, dark);
                CHECK(syntax_has_color(ui, c->kind));
                CHECK(!strcmp(buffer, c->source));
            }
        }
    }
    /* A comment starts past the right edge and above the first visible line. */
    char long_comment[8192];
    memset(long_comment, ' ', 300);
    strcpy(long_comment + 300, "<!--\n");
    for (int i = 0; i < 70; i++) strcat(long_comment, "inside comment\n");
    strcat(long_comment, "-->\n<tile/>\n");
    const int targets[] = { 1, 50 };
    for (int i = 0; i < CUI_COUNTOF(targets); i++) {
        cui_code_editor_goto(ui, "comment-editor", targets[i], 1);
        for (int frame = 0; frame < 3; frame++) {
            cui_begin_frame(ui, 1.0f / 60);
            cui_code_editor(ui, "comment-editor", long_comment, sizeof long_comment, .lang = "html", .height = 160, .no_line_numbers = true, .read_only = true);
            cui_end_frame(ui);
        }
        CHECK(syntax_has_color(ui, CUI__TK_COMMENT));
        CHECK(!syntax_has_color(ui, CUI__TK_TEXT));
    }
    cui_destroy(ui);
}

int main(void) {
    test_syntax_languages();
    test_syntax_rules();
    test_syntax_components();
    test_syntax_provider();
    printf("syntax: %d checks, %d failures\n", syntax_checks, syntax_failed);
    return syntax_failed ? 1 : 0;
}
