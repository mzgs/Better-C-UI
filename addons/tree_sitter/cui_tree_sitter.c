/* Optional document parser; depends only on the public CUI API. */
#include "cui/cui_tree_sitter.h"
#include <tree_sitter/api.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct cui_tree_sitter {
    cui_code_highlighter highlighter;
    TSParser *parser;
    TSTree *tree;
    TSQuery *query;
    TSQueryCursor *cursor;
    char **strings;
    uint32_t string_count;
    uint8_t *capture_kinds, *capture_depths;
    char *source;
    uint8_t *kinds;
    int len;
    cui_tree_sitter_error error;
};

static const uint8_t *cui__ts_update(void *user, const char *source, int len);

typedef struct {
    uint32_t start, end;
    uint16_t pattern;
    uint8_t kind, depth;
} cui__ts_capture;

enum { CUI__TS_EQ, CUI__TS_NOT_EQ, CUI__TS_MATCH, CUI__TS_NOT_MATCH, CUI__TS_ANY_OF, CUI__TS_NOT_ANY_OF };

static int cui__ts_operator(const char *name) {
    static const char *const names[] = { "eq?", "not-eq?", "match?", "not-match?", "any-of?", "not-any-of?" };
    for (int i = 0; i < (int)(sizeof names / sizeof names[0]); i++) if (!strcmp(name, names[i])) return i;
    return -1;
}

static int cui__ts_kind(const char *name, uint32_t n) {
    static const struct { const char *name; uint8_t kind; } names[] = {
        { "keyword", CUI_SYNTAX_KEYWORD }, { "conditional", CUI_SYNTAX_KEYWORD }, { "repeat", CUI_SYNTAX_KEYWORD },
        { "type", CUI_SYNTAX_TYPE }, { "constructor", CUI_SYNTAX_TYPE },
        { "string.special.key", CUI_SYNTAX_FUNCTION }, { "string", CUI_SYNTAX_STRING }, { "character", CUI_SYNTAX_STRING },
        { "escape", CUI_SYNTAX_STRING }, { "number", CUI_SYNTAX_NUMBER }, { "float", CUI_SYNTAX_NUMBER },
        { "comment", CUI_SYNTAX_COMMENT }, { "function", CUI_SYNTAX_FUNCTION }, { "method", CUI_SYNTAX_FUNCTION },
        { "preproc", CUI_SYNTAX_PREPROCESSOR }, { "constant", CUI_SYNTAX_CONSTANT }, { "boolean", CUI_SYNTAX_CONSTANT },
        { "punctuation", CUI_SYNTAX_PUNCTUATION }, { "operator", CUI_SYNTAX_PUNCTUATION }, { "delimiter", CUI_SYNTAX_PUNCTUATION },
        { "variable", CUI_SYNTAX_TEXT }, { "property", CUI_SYNTAX_TEXT }, { "label", CUI_SYNTAX_TEXT },
    };
    for (int i = 0; i < (int)(sizeof names / sizeof names[0]); i++) {
        size_t size = strlen(names[i].name);
        if (n >= size && !strncmp(name, names[i].name, size) && (n == size || name[size] == '.')) return names[i].kind;
    }
    return -1;   /* helper and unsupported captures do not erase other colors */
}

static bool cui__ts_validate(cui_tree_sitter *d) {
    for (uint32_t p = 0; p < ts_query_pattern_count(d->query); p++) {
        uint32_t count;
        const TSQueryPredicateStep *steps = ts_query_predicates_for_pattern(d->query, p, &count);
        for (uint32_t i = 0; i < count;) {
            uint32_t end = i;
            while (end < count && steps[end].type != TSQueryPredicateStepTypeDone) end++;
            d->error.offset = ts_query_start_byte_for_pattern(d->query, p);
            if (end - i < 3 || steps[i].type != TSQueryPredicateStepTypeString ||
                steps[i + 1].type != TSQueryPredicateStepTypeCapture) {
                d->error.message = "unsupported Tree-sitter predicate arguments";
                return false;
            }
            int op = cui__ts_operator(d->strings[steps[i].value_id]);
            if (op < 0) { d->error.message = "unsupported Tree-sitter predicate or directive"; return false; }
            if ((op < CUI__TS_ANY_OF && end - i != 3) ||
                ((op == CUI__TS_MATCH || op == CUI__TS_NOT_MATCH) && steps[i + 2].type != TSQueryPredicateStepTypeString)) {
                d->error.message = "unsupported Tree-sitter predicate arguments";
                return false;
            }
            for (uint32_t k = i + 1; k < end; k++) {
                if (steps[k].type == TSQueryPredicateStepTypeCapture) {
                    if (op >= CUI__TS_ANY_OF && k != i + 1) {
                        d->error.message = "any-of predicates require string alternatives";
                        return false;
                    }
                    if (ts_query_capture_quantifier_for_id(d->query, p, steps[k].value_id) != TSQuantifierOne) {
                        d->error.message = "quantified predicate captures are unsupported";
                        return false;
                    }
                }
            }
            if (op == CUI__TS_MATCH || op == CUI__TS_NOT_MATCH) {
                cui_regex_match match;
                const char *error = NULL;
                if (cui_regex_search(d->strings[steps[i + 2].value_id], 0, "", 0, 0, &match, &error) < 0) {
                    d->error.message = "predicate regex is unsupported by cui_regex_search";
                    return false;
                }
            }
            i = end + 1;
        }
    }
    d->error = (cui_tree_sitter_error){0};
    return true;
}

void cui_tree_sitter_destroy(cui_tree_sitter *d) {
    if (!d) return;
    ts_tree_delete(d->tree);
    if (d->cursor) ts_query_cursor_delete(d->cursor);
    ts_query_delete(d->query);
    ts_parser_delete(d->parser);
    for (uint32_t i = 0; i < d->string_count; i++) free(d->strings[i]);
    free(d->strings);
    free(d->capture_kinds);
    free(d->capture_depths);
    free(d->source);
    free(d->kinds);
    free(d);
}

cui_tree_sitter *cui_tree_sitter_create(const TSLanguage *language, const char *query, cui_tree_sitter_error *error) {
    cui_tree_sitter_error failure = { "out of memory", 0 };
    cui_tree_sitter *d = NULL;
    if (!language || !query || strlen(query) > UINT32_MAX) {
        failure.message = "a language and highlight query are required";
        goto fail;
    }
    d = calloc(1, sizeof *d);
    if (!d) goto fail;
    d->parser = ts_parser_new();
    if (!d->parser) goto fail;
    if (!ts_parser_set_language(d->parser, language)) {
        failure.message = "incompatible Tree-sitter grammar ABI";
        goto fail;
    }
    TSQueryError query_error;
    d->query = ts_query_new(language, query, (uint32_t)strlen(query), &failure.offset, &query_error);
    if (!d->query) { failure.message = "invalid Tree-sitter highlight query"; goto fail; }
    d->cursor = ts_query_cursor_new();
    if (!d->cursor) goto fail;
    ts_query_cursor_set_match_limit(d->cursor, 65536);
    d->string_count = ts_query_string_count(d->query);
    d->strings = calloc((d->string_count ? d->string_count : 1), sizeof *d->strings);
    if (!d->strings) { d->string_count = 0; goto fail; }
    for (uint32_t i = 0; i < d->string_count; i++) {
        uint32_t n;
        const char *value = ts_query_string_value_for_id(d->query, i, &n);
        d->strings[i] = malloc((size_t)n + 1);
        if (!d->strings[i]) goto fail;
        memcpy(d->strings[i], value, n); d->strings[i][n] = 0;
    }
    uint32_t captures = ts_query_capture_count(d->query);
    d->capture_kinds = malloc((captures ? captures : 1));
    d->capture_depths = calloc((captures ? captures : 1), 1);
    if (!d->capture_kinds || !d->capture_depths) goto fail;
    for (uint32_t i = 0; i < captures; i++) {
        uint32_t n;
        const char *name = ts_query_capture_name_for_id(d->query, i, &n);
        int kind = cui__ts_kind(name, n), depth = 0;
        d->capture_kinds[i] = kind < 0 ? UINT8_MAX : (uint8_t)kind;
        for (uint32_t k = 0; k < n && depth < UINT8_MAX; k++) if (name[k] == '.') depth++;
        d->capture_depths[i] = (uint8_t)depth;
    }
    if (!cui__ts_validate(d)) { failure = d->error; goto fail; }
    d->highlighter = (cui_code_highlighter){ d, cui__ts_update };
    if (error) *error = (cui_tree_sitter_error){0};
    return d;
fail:
    cui_tree_sitter_destroy(d);
    if (error) *error = failure;
    return NULL;
}

const cui_code_highlighter *cui_tree_sitter_highlighter(cui_tree_sitter *d) {
    return d ? &d->highlighter : NULL;
}

cui_tree_sitter_error cui_tree_sitter_last_error(const cui_tree_sitter *d) {
    return d ? d->error : (cui_tree_sitter_error){ "no Tree-sitter document", 0 };
}

static TSNode cui__ts_node(const TSQueryMatch *match, uint32_t id) {
    for (uint16_t i = 0; i < match->capture_count; i++) if (match->captures[i].index == id) return match->captures[i].node;
    return (TSNode){0};
}

static bool cui__ts_equal_text(const char *source, TSNode a, const char *b, uint32_t n) {
    return !ts_node_is_null(a) && ts_node_end_byte(a) - ts_node_start_byte(a) == n &&
           !memcmp(source + ts_node_start_byte(a), b, n);
}

static bool cui__ts_predicates(cui_tree_sitter *d, const TSQueryMatch *match, const char *source) {
    uint32_t count;
    const TSQueryPredicateStep *steps = ts_query_predicates_for_pattern(d->query, match->pattern_index, &count);
    for (uint32_t i = 0; i < count;) {
        uint32_t end = i;
        while (end < count && steps[end].type != TSQueryPredicateStepTypeDone) end++;
        int op = cui__ts_operator(d->strings[steps[i].value_id]);
        TSNode node = cui__ts_node(match, steps[i + 1].value_id);
        if (ts_node_is_null(node)) return false;
        bool found = false;
        for (uint32_t k = i + 2; k < end; k++) {
            uint32_t n;
            const char *value;
            if (steps[k].type == TSQueryPredicateStepTypeCapture) {
                TSNode other = cui__ts_node(match, steps[k].value_id);
                if (ts_node_is_null(other)) return false;
                n = ts_node_end_byte(other) - ts_node_start_byte(other);
                value = source + ts_node_start_byte(other);
            } else { value = d->strings[steps[k].value_id]; n = (uint32_t)strlen(value); }
            if (op == CUI__TS_MATCH || op == CUI__TS_NOT_MATCH) {
                cui_regex_match result;
                const char *error = NULL;
                /* shortcut: regexes compile per capture, cache them if predicate-heavy queries dominate. */
                int matched = cui_regex_search(value, 0, source + ts_node_start_byte(node),
                                              (int)(ts_node_end_byte(node) - ts_node_start_byte(node)), 0, &result, &error);
                if (matched < 0) { d->error.message = "predicate regex failed or exceeded its work limit"; return false; }
                found = matched > 0;
            } else if (cui__ts_equal_text(source, node, value, n)) found = true;
        }
        if (op == CUI__TS_NOT_EQ || op == CUI__TS_NOT_MATCH || op == CUI__TS_NOT_ANY_OF) found = !found;
        if (!found) return false;
        i = end + 1;
    }
    return true;
}

static int cui__ts_capture_order(const void *a, const void *b) {
    const cui__ts_capture *x = a, *y = b;
    uint32_t xn = x->end - x->start, yn = y->end - y->start;
    if (xn != yn) return xn > yn ? -1 : 1;
    if (x->depth != y->depth) return x->depth < y->depth ? -1 : 1;
    if (x->pattern != y->pattern) return x->pattern < y->pattern ? -1 : 1;
    if (x->kind != y->kind) return x->kind < y->kind ? -1 : 1;
    return x->start < y->start ? -1 : x->start > y->start;
}

static bool cui__ts_highlight(cui_tree_sitter *d, const char *source, int len, uint8_t *kinds) {
    cui__ts_capture *captures = NULL;
    size_t count = 0, capacity = 0;
    TSQueryMatch match;
    ts_query_cursor_exec(d->cursor, d->query, ts_tree_root_node(d->tree));
    while (ts_query_cursor_next_match(d->cursor, &match)) {
        if (!cui__ts_predicates(d, &match, source)) {
            if (d->error.message) goto fail;
            continue;
        }
        for (uint16_t i = 0; i < match.capture_count; i++) {
            TSQueryCapture capture = match.captures[i];
            uint8_t kind = d->capture_kinds[capture.index];
            if (kind == UINT8_MAX) continue;
            uint32_t start = ts_node_start_byte(capture.node), end = ts_node_end_byte(capture.node);
            if (start >= end || end > (uint32_t)len) continue;
            if (count == capacity) {
                size_t next = capacity ? capacity * 2 : 64;
                if (next < capacity || next > SIZE_MAX / sizeof *captures) { d->error.message = "too many highlight captures"; goto fail; }
                void *grown = realloc(captures, next * sizeof *captures);
                if (!grown) { d->error.message = "out of memory"; goto fail; }
                captures = grown; capacity = next;
            }
            captures[count++] = (cui__ts_capture){ start, end, match.pattern_index, kind, d->capture_depths[capture.index] };
        }
    }
    if (ts_query_cursor_did_exceed_match_limit(d->cursor)) { d->error.message = "highlight query exceeded its match limit"; goto fail; }
    if (count > 1) qsort(captures, count, sizeof *captures, cui__ts_capture_order);
    memset(kinds, CUI_SYNTAX_TEXT, (size_t)len);
    for (size_t i = 0; i < count; i++) memset(kinds + captures[i].start, captures[i].kind, captures[i].end - captures[i].start);
    free(captures);
    return true;
fail:
    free(captures);
    return false;
}

static TSPoint cui__ts_point(const char *source, int from, int to, TSPoint point) {
    for (int i = from; i < to; i++) {
        if (source[i] == '\n') { point.row++; point.column = 0; }
        else point.column++;
    }
    return point;
}

static const uint8_t *cui__ts_update(void *user, const char *source, int len) {
    cui_tree_sitter *d = user;
    if (!d) return NULL;
    if (!source || len < 0) { d->error.message = "invalid source buffer"; return NULL; }
    if (d->tree && d->source && d->len == len && !memcmp(d->source, source, (size_t)len) && !d->error.message) return d->kinds;
    d->error = (cui_tree_sitter_error){0};
    char *copy = malloc((size_t)len + 1);
    uint8_t *kinds = malloc((size_t)(len > 0 ? len : 1));
    if (!copy || !kinds) { free(copy); free(kinds); d->error.message = "out of memory"; return NULL; }
    memcpy(copy, source, (size_t)len); copy[len] = 0;
    if (d->tree && d->source) {
        int start = 0, old_end = d->len, new_end = len;
        while (start < old_end && start < new_end && d->source[start] == source[start]) start++;
        while (start > 0 && start < len && ((unsigned char)source[start] & 0xc0) == 0x80) start--;
        while (old_end > start && new_end > start && d->source[old_end - 1] == source[new_end - 1]) { old_end--; new_end--; }
        while (new_end < len && ((unsigned char)source[new_end] & 0xc0) == 0x80) { old_end++; new_end++; }
        TSPoint point = cui__ts_point(source, 0, start, (TSPoint){0});
        TSInputEdit edit = { (uint32_t)start, (uint32_t)old_end, (uint32_t)new_end, point,
                            cui__ts_point(d->source, start, old_end, point), cui__ts_point(source, start, new_end, point) };
        ts_tree_edit(d->tree, &edit);
    }
    TSTree *tree = ts_parser_parse_string(d->parser, d->tree, source, (uint32_t)len);
    ts_tree_delete(d->tree);
    d->tree = tree;
    if (!tree) { d->error.message = "Tree-sitter parsing failed"; goto fail; }
    if (!cui__ts_highlight(d, source, len, kinds)) {
        ts_tree_delete(d->tree); d->tree = NULL;
        goto fail;
    }
    free(d->source); free(d->kinds);
    d->source = copy; d->kinds = kinds; d->len = len;
    return kinds;
fail:
    free(copy); free(kinds);
    return NULL;
}
