#pragma once

#include "gristo/Arp.h"
#include "gristo/Types.h"

#include <algorithm>
#include <string>

namespace gristo {

// Massive-style "performer" modulation: drawn step curves (one value per 16th) that
// loop over 1, 2 or 4 bars and are routed to parameters with bipolar amounts.

constexpr int kNumModLanes = 4;
// Macro knobs are modulation sources too: a "lane" that holds one value. Sources
// 0..3 are the drawn lanes, 4..11 the macros.
constexpr int kNumMacros = 8;
constexpr int kNumModSources = kNumModLanes + kNumMacros;
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

// Arp targets: arpTarget(arp, one of the offsets below).
constexpr int kArpBase = 25;
constexpr int kArpDensity = 0;
constexpr int kArpAccent = 1;
constexpr int kArpSlide = 2;
constexpr int kArpGate = 3;
constexpr int kArpOctaves = 4;
constexpr int kArpLength = 5;
constexpr int kArpTranspose = 6;
constexpr int kArpVelocity = 7;
constexpr int kArpProbability = 8;
constexpr int kArpTargetsPerArp = 9;
constexpr int arpTarget(int arp, int which) { return kArpBase + arp * kArpTargetsPerArp + which; }
constexpr int arpProbTarget(int arp) { return arpTarget(arp, kArpProbability); }

// Built-in sounds. Synths: one per arp, then the chords.
constexpr int kNumSoundSynths = kNumArps + 1;
constexpr int kChordSynthIndex = kNumArps;
constexpr int synthOfArp(int arp) { return arp; }
constexpr int arpOfSynth(int synth) { return synth < kNumArps ? synth : -1; } // -1: the chords

constexpr int kSoundBase = kArpBase + kNumArps * kArpTargetsPerArp;
constexpr int kKitTune = 0, kKitDecay = 1, kKitTone = 2, kKitDrive = 3, kKitReverb = 4, kKitDelay = 5, kKitTargets = 6;
constexpr int kitTarget(int which) { return kSoundBase + which; }
constexpr int kSynthCutoff = 0, kSynthReso = 1, kSynthEnv = 2, kSynthAttack = 3, kSynthRelease = 4, kSynthLevel = 5,
              kSynthReverb = 6, kSynthDelay = 7, kSynthTargets = 8;
constexpr int kSynthBase = kSoundBase + kKitTargets;
constexpr int synthTarget(int synth, int which) { return kSynthBase + synth * kSynthTargets + which; }
constexpr int arpCutoffTarget(int arp) { return synthTarget(synthOfArp(arp), kSynthCutoff); }
constexpr int kChordCutoffTarget = synthTarget(kChordSynthIndex, kSynthCutoff);
constexpr int kFxSizeTarget = kSynthBase + kNumSoundSynths * kSynthTargets;
constexpr int kFxFeedbackTarget = kFxSizeTarget + 1;

constexpr int kNumTargets = kFxFeedbackTarget + 1;

// Which arp target a target is: arp index and one of kArpDensity..kArpProbability.
// False for anything else.
bool decodeArpTarget(int target, int& arp, int& which);
// Which built-in synth target: sound-engine synth index and kSynthCutoff..kSynthDelay.
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

// A lane plays its drawn curve, or runs as a tempo-synced LFO.
enum class LaneWave : int { Drawn = 0, Sine, Triangle, RampUp, RampDown, Square, Random, Count };
const char* laneWaveName(LaneWave wave);
constexpr int kLfoCycleSteps[] = { 2, 4, 8, 16, 32, 64, 128 }; // 1/8, 1/4, 1/2 note, 1, 2, 4, 8 bars
constexpr int kNumLfoRates = 7;
const char* lfoRateName(int rate);

struct ModLane
{
    std::array<float, kMaxModPoints> points {}; // 0..1, one per 16th
    int bars = 1;                               // 1, 2 or 4
    std::array<ModSlot, kModSlotsPerLane> slots {};
    LaneWave wave = LaneWave::Drawn;
    int lfoRate = 3; // index into kLfoCycleSteps

    int numPoints() const { return std::clamp(bars, 1, kMaxModBars) * kModPointsPerBar; }
    float valueAt(std::int64_t absoluteStep) const
    {
        if (wave == LaneWave::Drawn)
            return points[static_cast<size_t>(floorMod(absoluteStep, numPoints()))];
        return lfoAt(absoluteStep);
    }
    float lfoAt(std::int64_t absoluteStep) const; // 0..1, sampled at the 16th
};

struct ModulationParams
{
    std::array<ModLane, kNumModSources> lanes {}; // drawn lanes, then the macros
    void setMacro(int macro, float value) { lanes[static_cast<size_t>(kNumModLanes + macro)].points.fill(std::clamp(value, 0.0f, 1.0f)); }
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
