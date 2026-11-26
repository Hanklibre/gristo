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

// Where the drums come from: the map (Grids-style, with Fill), or Circles: a ring of
// steps per drum that you set by hand (after Patterning), each step with its own
// velocity, chance, nudge and repeats.
enum class PatternMode : int { Map = 0, Circles };

// One step of a circle. Nudge in 24ths of a step (+-12: half a step early or late);
// repeat 1..4 (a ratchet across the step); velocity 1..127; chance 0..100 %.
struct CircleStep
{
    bool on = false;
    int velocity = 100;
    int chance = 100;
    int nudge = 0;
    int repeat = 1;
    bool operator==(const CircleStep& o) const
    {
        return on == o.on && velocity == o.velocity && chance == o.chance && nudge == o.nudge && repeat == o.repeat;
    }
};
constexpr int kMaxNudge = 12;
constexpr int kMaxRepeat = 4;
// Packed into one int (24 bits, so it also survives a float): parameters can't hold
// these, so the plugin stores them alongside, as it does the hand edits.
inline int encodeCircleStep(const CircleStep& s)
{
    auto clampi = [](int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; };
    if (s == CircleStep {})
        return 0; // an empty step is 0
    return (s.on ? 1 : 0) | (clampi(s.velocity, 1, 127) << 1) | (clampi(s.chance, 0, 100) << 8)
           | ((clampi(s.nudge, -kMaxNudge, kMaxNudge) + 64) << 15) | ((clampi(s.repeat, 1, kMaxRepeat) - 1) << 22);
}
inline CircleStep decodeCircleStep(int code)
{
    CircleStep s;
    if (code == 0)
        return s; // empty: off, at the defaults
    s.on = (code & 1) != 0;
    s.velocity = (code >> 1) & 127;
    s.chance = (code >> 8) & 127;
    s.nudge = ((code >> 15) & 127) - 64;
    s.repeat = ((code >> 22) & 3) + 1;
    if (s.velocity < 1) s.velocity = 1;
    if (s.chance > 100) s.chance = 100;
    if (s.nudge < -kMaxNudge || s.nudge > kMaxNudge) s.nudge = 0;
    return s;
}
// Euclidean rhythm: `hits` spread as evenly as they go over `steps`, turned by `rotate`.
inline bool euclidHit(int step, int hits, int steps, int rotate)
{
    if (steps <= 0 || hits <= 0)
        return false;
    if (hits >= steps)
        return true;
    const int k = floorMod(step - rotate, steps);
    return (k * hits) % steps < hits;
}

struct PatternParams
{
    PatternMode mode = PatternMode::Map;
    // Circles: per drum, its steps (encodeCircleStep), counted in the drum's own loop
    // (Length steps, at its clock divider's rate).
    std::array<std::array<std::int32_t, kPatternSteps>, kNumVoices> circles {};
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
    // Circles: the step's own velocity (0..1; < 0 = from the level), nudge (in steps)
    // and repeats (0 = the voice's Ratchet).
    float velocity = -1.0f;
    float nudgeSteps = 0.0f;
    int repeat = 0;
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
    // Circles mode: each drum's ring of steps.
    static StepHits evaluateCircles(const PatternParams& params, std::int64_t absoluteStep);

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
