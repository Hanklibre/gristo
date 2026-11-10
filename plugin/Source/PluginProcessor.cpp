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
}

GristoProcessor::~GristoProcessor()
{
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
    const bool modulate = playing && c.soundModulated;
    auto lane = [&](int target) { return modulate ? engine_.modulationAt(target, step) * mod::targetRange(target) : 0.0f; };
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
        }
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GristoProcessor();
}
