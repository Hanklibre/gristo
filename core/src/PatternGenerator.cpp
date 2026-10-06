#include "gristo/PatternGenerator.h"

#include "gristo/Random.h"

#include <algorithm>

namespace gristo {

namespace {
constexpr std::uint32_t kChaosStream = 0xC4A05u;
constexpr std::uint32_t kProbabilityStream = 0x9B0B0u;
constexpr std::uint32_t kMapXStream = 0x3A9F0u;
constexpr std::uint32_t kMapYStream = 0x3A9F1u;
constexpr float kMapRandomSpan = 0.5f;
constexpr std::uint32_t kFillRandomStream = 0xF111Au;
constexpr float kChaosDepth = 0.45f;
} // namespace

std::pair<float, float> PatternGenerator::mapPositionAt(const PatternParams& p, std::int64_t absoluteStep)
{
    if (p.mapRandom <= 0.0f)
        return { p.mapX, p.mapY };
    const std::int64_t bar = floorDiv(absoluteStep, kStepsPerBar);
    const float r = std::clamp(p.mapRandom, 0.0f, 1.0f) * kMapRandomSpan;
    const float x = p.mapX + r * rnd::bipolar(rnd::hash(p.seed, kMapXStream, bar));
    const float y = p.mapY + r * rnd::bipolar(rnd::hash(p.seed, kMapYStream, bar));
    return { std::clamp(x, 0.0f, 1.0f), std::clamp(y, 0.0f, 1.0f) };
}

float PatternGenerator::fillAt(const PatternParams& p, int voice, std::int64_t absoluteStep)
{
    const auto v = static_cast<size_t>(voice);
    float fill = p.fill[v];
    if (p.fillRandom[v] > 0.0f)
    {
        const std::int64_t bar = floorDiv(absoluteStep, kStepsPerBar);
        const float u = rnd::bipolar(rnd::hash(p.seed, kFillRandomStream + static_cast<std::uint32_t>(voice), bar));
        fill += std::clamp(p.fillRandom[v], 0.0f, 1.0f) * kFillRandomSpan * u;
    }
    return std::clamp(fill, 0.0f, 1.0f);
}

StepHits PatternGenerator::evaluate(const PatternParams& p, std::int64_t absoluteStep) const
{
    const int patternStep = floorMod(absoluteStep, kPatternSteps);
    const std::int64_t cycle = floorDiv(absoluteStep, kPatternSteps);
    const auto [mapX, mapY] = mapPositionAt(p, absoluteStep);

    StepHits hits {};
    for (int v = 0; v < kNumVoices; ++v)
    {
        float level = map_.level(v, patternStep, mapX, mapY);
        if (p.chaos > 0.0f)
        {
            const float n = rnd::bipolar(rnd::hash(p.seed, kChaosStream + static_cast<std::uint32_t>(v), cycle, patternStep));
            level = std::clamp(level + n * p.chaos * kChaosDepth, 0.0f, 1.0f);
        }
        VoiceHit& h = hits[static_cast<size_t>(v)];
        h.level = level;
        h.threshold = 1.0f - fillAt(p, v, absoluteStep);
        h.on = level > h.threshold && level > 0.0f && !p.mute[static_cast<size_t>(v)];

        const float probability = std::clamp(p.probability[static_cast<size_t>(v)], 0.0f, 1.0f);
        if (h.on && probability < 1.0f)
        {
            const float roll = rnd::unit(rnd::hash(p.seed, kProbabilityStream + static_cast<std::uint32_t>(v), absoluteStep));
            if (roll >= probability)
            {
                h.on = false;
                h.dropped = true;
            }
        }
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
