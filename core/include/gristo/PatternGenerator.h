#pragma once

#include "gristo/DrumMap.h"

namespace gristo {

// Level at or above which a hit counts as accented (Grids uses 192/255).
constexpr float kAccentLevel = 0.75f;

struct PatternParams
{
    float mapX = 0.5f;
    float mapY = 0.5f;
    float chaos = 0.0f; // 0..1, perturbs levels, re-rolled every pattern cycle
    std::array<float, kNumVoices> fill { 0.6f, 0.55f, 0.6f, 0.35f, 0.4f, 0.35f };
    std::array<bool, kNumVoices> mute {};
    bool linear = false;
    std::uint32_t seed = 0x67726973u;
};

struct VoiceHit
{
    bool on = false;
    bool accent = false;
    float level = 0.0f;     // perturbed level, 0..1
    float threshold = 1.0f; // 1 - fill
};

using StepHits = std::array<VoiceHit, kNumVoices>;

// "What plays": reads the map at (X, Y), applies Chaos, Fill, mutes and Linear mode.
class PatternGenerator
{
public:
    explicit PatternGenerator(const DrumMap& map) : map_(map) {}

    // absoluteStep counts sixteenths from the host's song start (ppq 0 = step 0).
    StepHits evaluate(const PatternParams& params, std::int64_t absoluteStep) const;

    static void applyLinear(StepHits& hits);

private:
    const DrumMap& map_;
};

} // namespace gristo
