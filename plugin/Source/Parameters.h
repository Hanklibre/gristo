#pragma once

#include <atomic>
#include <gristo/Engine.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace gristo::params {

// Parameter IDs. Per-voice parameters are suffixed with the voice index (0..5).
inline const juce::String mapX = "mapX";
inline const juce::String mapY = "mapY";
inline const juce::String chaos = "chaos";
inline const juce::String fill = "fill";
inline const juce::String probability = "prob";
inline const juce::String fillRandom = "fillRnd";
inline const juce::String voiceHumanize = "human";
inline const juce::String voiceDynamics = "dyn";
inline const juce::String mute = "mute";
inline const juce::String probabilityRandom = "probRnd";
inline const juce::String velMin = "velMin";
inline const juce::String velMax = "velMax";
// Modulation (performer lanes): bars per lane, and per lane x slot a target + amount.
inline const juce::String modBars = "modBars";
inline const juce::String modTarget = "modTgt";
inline const juce::String modAmount = "modAmt";
inline const juce::String note = "note";
inline const juce::String linear = "linear";
inline const juce::String timing = "timing";
inline const juce::String shuffle = "shuffle";
inline const juce::String feeling = "feeling";
inline const juce::String feelMs = "feelMs";
inline const juce::String humanize = "humanize";
inline const juce::String humanizeLock = "humanizeLock";
inline const juce::String dynamics = "dynamics";
inline const juce::String variation = "variation";
inline const juce::String noteLength = "noteLength";
inline const juce::String voiceChannel = "chan"; // per voice MIDI channel

// Arps (prefix + arp index; triggers: arpTrig<arp>_<voice>)
inline const juce::String arpOn = "arpOn";
inline const juce::String arpMode = "arpMode";
inline const juce::String arpTrigger = "arpTrig";
inline const juce::String arpReset = "arpReset";
inline const juce::String arpChannel = "arpChan";
inline const juce::String arpLatch = "arpLatch";
inline const juce::String arpOrder = "arpOrder";
inline const juce::String arpOctaves = "arpOct";
inline const juce::String arpRoot = "arpRoot";
inline const juce::String arpScale = "arpScale";
inline const juce::String arpLength = "arpLen";
inline const juce::String arpDensity = "arpDens";
inline const juce::String arpAccent = "arpAcc";
inline const juce::String arpSlide = "arpSlide";
inline const juce::String arpMutate = "arpMut";
inline const juce::String arpSeed = "arpSeed";
inline const juce::String arpGate = "arpGate";
inline const juce::String arpVelMin = "arpVelMin";
inline const juce::String arpVelMax = "arpVelMax";
inline const juce::String arpShift = "arpShift";
inline const int kResetBarChoices[] = { 1, 2, 4, 8, 16 };
inline const juce::String kitOn = "kitOn";
inline const juce::String kitGain = "kitGain";
inline const juce::String freeRun = "freeRun";
inline const juce::String freeTempo = "freeTempo";
// Beads-style random modulation amounts
inline const juce::String mapRandom = "mapRnd";
inline const juce::String shuffleRandom = "shuffleRnd";
inline const juce::String feelRandom = "feelRnd";

inline juce::String voiceId(const juce::String& base, int voice) { return base + juce::String(voice); }
inline juce::String laneId(const juce::String& base, int lane) { return base + juce::String(lane); }
inline juce::String slotId(const juce::String& base, int lane, int slot) { return base + juce::String(lane) + "_" + juce::String(slot); }

// Drawn lane curves live outside the parameter tree (they are not automatable):
// written by the editor, read lock-free by the audio thread.
using ModPointStore = std::array<std::atomic<float>, kNumModLanes * kMaxModPoints>;
inline std::atomic<float>& lanePoint(ModPointStore& store, int lane, int index) { return store[static_cast<size_t>(lane * kMaxModPoints + index)]; }
inline const std::atomic<float>& lanePoint(const ModPointStore& store, int lane, int index) { return store[static_cast<size_t>(lane * kMaxModPoints + index)]; }
inline int barsForChoice(int choice) { return choice <= 0 ? 1 : choice == 1 ? 2 : 4; }

// "C1 · 36" (middle C = C3, the Ableton / Cubase convention; the number is unambiguous).
inline juce::String noteDisplayName(int noteNumber)
{
    return juce::MidiMessage::getMidiNoteName(noteNumber, true, true, 3) + juce::String::fromUTF8(" \xc2\xb7 ") + juce::String(noteNumber);
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Reads the current parameter values into engine parameters. Lock-free (atomic
// loads only), so it is safe on both the audio and the message thread.
EngineParams readEngineParams(const juce::AudioProcessorValueTreeState& state, const ModPointStore& lanes);

} // namespace gristo::params
