// Box2D's cross-platform determinism through the module: the same scene gives
// the same bits on every CI row (Linux x86_64 GCC and LLVM, Linux arm64,
// macOS arm64, Windows x86_64), with any worker count, through the built-in
// scheduler or a user task system, and on the SSE2 path where the CPU would
// take AVX2. The pinned values are the ones every row computes; upstream's
// falling-hinges values are upstream's own (shared/determinism.h).
//
// This translation unit is compiled with -ffp-contract=off (the package's
// cxxflags, as upstream compiles its tests): the falling-hinges setup does
// float arithmetic of its own (positions, b2MakeRot), which an FMA would
// change. The pyramid's setup is exact in binary and needs no flag.
#include <gtest/gtest.h>

import std;
import mcpplibs.mwrap.box2d;

#include "../examples/box2d/src/scenes.hpp"

namespace {

using box2d_scenes::Result;
using box2d_scenes::Config;

// Upstream's pins (single precision).
constexpr int kHingesSleepStep = 299;
constexpr std::uint32_t kHingesHash = 0x465F3DF4u;

// This wrapper's pins: a 20-row pyramid, 600 steps.
constexpr std::uint32_t kPyramidTransforms = 0x790D7C28u;
constexpr std::uint32_t kPyramidEvents = 0xE8B80415u;
constexpr std::uint64_t kPyramidState = 0xE4E1D89FFCE4C43Eull;
constexpr int kPyramidSleepStep = -1;  // sleep is off: it never falls asleep

const Config kRuns[] = {
    {1, false, false}, {2, false, false}, {4, false, false}, {8, false, false},
    {4, false, true}, {1, true, false}, {4, true, false},
};

std::string describe(const Config& config) {
    return std::format("workers={} sse2Fallback={} tasks={}", config.workers, config.sse2Fallback ? 1 : 0,
                       config.tasks ? "user" : "built-in");
}

void report(const char* scene, const Config& config, const Result& r) {
    std::printf("determinism %s %s steps=%d sleepStep=%d transforms=0x%08X events=0x%08X state=0x%016llX\n", scene,
                describe(config).c_str(), r.steps, r.sleepStep, r.transforms, r.events,
                static_cast<unsigned long long>(r.state));
}

}  // namespace

TEST(Determinism, Platform) {
    std::printf("determinism platform AVX2=%s\n", b2IsAVX2Available() ? "yes" : "no");
}

TEST(Determinism, FallingHingesUpstreamHash) {
    for (const Config& config : kRuns) {
        Result r = box2d_scenes::fallingHinges(config);
        report("falling-hinges", config, r);
        EXPECT_EQ(r.sleepStep, kHingesSleepStep) << describe(config);
        EXPECT_EQ(r.transforms, kHingesHash) << describe(config);
    }
}

TEST(Determinism, Pyramid600Steps) {
    for (const Config& config : kRuns) {
        Result r = box2d_scenes::pyramid(config);
        report("pyramid", config, r);
        EXPECT_EQ(r.steps, 600);
        EXPECT_EQ(r.sleepStep, kPyramidSleepStep) << describe(config);
        EXPECT_EQ(r.transforms, kPyramidTransforms) << describe(config);
        EXPECT_EQ(r.events, kPyramidEvents) << describe(config);
        EXPECT_EQ(r.state, kPyramidState) << describe(config);
    }
}
