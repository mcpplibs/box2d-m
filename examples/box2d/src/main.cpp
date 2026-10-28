// box2d-determinism — does this machine simulate Box2D bit for bit as every
// other one does? Runs Box2D's determinism scenes through import
// mcpplibs.mwrap.box2d and prints, per run, the step on which the world fell
// asleep and three hashes of its state: the djb2 hash of every body's final
// transform, a hash of the per-step event counts, and b2World_GetStateHash.
// Two machines, compilers, worker counts or SIMD paths agree when the lines
// agree.
//
//   box2d-determinism [--scene hinges|pyramid|all] [--workers N] [--sse2]
//                     [--tasks] [--steps N] [--json]
//
//   --scene    hinges: upstream's falling hinges (pinned upstream: sleep
//              step 299, transforms 0x465F3DF4); pyramid: a struck 20-row
//              pyramid, sleep off, --steps steps (600); all (the default)
//   --workers  worker count (1: serial; more: Box2D's built-in scheduler)
//   --sse2     take the 4-wide SSE2 solver where the CPU would take AVX2
//   --tasks    run tasks through a user task system (serially, on the caller)
//   --json     one JSON object per run instead of a line of text
import std;
import mcpplibs.mwrap.box2d;

#include "scenes.hpp"

namespace {

int usage() {
    std::cerr << "usage: box2d-determinism [--scene hinges|pyramid|all] [--workers N] [--sse2] [--tasks] [--steps N] [--json]\n";
    return 2;
}

void print(std::string_view scene, const box2d_scenes::Config& c, const box2d_scenes::Result& r, bool json) {
    if (json) {
        std::printf("{\"scene\":\"%.*s\",\"workers\":%d,\"sse2Fallback\":%s,\"tasks\":\"%s\",\"avx2\":%s,"
                    "\"steps\":%d,\"sleepStep\":%d,\"transforms\":\"0x%08X\",\"events\":\"0x%08X\","
                    "\"state\":\"0x%016llX\"}\n",
                    static_cast<int>(scene.size()), scene.data(), c.workers, c.sse2Fallback ? "true" : "false",
                    c.tasks ? "user" : "built-in", b2IsAVX2Available() ? "true" : "false", r.steps, r.sleepStep,
                    r.transforms, r.events, static_cast<unsigned long long>(r.state));
    } else {
        std::printf("%-14.*s workers=%d sse2Fallback=%d tasks=%-8s steps=%d sleepStep=%d transforms=0x%08X "
                    "events=0x%08X state=0x%016llX\n",
                    static_cast<int>(scene.size()), scene.data(), c.workers, c.sse2Fallback ? 1 : 0,
                    c.tasks ? "user" : "built-in", r.steps, r.sleepStep, r.transforms, r.events,
                    static_cast<unsigned long long>(r.state));
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string scene = "all";
    box2d_scenes::Config config;
    int steps = 600;
    bool json = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto value = [&]() -> std::optional<std::string_view> {
            if (i + 1 >= argc) return std::nullopt;
            return std::string_view(argv[++i]);
        };
        auto number = [&](int& out) {
            auto v = value();
            if (!v) return false;
            auto [end, error] = std::from_chars(v->data(), v->data() + v->size(), out);
            return error == std::errc{} && end == v->data() + v->size();
        };
        if (arg == "--scene") {
            auto v = value();
            if (!v || (*v != "hinges" && *v != "pyramid" && *v != "all")) return usage();
            scene = *v;
        } else if (arg == "--workers") {
            if (!number(config.workers) || config.workers < 1) return usage();
        } else if (arg == "--steps") {
            if (!number(steps) || steps < 1) return usage();
        } else if (arg == "--sse2") {
            config.sse2Fallback = true;
        } else if (arg == "--tasks") {
            config.tasks = true;
        } else if (arg == "--json") {
            json = true;
        } else {
            return usage();
        }
    }
    if (!json) {
        b2Version v = b2GetVersion();
        std::printf("Box2D %d.%d.%d, %s, AVX2 %s\n", v.major, v.minor, v.revision,
                    b2IsDoublePrecision() ? "double precision" : "single precision",
                    b2IsAVX2Available() ? "available" : "not available");
    }
    if (scene != "pyramid") print("falling-hinges", config, box2d_scenes::fallingHinges(config), json);
    if (scene != "hinges") print("pyramid", config, box2d_scenes::pyramid(config, steps), json);
    return 0;
}
