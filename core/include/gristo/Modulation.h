#pragma once

#include "gristo/Arp.h"
#include "gristo/Types.h"

#include <algorithm>
#include <string>

namespace gristo {

// Massive-style "performer" modulation: drawn step curves (one value per 16th) that
// loop over 1, 2 or 4 bars and are routed to parameters with bipolar amounts.

constexpr int kNumModLanes = 4;
constexpr int kModSlotsPerLane = 4;
constexpr int kModPointsPerBar = kStepsPerBar;
constexpr int kMaxModBars = 4;
constexpr int kMaxModPoints = kModPointsPerBar * kMaxModBars;

// Modulation targets. Per-voice targets are base + voice index.
namespace mod {
constexpr int kNone = 0;
constexpr int kFill = 1;         // + voice, 6 targets
constexpr int kProbability = 7;  // + voice
constexpr int kVelocity = 13;    // + voice
constexpr int kMapX = 19;
constexpr int kMapY = 20;
constexpr int kChaos = 21;
constexpr int kShuffle = 22;
constexpr int kFlamMs = 23;
constexpr int kHumanize = 24;

// Arp targets: kArpBase + arp * kArpTargetsPerArp + one of the offsets below.
constexpr int kArpBase = 25;
constexpr int kArpDensity = 0;
constexpr int kArpAccent = 1;
constexpr int kArpSlide = 2;
constexpr int kArpGate = 3;
constexpr int kArpOctaves = 4;
constexpr int kArpLength = 5;
constexpr int kArpTranspose = 6;
constexpr int kArpVelocity = 7;
constexpr int kArpTargetsPerArp = 8;
constexpr int arpTarget(int arp, int which) { return kArpBase + arp * kArpTargetsPerArp + which; }

// Built-in synth cutoff per arp (added after the arp targets, so saved routings keep their meaning).
constexpr int kArpCutoffBase = kArpBase + kNumArps * kArpTargetsPerArp;
constexpr int arpCutoffTarget(int arp) { return kArpCutoffBase + arp; }

// Arp probability (added later still, for the same reason).
constexpr int kArpProbBase = kArpCutoffBase + kNumArps;
constexpr int arpProbTarget(int arp) { return kArpProbBase + arp; }

// Built-in sounds (appended for the same reason). Synths: 0, 1 = arps, 2 = chords.
constexpr int kSoundBase = kArpProbBase + kNumArps;
constexpr int kKitTune = 0, kKitDecay = 1, kKitTone = 2, kKitDrive = 3, kKitReverb = 4, kKitDelay = 5, kKitTargets = 6;
constexpr int kSynthReso = 0, kSynthEnv = 1, kSynthAttack = 2, kSynthRelease = 3, kSynthLevel = 4, kSynthReverb = 5,
              kSynthDelay = 6, kSynthTargets = 7;
constexpr int kNumSoundSynths = 3;
constexpr int kitTarget(int which) { return kSoundBase + which; }
constexpr int synthTarget(int synth, int which) { return kSoundBase + kKitTargets + synth * kSynthTargets + which; }
constexpr int kChordCutoffTarget = kSoundBase + kKitTargets + kNumSoundSynths * kSynthTargets;
constexpr int kFxSizeTarget = kChordCutoffTarget + 1;
constexpr int kFxFeedbackTarget = kChordCutoffTarget + 2;

constexpr int kNumTargets = kFxFeedbackTarget + 1;

std::string targetName(int target);

// Full-scale modulation range of a target in its own units (amount 100 % with a
// lane at 1 moves the parameter by this much). Equals the parameter's knob range.
float targetRange(int target);
} // namespace mod

struct ModSlot
{
    int target = mod::kNone;
    float amount = 0.0f; // -1..1
};

struct ModLane
{
    std::array<float, kMaxModPoints> points {}; // 0..1, one per 16th
    int bars = 1;                               // 1, 2 or 4
    std::array<ModSlot, kModSlotsPerLane> slots {};

    int numPoints() const { return std::clamp(bars, 1, kMaxModBars) * kModPointsPerBar; }
    float valueAt(std::int64_t absoluteStep) const { return points[static_cast<size_t>(floorMod(absoluteStep, numPoints()))]; }
};

struct ModulationParams
{
    std::array<ModLane, kNumModLanes> lanes {};
};

// Quick shapes for the lane editor (and the render tool).
enum class ModShape : int
{
    Off = 0,
    Full,
    RampUp,
    RampDown,
    Triangle,
    Sine,
    Square,
    BeatSaw,
    Offbeats,
    Random,
    Count
};

const char* modShapeName(ModShape shape);
void fillModShape(float* points, int numPoints, ModShape shape, std::uint32_t seed = 0x5eedu);

} // namespace gristo
