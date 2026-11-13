// Headless tests of the plugin: parameters, presets, state and the audio / MIDI it
// produces. Runs without a display.

#include "TestFramework.h"

#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace p = gristo::params;

namespace {

void setParam(GristoProcessor& proc, const juce::String& id, float value)
{
    auto* param = proc.state.getParameter(id);
    if (param == nullptr)
    {
        CHECK_MSG(false, "no parameter " << id.toStdString());
        return;
    }
    param->setValueNotifyingHost(param->convertTo0to1(value));
}

float getParam(GristoProcessor& proc, const juce::String& id) { return proc.state.getRawParameterValue(id)->load(); }

struct Run
{
    int nonFinite = 0;
    float peak = 0.0f;
    std::array<int, 17> noteOns {}; // by MIDI channel
    int unpaired = 0;
    std::map<std::pair<int, int>, int> sounding; // carried into the next run
};

// Plays `blocks` blocks from the internal clock and collects what comes out.
Run play(GristoProcessor& proc, int blocks, int blockSize = 256, const Run* previous = nullptr)
{
    Run run;
    juce::AudioBuffer<float> buffer(2, blockSize);
    auto& sounding = run.sounding;
    if (previous != nullptr)
        sounding = previous->sounding;
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        proc.processBlock(buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
            {
                const float s = buffer.getSample(ch, i);
                run.nonFinite += std::isfinite(s) ? 0 : 1;
                run.peak = std::max(run.peak, std::abs(s));
            }
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            int& n = sounding[{ m.getChannel(), m.getNoteNumber() }];
            if (m.isNoteOn())
            {
                ++run.noteOns[static_cast<size_t>(m.getChannel())];
                run.unpaired += n > 0 ? 1 : 0;
                n = 1;
            }
            else if (m.isNoteOff())
            {
                run.unpaired += n == 0 ? 1 : 0;
                n = 0;
            }
        }
    }
    return run;
}

std::unique_ptr<GristoProcessor> makeProcessor()
{
    auto proc = std::make_unique<GristoProcessor>();
    proc->prepareToPlay(48000.0, 256);
    setParam(*proc, p::freeRun, 1.0f);
    return proc;
}

void randomise(GristoProcessor& proc, juce::Random& random)
{
    for (auto* param : proc.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param)) // a value the parameter can really hold
            ranged->setValueNotifyingHost(ranged->convertTo0to1(ranged->convertFrom0to1(random.nextFloat())));
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
        for (int k = 0; k < gristo::kMaxModPoints; ++k)
            p::lanePoint(proc.modPoints, lane, k).store(random.nextFloat());
}

} // namespace

TEST("plugin: plays out of the box, finite audio and paired notes")
{
    auto proc = makeProcessor();
    Run run = play(*proc, 800);
    CHECK(run.nonFinite == 0);
    CHECK(run.peak > 0.05f && run.peak <= 1.0f);
    CHECK(run.noteOns[10] > 8); // drums on channel 10
    CHECK(run.unpaired == 0);
}

TEST("plugin: every factory preset loads cleanly and plays")
{
    const auto& list = PresetManager::factoryPresets();
    CHECK(list.size() >= 22);
    for (int i = 0; i < static_cast<int>(list.size()); ++i)
    {
        auto proc = makeProcessor();
        for (const auto& [id, value] : list[static_cast<size_t>(i)].values)
        {
            auto* param = proc->state.getParameter(id);
            CHECK_MSG(param != nullptr, list[static_cast<size_t>(i)].name.toStdString() << ": no parameter " << id.toStdString());
            if (param != nullptr)
            {
                const auto range = proc->state.getParameterRange(id);
                CHECK_MSG(value >= range.start && value <= range.end, id.toStdString() << " out of range");
                CHECK_MSG(id == p::freeTempo || !PresetManager::isRoutingParameter(id), id.toStdString() << " is a routing parameter");
            }
        }
        proc->presets.loadFactory(i);
        CHECK(!proc->presets.isDirty());
        setParam(*proc, p::freeRun, 1.0f);
        Run run = play(*proc, 400);
        CHECK_MSG(run.nonFinite == 0 && run.peak > 0.02f, list[static_cast<size_t>(i)].name.toStdString() << " peak " << run.peak);
        CHECK_MSG(run.unpaired == 0, list[static_cast<size_t>(i)].name.toStdString() << ": " << run.unpaired << " unpaired notes");
    }
}

TEST("plugin: a snapshot restores every musical setting and leaves routing alone")
{
    auto proc = makeProcessor();
    juce::Random random(42);
    randomise(*proc, random);
    const Snapshot before = proc->presets.captureSnapshot();
    const float channel = getParam(*proc, p::laneId(p::arpChannel, 1));
    proc->presets.loadInit();
    CHECK(proc->presets.captureSnapshot() != before);
    CHECK(std::abs(getParam(*proc, p::laneId(p::arpChannel, 1)) - channel) < 0.01f); // presets don't touch routing
    proc->presets.applySnapshot(before);
    const Snapshot after = proc->presets.captureSnapshot();
    CHECK(after.values.size() == before.values.size());
    int differ = 0;
    for (size_t i = 0; i < before.values.size(); ++i)
        differ += std::abs(before.values[i] - after.values[i]) > 1.0e-6f ? 1 : 0;
    CHECK_MSG(differ == 0, differ << " values differ");
    CHECK(after.lanePoints == before.lanePoints);
}

TEST("plugin: preset files and host state round-trip; older formats are refused")
{
    auto proc = makeProcessor();
    juce::Random random(7);
    randomise(*proc, random);
    const Snapshot saved = proc->presets.captureSnapshot();
    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("gristo_test_preset.gristo");
    CHECK(proc->presets.saveFile(file, "Round trip"));
    proc->presets.loadInit();
    CHECK(proc->presets.loadFile(file));
    {
        const Snapshot loaded = proc->presets.captureSnapshot();
        int differ = 0;
        for (size_t i = 0; i < saved.values.size(); ++i)
            differ += std::abs(saved.values[i] - loaded.values[i]) > 1.0e-4f ? 1 : 0;
        CHECK_MSG(differ == 0, differ << " values differ after a preset round trip");
    }
    file.replaceWithText("<GristoPreset version=\"1\" name=\"Old\"><PARAM id=\"mapX\" value=\"0.1\"/></GristoPreset>");
    CHECK(!proc->presets.loadFile(file));
    file.deleteFile();

    juce::MemoryBlock state;
    proc->getStateInformation(state);
    GristoProcessor other;
    other.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    int differ = 0;
    for (int i = 0; i < proc->getParameters().size(); ++i)
        differ += std::abs(proc->getParameters()[i]->getValue() - other.getParameters()[i]->getValue()) > 1.0e-4f ? 1 : 0;
    CHECK_MSG(differ == 0, differ << " parameters differ after a state round trip");
}

TEST("plugin: mute and solo silence parts; changes are picked up while playing")
{
    auto proc = makeProcessor();
    setParam(*proc, p::laneId(p::arpOn, 0), 1.0f);
    setParam(*proc, p::laneId(p::arpMode, 0), 1.0f); // Generate
    Run open = play(*proc, 800);
    CHECK(open.noteOns[10] > 8 && open.noteOns[1] >= 3);

    for (int v = 0; v < gristo::kNumVoices; ++v)
        setParam(*proc, p::voiceId(p::partMute, v), 1.0f);
    const Run gap = play(*proc, 20, 256, &open); // notes already sounding end
    Run muted = play(*proc, 800, 256, &gap);
    CHECK(muted.noteOns[10] == 0);
    CHECK(muted.noteOns[1] >= 3); // the arp is still clocked by the muted kick
    CHECK(muted.unpaired == 0);

    for (int v = 0; v < gristo::kNumVoices; ++v)
        setParam(*proc, p::voiceId(p::partMute, v), 0.0f);
    setParam(*proc, p::voiceId(p::partSolo, gristo::Snare), 1.0f);
    const Run gap2 = play(*proc, 20, 256, &muted);
    Run solo = play(*proc, 800, 256, &gap2);
    CHECK(solo.unpaired == 0);
    CHECK(solo.noteOns[1] == 0);
    CHECK(solo.noteOns[10] > 0 && solo.noteOns[10] < open.noteOns[10]);
}

TEST("plugin: lanes on sound knobs stay finite at every block size")
{
    for (int blockSize : { 16, 256, 2048 })
    {
        auto proc = std::make_unique<GristoProcessor>();
        proc->prepareToPlay(48000.0, blockSize);
        setParam(*proc, p::freeRun, 1.0f);
        setParam(*proc, p::laneId(p::arpOn, 0), 1.0f);
        setParam(*proc, p::laneId(p::arpMode, 0), 1.0f);
        const int targets[] = { gristo::mod::synthTarget(0, gristo::mod::kSynthCutoff), gristo::mod::kitTarget(gristo::mod::kKitTune),
                                gristo::mod::kFxSizeTarget, gristo::mod::synthTarget(0, gristo::mod::kSynthLevel) };
        for (int slot = 0; slot < 4; ++slot)
        {
            setParam(*proc, p::slotId(p::modTarget, 0, slot), static_cast<float>(targets[slot]));
            setParam(*proc, p::slotId(p::modAmount, 0, slot), slot % 2 == 0 ? 1.0f : -1.0f);
        }
        Run run = play(*proc, 48000 * 4 / blockSize, blockSize);
        CHECK_MSG(run.nonFinite == 0 && run.peak > 0.02f && run.peak <= 1.0f, "block " << blockSize << " peak " << run.peak);
    }
}

TEST("plugin: MIDI export matches what plays live")
{
    auto proc = makeProcessor();
    setParam(*proc, p::laneId(p::arpOn, 0), 1.0f);
    setParam(*proc, p::laneId(p::arpMode, 0), 1.0f);
    setParam(*proc, p::chordOn, 1.0f);
    setParam(*proc, p::humanize, 0.5f);
    const double tempo = getParam(*proc, p::freeTempo);
    const int blocks = static_cast<int>(std::ceil(16.0 * 60.0 / tempo * 48000.0 / 256.0)); // four bars
    // Live: note-ons per channel and their times, from bar 1.
    std::vector<double> liveKick;
    std::array<int, 17> live {};
    {
        juce::AudioBuffer<float> buffer(2, 256);
        const int kickNote = static_cast<int>(getParam(*proc, p::voiceId(p::note, 0)));
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            proc->processBlock(buffer, midi);
            for (const auto meta : midi)
            {
                const auto m = meta.getMessage();
                const double beats = (b * 256.0 + meta.samplePosition) * tempo / (60.0 * 48000.0);
                if (!m.isNoteOn() || beats >= 16.0)
                    continue;
                ++live[static_cast<size_t>(m.getChannel())];
                if (m.getChannel() == 10 && m.getNoteNumber() == kickNote)
                    liveKick.push_back(beats);
            }
        }
    }
    auto noteOns = [](const juce::MidiFile& file, int track, std::vector<double>* kickBeats = nullptr) {
        int n = 0;
        if (track < file.getNumTracks())
            for (const auto* e : *file.getTrack(track))
                if (e->message.isNoteOn())
                {
                    ++n;
                    if (kickBeats != nullptr && e->message.getNoteNumber() == 36)
                        kickBeats->push_back(e->message.getTimeStamp() / 960.0);
                }
        return n;
    };
    const auto all = proc->exportMidi(4, GristoProcessor::kAllParts);
    CHECK(all.getNumTracks() == 3); // drums, arp 1, chords: the parts that are on
    std::vector<double> fileKick;
    CHECK_MSG(noteOns(all, 0, &fileKick) == live[10], "drums " << noteOns(all, 0) << " vs live " << live[10]);
    CHECK_MSG(noteOns(all, 1) == live[1], "arp " << noteOns(all, 1) << " vs live " << live[1]);
    CHECK_MSG(noteOns(all, 2) == live[3], "chords " << noteOns(all, 2) << " vs live " << live[3]);
    CHECK(fileKick.size() == liveKick.size() && !fileKick.empty());
    for (size_t i = 0; i < std::min(fileKick.size(), liveKick.size()); ++i)
        CHECK_MSG(std::abs(fileKick[i] - liveKick[i]) < 0.01, "kick " << i << " at " << fileKick[i] << " vs " << liveKick[i]);

    // One part, every note closed inside the file, arps at their musical pitch.
    const auto arp = proc->exportMidi(4, gristo::kArpVoiceBase);
    CHECK(arp.getNumTracks() == 1);
    int open = 0, lowest = 127;
    for (const auto* e : *arp.getTrack(0))
    {
        open += e->message.isNoteOn() ? 1 : e->message.isNoteOff() ? -1 : 0;
        if (e->message.isNoteOn())
            lowest = std::min(lowest, e->message.getNoteNumber());
        CHECK(e->message.getTimeStamp() <= 16.0 * 960.0);
    }
    CHECK(open == 0);
    CHECK(lowest >= 36); // arp 1's root, no Out Shift
    const auto file = proc->exportMidiToTempFile(2, GristoProcessor::kAllDrums);
    CHECK(file.existsAsFile() && file.getSize() > 50);
    file.deleteFile();
}

TEST("plugin: a macro moves what it is routed to, also while stopped")
{
    auto kicks = [](float macro) {
        auto proc = makeProcessor();
        setParam(*proc, p::slotId(p::macroTarget, 0, 0), static_cast<float>(gristo::mod::kFill + gristo::Kick));
        setParam(*proc, p::slotId(p::macroAmount, 0, 0), -1.0f);
        setParam(*proc, p::laneId(p::macroValue, 0), macro);
        for (int v = 1; v < gristo::kNumVoices; ++v)
            setParam(*proc, p::voiceId(p::partMute, v), 1.0f);
        Run run = play(*proc, 1600);
        return run.noteOns[10];
    };
    CHECK(kicks(0.0f) > 4);
    CHECK(kicks(1.0f) == 0); // fill pulled all the way down
}

TEST("plugin: undo and redo step through finished edits")
{
    auto proc = makeProcessor();
    proc->pump(30);
    CHECK(!proc->canUndo());
    setParam(*proc, p::mapX, 0.2f);
    proc->pump(30);
    setParam(*proc, p::mapX, 0.9f);
    proc->pump(30);
    CHECK(proc->canUndo());
    proc->undo();
    CHECK(std::abs(getParam(*proc, p::mapX) - 0.2f) < 1.0e-4f);
    proc->pump(30); // undoing isn't itself a new step
    CHECK(proc->canRedo());
    proc->undo();
    CHECK(std::abs(getParam(*proc, p::mapX) - 0.5f) < 1.0e-4f);
    proc->redo();
    proc->redo();
    CHECK(std::abs(getParam(*proc, p::mapX) - 0.9f) < 1.0e-4f);
    CHECK(!proc->canRedo());
}

TEST("plugin: scenes store, recall and survive a project reload")
{
    auto proc = makeProcessor();
    setParam(*proc, p::freeRun, 0.0f); // stopped: a recall is immediate
    setParam(*proc, p::mapX, 0.2f);
    proc->storeScene(0);
    setParam(*proc, p::mapX, 0.8f);
    proc->storeScene(3);
    CHECK(proc->sceneUsed(0) && proc->sceneUsed(3) && !proc->sceneUsed(1));
    proc->recallScene(0);
    proc->pump(3);
    CHECK(std::abs(getParam(*proc, p::mapX) - 0.2f) < 1.0e-4f);
    CHECK(proc->currentScene.load() == 0);
    proc->recallScene(1); // empty: nothing happens
    proc->pump(3);
    CHECK(proc->currentScene.load() == 0);

    juce::MemoryBlock state;
    proc->getStateInformation(state);
    GristoProcessor other;
    other.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(other.sceneUsed(0) && other.sceneUsed(3) && !other.sceneUsed(1));
    other.recallScene(3);
    other.pump(3);
    CHECK(std::abs(getParam(other, p::mapX) - 0.8f) < 1.0e-4f);

    // Playing: the recall waits for the bar line.
    setParam(*proc, p::freeRun, 1.0f);
    play(*proc, 40);
    proc->recallScene(3);
    proc->pump(2);
    CHECK(std::abs(getParam(*proc, p::mapX) - 0.2f) < 1.0e-4f); // not yet
    Run run;
    for (int i = 0; i < 2000 && proc->pendingScene.load() >= 0; ++i)
    {
        run = play(*proc, 2, 256, &run);
        proc->pump(1);
    }
    CHECK(std::abs(getParam(*proc, p::mapX) - 0.8f) < 1.0e-4f);
    CHECK(std::fmod(proc->displayPpq.load() + 0.2, 4.0) < 0.5); // right at a bar line
}

TEST("plugin: dice gives new, playable settings and respects locks")
{
    auto proc = makeProcessor();
    const Snapshot before = proc->presets.captureSnapshot();
    proc->diceLocks.store((1 << GristoProcessor::NumSections) - 1);
    proc->diceAll();
    CHECK(proc->presets.captureSnapshot() == before); // everything locked
    proc->diceLocks.store(0);
    Run run;
    for (int i = 0; i < 12; ++i)
    {
        proc->diceAll();
        setParam(*proc, p::laneId(p::arpOn, i % 3), 1.0f);
        setParam(*proc, p::chordOn, 1.0f);
        run = play(*proc, 200, 256, &run);
        CHECK_MSG(run.nonFinite == 0 && run.peak <= 1.0f, "dice round " << i);
    }
    CHECK(proc->presets.captureSnapshot() != before);
}

TEST("plugin: a drum sample loads, plays and comes back with the project")
{
    // A short 440 Hz one-shot as a WAV file.
    const auto wav = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("gristo_test_shot.wav");
    wav.deleteFile();
    {
        juce::AudioBuffer<float> buffer(1, 4410);
        for (int i = 0; i < 4410; ++i)
            buffer.setSample(0, i, 0.5f * std::sin(6.2831853f * 440.0f * static_cast<float>(i) / 44100.0f));
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(new juce::FileOutputStream(wav), 44100.0, 1, 16, {}, 0));
        CHECK(writer != nullptr);
        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer(buffer, 0, 4410);
    }
    auto proc = makeProcessor();
    CHECK(!proc->loadDrumSample(0, juce::URL(juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("gristo_no_such.wav"))));
    CHECK(proc->loadDrumSample(0, juce::URL(wav)));
    CHECK(proc->drumSampleName(0) == "gristo_test_shot.wav");
    CHECK(GristoProcessor::samplesFolder().getChildFile("gristo_test_shot.wav").existsAsFile()); // copied in
    Run run = play(*proc, 600);
    CHECK(run.nonFinite == 0 && run.peak > 0.05f);

    juce::MemoryBlock state;
    proc->getStateInformation(state);
    GristoProcessor other;
    other.prepareToPlay(48000.0, 256);
    other.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(other.drumSampleName(0) == "gristo_test_shot.wav");
    proc->clearDrumSample(0);
    CHECK(proc->drumSampleName(0).isEmpty());
    run = play(*proc, 200, 256, &run);
    CHECK(run.nonFinite == 0);
    GristoProcessor::samplesFolder().getChildFile("gristo_test_shot.wav").deleteFile();
    wav.deleteFile();
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    int ran = 0;
    for (const auto& test : gt::registry())
    {
        const int before = gt::failures();
        test.fn();
        std::cout << (gt::failures() == before ? "[ ok ] " : "[FAIL] ") << test.name << "\n";
        ++ran;
    }
    std::cout << "\n" << ran << " plugin tests, " << gt::failures() << " failures\n";
    return gt::failures() == 0 ? 0 : 1;
}

TEST("plugin: played by Keys, a white key from C1 plays its chord")
{
    auto proc = makeProcessor();
    setParam(*proc, p::chordOn, 1.0f);
    setParam(*proc, p::chordMode, 1.0f);
    setParam(*proc, p::chordStrum, 20.0f);
    setParam(*proc, p::chordVoicing, 2.0f);
    proc->pump(2);
    const int channel = static_cast<int>(getParam(*proc, p::chordChannel));
    juce::AudioBuffer<float> buffer(2, 256);
    auto run = [&](int note, bool on, int blocks) {
        int ons = 0, offs = 0;
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0 && note >= 0)
                midi.addEvent(on ? juce::MidiMessage::noteOn(1, note, 0.8f) : juce::MidiMessage::noteOff(1, note), 0);
            proc->processBlock(buffer, midi);
            for (const auto meta : midi)
                if (meta.getMessage().getChannel() == channel)
                {
                    ons += meta.getMessage().isNoteOn() ? 1 : 0;
                    offs += meta.getMessage().isNoteOff() ? 1 : 0;
                }
        }
        return std::make_pair(ons, offs);
    };
    CHECK(run(-1, false, 100) == std::make_pair(0, 0)); // nothing until a key goes down
    CHECK(run(38, true, 100) == std::make_pair(4, 0));  // D1: the second of the seven chords around home, triad + bass
    CHECK(run(40, true, 100) == std::make_pair(4, 4));  // E1 takes over
    CHECK(run(40, false, 100) == std::make_pair(4, 4)); // back to D1, still held
    CHECK(run(38, false, 100) == std::make_pair(0, 4));
    CHECK(run(37, true, 50) == std::make_pair(0, 0));   // black keys in the zone do nothing
}

TEST("plugin: the big knobs act on the sound and the pattern; Catch loops the harmony; Mutate nudges")
{
    // Busy all the way down: no drums. Dirt of every kind: finite and bounded.
    {
        auto proc = makeProcessor();
        setParam(*proc, p::busy, 0.0f);
        proc->pump(2);
        const Run quiet = play(*proc, 400);
        CHECK(quiet.noteOns[10] == 0);
        setParam(*proc, p::busy, 1.0f);
        proc->pump(2);
        const Run full = play(*proc, 400, 256, &quiet);
        CHECK(full.noteOns[10] > 60);
        CHECK(full.unpaired == 0);
    }
    for (int grit = 0; grit < 3; ++grit)
    {
        auto proc = makeProcessor();
        setParam(*proc, p::grit, static_cast<float>(grit));
        setParam(*proc, p::dirt, 1.0f);
        setParam(*proc, p::space, 1.0f);
        setParam(*proc, p::loose, 1.0f);
        setParam(*proc, p::wild, 1.0f);
        setParam(*proc, p::chordOn, 1.0f);
        setParam(*proc, p::chordWander, 1.0f);
        proc->pump(2);
        const Run run = play(*proc, 1500);
        CHECK(run.nonFinite == 0);
        CHECK(run.peak > 0.05f && run.peak <= 1.0f);
        CHECK(run.unpaired == 0);
    }
    // Catch freezes the phrase playing now; again lets it go.
    {
        auto proc = makeProcessor();
        setParam(*proc, p::chordOn, 1.0f);
        proc->pump(2);
        play(*proc, 3000); // well into the song
        CHECK(!proc->harmonyCaught());
        proc->catchHarmony();
        CHECK(proc->harmonyCaught());
        CHECK(getParam(*proc, p::chordLoop) >= 0.0f);
        proc->catchHarmony();
        CHECK(!proc->harmonyCaught());
    }
    // Mutate: small amounts stay close, Keep protects a section, Undo goes back.
    {
        auto proc = makeProcessor();
        proc->pump(60);
        const float x = getParam(*proc, p::mapX), cutoff = getParam(*proc, p::laneId(p::synthCutoff, 0));
        proc->diceLocks.store(1 << GristoProcessor::DiceSound1);
        bool moved = false;
        for (int i = 0; i < 8; ++i)
        {
            proc->mutate(0.2f);
            CHECK(std::abs(getParam(*proc, p::mapX) - x) <= 8 * 0.2f * 0.35f + 1e-4f);
            moved = moved || std::abs(getParam(*proc, p::mapX) - x) > 1e-4f;
        }
        CHECK(moved);
        CHECK(getParam(*proc, p::laneId(p::synthCutoff, 0)) == cutoff);
        proc->mutate(0.0f);
        const Run run = play(*proc, 300);
        CHECK(run.unpaired == 0 && run.nonFinite == 0);
    }
}

TEST("plugin: morphing blends knobs towards a scene and flips switches halfway")
{
    auto proc = makeProcessor();
    setParam(*proc, p::busy, 0.2f);
    setParam(*proc, p::grit, 0.0f);
    proc->pump(2);
    CHECK(!proc->beginMorph(0)); // nothing stored there
    setParam(*proc, p::busy, 0.8f);
    setParam(*proc, p::grit, 2.0f);
    proc->storeScene(0);
    setParam(*proc, p::busy, 0.2f);
    setParam(*proc, p::grit, 0.0f);
    CHECK(proc->beginMorph(0));
    proc->setMorph(0.25f);
    CHECK(std::abs(getParam(*proc, p::busy) - 0.35f) < 0.011f);
    CHECK(getParam(*proc, p::grit) == 0.0f);
    proc->setMorph(0.75f);
    CHECK(std::abs(getParam(*proc, p::busy) - 0.65f) < 0.011f);
    CHECK(getParam(*proc, p::grit) == 2.0f);
    proc->setMorph(0.0f);
    CHECK(std::abs(getParam(*proc, p::busy) - 0.2f) < 0.011f);
    proc->setMorph(1.0f);
    CHECK(std::abs(getParam(*proc, p::busy) - 0.8f) < 0.011f);
    CHECK(proc->currentScene.load() == 0);
    proc->endMorph();
    const Run run = play(*proc, 200);
    CHECK(run.unpaired == 0 && run.nonFinite == 0);
}

TEST("plugin: MIDI learn binds a controller; the controller layouts drive knobs, switches, scenes and the harmony grid")
{
    auto proc = makeProcessor();
    juce::AudioBuffer<float> buffer(2, 256);
    auto send = [&](const juce::MidiMessage& m) {
        juce::MidiBuffer midi;
        midi.addEvent(m, 0);
        proc->processBlock(buffer, midi);
        proc->pump(2);
    };
    // Learn: the next controller moved is bound, and from then on it sets the value.
    proc->controls.startLearn(ControlTarget::param(p::dirt));
    send(juce::MidiMessage::controllerEvent(3, 21, 10));
    CHECK(!proc->controls.learning());
    CHECK(proc->controls.keyFor(ControlTarget::param(p::dirt)) == ControlMap::key(3, false, 21));
    send(juce::MidiMessage::controllerEvent(3, 21, 127));
    CHECK(std::abs(getParam(*proc, p::dirt) - 1.0f) < 0.01f);
    send(juce::MidiMessage::controllerEvent(3, 21, 0));
    CHECK(getParam(*proc, p::dirt) < 0.01f);
    send(juce::MidiMessage::controllerEvent(4, 21, 127)); // another channel: not this binding
    CHECK(getParam(*proc, p::dirt) < 0.01f);

    // Launch Control XL, rhythm layout on channel 15.
    proc->loadControllerLayout(GristoProcessor::LaunchControlRhythm, 15);
    send(juce::MidiMessage::controllerEvent(15, 13, 127)); // first knob, top row: Kick Fill
    CHECK(getParam(*proc, p::voiceId(p::fill, 0)) > 0.99f);
    send(juce::MidiMessage::controllerEvent(15, 82, 0)); // sixth fader: Perc 2 level
    CHECK(getParam(*proc, p::voiceId(p::drumLevel, 5)) < 0.01f);
    send(juce::MidiMessage::noteOn(15, 42, 1.0f)); // second Track Focus button: Snare mute
    send(juce::MidiMessage::noteOff(15, 42));
    CHECK(getParam(*proc, p::voiceId(p::partMute, 1)) > 0.5f);
    send(juce::MidiMessage::noteOn(15, 42, 1.0f));
    CHECK(getParam(*proc, p::voiceId(p::partMute, 1)) < 0.5f);
    CHECK(!proc->sceneUsed(2));
    send(juce::MidiMessage::noteOn(15, 75, 1.0f)); // third Track Control button: scene 3 (empty: stores)
    CHECK(proc->sceneUsed(2));
    CHECK(std::abs(getParam(*proc, p::dirt)) < 0.01f); // the learned binding on channel 3 is untouched
    CHECK(proc->controls.keyFor(ControlTarget::param(p::dirt)) >= 0);

    // Melody layout on channel 14: Grit buttons pick the kind.
    proc->loadControllerLayout(GristoProcessor::LaunchControlMelody, 14);
    send(juce::MidiMessage::noteOn(14, 92, 1.0f));
    CHECK(getParam(*proc, p::grit) == 2.0f);
    send(juce::MidiMessage::controllerEvent(14, 34, 127));
    CHECK(getParam(*proc, p::fxShimmer) > 0.99f);

    // APC40: column 5 (its channel), top row = the tonic column, richest row.
    proc->loadControllerLayout(GristoProcessor::Apc40, 5);
    send(juce::MidiMessage::noteOn(9, 53, 1.0f));
    CHECK(std::abs(getParam(*proc, p::chordHomeX) - 0.5f) < 0.01f);
    CHECK(getParam(*proc, p::chordHomeY) > 0.99f);
    send(juce::MidiMessage::noteOn(5, 57, 1.0f)); // first column, bottom row: far left, plain triads
    CHECK(std::abs(getParam(*proc, p::chordHomeX) - 0.1f) < 0.01f);
    CHECK(getParam(*proc, p::chordHomeY) < 0.01f);

    // Bound notes never reach the arps; the map comes back with the project.
    juce::MemoryBlock saved;
    proc->getStateInformation(saved);
    auto other = makeProcessor();
    other->setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    CHECK(other->controls.bindings().size() == proc->controls.bindings().size());
    // One controller per control: the layout loaded last (APC40, fourth track knob) holds Dirt now.
    CHECK(other->controls.keyFor(ControlTarget::param(p::dirt)) == ControlMap::key(5, false, 51));
    const Run run = play(*proc, 200);
    CHECK(run.nonFinite == 0);
}

TEST("plugin: a lane or macro can move any parameter; drum sound settings and trig conditions reach the engine")
{
    auto proc = makeProcessor();
    // Every knob-like parameter is a target and has a name.
    const int dirt = p::parameterTarget(p::dirt), bodyTune = p::parameterTarget(p::voiceId(p::drumBodyTune, 3));
    CHECK(dirt >= gristo::mod::kNumTargets && bodyTune > dirt - 2000 && bodyTune >= gristo::mod::kNumTargets);
    CHECK(p::targetDisplayName(dirt) == "Dirt");
    CHECK(p::parameterTarget(p::voiceId(p::note, 0)) < 0);   // MIDI routing stays put
    CHECK(p::parameterTarget(p::sourceTargetId(0, 0)) < 0);  // and so do the slots themselves
    CHECK(p::numParameterTargets() > 300);
    int unnamed = 0;
    for (int t = gristo::mod::kNumTargets; t < gristo::mod::kNumTargets + p::numParameterTargets(); ++t)
        unnamed += p::targetDisplayName(t).isEmpty() ? 1 : 0;
    CHECK(unnamed == 0);

    // Macro 1 routed to Busy at -100 %: turning the macro up empties the drums, without
    // touching the Busy knob itself.
    setParam(*proc, p::sourceTargetId(gristo::kNumModLanes, 0), static_cast<float>(p::parameterTarget(p::busy)));
    setParam(*proc, p::sourceAmountId(gristo::kNumModLanes, 0), -1.0f);
    proc->pump(2);
    const Run before = play(*proc, 400);
    CHECK(before.noteOns[10] > 8);
    setParam(*proc, p::laneId(p::macroValue, 0), 1.0f);
    proc->pump(2);
    const Run muted = play(*proc, 400, 256, &before);
    CHECK_MSG(muted.noteOns[10] <= 2, muted.noteOns[10] << " drum notes with Busy pulled down by the macro");
    CHECK(std::abs(getParam(*proc, p::busy) - 0.5f) < 1e-4f);
    CHECK(muted.unpaired == 0);
    setParam(*proc, p::laneId(p::macroValue, 0), 0.0f);
    proc->pump(2);
    const Run back = play(*proc, 400, 256, &muted);
    CHECK(back.noteOns[10] > 8);

    // A drawn lane on a drum's Body Tune: plays, finite, paired.
    setParam(*proc, p::sourceTargetId(0, 0), static_cast<float>(bodyTune));
    setParam(*proc, p::sourceAmountId(0, 0), 1.0f);
    setParam(*proc, p::voiceId(p::drumBody, 3), 1.0f);
    setParam(*proc, p::voiceId(p::drumBody, 0), 0.8f);
    setParam(*proc, p::voiceId(p::drumTune, 1), -7.0f);
    proc->pump(2);
    const Run lane = play(*proc, 800, 256, &back);
    CHECK(lane.nonFinite == 0 && lane.peak <= 1.0f && lane.unpaired == 0);

    // Trig condition: the kick only in the 4th of 4 bars.
    auto fresh = makeProcessor();
    setParam(*fresh, p::voiceId(p::voiceCondition, 0), 4.0f);
    for (int v = 1; v < gristo::kNumVoices; ++v)
        setParam(*fresh, p::voiceId(p::partMute, v), 1.0f);
    fresh->pump(2);
    const double blocksPerBar = 4.0 * 60.0 / 95.0 * 48000.0 / 256.0;
    const Run three = play(*fresh, static_cast<int>(blocksPerBar * 2.9));
    CHECK(three.noteOns[10] == 0);
    const Run fourth = play(*fresh, static_cast<int>(blocksPerBar * 1.1), 256, &three);
    CHECK(fourth.noteOns[10] > 0);
}
