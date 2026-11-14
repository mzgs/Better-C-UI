#!/usr/bin/env python3
"""Build-only registry and portable baseline queries for the pinned demo grammars."""
import json
from pathlib import Path
import re
import sys

mode, manifest, output, *args = sys.argv[1:]
languages = json.loads(Path(manifest).read_text(encoding="utf-8"))
assert languages and len({x['id'] for x in languages}) == len(languages)
out = Path(output)
out.mkdir(parents=True, exist_ok=True)

if mode == 'cmake':
    lines = []
    grammars = {}
    for entry in languages:
        key = (entry['crate'], entry['version'], entry['directory'])
        if key in grammars:
            lines.append('cui_demo_alias(%s %s)' % (entry['id'], grammars[key]))
        else:
            grammars[key] = entry['id']
            cache = (entry['crate'] + '_' + entry['version']).replace('-', '_').replace('.', '_')
            lines.append(('cui_demo_grammar({id} ' + cache + ' "{url}" {sha256} "{directory}")').format(**entry))
    (out / 'grammars.cmake').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    sys.exit(0)


def capture(node):
    name = node['type']
    if not node['named']:
        if re.fullmatch(r'[@#]?[A-Za-z_][\w-]*', name):
            return 'constant' if name in ('true', 'false', 'null', 'nil', 'none') else 'keyword'
        if name and not any(ord(c) < 32 for c in name) and name not in ('\"', "'", '`') and not any(c.isalnum() for c in name):
            return 'punctuation'
        return None
    name = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', name).lower()
    if name.startswith('k_'):
        return 'constant' if name in ('k_true', 'k_false', 'k_nil') else 'keyword'
    # shortcut: node-name categories give baseline colors; use language-specific
    # queries when semantic highlighting or embedded languages are required.
    if 'comment' in name:
        return 'comment'
    if re.search(r'(string|char|character|regex|regexp)(_|$)', name) and not re.search(r'(expression|pattern|type|argument|list|operator)', name):
        return 'string'
    if re.search(r'(^|_)(integer|float|number|decimal|hex|octal|binary|real)(_|$)', name) and not re.search(r'(expression|type|pattern|list)', name):
        return 'number'
    if re.fullmatch(r'(boolean|bool|null|nil|true|false|none)(_literal)?', name):
        return 'constant'
    if 'keyword' in name or name in ('tag_name', 'atx_heading', 'setext_heading', 'instruction', 'opcode'):
        return 'keyword'
    if name in ('type_identifier', 'primitive_type', 'predefined_type', 'builtin_type', 'intrinsic_type'):
        return 'type'
    if name.startswith('preproc_') and not node.get('fields'):
        return 'preproc'
    if name in ('directive', 'output_directive', 'graphql_directive'):
        return 'preproc'
    if name in ('code_span', 'link_destination'):
        return 'string'
    if name in ('emphasis', 'strong_emphasis'):
        return 'keyword'
    if name in ('function_name', 'command_name', 'property_name', 'attribute_name', 'bare_key', 'key', 'section_name'):
        return 'function'
    if name in ('addition', 'added_line'):
        return 'string'
    if name in ('deletion', 'removed_line'):
        return 'comment'
    return None


roots = dict(line.split('\t', 1) for line in Path(args[0]).read_text(encoding="utf-8").splitlines())


def c_string(value):
    return json.dumps(value, ensure_ascii=False).replace('?', r'\?')


source = ['#include "languages.h"']
headers = ['#ifndef CUI_DEMO_LANGUAGES_H', '#define CUI_DEMO_LANGUAGES_H',
           '#include <cui/cui_tree_sitter.h>',
           'enum { CUI_DEMO_LANGUAGE_COUNT = %d };' % len(languages),
           'typedef struct { const char *id, *name; const struct TSLanguage *(*language)(void); const char *query, *sample; } cui_demo_language;',
           'extern const cui_demo_language cui_demo_languages[CUI_DEMO_LANGUAGE_COUNT];', '#endif']
rows = []
for entry in languages:
    root = Path(roots[entry['id']])
    nodes = json.loads((root / 'src/node-types.json').read_text(encoding="utf-8"))
    patterns = []
    for node in nodes:
        if '@generated' in node:
            continue
        kind = capture(node)
        if kind:
            target = '(' + node['type'] + ')' if node['named'] else json.dumps(node['type'], ensure_ascii=False)
            patterns.append(target + ' @' + kind)
        for field, info in node.get('fields', {}).items():
            if field not in ('name', 'key', 'function'):
                continue
            if field == 'key' or 'function' in node['type'] or 'method' in node['type'] or field == 'function':
                for child in info['types']:
                    if child['named'] and child['type'] in ('identifier', 'name', 'string', 'property_identifier', 'field_identifier'):
                        category = 'string.special.key' if field == 'key' else 'function'
                        patterns.append('(%s %s: (%s) @%s)' % (node['type'], field, child['type'], category))
    assert patterns, entry['id']
    query = '\n'.join(dict.fromkeys(patterns))
    (out / (entry['id'] + '.scm')).write_text(query + '\n', encoding='utf-8')
    source.append('const struct TSLanguage *%s(void);' % entry['symbol'])
    source.append('static const char cui_demo_query_%s[] =\n%s;' % (entry['id'], '\n'.join(c_string(line + '\n') for line in query.splitlines())))
    rows.append('{%s, %s, %s, cui_demo_query_%s, %s}' % (c_string(entry['id']), c_string(entry['name']), entry['symbol'], entry['id'], c_string(entry['sample'])))
source.append('const cui_demo_language cui_demo_languages[CUI_DEMO_LANGUAGE_COUNT] = {\n    ' + ',\n    '.join(rows) + '\n};')
(out / 'languages.h').write_text('\n'.join(headers) + '\n', encoding='utf-8')
(out / 'languages.c').write_text('\n'.join(source) + '\n', encoding='utf-8')
