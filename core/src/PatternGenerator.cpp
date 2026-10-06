#include "gristo/PatternGenerator.h"

#include "gristo/Random.h"

#include <algorithm>

namespace gristo {

namespace {
constexpr std::uint32_t kChaosStream = 0xC4A05u;
constexpr float kChaosDepth = 0.45f;
} // namespace

StepHits PatternGenerator::evaluate(const PatternParams& p, std::int64_t absoluteStep) const
{
    const int patternStep = floorMod(absoluteStep, kPatternSteps);
    const std::int64_t cycle = floorDiv(absoluteStep, kPatternSteps);

    StepHits hits {};
    for (int v = 0; v < kNumVoices; ++v)
    {
        float level = map_.level(v, patternStep, p.mapX, p.mapY);
        if (p.chaos > 0.0f)
        {
            const float n = rnd::bipolar(rnd::hash(p.seed, kChaosStream + static_cast<std::uint32_t>(v), cycle, patternStep));
            level = std::clamp(level + n * p.chaos * kChaosDepth, 0.0f, 1.0f);
        }
        VoiceHit& h = hits[static_cast<size_t>(v)];
        h.level = level;
        h.threshold = 1.0f - std::clamp(p.fill[static_cast<size_t>(v)], 0.0f, 1.0f);
        h.on = level > h.threshold && level > 0.0f && !p.mute[static_cast<size_t>(v)];
        h.accent = h.on && level >= kAccentLevel;
    }

    if (p.linear)
        applyLinear(hits);
    return hits;
}

void PatternGenerator::applyLinear(StepHits& hits)
{
    auto silence = [&hits](int v) { hits[static_cast<size_t>(v)].on = hits[static_cast<size_t>(v)].accent = false; };

    if (hits[Snare].on)
    {
        for (int v : { Kick, HiHat1, HiHat2, Perc1, Perc2 })
            silence(v);
    }
    else if (hits[Kick].on)
    {
        for (int v : { HiHat1, HiHat2, Perc1, Perc2 })
            silence(v);
    }
    if (hits[HiHat2].on) silence(HiHat1);
    if (hits[Perc2].on) silence(Perc1);
}

} // namespace gristo
