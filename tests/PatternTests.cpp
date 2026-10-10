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

TEST("probability: 1 changes nothing, 0 silences the voice")
{
    DrumMap map;
    PatternGenerator gen(map);
    PatternParams base;
    base.fill.fill(0.8f);
    PatternParams zero = base;
    zero.probability[HiHat1] = 0.0f;
    for (std::int64_t s = 0; s < kPatternSteps * 4; ++s)
    {
        const StepHits a = gen.evaluate(base, s);
        const StepHits b = gen.evaluate(zero, s);
        for (int v = 0; v < kNumVoices; ++v)
        {
            CHECK(!a[static_cast<size_t>(v)].dropped);
            if (v == HiHat1)
            {
                CHECK(!b[HiHat1].on);
                CHECK(b[HiHat1].dropped == a[HiHat1].on);
            }
            else
            {
                CHECK(a[static_cast<size_t>(v)].on == b[static_cast<size_t>(v)].on);
            }
        }
    }
}

TEST("probability: plays roughly the requested share, deterministically, varying per bar")
{
    DrumMap map;
    PatternGenerator gen(map);
    PatternParams p;
    p.fill.fill(0.9f);
    p.probability.fill(0.5f);
    int eligible = 0, played = 0, differsAcrossCycles = 0;
    for (std::int64_t s = 0; s < kPatternSteps * 200; ++s)
    {
        const StepHits h = gen.evaluate(p, s);
        const StepHits again = gen.evaluate(p, s);
        const StepHits nextCycle = gen.evaluate(p, s + kPatternSteps);
        for (int v = 0; v < kNumVoices; ++v)
        {
            const auto& hit = h[static_cast<size_t>(v)];
            CHECK(hit.on == again[static_cast<size_t>(v)].on);
            CHECK(!(hit.on && hit.dropped));
            if (hit.on || hit.dropped)
            {
                ++eligible;
                played += hit.on ? 1 : 0;
                differsAcrossCycles += hit.on != nextCycle[static_cast<size_t>(v)].on ? 1 : 0;
            }
        }
    }
    const double share = played / double(eligible);
    CHECK_MSG(share > 0.45 && share < 0.55, "share " << share);
    CHECK(differsAcrossCycles > eligible / 4);
}

TEST("probability: a dropped hit frees its slot in linear mode")
{
    DrumMap map;
    PatternGenerator gen(map);
    PatternParams dropped;
    dropped.linear = true;
    dropped.fill.fill(0.9f);
    dropped.probability[Snare] = 0.0f;
    PatternParams muted = dropped;
    muted.probability[Snare] = 1.0f;
    muted.mute[Snare] = true;
    for (std::int64_t s = 0; s < kPatternSteps; ++s)
    {
        const StepHits a = gen.evaluate(dropped, s);
        const StepHits b = gen.evaluate(muted, s);
        for (int v = 0; v < kNumVoices; ++v)
            CHECK(a[static_cast<size_t>(v)].on == b[static_cast<size_t>(v)].on);
    }
}

TEST("random: map random is held per bar, bounded, and changes the pattern")
{
    DrumMap map;
    PatternGenerator gen(map);
    PatternParams p;
    p.mapX = 0.5f;
    p.mapY = 0.5f;
    p.fill.fill(0.7f);
    CHECK(PatternGenerator::mapPositionAt(p, 123).first == 0.5f);

    p.mapRandom = 1.0f;
    int moved = 0;
    for (std::int64_t bar = 0; bar < 64; ++bar)
    {
        const auto pos = PatternGenerator::mapPositionAt(p, bar * kStepsPerBar);
        CHECK(pos.first >= 0.0f && pos.first <= 1.0f && pos.second >= 0.0f && pos.second <= 1.0f);
        for (int s = 1; s < kStepsPerBar; ++s)
            CHECK(PatternGenerator::mapPositionAt(p, bar * kStepsPerBar + s) == pos);
        moved += std::abs(pos.first - 0.5f) > 0.05f ? 1 : 0;
    }
    CHECK(moved > 40);

    PatternParams still = p;
    still.mapRandom = 0.0f;
    int differences = 0;
    for (std::int64_t s = 0; s < kPatternSteps * 8; ++s)
    {
        const StepHits a = gen.evaluate(p, s);
        const StepHits b = gen.evaluate(still, s);
        for (int v = 0; v < kNumVoices; ++v)
            differences += a[static_cast<size_t>(v)].on != b[static_cast<size_t>(v)].on ? 1 : 0;
    }
    CHECK(differences > 0);
}

TEST("random: fill random is held per bar per voice and stays in range")
{
    DrumMap map;
    PatternGenerator gen(map);
    PatternParams p;
    p.fill.fill(0.5f);
    CHECK(PatternGenerator::fillAt(p, Kick, 5) == 0.5f);
    p.fillRandom[HiHat1] = 1.0f;
    int barsDiffer = 0;
    for (std::int64_t bar = 0; bar < 64; ++bar)
    {
        const float f = PatternGenerator::fillAt(p, HiHat1, bar * kStepsPerBar);
        CHECK(std::abs(f - 0.5f) <= PatternGenerator::kFillRandomSpan + 1e-6f);
        for (int s = 1; s < kStepsPerBar; ++s)
            CHECK(PatternGenerator::fillAt(p, HiHat1, bar * kStepsPerBar + s) == f);
        CHECK(PatternGenerator::fillAt(p, Kick, bar * kStepsPerBar) == 0.5f); // other voices untouched
        barsDiffer += std::abs(f - PatternGenerator::fillAt(p, HiHat1, (bar + 1) * kStepsPerBar)) > 0.05f ? 1 : 0;
    }
    CHECK(barsDiffer > 40);

    // Density of the hats changes between bars.
    int minHits = 99, maxHits = 0;
    for (std::int64_t bar = 0; bar < 32; ++bar)
    {
        int hits = 0;
        for (int s = 0; s < kStepsPerBar; ++s)
            hits += gen.evaluate(p, bar * kStepsPerBar + s)[HiHat1].on ? 1 : 0;
        minHits = std::min(minHits, hits);
        maxHits = std::max(maxHits, hits);
    }
    CHECK(maxHits > minHits);
}

TEST("random: probability random is held per bar per voice")
{
    DrumMap map;
    PatternGenerator gen(map);
    PatternParams p;
    p.probability.fill(0.5f);
    p.probabilityRandom[Snare] = 1.0f;
    int differ = 0;
    for (std::int64_t bar = 0; bar < 64; ++bar)
    {
        const float pr = PatternGenerator::probabilityAt(p, Snare, bar * kStepsPerBar);
        CHECK(std::abs(pr - 0.5f) <= PatternGenerator::kProbabilityRandomSpan + 1e-6f);
        CHECK(PatternGenerator::probabilityAt(p, Snare, bar * kStepsPerBar + 9) == pr);
        CHECK(PatternGenerator::probabilityAt(p, Kick, bar * kStepsPerBar) == 0.5f);
        differ += std::abs(pr - PatternGenerator::probabilityAt(p, Snare, (bar + 1) * kStepsPerBar)) > 0.05f ? 1 : 0;
    }
    CHECK(differ > 40);
}
