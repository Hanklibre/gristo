#pragma once

#include "Parameters.h"
#include "Presets.h"

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>

class GristoProcessor : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener, private juce::Timer
{
public:
    GristoProcessor();
    ~GristoProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState state;

    // Read by the editor for the playhead display.
    std::atomic<double> displayPpq { 0.0 };
    std::atomic<double> displayBpm { 120.0 };
    std::atomic<bool> displayPlaying { false };

    // Drawn modulation lane curves (see Parameters.h) and the editor's zoom.
    gristo::params::ModPointStore modPoints;
    void writeLanes(juce::ValueTree& lanes) const;
    void readLanes(const juce::ValueTree& lanes);
    void resetLanes(); // the starting curves

    PresetManager presets { *this };

    // The pattern as a MIDI file, `bars` long from bar 1, exactly as it plays now
    // (same groove, humanize, lanes, held notes). `part` is 0..kNumParts-1, kAllDrums
    // for the six drums on one track, or kAllParts for one track per sounding part.
    // Pitches are the musical ones, ready to drop on an instrument track: arps and
    // chords without their Out Shift, drums moved up from the split range to GM.
    // Message thread.
    // Every note of every part between two positions, as it plays now (for the
    // timeline). Muted parts are included. Message thread.
    struct NoteSpan
    {
        int part = 0;
        double start = 0.0, end = 0.0; // beats
        float velocity = 0.0f;
        int pitch = -1; // arps and chords
    };
    std::vector<NoteSpan> renderSpans(double startBeats, double endBeats) const;
    std::uint32_t parameterGeneration() const { return paramGeneration_.load(std::memory_order_relaxed); }

    // Runs the message-thread housekeeping (undo history, scenes, macro CCs) `ticks`
    // times, as its 100 Hz timer would. For tests, which have no message loop.
    void pump(int ticks) { for (int i = 0; i < ticks; ++i) timerCallback(); }

    // --- Undo: every finished edit (and preset / scene / dice) is one step back.
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    void undo();
    void redo();

    // --- Scenes: eight snapshots of everything musical. A recall waits for the next
    // bar while playing. Chain plays the stored scenes in order, each for chainBars.
    // MIDI: notes C1..G1 on channel 16 recall scenes 1..8.
    static constexpr int kNumScenes = 8;
    bool sceneUsed(int scene) const { return scene >= 0 && scene < kNumScenes && !scenes_[static_cast<size_t>(scene)].values.empty(); }
    void storeScene(int scene);
    void clearScene(int scene);
    void recallScene(int scene) { if (sceneUsed(scene)) pendingScene.store(scene); }
    std::atomic<int> currentScene { -1 }, pendingScene { -1 };
    std::atomic<bool> chainScenes { false };
    std::atomic<int> chainBars { 4 };
    void writeScenes(juce::ValueTree& parent) const;
    void readScenes(const juce::ValueTree& parent); // clears the scenes if there are none

    // --- Dice: new values for one section, or for every section that isn't locked.
    enum Section { DicePattern = 0, DiceGroove, DiceKit, DiceArp1, DiceArp2, DiceArp3, DiceChords,
                   DiceSound1, DiceSound2, DiceSound3, DiceSound4, NumSections };
    void dice(int section);
    void diceAll();
    std::atomic<int> diceLocks { 0 }; // bit per Section

    // --- Your own drum samples: a one-shot per voice, kept in Documents/Gristo/Samples
    // (files chosen elsewhere are copied there) and remembered by name.
    static juce::File samplesFolder();
    bool loadDrumSample(int voice, const juce::URL& source); // message thread; false if it can't be read
    void clearDrumSample(int voice);
    juce::String drumSampleName(int voice) const; // empty: the kit's own sound
    void writeSamples(juce::ValueTree& parent) const;
    void readSamples(const juce::ValueTree& parent);

    // --- Macro MIDI learn: the next CC on MIDI in is bound to the macro.
    std::atomic<int> learnMacro { -1 };
    std::array<std::atomic<int>, gristo::kNumMacros> macroCc;

    static constexpr int kAllParts = -1, kAllDrums = -2;
    juce::MidiFile exportMidi(int bars, int part) const;
    juce::File exportMidiToTempFile(int bars, int part) const; // for drag and drop; empty on failure

    // On-screen keyboard (feeds the arps like MIDI in) and whether it's shown.
    juce::MidiKeyboardState keyboardState;
    std::atomic<bool> showKeys { false }; // the chord pads cover chords on touch screens
    std::atomic<float> uiScale { 0.0f }; // 0 = pick a size that fits the screen

    // Arp display: advances since the last reset, and the notes feeding each arp.
    std::array<std::atomic<std::int64_t>, gristo::kNumArps> displayArpCount {};
    std::array<std::array<std::atomic<int>, gristo::kMaxHeldNotes>, gristo::kNumArps> displayArpInput {};
    std::array<std::atomic<int>, gristo::kNumArps> displayArpInputCount {};
    std::atomic<int> displayChordSlot { -1 }; // slot playing now (-1 = none / stopped)
    std::array<std::atomic<float>, gristo::kNumParts> displayActivity {}; // velocity of the latest note-on; the editor clears it

private:
    gristo::BlockInfo transportFor(int numSamples);

    // Everything the audio thread needs from the parameters, re-read only after a change.
    struct Settings
    {
        gristo::EngineParams engine;
        gristo::sound::KitParams kit;
        gristo::sound::SynthParams synths[gristo::sound::kNumSynths];
        gristo::sound::FxParams fx;
        int notes[gristo::kNumVoices] {}, channels[gristo::kNumParts] {}, shifts[gristo::kNumArps + 1] {};
        bool latch[gristo::kNumArps] {};
        bool kitOn = true, freeRun = false, soundModulated = false, separateOuts = false;
        float kitGain = 0.7f, freeTempo = 95.0f;
    };
    void parameterChanged(const juce::String& id, float value) override;
    void refreshSettings();
    void applySoundSettings(std::int64_t step, bool playing); // lanes applied to the sound knobs at `step`
    Settings settings_;

    void timerCallback() override; // message thread: undo history, scenes, macro CCs
    void setValue(const juce::String& id, float value);
    std::vector<Snapshot> undo_, redo_;
    Snapshot lastSnapshot_;
    int historyTick_ = 0;
    std::array<Snapshot, kNumScenes> scenes_;

    bool setDrumSampleFile(int voice, const juce::File& file);
    mutable juce::SpinLock sampleLock_;
    std::array<std::shared_ptr<gristo::sound::SampleData>, gristo::kNumVoices> samples_;
    std::array<juce::String, gristo::kNumVoices> sampleNames_;
    std::vector<std::shared_ptr<gristo::sound::SampleData>> retiredSamples_; // still in use by the audio thread for a moment
    std::atomic<bool> samplesChanged_ { false };
    std::int64_t sceneRequestBar_ = -1, chainQueuedBar_ = -1;
    int lastPending_ = -1;
    std::array<std::atomic<float>, gristo::kNumMacros> macroCcValue; // -1 = nothing new
    std::atomic<std::uint32_t> paramGeneration_ { 1 };
    std::uint32_t seenGeneration_ = 0;

    gristo::Engine engine_;
    gristo::sound::SoundEngine sound_;
    bool soundWasOn_ = true;
    double lastBpm_ = 120.0;
    std::vector<gristo::OutputEvent> events_;
    // What each note-on was actually sent as, so its note-off matches even if the
    // note or channel setting changed in between (no hanging notes in the host).
    std::array<std::pair<int, int>, gristo::kNumVoices> drumSent_ {}; // note, channel
    std::array<std::array<juce::int8, 128>, gristo::kChordVoice + 1> sentChannel_ {};
    double sampleRate_ = 48000.0;
    double freePpq_ = 0.0;

    // MIDI in: notes held now (press order) and the last chord, for Latch.
    void handleMidiInput(const juce::MidiBuffer& midi);
    static int chordKeyIndex(int note); // -1: not a chord key, -2: a black key between them
    std::array<int, gristo::kMaxHeldNotes> held_ {};
    int heldCount_ = 0;
    std::array<int, gristo::kMaxHeldNotes> latched_ {};
    int latchedCount_ = 0;
    // Chords' Keys clock: the white keys from C1 held now (index of the chord, press order).
    std::array<int, gristo::kMaxHeldNotes> chordKeys_ {};
    int chordKeyCount_ = 0;
    bool wasFreeRunning_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GristoProcessor)
};
