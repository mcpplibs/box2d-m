#!/usr/bin/env python3
"""Box2D's determinism on this machine, through examples/box2d.

Builds examples/box2d (box2d-determinism, written against the module), runs
both scenes under every configuration tests/determinism_test.cpp runs — 1, 2,
4 and 8 workers of the built-in scheduler, 4 workers of a user task system,
the SSE2 fallback with 1 and 4 workers — and requires every run of a scene to
give the same sleep step and hashes, and those to be the values the test
pins (upstream's for the falling hinges). It prints one line per scene, the
line CI rows are compared by.

  python3 tools/run_determinism.py [--toolchain NAME@VERSION] [--target TRIPLE]
"""
import argparse
import json
from pathlib import Path
import platform
import re
import subprocess
import sys

CONFIGS = [['--workers', '1'], ['--workers', '2'], ['--workers', '4'], ['--workers', '8'],
           ['--workers', '4', '--tasks'], ['--workers', '1', '--sse2'], ['--workers', '4', '--sse2']]


def pins(root: Path) -> dict:
    text = (root / 'tests/determinism_test.cpp').read_text(encoding='utf-8')
    value = lambda name: int(re.search(r'\b' + name + r' = (-?(?:0x)?[0-9A-Fa-f]+)', text).group(1), 0)
    return {
        'falling-hinges': {'sleepStep': value('kHingesSleepStep'), 'transforms': value('kHingesHash')},
        'pyramid': {'sleepStep': value('kPyramidSleepStep'), 'transforms': value('kPyramidTransforms'),
                    'events': value('kPyramidEvents'), 'state': value('kPyramidState')},
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain')
    parser.add_argument('--target')
    parser.add_argument('--mcpp', default='mcpp')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    example = root / 'examples/box2d'
    select = (['--toolchain', args.toolchain] if args.toolchain else []) + (['--target', args.target] if args.target else [])
    subprocess.run([args.mcpp, 'build', *select], cwd=example, check=True)
    runs: dict[str, list[dict]] = {}
    for config in CONFIGS:
        out = subprocess.run([args.mcpp, 'run', *select, '--', '--json', *config], cwd=example, check=True,
                             capture_output=True, text=True).stdout
        for line in out.splitlines():
            if line.startswith('{'):
                run = json.loads(line)
                runs.setdefault(run['scene'], []).append(run)
    expected = pins(root)
    failed = False
    host = f'{platform.system()} {platform.machine()}' + (f' {args.target}' if args.target else '')
    for scene, results in runs.items():
        keys = ('steps', 'sleepStep', 'transforms', 'events', 'state')
        distinct = {tuple(r[k] for k in keys) for r in results}
        first = results[0]
        print(f'Box2D determinism, {host}{", " + args.toolchain if args.toolchain else ""}: {scene}: '
              f'sleepStep={first["sleepStep"]} transforms={first["transforms"]} events={first["events"]} '
              f'state={first["state"]} — {len(results)} runs, {"identical" if len(distinct) == 1 else "DIFFERENT"}, '
              f'AVX2 {"available" if first["avx2"] else "not available"}', flush=True)
        if len(distinct) != 1:
            failed = True
            for r in results:
                print('   ', json.dumps(r))
        for key, want in expected[scene].items():
            got = first[key] if isinstance(first[key], int) else int(first[key], 16)
            if got != want:
                failed = True
                print(f'    {scene} {key}: 0x{got:X} != pinned 0x{want:X}')
    if set(runs) != set(expected):
        sys.exit(f'scenes {sorted(runs)} != {sorted(expected)}')
    if failed:
        sys.exit('Box2D is not deterministic here, or not as pinned')


if __name__ == '__main__':
    main()
