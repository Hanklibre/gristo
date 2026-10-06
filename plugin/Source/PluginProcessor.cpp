#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace p = gristo::params;

GristoProcessor::GristoProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "GRISTO", p::createLayout())
{
}

bool GristoProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void GristoProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sampleRate_ = sampleRate;
    engine_.reset();
    kit_.prepare(sampleRate);
    events_.clear();
    events_.reserve(static_cast<size_t>(std::max(512, samplesPerBlock)));
    freePpq_ = 0.0;
    wasFreeRunning_ = false;
}

gristo::BlockInfo GristoProcessor::transportFor(int numSamples)
{
    gristo::BlockInfo b;
    b.sampleRate = sampleRate_;
    b.numSamples = numSamples;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto pos = hostPlayHead->getPosition(); pos.hasValue() && pos->getIsPlaying())
        {
            if (const auto ppq = pos->getPpqPosition())
            {
                b.playing = true;
                b.ppqStart = *ppq;
                b.bpm = pos->getBpm().orFallback(120.0);
                wasFreeRunning_ = false;
                return b;
            }
        }
    }

    // No running host transport: optionally run from the internal clock.
    const bool freeRun = state.getRawParameterValue(p::freeRun)->load() > 0.5f;
    if (!freeRun)
    {
        wasFreeRunning_ = false;
        return b;
    }
    if (!wasFreeRunning_)
        freePpq_ = 0.0;
    wasFreeRunning_ = true;

    b.playing = true;
    b.bpm = state.getRawParameterValue(p::freeTempo)->load();
    b.ppqStart = freePpq_;
    freePpq_ += numSamples * b.bpm / (60.0 * sampleRate_);
    return b;
}

void GristoProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    midi.clear(); // incoming MIDI is not used yet (pattern switching is on the roadmap)

    engine_.setParams(p::readEngineParams(state));
    const gristo::BlockInfo block = transportFor(numSamples);

    events_.clear();
    engine_.process(block, events_);

    const int channel = static_cast<int>(state.getRawParameterValue(p::midiChannel)->load());
    int notes[gristo::kNumVoices];
    for (int v = 0; v < gristo::kNumVoices; ++v)
        notes[v] = static_cast<int>(state.getRawParameterValue(p::voiceId(p::note, v))->load());

    const bool kitOn = state.getRawParameterValue(p::kitOn)->load() > 0.5f;
    const float kitGain = state.getRawParameterValue(p::kitGain)->load();
    float* left = buffer.getWritePointer(0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    int rendered = 0;
    auto renderKitTo = [&](int end) {
        if (!kitOn || end <= rendered)
            return;
        if (right != nullptr)
        {
            kit_.render(left + rendered, right + rendered, end - rendered, kitGain);
        }
        else
        {
            // Mono output: both sides are summed into the one channel at half level.
            kit_.render(left + rendered, left + rendered, end - rendered, kitGain * 0.5f);
        }
        rendered = end;
    };

    for (const auto& ev : events_)
    {
        const int note = notes[ev.voice];
        if (ev.noteOn)
        {
            const auto velocity = static_cast<juce::uint8>(juce::jlimit(1, 127, juce::roundToInt(ev.velocity * 127.0f)));
            midi.addEvent(juce::MidiMessage::noteOn(channel, note, velocity), ev.sampleOffset);
            renderKitTo(ev.sampleOffset);
            if (kitOn)
                kit_.trigger(ev.voice, ev.velocity);
        }
        else
        {
            midi.addEvent(juce::MidiMessage::noteOff(channel, note), ev.sampleOffset);
        }
    }
    renderKitTo(numSamples);

    displayPlaying.store(block.playing);
    if (block.playing)
    {
        displayPpq.store(block.ppqStart);
        displayBpm.store(block.bpm);
    }
}

juce::AudioProcessorEditor* GristoProcessor::createEditor()
{
    return new GristoEditor(*this);
}

void GristoProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void GristoProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(state.state.getType()))
            state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GristoProcessor();
}
