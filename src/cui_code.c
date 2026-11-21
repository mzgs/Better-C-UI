/* Code block with syntax highlighting, and a JSON viewer. */
#include "cui_internal.h"


/* ------------------------------------------------------------------------ */
/* Tokenizer                                                                */
/* ------------------------------------------------------------------------ */

static const char *const kw_c[] = { "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue", "return", "goto",
                                    "typedef", "struct", "union", "enum", "static", "const", "extern", "volatile", "inline", "sizeof",
                                    "class", "public", "private", "protected", "namespace", "template", "typename", "new", "delete",
                                    "using", "virtual", "override", "auto", "constexpr", "nullptr", "this", "operator", "try", "catch", "throw", NULL };
static const char *const ty_c[] = { "void", "char", "short", "int", "long", "float", "double", "bool", "unsigned", "signed", "size_t",
                                    "uint8_t", "uint16_t", "uint32_t", "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t", "FILE", NULL };
static const char *const kw_js[] = { "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue", "return",
                                     "function", "const", "let", "var", "new", "class", "extends", "import", "export", "from", "async",
                                     "await", "try", "catch", "throw", "typeof", "instanceof", "in", "of", "this", "yield", "interface",
                                     "type", "implements", "public", "private", "readonly", "as", NULL };
static const char *const ty_js[] = { "string", "number", "boolean", "any", "unknown", "never", "void", "Promise", "Array", "Record", NULL };
static const char *const kw_py[] = { "if", "elif", "else", "for", "while", "def", "class", "return", "import", "from", "as", "with",
                                     "try", "except", "finally", "raise", "lambda", "yield", "pass", "break", "continue", "in", "is",
                                     "not", "and", "or", "global", "nonlocal", "async", "await", "del", "assert", NULL };
static const char *const ty_py[] = { "int", "float", "str", "bool", "list", "dict", "tuple", "set", "bytes", "self", NULL };
static const char *const kw_rs[] = { "fn", "let", "mut", "if", "else", "match", "for", "while", "loop", "return", "struct", "enum",
                                     "impl", "trait", "pub", "use", "mod", "crate", "self", "Self", "where", "as", "ref", "move",
                                     "async", "await", "const", "static", "unsafe", "dyn", "in", "break", "continue", NULL };
static const char *const ty_rs[] = { "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64", "usize", "isize", "bool",
                                     "char", "str", "String", "Vec", "Option", "Result", "Box", NULL };
static const char *const kw_go[] = { "func", "package", "import", "var", "const", "type", "struct", "interface", "map", "chan", "if",
                                     "else", "for", "range", "switch", "case", "default", "return", "break", "continue", "go", "defer",
                                     "select", "fallthrough", "goto", NULL };
static const char *const ty_go[] = { "int", "int64", "int32", "uint", "uint64", "float64", "float32", "string", "bool", "byte", "rune", "error", NULL };
static const char *const kw_sh[] = { "if", "then", "else", "elif", "fi", "for", "in", "do", "done", "while", "case", "esac", "function",
                                     "return", "export", "local", "echo", "cd", "sudo", "cmake", "make", "git", "apt", "brew", NULL };
static const char *const kw_java[] = {
    "abstract", "assert", "boolean", "break", "case", "catch", "class", "const", "continue", "default",
    "do", "else", "enum", "extends", "final", "finally", "for", "if", "implements", "import",
    "instanceof", "interface", "native", "new", "package", "private", "protected", "public", "record", "return",
    "sealed", "static", "strictfp", "super", "switch", "synchronized", "this", "throw", "throws", "transient",
    "try", "var", "void", "volatile", "while", "yield",
    NULL
};
static const char *const ty_java[] = {
    "byte", "char", "double", "float", "int", "long", "short", "String", "Object", "Integer",
    "Boolean", "List", "Map", "Set", "Optional",
    NULL
};
static const char *const kw_cs[] = {
    "abstract", "as", "async", "await", "base", "break", "case", "catch", "checked", "class",
    "const", "continue", "default", "delegate", "do", "else", "enum", "event", "explicit", "extern",
    "finally", "fixed", "for", "foreach", "goto", "if", "implicit", "in", "interface", "internal",
    "is", "lock", "namespace", "new", "operator", "out", "override", "params", "private", "protected",
    "public", "readonly", "record", "ref", "required", "return", "sealed", "sizeof", "stackalloc", "static",
    "struct", "switch", "this", "throw", "try", "typeof", "unchecked", "unsafe", "using", "var",
    "virtual", "volatile", "when", "where", "while", "yield",
    NULL
};
static const char *const ty_cs[] = {
    "bool", "byte", "char", "decimal", "double", "dynamic", "float", "int", "long", "object",
    "sbyte", "short", "string", "uint", "ulong", "ushort", "void", "Task", "List", "Dictionary",
    NULL
};
static const char *const kw_swift[] = {
    "actor", "associatedtype", "async", "await", "break", "case", "catch", "class", "continue", "convenience",
    "default", "defer", "deinit", "do", "else", "enum", "extension", "fallthrough", "fileprivate", "final",
    "for", "func", "guard", "if", "import", "in", "indirect", "infix", "init", "inout",
    "internal", "is", "lazy", "let", "mutating", "nonmutating", "open", "operator", "override", "private",
    "protocol", "public", "repeat", "required", "rethrows", "return", "self", "some", "static", "struct",
    "subscript", "super", "switch", "throws", "try", "typealias", "var", "weak", "where", "while",
    NULL
};
static const char *const ty_swift[] = {
    "Any", "AnyObject", "Bool", "Character", "Double", "Float", "Int", "String", "UInt", "Array",
    "Dictionary", "Optional", "Set", "Void",
    NULL
};
static const char *const kw_kotlin[] = {
    "abstract", "actual", "annotation", "as", "break", "by", "catch", "class", "companion", "const",
    "constructor", "continue", "crossinline", "data", "delegate", "do", "dynamic", "else", "enum", "expect",
    "external", "final", "finally", "for", "fun", "get", "if", "import", "in", "infix",
    "init", "inline", "inner", "interface", "internal", "is", "lateinit", "noinline", "object", "open",
    "operator", "out", "override", "package", "private", "protected", "public", "reified", "return", "sealed",
    "set", "super", "suspend", "tailrec", "this", "throw", "try", "typealias", "val", "var",
    "vararg", "when", "where", "while",
    NULL
};
static const char *const ty_kotlin[] = {
    "Any", "Boolean", "Byte", "Char", "Double", "Float", "Int", "Long", "Nothing", "Short",
    "String", "Unit", "Array", "List", "Map", "Set",
    NULL
};
static const char *const kw_php[] = {
    "abstract", "and", "array", "as", "break", "callable", "case", "catch", "class", "clone",
    "const", "continue", "declare", "default", "die", "do", "echo", "else", "elseif", "empty",
    "endfor", "endforeach", "endif", "endswitch", "endwhile", "enum", "eval", "exit", "extends", "final",
    "finally", "fn", "for", "foreach", "function", "global", "goto", "if", "implements", "include",
    "include_once", "instanceof", "interface", "isset", "list", "match", "namespace", "new", "or", "print",
    "private", "protected", "public", "readonly", "require", "require_once", "return", "static", "switch", "throw",
    "trait", "try", "unset", "use", "var", "while", "xor", "yield",
    NULL
};
static const char *const ty_php[] = {
    "bool", "float", "int", "iterable", "mixed", "never", "object", "self", "static", "string",
    "void",
    NULL
};
static const char *const kw_ruby[] = {
    "alias", "and", "begin", "break", "case", "class", "def", "defined", "do", "else",
    "elsif", "end", "ensure", "for", "if", "in", "module", "next", "not", "or",
    "redo", "rescue", "retry", "return", "self", "super", "then", "undef", "unless", "until",
    "when", "while", "yield",
    NULL
};
static const char *const ty_ruby[] = {
    "Array", "Class", "Float", "Hash", "Integer", "Module", "Object", "Range", "Regexp", "String",
    "Symbol",
    NULL
};
static const char *const kw_lua[] = {
    "and", "break", "do", "else", "elseif", "end", "for", "function", "goto", "if",
    "in", "local", "not", "or", "repeat", "return", "then", "until", "while",
    NULL
};
static const char *const kw_scala[] = {
    "abstract", "case", "catch", "class", "def", "derives", "do", "else", "end", "enum",
    "export", "extends", "extension", "false", "final", "finally", "for", "forSome", "given", "if",
    "implicit", "import", "infix", "inline", "lazy", "match", "new", "object", "opaque", "open",
    "override", "package", "private", "protected", "return", "sealed", "super", "then", "this", "throw",
    "trait", "transparent", "true", "try", "type", "using", "val", "var", "while", "with",
    "yield",
    NULL
};
static const char *const ty_scala[] = {
    "Any", "Boolean", "Byte", "Char", "Double", "Float", "Int", "Long", "Nothing", "Null",
    "Option", "Short", "String", "Unit", "List", "Map", "Seq", "Set",
    NULL
};
static const char *const kw_sql[] = {
    "add", "all", "alter", "and", "as", "asc", "begin", "between", "by", "case",
    "check", "column", "commit", "constraint", "create", "cross", "database", "default", "delete", "desc",
    "distinct", "drop", "else", "end", "except", "exists", "foreign", "from", "full", "group",
    "having", "in", "index", "inner", "insert", "intersect", "into", "is", "join", "key",
    "left", "like", "limit", "not", "null", "offset", "on", "or", "order", "outer",
    "primary", "procedure", "references", "right", "rollback", "select", "set", "table", "then", "top",
    "trigger", "truncate", "union", "unique", "update", "use", "values", "view", "when", "where",
    "with",
    NULL
};
static const char *const ty_sql[] = {
    "bigint", "binary", "bit", "blob", "boolean", "char", "date", "datetime", "decimal", "double",
    "float", "int", "integer", "interval", "json", "numeric", "real", "serial", "smallint", "text",
    "time", "timestamp", "varchar",
    NULL
};
static const char *const kw_css[] = {
    "important", "inherit", "initial", "unset", "revert", "none", "auto", "block", "flex", "grid",
    "inline", "relative", "absolute", "fixed", "sticky", "solid", "transparent",
    NULL
};
static const char *const kw_cmake[] = {
    "add_compile_definitions", "add_compile_options", "add_custom_command", "add_custom_target", "add_definitions", "add_dependencies", "add_executable", "add_library", "add_subdirectory", "break",
    "cmake_minimum_required", "configure_file", "continue", "else", "elseif", "enable_language", "enable_testing", "endblock", "endforeach", "endfunction",
    "endif", "endmacro", "endwhile", "execute_process", "file", "find_package", "find_path", "find_library", "foreach", "function",
    "get_filename_component", "if", "include", "install", "list", "macro", "message", "option", "project", "return",
    "set", "set_property", "string", "target_compile_definitions", "target_compile_features", "target_compile_options", "target_include_directories", "target_link_libraries", "target_sources", "unset",
    "while",
    NULL
};
static const char *const kw_make[] = {
    "define", "else", "endef", "endif", "export", "ifdef", "ifeq", "ifndef", "ifneq", "include",
    "override", "private", "sinclude", "undefine", "unexport", "vpath",
    NULL
};
static const char *const kw_elixir[] = {
    "after", "alias", "and", "case", "catch", "cond", "def", "defdelegate", "defexception", "defguard",
    "defimpl", "defmacro", "defmodule", "defp", "defprotocol", "defstruct", "do", "else", "end", "fn",
    "for", "if", "import", "in", "not", "or", "quote", "raise", "receive", "require",
    "rescue", "super", "throw", "try", "unless", "unquote", "use", "when", "with",
    NULL
};
static const char *const kw_zig[] = {
    "addrspace", "align", "allowzero", "and", "anyframe", "anytype", "asm", "async", "await", "break",
    "callconv", "catch", "comptime", "const", "continue", "defer", "else", "enum", "errdefer", "error",
    "export", "extern", "fn", "for", "if", "inline", "linksection", "noalias", "noinline", "nosuspend",
    "opaque", "or", "orelse", "packed", "pub", "resume", "return", "section", "struct", "suspend",
    "switch", "test", "threadlocal", "try", "union", "unreachable", "usingnamespace", "var", "volatile", "while",
    NULL
};
static const char *const ty_zig[] = {
    "anyerror", "bool", "c_char", "c_int", "c_long", "c_short", "c_uint", "f16", "f32", "f64",
    "f128", "i8", "i16", "i32", "i64", "isize", "noreturn", "type", "u8", "u16",
    "u32", "u64", "usize", "void",
    NULL
};
static const char *const kw_graphql[] = {
    "directive", "enum", "extend", "fragment", "implements", "input", "interface",
    "mutation", "on", "query", "repeatable", "scalar", "schema", "subscription", "type", "union", NULL
};
static const char *const ty_graphql[] = { "Boolean", "Float", "ID", "Int", "String", NULL };
static const char *const kw_jsdoc[] = {
    "abstract", "async", "author", "callback", "class", "constant", "constructor", "default",
    "deprecated", "description", "enum", "example", "exports", "extends", "external", "file",
    "function", "implements", "interface", "link", "member", "module", "namespace", "override",
    "param", "private", "property", "protected", "public", "readonly", "returns", "see", "since",
    "static", "template", "this", "throws", "todo", "type", "typedef", "version", "yields", NULL
};
static const char *const kw_proto[] = {
    "enum", "extend", "extensions", "group", "import", "map", "max", "message", "oneof",
    "option", "optional", "package", "public", "repeated", "required", "reserved", "returns",
    "rpc", "service", "stream", "syntax", "to", "weak", NULL
};
static const char *const ty_proto[] = {
    "bool", "bytes", "double", "fixed32", "fixed64", "float", "int32", "int64", "sfixed32",
    "sfixed64", "sint32", "sint64", "string", "uint32", "uint64", NULL
};
static const char *const none[] = { NULL };

static bool cui__syntax_equal(const char *a, const char *b, int n, bool fold) {
    for (int i = 0; i < n; i++) {
        if ((fold ? cui__ascii_lower(a[i]) : a[i]) != (fold ? cui__ascii_lower(b[i]) : b[i])) return false;
    }
    return true;
}

cui__lang cui__lang_for(const char *lang) {
    static const struct { const char *names; cui__lang rules; } languages[] = {
        { "json", { .no_caps_types = true } },
        { "astro", { .mode = CUI__LEX_MARKUP, .block_start = "<!--", .block_end = "-->", .no_caps_types = true, .hyphen_words = true } },
        { "bash|sh|shell|zsh", { .kw = kw_sh, .line_comment = "#", .no_caps_types = true } },
        { "c|h", { .kw = kw_c, .types = ty_c, .line_comment = "//", .block_start = "/*", .block_end = "*/", .hash_preproc = true } },
        { "cmake", { .kw = kw_cmake, .line_comment = "#", .block_start = "#[[", .block_end = "]]", .case_insensitive = true } },
        { "cpp|c++|cxx|cc|hpp", { .kw = kw_c, .types = ty_c, .line_comment = "//", .block_start = "/*", .block_end = "*/", .hash_preproc = true } },
        { "csharp|cs|c#", { .kw = kw_cs, .types = ty_cs, .line_comment = "//", .block_start = "/*", .block_end = "*/", .doubled_quotes = true } },
        { "css", { .kw = kw_css, .mode = CUI__LEX_CSS, .block_start = "/*", .block_end = "*/", .hyphen_words = true } },
        { "diff|patch", { .mode = CUI__LEX_DIFF } },
        { "ejs", { .mode = CUI__LEX_TEMPLATE, .block_start = "<!--", .block_end = "-->", .no_caps_types = true, .hyphen_words = true } },
        { "elixir|ex|exs", { .kw = kw_elixir, .line_comment = "#" } },
        { "erb|rhtml", { .mode = CUI__LEX_TEMPLATE, .block_start = "<!--", .block_end = "-->", .no_caps_types = true, .hyphen_words = true } },
        { "go|golang", { .kw = kw_go, .types = ty_go, .line_comment = "//", .block_start = "/*", .block_end = "*/", .backtick = true } },
        { "graphql", { .kw = kw_graphql, .types = ty_graphql, .line_comment = "#", .no_single_quotes = true } },
        { "html|htm", { .mode = CUI__LEX_MARKUP, .block_start = "<!--", .block_end = "-->", .no_caps_types = true, .hyphen_words = true } },
        { "java", { .kw = kw_java, .types = ty_java, .line_comment = "//", .block_start = "/*", .block_end = "*/" } },
        { "javascript|js|jsx", { .kw = kw_js, .types = ty_js, .line_comment = "//", .block_start = "/*", .block_end = "*/", .backtick = true } },
        { "jsdoc", { .kw = kw_jsdoc, .types = ty_js, .no_caps_types = true, .hyphen_words = true } },
        { "kotlin|kt|kts", { .kw = kw_kotlin, .types = ty_kotlin, .line_comment = "//", .block_start = "/*", .block_end = "*/" } },
        { "lua", { .kw = kw_lua, .line_comment = "--", .block_start = "--[[", .block_end = "]]" } },
        { "make|makefile", { .mode = CUI__LEX_CONFIG, .kw = kw_make, .line_comment = "#", .no_caps_types = true, .hyphen_words = true } },
        { "markdown|md", { .mode = CUI__LEX_MARKDOWN, .backtick = true, .no_caps_types = true } },
        { "markdown_inline|md_inline", { .mode = CUI__LEX_MARKDOWN_INLINE, .backtick = true, .no_caps_types = true } },
        { "php", { .kw = kw_php, .types = ty_php, .line_comment = "//", .block_start = "/*", .block_end = "*/", .line_comment2 = "#", .backtick = true } },
        { "proto|protobuf", { .kw = kw_proto, .types = ty_proto, .line_comment = "//", .block_start = "/*", .block_end = "*/" } },
        { "python|py", { .kw = kw_py, .types = ty_py, .line_comment = "#" } },
        { "ruby|rb", { .kw = kw_ruby, .types = ty_ruby, .line_comment = "#", .block_start = "=begin", .block_end = "=end", .backtick = true } },
        { "rust|rs", { .kw = kw_rs, .types = ty_rs, .line_comment = "//", .block_start = "/*", .block_end = "*/", .char_quotes = true } },
        { "scala|sc", { .kw = kw_scala, .types = ty_scala, .line_comment = "//", .block_start = "/*", .block_end = "*/" } },
        { "sql", { .kw = kw_sql, .types = ty_sql, .line_comment = "--", .block_start = "/*", .block_end = "*/", .case_insensitive = true, .doubled_quotes = true, .literal_strings = true } },
        { "svelte", { .mode = CUI__LEX_MARKUP, .block_start = "<!--", .block_end = "-->", .no_caps_types = true, .hyphen_words = true } },
        { "swift", { .kw = kw_swift, .types = ty_swift, .line_comment = "//", .block_start = "/*", .block_end = "*/" } },
        { "toml", { .mode = CUI__LEX_CONFIG, .line_comment = "#", .no_caps_types = true, .hyphen_words = true } },
        { "tsx", { .kw = kw_js, .types = ty_js, .line_comment = "//", .block_start = "/*", .block_end = "*/", .backtick = true } },
        { "typescript|ts", { .kw = kw_js, .types = ty_js, .line_comment = "//", .block_start = "/*", .block_end = "*/", .backtick = true } },
        { "yaml|yml", { .doubled_quotes = true, .mode = CUI__LEX_CONFIG, .line_comment = "#", .no_caps_types = true, .hyphen_words = true } },
        { "zig", { .kw = kw_zig, .types = ty_zig, .line_comment = "//" } },
    };
    cui__lang l;
    memset(&l, 0, sizeof l);
    l.kw = l.types = none;
    if (!lang) return l;
    if (!strcmp(lang, "text") || !strcmp(lang, "plain")) { l.plain = true; return l; }
    size_t n = strlen(lang);
    for (int i = 0; i < CUI_COUNTOF(languages); i++) {
        const char *a = languages[i].names;
        while (*a) {
            const char *end = strchr(a, '|');
            size_t len = end ? (size_t)(end - a) : strlen(a);
            if (len == n && !strncmp(a, lang, n)) return languages[i].rules;
            if (!end) break;
            a = end + 1;
        }
    }
    return l;
}

static bool cui__syntax_word(const char *const *list, const char *s, int n, bool fold) {
    for (int k = 0; list && list[k]; k++)
        if ((int)strlen(list[k]) == n && cui__syntax_equal(list[k], s, n, fold)) return true;
    return false;
}

static bool cui__syntax_at(const char *s, int n, int i, const char *delimiter, bool fold) {
    if (!delimiter) return false;
    size_t len = strlen(delimiter);
    if (len && delimiter[len - 1] == ' ') {   /* REM is a word, followed by whitespace or the line end. */
        len--;
        if (len > (size_t)(n - i) || !cui__syntax_equal(s + i, delimiter, (int)len, fold)) return false;
        return len == (size_t)(n - i) || s[i + len] == ' ' || s[i + len] == '\t';
    }
    return len <= (size_t)(n - i) && cui__syntax_equal(s + i, delimiter, (int)len, fold);
}

static int cui__syntax_string_end(const cui__lang *L, const char *s, int n, int i) {
    char quote = s[i++], escape = L->string_escape ? L->string_escape : '\\';
    while (i < n) {
        if (s[i] == quote) {
            i++;
            if (!L->doubled_quotes || i == n || s[i] != quote) return i;
        } else if (!L->literal_strings && s[i] == escape && i + 1 < n) i++;
        i++;
    }
    return i;
}

static bool cui__ident_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (unsigned char)c >= 0x80; }
static bool cui__ident_char(char c) { return cui__ident_start(c) || (c >= '0' && c <= '9'); }

static bool cui__syntax_quote(const cui__lang *L, const char *s, int n, int i) {
    if (L->mode == CUI__LEX_MARKDOWN || L->mode == CUI__LEX_MARKDOWN_INLINE) return s[i] == '`';
    if (s[i] == '"' || (s[i] == '`' && L->backtick)) return true;
    if (s[i] != '\'' || L->no_single_quotes) return false;
    if (!L->char_quotes) return true;
    int end = i + 1;
    if (end < n && s[end] == '\\') end += 2;
    else if (end < n) end = cui__utf8_next(s, n, end);
    return end < n && s[end] == '\'';
}

/* shortcut: lexical rules only, use a parser if nested comments, multiline
 * strings, interpolation or embedded languages need exact highlighting. */
int cui__tokenize(const cui__lang *L, bool json, const char *s, int n, bool *in_block, cui__tok *out, int cap) {
    int count = 0, i = 0;
#define EMIT(st, ln, k) do { if (count < cap) { out[count].start = (st); out[count].len = (ln); out[count].kind = (uint8_t)(k); count++; } } while (0)
    if (L->plain) { if (n > 0) EMIT(0, n, CUI__TK_TEXT); return count; }
    if (L->mode == CUI__LEX_DIFF) {
        if (n > 0) EMIT(0, n, s[0] == '+' ? CUI__TK_STRING : s[0] == '-' ? CUI__TK_PREPROC : s[0] == '@' ? CUI__TK_KEYWORD : CUI__TK_TEXT);
        return count;
    }
    bool markup = L->mode == CUI__LEX_MARKUP || L->mode == CUI__LEX_TEMPLATE;
    bool markdown = L->mode == CUI__LEX_MARKDOWN || L->mode == CUI__LEX_MARKDOWN_INLINE;
    int first = 0;
    while (first < n && (s[first] == ' ' || s[first] == '\t')) first++;
    if (L->mode == CUI__LEX_MARKDOWN && first < n && (s[first] == '#' || s[first] == '>')) {
        EMIT(0, n, CUI__TK_KEYWORD);
        return count;
    }
    while (i < n) {
        if (*in_block && L->block_end) {
            int st = i;
            while (i < n && !cui__syntax_at(s, n, i, L->block_end, false)) i++;
            if (i < n) { i += (int)strlen(L->block_end); *in_block = false; }
            EMIT(st, i - st, CUI__TK_COMMENT);
            continue;
        }
        char c = s[i];
        if (c == ' ' || c == '\t') { int st = i; while (i < n && s[i] == c) i++; EMIT(st, i - st, CUI__TK_TEXT); continue; }
        if (cui__syntax_at(s, n, i, L->block_start, false)) {
            int st = i;
            i += (int)strlen(L->block_start);
            *in_block = true;
            while (i < n && !cui__syntax_at(s, n, i, L->block_end, false)) i++;
            if (i < n) { i += (int)strlen(L->block_end); *in_block = false; }
            EMIT(st, i - st, CUI__TK_COMMENT);
            continue;
        }
        if (cui__syntax_at(s, n, i, L->line_comment, L->case_insensitive) ||
            cui__syntax_at(s, n, i, L->line_comment2, L->case_insensitive)) { EMIT(i, n - i, CUI__TK_COMMENT); break; }
        if (L->mode == CUI__LEX_TEMPLATE && cui__syntax_at(s, n, i, "<%", false)) {
            int st = i;
            bool comment = i + 2 < n && s[i + 2] == '#';
            i += 2;
            while (i < n && !cui__syntax_at(s, n, i, "%>", false)) {
                if (!comment && (s[i] == '\'' || s[i] == '"')) i = cui__syntax_string_end(L, s, n, i);
                else i++;
            }
            if (i < n) i += 2;
            EMIT(st, i - st, comment ? CUI__TK_COMMENT : CUI__TK_PREPROC);
            continue;
        }
        if (L->mode == CUI__LEX_CONFIG && i == first && c == '[') {
            int st = i++;
            while (i < n && s[i] != ']') i++;
            if (i < n) i++;
            if (i < n && s[i] == ']') i++;
            EMIT(st, i - st, CUI__TK_TYPE);
            continue;
        }
        if (L->hash_preproc && c == '#') {
            int st = i++;
            while (i < n && cui__ident_char(s[i])) i++;
            EMIT(st, i - st, CUI__TK_PREPROC);
            continue;
        }
        if (cui__syntax_quote(L, s, n, i)) {
            int st = i;
            i = cui__syntax_string_end(L, s, n, i);
            int k = i;
            while (k < n && (s[k] == ' ' || s[k] == '\t')) k++;
            EMIT(st, i - st, (json || L->mode == CUI__LEX_CONFIG) && k < n && (s[k] == ':' || (L->mode == CUI__LEX_CONFIG && s[k] == '=')) ? CUI__TK_FUNC : CUI__TK_STRING);
            continue;
        }
        if (!markup && !markdown &&
            ((c >= '0' && c <= '9') || (c == '-' && json && i + 1 < n && s[i + 1] >= '0' && s[i + 1] <= '9'))) {
            int st = i++;
            while (i < n && (cui__ident_char(s[i]) || s[i] == '.' ||
                   ((s[i] == '+' || s[i] == '-') && (s[i - 1] == 'e' || s[i - 1] == 'E')))) i++;
            EMIT(st, i - st, CUI__TK_NUMBER);
            continue;
        }
        if (cui__ident_start(c) || (L->hyphen_words && c == '-' && i + 1 < n && cui__ident_start(s[i + 1]))) {
            int st = i++;
            while (i < n && (cui__ident_char(s[i]) || (L->hyphen_words && s[i] == '-'))) i++;
            if (cap <= 0) continue;
            int len = i - st, kind = CUI__TK_TEXT, k = i;
            while (k < n && (s[k] == ' ' || s[k] == '\t')) k++;
            if (markup) {
                if (st > 0 && (s[st - 1] == '<' || (st > 1 && s[st - 1] == '/' && s[st - 2] == '<'))) kind = CUI__TK_KEYWORD;
                else if (k < n && s[k] == '=') kind = CUI__TK_FUNC;
            } else if (markdown) kind = CUI__TK_TEXT;
            else if ((L->mode == CUI__LEX_CONFIG || L->mode == CUI__LEX_CSS) && k < n && (s[k] == ':' || s[k] == '=')) kind = CUI__TK_FUNC;
            else if (json) kind = CUI__TK_CONST;
            else if (cui__syntax_word(L->kw, s + st, len, L->case_insensitive)) kind = CUI__TK_KEYWORD;
            else if (cui__syntax_word(L->types, s + st, len, L->case_insensitive)) kind = CUI__TK_TYPE;
            else {
                static const char *const constants[] = { "true", "false", "NULL", "None", "null", "True", "False", "nil", "undef", "nothing", "NaN", "Inf", NULL };
                if (cui__syntax_word(constants, s + st, len, L->case_insensitive)) kind = CUI__TK_CONST;
                else if (L->mode == CUI__LEX_CSS) kind = CUI__TK_TYPE;
                else if (k < n && s[k] == '(') kind = CUI__TK_FUNC;
                else if (!L->no_caps_types && s[st] >= 'A' && s[st] <= 'Z' && len > 1) kind = CUI__TK_TYPE;
            }
            EMIT(st, len, kind);
            continue;
        }
        EMIT(i, 1, CUI__TK_PUNCT);
        i++;
    }
#undef EMIT
    return count;
}

bool cui__syntax_block_state(const cui__lang *L, const char *s, int n, bool in_block) {
    if (!L->block_start || (!in_block && !memchr(s, L->block_start[0], (size_t)n))) return false;
    cui__tokenize(L, false, s, n, &in_block, NULL, 0);
    return in_block;
}

const uint8_t *cui__syntax_snapshot(cui_ctx *ui, const cui_code_highlighter *highlighter, const char *source, int len, int from, int to) {
    if (highlighter && highlighter->update) {
        const uint8_t *kinds = highlighter->update(highlighter->user, source, len);
        if (kinds) {
            for (int i = from; i < to; i++) if (kinds[i] >= CUI_SYNTAX_COUNT) return NULL;
            uint8_t *copy = cui__frame_alloc(ui, (size_t)CUI_MAX(1, to - from));
            memcpy(copy, kinds + from, (size_t)(to - from));
            return copy;
        }
    }
    return NULL;
}

/* Keep whitespace runs separate: both renderers expand tabs themselves. */
int cui__syntax_tokens(const uint8_t *kinds, const char *text, int n, cui__tok *out, int cap) {
    int i = 0, count = 0;
    while (i < n && count < cap) {
        int start = i++;
        while (i < n && kinds[i] == kinds[start] &&
               (text[start] == '\t' ? text[i] == '\t' : text[start] == ' ' ? text[i] == ' ' : text[i] != '\t' && text[i] != ' ')) i++;
        out[count].start = start;
        out[count].len = i - start;
        out[count++].kind = kinds[start];
    }
    return count;
}

void cui__syntax_colors(cui_ctx *ui, cui_color *c) {
    bool dark = ui->mode == CUI_THEME_DARK;
    c[CUI__TK_TEXT] = CUI_C(ui, FOREGROUND);
    c[CUI__TK_KEYWORD] = dark ? cui_oklch(0.76f, 0.14f, 305, 1) : cui_oklch(0.52f, 0.20f, 305, 1);
    c[CUI__TK_TYPE] = dark ? cui_oklch(0.80f, 0.11f, 195, 1) : cui_oklch(0.52f, 0.11f, 200, 1);
    c[CUI__TK_STRING] = dark ? cui_oklch(0.80f, 0.13f, 145, 1) : cui_oklch(0.52f, 0.14f, 150, 1);
    c[CUI__TK_NUMBER] = dark ? cui_oklch(0.80f, 0.13f, 60, 1) : cui_oklch(0.58f, 0.16f, 50, 1);
    c[CUI__TK_COMMENT] = CUI_C(ui, TEXT_MUTED);
    c[CUI__TK_FUNC] = dark ? cui_oklch(0.78f, 0.12f, 250, 1) : cui_oklch(0.50f, 0.17f, 255, 1);
    c[CUI__TK_PREPROC] = dark ? cui_oklch(0.76f, 0.12f, 350, 1) : cui_oklch(0.52f, 0.18f, 355, 1);
    c[CUI__TK_PUNCT] = CUI_C(ui, TEXT_SECONDARY);
    c[CUI__TK_CONST] = dark ? cui_oklch(0.80f, 0.13f, 60, 1) : cui_oklch(0.58f, 0.16f, 50, 1);
}

/* the highlighter by language name, for other components (the diff viewer) */
int cui__tokenize_lang(const char *lang, const char *s, int n, bool *in_block, cui__tok *out, int cap) {
    bool json = lang && !strcmp(lang, "json");
    cui__lang L = cui__lang_for(json ? NULL : lang);
    return cui__tokenize(&L, json, s, n, in_block, out, cap < 256 ? cap : 256);
}

float cui__draw_code_line(cui_ctx *ui, const char *lang, const char *s, int n, bool *in_block, float x, float y, float size, const cui_color *colors, float max_x) {
    if (!lang) {
        cui__draw_text_line(ui, CUI_FONT_MONO, size, x, y, colors[CUI__TK_TEXT], s, n, true);
        return x + cui__text_width(ui, CUI_FONT_MONO, size, s, n, true);
    }
    cui__tok tk[256];
    int nt = cui__tokenize_lang(lang, s, n, in_block, tk, 256), pos = 0;
    for (int t = 0; t < nt && x <= max_x; t++) {
        if (tk[t].start > pos) x += cui__text_width(ui, CUI_FONT_MONO, size, s + pos, tk[t].start - pos, true);
        cui__draw_text_line(ui, CUI_FONT_MONO, size, x, y, colors[tk[t].kind], s + tk[t].start, tk[t].len, true);
        x += cui__text_width(ui, CUI_FONT_MONO, size, s + tk[t].start, tk[t].len, true);
        pos = tk[t].start + tk[t].len;
    }
    if (pos < n && x <= max_x) {
        cui__draw_text_line(ui, CUI_FONT_MONO, size, x, y, colors[CUI__TK_TEXT], s + pos, n - pos, true);
        x += cui__text_width(ui, CUI_FONT_MONO, size, s + pos, n - pos, true);
    }
    return x;
}

/* ------------------------------------------------------------------------ */
/* Code block                                                               */
/* ------------------------------------------------------------------------ */

typedef struct {
    const char *code;
    const uint8_t *syntax;
    int len;
    cui__lang lang;
    bool json;
    bool line_numbers;
    int hl_from, hl_to;
    float size, lh, gutter, pad;
    cui__selection *selection;
    int selection_offset;
    cui_color colors[CUI__TK_COUNT], gutter_c, hl_c;
} cui__code_paint;

static void cui__paint_code(cui_ctx *ui, cui_node *n, void *data) {
    cui__code_paint *p = data;
    cui_rect r = n->rect;
    float fh = cui__font_line_height(ui, CUI_FONT_MONO, p->size);
    float x0 = r.x + p->pad + p->gutter, y = r.y + p->pad;
    bool block = false;
    int line = 1, i = 0;
    cui__tok fixed_tokens[256], *toks = fixed_tokens;
    int tok_cap = CUI_COUNTOF(fixed_tokens);
    while (i <= p->len) {
        if (i == p->len && p->len > 0 && p->code[p->len - 1] == '\n') break;   /* no phantom line after a final newline */
        int e = i;
        while (e < p->len && p->code[e] != '\n') e++;
        if (line >= p->hl_from && line <= p->hl_to && p->hl_from > 0)
            cui_draw_rect(ui, (cui_rect){ r.x, y - 1, r.w, p->lh }, CUI_CORNERS(0), p->hl_c);
        float ty = y + (p->lh - fh) * 0.5f - 1;
        if (p->line_numbers) {
            const char *ln = cui__frame_printf(ui, "%d", line);
            float w = cui__text_width(ui, CUI_FONT_MONO, p->size, ln, -1, true);
            cui__draw_text_line(ui, CUI_FONT_MONO, p->size, r.x + p->pad + p->gutter - 16 - w, ty, p->gutter_c, ln, -1, true);
        }
        if (p->syntax && e - i + 1 > tok_cap) {
            tok_cap = e - i + 1;
            toks = cui__frame_alloc(ui, sizeof *toks * (size_t)tok_cap);
        }
        int nt = p->syntax ? cui__syntax_tokens(p->syntax + i, p->code + i, e - i, toks, tok_cap) :
                            cui__tokenize(&p->lang, p->json, p->code + i, e - i, &block, toks, 256);
        float x = x0;
        int pos = 0;
        float tw = cui__text_width(ui, CUI_FONT_MONO, p->size, "    ", 4, true);
        for (int k = 0; k < nt; k++) {
            const char *s = p->code + i + toks[k].start;
            int len = toks[k].len;
            pos = toks[k].start + len;
            int off = 0;
            while (off < len) {
                /* tabs become four spaces, even inside whole-line tokens (plain, diff, block comments) */
                if (s[off] == '\t') {
                    cui__selection_line(ui, p->selection, p->selection_offset + i + toks[k].start + off,
                        CUI_FONT_MONO, p->size, s + off, 1, (cui_rect){ x, y - 1, tw, p->lh }, true, 0);
                    x += tw;
                    off++;
                    continue;
                }
                int seg = off;
                bool blank = true;
                while (off < len && s[off] != '\t') {
                    if (s[off] != ' ') blank = false;
                    off++;
                }
                int slen = off - seg;
                float sw = cui__text_width(ui, CUI_FONT_MONO, p->size, s + seg, slen, true);
                cui__selection_line(ui, p->selection, p->selection_offset + i + toks[k].start + seg,
                    CUI_FONT_MONO, p->size, s + seg, slen, (cui_rect){ x, y - 1, sw, p->lh }, true, 0);
                if (!blank)
                    cui__draw_text_line(ui, CUI_FONT_MONO, p->size, x, ty, p->colors[toks[k].kind], s + seg, slen, true);
                x += sw;
            }
        }
        if (pos < e - i) {
            const char *s = p->code + i + pos;
            int rem = (e - i) - pos;
            float rw = cui__text_width(ui, CUI_FONT_MONO, p->size, s, rem, true);
            cui__selection_line(ui, p->selection, p->selection_offset + i + pos,
                CUI_FONT_MONO, p->size, s, rem, (cui_rect){ x, y - 1, rw, p->lh }, true, 0);
            cui__draw_text_line(ui, CUI_FONT_MONO, p->size, x, ty, p->colors[CUI__TK_TEXT], s, rem, true);
        }
        y += p->lh;
        line++;
        if (e >= p->len) break;
        i = e + 1;
    }
}

static void cui__code_block_ex_body(cui_ctx *ui, const char *id_str, const char *code, const cui_code_opts *o);

/* left to right in any interface: see cui__keep_ltr */
void cui_code_block_ex(cui_ctx *ui, const char *id_str, const char *code, const cui_code_opts *o) {
    int first = ui->node_count;
    cui__code_block_ex_body(ui, id_str, code, o);
    cui__keep_ltr(ui, first);
}

static void cui__code_block_ex_body(cui_ctx *ui, const char *id_str, const char *code, const cui_code_opts *o) {
    cui_code_opts d;
    memset(&d, 0, sizeof d);
    if (!o) o = &d;
    if (!code) code = "";
    cui_id id = cui_make_id(ui, id_str);
    float size = cui__fs(ui, o->size > 0 ? o->size : 13);
    float lh = size * 1.6f;
    int len = (int)strlen(code);
    int lines = 1;
    float maxw = 0;
    {
        int i = 0;
        while (i <= len) {
            int e = i;
            while (e < len && code[e] != '\n') e++;
            int tabs = 0;
            for (int k = i; k < e; k++) if (code[k] == '\t') tabs++;
            float w = cui__text_width(ui, CUI_FONT_MONO, size, code + i, e - i, true) + tabs * cui__text_width(ui, CUI_FONT_MONO, size, "   ", 3, true);
            if (w > maxw) maxw = w;
            if (e >= len) break;
            i = e + 1;
            lines++;
        }
        if (len > 0 && code[len - 1] == '\n') lines--;
    }
    float pad = 16;
    float gutter = o->line_numbers ? 16 + cui__text_width(ui, CUI_FONT_MONO, size, "000", lines >= 100 ? 3 : 2, true) + 4 : 0;
    float content_h = lines * lh + pad * 2;
    int outer = cui__node_new(ui, id);
    cui_node *on = &ui->nodes[outer];
    on->w = CUI_FILL;
    on->radius = CUI_CORNERS(ui->theme.radius_control);
    on->bg = ui->mode == CUI_THEME_DARK ? cui_mix(CUI_C(ui, SURFACE), CUI_C(ui, BACKGROUND), 0.4f) : CUI_C(ui, SURFACE_MUTED);
    on->border = 1; on->border_color = CUI_C(ui, BORDER);
    on->flags |= CUI_NF_CLIP;   /* the outline is drawn over the title bar */
    cui__open(ui, outer);
    if ((o->title || o->lang) && !o->no_header) {
        int head = cui__node_new(ui, 0);
        cui_node *hn = &ui->nodes[head];
        hn->dir = CUI_ROW; hn->align = CUI_ALIGN_CENTER; hn->gap = 8;
        hn->w = CUI_FILL; hn->h = CUI_PX(40);
        hn->pad[1] = 6; hn->pad[3] = 14;
        hn->bg = CUI_C(ui, SURFACE);
        hn->radius = (cui_corners){ ui->theme.radius_control, ui->theme.radius_control, 0, 0 };
        cui__open(ui, head);
        cui__icon_node(ui, o->title ? CUI_ICON_FILE_TEXT : CUI_ICON_CODE, 14, CUI_C(ui, TEXT_MUTED));
        int t = cui__text_node(ui, o->title ? o->title : o->lang, -1, CUI_FONT_MEDIUM, cui__fs(ui, ui->theme.text_sm), CUI_C(ui, TEXT_SECONDARY), false);
        ui->nodes[t].line_height = 1.3f;
        ui->nodes[t].w = CUI_FILL;
        if (o->title && o->lang) {
            cui_badge_opts bo;
            memset(&bo, 0, sizeof bo);
            bo.size = CUI_SIZE_SM;
            cui_badge_ex(ui, o->lang, &bo);
        }
        cui_push_id(ui, id_str);
        cui_copy_button(ui, "copy", code);
        cui_pop_id(ui);
        cui__close(ui);
        int line = cui__leaf(ui, 0, CUI_FILL, CUI_PX(1));
        ui->nodes[line].bg = CUI_C(ui, BORDER);
    }
    cui_box sb;
    memset(&sb, 0, sizeof sb);
    sb.width = CUI_FILL;
    if (o->max_height > 0 && content_h > o->max_height) sb.height = CUI_PX(o->max_height);
    cui_scroll_state *ss = cui_state(ui, id ^ 0x5C0Du, sizeof *ss);
    ss->fade = cui__backdrop(ui);   /* long lines fade out at the edge */
    cui__scroll_begin_ex(ui, id ^ 0x5C0Du, &sb, true);
    if (!(o->max_height > 0 && content_h > o->max_height)) { ui->nodes[cui__current(ui)].h = CUI_FIT; ui->nodes[cui__current(ui)].min_h = 0; }
    int canvas = cui__leaf(ui, 0, CUI_FILL, CUI_PX(content_h));
    cui_node *cn = &ui->nodes[canvas];
    cn->min_w = maxw + gutter + pad * 2 + 8;   /* at least as wide as the code; the band spans the block */
    cui__code_paint *p = cui__frame_alloc(ui, sizeof *p);
    p->code = cui__frame_strdup(ui, code, len);
    p->len = len;
    p->syntax = cui__syntax_snapshot(ui, o->highlighter, code, len, 0, len);
    p->selection = ui->building.selection;
    p->selection_offset = cui__selection_append(ui, p->selection, code, len);
    p->json = o->lang && !strcmp(o->lang, "json");
    p->lang = cui__lang_for(p->json ? NULL : o->lang);
    p->line_numbers = o->line_numbers;
    p->hl_from = o->highlight_from; p->hl_to = o->highlight_to > 0 ? o->highlight_to : o->highlight_from;
    p->size = size; p->lh = lh; p->gutter = gutter; p->pad = pad;
    cui__syntax_colors(ui, p->colors);
    p->gutter_c = cui_alpha(CUI_C(ui, TEXT_MUTED), 0.7f);
    p->hl_c = cui_alpha(cui__emphasis(ui), ui->mode == CUI_THEME_DARK ? 0.12f : 0.08f);
    cn->paint = cui__paint_code;
    cn->paint_data = p;
    cui__scroll_end_ex(ui);
    cui__close(ui);
    ui->last_id = id;
}

/* ------------------------------------------------------------------------ */
/* JSON                                                                     */
/* ------------------------------------------------------------------------ */

enum { JV_NULL, JV_BOOL, JV_NUM, JV_STR, JV_ARR, JV_OBJ };
typedef struct { uint8_t type; int key_start, key_len; int val_start, val_len; int parent, first, next, count, depth; } cui__jnode;

typedef struct { const char *s; int n, i; cui__jnode *nodes; int count, cap; const char *err; int err_at; } cui__jparse;

static void cui__jws(cui__jparse *p) { while (p->i < p->n && (p->s[p->i] == ' ' || p->s[p->i] == '\t' || p->s[p->i] == '\n' || p->s[p->i] == '\r')) p->i++; }
static bool cui__jfail(cui__jparse *p, const char *msg) { if (!p->err) { p->err = msg; p->err_at = p->i; } return false; }

static bool cui__jstring(cui__jparse *p, int *start, int *len) {
    if (p->i >= p->n || p->s[p->i] != '"') return cui__jfail(p, "expected a string");
    int st = ++p->i;
    while (p->i < p->n && p->s[p->i] != '"') {
        if (p->s[p->i] == '\\') {
            if (++p->i >= p->n) return cui__jfail(p, "unterminated escape");
            if (p->s[p->i] == 'u') {
                for (int k = 0; k < 4; k++)
                    if (++p->i >= p->n || cui__hex_digit((unsigned char)p->s[p->i]) < 0)
                        return cui__jfail(p, "expected four hex digits");
            } else if (!strchr("\"\\/bfnrt", p->s[p->i])) return cui__jfail(p, "invalid escape");
        } else if ((unsigned char)p->s[p->i] < 0x20) return cui__jfail(p, "control character in string");
        p->i++;
    }
    if (p->i >= p->n) return cui__jfail(p, "unterminated string");
    *start = st; *len = p->i - st;
    p->i++;
    return true;
}

static int cui__jvalue(cui__jparse *p, int parent, int depth, int key_start, int key_len);

static int cui__jnew(cui__jparse *p, int parent, int depth, int key_start, int key_len) {
    if (p->count >= p->cap) { cui__jfail(p, "too large"); return -1; }
    cui__jnode *nd = &p->nodes[p->count];
    memset(nd, 0, sizeof *nd);
    nd->parent = parent; nd->first = -1; nd->next = -1; nd->depth = depth;
    nd->key_start = key_start; nd->key_len = key_len;
    return p->count++;
}

static int cui__jvalue(cui__jparse *p, int parent, int depth, int key_start, int key_len) {
    if (depth > 64) { cui__jfail(p, "nested too deeply"); return -1; }
    cui__jws(p);
    if (p->i >= p->n) { cui__jfail(p, "unexpected end"); return -1; }
    int me = cui__jnew(p, parent, depth, key_start, key_len);
    if (me < 0) return -1;
    char c = p->s[p->i];
    int st = p->i;
    if (c == '{' || c == '[') {
        bool obj = c == '{';
        p->nodes[me].type = obj ? JV_OBJ : JV_ARR;
        p->i++;
        cui__jws(p);
        int last = -1;
        if (p->i < p->n && p->s[p->i] == (obj ? '}' : ']')) { p->i++; return me; }
        for (;;) {
            int ks = 0, kl = -1;
            cui__jws(p);
            if (obj) {
                if (!cui__jstring(p, &ks, &kl)) return -1;
                cui__jws(p);
                if (p->i >= p->n || p->s[p->i] != ':') { cui__jfail(p, "expected ':'"); return -1; }
                p->i++;
            }
            int child = cui__jvalue(p, me, depth + 1, ks, kl);
            if (child < 0) return -1;
            if (last < 0) p->nodes[me].first = child; else p->nodes[last].next = child;
            last = child;
            p->nodes[me].count++;
            cui__jws(p);
            if (p->i < p->n && p->s[p->i] == ',') { p->i++; continue; }
            if (p->i < p->n && p->s[p->i] == (obj ? '}' : ']')) { p->i++; break; }
            cui__jfail(p, obj ? "expected ',' or '}'" : "expected ',' or ']'");
            return -1;
        }
        return me;
    }
    if (c == '"') {
        int s0, l0;
        if (!cui__jstring(p, &s0, &l0)) return -1;
        p->nodes[me].type = JV_STR;
        p->nodes[me].val_start = s0 - 1; p->nodes[me].val_len = l0 + 2;
        return me;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
        if (c == '-') p->i++;
        if (p->i >= p->n || p->s[p->i] < '0' || p->s[p->i] > '9') { cui__jfail(p, "expected a digit"); return -1; }
        if (p->s[p->i] == '0') p->i++;
        else while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
        if (p->i < p->n && p->s[p->i] == '.') {
            int digits = ++p->i;
            while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
            if (p->i == digits) { cui__jfail(p, "expected a fractional digit"); return -1; }
        }
        if (p->i < p->n && (p->s[p->i] == 'e' || p->s[p->i] == 'E')) {
            p->i++;
            if (p->i < p->n && (p->s[p->i] == '+' || p->s[p->i] == '-')) p->i++;
            int digits = p->i;
            while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
            if (p->i == digits) { cui__jfail(p, "expected an exponent digit"); return -1; }
        }
        p->nodes[me].type = JV_NUM;
    } else if (!strncmp(p->s + p->i, "true", 4) || !strncmp(p->s + p->i, "null", 4)) {
        p->nodes[me].type = p->s[p->i] == 't' ? JV_BOOL : JV_NULL;
        p->i += 4;
    } else if (!strncmp(p->s + p->i, "false", 5)) {
        p->nodes[me].type = JV_BOOL;
        p->i += 5;
    } else {
        cui__jfail(p, "unexpected character");
        return -1;
    }
    p->nodes[me].val_start = st; p->nodes[me].val_len = p->i - st;
    return me;
}

typedef struct { uint8_t collapsed[512]; bool init; } cui__json_state;   /* by node index, bit per node */

static void cui__json_rows(cui_ctx *ui, cui_id id, cui__jparse *p, cui__json_state *st, int node, const cui_color *col, bool last) {
    cui__jnode *nd = &p->nodes[node];
    bool container = nd->type == JV_ARR || nd->type == JV_OBJ;
    bool collapsed = node < 4096 && (st->collapsed[node >> 3] >> (node & 7) & 1);
    cui_id rid = cui__hash(&node, sizeof node, id);
    cui_interact r = cui__interact(ui, rid, 0);
    if (r.clicked && container && node < 4096) { st->collapsed[node >> 3] ^= (uint8_t)(1 << (node & 7)); collapsed = !collapsed; }
    float hv_v = cui__hover_fade(ui, rid, (r.hovered && container), CUI__T_HIGHLIGHT), *hv = &hv_v;
    float fs = cui__fs(ui, 13);
    int row = cui__node_new(ui, rid);
    cui_node *rn = &ui->nodes[row];
    rn->dir = CUI_ROW; rn->align = CUI_ALIGN_CENTER; rn->gap = 0;
    rn->h = CUI_PX(24); rn->w = CUI_FILL;
    rn->pad[3] = 8 + 16.0f * (float)nd->depth;
    rn->radius = CUI_CORNERS(6);
    rn->bg = cui_alpha(CUI_C(ui, HOVER), *hv);
    if (container) { cui__mark_interactive(ui, row, 0); if (r.hovered) cui__set_cursor(ui, CUI_CURSOR_HAND); }
    cui__open(ui, row);
    if (container) cui__chevron_node(ui, collapsed ? -CUI_PI * 0.5f : 0, 12, CUI_C(ui, TEXT_MUTED));
    else cui_gap(ui, 12);
    cui_gap(ui, 4);
    if (nd->key_len >= 0 && nd->parent >= 0 && p->nodes[nd->parent].type == JV_OBJ) {
        int k = cui__text_node(ui, cui__frame_printf(ui, "\"%.*s\"", nd->key_len, p->s + nd->key_start), -1, CUI_FONT_MONO, fs, col[CUI__TK_FUNC], false);
        ui->nodes[k].line_height = 1.3f;
        int c = cui__text_node(ui, ": ", 2, CUI_FONT_MONO, fs, col[CUI__TK_PUNCT], false);
        ui->nodes[c].line_height = 1.3f;
    }
    const char *val;
    cui_color vc = col[CUI__TK_TEXT];
    switch (nd->type) {
    case JV_OBJ: val = collapsed ? cui__frame_printf(ui, "{ %d key%s }", nd->count, nd->count == 1 ? "" : "s") : "{"; vc = col[CUI__TK_PUNCT]; break;
    case JV_ARR: val = collapsed ? cui__frame_printf(ui, "[ %d item%s ]", nd->count, nd->count == 1 ? "" : "s") : "["; vc = col[CUI__TK_PUNCT]; break;
    case JV_STR: val = cui__frame_strdup(ui, p->s + nd->val_start, nd->val_len); vc = col[CUI__TK_STRING]; break;
    case JV_NUM: val = cui__frame_strdup(ui, p->s + nd->val_start, nd->val_len); vc = col[CUI__TK_NUMBER]; break;
    default: val = cui__frame_strdup(ui, p->s + nd->val_start, nd->val_len); vc = col[CUI__TK_CONST]; break;
    }
    int v = cui__text_node(ui, val, -1, CUI_FONT_MONO, fs, vc, false);
    ui->nodes[v].line_height = 1.3f;
    if (!container || collapsed) {
        if (!last) { int c = cui__text_node(ui, ",", 1, CUI_FONT_MONO, fs, col[CUI__TK_PUNCT], false); ui->nodes[c].line_height = 1.3f; }
    }
    cui__close(ui);
    if (container && !collapsed) {
        for (int c = nd->first; c >= 0; c = p->nodes[c].next) cui__json_rows(ui, id, p, st, c, col, p->nodes[c].next < 0);
        int close = cui__node_new(ui, 0);
        cui_node *cn = &ui->nodes[close];
        cn->dir = CUI_ROW; cn->align = CUI_ALIGN_CENTER;
        cn->h = CUI_PX(24); cn->pad[3] = 8 + 16.0f * (float)nd->depth + 16;
        cui__open(ui, close);
        int t = cui__text_node(ui, nd->type == JV_OBJ ? (last ? "}" : "},") : (last ? "]" : "],"), -1, CUI_FONT_MONO, fs, col[CUI__TK_PUNCT], false);
        ui->nodes[t].line_height = 1.3f;
        cui__close(ui);
    }
}

static bool cui__json_view_ex_body(cui_ctx *ui, const char *id_str, const char *json, const cui_json_opts *o);

/* left to right in any interface: see cui__keep_ltr */
bool cui_json_view_ex(cui_ctx *ui, const char *id_str, const char *json, const cui_json_opts *o) {
    int first = ui->node_count;
    bool r = cui__json_view_ex_body(ui, id_str, json, o);
    cui__keep_ltr(ui, first);
    return r;
}

static bool cui__json_view_ex_body(cui_ctx *ui, const char *id_str, const char *json, const cui_json_opts *o) {
    cui_json_opts d;
    memset(&d, 0, sizeof d);
    if (!o) o = &d;
    cui_id id = cui_make_id(ui, id_str);
    cui__json_state *st = cui_state(ui, id, sizeof *st);
    cui__jparse p;
    memset(&p, 0, sizeof p);
    p.s = json ? json : "";
    p.n = (int)strlen(p.s);
    p.cap = 4096;
    p.nodes = cui__frame_alloc(ui, sizeof(cui__jnode) * (size_t)p.cap);
    int root = cui__jvalue(&p, -1, 0, 0, -1);
    if (root >= 0) { cui__jws(&p); if (p.i < p.n) cui__jfail(&p, "unexpected text after the value"); }
    bool ok = root >= 0 && !p.err;
    if (ok && !st->init) {
        int depth = o->expand_depth > 0 ? o->expand_depth : 2;
        for (int k = 0; k < p.count && k < 4096; k++)
            if ((p.nodes[k].type == JV_ARR || p.nodes[k].type == JV_OBJ) && p.nodes[k].depth >= depth) st->collapsed[k >> 3] |= (uint8_t)(1 << (k & 7));
        st->init = true;
    }
    cui_color col[CUI__TK_COUNT];
    cui__syntax_colors(ui, col);
    int outer = cui__node_new(ui, id);
    cui_node *on = &ui->nodes[outer];
    on->w = CUI_FILL;
    on->radius = CUI_CORNERS(ui->theme.radius_control);
    on->bg = ui->mode == CUI_THEME_DARK ? cui_mix(CUI_C(ui, SURFACE), CUI_C(ui, BACKGROUND), 0.4f) : CUI_C(ui, SURFACE_MUTED);
    on->border = 1; on->border_color = CUI_C(ui, BORDER);
    on->flags |= CUI_NF_CLIP;   /* the outline is drawn over the title bar */
    cui__open(ui, outer);
    cui_box sb;
    memset(&sb, 0, sizeof sb);
    sb.width = CUI_FILL;
    sb.pad = 8;
    if (o->max_height > 0) sb.max_height = o->max_height;
    cui__scroll_begin_ex(ui, id ^ 0x15011u, &sb, false);
    ui->nodes[cui__current(ui)].h = CUI_FIT;
    ui->nodes[cui__current(ui)].min_h = 0;
    if (o->max_height > 0) ui->nodes[cui__current(ui)].max_h = o->max_height;
    if (ok) {
        cui_push_id(ui, id_str);
        cui__json_rows(ui, id, &p, st, root, col, true);
        cui_pop_id(ui);
    } else {
        int line = 1, colno = 1;
        for (int k = 0; k < p.err_at && k < p.n; k++) { if (p.s[k] == '\n') { line++; colno = 1; } else colno++; }
        int row = cui__node_new(ui, 0);
        ui->nodes[row].dir = CUI_ROW; ui->nodes[row].gap = 8; ui->nodes[row].align = CUI_ALIGN_CENTER;
        ui->nodes[row].pad[0] = ui->nodes[row].pad[2] = 6; ui->nodes[row].pad[3] = 6;
        cui__open(ui, row);
        cui__icon_node(ui, CUI_ICON_CIRCLE_ALERT, 16, CUI_C(ui, DANGER));
        int t = cui__text_node(ui, cui__frame_printf(ui, "Invalid JSON: %s (line %d, column %d)", p.err ? p.err : "error", line, colno), -1,
                               CUI_FONT_BODY, cui__fs(ui, ui->theme.text_sm), CUI_C(ui, DANGER), true);
        ui->nodes[t].line_height = 1.4f;
        cui__close(ui);
    }
    cui__scroll_end_ex(ui);
    cui__close(ui);
    ui->last_id = id;
    return ok;
}
