#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <tuple>

namespace p = gristo::params;

namespace {
// Version 2 renumbered the parts, synths and modulation targets: older projects open
// with the default patch instead of a scrambled one.
constexpr int kStateVersion = 2;
} // namespace

GristoProcessor::GristoProcessor()
    // Main mix, then one optional stereo output per part (see Separate Outputs).
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Kit", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Arp 1", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Arp 2", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Arp 3", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Chords", juce::AudioChannelSet::stereo(), false)
                         .withOutput("FX", juce::AudioChannelSet::stereo(), false)),
      state(*this, nullptr, "GRISTO", p::createLayout())
{
    resetLanes();
    presets.markClean();
    for (auto* param : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            state.addParameterListener(withId->paramID, this);
    for (auto& cc : macroCc)
        cc.store(-1);
    for (auto& v : macroCcValue)
        v.store(-1.0f);
    lastSnapshot_ = presets.captureSnapshot();
    startTimerHz(100);
}

GristoProcessor::~GristoProcessor()
{
    stopTimer();
    for (auto* param : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            state.removeParameterListener(withId->paramID, this);
}

void GristoProcessor::parameterChanged(const juce::String&, float) { paramGeneration_.fetch_add(1, std::memory_order_relaxed); }

void GristoProcessor::refreshSettings()
{
    // Reading ~400 parameters by id is the expensive part of a block, so it only
    // happens when one changed. The drawn lanes are plain atomics: compared every block.
    const auto generation = paramGeneration_.load(std::memory_order_relaxed);
    bool changed = generation != seenGeneration_;
    if (changed)
    {
        seenGeneration_ = generation;
        auto& c = settings_;
        c.engine = p::readEngineParams(state, modPoints);
        c.kit = p::readKitParams(state);
        for (int sIndex = 0; sIndex < gristo::sound::kNumSynths; ++sIndex)
            c.synths[sIndex] = p::readSynthParams(state, sIndex);
        c.fx = p::readFxParams(state);
        auto value = [this](const juce::String& id) { return state.getRawParameterValue(id)->load(std::memory_order_relaxed); };
        for (int v = 0; v < gristo::kNumVoices; ++v)
        {
            c.notes[v] = static_cast<int>(value(p::voiceId(p::note, v)));
            c.channels[v] = static_cast<int>(value(p::voiceId(p::voiceChannel, v)));
        }
        c.channels[gristo::kChordVoice] = static_cast<int>(value(p::chordChannel));
        c.shifts[gristo::kNumArps] = static_cast<int>(value(p::chordShift));
        for (int a = 0; a < gristo::kNumArps; ++a)
        {
            c.channels[gristo::kArpVoiceBase + a] = static_cast<int>(value(p::laneId(p::arpChannel, a)));
            c.shifts[a] = static_cast<int>(value(p::laneId(p::arpShift, a)));
            c.latch[a] = value(p::laneId(p::arpLatch, a)) > 0.5f;
        }
        c.kitOn = value(p::kitOn) > 0.5f;
        c.separateOuts = value(p::separateOuts) > 0.5f;
        c.kitGain = value(p::kitGain);
        c.freeRun = value(p::freeRun) > 0.5f;
        c.freeTempo = value(p::freeTempo);
        c.soundModulated = false;
        for (const auto& lane : c.engine.modulation.lanes)
            for (const auto& slot : lane.slots)
                c.soundModulated = c.soundModulated || (slot.target >= gristo::mod::kSoundBase && std::abs(slot.amount) > 0.0f);
    }
    else
    {
        for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
        {
            auto& points = settings_.engine.modulation.lanes[static_cast<size_t>(lane)].points;
            for (int k = 0; k < gristo::kMaxModPoints; ++k)
            {
                const float v = p::lanePoint(modPoints, lane, k).load(std::memory_order_relaxed);
                if (!juce::exactlyEqual(v, points[static_cast<size_t>(k)]))
                {
                    points[static_cast<size_t>(k)] = v;
                    changed = true;
                }
            }
        }
    }
    if (changed)
        engine_.setParams(settings_.engine);
}

void GristoProcessor::applySoundSettings(std::int64_t step, bool playing)
{
    namespace mod = gristo::mod;
    const auto& c = settings_;
    // Stopped: the drawn lanes rest, the macros still act.
    const int firstSource = playing ? 0 : gristo::kNumModLanes;
    auto lane = [&](int target) {
        return c.soundModulated ? engine_.modulationAt(target, step, firstSource) * mod::targetRange(target) : 0.0f;
    };
    auto unit = [](float v) { return juce::jlimit(0.0f, 1.0f, v); };

    auto kit = c.kit;
    kit.tune = juce::jlimit(-12.0f, 12.0f, kit.tune + lane(mod::kitTarget(mod::kKitTune)));
    kit.decay = unit(kit.decay + lane(mod::kitTarget(mod::kKitDecay)));
    kit.tone = unit(kit.tone + lane(mod::kitTarget(mod::kKitTone)));
    kit.drive = unit(kit.drive + lane(mod::kitTarget(mod::kKitDrive)));
    kit.reverbSend = unit(kit.reverbSend + lane(mod::kitTarget(mod::kKitReverb)));
    kit.delaySend = unit(kit.delaySend + lane(mod::kitTarget(mod::kKitDelay)));
    sound_.setKit(kit);

    for (int a = 0; a < gristo::sound::kNumSynths; ++a)
    {
        auto synth = c.synths[a];
        synth.cutoffMod = lane(mod::synthTarget(a, mod::kSynthCutoff));
        synth.resonance = unit(synth.resonance + lane(mod::synthTarget(a, mod::kSynthReso)));
        synth.envAmount = unit(synth.envAmount + lane(mod::synthTarget(a, mod::kSynthEnv)));
        synth.attack = unit(synth.attack + lane(mod::synthTarget(a, mod::kSynthAttack)));
        synth.release = unit(synth.release + lane(mod::synthTarget(a, mod::kSynthRelease)));
        synth.level = unit(synth.level + lane(mod::synthTarget(a, mod::kSynthLevel)));
        synth.reverbSend = unit(synth.reverbSend + lane(mod::synthTarget(a, mod::kSynthReverb)));
        synth.delaySend = unit(synth.delaySend + lane(mod::synthTarget(a, mod::kSynthDelay)));
        sound_.setSynth(a, synth);
    }
    auto fx = c.fx;
    fx.bpm = lastBpm_;
    fx.reverbSize = unit(fx.reverbSize + lane(mod::kFxSizeTarget));
    fx.delayFeedback = juce::jlimit(0.0f, 0.9f, fx.delayFeedback + lane(mod::kFxFeedbackTarget));
    sound_.setFx(fx);
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
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    for (int bus = 1; bus < layouts.outputBuses.size(); ++bus)
        if (!layouts.outputBuses[bus].isDisabled() && layouts.outputBuses[bus] != juce::AudioChannelSet::stereo())
            return false;
    return true;
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
    paramGeneration_.fetch_add(1); // read everything afresh
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
    if (!settings_.freeRun)
    {
        wasFreeRunning_ = false;
        return b;
    }
    if (!wasFreeRunning_)
        freePpq_ = 0.0;
    wasFreeRunning_ = true;

    b.playing = true;
    b.bpm = settings_.freeTempo;
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

    refreshSettings();
    if (samplesChanged_.load(std::memory_order_relaxed))
    {
        // Picked up without waiting: if the message thread is busy swapping, next block.
        const juce::SpinLock::ScopedTryLockType lock(sampleLock_);
        if (lock.isLocked())
        {
            for (int v = 0; v < gristo::kNumVoices; ++v)
                sound_.setDrumSample(v, samples_[static_cast<size_t>(v)].get());
            samplesChanged_.store(false, std::memory_order_relaxed);
        }
    }
    for (int a = 0; a < gristo::kNumArps; ++a)
    {
        const bool latch = settings_.latch[a];
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

    const int* notes = settings_.notes;
    const int* channels = settings_.channels;
    const int* shifts = settings_.shifts;

    const bool kitOn = settings_.kitOn;
    if (kitOn && !soundWasOn_)
        sound_.reset(); // nothing stale from while the sound was off
    soundWasOn_ = kitOn;
    if (block.playing)
        lastBpm_ = block.bpm;
    // Lanes on the sounds follow the 16th grid inside the block: the settings are
    // applied at the block start and again at every step boundary (the sound engine
    // glides levels, filters and sends in between).
    const double beatsPerSample = block.bpm / (60.0 * sampleRate_);
    auto nextStep = static_cast<std::int64_t>(std::floor(block.ppqStart * gristo::kStepsPerBeat));
    if (kitOn)
        applySoundSettings(nextStep, block.playing);
    ++nextStep;
    auto boundarySample = [&](std::int64_t step) {
        return static_cast<int>(std::ceil((static_cast<double>(step) / gristo::kStepsPerBeat - block.ppqStart) / beatsPerSample));
    };
    const bool stepwise = kitOn && block.playing && settings_.soundModulated && beatsPerSample > 0.0;
    const float kitGain = settings_.kitGain;
    auto mainOut = getBusBuffer(buffer, false, 0);
    float* left = mainOut.getWritePointer(0);
    float* right = mainOut.getNumChannels() > 1 ? mainOut.getWritePointer(1) : nullptr;

    // Separate Outputs: every part whose output bus the host has switched on goes
    // there instead of the main mix. Buses in order: Kit, Arp 1-3, Chords, FX.
    gristo::sound::Stems stems;
    bool anyStem = false;
    if (settings_.separateOuts && right != nullptr)
        for (int bus = 1; bus < getBusCount(false); ++bus)
        {
            auto out = getBusBuffer(buffer, false, bus);
            if (out.getNumChannels() < 2)
                continue;
            float* const pair[2] = { out.getWritePointer(0), out.getWritePointer(1) };
            float** target = bus == 1 ? stems.kit : bus == 6 ? stems.fx : stems.synth[bus - 2];
            target[0] = pair[0];
            target[1] = pair[1];
            anyStem = true;
        }

    int rendered = 0;
    auto renderKitTo = [&](int end) {
        if (!kitOn || end <= rendered)
            return;
        if (right != nullptr)
        {
            gristo::sound::Stems at = stems;
            for (float** pair : { at.kit, at.synth[0], at.synth[1], at.synth[2], at.synth[3], at.fx })
                for (int ch = 0; ch < 2; ++ch)
                    if (pair[ch] != nullptr)
                        pair[ch] += rendered;
            sound_.render(left + rendered, right + rendered, end - rendered, kitGain, anyStem ? &at : nullptr);
        }
        else
        {
            // Mono output: both sides are summed into the one channel at half level.
            sound_.render(left + rendered, left + rendered, end - rendered, kitGain * 0.5f);
        }
        rendered = end;
    };

    // Arp / chord part (0..kNumArps, the chords last) -> its synth.
    auto synthOf = [](int part) { return part == gristo::kNumArps ? gristo::sound::kChordSynth : gristo::mod::synthOfArp(part); };
    auto renderWithSteps = [&](int end) {
        while (stepwise && boundarySample(nextStep) < end)
        {
            renderKitTo(std::max(rendered, boundarySample(nextStep)));
            applySoundSettings(nextStep, true);
            ++nextStep;
        }
        renderKitTo(end);
    };
    for (const auto& ev : events_)
    {
        const bool isArp = ev.voice >= gristo::kArpVoiceBase;
        int note = isArp ? ev.note : notes[ev.voice];
        int channel = channels[ev.voice];
        if (ev.noteOn)
        {
            if (isArp)
                sentChannel_[static_cast<size_t>(ev.voice)][static_cast<size_t>(note & 127)] = static_cast<juce::int8>(channel);
            else
                drumSent_[static_cast<size_t>(ev.voice)] = { note, channel };
        }
        else if (isArp)
        {
            const int sent = sentChannel_[static_cast<size_t>(ev.voice)][static_cast<size_t>(note & 127)];
            channel = sent > 0 ? sent : channel;
        }
        else if (drumSent_[static_cast<size_t>(ev.voice)].second > 0)
            std::tie(note, channel) = drumSent_[static_cast<size_t>(ev.voice)];
        renderWithSteps(ev.sampleOffset);
        if (ev.noteOn)
        {
            displayActivity[static_cast<size_t>(ev.voice)].store(ev.velocity, std::memory_order_relaxed);
            const auto velocity = static_cast<juce::uint8>(juce::jlimit(1, 127, juce::roundToInt(ev.velocity * 127.0f)));
            midi.addEvent(juce::MidiMessage::noteOn(channel, note, velocity), ev.sampleOffset);
            if (kitOn)
            {
                if (isArp)
                {
                    // The kit plays the unshifted pitch, as the synth behind a Pitch device would.
                    const int part = ev.voice - gristo::kArpVoiceBase;
                    sound_.noteOn(synthOf(part), note, static_cast<float>(note - shifts[part]), ev.velocity);
                }
                else
                    sound_.triggerDrum(ev.voice, ev.velocity);
            }
        }
        else
        {
            midi.addEvent(juce::MidiMessage::noteOff(channel, note), ev.sampleOffset);
            if (isArp)
                sound_.noteOff(synthOf(ev.voice - gristo::kArpVoiceBase), note);
        }
    }
    renderWithSteps(numSamples);

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
        if (m.isController())
        {
            const int cc = m.getControllerNumber();
            const int learning = learnMacro.load(std::memory_order_relaxed);
            if (learning >= 0 && learning < gristo::kNumMacros)
            {
                macroCc[static_cast<size_t>(learning)].store(cc);
                learnMacro.store(-1);
            }
            for (int k = 0; k < gristo::kNumMacros; ++k)
                if (macroCc[static_cast<size_t>(k)].load(std::memory_order_relaxed) == cc)
                    macroCcValue[static_cast<size_t>(k)].store(static_cast<float>(m.getControllerValue()) / 127.0f);
            continue;
        }
        if (m.getChannel() == 16 && (m.isNoteOn() || m.isNoteOff()))
        {
            // Channel 16, C1..G1: scene recall (not notes for the arps).
            if (m.isNoteOn() && m.getNoteNumber() >= 36 && m.getNoteNumber() < 36 + kNumScenes)
                pendingScene.store(m.getNoteNumber() - 36);
            continue;
        }
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
        {
            const float v = values[k].getFloatValue();
            p::lanePoint(modPoints, lane, k).store(std::isfinite(v) ? juce::jlimit(0.0f, 1.0f, v) : 0.0f);
        }
    }
}

void GristoProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto tree = state.copyState();
    // Drawn lanes, UI size and the preset name are not parameters: store them alongside.
    auto lanes = tree.getOrCreateChildWithName("MODLANES", nullptr);
    writeLanes(lanes);
    tree.setProperty("stateVersion", kStateVersion, nullptr);
    tree.setProperty("uiScale", uiScale.load(), nullptr);
    tree.setProperty("showKeys", showKeys.load(), nullptr);
    presets.writeTo(tree);
    tree.removeChild(tree.getChildWithName("SCENES"), nullptr);
    writeScenes(tree);
    writeSamples(tree);
    juce::StringArray ccs;
    for (const auto& cc : macroCc)
        ccs.add(juce::String(cc.load()));
    tree.setProperty("macroCc", ccs.joinIntoString(","), nullptr);
    tree.setProperty("diceLocks", diceLocks.load(), nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary(*xml, destData);
}

void GristoProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(state.state.getType()) && xml->getIntAttribute("stateVersion", 1) == kStateVersion)
        {
            auto tree = juce::ValueTree::fromXml(*xml);
            // A damaged or hand-edited state must not feed NaN into the engine.
            for (auto child : tree)
                if (child.hasType("PARAM") && !std::isfinite(static_cast<float>(child["value"])))
                {
                    if (auto* param = state.getParameter(child["id"].toString()))
                        child.setProperty("value", param->convertFrom0to1(param->getDefaultValue()), nullptr);
                    else
                        child.removeProperty("value", nullptr);
                }
            const auto lanes = tree.getChildWithName("MODLANES");
            if (lanes.isValid())
                readLanes(lanes);
            if (tree.hasProperty("uiScale"))
                uiScale.store(static_cast<float>(tree.getProperty("uiScale")));
            if (tree.hasProperty("showKeys"))
                showKeys.store(static_cast<bool>(tree.getProperty("showKeys")));
            state.replaceState(tree);
            paramGeneration_.fetch_add(1);
            presets.readFrom(tree);
            readScenes(tree);
            readSamples(tree);
            const auto ccs = juce::StringArray::fromTokens(tree.getProperty("macroCc", "").toString(), ",", {});
            for (int k = 0; k < gristo::kNumMacros; ++k)
                macroCc[static_cast<size_t>(k)].store(k < ccs.size() ? juce::jlimit(-1, 127, ccs[k].getIntValue()) : -1);
            diceLocks.store(static_cast<int>(tree.getProperty("diceLocks", 0)));
            lastSnapshot_ = presets.captureSnapshot(); // opening a project isn't an undo step
            undo_.clear();
            redo_.clear();
        }
}

std::vector<GristoProcessor::NoteSpan> GristoProcessor::renderSpans(double startBeats, double endBeats) const
{
    constexpr int kBlock = 512;
    constexpr double kRate = 48000.0;
    gristo::EngineParams params = p::readEngineParams(state, modPoints);
    params.partMute.fill(false);
    gristo::Engine engine;
    engine.setParams(params);
    for (int a = 0; a < gristo::kNumArps; ++a)
    {
        int notes[gristo::kMaxHeldNotes] {};
        const int count = juce::jlimit(0, static_cast<int>(gristo::kMaxHeldNotes), displayArpInputCount[static_cast<size_t>(a)].load());
        for (int i = 0; i < count; ++i)
            notes[i] = displayArpInput[static_cast<size_t>(a)][static_cast<size_t>(i)].load();
        engine.setArpInput(a, notes, count);
    }
    const double tempo = juce::jlimit(20.0, 400.0, displayBpm.load());
    const double beatsPerBlock = kBlock * tempo / (60.0 * kRate);
    std::vector<NoteSpan> spans;
    std::vector<gristo::OutputEvent> events;
    auto close = [&spans](int part, int pitch, double beats) {
        for (auto it = spans.rbegin(); it != spans.rend(); ++it)
            if (it->part == part && it->pitch == pitch && it->end < 0.0)
            {
                it->end = beats;
                return;
            }
    };
    for (double ppq = startBeats; ppq < endBeats; ppq += beatsPerBlock)
    {
        events.clear();
        engine.process({ true, ppq, tempo, kRate, kBlock }, events);
        for (const auto& ev : events)
        {
            const double beats = ppq + ev.sampleOffset * tempo / (60.0 * kRate);
            if (!ev.noteOn)
                close(ev.voice, ev.note, beats);
            else if (beats < endBeats)
                spans.push_back({ ev.voice, beats, -1.0, ev.velocity, ev.note });
        }
    }
    for (auto& span : spans)
        if (span.end < 0.0)
            span.end = endBeats;
    return spans;
}

juce::MidiFile GristoProcessor::exportMidi(int bars, int part) const
{
    constexpr int kTicksPerBeat = 960, kBlock = 512;
    constexpr double kRate = 48000.0;
    auto value = [this](const juce::String& id) { return state.getRawParameterValue(id)->load(); };

    gristo::EngineParams params = p::readEngineParams(state, modPoints);
    const auto wasMuted = params.partMute;
    params.partMute.fill(false); // the engine plays everything; muted parts are left out below
    gristo::Engine engine;
    engine.setParams(params);
    for (int a = 0; a < gristo::kNumArps; ++a)
    {
        int notes[gristo::kMaxHeldNotes] {};
        const int count = juce::jlimit(0, static_cast<int>(gristo::kMaxHeldNotes), displayArpInputCount[static_cast<size_t>(a)].load());
        for (int i = 0; i < count; ++i)
            notes[i] = displayArpInput[static_cast<size_t>(a)][static_cast<size_t>(i)].load();
        engine.setArpInput(a, notes, count);
    }
    const double bpm = displayPlaying.load() ? displayBpm.load() : (value(p::freeRun) > 0.5f ? static_cast<double>(value(p::freeTempo)) : displayBpm.load());
    const double tempo = juce::jlimit(20.0, 400.0, bpm);
    const double endBeats = std::max(1, bars) * 4.0, beatsPerBlock = kBlock * tempo / (60.0 * kRate);

    // What each part is exported as.
    auto included = [&](int voice) {
        if (part == kAllParts)
            return !wasMuted[static_cast<size_t>(voice)];
        if (part == kAllDrums)
            return voice < gristo::kNumVoices;
        return voice == part;
    };
    auto trackOf = [&](int voice) { return part == kAllParts ? (voice < gristo::kNumVoices ? 0 : voice - gristo::kNumVoices + 1) : 0; };
    auto pitchOf = [&](const gristo::OutputEvent& ev) {
        if (ev.voice >= gristo::kNumVoices)
            return ev.voice == gristo::kChordVoice ? ev.note - params.chords.shift
                                                    : ev.note - params.arps[static_cast<size_t>(ev.voice - gristo::kArpVoiceBase)].shift;
        const int note = static_cast<int>(value(p::voiceId(p::note, ev.voice)));
        return note < 24 ? note + 36 : note; // out of the split range, back to GM
    };

    const int numTracks = part == kAllParts ? gristo::kNumArps + 2 : 1;
    std::vector<juce::MidiMessageSequence> tracks(static_cast<size_t>(numTracks));
    std::vector<gristo::OutputEvent> events;
    std::vector<std::array<int, 128>> open(static_cast<size_t>(numTracks), std::array<int, 128> {});
    auto add = [&](const gristo::OutputEvent& ev, double beats) {
        if (!included(ev.voice))
            return;
        const int pitch = juce::jlimit(0, 127, pitchOf(ev));
        const double tick = std::round(juce::jlimit(0.0, endBeats, beats) * kTicksPerBeat);
        const auto velocity = static_cast<juce::uint8>(juce::jlimit(1, 127, juce::roundToInt(ev.velocity * 127.0f)));
        // Only close notes this file opened (one that would start after the end is left out).
        int& sounding = open[static_cast<size_t>(trackOf(ev.voice))][static_cast<size_t>(pitch)];
        if (!ev.noteOn && sounding == 0)
            return;
        sounding += ev.noteOn ? 1 : -1;
        tracks[static_cast<size_t>(trackOf(ev.voice))].addEvent(
            ev.noteOn ? juce::MidiMessage::noteOn(1, pitch, velocity) : juce::MidiMessage::noteOff(1, pitch), tick);
    };
    double ppq = 0.0;
    while (ppq < endBeats)
    {
        events.clear();
        engine.process({ true, ppq, tempo, kRate, kBlock }, events);
        for (const auto& ev : events)
        {
            const double beats = ppq + ev.sampleOffset * tempo / (60.0 * kRate);
            if (beats < endBeats || !ev.noteOn) // notes starting after the end belong to the next loop
                add(ev, beats);
        }
        ppq += beatsPerBlock;
    }
    events.clear();
    engine.process({ false, ppq, tempo, kRate, kBlock }, events); // stopping ends what still sounds
    for (const auto& ev : events)
        if (!ev.noteOn)
            add(ev, endBeats);

    juce::MidiFile file;
    file.setTicksPerQuarterNote(kTicksPerBeat);
    const char* const names[] = { "Drums", "Arp 1", "Arp 2", "Arp 3", "Chords" };
    for (int t = 0; t < numTracks; ++t)
    {
        auto& track = tracks[static_cast<size_t>(t)];
        if (part == kAllParts && track.getNumEvents() == 0)
            continue; // parts that are off
        const juce::String name = part == kAllParts ? juce::String(names[t]) : part == kAllDrums ? juce::String("Drums") : juce::String(gristo::partName(part));
        track.addEvent(juce::MidiMessage::textMetaEvent(3, "Gristo " + name), 0.0);
        if (file.getNumTracks() == 0)
        {
            track.addEvent(juce::MidiMessage::tempoMetaEvent(juce::roundToInt(60000000.0 / tempo)), 0.0);
            track.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0.0);
        }
        track.addEvent(juce::MidiMessage::endOfTrack(), endBeats * kTicksPerBeat);
        track.sort();
        track.updateMatchedPairs();
        file.addTrack(track);
    }
    return file;
}

juce::File GristoProcessor::exportMidiToTempFile(int bars, int part) const
{
    const auto midi = exportMidi(bars, part);
    if (midi.getNumTracks() == 0)
        return {};
    const juce::String name = part == kAllParts ? juce::String("All") : part == kAllDrums ? juce::String("Drums") : juce::String(gristo::partName(part));
    auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("GristoMidi");
    folder.createDirectory();
    const auto file = folder.getChildFile("Gristo " + name + " " + juce::String(bars) + " bars.mid");
    file.deleteFile();
    juce::FileOutputStream out(file);
    if (!out.openedOk() || !midi.writeTo(out, part == kAllParts ? 1 : 0))
        return {};
    out.flush();
    return file;
}

// ---------------------------------------------------------------------------------
// Drum samples (message thread)

juce::File GristoProcessor::samplesFolder()
{
    auto folder = PresetManager::userFolder().getSiblingFile("Samples");
    folder.createDirectory();
    return folder;
}

bool GristoProcessor::setDrumSampleFile(int voice, const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        return false;
    // One-shots: anything longer than 20 s is cut there.
    const auto length = static_cast<int>(std::min<juce::int64>(reader->lengthInSamples, static_cast<juce::int64>(reader->sampleRate * 20.0)));
    juce::AudioBuffer<float> buffer(2, length);
    reader->read(&buffer, 0, length, 0, true, true);
    auto data = std::make_shared<gristo::sound::SampleData>();
    data->sampleRate = reader->sampleRate;
    const float* l = buffer.getReadPointer(0);
    const float* r = buffer.getReadPointer(reader->numChannels > 1 ? 1 : 0);
    data->left.resize(static_cast<size_t>(length) + 1, 0.0f); // one extra sample, so it ends at zero
    data->right.resize(static_cast<size_t>(length) + 1, 0.0f);
    for (int i = 0; i < length; ++i)
    {
        data->left[static_cast<size_t>(i)] = std::isfinite(l[i]) ? juce::jlimit(-4.0f, 4.0f, l[i]) : 0.0f;
        data->right[static_cast<size_t>(i)] = std::isfinite(r[i]) ? juce::jlimit(-4.0f, 4.0f, r[i]) : 0.0f;
    }
    {
        const juce::SpinLock::ScopedLockType lock(sampleLock_);
        if (samples_[static_cast<size_t>(voice)] != nullptr)
            retiredSamples_.push_back(samples_[static_cast<size_t>(voice)]);
        if (retiredSamples_.size() > 12)
            retiredSamples_.erase(retiredSamples_.begin());
        samples_[static_cast<size_t>(voice)] = std::move(data);
        sampleNames_[static_cast<size_t>(voice)] = file.getFileName();
    }
    samplesChanged_.store(true);
    return true;
}

bool GristoProcessor::loadDrumSample(int voice, const juce::URL& source)
{
    if (voice < 0 || voice >= gristo::kNumVoices)
        return false;
    // Keep a copy in the samples folder, so the patch finds it again.
    juce::File file = source.isLocalFile() ? source.getLocalFile() : juce::File();
    if (!file.existsAsFile() || !file.isAChildOf(samplesFolder()))
    {
        const auto name = juce::File::createLegalFileName(source.isLocalFile() ? file.getFileName() : source.getFileName());
        const auto target = samplesFolder().getChildFile(name.isNotEmpty() ? name : juce::String("sample.wav"));
        if (file.existsAsFile())
        {
            if (file.getSize() > 64 * 1024 * 1024 || (!target.existsAsFile() && !file.copyFileTo(target)))
                return false;
        }
        else
        {
            std::unique_ptr<juce::InputStream> in(source.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)));
            if (in == nullptr || in->getTotalLength() > 64 * 1024 * 1024)
                return false;
            juce::MemoryBlock data;
            in->readIntoMemoryBlock(data, 64 * 1024 * 1024 + 1);
            if (data.getSize() == 0 || data.getSize() > 64 * 1024 * 1024 || !target.replaceWithData(data.getData(), data.getSize()))
                return false;
        }
        file = target;
    }
    return setDrumSampleFile(voice, file);
}

void GristoProcessor::clearDrumSample(int voice)
{
    if (voice < 0 || voice >= gristo::kNumVoices)
        return;
    {
        const juce::SpinLock::ScopedLockType lock(sampleLock_);
        if (samples_[static_cast<size_t>(voice)] != nullptr)
            retiredSamples_.push_back(samples_[static_cast<size_t>(voice)]);
        samples_[static_cast<size_t>(voice)].reset();
        sampleNames_[static_cast<size_t>(voice)].clear();
    }
    samplesChanged_.store(true);
}

juce::String GristoProcessor::drumSampleName(int voice) const
{
    const juce::SpinLock::ScopedLockType lock(sampleLock_);
    return voice >= 0 && voice < gristo::kNumVoices ? sampleNames_[static_cast<size_t>(voice)] : juce::String();
}

void GristoProcessor::writeSamples(juce::ValueTree& parent) const
{
    parent.removeChild(parent.getChildWithName("SAMPLES"), nullptr);
    juce::ValueTree samples("SAMPLES");
    for (int v = 0; v < gristo::kNumVoices; ++v)
        samples.setProperty("voice" + juce::String(v), drumSampleName(v), nullptr);
    parent.appendChild(samples, nullptr);
}

void GristoProcessor::readSamples(const juce::ValueTree& parent)
{
    const auto samples = parent.getChildWithName("SAMPLES");
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        // Names only, looked up in the samples folder: a missing file means the kit's own sound.
        const auto name = juce::File::createLegalFileName(samples.getProperty("voice" + juce::String(v), "").toString());
        if (name == drumSampleName(v))
            continue;
        const auto file = samplesFolder().getChildFile(name);
        if (name.isEmpty() || !file.existsAsFile() || !setDrumSampleFile(v, file))
            clearDrumSample(v);
    }
}

// ---------------------------------------------------------------------------------
// Undo, scenes, dice (message thread)

void GristoProcessor::setValue(const juce::String& id, float value)
{
    if (auto* param = state.getParameter(id))
        param->setValueNotifyingHost(param->convertTo0to1(value));
}

void GristoProcessor::timerCallback()
{
    // MIDI-learned macros.
    for (int k = 0; k < gristo::kNumMacros; ++k)
    {
        const float v = macroCcValue[static_cast<size_t>(k)].exchange(-1.0f);
        if (v >= 0.0f)
            setValue(p::laneId(p::macroValue, k), v);
    }

    // Scenes: recall at once when stopped, else just before the next bar line.
    const bool playing = displayPlaying.load();
    const double ppq = displayPpq.load(), bpm = std::max(20.0, displayBpm.load());
    const auto bar = static_cast<std::int64_t>(std::floor(ppq / 4.0));
    if (chainScenes.load() && playing && pendingScene.load() < 0)
    {
        const int span = std::max(1, chainBars.load());
        if ((bar + 1) % span == 0 && chainQueuedBar_ != bar)
        {
            chainQueuedBar_ = bar;
            const int current = currentScene.load();
            for (int step = 1; step <= kNumScenes; ++step)
                if (const int next = ((current < 0 ? -1 : current) + step) % kNumScenes; sceneUsed(next))
                {
                    pendingScene.store(next);
                    break;
                }
        }
    }
    const int pending = pendingScene.load();
    if (pending != lastPending_)
    {
        lastPending_ = pending;
        sceneRequestBar_ = bar;
    }
    if (pending >= 0)
    {
        const double secondsToBar = (4.0 - std::fmod(ppq, 4.0)) * 60.0 / bpm;
        if (!sceneUsed(pending))
            pendingScene.compare_exchange_strong(lastPending_, -1);
        else if (!playing || secondsToBar <= 0.025 || bar != sceneRequestBar_)
        {
            presets.applySnapshot(scenes_[static_cast<size_t>(pending)]);
            currentScene.store(pending);
            int expected = pending;
            pendingScene.compare_exchange_strong(expected, -1);
        }
    }

    // Undo history: a few times a second, once the mouse is up, a changed state
    // becomes a step.
    if (++historyTick_ >= 25)
    {
        historyTick_ = 0;
        if (!juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        {
            auto now = presets.captureSnapshot();
            if (now != lastSnapshot_)
            {
                undo_.push_back(std::move(lastSnapshot_));
                if (undo_.size() > 64)
                    undo_.erase(undo_.begin());
                redo_.clear();
                lastSnapshot_ = std::move(now);
            }
        }
    }
}

void GristoProcessor::undo()
{
    if (undo_.empty())
        return;
    redo_.push_back(presets.captureSnapshot());
    lastSnapshot_ = std::move(undo_.back());
    undo_.pop_back();
    presets.applySnapshot(lastSnapshot_);
    lastSnapshot_ = presets.captureSnapshot(); // exactly what the parameters hold now
}

void GristoProcessor::redo()
{
    if (redo_.empty())
        return;
    undo_.push_back(presets.captureSnapshot());
    lastSnapshot_ = std::move(redo_.back());
    redo_.pop_back();
    presets.applySnapshot(lastSnapshot_);
    lastSnapshot_ = presets.captureSnapshot();
}

void GristoProcessor::storeScene(int scene)
{
    if (scene < 0 || scene >= kNumScenes)
        return;
    scenes_[static_cast<size_t>(scene)] = presets.captureSnapshot();
    currentScene.store(scene);
}

void GristoProcessor::clearScene(int scene)
{
    if (scene < 0 || scene >= kNumScenes)
        return;
    scenes_[static_cast<size_t>(scene)] = {};
    int expected = scene;
    currentScene.compare_exchange_strong(expected, -1);
}

void GristoProcessor::writeScenes(juce::ValueTree& parent) const
{
    juce::ValueTree scenes("SCENES");
    auto encode = [](const std::vector<float>& v) { return juce::MemoryBlock(v.data(), v.size() * sizeof(float)).toBase64Encoding(); };
    for (int i = 0; i < kNumScenes; ++i)
        if (sceneUsed(i))
        {
            juce::ValueTree scene("SCENE");
            scene.setProperty("index", i, nullptr);
            scene.setProperty("values", encode(scenes_[static_cast<size_t>(i)].values), nullptr);
            scene.setProperty("lanes", encode(scenes_[static_cast<size_t>(i)].lanePoints), nullptr);
            scenes.appendChild(scene, nullptr);
        }
    scenes.setProperty("chain", chainScenes.load(), nullptr);
    scenes.setProperty("chainBars", chainBars.load(), nullptr);
    parent.appendChild(scenes, nullptr);
}

void GristoProcessor::readScenes(const juce::ValueTree& parent)
{
    for (auto& scene : scenes_)
        scene = {};
    currentScene.store(-1);
    pendingScene.store(-1);
    const auto scenes = parent.getChildWithName("SCENES");
    chainScenes.store(static_cast<bool>(scenes.getProperty("chain", false)));
    chainBars.store(juce::jlimit(1, 16, static_cast<int>(scenes.getProperty("chainBars", 4))));
    const size_t expected = presets.captureSnapshot().values.size();
    auto decode = [](const juce::String& text) {
        juce::MemoryBlock block;
        block.fromBase64Encoding(text);
        std::vector<float> v(block.getSize() / sizeof(float));
        if (!v.empty())
            std::memcpy(v.data(), block.getData(), v.size() * sizeof(float));
        for (float& x : v)
            x = std::isfinite(x) ? juce::jlimit(0.0f, 1.0f, x) : 0.0f;
        return v;
    };
    for (const auto& scene : scenes)
    {
        const int i = static_cast<int>(scene.getProperty("index", -1));
        Snapshot snapshot { decode(scene["values"].toString()), decode(scene["lanes"].toString()) };
        // Only scenes saved with this exact set of parameters fit.
        if (i >= 0 && i < kNumScenes && snapshot.values.size() == expected
            && snapshot.lanePoints.size() == static_cast<size_t>(gristo::kNumModLanes * gristo::kMaxModPoints))
            scenes_[static_cast<size_t>(i)] = std::move(snapshot);
    }
}

void GristoProcessor::diceAll()
{
    const int locks = diceLocks.load();
    for (int section = 0; section < NumSections; ++section)
        if ((locks & (1 << section)) == 0)
            dice(section);
}

void GristoProcessor::dice(int section)
{
    auto& r = juce::Random::getSystemRandom();
    auto range = [&r](float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); };
    auto pick = [&r](std::initializer_list<float> options) { return *(options.begin() + r.nextInt(static_cast<int>(options.size()))); };
    auto choices = [this](const juce::String& id) {
        const auto* param = state.getParameter(id);
        return param != nullptr ? param->getNumSteps() : 1;
    };
    if (section == DicePattern)
    {
        setValue(p::mapX, r.nextFloat());
        setValue(p::mapY, r.nextFloat());
        setValue(p::chaos, r.nextFloat() < 0.7f ? 0.0f : range(0.05f, 0.35f));
        setValue(p::variation, static_cast<float>(r.nextInt(100)));
        const float centres[gristo::kNumVoices] = { 0.58f, 0.52f, 0.6f, 0.35f, 0.4f, 0.35f };
        for (int v = 0; v < gristo::kNumVoices; ++v)
            setValue(p::voiceId(p::fill, v), juce::jlimit(0.0f, 1.0f, centres[v] + range(-0.2f, 0.2f)));
    }
    else if (section == DiceGroove)
    {
        setValue(p::timing, static_cast<float>(r.nextInt(choices(p::timing))));
        setValue(p::shuffle, r.nextFloat() < 0.3f ? 0.0f : range(0.5f, 4.5f));
        setValue(p::feeling, r.nextFloat() < 0.5f ? 0.0f : static_cast<float>(r.nextInt(choices(p::feeling))));
        setValue(p::feelMs, range(5.0f, 20.0f));
        setValue(p::humanize, range(0.0f, 0.5f));
    }
    else if (section == DiceKit)
    {
        setValue(p::kitType, static_cast<float>(r.nextInt(choices(p::kitType))));
        setValue(p::kitTune, static_cast<float>(juce::roundToInt(range(-3.0f, 3.0f))));
        setValue(p::kitDecay, range(0.35f, 0.65f));
        setValue(p::kitTone, range(0.35f, 0.65f));
        setValue(p::kitDrive, r.nextFloat() < 0.5f ? 0.0f : range(0.05f, 0.4f));
    }
    else if (section >= DiceArp1 && section <= DiceArp3)
    {
        const int a = section - DiceArp1;
        setValue(p::laneId(p::arpMode, a), 1.0f); // Generate
        setValue(p::laneId(p::arpSeed, a), static_cast<float>(r.nextInt({ 1, 10000 })));
        setValue(p::laneId(p::arpLength, a), pick({ 4.0f, 6.0f, 8.0f, 8.0f, 12.0f, 16.0f }));
        setValue(p::laneId(p::arpDensity, a), range(0.5f, 0.95f));
        setValue(p::laneId(p::arpAccent, a), range(0.1f, 0.4f));
        setValue(p::laneId(p::arpSlide, a), r.nextFloat() < 0.4f ? 0.0f : range(0.05f, 0.35f));
        setValue(p::laneId(p::arpOctaves, a), pick({ 1.0f, 1.0f, 2.0f }));
        setValue(p::laneId(p::arpGate, a), pick({ 1.0f, 2.0f, 2.0f, 3.0f, 4.0f }));
        const int first = r.nextInt(gristo::kNumVoices), second = r.nextFloat() < 0.4f ? r.nextInt(gristo::kNumVoices) : first;
        for (int v = 0; v < gristo::kNumVoices; ++v)
            setValue(p::slotId(p::arpTrigger, a, v), v == first || v == second ? 1.0f : 0.0f);
    }
    else if (section == DiceChords)
    {
        const auto seed = static_cast<std::uint32_t>(r.nextInt({ 1, 10000 }));
        const auto scale = static_cast<gristo::ChordScale>(static_cast<int>(state.getRawParameterValue(p::chordScale)->load()));
        const auto slots = gristo::Chords::generate(seed, scale);
        for (int i = 0; i < gristo::kMaxChordSlots; ++i)
        {
            const auto& slot = slots[static_cast<size_t>(i)];
            setValue(p::voiceId(p::chordSlotUsed, i), slot.used ? 1.0f : 0.0f);
            const gristo::ChordSlot s = slot.used ? slot : gristo::ChordSlot {};
            setValue(p::voiceId(p::chordDegree, i), static_cast<float>(s.degree));
            setValue(p::voiceId(p::chordBorrowed, i), s.borrowed ? 1.0f : 0.0f);
            setValue(p::voiceId(p::chordType, i), static_cast<float>(s.type));
            setValue(p::voiceId(p::chordInversion, i), static_cast<float>(s.inversion));
            setValue(p::voiceId(p::chordQuality, i), static_cast<float>(s.quality));
            setValue(p::voiceId(p::chordAlter, i), static_cast<float>(s.alter));
            setValue(p::voiceId(p::chordLength, i), s.lengthSteps <= 8 ? 0.0f : s.lengthSteps >= 32 ? 2.0f : 1.0f);
        }
        setValue(p::chordSeed, static_cast<float>(seed));
    }
    else if (section >= DiceSound1 && section <= DiceSound4)
    {
        const int synth = section - DiceSound1;
        // Arp 1 stays a bass; the others get any sound of the same family or a neighbour.
        const int count = static_cast<int>(gristo::sound::SynthSound::Count);
        int sound = r.nextInt(count);
        for (int tries = 0; tries < 40; ++tries, sound = r.nextInt(count))
        {
            const juce::String category = gristo::sound::synthCategoryName(static_cast<gristo::sound::SynthSound>(sound));
            if (synth == 0 ? category == "Bass" : synth == gristo::sound::kChordSynth ? (category == "Pads" || category == "Keys") : category != "Bass")
                break;
        }
        setValue(p::laneId(p::synthSound, synth), static_cast<float>(sound));
        setValue(p::laneId(p::synthCutoff, synth), range(0.35f, 0.75f));
        setValue(p::laneId(p::synthResonance, synth), range(0.3f, 0.7f));
        setValue(p::laneId(p::synthEnv, synth), range(0.3f, 0.7f));
        setValue(p::laneId(p::synthAttack, synth), range(0.4f, 0.6f));
        setValue(p::laneId(p::synthRelease, synth), range(0.4f, 0.65f));
        setValue(p::laneId(p::synthReverb, synth), synth == 0 ? range(0.0f, 0.15f) : range(0.1f, 0.45f));
        setValue(p::laneId(p::synthDelay, synth), r.nextFloat() < 0.5f ? 0.0f : range(0.1f, 0.35f));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GristoProcessor();
}
