#pragma once

#include "gristo/DrumMap.h"

#include <utility>

namespace gristo {

// Level at or above which a hit counts as accented (Grids uses 192/255).
constexpr float kAccentLevel = 0.75f;

struct PatternParams
{
    float mapX = 0.5f;
    float mapY = 0.5f;
    float chaos = 0.0f; // 0..1, perturbs levels, re-rolled every pattern cycle
    // Beads-style random modulation of the map position, sampled and held per bar:
    // X and Y each move up to +-0.5 * mapRandom around (mapX, mapY).
    float mapRandom = 0.0f;
    std::array<float, kNumVoices> fill { 0.6f, 0.55f, 0.6f, 0.35f, 0.4f, 0.35f };
    // Beads-style random modulation of Fill, per voice, sampled and held per bar:
    // Fill moves up to +-0.35 * fillRandom.
    std::array<float, kNumVoices> fillRandom {};
    // Chance (0..1) that a hit which passes Fill actually plays. Rolled per absolute
    // step, so every bar differs but a re-render is identical.
    std::array<float, kNumVoices> probability { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    // Beads-style random modulation of Probability, per voice, held per bar: +-0.5 x r.
    std::array<float, kNumVoices> probabilityRandom {};
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
    bool dropped = false;   // passed Fill but was removed by Probability
};

using StepHits = std::array<VoiceHit, kNumVoices>;

// "What plays": reads the map at (X, Y), then applies Chaos, Fill, mutes, Probability
// and Linear mode, in that order. Probability runs before Linear so a dropped hit
// frees its slot for the next voice in the priority chain.
class PatternGenerator
{
public:
    explicit PatternGenerator(const DrumMap& map) : map_(map) {}

    // absoluteStep counts sixteenths from the host's song start (ppq 0 = step 0).
    StepHits evaluate(const PatternParams& params, std::int64_t absoluteStep) const;

    static void applyLinear(StepHits& hits);

    // Fill in effect for a voice in the bar containing absoluteStep (after Fill Random).
    static float fillAt(const PatternParams& params, int voice, std::int64_t absoluteStep);

    static constexpr float kFillRandomSpan = 0.35f;

    // Probability in effect for a voice in the bar containing absoluteStep.
    static float probabilityAt(const PatternParams& params, int voice, std::int64_t absoluteStep);

    static constexpr float kProbabilityRandomSpan = 0.5f;

    // Map position in effect for the bar containing absoluteStep (after Map Random).
    static std::pair<float, float> mapPositionAt(const PatternParams& params, std::int64_t absoluteStep);

private:
    const DrumMap& map_;
};

} // namespace gristo
