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
// The first two arps' targets sit in the original blocks below; arps added later
// (the 3rd) get a block of their own after the FX targets, so saved routings keep
// their meaning.
constexpr int kLegacyArps = 2;

// Built-in synth cutoff per arp (added after the arp targets).
constexpr int kArpCutoffBase = kArpBase + kLegacyArps * kArpTargetsPerArp;

// Arp probability (added later still, for the same reason).
constexpr int kArpProbBase = kArpCutoffBase + kLegacyArps;

// Built-in sounds (appended for the same reason). Sound-engine synths: 0, 1 = arps 1
// and 2, 2 = chords, 3 = arp 3.
constexpr int kSoundBase = kArpProbBase + kLegacyArps;
constexpr int kKitTune = 0, kKitDecay = 1, kKitTone = 2, kKitDrive = 3, kKitReverb = 4, kKitDelay = 5, kKitTargets = 6;
constexpr int kSynthReso = 0, kSynthEnv = 1, kSynthAttack = 2, kSynthRelease = 3, kSynthLevel = 4, kSynthReverb = 5,
              kSynthDelay = 6, kSynthTargets = 7;
constexpr int kNumSoundSynths = 3; // synths with targets in the original sound block
constexpr int kitTarget(int which) { return kSoundBase + which; }
constexpr int kChordCutoffTarget = kSoundBase + kKitTargets + kNumSoundSynths * kSynthTargets;
constexpr int kFxSizeTarget = kChordCutoffTarget + 1;
constexpr int kFxFeedbackTarget = kChordCutoffTarget + 2;

// Arps from the 3rd on: arp targets, then cutoff, probability and their synth's targets.
constexpr int kExtraArpBase = kFxFeedbackTarget + 1;
constexpr int kExtraCutoff = kArpTargetsPerArp, kExtraProb = kArpTargetsPerArp + 1, kExtraSynth = kArpTargetsPerArp + 2;
constexpr int kExtraArpTargets = kExtraSynth + kSynthTargets;
constexpr int extraArpBase(int arp) { return kExtraArpBase + (arp - kLegacyArps) * kExtraArpTargets; }

// Sound-engine synth of an arp, and the arp of a synth (-1 for the chords).
constexpr int synthOfArp(int arp) { return arp < kLegacyArps ? arp : arp + 1; }
constexpr int arpOfSynth(int synth) { return synth < kLegacyArps ? synth : synth == kLegacyArps ? -1 : synth - 1; }

constexpr int arpTarget(int arp, int which)
{
    return arp < kLegacyArps ? kArpBase + arp * kArpTargetsPerArp + which : extraArpBase(arp) + which;
}
constexpr int arpCutoffTarget(int arp) { return arp < kLegacyArps ? kArpCutoffBase + arp : extraArpBase(arp) + kExtraCutoff; }
constexpr int arpProbTarget(int arp) { return arp < kLegacyArps ? kArpProbBase + arp : extraArpBase(arp) + kExtraProb; }
constexpr int synthTarget(int synth, int which)
{
    return synth <= kLegacyArps ? kSoundBase + kKitTargets + synth * kSynthTargets + which
                                : extraArpBase(arpOfSynth(synth)) + kExtraSynth + which;
}

constexpr int kNumTargets = kExtraArpBase + (kNumArps - kLegacyArps) * kExtraArpTargets;

// Which arp target a target is: arp index and one of kArpDensity..kArpVelocity,
// kExtraCutoff or kExtraProb. False for anything else.
bool decodeArpTarget(int target, int& arp, int& which);
// Which built-in synth target: sound-engine synth index and kSynthReso..kSynthDelay.
bool decodeSynthTarget(int target, int& synth, int& which);

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
