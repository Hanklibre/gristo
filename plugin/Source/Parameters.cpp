#include "Parameters.h"

#include <map>
#include <mutex>

namespace gristo::params {

namespace {

struct IdKey
{
    juce::String base;
    int a, b;
    bool operator<(const IdKey& o) const
    {
        if (a != o.a)
            return a < o.a;
        if (b != o.b)
            return b < o.b;
        return base.compare(o.base) < 0;
    }
};

juce::String cachedId(const juce::String& base, int a, int b)
{
    thread_local std::map<IdKey, juce::String> cache;
    IdKey key { base, a, b };
    auto it = cache.find(key);
    if (it == cache.end())
        it = cache.emplace(key, b < 0 ? base + juce::String(a) : base + juce::String(a) + "_" + juce::String(b)).first;
    return it->second; // a reference-counted copy: no allocation
}

} // namespace

juce::String voiceId(const juce::String& base, int voice) { return cachedId(base, voice, -1); }
juce::String laneId(const juce::String& base, int lane) { return cachedId(base, lane, -1); }
juce::String slotId(const juce::String& base, int lane, int slot) { return cachedId(base, lane, slot); }
juce::String sourceTargetId(int source, int slot)
{
    return source < kNumModLanes ? slotId(modTarget, source, slot) : slotId(macroTarget, source - kNumModLanes, slot);
}
juce::String sourceAmountId(int source, int slot)
{
    return source < kNumModLanes ? slotId(modAmount, source, slot) : slotId(macroAmount, source - kNumModLanes, slot);
}

namespace {

constexpr int kVersion = 1;

// GM drum notes (kick, snare, CH, OH, clap, low tom) three octaves down, below anything the
// arps play, so Live can split drums from arps by note. A Pitch +36 restores GM.
const int kDefaultNotes[kNumVoices] = { 0, 2, 6, 10, 3, 9 };

thread_local const ParamOffsets* activeOffsets = nullptr;

float load(const juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    const float raw = s.getRawParameterValue(id)->load(std::memory_order_relaxed);
    if (activeOffsets != nullptr)
        for (int i = 0; i < activeOffsets->count; ++i)
            if (const auto& e = activeOffsets->entries[static_cast<size_t>(i)]; e.id != nullptr && *e.id == id)
                if (auto* param = s.getParameter(id))
                {
                    const auto& range = param->getNormalisableRange();
                    const float moved = juce::jlimit(0.0f, 1.0f, range.convertTo0to1(raw) + e.offset);
                    return range.snapToLegalValue(range.convertFrom0to1(moved));
                }
    return raw;
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

std::unique_ptr<juce::AudioParameterFloat> percentParam(const juce::String& id, const juce::String& name, float def,
                                                        float max = 1.0f)
{
    auto attributes = juce::AudioParameterFloatAttributes()
                          .withLabel("%")
                          .withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + "%"; })
                          .withValueFromStringFunction([](const juce::String& t) { return t.getFloatValue() / 100.0f; });
    return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, kVersion }, name,
                                                       juce::NormalisableRange<float> { 0.0f, max }, def, attributes);
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
    layout.add(percentParam(mapRandom, "Map Random", 0.0f));

    // Voices
    for (int v = 0; v < kNumVoices; ++v)
    {
        const juce::String name = voiceName(v);
        layout.add(floatParam(voiceId(fill, v), name + " Fill", { 0.0f, 1.0f }, pattern.fill[static_cast<size_t>(v)]));
        layout.add(percentParam(voiceId(probability, v), name + " Probability", 1.0f));
        layout.add(percentParam(voiceId(fillRandom, v), name + " Fill Random", 0.0f));
        layout.add(percentParam(voiceId(voiceHumanize, v), name + " Humanize", 1.0f, 2.0f));
        layout.add(percentParam(voiceId(voiceDynamics, v), name + " Dynamics", 1.0f, 2.0f));

        layout.add(percentParam(voiceId(probabilityRandom, v), name + " Probability Random", 0.0f));
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { voiceId(voiceLength, v), kVersion }, name + " Length", 1, kPatternSteps, kPatternSteps));
        layout.add(percentParam(voiceId(voiceRatchet, v), name + " Ratchet", 0.0f));
        layout.add(percentParam(voiceId(voiceSwing, v), name + " Swing", 1.0f));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { voiceId(voiceCondition, v), kVersion }, name + " Plays",
                                                                juce::StringArray { "Every bar", "1st of 2 bars", "2nd of 2 bars", "1st of 4 bars", "4th of 4 bars" }, 0));
        layout.add(floatParam(voiceId(drumTune, v), name + " Tune", { -24.0f, 24.0f, 0.1f }, 0.0f, "st", 1));
        layout.add(percentParam(voiceId(drumDecay, v), name + " Decay", 0.5f));
        layout.add(percentParam(voiceId(drumTone, v), name + " Tone", 0.5f));
        layout.add(percentParam(voiceId(drumPunch, v), name + " Punch", 0.5f));
        layout.add(percentParam(voiceId(drumNoise, v), name + " Noise", 0.5f));
        layout.add(percentParam(voiceId(drumDrive, v), name + " Drive", 0.0f));
        layout.add(floatParam(voiceId(drumPan, v), name + " Pan", { -1.0f, 1.0f, 0.01f }, 0.0f));
        layout.add(percentParam(voiceId(drumBody, v), name + " Body", 0.0f));
        layout.add(floatParam(voiceId(drumBodyTune, v), name + " Body Tune", { -24.0f, 24.0f, 0.1f }, 0.0f, "st", 1));
        layout.add(percentParam(voiceId(drumBodyDecay, v), name + " Body Decay", 0.5f));
        {
            juce::StringArray engines { "Kit's own" };
            for (int e = 0; e < static_cast<int>(sound::DrumEngine::Count); ++e)
                engines.add(sound::drumEngineName(static_cast<sound::DrumEngine>(e)));
            layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { voiceId(drumEngine, v), kVersion }, name + " Engine", engines, 0));
        }
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { voiceId(drumFilter, v), kVersion }, name + " Filter",
                                                                juce::StringArray { "Off", "Low-pass", "Band-pass", "High-pass" }, 0));
        layout.add(percentParam(voiceId(drumCutoff, v), name + " Cutoff", 0.5f));
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { voiceId(velMin, v), kVersion }, name + " Velocity Min", 1, 127,
                                                             juce::roundToInt(dyn.velMin[static_cast<size_t>(v)] * 127.0f)));
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { voiceId(velMax, v), kVersion }, name + " Velocity Max", 1, 127,
                                                             juce::roundToInt(dyn.velMax[static_cast<size_t>(v)] * 127.0f)));
        auto noteAttributes = juce::AudioParameterIntAttributes()
                                  .withStringFromValueFunction([](int n, int) { return noteDisplayName(n); })
                                  .withValueFromStringFunction([](const juce::String& t) {
                                      // Accept "36", "C1" or "C1 · 36": a trailing number wins.
                                      const auto digits = t.fromLastOccurrenceOf(" ", false, false).trim();
                                      if (digits.containsOnly("0123456789") && digits.isNotEmpty())
                                          return digits.getIntValue();
                                      for (int n = 0; n < 128; ++n)
                                          if (juce::MidiMessage::getMidiNoteName(n, true, true, 3).equalsIgnoreCase(t.trim()))
                                              return n;
                                      return t.getIntValue();
                                  });
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { voiceId(note, v), kVersion },
                                                             name + " Note", 0, 127, kDefaultNotes[v], noteAttributes));
    }
    layout.add(boolParam(linear, "Linear", false));

    // Groove
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { timing, kVersion }, "Timing",
                                                            juce::StringArray { "Trip", "Shake", "Push", "Clave" }, 1));
    layout.add(floatParam(shuffle, "Shuffle", { 0.0f, 6.0f }, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { feeling, kVersion }, "Feeling",
                                                            juce::StringArray { "Tight", "Rolls", "Flam", "Slop" }, 0));
    layout.add(floatParam(feelMs, "Flam / Slop", { 0.0f, 30.0f }, 9.0f, "ms", 1));
    layout.add(percentParam(shuffleRandom, "Shuffle Random", 0.0f));
    layout.add(percentParam(feelRandom, "Flam Random", 0.0f));

    // Humanize + dynamics
    layout.add(floatParam(humanize, "Humanize", { 0.0f, 1.0f }, 0.0f));
    // The big knobs
    layout.add(percentParam(busy, "Busy", 0.5f));
    layout.add(percentParam(loose, "Loose", 0.4f));
    layout.add(percentParam(wild, "Wild", 0.1f));
    layout.add(percentParam(dirt, "Dirt", 0.3f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { grit, kVersion }, "Grit", juce::StringArray { "Tape", "Crunch", "Drive" }, 0));
    layout.add(percentParam(alive, "Alive", 0.5f));
    layout.add(percentParam(space, "Space", 0.5f));
    layout.add(boolParam(humanizeLock, "Humanize Lock", false));
    layout.add(floatParam(dynamics, "Dynamics", { 0.0f, 1.0f }, 0.6f));

    // Output
    layout.add(floatParam(noteLength, "Note Length", { 0.02f, 1.0f }, 0.125f, "beats"));
    for (int v = 0; v < kNumVoices; ++v)
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { voiceId(voiceChannel, v), kVersion },
                                                             juce::String(voiceName(v)) + " MIDI Channel", 1, 16, 10));

    // Arps
    juce::StringArray orders, scales;
    for (int i = 0; i < static_cast<int>(ArpOrder::Count); ++i)
        orders.add(arpOrderName(static_cast<ArpOrder>(i)));
    for (int i = 0; i < static_cast<int>(ArpScale::Count); ++i)
        scales.add(arpScaleName(static_cast<ArpScale>(i)));
    auto noteAttributes = juce::AudioParameterIntAttributes().withStringFromValueFunction([](int n, int) { return noteDisplayName(n); });
    for (int a = 0; a < kNumArps; ++a)
    {
        const juce::String name = "Arp " + juce::String(a + 1);
        auto id = [a](const juce::String& base) { return juce::ParameterID { laneId(base, a), kVersion }; };
        layout.add(std::make_unique<juce::AudioParameterBool>(id(arpOn), name + " On", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(id(arpMode), name + " Mode", juce::StringArray { "Arp", "Generate" }, 0));
        for (int v = 0; v < kNumVoices; ++v)
            layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { slotId(arpTrigger, a, v), kVersion },
                                                                  name + " Trigger " + voiceName(v), v == (a == 0 ? Kick : a == 1 ? HiHat1 : Snare)));
        layout.add(std::make_unique<juce::AudioParameterChoice>(id(arpReset), name + " Reset",
                                                                juce::StringArray { "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" }, 2));
        layout.add(std::make_unique<juce::AudioParameterChoice>(id(arpDivide), name + " Step On",
                                                                juce::StringArray { "Every hit", "Every 2nd hit", "Every 3rd hit", "Every 4th hit", "Every 6th hit", "Every 8th hit" }, 0));
        layout.add(std::make_unique<juce::AudioParameterInt>(id(arpChannel), name + " MIDI Channel", 1, 16, a < 2 ? a + 1 : a + 2)); // 3 = chords
        layout.add(std::make_unique<juce::AudioParameterBool>(id(arpLatch), name + " Latch", true));
        layout.add(std::make_unique<juce::AudioParameterChoice>(id(arpOrder), name + " Order", orders, 0));
        layout.add(std::make_unique<juce::AudioParameterInt>(id(arpOctaves), name + " Octaves", 1, 4, 1));
        layout.add(std::make_unique<juce::AudioParameterInt>(id(arpRoot), name + " Root", 24, 96, a == 0 ? 36 : a == 1 ? 60 : 72, noteAttributes));
        layout.add(std::make_unique<juce::AudioParameterChoice>(id(arpScale), name + " Scale", scales, 1));
        layout.add(std::make_unique<juce::AudioParameterInt>(id(arpLength), name + " Length", 1, kMaxArpSteps, 8));
        layout.add(percentParam(laneId(arpDensity, a), name + " Density", 0.8f));
        layout.add(percentParam(laneId(arpAccent, a), name + " Accent", 0.25f));
        layout.add(percentParam(laneId(arpSlide, a), name + " Slide", 0.15f));
        layout.add(percentParam(laneId(arpMutate, a), name + " Mutate", 0.0f));
        layout.add(std::make_unique<juce::AudioParameterInt>(id(arpSeed), name + " Pattern", 1, 9999, a + 1));
        layout.add(floatParam(laneId(arpGate, a), name + " Gate", { 0.25f, 8.0f, 0.25f }, 2.0f, "16ths"));
        layout.add(std::make_unique<juce::AudioParameterInt>(id(arpVelMin), name + " Velocity Min", 1, 127, 40));
        layout.add(std::make_unique<juce::AudioParameterInt>(id(arpVelMax), name + " Velocity Max", 1, 127, 127));
        layout.add(percentParam(laneId(arpProbability, a), name + " Probability", 1.0f));
        layout.add(percentParam(laneId(arpProbabilityRandom, a), name + " Probability Random", 0.0f));
        layout.add(std::make_unique<juce::AudioParameterBool>(id(arpFollow), name + " Follow Chords", false));
        layout.add(std::make_unique<juce::AudioParameterInt>(
            id(arpShift), name + " Out Shift", -60, 60, a == 1 ? 36 : 0, // arp 3 (from C4) sits between arps 1 and 2
            juce::AudioParameterIntAttributes().withStringFromValueFunction(
                [](int v, int) { return (v > 0 ? "+" : "") + juce::String(v) + " st"; })));
    }
    for (int part = 0; part < kNumParts; ++part)
    {
        layout.add(boolParam(voiceId(partMute, part), juce::String(partName(part)) + " Mute", false));
        layout.add(boolParam(voiceId(partSolo, part), juce::String(partName(part)) + " Solo", false));
    }
    layout.add(boolParam(kitOn, "Sound On", true));
    layout.add(boolParam(separateOuts, "Separate Outputs", false));
    layout.add(floatParam(kitGain, "Sound Level", { 0.0f, 1.0f }, 0.7f));

    // Built-in sounds
    {
        juce::StringArray kits, sounds;
        for (int k = 0; k < static_cast<int>(sound::Kit::Count); ++k)
            kits.add(sound::kitName(static_cast<sound::Kit>(k)));
        for (int s = 0; s < static_cast<int>(sound::SynthSound::Count); ++s)
            sounds.add(sound::synthSoundName(static_cast<sound::SynthSound>(s)));
        auto semis = juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) {
            const int st = juce::roundToInt(v);
            return (st > 0 ? "+" : "") + juce::String(st) + " st";
        });
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { kitType, kVersion }, "Drum Kit", kits, 1));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { kitTune, kVersion }, "Kit Tune",
                                                               juce::NormalisableRange<float> { -12.0f, 12.0f, 0.1f }, 0.0f, semis));
        layout.add(percentParam(kitDecay, "Kit Decay", 0.5f));
        layout.add(percentParam(kitTone, "Kit Tone", 0.5f));
        layout.add(percentParam(kitDrive, "Kit Drive", 0.0f));
        for (int v = 0; v < kNumVoices; ++v)
            layout.add(percentParam(voiceId(drumLevel, v), juce::String(voiceName(v)) + " Level", 0.8f));
        layout.add(percentParam(kitReverb, "Kit Reverb", 0.08f));
        layout.add(percentParam(kitDelay, "Kit Delay", 0.0f));
        for (int a = 0; a < sound::kNumSynths; ++a)
        {
            const int arp = mod::arpOfSynth(a);
            const juce::String name = arp < 0 ? juce::String("Chords ") : "Arp " + juce::String(arp + 1) + " ";
            const int defaultSound = static_cast<int>(a == 0 ? sound::SynthSound::AcidBass
                                                      : a == 1 ? sound::SynthSound::Pluck
                                                      : a == sound::kChordSynth ? sound::SynthSound::WarmPad : sound::SynthSound::SoftLead);
            layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { laneId(synthSound, a), kVersion },
                                                                    name + "Sound", sounds, defaultSound));
            layout.add(percentParam(laneId(synthCutoff, a), name + "Cutoff", 0.5f));
            layout.add(percentParam(laneId(synthResonance, a), name + "Resonance", 0.5f));
            layout.add(percentParam(laneId(synthEnv, a), name + "Env Amount", 0.5f));
            layout.add(percentParam(laneId(synthAttack, a), name + "Attack", 0.5f));
            layout.add(percentParam(laneId(synthRelease, a), name + "Release", 0.5f));
            layout.add(percentParam(laneId(synthGlide, a), name + "Glide", 0.3f));
            layout.add(percentParam(laneId(synthLevel, a), name + "Level", 0.7f));
            layout.add(percentParam(laneId(synthReverb, a), name + "Reverb", a == 0 ? 0.1f : a == 1 ? 0.25f : 0.3f));
            layout.add(percentParam(laneId(synthDelay, a), name + "Delay", a == 1 || a == 2 ? 0.2f : 0.0f));
            layout.add(percentParam(laneId(synthDuck, a), name + "Duck", 0.0f));
            layout.add(percentParam(laneId(synthBody, a), name + "Body", 0.0f));
            layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { laneId(synthOctave, a), kVersion }, name + "Octave", -3, 3, 0,
                juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return (v > 0 ? "+" : "") + juce::String(v) + " oct"; })));
        }
        // Chord sequencer
        {
            juce::StringArray keys;
            for (int k = 0; k < 12; ++k)
                keys.add(pitchClassName(k));
            auto pid = [](const juce::String& id) { return juce::ParameterID { id, kVersion }; };
            layout.add(std::make_unique<juce::AudioParameterBool>(pid(chordOn), "Chords On", false));
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid(chordKey), "Chord Key", keys, 9));
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid(chordMinor), "Chord Mode", juce::StringArray { "Major", "Minor" }, 1));
            layout.add(floatParam(chordHomeX, "Harmony X", { 0.0f, 1.0f }, 0.5f));
            layout.add(floatParam(chordHomeY, "Harmony Y", { 0.0f, 1.0f }, 0.0f));
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid(chordMode), "Chords Played By", juce::StringArray { "Wander", "Keys", "Progression" }, 0));
            layout.add(percentParam(chordWander, "Wander", 0.3f));
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid(chordRate), "Chord Length", juce::StringArray { "1/2 bar", "1 bar", "2 bars" }, 1));
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid(chordPhrase), "Harmony Phrase", juce::StringArray { "2 bars", "4 bars", "8 bars" }, 1));
            layout.add(std::make_unique<juce::AudioParameterBool>(pid(chordFrozen), "Harmony Caught", false));
            layout.add(std::make_unique<juce::AudioParameterInt>(pid(chordLoop), "Harmony Loop", 0, 1000000, 0));
            layout.add(std::make_unique<juce::AudioParameterInt>(pid(chordSeed), "Chord Pattern", 1, 9999, 1));
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid(chordOctave), "Chord Octave", juce::StringArray { "C1", "C2", "C3", "C4" }, 1));
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid(chordPlay), "Chord Play", juce::StringArray { "Off", "Held", "Stabs" }, 1));
            for (int v = 0; v < kNumVoices; ++v)
                layout.add(std::make_unique<juce::AudioParameterBool>(pid(voiceId(chordStab, v)), "Chord Stab " + juce::String(voiceName(v)), v == Snare));
            layout.add(floatParam(chordGate, "Chord Stab Gate", { 0.25f, 8.0f, 0.25f }, 2.0f, "16ths"));
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid(chordVoicing), "Chord Voicing", juce::StringArray { "Close", "Smooth", "Smooth + bass", "Drop 2", "Open", "Rootless", "Shell", "Upper + bass" }, 2));
            layout.add(floatParam(chordStrum, "Chord Strum", { 0.0f, 80.0f, 1.0f }, 0.0f, "ms"));
            layout.add(std::make_unique<juce::AudioParameterChoice>(pid(chordStrumDir), "Chord Strum Direction", juce::StringArray { "Up", "Down", "Alternate" }, 0));
            layout.add(std::make_unique<juce::AudioParameterInt>(pid(chordVelMin), "Chord Velocity Min", 1, 127, 50));
            layout.add(std::make_unique<juce::AudioParameterInt>(pid(chordVelMax), "Chord Velocity Max", 1, 127, 100));
            layout.add(std::make_unique<juce::AudioParameterInt>(pid(chordChannel), "Chord MIDI Channel", 1, 16, 3));
            layout.add(std::make_unique<juce::AudioParameterInt>(
                pid(chordShift), "Chord Out Shift", -60, 60, -36,
                juce::AudioParameterIntAttributes().withStringFromValueFunction(
                    [](int v, int) { return (v > 0 ? "+" : "") + juce::String(v) + " st"; })));
        }
        layout.add(percentParam(fxSize, "Reverb Size", 0.5f));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { fxTime, kVersion }, "Delay Time",
                                                                juce::StringArray { "1/16", "1/8", "1/8D", "1/4" }, 2));
        layout.add(percentParam(fxFeedback, "Delay Feedback", 0.35f, 0.9f));
        layout.add(percentParam(fxShimmer, "Shimmer", 0.0f));
        // Beads
        layout.add(percentParam(beadsLevel, "Beads Level", 0.0f));
        layout.add(percentParam(beadsReverb, "Beads Reverb", 0.3f));
        layout.add(percentParam(beadsDensity, "Beads Density", 0.1f));
        layout.add(percentParam(beadsTime, "Beads Time", 0.5f));
        layout.add(percentParam(beadsSize, "Beads Size", 0.5f));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { beadsPitch, kVersion }, "Beads Pitch",
            juce::StringArray { "-2 oct", "-oct -5th", "-oct -4th", "-1 oct", "-5th", "-4th", "Unison", "+4th", "+5th", "+1 oct", "+oct +4th", "+oct +5th", "+2 oct" }, 6));
        layout.add(percentParam(beadsShape, "Beads Shape", 0.5f));
        layout.add(percentParam(beadsTimeRandom, "Beads Time Random", 0.0f));
        layout.add(percentParam(beadsSizeRandom, "Beads Size Random", 0.0f));
        layout.add(percentParam(beadsPitchRandom, "Beads Pitch Random", 0.0f));
        layout.add(percentParam(beadsShapeRandom, "Beads Shape Random", 0.0f));
        layout.add(percentParam(beadsFeedback, "Beads Feedback", 0.0f));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { beadsQuality, kVersion }, "Beads Quality",
                                                                juce::StringArray { "Digital", "Vintage digital", "Clean tape", "Dirty tape" }, 0));
        layout.add(boolParam(beadsFreeze, "Beads Freeze", false));
        layout.add(boolParam(beadsMusical, "Beads Musical Pitch", true));
        for (int v = 0; v < kNumVoices; ++v)
            layout.add(boolParam(voiceId(beadsTrigger, v), "Beads Gate " + juce::String(voiceName(v)), v == HiHat1));
        for (int s = 0; s < sound::kNumSynths; ++s)
            layout.add(percentParam(laneId(beadsSend, s), (s < kNumArps ? "Beads In Arp " + juce::String(s + 1) : juce::String("Beads In Chords")), 0.5f));
    }

    // Free-running clock for standalone use / auditioning without host transport.
    layout.add(boolParam(freeRun, "Free Run", false));
    layout.add(floatParam(freeTempo, "Free Tempo", { 40.0f, 240.0f, 0.1f }, 95.0f, "bpm", 1));
    // Modulation lanes
    const auto targetAttributes = juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return targetDisplayName(v); });
    for (int lane = 0; lane < kNumModLanes; ++lane)
    {
        const juce::String laneName = "Lane " + juce::String(lane + 1);
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { laneId(modBars, lane), kVersion }, laneName + " Length",
                                                                juce::StringArray { "1 bar", "2 bars", "4 bars" }, 0));
        {
            juce::StringArray waves, rates;
            for (int w = 0; w < static_cast<int>(LaneWave::Count); ++w)
                waves.add(laneWaveName(static_cast<LaneWave>(w)));
            for (int r = 0; r < kNumLfoRates; ++r)
                rates.add(lfoRateName(r));
            layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { laneId(modWave, lane), kVersion }, laneName + " Mode", waves, 0));
            layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { laneId(modRate, lane), kVersion }, laneName + " LFO Rate", rates, 3));
        }
        for (int slot = 0; slot < kModSlotsPerLane; ++slot)
        {
            const juce::String slotName = laneName + " Slot " + juce::String(slot + 1);
            layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { slotId(modTarget, lane, slot), kVersion }, slotName + " Target",
                                                                 0, kMaxTargets - 1, 0, targetAttributes));
            auto attributes = juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) {
                const int pct = juce::roundToInt(v * 100.0f);
                return (pct > 0 ? "+" : "") + juce::String(pct) + "%";
            }).withValueFromStringFunction([](const juce::String& t) { return t.getFloatValue() / 100.0f; });
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { slotId(modAmount, lane, slot), kVersion },
                                                                   slotName + " Amount", juce::NormalisableRange<float> { -1.0f, 1.0f }, 0.0f,
                                                                   attributes));
        }
    }
    for (int m = 0; m < kNumMacros; ++m)
    {
        const juce::String name = "Macro " + juce::String(m + 1);
        layout.add(percentParam(laneId(macroValue, m), name, 0.0f));
        for (int slot = 0; slot < kModSlotsPerLane; ++slot)
        {
            const juce::String slotName = name + " Slot " + juce::String(slot + 1);
            layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { slotId(macroTarget, m, slot), kVersion }, slotName + " Target",
                                                                 0, kMaxTargets - 1, 0, targetAttributes));
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { slotId(macroAmount, m, slot), kVersion },
                                                                   slotName + " Amount", juce::NormalisableRange<float> { -1.0f, 1.0f }, 0.0f));
        }
    }
    // Added later: at the end, so saved routings to the parameters above keep pointing
    // at the same ones (routings are stored by position).
    layout.add(floatParam(chordNoteChance, "Chord Note Chance", { 0.0f, 100.0f, 1.0f }, 100.0f, "%"));
    for (int v = 0; v < kNumVoices; ++v)
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { voiceId(drumBodyType, v), kVersion },
                                                                juce::String(voiceName(v)) + " Body Type",
                                                                juce::StringArray { "Shell", "Box", "Metal", "Tube", "Wood", "Glass" }, 0));
    // Rings
    layout.add(boolParam(ringsOn, "Rings On", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ringsSource, kVersion }, "Rings Played By",
                                                            juce::StringArray { "Arp 1", "Arp 2", "Arp 3", "Chords" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ringsModel, kVersion }, "Rings Model",
        juce::StringArray { "Modal", "Sympathetic strings", "String", "FM voice", "Sympathetic (chords)", "String + reverb" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ringsPolyphony, kVersion }, "Rings Polyphony",
                                                            juce::StringArray { "1 voice", "2 voices", "4 voices" }, 1));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { ringsOctave, kVersion }, "Rings Octave", -2, 2, 0));
    layout.add(percentParam(ringsStructure, "Rings Structure", 0.4f));
    layout.add(percentParam(ringsBrightness, "Rings Brightness", 0.5f));
    layout.add(percentParam(ringsDamping, "Rings Damping", 0.6f));
    layout.add(percentParam(ringsPosition, "Rings Position", 0.3f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ringsExciter, kVersion }, "Rings Exciter",
                                                            juce::StringArray { "Pluck", "Drums", "Pluck + drums" }, 0));
    layout.add(percentParam(ringsLevel, "Rings Level", 0.6f));
    layout.add(percentParam(ringsReverb, "Rings Reverb", 0.25f));
    layout.add(percentParam(ringsDelay, "Rings Delay", 0.0f));
    layout.add(percentParam(ringsBeads, "Rings Into Beads", 0.0f));
    // The mixer and the delay as its own effect
    for (int s = 0; s < sound::kNumSynths; ++s)
        layout.add(boolParam(laneId(beadsPre, s), (s < kNumArps ? "Arp " + juce::String(s + 1) : juce::String("Chords")) + " Beads Pre-Fader", false));
    layout.add(boolParam(ringsMute, "Rings Mute", false));
    layout.add(boolParam(ringsSolo, "Rings Solo", false));
    layout.add(boolParam(ringsBeadsPre, "Rings Beads Pre-Fader", false));
    layout.add(boolParam(beadsMute, "Beads Mute", false));
    layout.add(boolParam(beadsSolo, "Beads Solo", false));
    layout.add(percentParam(beadsToReverb, "Beads To Reverb", 0.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { delayType, kVersion }, "Delay Type",
                                                            juce::StringArray { "Tape", "BBD", "Clean" }, 0));
    layout.add(percentParam(delayAge, "Delay Age", 0.3f));
    layout.add(boolParam(delayPing, "Delay Ping-Pong", true));
    layout.add(percentParam(delayLevel, "Delay Level", 1.0f));
    layout.add(boolParam(delayMute, "Delay Mute", false));
    layout.add(percentParam(delayToReverb, "Delay To Reverb", 0.15f));
    layout.add(percentParam(delayToBeads, "Delay To Beads", 0.0f));
    layout.add(percentParam(reverbLevel, "Reverb Level", 1.0f));
    layout.add(boolParam(reverbMute, "Reverb Mute", false));
    // Rhythm
    for (int v = 0; v < kNumVoices; ++v)
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { voiceId(voiceDivide, v), kVersion },
                                                             juce::String(voiceName(v)) + " Clock Divide", 1, kMaxClockDivide, 1));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { chordStabEvery, kVersion }, "Chord Stab Every",
        juce::StringArray { "Stab every hit", "Every 2nd hit", "Every 3rd hit", "Every 4th hit", "Every 6th hit", "Every 8th hit" }, 0));
    layout.add(percentParam(chordStabChance, "Chord Stab Chance", 1.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { chordPush, kVersion }, "Chord Push",
        juce::StringArray { "Change on the 1", "Early 1/16", "Early 1/8", "Early 1/4", "Late 1/16", "Late 1/8", "Late 1/4" }, 0));
    for (int slot = 0; slot < kMaxProgression; ++slot)
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { laneId(chordProgression, slot), kVersion },
                                                             "Progression Chord " + juce::String(slot + 1), -1, cellCode((kHarmonyColumns - 1) / 2, kHarmonyRows - 1), -1));
    for (int synth = 0; synth < sound::kNumSynths; ++synth)
    {
        const juce::String name = synth < kNumArps ? "Synth " + juce::String(synth + 1) + " " : juce::String("Chord Synth ");
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { laneId(synthBodyType, synth), kVersion }, name + "Body Type",
                                                                juce::StringArray { "Shell", "Box", "Metal", "Tube", "Wood", "Glass" }, 4));
        layout.add(floatParam(laneId(synthBodyTune, synth), name + "Body Tune", { -24.0f, 24.0f, 0.1f }, 0.0f, "st", 1));
        layout.add(percentParam(laneId(synthBodyDecay, synth), name + "Body Decay", 0.4f));
    }
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { chordRestrike, kVersion }, "Chord Re-strike",
        juce::StringArray { "Held", "Strike 1/8", "Strike dotted 1/8", "Strike 1/4", "Strike dotted 1/4", "Strike 1/2", "Strike every bar" }, 0));
    return layout;
}

EngineParams readEngineParams(const juce::AudioProcessorValueTreeState& s, const ModPointStore& lanes, const StepEditStore* edits)
{
    EngineParams p;
    p.pattern.mapX = load(s, mapX);
    p.pattern.mapY = load(s, mapY);
    p.pattern.chaos = load(s, chaos);
    p.pattern.linear = load(s, linear) > 0.5f;
    const auto seedOffset = static_cast<std::uint32_t>(load(s, variation));
    p.pattern.seed += seedOffset * 7919u;
    p.groove.seed += seedOffset * 6151u;
    p.humanize.seed += seedOffset * 104729u;

    for (int v = 0; v < kNumVoices; ++v)
    {
        const auto i = static_cast<size_t>(v);
        p.pattern.fill[i] = load(s, voiceId(fill, v));
        p.pattern.probability[i] = load(s, voiceId(probability, v));
        p.pattern.fillRandom[i] = load(s, voiceId(fillRandom, v));
        p.humanize.voiceAmount[i] = load(s, voiceId(voiceHumanize, v));
        p.dynamics.voiceDynamics[i] = load(s, voiceId(voiceDynamics, v));
        p.pattern.probabilityRandom[i] = load(s, voiceId(probabilityRandom, v));
        p.pattern.length[i] = static_cast<int>(load(s, voiceId(voiceLength, v)));
        p.pattern.divide[i] = juce::jlimit(1, kMaxClockDivide, juce::roundToInt(load(s, voiceId(voiceDivide, v))));
        p.groove.ratchet[i] = load(s, voiceId(voiceRatchet, v));
        p.groove.swing[i] = load(s, voiceId(voiceSwing, v));
        p.pattern.condition[i] = static_cast<int>(load(s, voiceId(voiceCondition, v)));
        const float lo = load(s, voiceId(velMin, v)) / 127.0f;
        const float hi = load(s, voiceId(velMax, v)) / 127.0f;
        p.dynamics.velMin[i] = std::min(lo, hi);
        p.dynamics.velMax[i] = std::max(lo, hi);
    }

    p.groove.timing = static_cast<Timing>(juce::jlimit(0, kNumTimings - 1, static_cast<int>(load(s, timing))));
    p.groove.shuffle = load(s, shuffle);
    p.groove.feeling = static_cast<Feeling>(juce::jlimit(0, kNumFeelings - 1, static_cast<int>(load(s, feeling))));
    p.groove.feelMs = load(s, feelMs);
    p.groove.shuffleRandom = load(s, shuffleRandom);
    p.groove.feelRandom = load(s, feelRandom);
    p.pattern.mapRandom = load(s, mapRandom);

    p.humanize.amount = load(s, humanize);
    p.humanize.lock = load(s, humanizeLock) > 0.5f;
    p.dynamics.dynamics = load(s, dynamics);
    p.noteLengthBeats = load(s, noteLength);
    for (int lane = 0; lane < kNumModLanes; ++lane)
    {
        ModLane& l = p.modulation.lanes[static_cast<size_t>(lane)];
        l.bars = barsForChoice(static_cast<int>(load(s, laneId(modBars, lane))));
        l.wave = static_cast<LaneWave>(juce::jlimit(0, static_cast<int>(LaneWave::Count) - 1, static_cast<int>(load(s, laneId(modWave, lane)))));
        l.lfoRate = juce::jlimit(0, kNumLfoRates - 1, static_cast<int>(load(s, laneId(modRate, lane))));
        for (int k = 0; k < kMaxModPoints; ++k)
            l.points[static_cast<size_t>(k)] = lanePoint(lanes, lane, k).load(std::memory_order_relaxed);
        for (int slot = 0; slot < kModSlotsPerLane; ++slot)
        {
            ModSlot& sl = l.slots[static_cast<size_t>(slot)];
            sl.target = static_cast<int>(load(s, slotId(modTarget, lane, slot)));
            sl.amount = load(s, slotId(modAmount, lane, slot));
        }
    }
    for (int m = 0; m < kNumMacros; ++m)
    {
        ModLane& l = p.modulation.lanes[static_cast<size_t>(kNumModLanes + m)];
        l.bars = 1;
        p.modulation.setMacro(m, load(s, laneId(macroValue, m)));
        for (int slot = 0; slot < kModSlotsPerLane; ++slot)
        {
            l.slots[static_cast<size_t>(slot)].target = static_cast<int>(load(s, slotId(macroTarget, m, slot)));
            l.slots[static_cast<size_t>(slot)].amount = load(s, slotId(macroAmount, m, slot));
        }
    }
    for (int a = 0; a < kNumArps; ++a)
    {
        ArpParams& ap = p.arps[static_cast<size_t>(a)];
        ap.enabled = load(s, laneId(arpOn, a)) > 0.5f;
        ap.mode = static_cast<ArpMode>(juce::jlimit(0, 1, static_cast<int>(load(s, laneId(arpMode, a)))));
        for (int v = 0; v < kNumVoices; ++v)
            ap.triggers[static_cast<size_t>(v)] = load(s, slotId(arpTrigger, a, v)) > 0.5f;
        ap.resetBars = kResetBarChoices[juce::jlimit(0, 4, static_cast<int>(load(s, laneId(arpReset, a))))];
        ap.divide = kArpDivideChoices[juce::jlimit(0, 5, static_cast<int>(load(s, laneId(arpDivide, a))))];
        ap.order = static_cast<ArpOrder>(juce::jlimit(0, static_cast<int>(ArpOrder::Count) - 1, static_cast<int>(load(s, laneId(arpOrder, a)))));
        ap.octaves = static_cast<int>(load(s, laneId(arpOctaves, a)));
        ap.root = static_cast<int>(load(s, laneId(arpRoot, a)));
        ap.scale = static_cast<ArpScale>(juce::jlimit(0, static_cast<int>(ArpScale::Count) - 1, static_cast<int>(load(s, laneId(arpScale, a)))));
        ap.length = static_cast<int>(load(s, laneId(arpLength, a)));
        ap.density = load(s, laneId(arpDensity, a));
        ap.accent = load(s, laneId(arpAccent, a));
        ap.slide = load(s, laneId(arpSlide, a));
        ap.mutate = load(s, laneId(arpMutate, a));
        ap.seed = static_cast<std::uint32_t>(load(s, laneId(arpSeed, a)));
        ap.gateSteps = load(s, laneId(arpGate, a));
        ap.shift = static_cast<int>(load(s, laneId(arpShift, a)));
        ap.probability = load(s, laneId(arpProbability, a));
        ap.probabilityRandom = load(s, laneId(arpProbabilityRandom, a));
        ap.followChords = load(s, laneId(arpFollow, a)) > 0.5f;
        const float lo = load(s, laneId(arpVelMin, a)) / 127.0f;
        const float hi = load(s, laneId(arpVelMax, a)) / 127.0f;
        ap.velMin = std::min(lo, hi);
        ap.velMax = std::max(lo, hi);
    }

    // Mute and solo: with any part (or Rings, or Beads) soloed, only soloed ones sound.
    bool anySolo = load(s, ringsSolo) > 0.5f || load(s, beadsSolo) > 0.5f;
    for (int part = 0; part < kNumParts; ++part)
        anySolo = anySolo || load(s, voiceId(partSolo, part)) > 0.5f;
    for (int part = 0; part < kNumParts; ++part)
        p.partMute[static_cast<size_t>(part)] = load(s, voiceId(partMute, part)) > 0.5f
                                             || (anySolo && load(s, voiceId(partSolo, part)) <= 0.5f);

    ChordParams& c = p.chords;
    c.enabled = load(s, chordOn) > 0.5f;
    c.key = static_cast<int>(load(s, chordKey));
    c.minor = load(s, chordMinor) > 0.5f;
    c.homeX = load(s, chordHomeX);
    c.homeY = load(s, chordHomeY);
    c.mode = static_cast<HarmonyMode>(juce::jlimit(0, 2, static_cast<int>(load(s, chordMode))));
    for (int slot = 0; slot < kMaxProgression; ++slot)
        c.progression[static_cast<size_t>(slot)] = juce::roundToInt(load(s, laneId(chordProgression, slot)));
    c.wander = load(s, chordWander);
    c.chordSteps = kChordRateSteps[juce::jlimit(0, 2, static_cast<int>(load(s, chordRate)))];
    c.phraseBars = kChordPhraseBars[juce::jlimit(0, 2, static_cast<int>(load(s, chordPhrase)))];
    c.frozen = load(s, chordFrozen) > 0.5f;
    c.frozenPhrase = static_cast<int>(load(s, chordLoop));
    c.voicing = static_cast<ChordVoicing>(juce::jlimit(0, kNumChordVoicings - 1, static_cast<int>(load(s, chordVoicing))));
    c.noteChance = load(s, chordNoteChance) / 100.0f;
    c.strumMs = load(s, chordStrum);
    c.strumDirection = static_cast<StrumDirection>(juce::jlimit(0, 2, static_cast<int>(load(s, chordStrumDir))));
    c.seed = static_cast<std::uint32_t>(load(s, chordSeed));
    c.lowestNote = kChordLowestNotes[juce::jlimit(0, 3, static_cast<int>(load(s, chordOctave)))];
    c.play = static_cast<ChordPlay>(juce::jlimit(0, 2, static_cast<int>(load(s, chordPlay))));
    for (int v = 0; v < kNumVoices; ++v)
        c.stabTriggers[static_cast<size_t>(v)] = load(s, voiceId(chordStab, v)) > 0.5f;
    c.stabGateSteps = load(s, chordGate);
    {
        static const int every[] = { 1, 2, 3, 4, 6, 8 };
        c.stabEvery = every[juce::jlimit(0, 5, static_cast<int>(load(s, chordStabEvery)))];
    }
    c.stabChance = load(s, chordStabChance);
    c.heldPush = kChordPushSteps[juce::jlimit(0, 6, static_cast<int>(load(s, chordPush)))];
    c.restrikeSteps = kRestrikeSteps[juce::jlimit(0, 6, static_cast<int>(load(s, chordRestrike)))];
    {
        const float lo = load(s, chordVelMin) / 127.0f, hi = load(s, chordVelMax) / 127.0f;
        c.velMin = std::min(lo, hi);
        c.velMax = std::max(lo, hi);
    }
    c.shift = static_cast<int>(load(s, chordShift));
    if (edits != nullptr)
        for (int v = 0; v < kNumVoices; ++v)
            for (int k = 0; k < kPatternSteps; ++k)
                p.pattern.edits[static_cast<size_t>(v)][static_cast<size_t>(k)]
                    = static_cast<std::int8_t>(juce::jlimit(0, 2, (*edits)[static_cast<size_t>(v * kPatternSteps + k)].load(std::memory_order_relaxed)));
    MacroParams macros;
    macros.busy = load(s, busy);
    macros.loose = load(s, loose);
    macros.wild = load(s, wild);
    applyMacros(p, macros);
    return p;
}

namespace {
// Space: 0 = dry, 50 % = the sends as set, 100 % = two and a half times as much.
float spaceSend(const juce::AudioProcessorValueTreeState& s, float send)
{
    const float amount = load(s, space);
    return juce::jlimit(0.0f, 1.0f, send * (amount <= 0.5f ? amount * 2.0f : 1.0f + (amount - 0.5f) * 3.0f));
}
} // namespace

sound::KitParams readKitParams(const juce::AudioProcessorValueTreeState& s)
{
    sound::KitParams k;
    k.kit = static_cast<sound::Kit>(juce::jlimit(0, static_cast<int>(sound::Kit::Count) - 1, static_cast<int>(load(s, kitType))));
    k.tune = load(s, kitTune);
    k.decay = load(s, kitDecay);
    k.tone = load(s, kitTone);
    k.drive = load(s, kitDrive);
    for (int v = 0; v < kNumVoices; ++v)
    {
        k.level[static_cast<size_t>(v)] = load(s, voiceId(drumLevel, v));
        auto& t = k.voice[static_cast<size_t>(v)];
        t.tune = load(s, voiceId(drumTune, v));
        t.decay = load(s, voiceId(drumDecay, v));
        t.tone = load(s, voiceId(drumTone, v));
        t.punch = load(s, voiceId(drumPunch, v));
        t.noise = load(s, voiceId(drumNoise, v));
        t.drive = load(s, voiceId(drumDrive, v));
        t.pan = load(s, voiceId(drumPan, v));
        t.body = load(s, voiceId(drumBody, v));
        t.bodyTune = load(s, voiceId(drumBodyTune, v));
        t.bodyDecay = load(s, voiceId(drumBodyDecay, v));
        t.bodyType = juce::jlimit(0, 5, static_cast<int>(load(s, voiceId(drumBodyType, v))));
        t.engine = static_cast<int>(load(s, voiceId(drumEngine, v))) - 1;
        t.filter = static_cast<int>(load(s, voiceId(drumFilter, v)));
        t.cutoff = load(s, voiceId(drumCutoff, v));
    }
    k.reverbSend = spaceSend(s, load(s, kitReverb));
    k.delaySend = spaceSend(s, load(s, kitDelay));
    return k;
}

sound::SynthParams readSynthParams(const juce::AudioProcessorValueTreeState& s, int arp)
{
    sound::SynthParams p;
    p.sound = static_cast<sound::SynthSound>(
        juce::jlimit(0, static_cast<int>(sound::SynthSound::Count) - 1, static_cast<int>(load(s, laneId(synthSound, arp)))));
    p.cutoff = load(s, laneId(synthCutoff, arp));
    p.resonance = load(s, laneId(synthResonance, arp));
    p.envAmount = load(s, laneId(synthEnv, arp));
    p.attack = load(s, laneId(synthAttack, arp));
    p.release = load(s, laneId(synthRelease, arp));
    p.glide = load(s, laneId(synthGlide, arp));
    p.level = load(s, laneId(synthLevel, arp));
    p.reverbSend = spaceSend(s, load(s, laneId(synthReverb, arp)));
    p.delaySend = spaceSend(s, load(s, laneId(synthDelay, arp)));
    p.duck = load(s, laneId(synthDuck, arp));
    p.body = load(s, laneId(synthBody, arp));
    p.bodyType = juce::jlimit(0, 5, static_cast<int>(load(s, laneId(synthBodyType, arp))));
    p.bodyTune = load(s, laneId(synthBodyTune, arp));
    p.bodyDecay = load(s, laneId(synthBodyDecay, arp));
    return p;
}

sound::FxParams readFxParams(const juce::AudioProcessorValueTreeState& s)
{
    sound::FxParams f;
    // Space above the middle also makes the room larger and the echoes longer.
    const float more = std::max(0.0f, load(s, space) - 0.5f) * 2.0f;
    f.reverbSize = juce::jlimit(0.0f, 1.0f, load(s, fxSize) + 0.35f * more);
    f.delayBeats = kDelayBeatChoices[juce::jlimit(0, 3, static_cast<int>(load(s, fxTime)))];
    f.delayFeedback = juce::jlimit(0.0f, 0.9f, load(s, fxFeedback) + 0.3f * more);
    f.shimmer = load(s, fxShimmer);
    f.delayType = juce::jlimit(0, 2, static_cast<int>(load(s, delayType)));
    f.delayAge = load(s, delayAge);
    f.delayPingPong = load(s, delayPing) > 0.5f;
    return f;
}

sound::BeadsParams readBeadsParams(const juce::AudioProcessorValueTreeState& s)
{
    sound::BeadsParams b;
    b.level = load(s, beadsLevel);
    b.reverb = load(s, beadsReverb);
    b.density = load(s, beadsDensity);
    b.time = load(s, beadsTime);
    b.size = load(s, beadsSize);
    b.pitch = kBeadsIntervals[juce::jlimit(0, 12, static_cast<int>(load(s, beadsPitch)))];
    b.shape = load(s, beadsShape);
    b.timeRandom = load(s, beadsTimeRandom);
    b.sizeRandom = load(s, beadsSizeRandom);
    b.pitchRandom = load(s, beadsPitchRandom);
    b.shapeRandom = load(s, beadsShapeRandom);
    b.feedback = load(s, beadsFeedback);
    b.quality = juce::jlimit(0, 3, static_cast<int>(load(s, beadsQuality)));
    b.freeze = load(s, beadsFreeze) > 0.5f;
    b.musicalPitch = load(s, beadsMusical) > 0.5f;
    for (int v = 0; v < kNumVoices; ++v)
        b.triggers[static_cast<size_t>(v)] = load(s, voiceId(beadsTrigger, v)) > 0.5f;
    for (int i = 0; i < sound::kNumSynths; ++i)
        b.send[static_cast<size_t>(i)] = load(s, laneId(beadsSend, i));
    return b;
}

sound::MixParams readMixParams(const juce::AudioProcessorValueTreeState& s)
{
    sound::MixParams m;
    bool anySolo = load(s, ringsSolo) > 0.5f || load(s, beadsSolo) > 0.5f;
    for (int part = 0; part < kNumParts; ++part)
        anySolo = anySolo || load(s, voiceId(partSolo, part)) > 0.5f;
    auto muted = [&](const juce::String& mute, const juce::String& solo) {
        return load(s, mute) > 0.5f || (anySolo && load(s, solo) <= 0.5f);
    };
    for (int arp = 0; arp <= kNumArps; ++arp) // the arps, then the chords
    {
        const int part = kNumVoices + arp;
        const int synth = arp == kNumArps ? sound::kChordSynth : mod::synthOfArp(arp);
        m.synthMute[static_cast<size_t>(synth)] = muted(voiceId(partMute, part), voiceId(partSolo, part));
    }
    for (int synth = 0; synth < sound::kNumSynths; ++synth)
        m.beadsPre[static_cast<size_t>(synth)] = load(s, laneId(beadsPre, synth)) > 0.5f;
    m.ringsMute = muted(ringsMute, ringsSolo);
    m.ringsBeadsPre = load(s, ringsBeadsPre) > 0.5f;
    m.beadsMute = muted(beadsMute, beadsSolo);
    m.beadsToReverb = load(s, beadsToReverb);
    // The returns are solo-safe: soloing a part keeps its echoes and reverb.
    m.delayLevel = load(s, delayLevel);
    m.delayMute = load(s, delayMute) > 0.5f;
    m.delayToReverb = load(s, delayToReverb);
    m.delayToBeads = load(s, delayToBeads);
    m.reverbLevel = load(s, reverbLevel);
    m.reverbMute = load(s, reverbMute) > 0.5f;
    return m;
}

sound::RingsParams readRingsParams(const juce::AudioProcessorValueTreeState& s)
{
    sound::RingsParams r;
    r.on = load(s, ringsOn) > 0.5f;
    r.source = juce::jlimit(0, sound::kNumSynths - 1, static_cast<int>(load(s, ringsSource)));
    r.model = juce::jlimit(0, 5, static_cast<int>(load(s, ringsModel)));
    const int poly = juce::jlimit(0, 2, static_cast<int>(load(s, ringsPolyphony)));
    r.polyphony = poly == 0 ? 1 : poly == 1 ? 2 : 4;
    r.octave = juce::jlimit(-2, 2, juce::roundToInt(load(s, ringsOctave)));
    r.structure = load(s, ringsStructure);
    r.brightness = load(s, ringsBrightness);
    r.damping = load(s, ringsDamping);
    r.position = load(s, ringsPosition);
    r.exciter = juce::jlimit(0, 2, static_cast<int>(load(s, ringsExciter)));
    r.level = load(s, ringsLevel);
    r.reverb = load(s, ringsReverb);
    r.delay = load(s, ringsDelay);
    r.beads = load(s, ringsBeads);
    r.tonic = static_cast<float>(juce::jlimit(0, 11, static_cast<int>(load(s, chordKey)))); // the song's key
    return r;
}

sound::CharacterParams readCharacterParams(const juce::AudioProcessorValueTreeState& s)
{
    sound::CharacterParams c;
    c.dirt = load(s, dirt);
    c.grit = static_cast<sound::Grit>(juce::jlimit(0, static_cast<int>(sound::Grit::Count) - 1, static_cast<int>(load(s, grit))));
    c.alive = load(s, alive);
    return c;
}

namespace {
struct TargetTable
{
    juce::StringArray ids, names;
};
TargetTable& targetTable()
{
    static TargetTable table;
    return table;
}
} // namespace

void registerParameterTargets(const juce::AudioProcessor& processor)
{
    // Every instance has the same parameters: the table is filled once, and a host
    // that creates two instances at the same moment waits for it to be complete.
    static std::once_flag once;
    std::call_once(once, [&processor] {
    auto& table = targetTable();
    static const char* const excluded[] = { "modTgt", "modAmt", "macTgt", "macAmt", "note", "chan", "arpChan", "arpShift", "chChan", "chShift",
                                            "kitOn", "freeRun", "partSolo", "sepOuts", "chLoop", "chFrozen" };
    for (auto* param : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
        {
            const auto base = withId->paramID.trimCharactersAtEnd("0123456789_").trimCharactersAtEnd("0123456789");
            bool skip = false;
            for (const char* e : excluded)
                skip = skip || base == e;
            if (!skip && table.ids.size() < kMaxTargets - mod::kNumTargets)
            {
                table.ids.add(withId->paramID);
                table.names.add(withId->getName(64));
            }
            // The reverb type is gone (one reverb now): its place stays taken, so routings
            // saved before still point at the parameters that followed it.
            if (withId->paramID == fxShimmer)
            {
                table.ids.add("fxRevType");
                table.names.add("(unused)");
            }
        }
    });
}

int numParameterTargets() { return targetTable().ids.size(); }

int parameterTarget(const juce::String& paramId)
{
    const int index = targetTable().ids.indexOf(paramId);
    return index >= 0 ? mod::kNumTargets + index : -1;
}

const juce::String& parameterTargetId(int target)
{
    static const juce::String none;
    const int index = target - mod::kNumTargets;
    return index >= 0 && index < targetTable().ids.size() ? targetTable().ids.getReference(index) : none;
}

juce::String targetDisplayName(int target)
{
    if (target >= 0 && target < mod::kNumTargets)
        return mod::targetName(target);
    const int index = target - mod::kNumTargets;
    return index >= 0 && index < targetTable().names.size() ? targetTable().names[index] : juce::String("None");
}

void setActiveOffsets(const ParamOffsets* offsets) { activeOffsets = offsets; }

} // namespace gristo::params
