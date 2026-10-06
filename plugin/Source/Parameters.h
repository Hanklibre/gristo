#pragma once

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
inline const juce::String velLow = "velLow";
inline const juce::String velHigh = "velHigh";
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
inline const juce::String midiChannel = "midiChannel";
inline const juce::String kitOn = "kitOn";
inline const juce::String kitGain = "kitGain";
inline const juce::String freeRun = "freeRun";
inline const juce::String freeTempo = "freeTempo";
// Beads-style random modulation amounts
inline const juce::String mapRandom = "mapRnd";
inline const juce::String shuffleRandom = "shuffleRnd";
inline const juce::String feelRandom = "feelRnd";

inline juce::String voiceId(const juce::String& base, int voice) { return base + juce::String(voice); }

// "C1 · 36" (middle C = C3, the Ableton / Cubase convention; the number is unambiguous).
inline juce::String noteDisplayName(int noteNumber)
{
    return juce::MidiMessage::getMidiNoteName(noteNumber, true, true, 3) + juce::String::fromUTF8(" \xc2\xb7 ") + juce::String(noteNumber);
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Reads the current parameter values into engine parameters. Lock-free (atomic
// loads only), so it is safe on both the audio and the message thread.
EngineParams readEngineParams(const juce::AudioProcessorValueTreeState& state);

} // namespace gristo::params
