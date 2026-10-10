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

private:
    gristo::BlockInfo transportFor(int numSamples);

    gristo::Engine engine_;
    gristo::PreviewKit kit_;
    std::vector<gristo::OutputEvent> events_;
    double sampleRate_ = 48000.0;
    double freePpq_ = 0.0;
    bool wasFreeRunning_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GristoProcessor)
};
