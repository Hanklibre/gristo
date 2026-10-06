#include "gristo/Humanizer.h"

#include "gristo/Random.h"

#include <algorithm>
#include <cmath>

namespace gristo {

namespace {

// Timing deviation budget at amount = 1, in milliseconds.
constexpr double kEnsembleDriftMs = 4.0; // whole kit breathes together (per bar)
constexpr double kLimbDriftMs = 2.5;     // hands vs. feet drift independently (per 2 beats)
constexpr double kJitterMs = 4.0;        // per-hit scatter, scaled by structural weight

// Weak notes exaggerate the groove's own displacement by up to this fraction.
constexpr double kGrooveFollow = 0.25;

constexpr float kVelocityJitter = 0.10f;
constexpr float kVelocityDrift = 0.05f;
constexpr float kHandHierarchy = 0.07f;

enum Stream : std::uint32_t
{
    kEnsemble = 0x101,
    kFeet = 0x102,
    kHands = 0x103,
    kJitter = 0x110,
    kVelocity = 0x120,
    kVelocityDriftStream = 0x130,
};

bool isHand(int voice) { return voice != Kick; }

} // namespace

float Humanizer::structuralWeight(int voice, int stepInBar, float level, bool accent)
{
    float metric = metricWeight(stepInBar);
    // Backbeats are anchors for the snare even though they are "beat 2 and 4".
    if (voice == Snare && (stepInBar % kStepsPerBar == 4 || stepInBar % kStepsPerBar == 12))
        metric = std::max(metric, 0.85f);
    const float strength = accent ? 1.0f : std::clamp(level, 0.0f, 1.0f);
    return std::clamp(0.5f * metric + 0.5f * strength, 0.0f, 1.0f);
}

HumanizeResult Humanizer::apply(const HumanizeParams& p, const HumanizeInput& in)
{
    HumanizeResult r;
    const float voiceScale = (in.voice >= 0 && in.voice < kNumVoices) ? p.voiceAmount[static_cast<size_t>(in.voice)] : 1.0f;
    const float amount = std::clamp(p.amount * voiceScale, 0.0f, 1.0f);
    if (amount <= 0.0f)
        return r;

    const int stepInBar = floorMod(in.step, kStepsPerBar);
    const float w = structuralWeight(in.voice, stepInBar, in.level, in.accent);
    const float looseness = 1.0f - 0.65f * w; // anchors keep ~35% of the jitter

    // In lock mode every random lookup is keyed by the position inside the 2-bar
    // pattern, and the drift noise repeats every pattern length.
    const std::int64_t key = p.lock ? floorMod(in.step, kPatternSteps) : in.step;
    constexpr double kPatternBeats = kPatternSteps / double(kStepsPerBeat);
    double b = in.nominalBeats;
    if (p.lock)
    {
        b = std::fmod(b, kPatternBeats);
        if (b < 0.0) b += kPatternBeats;
    }
    // Drift lattices: one point per bar (ensemble) and per two beats (limbs / velocity).
    const int barsPeriod = p.lock ? static_cast<int>(kPatternBeats / 4.0) : 0;
    const int halfBarPeriod = p.lock ? static_cast<int>(kPatternBeats / 2.0) : 0;

    const auto v = static_cast<std::uint32_t>(in.voice);

    // --- timing -----------------------------------------------------------------
    const double ensemble = rnd::smoothNoise(p.seed, kEnsemble, b / 4.0, barsPeriod);
    const double limb = rnd::smoothNoise(p.seed, isHand(in.voice) ? kHands : kFeet, b / 2.0, halfBarPeriod);
    const double jitter = rnd::triangular(p.seed, kJitter + v, key);

    // Other tracks in the DAW are usually on the grid, so anchors also take less of
    // the drift: they are the time reference the rest of the kit moves around.
    const double drift = (kEnsembleDriftMs * ensemble + kLimbDriftMs * limb) * (0.4 + 0.6 * looseness);
    const double ms = amount * (drift + kJitterMs * looseness * jitter);
    double offset = ms * in.bpm / 60000.0;

    // Lean into the groove: weak notes go further where the groove already moved them.
    offset += amount * kGrooveFollow * (1.0f - w) * (in.gridBeats - in.nominalBeats);

    const double cap = kMaxGapFraction * std::max(0.0, std::min(in.prevGapBeats, in.nextGapBeats));
    r.offsetBeats = std::clamp(offset, -cap, cap);

    // --- velocity ---------------------------------------------------------------
    float scale = 1.0f;
    scale += amount * kVelocityJitter * (1.0f - 0.5f * w) * rnd::triangular(p.seed, kVelocity + v, key);
    scale += amount * kVelocityDrift * rnd::smoothNoise(p.seed, kVelocityDriftStream + v, b / 2.0, halfBarPeriod);
    if (isHand(in.voice) && in.voice != Snare)
        scale += amount * kHandHierarchy * (metricWeight(stepInBar) - 0.5f);
    r.velocityScale = std::clamp(scale, 0.5f, 1.3f);
    return r;
}

} // namespace gristo
