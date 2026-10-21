#pragma once

#include <atomic>
#include <gristo/Engine.h>
#include <gristo/sound/Sound.h>
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
inline const juce::String arpProbability = "arpProb";
inline const juce::String arpProbabilityRandom = "arpProbRnd";
inline const juce::String arpFollow = "arpFollow";

// Chord sequencer (slot parameters: base + slot index)
inline const juce::String chordOn = "chOn";
inline const juce::String chordKey = "chKey";
inline const juce::String chordScale = "chScale";
inline const juce::String chordClock = "chClock";
inline const juce::String chordClockVoice = "chClockVoice";
inline const juce::String chordHits = "chHits";
inline const juce::String chordReset = "chReset";
inline const juce::String chordMutate = "chMutate";
inline const juce::String chordSeed = "chSeed";
inline const juce::String chordOctave = "chOct";
inline const juce::String chordPlay = "chPlay";
inline const juce::String chordStab = "chStab";
inline const juce::String chordGate = "chGate";
inline const juce::String chordVelMin = "chVelMin";
inline const juce::String chordVelMax = "chVelMax";
inline const juce::String chordChannel = "chChan";
inline const juce::String chordShift = "chShift";
inline const juce::String chordSlotUsed = "chSlot";
inline const juce::String chordDegree = "chDeg";
inline const juce::String chordBorrowed = "chBor";
inline const juce::String chordType = "chType";
inline const juce::String chordInversion = "chInv";
inline const juce::String chordQuality = "chQual";
inline const juce::String chordAlter = "chAlt";
inline const juce::String chordLength = "chLen";
inline const int kChordLengthSteps[] = { 8, 16, 32 };
inline const int kChordLowestNotes[] = { 36, 48, 60, 72 };
inline const int kResetBarChoices[] = { 1, 2, 4, 8, 16 };
// Built-in sounds (kitOn / kitGain are the master Sound switch and level)
inline const juce::String kitType = "kitType";
inline const juce::String kitTune = "kitTune";
inline const juce::String kitDecay = "kitDecay";
inline const juce::String kitTone = "kitTone";
inline const juce::String kitDrive = "kitDrive";
inline const juce::String drumLevel = "lvl"; // per voice
inline const juce::String kitReverb = "kitRev";
inline const juce::String kitDelay = "kitDly";
inline const juce::String synthSound = "arpSnd"; // per arp from here on
inline const juce::String synthCutoff = "arpCut";
inline const juce::String synthResonance = "arpRes";
inline const juce::String synthEnv = "arpEnv";
inline const juce::String synthAttack = "arpAtk";
inline const juce::String synthRelease = "arpRel";
inline const juce::String synthGlide = "arpGld";
inline const juce::String synthLevel = "arpLvl";
inline const juce::String synthReverb = "arpRev";
inline const juce::String synthDelay = "arpDly";
inline const juce::String fxSize = "fxSize";
inline const juce::String fxTime = "fxTime";
inline const juce::String fxFeedback = "fxFb";
inline const float kDelayBeatChoices[] = { 0.25f, 0.5f, 0.75f, 1.0f }; // 1/16, 1/8, dotted 1/8, 1/4
inline const juce::String kitOn = "kitOn";
inline const juce::String kitGain = "kitGain";
inline const juce::String freeRun = "freeRun";
inline const juce::String freeTempo = "freeTempo";
// Beads-style random modulation amounts
inline const juce::String mapRandom = "mapRnd";
inline const juce::String shuffleRandom = "shuffleRnd";
inline const juce::String feelRandom = "feelRnd";

// Indexed ids ("fill2", "modTgt1_3"). Built once per thread and cached, so the audio
// thread doesn't allocate a string for every parameter it reads each block.
juce::String voiceId(const juce::String& base, int voice);
juce::String laneId(const juce::String& base, int lane);
juce::String slotId(const juce::String& base, int lane, int slot);

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

// Built-in sound settings (also lock-free).
sound::KitParams readKitParams(const juce::AudioProcessorValueTreeState& state);
sound::SynthParams readSynthParams(const juce::AudioProcessorValueTreeState& state, int arp);
sound::FxParams readFxParams(const juce::AudioProcessorValueTreeState& state);

} // namespace gristo::params
