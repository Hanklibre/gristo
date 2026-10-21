#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace p = gristo::params;

GristoProcessor::GristoProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "GRISTO", p::createLayout())
{
    resetLanes();
    presets.markClean();
}

void GristoProcessor::resetLanes()
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
    sound_.prepare(sampleRate);
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
    keyboardState.processNextMidiBuffer(midi, 0, numSamples, true); // on-screen keys join MIDI in
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

    int notes[gristo::kNumVoices], channels[gristo::kChordVoice + 1];
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        notes[v] = static_cast<int>(state.getRawParameterValue(p::voiceId(p::note, v))->load());
        channels[v] = static_cast<int>(state.getRawParameterValue(p::voiceId(p::voiceChannel, v))->load());
    }
    int shifts[gristo::kNumArps + 1]; // arps, then the chords
    channels[gristo::kChordVoice] = static_cast<int>(state.getRawParameterValue(p::chordChannel)->load());
    shifts[gristo::kNumArps] = static_cast<int>(state.getRawParameterValue(p::chordShift)->load());
    for (int a = 0; a < gristo::kNumArps; ++a)
    {
        channels[gristo::kArpVoiceBase + a] = static_cast<int>(state.getRawParameterValue(p::laneId(p::arpChannel, a))->load());
        shifts[a] = static_cast<int>(state.getRawParameterValue(p::laneId(p::arpShift, a))->load());
    }

    const bool kitOn = state.getRawParameterValue(p::kitOn)->load() > 0.5f;
    if (kitOn && !soundWasOn_)
        sound_.reset(); // nothing stale from while the sound was off
    soundWasOn_ = kitOn;
    if (block.playing)
        lastBpm_ = block.bpm;
    if (kitOn)
    {
        // Modulation lanes on the sounds, sampled once per block (at its 16th).
        const auto step = static_cast<std::int64_t>(std::floor(block.ppqStart * gristo::kStepsPerBeat));
        auto lane = [&](int target) {
            return block.playing ? engine_.modulationAt(target, step) * gristo::mod::targetRange(target) : 0.0f;
        };
        auto unit = [](float v) { return juce::jlimit(0.0f, 1.0f, v); };
        namespace mod = gristo::mod;

        auto kit = p::readKitParams(state);
        kit.tune = juce::jlimit(-12.0f, 12.0f, kit.tune + lane(mod::kitTarget(mod::kKitTune)));
        kit.decay = unit(kit.decay + lane(mod::kitTarget(mod::kKitDecay)));
        kit.tone = unit(kit.tone + lane(mod::kitTarget(mod::kKitTone)));
        kit.drive = unit(kit.drive + lane(mod::kitTarget(mod::kKitDrive)));
        kit.reverbSend = unit(kit.reverbSend + lane(mod::kitTarget(mod::kKitReverb)));
        kit.delaySend = unit(kit.delaySend + lane(mod::kitTarget(mod::kKitDelay)));
        sound_.setKit(kit);

        const gristo::EngineParams modulated = engine_.effectiveParams(step);
        for (int a = 0; a < gristo::sound::kNumSynths; ++a)
        {
            auto synth = p::readSynthParams(state, a);
            if (a < gristo::kNumArps)
                synth.cutoffMod = block.playing ? modulated.arps[static_cast<size_t>(a)].cutoffOffset : 0.0f;
            else
                synth.cutoffMod = lane(mod::kChordCutoffTarget);
            synth.resonance = unit(synth.resonance + lane(mod::synthTarget(a, mod::kSynthReso)));
            synth.envAmount = unit(synth.envAmount + lane(mod::synthTarget(a, mod::kSynthEnv)));
            synth.attack = unit(synth.attack + lane(mod::synthTarget(a, mod::kSynthAttack)));
            synth.release = unit(synth.release + lane(mod::synthTarget(a, mod::kSynthRelease)));
            synth.level = unit(synth.level + lane(mod::synthTarget(a, mod::kSynthLevel)));
            synth.reverbSend = unit(synth.reverbSend + lane(mod::synthTarget(a, mod::kSynthReverb)));
            synth.delaySend = unit(synth.delaySend + lane(mod::synthTarget(a, mod::kSynthDelay)));
            sound_.setSynth(a, synth);
        }
        auto fx = p::readFxParams(state);
        fx.bpm = lastBpm_;
        fx.reverbSize = unit(fx.reverbSize + lane(mod::kFxSizeTarget));
        fx.delayFeedback = juce::jlimit(0.0f, 0.9f, fx.delayFeedback + lane(mod::kFxFeedbackTarget));
        sound_.setFx(fx);
    }
    const float kitGain = state.getRawParameterValue(p::kitGain)->load();
    float* left = buffer.getWritePointer(0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    int rendered = 0;
    auto renderKitTo = [&](int end) {
        if (!kitOn || end <= rendered)
            return;
        if (right != nullptr)
        {
            sound_.render(left + rendered, right + rendered, end - rendered, kitGain);
        }
        else
        {
            // Mono output: both sides are summed into the one channel at half level.
            sound_.render(left + rendered, left + rendered, end - rendered, kitGain * 0.5f);
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
                    sound_.noteOn(arp, note, static_cast<float>(note - shifts[arp]), ev.velocity);
                }
                else
                    sound_.triggerDrum(ev.voice, ev.velocity);
            }
        }
        else
        {
            midi.addEvent(juce::MidiMessage::noteOff(channel, note), ev.sampleOffset);
            if (isArp)
                sound_.noteOff(ev.voice - gristo::kArpVoiceBase, note);
        }
    }
    renderKitTo(numSamples);

    for (int a = 0; a < gristo::kNumArps; ++a)
        displayArpCount[static_cast<size_t>(a)].store(engine_.arpCount(a));
    displayPlaying.store(block.playing);
    displayChordSlot.store(block.playing ? engine_.chordPositionAt(static_cast<std::int64_t>(std::floor(block.ppqStart * gristo::kStepsPerBeat))).slot : -1);
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

void GristoProcessor::writeLanes(juce::ValueTree& lanes) const
{
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
    {
        juce::StringArray values;
        for (int k = 0; k < gristo::kMaxModPoints; ++k)
            values.add(juce::String(p::lanePoint(modPoints, lane, k).load(), 3));
        lanes.setProperty("lane" + juce::String(lane), values.joinIntoString(","), nullptr);
    }
}

void GristoProcessor::readLanes(const juce::ValueTree& lanes)
{
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
    {
        juce::StringArray values;
        values.addTokens(lanes.getProperty("lane" + juce::String(lane)).toString(), ",", {});
        for (int k = 0; k < gristo::kMaxModPoints && k < values.size(); ++k)
            p::lanePoint(modPoints, lane, k).store(juce::jlimit(0.0f, 1.0f, values[k].getFloatValue()));
    }
}

void GristoProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto tree = state.copyState();
    // Drawn lanes, UI size and the preset name are not parameters: store them alongside.
    auto lanes = tree.getOrCreateChildWithName("MODLANES", nullptr);
    writeLanes(lanes);
    tree.setProperty("uiScale", uiScale.load(), nullptr);
    tree.setProperty("showKeys", showKeys.load(), nullptr);
    presets.writeTo(tree);
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
            if (lanes.isValid())
                readLanes(lanes);
            if (tree.hasProperty("uiScale"))
                uiScale.store(static_cast<float>(tree.getProperty("uiScale")));
            if (tree.hasProperty("showKeys"))
                showKeys.store(static_cast<bool>(tree.getProperty("showKeys")));
            state.replaceState(tree);
            presets.readFrom(tree);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GristoProcessor();
}
