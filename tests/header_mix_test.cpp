// The raw header and the import in one translation unit (the side header
// first, the canonical order): the module's names are the very same entities
// as the header's; the macros a C++ entity cannot be work beside them — the
// declaration specifiers on declarations of one's own, the initializers, the
// assertions — and the headers' static constants sit beside the module's.
#include <mwrap/box2d/macros.hpp>
#include <gtest/gtest.h>

import std;
import mcpplibs.mwrap.box2d;

// Declarations of one's own in Box2D's style.
B2_API int mwrap_box2d_test_entry(int value);
int mwrap_box2d_test_entry(int value) { return value + 1; }

B2_INLINE b2Vec2 mwrap_box2d_twice(b2Vec2 v) { return B2_LITERAL(b2Vec2){2.0f * v.x, 2.0f * v.y}; }

struct B2_ALIGN_AS(16) Aligned {
    float value;
};

TEST(HeaderMix, SameEntities) {
    static_assert(std::is_same_v<decltype(&::b2CreateWorld), decltype(&b2CreateWorld)>);
    static_assert(std::is_same_v<::b2BodyId, b2BodyId> && sizeof(b2BodyId) == 8);
    static_assert(alignof(Aligned) == 16);
    EXPECT_EQ(&::b2World_Step, &b2World_Step);
    EXPECT_EQ(&::b2_lengthUnitsPerMeter, &b2_lengthUnitsPerMeter);
    EXPECT_EQ(::b2MakeRot(0.0f).c, 1.0f);   // the header's inline function, the module's export
    EXPECT_EQ(b2_dynamicBody, 2);            // the header's enumerator
    EXPECT_EQ(mwrap_box2d_test_entry(41), 42);
    EXPECT_EQ(mwrap_box2d_twice(b2Vec2{1.0f, 2.0f}), (b2Vec2{2.0f, 4.0f}));
}

TEST(HeaderMix, InitializersAndAssertions) {
    b2BodyId none = B2_NULL_ID;
    b2SimplexCache cache = B2_ZERO_INIT;
    EXPECT_TRUE(B2_IS_NULL(none));
    EXPECT_EQ(cache.count, 0);
    B2_ASSERT(B2_IS_NULL(none));     // holds: nothing is reported
    B2_VALIDATE(cache.count == 0);
    static int reported = 0;
    b2SetAssertCallback([](const char*, const char*, int) -> int {
        ++reported;
        return 0;  // no debugger break
    });
#if !defined(NDEBUG) || defined(B2_ENABLE_ASSERT)
    B2_ASSERT(!B2_IS_NULL(none));    // fails: reported through the callback, with its file and line
    EXPECT_EQ(reported, 1);
    EXPECT_EQ(&::b2InternalAssert, &b2InternalAssert);
#endif
}

TEST(HeaderMix, ConstantsBesideTheModules) {
    namespace m = mcpplibs::mwrap::box2d::macros;
    namespace b = mcpplibs::mwrap::box2d;
    // An object-like macro expands even in a qualified name: set it aside to
    // name the constant.
    constexpr auto raw = B2_MAX_POLYGON_VERTICES;
#pragma push_macro("B2_MAX_POLYGON_VERTICES")
#undef B2_MAX_POLYGON_VERTICES
    static_assert(m::B2_MAX_POLYGON_VERTICES == raw);
#pragma pop_macro("B2_MAX_POLYGON_VERTICES")
    // A function-like macro is not expanded when its name is followed by `)`.
    EXPECT_EQ(B2_ID_EQUALS(b2_nullBodyId, b::b2_nullBodyId), (m::B2_ID_EQUALS)(b2_nullBodyId, b::b2_nullBodyId));
    // The header's static constant and the module's: one value, two entities.
    EXPECT_EQ(::b2Rot_identity.c, b::b2Rot_identity.c);
    EXPECT_NE(static_cast<const void*>(&::b2Rot_identity), static_cast<const void*>(&b::b2Rot_identity));
}
