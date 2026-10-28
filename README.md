# box2d

> Box2D v3, the 2D physics engine for games, through C++23 modules.

**English** · [简体中文](README.zh.md) · [Upstream](https://github.com/erincatto/box2d) · [Releases](https://github.com/mcpplibs/box2d-m/releases) · [mcpp](https://github.com/mcpp-community/mcpp) · [Index](https://github.com/mcpplibs/mcpp-index)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![Box2D](https://img.shields.io/badge/Box2D-MIT-blue.svg)](upstream/box2d/LICENSE)
[![mcpp-community](https://img.shields.io/badge/mcpp--community-mwrap-blue)](https://github.com/mcpp-community)
[![CI](https://github.com/mcpplibs/box2d-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mcpplibs/box2d-m/actions/workflows/ci.yml)

Box2D 3.2.0 simulates rigid bodies in 2D: bodies, shapes, joints, contact and
sensor events, ray and shape casts, continuous collision and a built-in task
scheduler, in MIT-licensed C17. The upstream C API is unchanged:
`b2CreateWorld`, `b2World_Step`, `b2CreatePolygonShape` and `b2MakeRot` are
the upstream globals of `<box2d/box2d.h>`. The library is `compat.box2d`
3.2.0, built as upstream builds it for determinism (`-ffp-contract=off`, its
default SIMD paths). Box2D is cross-platform deterministic, and every CI run
checks it: upstream's own determinism scene and a struck 20-row pyramid give
the same bits on Linux x86_64 (GCC, LLVM), Linux arm64 (GCC, LLVM), macOS,
Windows and wasm32, with 1 to 8 workers and either SIMD width.

## Modules

- `import mcpplibs.mwrap.box2d;`: the whole API of `<box2d/box2d.h>` (worlds, bodies, shapes, chains, joints, events, queries, collision and geometry, the inline math, the vector operators), the headers' static constants (`b2_nullBodyId`, `b2Rot_identity` …) in `mcpplibs::mwrap::box2d`, re-exporting the module below
- `import mcpplibs.mwrap.box2d.macros;`: `B2_PI`, `B2_MAX_POLYGON_VERTICES`, `B2_HASH_INIT`, `B2_LINEAR_SLOP()`, `B2_ID_EQUALS` … in `mcpplibs::mwrap::box2d::macros`

> More Modules API -> [Usage and API](docs/usage.md)

## Getting started

```sh
mcpp new tower --template mcpplibs.mwrap.box2d@3.2.0
cd tower
mcpp run
```

The default template knocks a tower of boxes over with a ball on a fixed
60 Hz step, prints the boxes as they fall, and ends with the hash of the
final state: the same on every platform.

```toml
[dependencies]
"mcpplibs.mwrap.box2d" = "3.2.0"
```

```cpp
import std;
import mcpplibs.mwrap.box2d;

int main() {
    b2WorldDef worldDef = b2DefaultWorldDef();  // gravity (0, -10)
    b2WorldId world = b2CreateWorld(&worldDef);

    b2BodyDef groundDef = b2DefaultBodyDef();
    groundDef.position = b2Vec2{0.0f, -1.0f};
    b2BodyId ground = b2CreateBody(world, &groundDef);
    b2Polygon groundBox = b2MakeBox(20.0f, 1.0f);
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    b2CreatePolygonShape(ground, &shapeDef, &groundBox);

    b2BodyDef boxDef = b2DefaultBodyDef();
    boxDef.type = b2_dynamicBody;
    boxDef.position = b2Vec2{0.0f, 4.0f};
    b2BodyId box = b2CreateBody(world, &boxDef);
    b2Polygon square = b2MakeBox(0.5f, 0.5f);
    b2CreatePolygonShape(box, &shapeDef, &square);

    for (int i = 0; i < 90; ++i) b2World_Step(world, 1.0f / 60.0f, 4);
    b2Vec2 p = b2Body_GetPosition(box);
    std::println("the box rests at ({:.2f}, {:.2f})", p.x, p.y);  // (0.00, 0.50)
    b2DestroyWorld(world);
}
```

## Determinism

Box2D 3 gives the same result for the same inputs on every platform it
supports, provided the floating point is built as upstream builds it:
without contracting `a*b+c` into an FMA. `compat.box2d` builds the library
that way. Your own float arithmetic that feeds the simulation (positions you
compute, `b2MakeRot` and the other inline math, which compile into your code)
needs the same flag where FMA exists (arm64 always, x86_64 with AVX2/FMA):
`[build] cxxflags = ["-ffp-contract=off"]`, as this repository's tests and
examples use. [tools/run_determinism.py](tools/run_determinism.py) and
[examples/box2d](examples/box2d) (`box2d-determinism`) report the hashes on
any machine.

## Macros

Constants such as `B2_PI` and `B2_MAX_POLYGON_VERTICES` keep the type their
macro has. `B2_HUGE`, `B2_LINEAR_SLOP` and the other values that read
`b2_lengthUnitsPerMeter` are functions (`B2_LINEAR_SLOP()`), because
`b2SetLengthUnitsPerMeter` changes them at run time. `B2_IS_NULL`,
`B2_IS_NON_NULL` and `B2_ID_EQUALS` are constexpr functions whose body is the
macro. The declaration specifiers (`B2_API`, `B2_INLINE` …), the initializers
`B2_NULL_ID` / `B2_ZERO_INIT` and the assertions `B2_ASSERT` / `B2_VALIDATE`
stay macros in `<mwrap/box2d/macros.hpp>`, included before the import.
Details: [docs/usage.md](docs/usage.md).

## Examples

- [examples/box2d](examples/box2d): `box2d-determinism`, which runs the determinism scenes under a chosen
  worker count and SIMD path and prints the sleep step and the hashes, to compare machines and compilers.

## Build and testing

CI on Linux (x86_64 GCC and LLVM, arm64 GCC and LLVM), macOS and Windows
builds the modules and runs the tests, the export audit
([tools/check_exports.py](tools/check_exports.py)) and the determinism runs;
a Web job runs the tests and the determinism runs as wasm32-emscripten under
node. The audit checks that the generated sources are fresh, that every
declaration is exported and that every macro is mapped or listed with its
reason. A separate job scaffolds the default template as an installed package
and runs it.

## License

[Apache-2.0](LICENSE) for the wrapper, by mcpp-community ([NOTICE](NOTICE)).
Box2D is MIT ([upstream/box2d/LICENSE](upstream/box2d/LICENSE)); its license
and README are deployed beside every program that depends on this package.
