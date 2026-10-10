#pragma once

#include "Parameters.h"
#include "PreviewKit.h"

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

class GristoProcessor : public juce::AudioProcessor
{
public:
    GristoProcessor();
    ~GristoProcessor() override = default;

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
    std::atomic<float> uiScale { 0.0f }; // 0 = pick a size that fits the screen

    // Arp display: advances since the last reset, and the notes feeding each arp.
    std::array<std::atomic<std::int64_t>, gristo::kNumArps> displayArpCount {};
    std::array<std::array<std::atomic<int>, gristo::kMaxHeldNotes>, gristo::kNumArps> displayArpInput {};
    std::array<std::atomic<int>, gristo::kNumArps> displayArpInputCount {};

private:
    gristo::BlockInfo transportFor(int numSamples);

    gristo::Engine engine_;
    gristo::PreviewKit kit_;
    std::vector<gristo::OutputEvent> events_;
    double sampleRate_ = 48000.0;
    double freePpq_ = 0.0;

    // MIDI in: notes held now (press order) and the last chord, for Latch.
    void handleMidiInput(const juce::MidiBuffer& midi);
    std::array<int, gristo::kMaxHeldNotes> held_ {};
    int heldCount_ = 0;
    std::array<int, gristo::kMaxHeldNotes> latched_ {};
    int latchedCount_ = 0;
    bool wasFreeRunning_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GristoProcessor)
};
