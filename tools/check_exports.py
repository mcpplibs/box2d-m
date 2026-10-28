#!/usr/bin/env python3
"""Audit the module surface against the pinned release's public Box2D headers.

- src/box2d.cppm, src/macros.cppm and tests/macros_mix_test.cpp are what
  tools/gen_exports.py writes.
- Every type, opaque struct, enumerator, B2_API function, inline function,
  C++ operator and variable of the installed headers is exported by
  `export using`, and nothing else is.
- Every `static const` object of the headers is a constant of the same type
  in mcpplibs::mwrap::box2d, checked byte for byte against the header's by
  tests/macros_mix_test.cpp.
- Every macro of the headers is mapped in src/macros.cppm (a constant, a
  length-unit function or a constexpr function), or kept as a macro in
  <mwrap/box2d/macros.hpp> with its reason; and each mapping is checked
  against the raw header by tests/macros_mix_test.cpp.
"""
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from inventory import fetch, surface  # noqa: E402
from gen_exports import SIDE_HEADER  # noqa: E402


def mapped_names(text: str) -> set[str]:
    text = text[text.index('export module'):]
    names = set(re.findall(r'^inline constexpr auto (\w+) = c::value_\1;', text, flags=re.M))
    names |= set(re.findall(r'^inline auto (\w+)\(\) noexcept \{ return c::value_\1\(\); \}', text, flags=re.M))
    names |= set(re.findall(r'^constexpr auto \((\w+)\)\(', text, flags=re.M))
    return names


def main():
    root = Path(__file__).resolve().parents[1]
    problems = []
    generated = subprocess.run([sys.executable, str(root / 'tools/gen_exports.py'), '--check'], capture_output=True, text=True)
    if generated.returncode != 0:
        problems.append(generated.stderr.strip() or generated.stdout.strip())
    sf = surface(fetch(root))
    module = (root / 'src/box2d.cppm').read_text(encoding='utf-8')
    exported = set(re.findall(r'^export using ::(\w+|operator\S+);', module, flags=re.M))
    declared = (set(sf.types) | set(sf.opaque) | set(sf.enumerators) | set(sf.functions) | set(sf.inline)
                | set(sf.operators) | set(sf.variables))
    if declared - exported:
        problems.append('src/box2d.cppm lacks: ' + ', '.join(sorted(declared - exported)))
    if exported - declared:
        problems.append('src/box2d.cppm exports what the headers do not declare: ' + ', '.join(sorted(exported - declared)))

    constants = {m.group(2): m.group(1)
                 for m in re.finditer(r'^inline constexpr ::(\w+) (\w+) = ', module, flags=re.M)}
    if constants != {c.name: c.type for c in sf.constants}:
        problems.append(f'static constants: module {sorted(constants.items())} != headers '
                        f'{sorted((c.name, c.type) for c in sf.constants)}')

    macros_text = (root / 'src/macros.cppm').read_text(encoding='utf-8')
    mapped = mapped_names(macros_text)
    macros = set(sf.macros)
    unaccounted = macros - mapped - SIDE_HEADER.keys()
    if unaccounted:
        problems.append('macros neither mapped nor side-header: ' + ', '.join(sorted(unaccounted)))
    both = SIDE_HEADER.keys() & mapped
    if both:
        problems.append('listed as side-header yet mapped: ' + ', '.join(sorted(both)))
    stale = SIDE_HEADER.keys() - macros
    if stale:
        problems.append('listed but not in the headers: ' + ', '.join(sorted(stale)))
    test = (root / 'tests/macros_mix_test.cpp').read_text(encoding='utf-8')
    untested = {n for n in mapped if f'm::{n}' not in test} | {n for n in constants if f'b::{n}' not in test}
    if untested:
        problems.append('mapped but not checked against the header: ' + ', '.join(sorted(untested)))
    side = (root / 'include/mwrap/box2d/macros.hpp').read_text(encoding='utf-8')
    if '#include <box2d/box2d.h>' not in side:
        problems.append('<mwrap/box2d/macros.hpp> does not include <box2d/box2d.h>')
    if problems:
        sys.exit('\n'.join(problems))
    functions = sum(1 for m in sf.macros.values() if m.name in mapped and m.params is not None)
    runtime = len(re.findall(r'^inline auto (\w+)\(\) noexcept \{ return c::value_', macros_text, flags=re.M))
    print(f'{len(sf.types)} types, {len(sf.opaque)} opaque structs, {len(sf.enumerators)} enumerators, '
          f'{len(sf.functions)} B2_API functions ({len(sf.guarded)} under the assertion conditional), '
          f'{len(sf.inline)} inline functions, {len(sf.operators)} operators and {len(sf.variables)} variable exported; '
          f'{len(constants)} static constants as constants of their types; {len(macros)} macros: '
          f'{len(mapped) - functions - runtime} constants, {runtime} length-unit functions and {functions} functions '
          f'mapped (each checked against the header), {len(SIDE_HEADER)} kept as macros')


if __name__ == '__main__':
    main()
