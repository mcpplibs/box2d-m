// <mwrap/box2d/macros.hpp> — Box2D's macros as macros, for the ones a C++
// entity cannot be: the declaration specifiers (BOX2D_EXPORT, B2_API,
// B2_INLINE, B2_FORCE_INLINE, B2_ID_INLINE, B2_ALIGN_AS), the initializer
// syntax (B2_LITERAL, B2_ZERO_INIT, B2_NULL_ID) and the assertions
// (B2_ASSERT, B2_VALIDATE, B2_BREAKPOINT).
//
// Include it before `import mcpplibs.mwrap.box2d;`: it is <box2d/box2d.h>, as
// the module sees it (compat.box2d is a static library in single precision,
// so BOX2D_EXPORT is empty and b2Pos is b2Vec2). Every other macro is defined
// too, and while a macro is defined it expands even inside
// `mcpplibs::mwrap::box2d::macros::NAME`; a function-like one does not when its
// name is followed by `)`: `(macros::B2_ID_EQUALS)(a, b)` is the function. The
// header's `static const` objects (b2_nullBodyId …) are its own here, and
// `mcpplibs::mwrap::box2d::b2_nullBodyId` the module's constant of the same
// value.
#pragma once
#include <box2d/box2d.h>
