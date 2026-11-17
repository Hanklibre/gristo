#pragma once

#include "gristo/DrumMap.h"

#include <utility>

namespace gristo {

// Level at or above which a hit counts as accented (Grids uses 192/255).
constexpr float kAccentLevel = 0.75f;

constexpr int kMaxClockDivide = 8;

// The step of a drum's own loop at an absolute step, given its Length and clock
// divider; -1 when its clock doesn't tick on that step.
inline int voiceLoopStep(std::int64_t absoluteStep, int length, int divide)
{
    divide = divide < 1 ? 1 : divide > kMaxClockDivide ? kMaxClockDivide : divide;
    if (floorMod(absoluteStep, divide) != 0)
        return -1;
    length = length < 1 ? 1 : length > kPatternSteps ? kPatternSteps : length;
    return floorMod(floorDiv(absoluteStep, divide), length);
}

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
    // Polymeter: a voice loops its first `length` steps (kPatternSteps = the whole pattern).
    std::array<int, kNumVoices> length { kPatternSteps, kPatternSteps, kPatternSteps, kPatternSteps, kPatternSteps, kPatternSteps };
    // Clock divider per drum: it plays only on every Nth 16th and walks through its own
    // pattern (and Length, and hand edits) that many times slower. 1 = every 16th.
    std::array<int, kNumVoices> divide { 1, 1, 1, 1, 1, 1 };
    // Elektron-style trig condition per voice: 0 = every bar, 1 = 1st of 2 bars,
    // 2 = 2nd of 2, 3 = 1st of 4, 4 = 4th of 4 (a fill every four bars).
    std::array<int, kNumVoices> condition {};
    // Hand edits per drum and pattern step (0 = as generated, 1 = a hit, 2 = always a rest),
    // counted in the drum's own loop. They win over the map, Fill, Linear and the trig
    // condition; a hand-placed hit still answers to the drum's Probability and Mute.
    std::array<std::array<std::int8_t, kPatternSteps>, kNumVoices> edits {};
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
