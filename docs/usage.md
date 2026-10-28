# Using mcpplibs.mwrap.box2d

`import mcpplibs.mwrap.box2d;` declares Box2D 3.2.0's API as `<box2d/box2d.h>` does. Types, enumerators, functions, the inline math and the vector operators are the upstream globals, the very same entities as the header's, so a translation unit may include the header too. The library is `compat.box2d` 3.2.0: a static library in single precision, built as upstream's CMake builds it (`-std=gnu17`, `-ffp-contract=off`, SSE2 with the AVX2 contact solver dispatched at run time on x86_64, Neon on arm64, `-msimd128 -msse2` on the Web).

## Modules

| Module | What it holds |
|---|---|
| `mcpplibs.mwrap.box2d` | the 110 types (structs such as `b2WorldDef`, `b2BodyDef`, `b2ShapeDef`, `b2Polygon`, `b2Manifold`, the ids `b2WorldId` … `b2ContactId`, the enums `b2BodyType`, `b2ShapeType`, `b2JointType`, `b2HexColor` …, the callback function types, the aliases `b2Pos` and `b2WorldTransform`), 3 opaque structs (`b2DynamicTree`, `b2Recording`, `b2Replay`), all 176 enumerators, all 506 `B2_API` functions, the 91 inline functions (`b2MakeRot`, `b2Add`, `b2TransformPoint`, `b2StoreBodyId` …), the 8 C++ vector operators and `b2_lengthUnitsPerMeter`; the 13 static constants in `mcpplibs::mwrap::box2d`; re-exports the module below |
| `mcpplibs.mwrap.box2d.macros` | the macros, in `mcpplibs::mwrap::box2d::macros` |

The public headers are the ones upstream installs (`BOX2D_API_FILES` in src/CMakeLists.txt): `base.h`, `box2d.h`, `collision.h`, `config.h`, `constants.h`, `id.h`, `math_functions.h`, `math_types.h`, `types.h`.

### Adapted: the static constants

The headers define `b2Vec2_zero`, `b2Rot_identity`, `b2Transform_identity`, `b2Mat22_zero`, `b2Pos_zero`, `b2WorldTransform_identity`, `b2_emptySimplexCache` and the null ids `b2_nullWorldId`, `b2_nullBodyId`, `b2_nullShapeId`, `b2_nullChainId`, `b2_nullJointId`, `b2_nullContactId` as `static const` objects: internal linkage, which a named module cannot export. Each is an `inline constexpr` of the same type and initializer in `mcpplibs::mwrap::box2d`, under the same name, checked byte for byte against the header's in [tests/macros_mix_test.cpp](../tests/macros_mix_test.cpp). `using namespace mcpplibs::mwrap::box2d;` makes them read as they do with the header. With the header included too, its constants are its own (`::b2_nullBodyId`); do not also bring the namespace in with a using-directive there, or the unqualified names are ambiguous.

The inline functions need no adaptation: `B2_INLINE`, `B2_FORCE_INLINE` and `B2_ID_INLINE` are `inline` (external linkage) in C++, so they are exported as they are.

### Conditional: b2InternalAssert

`b2InternalAssert`, which `B2_ASSERT` calls, is declared by `base.h` and defined by the library only when `NDEBUG` is not defined (or `B2_ENABLE_ASSERT` is); the module exports it under the same condition.

### Determinism

Box2D 3 computes the same bits for the same inputs on every platform it supports, with any worker count and SIMD width, provided the floating point is compiled without contracting `a*b+c` into an FMA. `compat.box2d` compiles the library with `-ffp-contract=off`. Code of your own whose float arithmetic feeds the simulation (positions, velocities, `b2MakeRot` and the other inline math, which compile into your translation units) needs the flag too wherever an FMA exists (arm64 always; x86_64 under AVX2/FMA): `[build] cxxflags = ["-ffp-contract=off"]`. This package compiles its module and tests with it. Values that are exact in binary (integers, halves) are the same either way.

[tests/determinism_test.cpp](../tests/determinism_test.cpp) pins two scenes, run with 1, 2, 4 and 8 workers of the built-in scheduler, 4 workers of a user task system, and the SSE2 fallback with 1 and 4 workers:

| Scene | Sleep step | Transform hash | Event hash | `b2World_GetStateHash` |
|---|---|---|---|---|
| falling hinges (upstream's `shared/determinism.c`) | 299 | `0x465F3DF4` (upstream's pin) | `0xD019CCFF` | `0x8BD019D8675ACE67` |
| 20-row pyramid struck by a ball, 600 steps, sleep off | — | `0x790D7C28` | `0xE8B80415` | `0xE4E1D89FFCE4C43E` |

Every CI row computes these values: Linux x86_64 (GCC 16.1, LLVM 23.1.3), Linux arm64 (GCC 16.1, LLVM 23.1.3), macOS arm64, Windows x86_64 and wasm32-emscripten under node. [tools/run_determinism.py](../tools/run_determinism.py) runs the same configurations through [examples/box2d](../examples/box2d) and prints one line per scene.

## Macros

A named module cannot export a macro. `mcpplibs::mwrap::box2d::macros` holds the ones a C++ entity can be, under their original names.

- **Constants** as `inline constexpr`, each of the type its macro expands to: `B2_PI`, `B2_MAX_ROTATION`, `B2_CONTACT_RECYCLE_COS_ANGLE`, `B2_AABB_MARGIN_FRACTION`, `B2_TIME_TO_SLEEP` are `float`; `B2_NULL_INDEX`, `B2_ENABLE_VALIDATION`, `B2_VERSION_MAJOR` / `_MINOR` / `_REVISION`, `B2_HASH_INIT`, `B2_MAX_POLYGON_VERTICES`, `B2_MAX_WORKERS`, `B2_MAX_TASKS`, `B2_GRAPH_COLOR_COUNT`, `B2_MAX_WORLDS`, `B2_NAME_LENGTH`, `B2_MAX_RESTITUTION_ITERATIONS`, `B2_DEFAULT_CATEGORY_BITS` are `int`; `B2_DEFAULT_MASK_BITS` is `uint64_t` (`UINT64_MAX`). `B2_MAX_WORLDS` and `B2_NAME_LENGTH` are upstream's defaults, which the library is built with.
- **Length-unit values** as functions of no arguments: `B2_HUGE()`, `B2_LINEAR_SLOP()`, `B2_SPECULATIVE_DISTANCE()`, `B2_CONTACT_RECYCLE_DISTANCE()`, `B2_MAX_AABB_MARGIN()`. Their macros read `b2_lengthUnitsPerMeter`, which `b2SetLengthUnitsPerMeter` changes at run time, so they are not constants; each function returns the macro's value, of its type (`float`), at the call.
- **Computations** as `constexpr` function templates: `B2_IS_NULL`, `B2_IS_NON_NULL`, `B2_ID_EQUALS`. Each body is the macro itself, expanded on the parameters: the result has the macro's value and type (`bool`) for every id type. Each argument is evaluated once, before the call (`B2_ID_EQUALS` evaluates each of its arguments three times).

Each mapping is checked against the raw header — the constants for type and value, the length-unit values at two length units, the computations for null and non-null ids of every id type — in [tests/macros_mix_test.cpp](../tests/macros_mix_test.cpp).

### What stays a macro

These stay macros in `<mwrap/box2d/macros.hpp>`, which is `<box2d/box2d.h>`:

| Macros | Why |
|---|---|
| `BOX2D_EXPORT`, `B2_API`, `B2_INLINE`, `B2_FORCE_INLINE`, `B2_ID_INLINE` | declaration specifiers (`BOX2D_EXPORT` is empty here: the library is static) |
| `B2_ALIGN_AS(N)` | an alignment specifier |
| `B2_LITERAL(T)`, `B2_ZERO_INIT`, `B2_NULL_ID` | initializer syntax: C compound literals, `{}` for any type |
| `B2_ASSERT`, `B2_VALIDATE`, `B2_BREAKPOINT` | assertions: they stringify the condition and report the call site's `__FILE__` / `__LINE__`, or trap |

Include the side header before the import. While a header macro is defined, it expands even inside a qualified name: an object-like macro therefore hides `macros::B2_PI`; use the plain macro, or set it aside with `#pragma push_macro`. A function-like macro does not expand when its name is followed by `)`, so `(macros::B2_ID_EQUALS)(a, b)` calls the function beside the header ([tests/header_mix_test.cpp](../tests/header_mix_test.cpp)). [tools/check_exports.py](../tools/check_exports.py) accounts for all 40 macros.

## Licensing

The wrapper's code is Apache-2.0. Box2D is MIT ([upstream/box2d/LICENSE](../upstream/box2d/LICENSE)); a program built with it, statically linked, carries the MIT notice.

`runtime.deploy` copies the wrapper's LICENSE and NOTICE and Box2D's README and LICENSE into `licenses/box2d/` beside every program that depends on this package.

## Checks

- `mcpp test` runs five tests:
  - [tests/api_test.cpp](../tests/api_test.cpp): the API through the module only — the version, world definitions, a box that falls, rests and sleeps, null ids and id storage, polygon, circle, capsule and chain shapes, ray casts and AABB overlaps, a revolute joint, sensor events, the custom filter callback, hulls, mass, manifolds, segment distance and ray casts against shapes, the inline math and operators, the length unit, a custom allocator and a user task system;
  - [tests/macros_mix_test.cpp](../tests/macros_mix_test.cpp) (generated): every constant, length-unit value, function and static constant against the header;
  - [tests/header_mix_test.cpp](../tests/header_mix_test.cpp): the header beside the import — the same entities, `B2_API`, `B2_INLINE`, `B2_LITERAL` and `B2_ALIGN_AS` on declarations of one's own, `B2_NULL_ID`, `B2_ZERO_INIT` and `B2_ASSERT` through the assert callback;
  - [tests/macros_test.cpp](../tests/macros_test.cpp): the macros module alone, at compile time;
  - [tests/determinism_test.cpp](../tests/determinism_test.cpp): the two scenes above under every configuration, against the pinned values.
- `python3 tools/check_exports.py` checks that the sources are what [tools/gen_exports.py](../tools/gen_exports.py) writes from the pinned headers, and that every declaration is exported, every static constant adapted, and every macro mapped or kept.
- `python3 tools/run_determinism.py` builds [examples/box2d](../examples/box2d) in the dev and the release profile (optimized: GCC contracts `a*b+c` into an FMA only when optimizing, if allowed; Clang at every level) and compares every configuration's result with the pins; `mcpp test --profile release` runs the tests optimized and with `NDEBUG`.
- `python3 tools/test_template.py` scaffolds the default template from an installed copy of the package, then builds and runs it.
