// The macros module on its own (no header, no root module): the constants and
// the id functions are constant expressions, as the macros are, so they work
// in constant-initialised tables, array bounds and static_asserts; the
// length-unit values are functions of the length unit.
import std;
import mcpplibs.mwrap.box2d.macros;

namespace macros = mcpplibs::mwrap::box2d::macros;
using namespace mcpplibs::mwrap::box2d::macros;

// Every constant has the type its macro expands to.
static_assert(std::is_same_v<decltype(B2_PI), const float>);                       // 3.14159265359f
static_assert(std::is_same_v<decltype(B2_MAX_POLYGON_VERTICES), const int>);       // 8
static_assert(std::is_same_v<decltype(B2_DEFAULT_MASK_BITS), const std::uint64_t>);  // UINT64_MAX
static_assert(std::is_same_v<decltype(B2_DEFAULT_CATEGORY_BITS), const int>);      // 1
static_assert(std::is_same_v<decltype(B2_NULL_INDEX), const int>);                 // ( -1 )
static_assert(B2_PI == 3.14159265359f && B2_MAX_ROTATION == 0.25f * B2_PI);
static_assert(B2_VERSION_MAJOR == 3 && B2_VERSION_MINOR == 2 && B2_VERSION_REVISION == 0);
static_assert(B2_HASH_INIT == 5381 && B2_NULL_INDEX == -1);
static_assert(B2_MAX_WORKERS == 32 && B2_MAX_TASKS == 256 && B2_GRAPH_COLOR_COUNT == 24);
static_assert(B2_MAX_WORLDS == 128 && B2_NAME_LENGTH == 10 && B2_MAX_RESTITUTION_ITERATIONS == 63);
static_assert(B2_DEFAULT_MASK_BITS == ~std::uint64_t{0} && B2_DEFAULT_CATEGORY_BITS == 1);
static_assert(B2_TIME_TO_SLEEP == 0.5f && B2_AABB_MARGIN_FRACTION == 0.125f && B2_CONTACT_RECYCLE_COS_ANGLE == 0.98f);

// The id functions, at compile time, on any struct with the id members.
struct Id {
    std::int32_t index1;
    std::uint16_t world0;
    std::uint16_t generation;
};
static_assert(B2_IS_NULL(Id{0, 1, 1}) && !B2_IS_NULL(Id{1, 0, 0}));
static_assert(B2_IS_NON_NULL(Id{3, 0, 0}) && !B2_IS_NON_NULL(Id{}));
static_assert(B2_ID_EQUALS(Id{1, 2, 3}, Id{1, 2, 3}) && !B2_ID_EQUALS(Id{1, 2, 3}, Id{1, 2, 4}));
static_assert(std::is_same_v<decltype(B2_ID_EQUALS(Id{}, Id{})), bool>);

// A constant-initialised table and array bounds, as the macros allow.
struct Polygon {
    float vertices[2 * B2_MAX_POLYGON_VERTICES];
    int count;
};
constexpr Polygon square = {{-1, -1, 1, -1, 1, 1, -1, 1}, 4};
static_assert(square.count < B2_MAX_POLYGON_VERTICES && std::size(square.vertices) == 16);

int main() {
    // At the default length unit (1 per meter): upstream's documented values.
    if (B2_LINEAR_SLOP() != 0.005f || B2_SPECULATIVE_DISTANCE() != 4.0f * 0.005f) return 1;
    if (B2_HUGE() != 100.0f * 1000.0f || B2_MAX_AABB_MARGIN() != 0.05f) return 2;
    if (B2_CONTACT_RECYCLE_DISTANCE() != 10.0f * 0.005f) return 3;
    return macros::B2_VERSION_MAJOR == 3 && macros::B2_MAX_POLYGON_VERTICES == 8 ? 0 : 4;
}
