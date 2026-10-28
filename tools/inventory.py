#!/usr/bin/env python3
"""The public surface of Box2D 3.2.0, from the pinned release.

Shared by tools/gen_exports.py (which writes the module sources and the
generated macro test) and tools/check_exports.py (which audits them). The
headers come from the upstream tag archive, downloaded once into
target/upstream and checked against upstream/documents.json: the same bytes
compat.box2d builds.

The public headers are the ones upstream installs (src/CMakeLists.txt,
BOX2D_API_FILES). They are read as a C++ consumer of compat.box2d compiles
them: `__cplusplus` is defined, BOX2D_DOUBLE_PRECISION is not (single
precision, as the library is built), neither are BOX2D_USER_CONFIG,
BOX2D_EXPORT, BOX2D_DLL or box2d_EXPORTS (a static library). The remaining
conditionals (_MSC_VER, __GNUC__, NDEBUG, B2_ENABLE_ASSERT, BOX2D_VALIDATE)
only choose between two definitions of the same macro (B2_FORCE_INLINE,
B2_BREAKPOINT, B2_ASSERT, B2_VALIDATE, B2_ENABLE_VALIDATION): every branch is
read for its macro names, and a declaration under one of them is an error —
except under the assertion conditional, `!defined( NDEBUG ) ||
defined( B2_ENABLE_ASSERT )`, which declares b2InternalAssert: the library
defines it under the same condition (src/core.c), so the module exports it
under that condition too (Surface.guarded).
"""
from dataclasses import dataclass, field
import hashlib
import json
from pathlib import Path
import re
import sys
import tarfile
import urllib.request

INSTALLED = [  # src/CMakeLists.txt: BOX2D_API_FILES, in its order
    'base.h', 'box2d.h', 'collision.h', 'config.h', 'constants.h', 'id.h', 'math_functions.h', 'math_types.h',
    'types.h',
]

# How this build answers each conditional that selects declarations.
DECIDED = {
    '__cplusplus': True,
    'BOX2D_DOUBLE_PRECISION': False,
    'BOX2D_USER_CONFIG': False,
    'BOX2D_EXPORT': False,
}
# The assertion conditional: its declarations exist in a build without NDEBUG
# (or with B2_ENABLE_ASSERT), in the library and the consumer alike.
GUARD = '!defined( NDEBUG ) || defined( B2_ENABLE_ASSERT )'
# Conditionals that only choose between definitions of the same macros.
MACRO_ONLY = ['_MSC_VER', '__GNUC__', '__clang__', 'NDEBUG', 'B2_ENABLE_ASSERT', 'BOX2D_VALIDATE',
              'B2_ENABLE_VALIDATION', '_WIN32', 'BOX2D_DLL', 'box2d_EXPORTS', 'B2_MAX_WORLDS', 'B2_NAME_LENGTH']


def fetch(root: Path) -> Path:
    pins = json.loads((root / 'upstream/documents.json').read_text(encoding='utf-8'))
    cache = root / 'target/upstream'
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / ('box2d-' + pins['upstream_version'] + '.tar.gz')
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != pins['upstream_sha256']:
        print('+ fetch', pins['upstream'], flush=True)
        with urllib.request.urlopen(pins['upstream'], timeout=120) as response:
            archive.write_bytes(response.read())
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != pins['upstream_sha256']:
        sys.exit(f'upstream archive checksum {digest} != {pins["upstream_sha256"]}')
    with tarfile.open(archive) as tar:
        top = tar.getmembers()[0].name.split('/')[0]
        if not (cache / top / 'include/box2d/box2d.h').exists():
            tar.extractall(cache, filter='data')
    return cache / top


@dataclass
class Macro:
    name: str
    header: str
    params: list[str] | None   # None: object-like
    body: str


@dataclass
class Constant:            # `static const T name = init;` — internal linkage
    name: str
    type: str
    init: str


@dataclass
class Surface:
    types: list[str] = field(default_factory=list)           # typedef / struct / enum names
    opaque: list[str] = field(default_factory=list)          # declared, not defined (b2Replay, b2Recording)
    enumerators: dict[str, str] = field(default_factory=dict)  # enumerator: its enum
    functions: list[str] = field(default_factory=list)       # B2_API
    inline: list[str] = field(default_factory=list)          # B2_INLINE / B2_FORCE_INLINE / B2_ID_INLINE
    operators: list[str] = field(default_factory=list)       # the C++ vector operators of math_functions.h
    variables: list[str] = field(default_factory=list)       # extern variables
    constants: list[Constant] = field(default_factory=list)  # static const objects
    macros: dict[str, Macro] = field(default_factory=dict)
    guarded: dict[str, str] = field(default_factory=dict)    # declaration: the #if it is under


def strip(text: str) -> str:
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def condition(expr: str) -> bool | None:
    """True/False for a decided conditional, None for a macro-only one."""
    names = re.findall(r'\b[A-Za-z_]\w*\b', expr.replace('defined', ' '))
    if any(n in DECIDED for n in names):
        if len(names) != 1:
            sys.exit(f'a compound conditional over a decided name: {expr}')
        value = DECIDED[names[0]]
        return (not value) if expr.lstrip().startswith('!') or expr.startswith('ndef') else value
    if all(n in MACRO_ONLY for n in names):
        return None
    sys.exit(f'a conditional the inventory does not know: {expr}')


def preprocess(text: str, header: str, s: Surface) -> str:
    """The header's declarations as this build sees them; its macros into `s`."""
    out = []
    stack: list[tuple[bool | None | str, bool]] = []   # (active: True/False/None = macro-only/GUARD, taken)
    lines = text.replace('\\\n', ' ').split('\n')
    for line in lines:
        d = re.match(r'^\s*#\s*(\w+)\s*(.*)$', line)
        live = all(a is not False for a, _ in stack)
        macro_only = any(a is None for a, _ in stack)
        if d:
            kind, rest = d.group(1), d.group(2).strip()
            if kind == 'if' and rest == GUARD:
                stack.append((GUARD, True))
            elif kind in ('if', 'ifdef', 'ifndef'):
                expr = {'if': rest, 'ifdef': rest, 'ifndef': '!' + rest}[kind]
                v = condition(expr)
                stack.append((v, v is True))
            elif kind == 'elif':
                v, taken = stack.pop()
                c = condition(rest)
                if v is None or c is None:
                    stack.append((None, True))
                else:
                    stack.append((c and not taken, taken or c))
            elif kind == 'else':
                v, taken = stack.pop()
                stack.append((None, True) if v is None or v == GUARD else (not taken, True))
            elif kind == 'endif':
                stack.pop()
            elif kind == 'define' and live:
                m = re.match(r'(\w+)(\(([^)]*)\))?\s*(.*)$', rest)
                params = [p.strip() for p in m.group(3).split(',')] if m.group(2) else None
                s.macros.setdefault(m.group(1), Macro(m.group(1), header, params, ' '.join(m.group(4).split())))
            elif kind in ('pragma', 'include') or not live:
                pass
            else:
                sys.exit(f'{header}: a directive the inventory does not know: {line}')
            continue
        guarded = any(a == GUARD for a, _ in stack)
        if live and not macro_only and guarded:
            out.append('@GUARD@ ' + line if line.strip() else line)
        elif live and not macro_only:
            out.append(line)
        elif live and line.strip():
            sys.exit(f'{header}: a declaration under a macro-only conditional: {line.strip()}')
    return '\n'.join(out)


def statements(code: str):
    """Top-level statements: up to `;` at depth 0, or a function body's `}`."""
    start, depth, i = 0, 0, 0
    while i < len(code):
        ch = code[i]
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                head = code[start:i]
                head = head[:head.index('{')]
                if re.search(r'\)\s*(const\s*)?$', head):
                    yield code[start:i + 1].strip()
                    start = i + 1
        elif ch == ';' and depth == 0:
            yield code[start:i + 1].strip()
            start = i + 1
        i += 1
    if code[start:].strip():
        sys.exit('unterminated declaration: ' + code[start:][:200])


def surface(upstream: Path) -> Surface:
    headers = upstream / 'include/box2d'
    s = Surface()
    for name in INSTALLED:
        code = preprocess(strip((headers / name).read_text(encoding='utf-8')), name, s)
        for st in statements(code):
            one = ' '.join(st.split())
            if one.startswith('@GUARD@ '):
                one = one.replace('@GUARD@ ', '')
                g = re.search(r'(\w+) ?\(', one)
                s.guarded[g.group(1)] = GUARD
            if m := re.fullmatch(r'typedef (struct|enum) (\w+) \{(.*)\} (\w+) ?;', one):
                if m.group(2) != m.group(4):
                    sys.exit(f'{name}: typedef of another name: {one[:120]}')
                s.types.append(m.group(4))
                if m.group(1) == 'enum':
                    for e in re.findall(r'(?:^|,)\s*([A-Za-z_]\w*)\s*(?:=[^,]*)?', m.group(3)):
                        s.enumerators[e] = m.group(4)
            elif m := re.fullmatch(r'typedef struct (\w+) (\w+) ?;', one):
                if m.group(1) != m.group(2):
                    sys.exit(f'{name}: typedef of another name: {one}')
                s.opaque.append(m.group(1))
            elif m := re.fullmatch(r'typedef [\w\s\*]+?\**\s*(\w+) ?\(.*\) ?;', one):
                s.types.append(m.group(1))   # a function type: `typedef void b2TaskCallbackFcn( void* );`
            elif m := re.fullmatch(r'typedef \w+ (\w+) ?;', one):
                s.types.append(m.group(1))
            elif m := re.fullmatch(r'B2_API [^(]*?\b(\w+) ?\(.*\) ?;', one):
                s.functions.append(m.group(1))
            elif m := re.fullmatch(r'(?:B2_INLINE|B2_FORCE_INLINE|B2_ID_INLINE) [^(]*?\b(\w+) ?\(.*\}', one):
                s.inline.append(m.group(1))
            elif m := re.fullmatch(r'inline [^(]*?\b(operator\s*\S+?) ?\(.*\}', one):
                s.operators.append(m.group(1).replace(' ', ''))
            elif m := re.fullmatch(r'extern "C" BOX2D_EXPORT \w+ (\w+) ?;', one):
                s.variables.append(m.group(1))
            elif m := re.fullmatch(r'static const (\w+) (\w+) = (.*) ?;', one):
                s.constants.append(Constant(m.group(2), m.group(1), m.group(3)))
            else:
                sys.exit(f'{name}: a declaration the inventory does not know: {one[:200]}')
    # Opaque forward declarations that are defined later (b2SimplexCache) are types.
    s.opaque = sorted(set(s.opaque) - set(s.types))
    s.types = sorted(set(s.types))
    s.functions = sorted(set(s.functions))
    s.inline = sorted(set(s.inline))
    s.operators = sorted(set(s.operators))
    return s


if __name__ == '__main__':
    root = Path(__file__).resolve().parents[1]
    sf = surface(fetch(root))
    print(f'{len(sf.types)} types, {len(sf.opaque)} opaque, {len(sf.enumerators)} enumerators, '
          f'{len(sf.functions)} B2_API functions, {len(sf.inline)} inline functions, {len(sf.operators)} operators, '
          f'{len(sf.variables)} variables, {len(sf.constants)} static constants, {len(sf.macros)} macros')
    print('types:', ' '.join(sf.types))
    print('opaque:', ' '.join(sf.opaque))
    print('enums:', ' '.join(sorted(set(sf.enumerators.values()))))
    print('inline:', ' '.join(sf.inline))
    print('operators:', ' '.join(sf.operators))
    print('variables:', ' '.join(sf.variables))
    print('constants:', ' '.join(f'{c.type} {c.name} = {c.init}' for c in sf.constants))
    print('object-like:', ' '.join(f'{m.name}={m.body}' for m in sf.macros.values() if m.params is None))
    print('function-like:', ' '.join(m.name for m in sf.macros.values() if m.params is not None))
