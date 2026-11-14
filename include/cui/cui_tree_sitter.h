/* Optional syntax add-on. Build CUI_TREE_SITTER=ON and link cui::tree_sitter. */
#ifndef CUI_TREE_SITTER_H
#define CUI_TREE_SITTER_H
#include "cui.h"
/* The add-on is static, including when the core uses CUI_SHARED. */
#ifndef CUI_TREE_SITTER_API
#define CUI_TREE_SITTER_API
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef struct cui_tree_sitter cui_tree_sitter;
struct TSLanguage;
typedef struct cui_tree_sitter_error {
    const char *message;       /* NULL on success; static text, never freed by the caller */
    uint32_t offset;           /* query byte offset for creation errors, otherwise 0 */
} cui_tree_sitter_error;

/* One document per object, called on the UI thread. language is a generated
 * Tree-sitter grammar owned by the app and must outlive the object; query is
 * copied/compiled. No grammars are bundled with the library. NULL on failure.
 * Captures use keyword, type, string, number, comment, function, constant,
 * punctuation/operator, variable/property and dotted variants. Helper captures
 * beginning with '_' are ignored. Shorter captures override enclosing ones;
 * ties prefer dotted capture names, then later query patterns.
 * Supports #eq?, #not-eq?, #match?, #not-match?, #any-of?, #not-any-of?.
 * Predicates require single captures; regexes use cui_regex_search's syntax.
 * Unsupported predicates/directives are rejected, never silently ignored.
 * Language injections and locals queries are not processed. */
CUI_TREE_SITTER_API cui_tree_sitter *cui_tree_sitter_create(const struct TSLanguage *language, const char *query, cui_tree_sitter_error *error);
/* Borrowed until document destruction. Pass as .highlighter to the editor or
 * code block. NULL document returns NULL (the built-in lexer). */
CUI_TREE_SITTER_API const cui_code_highlighter *cui_tree_sitter_highlighter(cui_tree_sitter *document);
CUI_TREE_SITTER_API void cui_tree_sitter_destroy(cui_tree_sitter *document);
/* Last parse/highlight error; malformed source is allowed and is not itself
 * an error. Editor and code block fall back to .lang on backend failure.
 * Memory: source snapshot + one color-kind byte per source byte + parse tree.
 * Edits reuse the previous tree; capture colors are rebuilt after each edit.
 * Unchanged text reuses both. External buffer changes and undo are detected. */
CUI_TREE_SITTER_API cui_tree_sitter_error cui_tree_sitter_last_error(const cui_tree_sitter *document);

#ifdef __cplusplus
}
#endif
#endif
