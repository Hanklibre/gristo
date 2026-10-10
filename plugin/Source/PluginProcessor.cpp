#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace p = gristo::params;

GristoProcessor::GristoProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "GRISTO", p::createLayout())
{
    // Starting curves (one bar, repeated), so a lane does something as soon as it is routed.
    const gristo::ModShape defaults[gristo::kNumModLanes] = { gristo::ModShape::RampDown, gristo::ModShape::Sine,
                                                              gristo::ModShape::Square, gristo::ModShape::BeatSaw };
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
    {
        float bar[gristo::kModPointsPerBar] {};
        gristo::fillModShape(bar, gristo::kModPointsPerBar, defaults[lane]);
        for (int k = 0; k < gristo::kMaxModPoints; ++k)
            p::lanePoint(modPoints, lane, k).store(bar[k % gristo::kModPointsPerBar]);
    }
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
    handleMidiInput(midi);
    midi.clear(); // MIDI in is consumed (it feeds the arps); the output is Gristo's own

    engine_.setParams(p::readEngineParams(state, modPoints));
    for (int a = 0; a < gristo::kNumArps; ++a)
    {
        const bool latch = state.getRawParameterValue(p::laneId(p::arpLatch, a))->load() > 0.5f;
        const int* notes = latch ? latched_.data() : held_.data();
        const int count = latch ? latchedCount_ : heldCount_;
        engine_.setArpInput(a, notes, count);
        for (int i = 0; i < count; ++i)
            displayArpInput[static_cast<size_t>(a)][static_cast<size_t>(i)].store(notes[i]);
        displayArpInputCount[static_cast<size_t>(a)].store(count);
    }
    const gristo::BlockInfo block = transportFor(numSamples);

    events_.clear();
    engine_.process(block, events_);

    int notes[gristo::kNumVoices], channels[gristo::kNumVoices + gristo::kNumArps];
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        notes[v] = static_cast<int>(state.getRawParameterValue(p::voiceId(p::note, v))->load());
        channels[v] = static_cast<int>(state.getRawParameterValue(p::voiceId(p::voiceChannel, v))->load());
    }
    int shifts[gristo::kNumArps];
    for (int a = 0; a < gristo::kNumArps; ++a)
    {
        channels[gristo::kArpVoiceBase + a] = static_cast<int>(state.getRawParameterValue(p::laneId(p::arpChannel, a))->load());
        shifts[a] = static_cast<int>(state.getRawParameterValue(p::laneId(p::arpShift, a))->load());
    }

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
        const bool isArp = ev.voice >= gristo::kArpVoiceBase;
        const int note = isArp ? ev.note : notes[ev.voice];
        const int channel = channels[ev.voice];
        renderKitTo(ev.sampleOffset);
        if (ev.noteOn)
        {
            const auto velocity = static_cast<juce::uint8>(juce::jlimit(1, 127, juce::roundToInt(ev.velocity * 127.0f)));
            midi.addEvent(juce::MidiMessage::noteOn(channel, note, velocity), ev.sampleOffset);
            if (kitOn)
            {
                if (isArp)
                {
                    // The kit plays the unshifted pitch, as the synth behind a Pitch device would.
                    const int arp = ev.voice - gristo::kArpVoiceBase;
                    kit_.arpNoteOn(arp, note, note - shifts[arp], ev.velocity);
                }
                else
                    kit_.trigger(ev.voice, ev.velocity);
            }
        }
        else
        {
            midi.addEvent(juce::MidiMessage::noteOff(channel, note), ev.sampleOffset);
            if (isArp)
                kit_.arpNoteOff(ev.voice - gristo::kArpVoiceBase, note);
        }
    }
    renderKitTo(numSamples);

    for (int a = 0; a < gristo::kNumArps; ++a)
        displayArpCount[static_cast<size_t>(a)].store(engine_.arpCount(a));
    displayPlaying.store(block.playing);
    if (block.playing)
    {
        displayPpq.store(block.ppqStart);
        displayBpm.store(block.bpm);
    }
}

void GristoProcessor::handleMidiInput(const juce::MidiBuffer& midi)
{
    auto remove = [](std::array<int, gristo::kMaxHeldNotes>& list, int& count, int note) {
        for (int i = 0; i < count; ++i)
            if (list[static_cast<size_t>(i)] == note)
            {
                std::copy(list.begin() + i + 1, list.begin() + count, list.begin() + i);
                --count;
                return;
            }
    };
    auto add = [](std::array<int, gristo::kMaxHeldNotes>& list, int& count, int note) {
        for (int i = 0; i < count; ++i)
            if (list[static_cast<size_t>(i)] == note)
                return;
        if (count < gristo::kMaxHeldNotes)
            list[static_cast<size_t>(count++)] = note;
    };

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            // A key pressed after all keys were released starts a new latched chord.
            if (heldCount_ == 0)
                latchedCount_ = 0;
            add(held_, heldCount_, m.getNoteNumber());
            add(latched_, latchedCount_, m.getNoteNumber());
        }
        else if (m.isNoteOff())
        {
            remove(held_, heldCount_, m.getNoteNumber());
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            heldCount_ = 0;
        }
    }
}

juce::AudioProcessorEditor* GristoProcessor::createEditor()
{
    return new GristoEditor(*this);
}

void GristoProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto tree = state.copyState();
    // Drawn lanes and UI size are not parameters: store them in a child tree.
    auto lanes = tree.getOrCreateChildWithName("MODLANES", nullptr);
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
    {
        juce::StringArray values;
        for (int k = 0; k < gristo::kMaxModPoints; ++k)
            values.add(juce::String(p::lanePoint(modPoints, lane, k).load(), 3));
        lanes.setProperty("lane" + juce::String(lane), values.joinIntoString(","), nullptr);
    }
    tree.setProperty("uiScale", uiScale.load(), nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary(*xml, destData);
}

void GristoProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(state.state.getType()))
        {
            const auto tree = juce::ValueTree::fromXml(*xml);
            const auto lanes = tree.getChildWithName("MODLANES");
            for (int lane = 0; lane < gristo::kNumModLanes && lanes.isValid(); ++lane)
            {
                juce::StringArray values;
                values.addTokens(lanes.getProperty("lane" + juce::String(lane)).toString(), ",", {});
                for (int k = 0; k < gristo::kMaxModPoints && k < values.size(); ++k)
                    p::lanePoint(modPoints, lane, k).store(juce::jlimit(0.0f, 1.0f, values[k].getFloatValue()));
            }
            if (tree.hasProperty("uiScale"))
                uiScale.store(static_cast<float>(tree.getProperty("uiScale")));
            state.replaceState(tree);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GristoProcessor();
}
