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
        layout.add(boolParam(voiceId(mute, v), name + " Mute", false));
        layout.add(percentParam(voiceId(probabilityRandom, v), name + " Probability Random", 0.0f));
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
    layout.add(floatParam(shuffle, "Shuffle", { 0.0f, 6.0f }, 2.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { feeling, kVersion }, "Feeling",
                                                            juce::StringArray { "Tight", "Rolls", "Flam", "Slop" }, 0));
    layout.add(floatParam(feelMs, "Flam / Slop", { 0.0f, 30.0f }, 9.0f, "ms", 1));
    layout.add(percentParam(shuffleRandom, "Shuffle Random", 0.0f));
    layout.add(percentParam(feelRandom, "Flam Random", 0.0f));

    // Humanize + dynamics
    layout.add(floatParam(humanize, "Humanize", { 0.0f, 1.0f }, 0.3f));
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
                                                                  name + " Trigger " + voiceName(v), v == (a == 0 ? Kick : HiHat1)));
        layout.add(std::make_unique<juce::AudioParameterChoice>(id(arpReset), name + " Reset",
                                                                juce::StringArray { "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" }, 2));
        layout.add(std::make_unique<juce::AudioParameterInt>(id(arpChannel), name + " MIDI Channel", 1, 16, a + 1));
        layout.add(std::make_unique<juce::AudioParameterBool>(id(arpLatch), name + " Latch", true));
        layout.add(std::make_unique<juce::AudioParameterChoice>(id(arpOrder), name + " Order", orders, 0));
        layout.add(std::make_unique<juce::AudioParameterInt>(id(arpOctaves), name + " Octaves", 1, 4, 1));
        layout.add(std::make_unique<juce::AudioParameterInt>(id(arpRoot), name + " Root", 24, 96, a == 0 ? 36 : 60, noteAttributes));
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
    }
    layout.add(boolParam(kitOn, "Preview Kit", true));
    layout.add(floatParam(kitGain, "Kit Level", { 0.0f, 1.0f }, 0.7f));

    // Free-running clock for standalone use / auditioning without host transport.
    layout.add(boolParam(freeRun, "Free Run", false));
    layout.add(floatParam(freeTempo, "Free Tempo", { 40.0f, 240.0f, 0.1f }, 95.0f, "bpm", 1));
    // Modulation lanes
    juce::StringArray targetNames;
    for (int t = 0; t < mod::kNumTargets; ++t)
        targetNames.add(mod::targetName(t));
    for (int lane = 0; lane < kNumModLanes; ++lane)
    {
        const juce::String laneName = "Lane " + juce::String(lane + 1);
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { laneId(modBars, lane), kVersion }, laneName + " Length",
                                                                juce::StringArray { "1 bar", "2 bars", "4 bars" }, 0));
        for (int slot = 0; slot < kModSlotsPerLane; ++slot)
        {
            const juce::String slotName = laneName + " Slot " + juce::String(slot + 1);
            layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { slotId(modTarget, lane, slot), kVersion },
                                                                    slotName + " Target", targetNames, 0));
            auto attributes = juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) {
                const int pct = juce::roundToInt(v * 100.0f);
                return (pct > 0 ? "+" : "") + juce::String(pct) + "%";
            }).withValueFromStringFunction([](const juce::String& t) { return t.getFloatValue() / 100.0f; });
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { slotId(modAmount, lane, slot), kVersion },
                                                                   slotName + " Amount", juce::NormalisableRange<float> { -1.0f, 1.0f }, 0.0f,
                                                                   attributes));
        }
    }
    return layout;
}

EngineParams readEngineParams(const juce::AudioProcessorValueTreeState& s, const ModPointStore& lanes)
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
        p.pattern.mute[i] = load(s, voiceId(mute, v)) > 0.5f;
        p.pattern.probabilityRandom[i] = load(s, voiceId(probabilityRandom, v));
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
        for (int k = 0; k < kMaxModPoints; ++k)
            l.points[static_cast<size_t>(k)] = lanePoint(lanes, lane, k).load(std::memory_order_relaxed);
        for (int slot = 0; slot < kModSlotsPerLane; ++slot)
        {
            ModSlot& sl = l.slots[static_cast<size_t>(slot)];
            sl.target = static_cast<int>(load(s, slotId(modTarget, lane, slot)));
            sl.amount = load(s, slotId(modAmount, lane, slot));
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
        const float lo = load(s, laneId(arpVelMin, a)) / 127.0f;
        const float hi = load(s, laneId(arpVelMax, a)) / 127.0f;
        ap.velMin = std::min(lo, hi);
        ap.velMax = std::max(lo, hi);
    }
    return p;
}

} // namespace gristo::params
