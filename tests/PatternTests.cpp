#include "TestFramework.h"

#include "gristo/PatternGenerator.h"

using namespace gristo;

TEST("map: corner coordinates return node data exactly")
{
    DrumMap map;
    const int w = map.width(), h = map.height();
    for (int v = 0; v < kNumVoices; ++v)
        for (int s = 0; s < kPatternSteps; ++s)
        {
            CHECK_NEAR(map.level(v, s, 0.0f, 0.0f), map.nodeLevel(0, 0, v, s) / 255.0, 1e-6);
            CHECK_NEAR(map.level(v, s, 1.0f, 1.0f), map.nodeLevel(w - 1, h - 1, v, s) / 255.0, 1e-6);
            const float nx = 2.0f / static_cast<float>(w - 1);
            CHECK_NEAR(map.level(v, s, nx, 0.0f), map.nodeLevel(2, 0, v, s) / 255.0, 1e-5);
        }
}

TEST("map: every node has a name from the corpus")
{
    DrumMap map;
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x)
            CHECK(map.nodeName(x, y) != nullptr);
}

TEST("pattern: fill 0 plays nothing, fill 1 plays every non-zero level")
{
    DrumMap map;
    PatternGenerator gen(map);
    PatternParams p;
    p.fill.fill(0.0f);
    for (int s = 0; s < kPatternSteps; ++s)
        for (const auto& h : gen.evaluate(p, s))
            CHECK(!h.on);

    p.fill.fill(1.0f);
    for (int s = 0; s < kPatternSteps; ++s)
    {
        const StepHits hits = gen.evaluate(p, s);
        for (int v = 0; v < kNumVoices; ++v)
            CHECK(hits[static_cast<size_t>(v)].on == (map.level(v, s, p.mapX, p.mapY) > 0.0f));
    }
}

TEST("pattern: density grows monotonically with fill")
{
    DrumMap map;
    PatternGenerator gen(map);
    PatternParams p;
    int previous = -1;
    for (float f = 0.0f; f <= 1.0f; f += 0.1f)
    {
        p.fill.fill(f);
        int count = 0;
        for (int s = 0; s < kPatternSteps; ++s)
            for (const auto& h : gen.evaluate(p, s))
                count += h.on ? 1 : 0;
        CHECK(count >= previous);
        previous = count;
    }
}

TEST("pattern: the map's patterns repeat every 32 steps without chaos")
{
    DrumMap map;
    PatternGenerator gen(map);
    PatternParams p;
    for (std::int64_t s = -64; s < 64; ++s)
    {
        const StepHits a = gen.evaluate(p, s);
        const StepHits b = gen.evaluate(p, s + kPatternSteps);
        for (int v = 0; v < kNumVoices; ++v)
            CHECK(a[static_cast<size_t>(v)].on == b[static_cast<size_t>(v)].on);
    }
}

TEST("pattern: chaos is deterministic and varies between cycles")
{
    DrumMap map;
    PatternGenerator gen(map);
    PatternParams p;
    p.chaos = 0.8f;
    p.fill.fill(0.5f);
    int differences = 0;
    for (int s = 0; s < kPatternSteps; ++s)
    {
        const StepHits a = gen.evaluate(p, s);
        const StepHits again = gen.evaluate(p, s);
        const StepHits nextCycle = gen.evaluate(p, s + kPatternSteps);
        for (int v = 0; v < kNumVoices; ++v)
        {
            CHECK(a[static_cast<size_t>(v)].on == again[static_cast<size_t>(v)].on);
            differences += a[static_cast<size_t>(v)].on != nextCycle[static_cast<size_t>(v)].on ? 1 : 0;
        }
    }
    CHECK(differences > 0);
}

TEST("pattern: mutes silence a voice")
{
    DrumMap map;
    PatternGenerator gen(map);
    PatternParams p;
    p.fill.fill(1.0f);
    p.mute[Snare] = true;
    for (int s = 0; s < kPatternSteps; ++s)
        CHECK(!gen.evaluate(p, s)[Snare].on);
}

TEST("pattern: linear mode enforces the priority chains")
{
    DrumMap map;
    PatternGenerator gen(map);
    PatternParams p;
    p.linear = true;
    p.fill.fill(1.0f);
    for (float x : { 0.0f, 0.3f, 0.7f, 1.0f })
        for (float y : { 0.0f, 0.5f, 1.0f })
        {
            p.mapX = x;
            p.mapY = y;
            for (int s = 0; s < kPatternSteps; ++s)
            {
                const StepHits h = gen.evaluate(p, s);
                if (h[Snare].on)
                    CHECK(!h[Kick].on && !h[HiHat1].on && !h[HiHat2].on && !h[Perc1].on && !h[Perc2].on);
                if (h[Kick].on)
                    CHECK(!h[HiHat1].on && !h[HiHat2].on && !h[Perc1].on && !h[Perc2].on);
                if (h[HiHat2].on) CHECK(!h[HiHat1].on);
                if (h[Perc2].on) CHECK(!h[Perc1].on);
            }
        }
}
