// Headless tests of the plugin: parameters, presets, state and the audio / MIDI it
// produces. Runs without a display.

#include "TestFramework.h"

#include "PluginProcessor.h"

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
    CHECK(list.size() >= 14);
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
                CHECK_MSG(!PresetManager::isRoutingParameter(id), id.toStdString() << " is a routing parameter");
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
