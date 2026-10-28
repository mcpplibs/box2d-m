// The determinism scenes, written against `import mcpplibs.mwrap.box2d;` (the
// importer imports the module before including this file):
//
//   falling hinges   upstream's own cross-platform determinism scene
//                    (shared/determinism.c, test/test_determinism.c): 80
//                    boxes in 4 columns, hinged in pairs with limited,
//                    sprung, motorised revolute joints, rotated by ±0.1 rad
//                    through b2MakeRot (Box2D's own cosine and sine), fall
//                    and settle; stepped until every body sleeps. Upstream
//                    pins the step and the hash: 299, 0x465F3DF4.
//   pyramid          a 20-row pyramid of 210 unit boxes on a static ground,
//                    struck from the left by a heavy ball (radius 1, density
//                    8, launched at 24 m/s), with sleep off, stepped exactly
//                    600 times (10 s at 60 Hz, 4 sub-steps): the collapse is
//                    chaotic, so a difference in any bit of any step grows
//                    into the final transforms. Every initial coordinate and
//                    velocity is exact in binary (x = j + i/2 - 9.5, y = i +
//                    1/2), so the scene's own arithmetic is the same with or
//                    without FMA contraction in the caller: what the hash
//                    measures is the library.
//
// Each run records the step on which the world fell asleep, the djb2 hash
// (b2Hash) of every body's final b2Transform bytes in creation order, a hash
// of the per-step event counts (contact begins, body moves, awake bodies), and
// b2World_GetStateHash, Box2D's deep hash of the world.
#pragma once

namespace box2d_scenes {

struct Config {
    int workers = 1;          // 1: serial; more: the built-in scheduler, or `tasks`
    bool sse2Fallback = false;  // the 4-wide solver where the CPU would take AVX2
    bool tasks = false;       // a user task system (serial, on the caller)
};

struct Result {
    int steps = 0;
    int sleepStep = -1;
    std::uint32_t transforms = 0;
    std::uint32_t events = 0;
    std::uint64_t state = 0;
};

inline void* runInline(b2TaskCallbackFcn* task, void* taskContext, void*) {
    task(taskContext);
    return nullptr;  // executed serially: nothing to finish
}
inline void finishNothing(void*, void*) {}

inline b2WorldId makeWorld(const Config& config, bool sleep = true) {
    b2WorldDef def = b2DefaultWorldDef();
    def.enableSleep = sleep;
    def.workerCount = config.workers;
    if (config.tasks) {
        def.enqueueTask = runInline;
        def.finishTask = finishNothing;
    }
    b2WorldId world = b2CreateWorld(&def);
    b2World_EnableSSE2Fallback(world, config.sse2Fallback);
    return world;
}

inline std::uint32_t hashTransforms(const std::vector<b2BodyId>& bodies) {
    std::uint32_t hash = mcpplibs::mwrap::box2d::macros::B2_HASH_INIT;
    for (b2BodyId body : bodies) {
        b2WorldTransform t = b2Body_GetTransform(body);
        hash = b2Hash(hash, reinterpret_cast<const std::uint8_t*>(&t), static_cast<int>(sizeof t));
    }
    return hash;
}

// Steps `world` until `done` (or `limit` steps), recording the result.
template <class Done>
Result step(b2WorldId world, const std::vector<b2BodyId>& bodies, int limit, Done done) {
    Result r;
    r.events = mcpplibs::mwrap::box2d::macros::B2_HASH_INIT;
    while (r.steps < limit) {
        b2World_Step(world, 1.0f / 60.0f, 4);
        const int counts[3] = {b2World_GetContactEvents(world).beginCount, b2World_GetBodyEvents(world).moveCount,
                               b2World_GetAwakeBodyCount(world)};
        r.events = b2Hash(r.events, reinterpret_cast<const std::uint8_t*>(counts), static_cast<int>(sizeof counts));
        if (r.sleepStep < 0 && counts[1] == 0) r.sleepStep = r.steps;
        ++r.steps;
        if (done(r)) break;
    }
    r.transforms = hashTransforms(bodies);
    r.state = b2World_GetStateHash(world);
    return r;
}

inline Result fallingHinges(const Config& config) {
    b2WorldId world = makeWorld(config);
    {
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.position = b2Pos{0.0f, -1.0f};
        b2BodyId ground = b2CreateBody(world, &bodyDef);
        b2Polygon box = b2MakeBox(40.0f, 1.0f);
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        b2CreatePolygonShape(ground, &shapeDef, &box);
    }
    const int columnCount = 4, rowCount = 20;
    const float h = 0.25f;
    b2Polygon box = b2MakeSquare(h);
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    b2RevoluteJointDef jointDef = b2DefaultRevoluteJointDef();
    jointDef.enableLimit = true;
    jointDef.lowerAngle = -0.1f * mcpplibs::mwrap::box2d::macros::B2_PI;
    jointDef.upperAngle = 0.2f * mcpplibs::mwrap::box2d::macros::B2_PI;
    jointDef.enableSpring = true;
    jointDef.hertz = 1.0f;
    jointDef.dampingRatio = 1.0f;
    jointDef.enableMotor = true;
    jointDef.maxMotorTorque = 0.25f;
    jointDef.base.localFrameA.p = b2Vec2{-h, h};
    jointDef.base.localFrameB.p = b2Vec2{-h, -h};
    jointDef.base.constraintHertz = 60.0f;
    jointDef.base.constraintDampingRatio = 0.0f;
    jointDef.base.drawScale = 0.5f;

    std::vector<b2BodyId> bodies;
    const float offset = 0.4f * h, dx = 10.0f * h;
    const float xBase = -0.5f * dx * (columnCount - 1.0f);
    for (int j = 0; j < columnCount; ++j) {
        const float x = xBase + j * dx;
        b2BodyId prev{};
        for (int i = 0; i < rowCount; ++i) {
            b2BodyDef bodyDef = b2DefaultBodyDef();
            bodyDef.type = b2_dynamicBody;
            bodyDef.position.x = x + offset * i;
            bodyDef.position.y = h + 2.0f * h * i;
            bodyDef.rotation = b2MakeRot((i & 1) == 0 ? -0.1f : 0.1f);
            b2BodyId body = b2CreateBody(world, &bodyDef);
            if ((i & 1) == 0) {
                prev = body;
            } else {
                jointDef.base.bodyIdA = prev;
                jointDef.base.bodyIdB = body;
                b2CreateRevoluteJoint(world, &jointDef);
                prev = b2BodyId{};
            }
            b2CreatePolygonShape(body, &shapeDef, &box);
            bodies.push_back(body);
        }
    }
    // Upstream hashes the transforms on the first step with no body moving.
    Result r = step(world, bodies, 1000, [](const Result& r) { return r.sleepStep >= 0; });
    b2DestroyWorld(world);
    return r;
}

inline Result pyramid(const Config& config, int steps = 600) {
    b2WorldId world = makeWorld(config, false);
    {
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.position = b2Pos{0.0f, -1.0f};
        b2BodyId ground = b2CreateBody(world, &bodyDef);
        b2Polygon box = b2MakeBox(40.0f, 1.0f);
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        b2CreatePolygonShape(ground, &shapeDef, &box);
    }
    const int rows = 20;
    b2Polygon box = b2MakeBox(0.5f, 0.5f);
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    std::vector<b2BodyId> bodies;
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < rows - i; ++j) {
            b2BodyDef bodyDef = b2DefaultBodyDef();
            bodyDef.type = b2_dynamicBody;
            bodyDef.position = b2Pos{static_cast<float>(j) + 0.5f * static_cast<float>(i) - 9.5f,
                                     static_cast<float>(i) + 0.5f};
            b2BodyId body = b2CreateBody(world, &bodyDef);
            b2CreatePolygonShape(body, &shapeDef, &box);
            bodies.push_back(body);
        }
    }
    {
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = b2Pos{-30.0f, 3.0f};
        bodyDef.linearVelocity = b2Vec2{24.0f, 4.0f};
        bodyDef.isBullet = true;
        b2BodyId ball = b2CreateBody(world, &bodyDef);
        b2Circle circle = {{0.0f, 0.0f}, 1.0f};
        b2ShapeDef ballDef = b2DefaultShapeDef();
        ballDef.density = 8.0f;
        b2CreateCircleShape(ball, &ballDef, &circle);
        bodies.push_back(ball);
    }
    Result r = step(world, bodies, steps, [](const Result&) { return false; });
    b2DestroyWorld(world);
    return r;
}

}  // namespace box2d_scenes
