// The Box2D API through the module, spelled as <box2d/box2d.h> documents it:
// the version, worlds and their definitions, bodies, shapes (polygon, circle,
// capsule, segment, chain), joints, contact / body / sensor events, the
// pre-solve and custom-filter callbacks, ray casts and overlap queries, the
// collision and geometry functions (manifolds, hulls, mass, distance), the
// inline math and id functions, the C++ vector operators, the length unit,
// the allocator and log hooks, and a custom task system. Only the module is
// imported; no Box2D header is included.
#include <gtest/gtest.h>

import std;
import mcpplibs.mwrap.box2d;

using namespace mcpplibs::mwrap::box2d;  // b2_nullBodyId, b2Vec2_zero, b2Rot_identity …
namespace m = mcpplibs::mwrap::box2d::macros;

namespace {

// A world with a static ground segment from x = -20 to 20 at y = 0.
struct World {
    b2WorldId id;
    b2BodyId ground;
    explicit World(b2WorldDef def = b2DefaultWorldDef()) : id(b2CreateWorld(&def)) {
        b2BodyDef groundDef = b2DefaultBodyDef();
        ground = b2CreateBody(id, &groundDef);
        b2Segment segment = {{-20.0f, 0.0f}, {20.0f, 0.0f}};
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        b2CreateSegmentShape(ground, &shapeDef, &segment);
    }
    ~World() { b2DestroyWorld(id); }
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    b2BodyId box(b2Vec2 at, float half = 0.5f, bool events = false) {
        b2BodyDef def = b2DefaultBodyDef();
        def.type = b2_dynamicBody;
        def.position = at;
        b2BodyId body = b2CreateBody(id, &def);
        b2Polygon shape = b2MakeBox(half, half);
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.enableContactEvents = events;
        b2CreatePolygonShape(body, &shapeDef, &shape);
        return body;
    }
    void step(int count) {
        for (int i = 0; i < count; ++i) b2World_Step(id, 1.0f / 60.0f, 4);
    }
};

}  // namespace

TEST(Api, Version) {
    b2Version v = b2GetVersion();
    EXPECT_EQ(v.major, m::B2_VERSION_MAJOR);
    EXPECT_EQ(v.minor, m::B2_VERSION_MINOR);
    EXPECT_EQ(v.revision, m::B2_VERSION_REVISION);
    EXPECT_EQ(v.major * 100 + v.minor * 10 + v.revision, 320);
    EXPECT_FALSE(b2IsDoublePrecision());  // compat.box2d: single precision
    static_assert(std::is_same_v<b2Pos, b2Vec2> && std::is_same_v<b2WorldTransform, b2Transform>);
}

TEST(Api, WorldDefinition) {
    b2WorldDef def = b2DefaultWorldDef();
    EXPECT_EQ(def.gravity.y, -10.0f);
    EXPECT_TRUE(def.enableSleep);
    def.gravity = {0.0f, -9.8f};
    World world(def);
    EXPECT_TRUE(b2World_IsValid(world.id));
    EXPECT_EQ(b2World_GetGravity(world.id), (b2Vec2{0.0f, -9.8f}));
    b2World_SetGravity(world.id, b2Vec2{0.0f, -20.0f});
    EXPECT_EQ(b2World_GetGravity(world.id).y, -20.0f);
    int data = 7;
    b2World_SetUserData(world.id, &data);
    EXPECT_EQ(b2World_GetUserData(world.id), &data);
}

TEST(Api, FallRestSleep) {
    World world;
    b2BodyId body = world.box({0.0f, 4.0f}, 0.5f, true);
    EXPECT_TRUE(b2Body_IsValid(body));
    EXPECT_EQ(b2Body_GetType(body), b2_dynamicBody);
    b2MassData mass = b2Body_GetMassData(body);
    EXPECT_NEAR(mass.mass, 1.0f, 1e-5f);  // a 1x1 box of the default density 1

    int begins = 0, moves = 0;
    for (int i = 0; i < 240; ++i) {
        world.step(1);
        b2ContactEvents contacts = b2World_GetContactEvents(world.id);
        begins += contacts.beginCount;
        if (contacts.beginCount > 0) {
            EXPECT_TRUE(b2Shape_IsValid(contacts.beginEvents[0].shapeIdA) && b2Shape_IsValid(contacts.beginEvents[0].shapeIdB));
        }
        moves += b2World_GetBodyEvents(world.id).moveCount;
    }
    EXPECT_GE(begins, 1);
    EXPECT_GT(moves, 0);
    EXPECT_NEAR(b2Body_GetPosition(body).y, 0.5f, 0.01f);
    EXPECT_FALSE(b2Body_IsAwake(body));
    b2Body_ApplyLinearImpulseToCenter(body, b2Vec2{0.0f, 5.0f}, true);
    EXPECT_TRUE(b2Body_IsAwake(body));
    EXPECT_GT(b2Body_GetLinearVelocity(body).y, 4.0f);
}

TEST(Api, NullIds) {
    b2BodyId none = b2_nullBodyId;
    EXPECT_TRUE(m::B2_IS_NULL(none));
    EXPECT_FALSE(b2Body_IsValid(none));
    World world;
    EXPECT_TRUE(m::B2_IS_NON_NULL(world.ground));
    EXPECT_FALSE(m::B2_ID_EQUALS(world.ground, b2_nullBodyId));
    EXPECT_TRUE(m::B2_ID_EQUALS(world.ground, b2LoadBodyId(b2StoreBodyId(world.ground))));
    EXPECT_EQ(b2StoreWorldId(b2LoadWorldId(b2StoreWorldId(world.id))), b2StoreWorldId(world.id));
    EXPECT_TRUE(m::B2_IS_NULL(b2_nullWorldId) && m::B2_IS_NULL(b2_nullShapeId) && m::B2_IS_NULL(b2_nullJointId));
}

TEST(Api, ShapesAndQueries) {
    World world;
    b2BodyId body = world.box({0.0f, 0.5f});
    b2BodyDef def = b2DefaultBodyDef();
    def.position = {5.0f, 2.0f};
    b2BodyId ball = b2CreateBody(world.id, &def);
    b2Circle circle = {{0.0f, 0.0f}, 0.5f};
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    b2ShapeId circleShape = b2CreateCircleShape(ball, &shapeDef, &circle);
    EXPECT_EQ(b2Shape_GetType(circleShape), b2_circleShape);
    b2Capsule capsule = {{-1.0f, 0.0f}, {1.0f, 0.0f}, 0.25f};
    b2ShapeId capsuleShape = b2CreateCapsuleShape(ball, &shapeDef, &capsule);
    EXPECT_EQ(b2Shape_GetType(capsuleShape), b2_capsuleShape);
    EXPECT_EQ(b2Body_GetShapeCount(ball), 2);

    // The closest hit straight down from above the box is its top face.
    b2RayResult hit = b2World_CastRayClosest(world.id, b2Vec2{0.0f, 10.0f}, b2Vec2{0.0f, -20.0f}, b2DefaultQueryFilter());
    ASSERT_TRUE(hit.hit);
    EXPECT_TRUE(m::B2_ID_EQUALS(b2Shape_GetBody(hit.shapeId), body));
    EXPECT_NEAR(hit.point.y, 1.0f, 1e-4f);
    EXPECT_EQ(hit.normal, (b2Vec2{0.0f, 1.0f}));

    // Every shape a ray crosses, through the callback.
    struct Hits { int count = 0; } hits;
    auto collect = [](b2ShapeId, b2Pos, b2Vec2, float, void* context) -> float {
        ++static_cast<Hits*>(context)->count;
        return 1.0f;  // keep going
    };
    b2World_CastRay(world.id, b2Vec2{-10.0f, 2.0f}, b2Vec2{20.0f, 0.0f}, b2DefaultQueryFilter(), collect, &hits);
    EXPECT_EQ(hits.count, 2);  // the circle and the capsule at y = 2

    // An AABB overlap around the ball finds its two shapes.
    int overlaps = 0;
    auto count = [](b2ShapeId, void* context) -> bool {
        ++*static_cast<int*>(context);
        return true;
    };
    b2AABB box = {{4.0f, 1.5f}, {6.0f, 2.5f}};
    b2World_OverlapAABB(world.id, b2Pos_zero, box, b2DefaultQueryFilter(), count, &overlaps);
    EXPECT_EQ(overlaps, 2);
}

TEST(Api, Chain) {
    World world;
    b2Vec2 points[] = {{-5.0f, 0.0f}, {0.0f, 1.0f}, {5.0f, 0.0f}, {0.0f, -1.0f}};
    b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
    b2ChainDef def = b2DefaultChainDef();
    def.points = points;
    def.pointCount = 4;
    def.materials = &material;
    def.materialCount = 1;
    def.isLoop = true;
    b2ChainId chain = b2CreateChain(world.ground, &def);
    EXPECT_TRUE(b2Chain_IsValid(chain));
    EXPECT_EQ(b2Chain_GetSegmentCount(chain), 4);
    b2DestroyChain(chain);
    EXPECT_FALSE(b2Chain_IsValid(chain));
}

TEST(Api, RevoluteJoint) {
    World world;
    b2BodyId a = world.box({0.0f, 5.0f});
    b2BodyDef def = b2DefaultBodyDef();
    def.position = {0.0f, 8.0f};
    b2BodyId anchor = b2CreateBody(world.id, &def);  // static
    b2RevoluteJointDef joint = b2DefaultRevoluteJointDef();
    joint.base.bodyIdA = anchor;
    joint.base.bodyIdB = a;
    joint.base.localFrameA.p = {0.0f, 0.0f};
    joint.base.localFrameB.p = {0.0f, 3.0f};
    b2JointId id = b2CreateRevoluteJoint(world.id, &joint);
    EXPECT_TRUE(b2Joint_IsValid(id));
    EXPECT_EQ(b2Joint_GetType(id), b2_revoluteJoint);
    world.step(60);
    // Hanging from the anchor: 3 m below it.
    EXPECT_NEAR(b2Body_GetPosition(a).y, 5.0f, 0.05f);
    b2DestroyJoint(id);
    EXPECT_FALSE(b2Joint_IsValid(id));
}

TEST(Api, SensorEvents) {
    World world;
    b2BodyDef def = b2DefaultBodyDef();
    def.position = {0.0f, 2.0f};
    b2BodyId sensorBody = b2CreateBody(world.id, &def);
    b2Polygon zone = b2MakeBox(2.0f, 0.5f);
    b2ShapeDef sensorDef = b2DefaultShapeDef();
    sensorDef.isSensor = true;
    sensorDef.enableSensorEvents = true;
    b2CreatePolygonShape(sensorBody, &sensorDef, &zone);

    b2BodyDef boxDef = b2DefaultBodyDef();
    boxDef.type = b2_dynamicBody;
    boxDef.position = {0.0f, 6.0f};
    b2BodyId body = b2CreateBody(world.id, &boxDef);
    b2Polygon shape = b2MakeBox(0.25f, 0.25f);
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.enableSensorEvents = true;
    b2CreatePolygonShape(body, &shapeDef, &shape);

    int begins = 0;
    for (int i = 0; i < 120; ++i) {
        world.step(1);
        begins += b2World_GetSensorEvents(world.id).beginCount;
    }
    EXPECT_EQ(begins, 1);
}

TEST(Api, Callbacks) {
    World world;
    world.box({0.0f, 0.5f});
    static int filtered = 0;
    b2World_SetCustomFilterCallback(world.id, [](b2ShapeId, b2ShapeId, void*) -> bool {
        ++filtered;
        return true;
    }, nullptr);
    b2BodyDef def = b2DefaultBodyDef();
    def.type = b2_dynamicBody;
    def.position = {0.0f, 2.0f};
    b2BodyId body = b2CreateBody(world.id, &def);
    b2Polygon shape = b2MakeBox(0.5f, 0.5f);
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.enableCustomFiltering = true;
    b2CreatePolygonShape(body, &shapeDef, &shape);
    world.step(60);
    EXPECT_GT(filtered, 0);
}

TEST(Api, Geometry) {
    b2Vec2 points[] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0.5f, 0.5f}};
    b2Hull hull = b2ComputeHull(points, 5);
    EXPECT_EQ(hull.count, 4);
    EXPECT_TRUE(b2ValidateHull(&hull));
    b2Polygon polygon = b2MakePolygon(&hull, 0.0f);
    EXPECT_NEAR(b2ComputePolygonMass(&polygon, 2.0f).mass, 2.0f, 1e-5f);
    EXPECT_LE(polygon.count, m::B2_MAX_POLYGON_VERTICES);

    // Two unit boxes, the second 0.9 above the first: overlapping by 0.1.
    b2Polygon a = b2MakeBox(0.5f, 0.5f);
    b2Transform xf = {{0.0f, 0.9f}, b2Rot_identity};
    b2LocalManifold manifold = b2CollidePolygons(&a, &a, xf);
    EXPECT_EQ(manifold.pointCount, 2);
    EXPECT_NEAR(manifold.normal.y, 1.0f, 1e-6f);

    b2SegmentDistanceResult d = b2ComputeSegmentDistance({0, 0}, {1, 0}, {0, 2}, {1, 2});
    EXPECT_NEAR(d.distanceSquared, 4.0f, 1e-6f);

    b2RayCastInput ray = {{-5.0f, 0.0f}, {10.0f, 0.0f}, 1.0f};
    b2CastOutput out = b2CastRayPolygon(&a, &ray);
    EXPECT_TRUE(out.hit);
    EXPECT_NEAR(out.fraction, 0.45f, 1e-6f);
}

TEST(Api, InlineMathAndOperators) {
    b2Vec2 a = {1.0f, 2.0f}, b = {3.0f, -1.0f};
    EXPECT_EQ(b2Add(a, b), a + b);
    EXPECT_EQ(b2Sub(a, b), a - b);
    EXPECT_EQ(b2MulSV(2.0f, a), 2.0f * a);
    EXPECT_EQ(b2Neg(a), -a);
    EXPECT_EQ(b2Dot(a, b), 1.0f);
    EXPECT_EQ(b2Cross(a, b), -7.0f);
    b2Vec2 c = a;
    c += b;
    c -= b;
    c *= 1.0f;
    EXPECT_TRUE(c == a && !(c != a));
    EXPECT_EQ(b2Length(b2Vec2{3.0f, 4.0f}), 5.0f);
    EXPECT_EQ(b2MinInt(3, 4), 3);
    EXPECT_EQ(b2ClampFloat(5.0f, 0.0f, 1.0f), 1.0f);

    // Box2D's own cosine and sine (the cross-platform deterministic ones).
    b2Rot q = b2MakeRot(0.5f * m::B2_PI);
    EXPECT_NEAR(q.c, 0.0f, 1e-3f);
    EXPECT_NEAR(q.s, 1.0f, 1e-3f);
    EXPECT_TRUE(b2IsNormalizedRot(q));
    b2CosSin cs = b2ComputeCosSin(0.0f);
    EXPECT_EQ(cs.cosine, 1.0f);
    EXPECT_EQ(cs.sine, 0.0f);
    EXPECT_NEAR(b2Atan2(1.0f, 1.0f), 0.25f * m::B2_PI, 1e-4f);
    b2Transform xf = {{1.0f, 0.0f}, b2MakeRot(0.0f)};
    EXPECT_EQ(b2TransformPoint(xf, b2Vec2{1.0f, 1.0f}), (b2Vec2{2.0f, 1.0f}));
    EXPECT_EQ(b2InvTransformPoint(xf, b2Vec2{2.0f, 1.0f}), (b2Vec2{1.0f, 1.0f}));
    EXPECT_EQ(b2Transform_identity.q.c, 1.0f);
    EXPECT_EQ(b2Vec2_zero, (b2Vec2{0.0f, 0.0f}));
    EXPECT_TRUE(b2IsValidVec2(a) && b2IsValidFloat(1.0f) && !b2IsValidFloat(std::numeric_limits<float>::infinity()));
}

TEST(Api, LengthUnits) {
    EXPECT_EQ(b2GetLengthUnitsPerMeter(), 1.0f);
    EXPECT_EQ(m::B2_LINEAR_SLOP(), 0.005f);
    b2SetLengthUnitsPerMeter(100.0f);
    EXPECT_EQ(b2_lengthUnitsPerMeter, 100.0f);
    EXPECT_EQ(m::B2_LINEAR_SLOP(), 0.5f);
    b2SetLengthUnitsPerMeter(1.0f);
}

namespace {
std::atomic<long> allocated{0};
constexpr std::size_t kAlignment = 64;  // at least any alignment Box2D asks for
void* allocate(std::size_t size, int alignment) {
    if (static_cast<std::size_t>(alignment) > kAlignment) return nullptr;
    allocated += static_cast<long>(size);
    return ::operator new(size, std::align_val_t(kAlignment));
}
void release(void* memory, std::size_t size) {
    allocated -= static_cast<long>(size);
    ::operator delete(memory, std::align_val_t(kAlignment));
}
}  // namespace

TEST(Api, Allocator) {
    // b2SetAllocator reaches every allocation of a world made after it.
    b2SetAllocator(allocate, release);
    {
        World world;
        world.box({0.0f, 2.0f});
        world.step(10);
        EXPECT_GT(allocated.load(), 0);
        EXPECT_GT(b2GetByteCount(), 0u);
    }
    EXPECT_EQ(allocated.load(), 0);
    b2SetAllocator(nullptr, nullptr);
}

namespace {
// A task system that runs every task at once, on the caller.
int tasksRun = 0;
void* enqueue(b2TaskCallbackFcn* task, void* taskContext, void*) {
    ++tasksRun;
    task(taskContext);
    return nullptr;  // executed serially: nothing to finish
}
void finish(void*, void*) {}
}  // namespace

TEST(Api, TaskCallbacks) {
    b2WorldDef def = b2DefaultWorldDef();
    def.workerCount = 4;
    def.enqueueTask = enqueue;
    def.finishTask = finish;
    World world(def);
    EXPECT_EQ(b2World_GetWorkerCount(world.id), 4);
    for (int i = 0; i < 10; ++i) world.box({0.0f, 1.0f + 1.5f * i});
    world.step(60);
    EXPECT_GT(tasksRun, 0);
}
