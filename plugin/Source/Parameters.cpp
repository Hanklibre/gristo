#include "Parameters.h"

namespace gristo::params {

namespace {

constexpr int kVersion = 1;

const int kDefaultNotes[kNumVoices] = { 36, 38, 42, 46, 39, 45 }; // GM: kick, snare, CH, OH, clap, low tom

float load(const juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    return s.getRawParameterValue(id)->load(std::memory_order_relaxed);
}

std::unique_ptr<juce::AudioParameterFloat> floatParam(const juce::String& id, const juce::String& name,
                                                      juce::NormalisableRange<float> range, float def,
                                                      const juce::String& label = {}, int decimals = 2)
{
    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel(label)
                          .withStringFromValueFunction([decimals](float v, int) { return juce::String(v, decimals); });
    return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, kVersion }, name, range, def, attributes);
}

std::unique_ptr<juce::AudioParameterBool> boolParam(const juce::String& id, const juce::String& name, bool def)
{
    return std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, kVersion }, name, def);
}

} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const PatternParams pattern;
    const DynamicsParams dyn;

    // Map
    layout.add(floatParam(mapX, "Map X", { 0.0f, 1.0f }, 0.5f));
    layout.add(floatParam(mapY, "Map Y", { 0.0f, 1.0f }, 0.5f));
    layout.add(floatParam(chaos, "Chaos", { 0.0f, 1.0f }, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { variation, kVersion }, "Variation", 0, 99, 0));

    // Voices
    for (int v = 0; v < kNumVoices; ++v)
    {
        const juce::String name = voiceName(v);
        layout.add(floatParam(voiceId(fill, v), name + " Fill", { 0.0f, 1.0f }, pattern.fill[static_cast<size_t>(v)]));
        layout.add(boolParam(voiceId(mute, v), name + " Mute", false));
        layout.add(floatParam(voiceId(velLow, v), name + " Velocity Low", { 0.0f, 1.0f }, dyn.velLow[static_cast<size_t>(v)]));
        layout.add(floatParam(voiceId(velHigh, v), name + " Velocity High", { 0.0f, 1.0f }, dyn.velHigh[static_cast<size_t>(v)]));
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { voiceId(note, v), kVersion },
                                                             name + " Note", 0, 127, kDefaultNotes[v]));
    }
    layout.add(boolParam(linear, "Linear", false));

    // Groove
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { timing, kVersion }, "Timing",
                                                            juce::StringArray { "Trip", "Shake", "Push", "Clave" }, 1));
    layout.add(floatParam(shuffle, "Shuffle", { 0.0f, 6.0f }, 2.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { feeling, kVersion }, "Feeling",
                                                            juce::StringArray { "Tight", "Rolls", "Flam", "Slop" }, 0));
    layout.add(floatParam(feelMs, "Flam / Slop", { 0.0f, 30.0f }, 9.0f, "ms", 1));

    // Humanize + dynamics
    layout.add(floatParam(humanize, "Humanize", { 0.0f, 1.0f }, 0.3f));
    layout.add(boolParam(humanizeLock, "Humanize Lock", false));
    layout.add(floatParam(dynamics, "Dynamics", { 0.0f, 1.0f }, 0.6f));

    // Output
    layout.add(floatParam(noteLength, "Note Length", { 0.02f, 1.0f }, 0.125f, "beats"));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { midiChannel, kVersion }, "MIDI Channel", 1, 16, 10));
    layout.add(boolParam(kitOn, "Preview Kit", true));
    layout.add(floatParam(kitGain, "Kit Level", { 0.0f, 1.0f }, 0.7f));

    // Free-running clock for standalone use / auditioning without host transport.
    layout.add(boolParam(freeRun, "Free Run", false));
    layout.add(floatParam(freeTempo, "Free Tempo", { 40.0f, 240.0f, 0.1f }, 95.0f, "bpm", 1));
    return layout;
}

EngineParams readEngineParams(const juce::AudioProcessorValueTreeState& s)
{
    EngineParams p;
    p.pattern.mapX = load(s, mapX);
    p.pattern.mapY = load(s, mapY);
    p.pattern.chaos = load(s, chaos);
    p.pattern.linear = load(s, linear) > 0.5f;
    const auto seedOffset = static_cast<std::uint32_t>(load(s, variation));
    p.pattern.seed += seedOffset * 7919u;
    p.humanize.seed += seedOffset * 104729u;

    for (int v = 0; v < kNumVoices; ++v)
    {
        const auto i = static_cast<size_t>(v);
        p.pattern.fill[i] = load(s, voiceId(fill, v));
        p.pattern.mute[i] = load(s, voiceId(mute, v)) > 0.5f;
        p.dynamics.velLow[i] = load(s, voiceId(velLow, v));
        p.dynamics.velHigh[i] = std::max(p.dynamics.velLow[i], load(s, voiceId(velHigh, v)));
    }

    p.groove.timing = static_cast<Timing>(juce::jlimit(0, kNumTimings - 1, static_cast<int>(load(s, timing))));
    p.groove.shuffle = load(s, shuffle);
    p.groove.feeling = static_cast<Feeling>(juce::jlimit(0, kNumFeelings - 1, static_cast<int>(load(s, feeling))));
    p.groove.feelMs = load(s, feelMs);

    p.humanize.amount = load(s, humanize);
    p.humanize.lock = load(s, humanizeLock) > 0.5f;
    p.dynamics.dynamics = load(s, dynamics);
    p.noteLengthBeats = load(s, noteLength);
    return p;
}

} // namespace gristo::params
