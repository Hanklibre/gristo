#include "gristo/Modulation.h"

#include "gristo/Random.h"

#include <algorithm>
#include <cmath>

namespace gristo {

namespace mod {

bool decodeArpTarget(int target, int& arp, int& which)
{
    if (target < kArpBase || target >= kSoundBase)
        return false;
    arp = (target - kArpBase) / kArpTargetsPerArp;
    which = (target - kArpBase) % kArpTargetsPerArp;
    return true;
}

bool decodeSynthTarget(int target, int& synth, int& which)
{
    if (target < kSynthBase || target >= kFxSizeTarget)
        return false;
    synth = (target - kSynthBase) / kSynthTargets;
    which = (target - kSynthBase) % kSynthTargets;
    return true;
}

std::string targetName(int target)
{
    if (target >= kFill && target < kFill + kNumVoices) return std::string(voiceName(target - kFill)) + " Fill";
    if (target >= kProbability && target < kProbability + kNumVoices) return std::string(voiceName(target - kProbability)) + " Probability";
    if (target >= kVelocity && target < kVelocity + kNumVoices) return std::string(voiceName(target - kVelocity)) + " Velocity";
    int arp = 0, synth = 0, which = 0;
    if (decodeArpTarget(target, arp, which))
    {
        static const char* const names[kArpTargetsPerArp] = { "Density", "Accent", "Slide", "Gate", "Octaves",
                                                              "Length", "Transpose", "Velocity", "Probability" };
        return "Arp " + std::to_string(arp + 1) + " " + names[which];
    }
    if (decodeSynthTarget(target, synth, which))
    {
        static const char* const names[kSynthTargets] = { "Cutoff", "Resonance", "Env", "Attack", "Release", "Level", "Reverb", "Delay" };
        const int owner = arpOfSynth(synth);
        return (owner < 0 ? std::string("Chords") : "Arp " + std::to_string(owner + 1)) + " " + names[which];
    }
    if (target >= kSoundBase && target < kSoundBase + kKitTargets)
    {
        static const char* const kit[kKitTargets] = { "Tune", "Decay", "Tone", "Drive", "Reverb", "Delay" };
        return std::string("Kit ") + kit[target - kSoundBase];
    }
    switch (target)
    {
        case kMapX: return "Map X";
        case kMapY: return "Map Y";
        case kChaos: return "Chaos";
        case kShuffle: return "Shuffle";
        case kFlamMs: return "Flam ms";
        case kHumanize: return "Humanize";
        case kFxSizeTarget: return "Reverb Size";
        case kFxFeedbackTarget: return "Delay Feedback";
        default: return "None";
    }
}

float targetRange(int target)
{
    if (target == kitTarget(kKitTune))
        return 24.0f; // the Tune knob: -12..12 semitones
    if (target == kFxFeedbackTarget)
        return 0.9f;
    int arp = 0, which = 0;
    if (decodeArpTarget(target, arp, which))
    {
        switch (which)
        {
            case kArpGate: return 7.75f;              // the Gate knob: 0.25 .. 8 16ths
            case kArpOctaves: return 3.0f;            // 1 .. 4
            case kArpLength: return static_cast<float>(kMaxArpSteps - 1);
            case kArpTranspose: return 12.0f;         // semitones
            default: return 1.0f;
        }
    }
    switch (target)
    {
        case kShuffle: return 6.0f;
        case kFlamMs: return 30.0f;
        case kNone: return 0.0f;
        default: return 1.0f;
    }
}

} // namespace mod

const char* laneWaveName(LaneWave wave)
{
    switch (wave)
    {
        case LaneWave::Drawn: return "Drawn";
        case LaneWave::Sine: return "LFO sine";
        case LaneWave::Triangle: return "LFO triangle";
        case LaneWave::RampUp: return "LFO ramp up";
        case LaneWave::RampDown: return "LFO ramp down";
        case LaneWave::Square: return "LFO square";
        case LaneWave::Random: return "LFO random";
        case LaneWave::Count: break;
    }
    return "";
}

const char* lfoRateName(int rate)
{
    static const char* const names[kNumLfoRates] = { "1/8", "1/4", "1/2", "1 bar", "2 bars", "4 bars", "8 bars" };
    return names[std::clamp(rate, 0, kNumLfoRates - 1)];
}

float ModLane::lfoAt(std::int64_t absoluteStep) const
{
    const int cycle = kLfoCycleSteps[std::clamp(lfoRate, 0, kNumLfoRates - 1)];
    const auto index = floorMod(absoluteStep, cycle);
    const float x = static_cast<float>(index) / static_cast<float>(cycle); // 0 .. <1
    switch (wave)
    {
        case LaneWave::Sine: return 0.5f - 0.5f * std::cos(6.28318530718f * x);
        case LaneWave::Triangle: return 1.0f - std::abs(2.0f * x - 1.0f);
        case LaneWave::RampUp: return cycle > 1 ? static_cast<float>(index) / static_cast<float>(cycle - 1) : 1.0f;
        case LaneWave::RampDown: return cycle > 1 ? 1.0f - static_cast<float>(index) / static_cast<float>(cycle - 1) : 1.0f;
        case LaneWave::Square: return x < 0.5f ? 1.0f : 0.0f;
        case LaneWave::Random: // a new value every cycle, the same on every playthrough
            return rnd::unit(rnd::hash(0x1f0u, static_cast<std::uint32_t>(lfoRate), floorDiv(absoluteStep, cycle)));
        default: return 0.0f;
    }
}

const char* modShapeName(ModShape shape)
{
    switch (shape)
    {
        case ModShape::Off: return "Off";
        case ModShape::Full: return "Full";
        case ModShape::RampUp: return "Ramp up";
        case ModShape::RampDown: return "Ramp down";
        case ModShape::Triangle: return "Triangle";
        case ModShape::Sine: return "Sine";
        case ModShape::Square: return "Square (2nd half of each bar)";
        case ModShape::BeatSaw: return "Saw per beat";
        case ModShape::Offbeats: return "Offbeat 8ths";
        case ModShape::Random: return "Random steps";
        case ModShape::Count: break;
    }
    return "";
}

void fillModShape(float* points, int numPoints, ModShape shape, std::uint32_t seed)
{
    if (numPoints <= 0)
        return;
    const float n = static_cast<float>(numPoints);
    for (int i = 0; i < numPoints; ++i)
    {
        const float x = static_cast<float>(i) / n; // 0 .. <1 over the lane
        float v = 0.0f;
        switch (shape)
        {
            case ModShape::Off: v = 0.0f; break;
            case ModShape::Full: v = 1.0f; break;
            case ModShape::RampUp: v = numPoints > 1 ? static_cast<float>(i) / (n - 1.0f) : 1.0f; break;
            case ModShape::RampDown: v = numPoints > 1 ? 1.0f - static_cast<float>(i) / (n - 1.0f) : 1.0f; break;
            case ModShape::Triangle: v = 1.0f - std::abs(2.0f * x - 1.0f); break;
            case ModShape::Sine: v = 0.5f - 0.5f * std::cos(6.28318530718f * x); break;
            case ModShape::Square: v = (i % kStepsPerBar) >= kStepsPerBar / 2 ? 1.0f : 0.0f; break;
            case ModShape::BeatSaw: v = static_cast<float>(i % kStepsPerBeat) / static_cast<float>(kStepsPerBeat - 1); break;
            case ModShape::Offbeats: v = (i % kStepsPerBeat) == 2 ? 1.0f : 0.0f; break;
            case ModShape::Random: v = std::round(rnd::unit(rnd::hash(seed, 0xA11u, i)) * 8.0f) / 8.0f; break;
            case ModShape::Count: break;
        }
        points[i] = std::clamp(v, 0.0f, 1.0f);
    }
}

} // namespace gristo
