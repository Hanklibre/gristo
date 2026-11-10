#include "PluginEditor.h"

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace p = gristo::params;
namespace mod = gristo::mod;
using namespace theme;

namespace {

// Layout (design size 1280 x 760): top bar, part strip on the left, the selected
// part's page, and the timeline (pattern, lanes, routing) along the bottom.
#if JUCE_ANDROID || JUCE_IOS || GRISTO_SIMULATE_MOBILE
constexpr bool kIsMobile = true;
#else
constexpr bool kIsMobile = false;
#endif
constexpr int kTopBar = 78; // two rows: transport, then scenes and the help line
constexpr int kPageX = 200, kPageY = 82, kPageW = 1072, kPageH = 500;
constexpr int kTimelineY = 588;
constexpr int partRowY(int part) { return 88 + part * 36 + (part >= 6 ? 6 : 0) + (part >= 9 ? 6 : 0) + (part >= 10 ? 6 : 0); }
constexpr int kMacroY = 506; // two rows of four macro knobs under the parts

// Voices matrix geometry
constexpr int kVoiceX0 = kPageX + 384;
constexpr int kVoiceColW = 70;
constexpr int kVoiceRowY[] = { 124, 180, 236, 292, 348, 404 }; // Pattern tab
constexpr int kPlayRowY[] = { 124, 172, 220, 268, 316, 364, 412 }; // Play tab
constexpr int kVoiceRowH = 44;

// The arp and chord blocks keep their own 952-wide layout (from x = 24), moved into the page.
constexpr int kBlockDx = kPageX - 24 + (kPageW - 952) / 2;
constexpr int kModY = 60;
constexpr int kSlotX = 972;
constexpr int kModRowY = kModY + 34;      // the block's first row
constexpr int kSlotRowY[] = { kTimelineY + 34, kTimelineY + 72, kTimelineY + 110, kTimelineY + 148 };
constexpr int kModBottomY = kModY + 234;  // the bottom row of the arp / chord blocks
const char* const kTriggerNames[] = { "BD", "SD", "H1", "H2", "P1", "P2" };

const float kScales[] = { 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.25f };

} // namespace

// ---------------------------------------------------------------------------------
// View

GristoView::GristoView(GristoProcessor& proc)
    : processor_(proc), mapPad_(proc.state, previewEngine_.map()), patternView_(previewEngine_), timelineView_(proc), arpView_(proc), laneEditor_(proc)
{
    setLookAndFeel(&lookAndFeel_);

    addAndMakeVisible(mapPad_);
    addChildComponent(patternView_); // replaced by the timeline
    addAndMakeVisible(timelineView_);
    timelineView_.onSelect = [this](int part) { partSelect_[part].triggerClick(); };
    setHelp(timelineView_, "Timeline", "Every part over the bars of the selected lane, each note where it will sound. Click a row to open that part.");
    nodeLabel_.setJustificationType(juce::Justification::centredLeft);
    nodeLabel_.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(nodeLabel_);
    sizeButton_.onClick = [this] { showSizeMenu(); };
    addAndMakeVisible(sizeButton_);
    if (kIsMobile)
    {
        // No title bar on mobile, so the audio / MIDI settings live here instead of a zoom menu.
        sizeButton_.setButtonText("Settings");
        sizeButton_.onClick = [this] {
            if (onSettingsRequested)
                onSettingsRequested();
        };
    }
    keysButton_.setClickingTogglesState(true);
    keysButton_.setToggleState(proc.showKeys.load(), juce::dontSendNotification);
    keysButton_.setColour(juce::TextButton::buttonOnColourId, colours::accent);
    keysButton_.setColour(juce::TextButton::textColourOnId, colours::background);
    keysButton_.onClick = [this] {
        processor_.showKeys.store(keysButton_.getToggleState());
        if (onKeysToggled)
            onKeysToggled();
    };
    addAndMakeVisible(keysButton_);
    setHelp(keysButton_, "Keys", "Shows a keyboard to play chords into the arps (Arp mode) without a MIDI keyboard.");
    setupPresetBar();

    setupKnob(chaos_, p::chaos, "Chaos");
    setupKnob(variation_, p::variation, "Variation");
    setupRandomKnob(mapRandom_, p::mapRandom, "Map Random");
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        setupCellKnob(fills_[v], p::voiceId(p::fill, v), false);
        setupCellKnob(fillRandoms_[v], p::voiceId(p::fillRandom, v), true);
        setupCellKnob(probabilities_[v], p::voiceId(p::probability, v), false);
        setupCellKnob(probabilityRandoms_[v], p::voiceId(p::probabilityRandom, v), true);
        setupCellKnob(voiceHumanize_[v], p::voiceId(p::voiceHumanize, v), false);
        setupCellKnob(voiceDynamics_[v], p::voiceId(p::voiceDynamics, v), false);
        setupCellKnob(voiceRatchet_[v], p::voiceId(p::voiceRatchet, v), false);
        setupCellKnob(voiceSwing_[v], p::voiceId(p::voiceSwing, v), false);
        voiceLengths_[v] = std::make_unique<NumberSelector>(*proc.state.getParameter(p::voiceId(p::voiceLength, v)), "", 1, gristo::kPatternSteps, " steps");
        addAndMakeVisible(*voiceLengths_[v]);
        velocityRanges_[v] = std::make_unique<VelocityRange>(*proc.state.getParameter(p::voiceId(p::velMin, v)),
                                                             *proc.state.getParameter(p::voiceId(p::velMax, v)), voiceColours[v]);
        addAndMakeVisible(*velocityRanges_[v]);
        notes_[v] = std::make_unique<NoteSelector>(*proc.state.getParameter(p::voiceId(p::note, v)));
        addAndMakeVisible(*notes_[v]);
        setupToggle(mutes_[v], p::voiceId(p::partMute, v), "Mute");
        sampleButtons_[v].onClick = [this, v] { showSampleMenu(v); };
        addAndMakeVisible(sampleButtons_[v]);
        setHelp(sampleButtons_[v], juce::String(gristo::voiceName(v)) + " sample",
                "Load your own one-shot for this drum instead of the kit's sound. Kit Tune, Decay (below 50% = shorter) and level still apply.");
    }
    for (int t = 0; t < 2; ++t)
    {
        voiceTabs_[t].setButtonText(t == 0 ? "Pattern" : "Play");
        voiceTabs_[t].setClickingTogglesState(false);
        voiceTabs_[t].onClick = [this, t] { showVoiceTab(t); };
        addAndMakeVisible(voiceTabs_[t]);
    }
    setHelp(voiceTabs_[0], "Pattern", "What each drum plays: how many notes, how likely, ratchets and its loop length.");
    setHelp(voiceTabs_[1], "Play", "How each drum is played and sent: feel, swing, velocity, MIDI note and channel, your own sample.");
    setupChoice(timing_, p::timing, "Timing");
    setupKnob(shuffle_, p::shuffle, "Shuffle");
    setupChoice(feeling_, p::feeling, "Feeling");
    setupKnob(feelMs_, p::feelMs, "Flam ms");
    setupRandomKnob(shuffleRandom_, p::shuffleRandom, "Random");
    setupRandomKnob(feelRandom_, p::feelRandom, "Random");
    setupKnob(humanize_, p::humanize, "Humanize");
    setupKnob(dynamics_, p::dynamics, "Dynamics");
    setupToggle(lock_, p::humanizeLock, "Lock humanize");
    setupToggle(linear_, p::linear, "Linear");
    setupToggle(kitOn_, p::kitOn, "Sound");
    setupKnob(kitGain_, p::kitGain, "Level");
    setupToggle(freeRun_, p::freeRun, "Play");
    setupKnob(freeTempo_, p::freeTempo, "Tempo");
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        channels_[v] = std::make_unique<NumberSelector>(*proc.state.getParameter(p::voiceId(p::voiceChannel, v)), "Ch ", 1, 16);
        addAndMakeVisible(*channels_[v]);
        setHelp(*channels_[v], juce::String(gristo::voiceName(v)) + " MIDI channel",
                "MIDI channel this voice is sent on, for DAWs that keep channels. Ableton merges them: split by note there.");
    }
    setupArpPage();
    setupSoundPage();
    setupChordPage();
    for (int pg = 0; pg <= kSoundsPage; ++pg)
    {
        auto& tab = pageTabs_[pg];
        tab.setButtonText(pg == 0 ? "Modulation" : pg == kSoundsPage ? "Sounds" : pg == kChordsPage ? "Chords" : "Arp " + juce::String(pg));
        tab.setClickingTogglesState(true);
        tab.setRadioGroupId(4712);
        tab.setColour(juce::TextButton::buttonOnColourId,
                      pg == 0 || pg >= kChordsPage ? colours::accent : arpColours[pg - 1]);
        tab.setColour(juce::TextButton::textColourOnId, colours::background);
        tab.onClick = [this, pg] {
            showPage(pg);
            if (onBottomPageChanged)
                onBottomPageChanged(pg);
        };
        addAndMakeVisible(tab);
        if (pg == kSoundsPage)
            setHelp(tab, "Sounds", "The built-in sounds: drum kit, a synth for each arp and the chords, reverb and delay.");
        else if (pg == kChordsPage)
            setHelp(tab, "Chords", "A Scaler-style chord progression that plays its own part and can feed the arps.");
        else
            setHelp(tab, pg == 0 ? "Modulation" : "Arp " + juce::String(pg),
                    pg == 0 ? "Drawn modulation lanes and their routing."
                            : "A drum-clocked arpeggiator / Sting-style sequencer on its own MIDI channel.");
    }
    setupKnob(noteLength_, p::noteLength, "Note Len");

    // ---- Modulation panel
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
    {
        auto& tab = laneTabs_[lane];
        tab.setButtonText("Lane " + juce::String(lane + 1));
        tab.setClickingTogglesState(true);
        tab.setRadioGroupId(4711);
        tab.setColour(juce::TextButton::buttonOnColourId, laneColours[lane].withAlpha(0.85f));
        tab.setColour(juce::TextButton::textColourOnId, colours::background);
        tab.onClick = [this, lane] { selectLane(lane); };
        addAndMakeVisible(tab);
        setHelp(tab, "Lane " + juce::String(lane + 1), "Show this modulation lane. Each lane is a drawn curve that loops over 1, 2 or 4 bars.");
    }
    addAndMakeVisible(laneHandle_);
    addAndMakeVisible(laneEditor_);
    for (const auto& name : { "1 bar", "2 bars", "4 bars" })
        laneLength_.addItem(name, laneLength_.getNumItems() + 1);
    addAndMakeVisible(laneLength_);
    shapesButton_.setButtonText("Shapes");
    shapesButton_.onClick = [this] {
        juce::PopupMenu menu;
        for (int s = 0; s < static_cast<int>(gristo::ModShape::Count); ++s)
            menu.addItem(s + 1, gristo::modShapeName(static_cast<gristo::ModShape>(s)));
        juce::Component::SafePointer<GristoView> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&shapesButton_),
                           [safe](int result) {
                               if (safe != nullptr && result > 0)
                                   safe->laneEditor_.applyShape(static_cast<gristo::ModShape>(result - 1));
                           });
    };
    addAndMakeVisible(shapesButton_);
    for (int w = 0; w < static_cast<int>(gristo::LaneWave::Count); ++w)
        laneWave_.addItem(gristo::laneWaveName(static_cast<gristo::LaneWave>(w)), w + 1);
    for (int r = 0; r < gristo::kNumLfoRates; ++r)
        laneRate_.addItem(gristo::lfoRateName(r), r + 1);
    addAndMakeVisible(laneWave_);
    addChildComponent(laneRate_);
    setHelp(laneWave_, "Lane mode", "Drawn: the curve you draw. LFO: a wave that runs in time with the tempo instead.");
    setHelp(laneRate_, "LFO rate", "How long one cycle of the LFO lasts.");

    juce::StringArray targetNames;
    for (int t = 0; t < mod::kNumTargets; ++t)
        targetNames.add(mod::targetName(t));
    for (auto& slot : slots_)
    {
        slot.target.addItemList(targetNames, 1);
        addAndMakeVisible(slot.target);
        slot.amount.setSliderStyle(juce::Slider::LinearBar);
        slot.amount.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        slot.amount.setColour(juce::Slider::textBoxTextColourId, colours::text);
        slot.amount.setDoubleClickReturnValue(true, 0.0);
        addAndMakeVisible(slot.amount);
        setHelp(slot.target, "Routing target", "Parameter this lane modulates. Choose None to remove the routing.");
        setHelp(slot.amount, "Routing amount", "How far the lane moves the target at full curve height. Negative = the curve pushes the value down. Double-click = 0.");
    }

    // ---- Drop targets (Massive-style: drag a lane handle onto a knob)
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        makeTarget(fills_[v].slider, fills_[v].slider, mod::kFill + v);
        makeTarget(probabilities_[v].slider, probabilities_[v].slider, mod::kProbability + v);
        makeTarget(*velocityRanges_[v], *velocityRanges_[v], mod::kVelocity + v);
        targetSliders_[static_cast<size_t>(mod::kFill + v)] = &fills_[v].slider;
        targetSliders_[static_cast<size_t>(mod::kProbability + v)] = &probabilities_[v].slider;
    }
    makeTarget(chaos_.slider, chaos_.slider, mod::kChaos);
    makeTarget(shuffle_.slider, shuffle_.slider, mod::kShuffle);
    makeTarget(feelMs_.slider, feelMs_.slider, mod::kFlamMs);
    makeTarget(humanize_.slider, humanize_.slider, mod::kHumanize);
    targetSliders_[mod::kChaos] = &chaos_.slider;
    targetSliders_[mod::kShuffle] = &shuffle_.slider;
    targetSliders_[mod::kFlamMs] = &feelMs_.slider;
    targetSliders_[mod::kHumanize] = &humanize_.slider;
    mapPad_.onLaneAxisChosen = [this](int lane, int target) { assignLane(lane, target); };

    // ---- Help texts, shown in the header box while hovering
    setHelp(mapPad_, "Map", "Drag to morph between 36 grooves. Left-right: straight to syncopated. Top-bottom: four-on-the-floor to broken. Drop a lane here to modulate X or Y.");
    setHelp(patternView_, "Pattern", "The 2 bars playing now, drawn where each note lands. Grey ticks = straight grid, outlines = notes skipped by Probability.");
    setHelp(chaos_, "Chaos", "Randomly raises and lowers step levels every 2 bars, adding and removing notes.");
    setHelp(mapRandom_, "Map Random", "Moves the map position randomly around the crosshair, to a new spot every bar.");
    setHelp(variation_, "Variation", "Picks a different set of random choices for Chaos, Probability, Humanize and all Random knobs.");
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        const juce::String name = gristo::voiceName(v);
        setHelp(fills_[v], name + " Fill", "How many notes this voice plays. Low = backbone only, high = ghost notes and fills.");
        setHelp(fillRandoms_[v], name + " Fill Random", "Varies this voice's Fill every bar, up to +-35 %. Busy bars and sparse bars.");
        setHelp(probabilities_[v], name + " Probability", "Chance that each note plays. Skipped notes differ every bar but repeat on re-render.");
        setHelp(probabilityRandoms_[v], name + " Probability Random", "Varies this voice's Probability every bar, up to +-50 %.");
        setHelp(voiceHumanize_[v], name + " Humanize", "Scales the master Humanize for this voice. 0 % = on the grid, 200 % = twice as loose.");
        setHelp(voiceRatchet_[v], name + " Ratchet", "Chance that a hit plays as two or three quick hits. Arps listening to this drum ratchet with it.");
        setHelp(voiceSwing_[v], name + " Swing", "How much of the Shuffle this drum follows. 0 % = straight on the grid while the others swing.");
        setHelp(*voiceLengths_[v], name + " Length", "Polymeter: the drum loops after this many 16ths. 32 = the whole 2-bar pattern. Try 5 or 7 on a hat.");
        setHelp(voiceDynamics_[v], name + " Dynamics", "Scales the master Dynamics for this voice: how much softer its weaker notes are.");
        setHelp(*velocityRanges_[v], name + " Velocity", "MIDI velocity range. Accents play at the top, ghost notes near the bottom. Drag the ends; double-click = 1-127.");
        setHelp(*notes_[v], name + " Note", "MIDI note this voice sends. Defaults are GM three octaves down (Pitch +36 in Live restores GM). Click for a list, scroll to step.");
        setHelp(mutes_[v].button, name + " Mute", "Silence this voice. It still clocks the arps and chords that listen to it.");
    }
    setHelp(timing_.box, "Timing", "How the 16ths move. Trip: tuplet snaps. Shake: MPC swing. Push: leans early. Clave: swings around 3-3-2.");
    setHelp(feeling_.box, "Feeling", "Tight: all together. Rolls: Hihat 2 plays rolls. Flam: kick and snare late. Slop: Flam, hats follow.");
    setHelp(shuffle_, "Shuffle", "Depth of the Timing model. 0 = straight, 6 = extreme.");
    setHelp(shuffleRandom_, "Shuffle Random", "Varies the Shuffle depth from beat to beat, up to +-3.");
    setHelp(feelMs_, "Flam ms", "How late kick and snare land in the Flam and Slop feelings.");
    setHelp(feelRandom_, "Feel Random", "Flam / Slop: each hit's delay varies 0 to 2x. Rolls: each roll has 2 to 5 hits.");
    setHelp(humanize_, "Humanize", "Player-like timing and velocity variation. Downbeats stay tight, ghost notes loosen up.");
    setHelp(dynamics_, "Dynamics", "How much softer the weaker notes of the pattern are played. 0 = all the same.");
    setHelp(lock_.button, "Lock", "Humanize repeats every 2 bars instead of varying bar to bar.");
    setHelp(linear_.button, "Linear", "One limb at a time: snare beats kick beats hats and percussion on the same step.");
    setHelp(kitOn_.button, "Sound", "Built-in drums and arp synths (Sounds tab). Turn off when only your own instruments should play.");
    setHelp(kitGain_, "Level", "Volume of the built-in sounds.");
    setHelp(freeRun_.button, "Free Run", "Play at the Tempo knob while the host transport is stopped (or in the standalone app).");
    setHelp(freeTempo_, "Tempo", "Tempo for Free Run.");
    setHelp(noteLength_, "Note Length", "Length of the MIDI notes, in beats.");
    setHelp(laneHandle_, "Assign lane", "Drag onto a knob (Fill, Probability, Chaos, Shuffle, Flam ms, Humanize), a Velocity range or the map to route this lane there. On the Arp pages, use the L1-L4 chips.");
    setHelp(laneEditor_, "Lane curve", "Draw the modulation curve, one value per 16th. Right-click for shapes. The bright column is the current step.");
    setHelp(laneLength_, "Lane length", "How many bars the lane's curve spans before it loops.");
    setHelp(shapesButton_, "Shapes", "Fill the lane with a ready-made curve: ramps, triangle, sine, square, per-beat saw, offbeats or random steps.");
    setHelp(sizeButton_, "UI size", "Scale the whole window.");

    helpTitle_ = "Gristo";
    helpText_ = "Hover over any control to see what it does.";
    addMouseListener(this, true);

    addChildComponent(armCatcher_);
    armCatcher_.setAlwaysOnTop(true);
    armCatcher_.onTap = [this](juce::Point<int> pos) {
        // Route the armed lane to whatever target is under the tap.
        const int lane = armedLane_;
        disarmLane();
        for (auto* c = getComponentAt(pos); c != nullptr && c != this; c = c->getParentComponent())
            if (auto* target = dynamic_cast<ModDropTarget*>(c); target != nullptr && target->onLaneDropped)
            {
                target->onLaneDropped(lane);
                return;
            }
    };
    for (auto* handle : laneHandles())
        handle->onTap = [this](int lane) {
            if (armedLane_ == lane)
                disarmLane();
            else
                armLane(lane);
        };

    laneTabs_[0].setToggleState(true, juce::dontSendNotification);
    selectLane(0);
    setupPartStrip();
    setupPerformance();
    partSelect_[0].setToggleState(true, juce::dontSendNotification);
    showPage(0);

    setSize(kWidth, kHeight);
    timerCallback();
    startTimerHz(30);
}

GristoView::~GristoView()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void GristoView::setHelp(juce::Component& component, const juce::String& title, const juce::String& text)
{
    component.getProperties().set("helpTitle", title);
    component.getProperties().set("helpText", text.replace("+-", juce::String::fromUTF8("\xc2\xb1")));
}

void GristoView::setHelp(Knob& knob, const juce::String& title, const juce::String& text)
{
    setHelp(knob.slider, title, text);
    setHelp(knob.label, title, text);
}

void GristoView::showHelpFor(juce::Component* component)
{
    for (auto* c = component; c != nullptr && c != this; c = c->getParentComponent())
    {
        if (c->getProperties().contains("helpTitle"))
        {
            const juce::String title = c->getProperties()["helpTitle"];
            const juce::String text = c->getProperties()["helpText"];
            if (title != helpTitle_ || text != helpText_)
            {
                helpTitle_ = title;
                helpText_ = text;
                repaint(520, 40, 760, 34);
                if (onHelpChanged)
                    onHelpChanged();
            }
            return;
        }
    }
}

void GristoView::mouseMove(const juce::MouseEvent& e) { showHelpFor(e.originalComponent); }
void GristoView::mouseEnter(const juce::MouseEvent& e) { showHelpFor(e.originalComponent); }
void GristoView::mouseDown(const juce::MouseEvent& e)
{
    showHelpFor(e.originalComponent); // touch has no hover
    if (e.mods.isPopupMenu())
        for (int i = 0; i < GristoProcessor::kNumScenes; ++i)
            if (e.originalComponent == &sceneButtons_[i])
                showSceneMenu(i);
}

void GristoView::setupKnob(Knob& k, const juce::String& paramId, const juce::String& text)
{
    k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
    k.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k.slider.setColour(juce::Slider::textBoxTextColourId, colours::text);
    addAndMakeVisible(k.slider);
    k.label.setText(text, juce::dontSendNotification);
    k.label.setJustificationType(juce::Justification::centred);
    k.label.setFont(juce::FontOptions(12.0f));
    k.label.setColour(juce::Label::textColourId, colours::dim);
    addAndMakeVisible(k.label);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor_.state, paramId, k.slider);
}

void GristoView::setupRandomKnob(Knob& k, const juce::String& paramId, const juce::String& text)
{
    setupKnob(k, paramId, text);
    k.slider.getProperties().set("random", true);
    k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
    k.label.setFont(juce::FontOptions(11.0f));
    k.label.setColour(juce::Label::textColourId, colours::random);
}

void GristoView::setupCellKnob(Knob& k, const juce::String& paramId, bool isRandom)
{
    setupKnob(k, paramId, {});
    k.label.setVisible(false);
    k.slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 14);
    if (isRandom)
        k.slider.getProperties().set("random", true);
}

void GristoView::setupToggle(Toggle& t, const juce::String& paramId, const juce::String& text)
{
    t.button.setButtonText(text);
    addAndMakeVisible(t.button);
    t.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor_.state, paramId, t.button);
}

void GristoView::setupChoice(Choice& c, const juce::String& paramId, const juce::String& text)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(processor_.state.getParameter(paramId)))
        c.box.addItemList(choice->choices, 1);
    addAndMakeVisible(c.box);
    c.label.setText(text, juce::dontSendNotification);
    c.label.setFont(juce::FontOptions(12.0f));
    c.label.setColour(juce::Label::textColourId, colours::dim);
    addAndMakeVisible(c.label);
    c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor_.state, paramId, c.box);
}

void GristoView::placeKnob(Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds(area.removeFromTop(16));
    k.slider.setBounds(area);
}

// ---------------------------------------------------------------------------------
// Modulation

void GristoView::makeTarget(juce::Component&, ModDropTarget& dropTarget, int target)
{
    dropTarget.onLaneDropped = [this, target](int lane) { assignLane(lane, target); };
    if (auto* knob = dynamic_cast<ModSlider*>(&dropTarget))
        knob->onContextMenu = [this, target, knob] { showModMenu(target, *knob); };
}

void GristoView::showModMenu(int target, juce::Component& knob)
{
    // Which slot of each source (if any) is routed to this knob.
    auto slotOf = [this, target](int source) {
        for (int s = 0; s < gristo::kModSlotsPerLane; ++s)
            if (static_cast<int>(processor_.state.getRawParameterValue(p::sourceTargetId(source, s))->load()) == target)
                return s;
        return -1;
    };
    juce::PopupMenu menu, macros;
    menu.addSectionHeader(mod::targetName(target));
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
        menu.addItem(lane + 1, "Modulate with lane " + juce::String(lane + 1), true, slotOf(lane) >= 0);
    bool anyMacro = false;
    for (int m = 0; m < gristo::kNumMacros; ++m)
    {
        const bool routed = slotOf(gristo::kNumModLanes + m) >= 0;
        anyMacro = anyMacro || routed;
        macros.addItem(gristo::kNumModLanes + m + 1, "Macro " + juce::String(m + 1), true, routed);
    }
    menu.addSubMenu("Control with macro", macros, true, nullptr, anyMacro);
    juce::Component::SafePointer<GristoView> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&knob), [safe, slotOf, target](int result) {
        if (safe == nullptr || result <= 0)
            return;
        const int source = result - 1, slot = slotOf(source);
        if (slot < 0)
            safe->assignSource(source, target);
        else
        {
            safe->setParameter(p::sourceTargetId(source, slot), static_cast<float>(mod::kNone));
            safe->setParameter(p::sourceAmountId(source, slot), 0.0f);
        }
    });
}

void GristoView::setParameter(const juce::String& id, float value)
{
    if (auto* param = processor_.state.getParameter(id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(value));
        param->endChangeGesture();
    }
}

void GristoView::assignSource(int source, int target)
{
    auto targetOf = [&](int slot) {
        return static_cast<int>(processor_.state.getRawParameterValue(p::sourceTargetId(source, slot))->load());
    };
    int slot = -1;
    for (int s = 0; s < gristo::kModSlotsPerLane && slot < 0; ++s)
        if (targetOf(s) == target)
            slot = s;
    for (int s = 0; s < gristo::kModSlotsPerLane && slot < 0; ++s)
        if (targetOf(s) == mod::kNone)
            slot = s;
    if (slot < 0)
        slot = gristo::kModSlotsPerLane - 1; // all slots taken: reuse the last one

    const bool alreadyRouted = targetOf(slot) == target;
    const float amount = processor_.state.getRawParameterValue(p::sourceAmountId(source, slot))->load();
    setParameter(p::sourceTargetId(source, slot), static_cast<float>(target));
    if (!alreadyRouted || std::abs(amount) < 1.0e-6f)
    {
        // Start with an audible amount that points into the knob's free travel: a knob
        // above halfway gets -50 %, below halfway +50 %. Velocity starts with dips.
        float start = 0.5f;
        if (auto* s = targetSliders_[static_cast<size_t>(target)])
            start = s->valueToProportionOfLength(s->getValue()) > 0.5 ? -0.5f : 0.5f;
        else if (target == mod::kMapX || target == mod::kMapY)
            start = processor_.state.getRawParameterValue(target == mod::kMapX ? p::mapX : p::mapY)->load() > 0.5f ? -0.5f : 0.5f;
        else if (target >= mod::kVelocity && target < mod::kVelocity + gristo::kNumVoices)
            start = -0.5f;
        else if (int arp = 0, which = 0; mod::decodeArpTarget(target, arp, which) && which == mod::kArpVelocity)
            start = -0.5f;
        else if (mod::decodeArpTarget(target, arp, which) && which == mod::kArpTranspose)
            start = 7.0f / 12.0f; // up a fifth
        setParameter(p::sourceAmountId(source, slot), start);
    }
    if (source < gristo::kNumModLanes)
    {
        laneTabs_[source].setToggleState(true, juce::dontSendNotification);
        selectLane(source);
    }
    else
        selectSource(source);
}

void GristoView::assignLane(int lane, int target) { assignSource(lane, target); }


void GristoView::selectLane(int lane)
{
    selectedLane_ = lane;
    laneEditor_.setLane(lane);
    laneHandle_.setLane(lane);
    laneLengthAttachment_.reset();
    laneLengthAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.state, p::laneId(p::modBars, lane), laneLength_);
    laneWaveAttachment_.reset();
    laneWaveAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.state, p::laneId(p::modWave, lane), laneWave_);
    laneRateAttachment_.reset();
    laneRateAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.state, p::laneId(p::modRate, lane), laneRate_);
    selectSource(lane);
}

juce::Colour GristoView::sourceColour(int source) { return source < gristo::kNumModLanes ? laneColours[source] : colours::accent; }

void GristoView::selectSource(int source)
{
    // The routing rows show one source: a drawn lane or a macro.
    selectedSource_ = source;
    sourceBox_.setSelectedId(source + 1, juce::dontSendNotification);
    for (int s = 0; s < gristo::kModSlotsPerLane; ++s)
    {
        auto& slot = slots_[s];
        slot.targetAttachment.reset();
        slot.amountAttachment.reset();
        slot.targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor_.state, p::sourceTargetId(source, s), slot.target);
        slot.amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor_.state, p::sourceAmountId(source, s), slot.amount);
        slot.amount.setColour(juce::Slider::trackColourId, sourceColour(source));
    }
    repaint();
}

void GristoView::updateModulationDisplay(const gristo::EngineParams& params)
{
    for (auto* s : targetSliders_)
        if (s != nullptr)
        {
            s->rings.clear();
            s->live = -1.0f;
        }
    for (auto& vr : velocityRanges_)
        vr->rings.clear();
    const int shownArp = isArpPage() ? page_ - 1 : -1;
    if (arpVelocity_ != nullptr)
        arpVelocity_->rings.clear();
    arpView_.transposeLanes.clear();

    bool mapModulated = false;
    for (int lane = 0; lane < gristo::kNumModSources; ++lane)
        for (const auto& slot : params.modulation.lanes[static_cast<size_t>(lane)].slots)
        {
            if (slot.target <= mod::kNone || slot.target >= mod::kNumTargets || std::abs(slot.amount) < 1.0e-6f)
                continue;
            const auto colour = sourceColour(lane);
            if (auto* s = targetSliders_[static_cast<size_t>(slot.target)])
            {
                // Every target's full modulation range equals its knob range, so the
                // amount maps directly onto the knob's travel.
                const float base = static_cast<float>(s->valueToProportionOfLength(s->getValue()));
                s->rings.push_back({ colour, base, juce::jlimit(0.0f, 1.0f, base + slot.amount) });
            }
            else if (slot.target >= mod::kVelocity && slot.target < mod::kVelocity + gristo::kNumVoices)
                velocityRanges_[static_cast<size_t>(slot.target - mod::kVelocity)]->rings.push_back({ colour, 0.0f, slot.amount });
            else if (slot.target == mod::kMapX || slot.target == mod::kMapY)
                mapModulated = true;
            else if (shownArp >= 0 && slot.target == mod::arpTarget(shownArp, mod::kArpVelocity) && arpVelocity_ != nullptr)
                arpVelocity_->rings.push_back({ colour, 0.0f, slot.amount });
            else if (shownArp >= 0 && slot.target == mod::arpTarget(shownArp, mod::kArpTranspose))
                arpView_.transposeLanes.push_back(colour);
        }

    // Live values while playing.
    const bool playing = processor_.displayPlaying.load();
    const auto step = static_cast<std::int64_t>(std::floor(processor_.displayPpq.load() * gristo::kStepsPerBeat));
    const gristo::EngineParams ep = previewEngine_.effectiveParams(step);
    if (playing)
    {
        auto show = [&](int target, double value) {
            if (auto* s = targetSliders_[static_cast<size_t>(target)]; s != nullptr && !s->rings.empty())
                s->live = static_cast<float>(s->valueToProportionOfLength(value));
        };
        for (int v = 0; v < gristo::kNumVoices; ++v)
        {
            show(mod::kFill + v, ep.pattern.fill[static_cast<size_t>(v)]);
            show(mod::kProbability + v, ep.pattern.probability[static_cast<size_t>(v)]);
        }
        show(mod::kChaos, ep.pattern.chaos);
        show(mod::kShuffle, ep.groove.shuffle);
        show(mod::kFlamMs, ep.groove.feelMs);
        show(mod::kHumanize, ep.humanize.amount);
        for (int a = 0; a < gristo::kNumArps; ++a)
            show(mod::arpCutoffTarget(a), juce::jlimit(0.0, 1.0, synthRows_[mod::synthOfArp(a)].cutoff.slider.getValue()
                                                                     + ep.arps[static_cast<size_t>(a)].cutoffOffset));
        for (const auto& [knob, target] : soundTargets_)
        {
            const auto& s = knob->slider;
            show(target, juce::jlimit(s.getMinimum(), s.getMaximum(),
                                      s.getValue() + previewEngine_.modulationAt(target, step) * mod::targetRange(target)));
        }
        if (shownArp >= 0)
        {
            const auto& ap = ep.arps[static_cast<size_t>(shownArp)];
            show(mod::arpTarget(shownArp, mod::kArpDensity), ap.density);
            show(mod::arpProbTarget(shownArp), ap.probability);
            show(mod::arpTarget(shownArp, mod::kArpAccent), ap.accent);
            show(mod::arpTarget(shownArp, mod::kArpSlide), ap.slide);
            show(mod::arpTarget(shownArp, mod::kArpGate), ap.gateSteps);
            show(mod::arpTarget(shownArp, mod::kArpOctaves), ap.octaves);
            show(mod::arpTarget(shownArp, mod::kArpLength), ap.length);
        }
    }
    if (shownArp >= 0)
    {
        const auto& ap = ep.arps[static_cast<size_t>(shownArp)];
        if (arpVelocity_ != nullptr)
        {
            arpVelocity_->showLive = playing;
            arpVelocity_->liveOffset = ap.velocityOffset;
            arpVelocity_->repaint();
        }
        arpView_.showLive = playing;
        arpView_.liveTranspose = ap.transpose;
    }
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        auto& vr = *velocityRanges_[v];
        vr.showLive = playing;
        vr.liveOffset = ep.dynamics.velocityOffset[static_cast<size_t>(v)];
        vr.repaint();
    }
    for (auto* s : targetSliders_)
        if (s != nullptr)
            s->repaint();

    // Map: live position = modulated + random position of the current bar.
    std::optional<juce::Point<float>> live;
    if (playing && (params.pattern.mapRandom > 0.0f || mapModulated))
    {
        const auto [lx, ly] = gristo::PatternGenerator::mapPositionAt(ep.pattern, step);
        live = juce::Point<float>(lx, ly);
    }
    mapPad_.setRandomDisplay(params.pattern.mapRandom * 0.5f, live);
}

void GristoView::setScaleLabel(float scale)
{
    sizeButton_.setButtonText("Size " + juce::String(juce::roundToInt(scale * 100.0f)) + "%");
}

// ---------------------------------------------------------------------------------
// Presets

void GristoView::setupPresetBar()
{
    for (auto* b : { &presetPrev_, &presetName_, &presetNext_, &presetSave_ })
        addAndMakeVisible(*b);
    presetPrev_.onClick = [this] { processor_.presets.step(-1); presetLoaded(); };
    presetNext_.onClick = [this] { processor_.presets.step(1); presetLoaded(); };
    presetName_.onClick = [this] { showPresetMenu(); };
    presetSave_.onClick = [this] { savePreset(false); };
    if (kIsMobile)
    {
        addAndMakeVisible(presetShare_);
        presetShare_.onClick = [this] { sharePreset(); };
        setHelp(presetShare_, "Share preset", "Send the current sound as a .gristo file: Quick Share, email, Drive... Import it on the PC.");
    }
    presetName_.setButtonText(processor_.presets.currentName());
    setHelp(presetName_, "Preset", "Click for factory and your presets, Save As, Import / Export and Init. * = changed since loading.");
    setHelp(presetPrev_, "Previous preset", "Load the previous preset (factory first, then yours).");
    setHelp(presetNext_, "Next preset", "Load the next preset (factory first, then yours).");
    setHelp(presetSave_, "Save preset", "Save over your current preset, or name a new one. MIDI notes, channels and Shift aren't stored.");
}

void GristoView::presetLoaded()
{
    auto& presets = processor_.presets;
    presetName_.setButtonText(presets.currentName() + (presets.isDirty() ? " *" : ""));
    laneEditor_.repaint();
    repaint();
}

void GristoView::showPresetMenu()
{
    auto& presets = processor_.presets;
    juce::PopupMenu menu;
    menu.addSectionHeader("Factory");
    const auto& factory = PresetManager::factoryPresets();
    for (int i = 0; i < static_cast<int>(factory.size()); ++i)
        menu.addItem(1000 + i, factory[static_cast<size_t>(i)].name, true,
                     presets.currentFile() == juce::File() && presets.currentName() == factory[static_cast<size_t>(i)].name);
    const auto files = presets.userPresets();
    menu.addSectionHeader("My presets");
    if (files.isEmpty())
        menu.addItem(-1, "(none yet: Save As to make one)", false);
    for (int i = 0; i < files.size(); ++i)
        menu.addItem(2000 + i, files[i].getFileNameWithoutExtension(), true, files[i] == presets.currentFile());
    menu.addSeparator();
    menu.addItem(1, "Save");
    menu.addItem(2, "Save As...");
    menu.addItem(3, "Import...");
    menu.addItem(4, "Export...");
    menu.addItem(5, "Init (start from scratch)");
    if (kIsMobile)
        menu.addItem(7, "Share...");
    else
        menu.addItem(6, "Show preset folder");

    juce::Component::SafePointer<GristoView> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetName_), [safe, files](int result) {
        if (safe == nullptr || result == 0)
            return;
        auto& pm = safe->processor_.presets;
        if (result >= 2000)
        {
            if (!pm.loadFile(files[result - 2000]))
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Preset",
                                                       "That preset file couldn't be read (damaged, or not a Gristo preset).", "OK",
                                                       safe.getComponent());
        }
        else if (result >= 1000)
            pm.loadFactory(result - 1000);
        else if (result == 1)
            safe->savePreset(false);
        else if (result == 2)
            safe->savePreset(true);
        else if (result == 3)
            safe->importPreset();
        else if (result == 4)
            safe->exportPreset();
        else if (result == 5)
            pm.loadInit();
        else if (result == 6)
            PresetManager::userFolder().revealToUser();
        else if (result == 7)
            safe->sharePreset();
        safe->presetLoaded();
    });
}

void GristoView::askPresetName(const juce::String& suggestion, std::function<void(juce::String)> done)
{
    auto* window = new juce::AlertWindow("Save preset", "Name for the preset:", juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor("name", suggestion);
    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true, juce::ModalCallbackFunction::create([window, done](int result) {
                                if (result == 1)
                                    done(window->getTextEditorContents("name").trim());
                            }),
                            true);
}

void GristoView::confirm(const juce::String& title, const juce::String& message, const juce::String& yes, std::function<void()> onYes)
{
    auto* window = new juce::AlertWindow(title, message, juce::MessageBoxIconType::QuestionIcon, this);
    window->addButton(yes, 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true, juce::ModalCallbackFunction::create([onYes](int result) {
                                if (result == 1)
                                    onYes();
                            }),
                            true);
}

void GristoView::savePreset(bool askForName)
{
    auto& presets = processor_.presets;
    const auto current = presets.currentFile();
    if (!askForName && current.existsAsFile())
    {
        presets.saveFile(current, presets.currentName());
        presetLoaded();
        return;
    }
    juce::Component::SafePointer<GristoView> safe(this);
    const auto suggestion = presets.currentName() == "Init" ? juce::String("My groove") : presets.currentName();
    askPresetName(suggestion, [safe](juce::String name) {
        if (safe == nullptr || name.isEmpty())
            return;
        const auto file = PresetManager::userFolder().getChildFile(juce::File::createLegalFileName(name) + PresetManager::kExtension);
        auto write = [safe, file, name] {
            if (safe == nullptr)
                return;
            safe->processor_.presets.saveFile(file, name);
            safe->presetLoaded();
        };
        if (file.existsAsFile() && file != safe->processor_.presets.currentFile())
            safe->confirm("Replace preset?", "\"" + name + "\" already exists.", "Replace", write);
        else
            write();
    });
}

void GristoView::importPreset()
{
    chooser_ = std::make_unique<juce::FileChooser>("Import a Gristo preset", juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                                                   kIsMobile ? juce::String() : juce::String("*") + PresetManager::kExtension);
    juce::Component::SafePointer<GristoView> safe(this);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe](const juce::FileChooser& chooser) {
                              if (safe == nullptr || chooser.getURLResults().isEmpty())
                                  return;
                              auto& presets = safe->processor_.presets;
                              const auto copy = presets.importUrl(chooser.getURLResult());
                              if (!copy.existsAsFile() || !presets.loadFile(copy))
                                  juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Import",
                                                                         "That file isn't a Gristo preset.", "OK", safe.getComponent());
                              safe->presetLoaded();
                          });
}

void GristoView::exportPreset()
{
    const auto name = processor_.presets.currentName();
    const auto start = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                           .getChildFile(juce::File::createLegalFileName(name) + PresetManager::kExtension);
    chooser_ = std::make_unique<juce::FileChooser>("Export preset", start, juce::String("*") + PresetManager::kExtension);
    juce::Component::SafePointer<GristoView> safe(this);
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe](const juce::FileChooser& chooser) {
                              if (safe == nullptr || chooser.getURLResults().isEmpty())
                                  return;
                              auto url = chooser.getURLResult();
                              if (url.isLocalFile() && !url.getLocalFile().hasFileExtension(PresetManager::kExtension))
                                  url = juce::URL(url.getLocalFile().withFileExtension(PresetManager::kExtension));
                              // Export writes a copy; the preset you're working on stays current.
                              if (!safe->processor_.presets.exportUrl(url))
                                  juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Export",
                                                                         "Couldn't write the preset there.", "OK", safe.getComponent());
                          });
}

void GristoView::sharePreset()
{
    const auto file = processor_.presets.writeTemporary();
    if (!file.existsAsFile())
        return;
    juce::Component::SafePointer<GristoView> safe(this);
    shareBox_ = juce::ContentSharer::shareFilesScoped({ juce::URL(file) }, [safe](bool success, const juce::String& error) {
        if (!success && error.isNotEmpty() && safe != nullptr)
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Share",
                                                   error + "\nTry Export... in the preset menu instead.", "OK", safe.getComponent());
    }, this);
}

void GristoView::showSizeMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < static_cast<int>(std::size(kScales)); ++i)
        menu.addItem(i + 1, juce::String(juce::roundToInt(kScales[i] * 100.0f)) + "%");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&sizeButton_),
                       [safe = juce::Component::SafePointer<GristoView>(this)](int result) {
                           if (safe != nullptr && result > 0 && safe->onScaleChosen)
                               safe->onScaleChosen(kScales[result - 1]);
                       });
}

// ---------------------------------------------------------------------------------
// Timer, paint, layout

void GristoView::timerCallback()
{
    const gristo::EngineParams params = p::readEngineParams(processor_.state, processor_.modPoints);
    previewEngine_.setParams(params);
    mapPad_.refreshHeat(params);
    updateModulationDisplay(params);
    for (int part = 0; part < gristo::kNumParts; ++part)
    {
        float& a = activity_[static_cast<size_t>(part)];
        const float hit = processor_.displayActivity[static_cast<size_t>(part)].exchange(0.0f, std::memory_order_relaxed);
        const float next = std::max(a * 0.8f, hit > 0.0f ? 0.4f + 0.6f * hit : 0.0f);
        if (std::abs(next - a) > 0.01f)
            repaint(84, partRowY(part) + 4, 10, 10);
        a = next < 0.02f ? 0.0f : next;
    }
    {
        const bool lfo = params.modulation.lanes[static_cast<size_t>(selectedLane_)].wave != gristo::LaneWave::Drawn;
        laneRate_.setVisible(lfo);
        shapesButton_.setVisible(!lfo);
    }
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        const auto name = processor_.drumSampleName(v);
        const auto text = name.isEmpty() ? juce::String("Kit") : juce::File::createLegalFileName(name).upToLastOccurrenceOf(".", false, false);
        if (sampleButtons_[v].getButtonText() != text)
            sampleButtons_[v].setButtonText(text);
    }
    undoButton_.setEnabled(processor_.canUndo());
    redoButton_.setEnabled(processor_.canRedo());
    {
        const int current = processor_.currentScene.load(), pending = processor_.pendingScene.load();
        const bool blink = (juce::Time::getMillisecondCounter() / 250) % 2 == 0;
        for (int i = 0; i < GristoProcessor::kNumScenes; ++i)
        {
            const auto colour = i == pending ? (blink ? colours::accent : colours::cool)
                              : i == current ? colours::accent
                              : processor_.sceneUsed(i) ? colours::cool : colours::background;
            if (sceneButtons_[i].findColour(juce::TextButton::buttonColourId) != colour)
            {
                sceneButtons_[i].setColour(juce::TextButton::buttonColourId, colour);
                sceneButtons_[i].setColour(juce::TextButton::textColourOffId, i == current || (i == pending && blink) ? colours::background : colours::text);
            }
        }
    }
    mapPad_.repaint();
    nodeLabel_.setText("Near: " + mapPad_.nearestNodeName(), juce::dontSendNotification);
    timelineView_.update(processor_.displayPpq.load(), processor_.displayPlaying.load(),
                         params.modulation.lanes[static_cast<size_t>(selectedLane_)].bars, selectedPart_);
    laneEditor_.repaint();
    if (++presetRefresh_ >= 8) // a few times a second is plenty
    {
        presetRefresh_ = 0;
        auto& presets = processor_.presets;
        presetName_.setButtonText(presets.currentName() + (presets.isDirty() ? " *" : ""));
    }
    if (page_ == kChordsPage)
        updateChordPage(params);
    if (isArpPage())
    {
        const auto& ap = params.arps[static_cast<size_t>(page_ - 1)];
        arpView_.update(ap);
        updateArpOutput(params, page_ - 1);
        const bool gen = ap.mode == gristo::ArpMode::Generate;
        for (juce::Component* c : std::initializer_list<juce::Component*> { &arpScale_.box, &arpScale_.label, arpRoot_.get(), &arpRootLabel_,
                                                                          &arpLength_.slider, &arpLength_.label, &arpDensity_.slider,
                                                                          &arpDensity_.label, &arpAccent_.slider, &arpAccent_.label,
                                                                          &arpSlide_.slider, &arpSlide_.label, &arpMutate_.slider,
                                                                          &arpMutate_.label, &arpNew_ })
            c->setAlpha(gen ? 1.0f : 0.35f);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &arpOrder_.box, &arpOrder_.label })
            c->setAlpha(gen ? 0.35f : 1.0f);
    }
}

// ---------------------------------------------------------------------------------
// Bottom panel pages and the arp page

std::vector<juce::Component*> GristoView::modulationPageComponents()
{
    std::vector<juce::Component*> list { &laneHandle_, &laneEditor_, &laneLength_, &shapesButton_ };
    for (auto& tab : laneTabs_)
        list.push_back(&tab);
    for (auto& slot : slots_)
    {
        list.push_back(&slot.target);
        list.push_back(&slot.amount);
    }
    return list;
}

void GristoView::updateArpOutput(const gristo::EngineParams& params, int arp)
{
    // The notes each arp can send right now (its pool, shifted), to show where they
    // land and to warn when they collide with the drums or the other arps.
    auto outRange = [this, &params](int a, int& lo, int& hi) {
        const auto& ap = params.arps[static_cast<size_t>(a)];
        int held[gristo::kMaxHeldNotes];
        const int count = std::clamp(processor_.displayArpInputCount[static_cast<size_t>(a)].load(), 0, gristo::kMaxHeldNotes);
        for (int i = 0; i < count; ++i)
            held[i] = processor_.displayArpInput[static_cast<size_t>(a)][static_cast<size_t>(i)].load();
        const gristo::ArpPool pool = gristo::Arp::pool(ap, held, count);
        lo = 128;
        hi = -1;
        for (int i = 0; i < pool.size; ++i)
        {
            const int n = gristo::Arp::outputNote(pool.notes[static_cast<size_t>(i)], ap.shift);
            lo = std::min(lo, n);
            hi = std::max(hi, n);
        }
        return pool.size > 0;
    };
    auto name = [](int note) { return juce::MidiMessage::getMidiNoteName(note, true, true, 3); };

    int lo = 0, hi = 0;
    if (!outRange(arp, lo, hi))
    {
        arpOutLabel_.setColour(juce::Label::textColourId, colours::dim);
        arpOutLabel_.setText("MIDI out: no notes yet (hold notes, or use Generate)", juce::dontSendNotification);
        return;
    }
    const int shift = params.arps[static_cast<size_t>(arp)].shift;
    juce::String text = "MIDI out " + name(lo) + " - " + name(hi) + ".  In Live: chain key zone " + name(lo) + " - " + name(hi);
    if (shift != 0)
        text << ", Pitch " << (shift > 0 ? "-" : "+") << std::abs(shift) << " before the synth";

    juce::StringArray clashes;
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        const int n = static_cast<int>(processor_.state.getRawParameterValue(p::voiceId(p::note, v))->load());
        if (n >= lo && n <= hi)
            clashes.addIfNotAlreadyThere("drums");
    }
    for (int other = 0; other < gristo::kNumArps; ++other)
    {
        int otherLo = 0, otherHi = 0;
        if (other != arp && params.arps[static_cast<size_t>(other)].enabled && outRange(other, otherLo, otherHi) && otherLo <= hi
            && otherHi >= lo)
            clashes.add("Arp " + juce::String(other + 1));
    }
    if (params.chords.enabled && params.chords.play != gristo::ChordPlay::Off)
    {
        bool chordClash = false;
        for (const auto& slot : params.chords.slots)
            if (slot.used)
            {
                const auto chord = gristo::Chords::build(params.chords, slot);
                for (int i = 0; i < chord.count; ++i)
                {
                    const int n = gristo::Arp::outputNote(chord.notes[static_cast<size_t>(i)], params.chords.shift);
                    chordClash = chordClash || (n >= lo && n <= hi);
                }
            }
        if (chordClash)
            clashes.add("the chords");
    }
    if (!clashes.isEmpty())
        text << ".  Overlaps " << clashes.joinIntoString(" and ") << ": change Shift";
    arpOutLabel_.setColour(juce::Label::textColourId, clashes.isEmpty() ? colours::dim : colours::hot);
    arpOutLabel_.setText(text, juce::dontSendNotification);
}

void GristoView::bindArpTargets(int arp)
{
    // The arp page has one set of knobs; route drops and rings to the arp shown.
    for (int a = 0; a < gristo::kNumArps; ++a)
    {
        for (int w = 0; w < mod::kArpTargetsPerArp; ++w)
            targetSliders_[static_cast<size_t>(mod::arpTarget(a, w))] = nullptr;
        targetSliders_[static_cast<size_t>(mod::arpProbTarget(a))] = nullptr;
    }
    makeTarget(arpProb_.slider, arpProb_.slider, mod::arpProbTarget(arp));
    targetSliders_[static_cast<size_t>(mod::arpProbTarget(arp))] = &arpProb_.slider;
    const std::pair<Knob*, int> knobs[] = { { &arpDensity_, mod::kArpDensity }, { &arpAccent_, mod::kArpAccent },
                                            { &arpSlide_, mod::kArpSlide },     { &arpGate_, mod::kArpGate },
                                            { &arpOctaves_, mod::kArpOctaves }, { &arpLength_, mod::kArpLength } };
    for (const auto& [knob, which] : knobs)
    {
        makeTarget(knob->slider, knob->slider, mod::arpTarget(arp, which));
        targetSliders_[static_cast<size_t>(mod::arpTarget(arp, which))] = &knob->slider;
    }
    makeTarget(*arpVelocity_, *arpVelocity_, mod::arpTarget(arp, mod::kArpVelocity));
    makeTarget(arpView_, arpView_, mod::arpTarget(arp, mod::kArpTranspose));
}

void GristoView::setupSmallKnob(Knob& k, const juce::String& paramId, const juce::String& text)
{
    setupKnob(k, paramId, text);
    k.slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    k.slider.setPopupDisplayEnabled(true, true, this);
    k.label.setFont(juce::FontOptions(10.5f));
}

void GristoView::placeSmallKnob(Knob& k, int x, int y)
{
    k.slider.setBounds(x, y, 44, 32);
    k.label.setBounds(x - 6, y + 31, 56, 13);
}

void GristoView::setupSoundPage()
{
    setupChoice(soundKit_, p::kitType, "Kit");
    soundKit_.label.setVisible(false);
    setupSmallKnob(kitTune_, p::kitTune, "Tune");
    setupSmallKnob(kitDecay_, p::kitDecay, "Decay");
    setupSmallKnob(kitTone_, p::kitTone, "Tone");
    setupSmallKnob(kitDrive_, p::kitDrive, "Drive");
    setupSmallKnob(kitReverb_, p::kitReverb, "Reverb");
    setupSmallKnob(kitDelay_, p::kitDelay, "Delay");
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        setupSmallKnob(drumLevels_[v], p::voiceId(p::drumLevel, v), kTriggerNames[v]);
        drumLevels_[v].label.setColour(juce::Label::textColourId, voiceColours[v]);
        setHelp(drumLevels_[v], juce::String(gristo::voiceName(v)) + " level", "Volume of this drum in the built-in kit.");
    }
    setHelp(soundKit_.box, "Drum kit", "Twelve synthesized kits, from 808 and Trap to House 909, Jungle, Amapiano and Dancehall. The knobs shape the whole kit.");
    setHelp(kitTune_, "Kit Tune", "Tunes every drum up or down (semitones). Low tune = deeper 808s.");
    setHelp(kitDecay_, "Kit Decay", "Shorter or longer drums. 50% = as designed; turn up for long trap 808 booms.");
    setHelp(kitTone_, "Kit Tone", "Below 50% darker, above 50% brighter and airier.");
    setHelp(kitDrive_, "Kit Drive", "Saturates the whole kit for weight and grit.");
    setHelp(kitReverb_, "Kit Reverb", "How much of the kit goes to the reverb.");
    setHelp(kitDelay_, "Kit Delay", "How much of the kit goes to the delay.");

    for (int a = 0; a < gristo::sound::kNumSynths; ++a)
    {
        auto& row = synthRows_[a];
        // Sounds grouped by category; item ids stay index + 1 for the attachment.
        for (const char* category : { "Bass", "Keys", "Pads", "Leads", "Brass" })
        {
            row.sound.addSectionHeading(category);
            for (int i = 0; i < static_cast<int>(gristo::sound::SynthSound::Count); ++i)
            {
                const auto sound = static_cast<gristo::sound::SynthSound>(i);
                if (juce::String(gristo::sound::synthCategoryName(sound)) == category)
                    row.sound.addItem(gristo::sound::synthSoundName(sound), i + 1);
            }
        }
        addAndMakeVisible(row.sound);
        row.soundAttachment = std::make_unique<ComboIdAttachment>(*processor_.state.getParameter(p::laneId(p::synthSound, a)), row.sound);
        const int arp = mod::arpOfSynth(a);
        const juce::String name = arp < 0 ? juce::String("Chords ") : "Arp " + juce::String(arp + 1) + " ";
        setupSmallKnob(row.cutoff, p::laneId(p::synthCutoff, a), "Cutoff");
        setupSmallKnob(row.resonance, p::laneId(p::synthResonance, a), "Reso");
        setupSmallKnob(row.env, p::laneId(p::synthEnv, a), "Env");
        setupSmallKnob(row.attack, p::laneId(p::synthAttack, a), "Attack");
        setupSmallKnob(row.release, p::laneId(p::synthRelease, a), "Release");
        setupSmallKnob(row.glide, p::laneId(p::synthGlide, a), "Glide");
        setupSmallKnob(row.level, p::laneId(p::synthLevel, a), "Level");
        setupSmallKnob(row.reverb, p::laneId(p::synthReverb, a), "Reverb");
        setupSmallKnob(row.delay, p::laneId(p::synthDelay, a), "Delay");
        setHelp(row.sound, name + "sound", "The built-in synth this arp plays: basses, keys, pads or leads. Pads and keys ring out between notes.");
        setHelp(row.cutoff, name + "Cutoff", "Filter brightness. 50% = as designed. Drag a lane here for filter sweeps.");
        setHelp(row.resonance, name + "Resonance", "Filter emphasis. Turn up on Acid Bass for squelch.");
        setHelp(row.env, name + "Env Amount", "How far the filter opens on each note. Accents (loud hits) open it more.");
        setHelp(row.attack, name + "Attack", "Fade-in time. 50% = as designed; up for swelling pads.");
        setHelp(row.release, name + "Release", "How long notes ring after they end. 50% = as designed.");
        setHelp(row.glide, name + "Glide", "Pitch glide time on slides (overlapping notes from the generator's Slide).");
        setHelp(row.level, name + "Level", "Volume of this arp's synth.");
        setHelp(row.reverb, name + "Reverb", "How much of this synth goes to the reverb.");
        setHelp(row.delay, name + "Delay", "How much of this synth goes to the delay.");
        setupSmallKnob(row.duck, p::laneId(p::synthDuck, a), "Duck");
        setHelp(row.duck, name + "Duck", "Sidechain pumping: every kick pushes this sound down and it swells back.");
        if (arp >= 0)
        {
            makeTarget(row.cutoff.slider, row.cutoff.slider, mod::arpCutoffTarget(arp));
            targetSliders_[static_cast<size_t>(mod::arpCutoffTarget(arp))] = &row.cutoff.slider;
        }
    }

    setupSmallKnob(fxSize_, p::fxSize, "Size");
    setupSmallKnob(fxFeedback_, p::fxFeedback, "Feedback");
    setupChoice(fxTime_, p::fxTime, "Delay");
    fxTime_.label.setFont(juce::FontOptions(10.5f));
    setHelp(fxSize_, "Reverb Size", "Small room to big hall.");
    setHelp(fxFeedback_, "Delay Feedback", "How many echoes. The delay ping-pongs left and right and follows the tempo.");
    setHelp(fxTime_.box, "Delay Time", "Echo spacing in note values, synced to the tempo.");

    // Every sound knob can take a lane.
    soundTargets_ = { { &kitTune_, mod::kitTarget(mod::kKitTune) },     { &kitDecay_, mod::kitTarget(mod::kKitDecay) },
                      { &kitTone_, mod::kitTarget(mod::kKitTone) },     { &kitDrive_, mod::kitTarget(mod::kKitDrive) },
                      { &kitReverb_, mod::kitTarget(mod::kKitReverb) }, { &kitDelay_, mod::kitTarget(mod::kKitDelay) },
                      { &fxSize_, mod::kFxSizeTarget },                 { &fxFeedback_, mod::kFxFeedbackTarget },
                      { &synthRows_[gristo::sound::kChordSynth].cutoff, mod::kChordCutoffTarget } };
    for (int a = 0; a < gristo::sound::kNumSynths; ++a)
    {
        auto& row = synthRows_[a];
        const std::pair<Knob*, int> knobs[] = { { &row.resonance, mod::kSynthReso }, { &row.env, mod::kSynthEnv },
                                                { &row.attack, mod::kSynthAttack },  { &row.release, mod::kSynthRelease },
                                                { &row.level, mod::kSynthLevel },    { &row.reverb, mod::kSynthReverb },
                                                { &row.delay, mod::kSynthDelay } };
        for (const auto& [knob, which] : knobs)
            soundTargets_.push_back({ knob, mod::synthTarget(a, which) });
    }
    for (const auto& [knob, target] : soundTargets_)
    {
        makeTarget(knob->slider, knob->slider, target);
        targetSliders_[static_cast<size_t>(target)] = &knob->slider;
    }

    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
    {
        soundLaneHandles_[lane].setLane(lane);
        soundLaneHandles_[lane].setCompact(true);
        addAndMakeVisible(soundLaneHandles_[lane]);
        setHelp(soundLaneHandles_[lane], "Lane " + juce::String(lane + 1),
                "Drag onto any knob here (or tap it, then tap the knob): filter sweeps, drive, sends, reverb size... Draw the curve on the Modulation tab.");
    }
}

// ---------------------------------------------------------------------------------
// Chords page

void GristoView::setupChordPage()
{
    setupToggle(chordOn_, p::chordOn, "On");
    setupChoice(chordKey_, p::chordKey, "Key");
    setupChoice(chordScale_, p::chordScale, "Scale");
    setupChoice(chordClock_, p::chordClock, "Clock");
    setupChoice(chordClockVoice_, p::chordClockVoice, "Voice");
    setupChoice(chordReset_, p::chordReset, "Reset");
    setupChoice(chordPlay_, p::chordPlay, "Play");
    setupChoice(chordOctave_, p::chordOctave, "Octave");
    for (Choice* c : { &chordKey_, &chordScale_, &chordClock_, &chordClockVoice_, &chordReset_, &chordPlay_, &chordOctave_ })
        c->label.setVisible(false);
    setupChoice(chordVoicing_, p::chordVoicing, "Voicing");
    setupChoice(chordStrumDir_, p::chordStrumDir, "Strum");
    setHelp(chordVoicing_.box, "Voicing", "As written: root position or the inversion you picked. Smooth: every chord in one octave, so notes move as little as possible. + bass: the root underneath.");
    setHelp(chordStrumDir_.box, "Strum direction", "Which way the Strum knob rolls the chord: low to high, high to low, or back and forth.");
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        setupToggle(chordStabs_[v], p::voiceId(p::chordStab, v), kTriggerNames[v]);
        setHelp(chordStabs_[v].button, juce::String("Stab on ") + gristo::voiceName(v), "Play mode Stabs: the chord plays on hits of this voice.");
    }
    auto& st = processor_.state;
    chordHits_ = std::make_unique<NumberSelector>(*st.getParameter(p::chordHits), "every ", 1, 16, " hits");
    chordChannel_ = std::make_unique<NumberSelector>(*st.getParameter(p::chordChannel), "Ch ", 1, 16);
    chordShift_ = std::make_unique<NumberSelector>(*st.getParameter(p::chordShift), "", -60, 60, " st", 12, true);
    chordVelocity_ = std::make_unique<VelocityRange>(*st.getParameter(p::chordVelMin), *st.getParameter(p::chordVelMax), colours::random);
    for (juce::Component* c : std::initializer_list<juce::Component*> { chordHits_.get(), chordChannel_.get(), chordShift_.get(), chordVelocity_.get(),
                                                                        &chordLibrary_, &chordNew_, &chordClear_, &chordSlots_, &chordPads_, &chordOutLabel_ })
        addAndMakeVisible(c);
    setupSmallKnob(chordMutate_, p::chordMutate, "Mutate");
    setupSmallKnob(chordGate_, p::chordGate, "Gate");
    setupSmallKnob(chordStrum_, p::chordStrum, "Strum");
    setHelp(chordStrum_, "Strum", "Time between the notes of a chord, in ms. 0 = all together.");
    chordOutLabel_.setFont(juce::FontOptions(11.0f));
    chordOutLabel_.setColour(juce::Label::textColourId, colours::dim);

    for (int t = 0; t < static_cast<int>(gristo::ChordType::Count); ++t)
    {
        chordTypes_[t].setButtonText(gristo::chordTypeName(static_cast<gristo::ChordType>(t)));
        chordTypes_[t].onClick = [this, t] { editSelectedChord([t](gristo::ChordSlot& s) { s.type = static_cast<gristo::ChordType>(t); }); };
        addAndMakeVisible(chordTypes_[t]);
        setHelp(chordTypes_[t], "Chord type", "Triad, 7th, 9th, sus, 6, 6/9, add9 or 11, built from the scale (or the Quality). Applies to the selected slot.");
    }
    for (int q = 0; q < static_cast<int>(gristo::ChordQuality::Count); ++q)
        chordQuality_.addItem(gristo::chordQualityName(static_cast<gristo::ChordQuality>(q)), q + 1);
    chordQuality_.onChange = [this] {
        const int q = chordQuality_.getSelectedId() - 1;
        if (q >= 0)
            editSelectedChord([q](gristo::ChordSlot& s) { s.quality = static_cast<gristo::ChordQuality>(q); });
    };
    addAndMakeVisible(chordQuality_);
    setHelp(chordQuality_, "Quality",
            "Scale: the chord the key gives. Major, Minor, Dominant, Dim, Half-dim or Aug override it for chords from outside the key, "
            "like the dominant III7 in jazz turnarounds (Dominant on III).");
    chordFlat_.onClick = [this] { editSelectedChord([](gristo::ChordSlot& s) { s.alter = s.alter < 0 ? 0 : -1; }); };
    chordSharp_.onClick = [this] { editSelectedChord([](gristo::ChordSlot& s) { s.alter = s.alter > 0 ? 0 : 1; }); };
    addAndMakeVisible(chordFlat_);
    addAndMakeVisible(chordSharp_);
    setHelp(chordFlat_, "Flat root", "Moves the selected chord's root down a semitone: bII for a tritone sub, bIII, bVI... Tap again to undo.");
    setHelp(chordSharp_, "Sharp root", "Moves the selected chord's root up a semitone (#IV, or So What's half-step lift). Tap again to undo.");
    const char* inversionNames[] = { "Root", "1st", "2nd" };
    const char* lengthNames[] = { "1/2", "1 bar", "2 bars" };
    for (int i = 0; i < 3; ++i)
    {
        chordInversions_[i].setButtonText(inversionNames[i]);
        chordInversions_[i].onClick = [this, i] { editSelectedChord([i](gristo::ChordSlot& s) { s.inversion = i; }); };
        chordLengths_[i].setButtonText(lengthNames[i]);
        chordLengths_[i].onClick = [this, i] { editSelectedChord([i](gristo::ChordSlot& s) { s.lengthSteps = p::kChordLengthSteps[i]; }); };
        addAndMakeVisible(chordInversions_[i]);
        addAndMakeVisible(chordLengths_[i]);
        setHelp(chordInversions_[i], "Inversion", "Which chord note is at the bottom: root, the 3rd (1st inversion) or the 5th (2nd).");
        setHelp(chordLengths_[i], "Length", "How long the selected chord lasts (Bars clock).");
    }
    std::vector<juce::TextButton*> toggles { &chordFlat_, &chordSharp_ };
    for (auto& b : chordTypes_)
        toggles.push_back(&b);
    for (int i = 0; i < 3; ++i)
    {
        toggles.push_back(&chordInversions_[i]);
        toggles.push_back(&chordLengths_[i]);
    }
    for (auto* b : toggles)
    {
        b->setColour(juce::TextButton::buttonOnColourId, colours::accent);
        b->setColour(juce::TextButton::textColourOnId, colours::background);
    }
    chordClear_.onClick = [this] { editSelectedChord([](gristo::ChordSlot& s) { s.used = false; }); };
    chordLibrary_.onClick = [this] { showChordLibrary(); };
    chordNew_.onClick = [this] { newChordProgression(); };
    chordSlots_.onSelect = [this](int slot) { selectedChordSlot_ = slot; };
    chordPads_.onPad = [this](int degree, bool borrowed) {
        editSelectedChord([degree, borrowed](gristo::ChordSlot& s) {
            s.used = true;
            s.degree = degree;
            s.borrowed = borrowed;
            s.quality = gristo::ChordQuality::Scale; // a pad gives the plain chord
            s.alter = 0;
        });
        // Filling the progression left to right: move on to the next slot.
        selectedChordSlot_ = std::min(selectedChordSlot_ + 1, gristo::kMaxChordSlots - 1);
    };

    setHelp(chordOn_.button, "Chords on", "Switch the chord sequencer on. Arps with Chords on their page follow it.");
    setHelp(chordKey_.box, "Key", "The key's tonic.");
    setHelp(chordScale_.box, "Scale", "The scale the chords are built from. Borrowed chords come from the parallel major / minor.");
    setHelp(chordClock_.box, "Clock", "Bars: each chord lasts its length. Drum hits: the chord changes every N hits of a drum voice. Keys: you play the chords, one per white key from C1 (MIDI in or the Keys keyboard).");
    setHelp(*chordHits_, "Hits per chord", "Drum hits clock: how many hits of the voice before the next chord.");
    setHelp(chordClockVoice_.box, "Clock voice", "Drum hits clock: the drum voice that is counted.");
    setHelp(chordReset_.box, "Reset", "Drum hits clock: the progression starts over every N bars.");
    setHelp(chordPlay_.box, "Play", "Off: only feeds the arps. Held: chords sustain. Stabs: chords hit with the chosen drum voices.");
    setHelp(chordOctave_.box, "Octave", "Where the chords sit (the lowest tonic).");
    setHelp(chordLibrary_, "Progression library", "Load a ready-made progression (keeps your key).");
    setHelp(chordNew_, "New progression", "Write a new progression that fits the scale.");
    setHelp(chordMutate_, "Mutate", "Chance per chord and loop of swapping in a related chord.");
    setHelp(chordGate_, "Stab gate", "Stab length in 16ths.");
    setHelp(chordClear_, "Clear slot", "Empty the selected slot.");
    setHelp(chordSlots_, "Progression", "Tap a slot to select it, then tap a pad to put a chord there. The slot playing now glows.");
    setHelp(chordPads_, "Chord pads", "The chords of the key (top) and borrowed from the parallel major / minor (bottom).");
    setHelp(*chordChannel_, "Chords MIDI channel", "MIDI channel of the chord part, for DAWs that keep channels.");
    setHelp(*chordShift_, "Chords Out Shift", "Moves the chord part's MIDI out into its own note range (for Live's key zones).");
    setHelp(*chordVelocity_, "Chords velocity", "Velocity range. Stabs follow the drum hit inside it; held chords sit near the top.");
    setHelp(chordOutLabel_, "MIDI out range", "Where the chord notes land, and the matching Live setup.");
}

std::vector<juce::Component*> GristoView::chordPageComponents()
{
    std::vector<juce::Component*> list { &chordOn_.button, &chordKey_.box, &chordScale_.box, &chordClock_.box, &chordClockVoice_.box,
                                         &chordReset_.box, &chordPlay_.box, &chordOctave_.box, chordHits_.get(), chordChannel_.get(),
                                         chordShift_.get(), chordVelocity_.get(), &chordLibrary_, &chordNew_, &chordClear_,
                                         &chordSlots_, &chordPads_, &chordOutLabel_, &chordMutate_.slider, &chordMutate_.label,
                                         &chordGate_.slider, &chordGate_.label, &chordQuality_, &chordFlat_, &chordSharp_,
                                         &chordStrum_.slider, &chordStrum_.label, &chordVoicing_.box, &chordVoicing_.label,
                                         &chordStrumDir_.box, &chordStrumDir_.label };
    for (auto& t : chordStabs_)
        list.push_back(&t.button);
    for (auto& b : chordTypes_)
        list.push_back(&b);
    for (auto& b : chordInversions_)
        list.push_back(&b);
    for (auto& b : chordLengths_)
        list.push_back(&b);
    return list;
}

void GristoView::writeChordSlot(int index, const gristo::ChordSlot& slot)
{
    // Only touch what changes: loading a progression stays a handful of updates.
    auto set = [this](const juce::String& id, float value) {
        if (auto* param = processor_.state.getParameter(id))
            if (std::abs(param->convertFrom0to1(param->getValue()) - value) > 1.0e-4f)
                setParameter(id, value);
    };
    set(p::voiceId(p::chordSlotUsed, index), slot.used ? 1.0f : 0.0f);
    // An emptied slot goes back to defaults, so a chord put in it later doesn't
    // inherit the type or quality of whatever was there before.
    const gristo::ChordSlot s = slot.used ? slot : gristo::ChordSlot {};
    set(p::voiceId(p::chordDegree, index), static_cast<float>(s.degree));
    set(p::voiceId(p::chordBorrowed, index), s.borrowed ? 1.0f : 0.0f);
    set(p::voiceId(p::chordType, index), static_cast<float>(s.type));
    set(p::voiceId(p::chordInversion, index), static_cast<float>(s.inversion));
    set(p::voiceId(p::chordQuality, index), static_cast<float>(s.quality));
    set(p::voiceId(p::chordAlter, index), static_cast<float>(s.alter));
    set(p::voiceId(p::chordLength, index), s.lengthSteps <= 8 ? 0.0f : s.lengthSteps >= 32 ? 2.0f : 1.0f);
}

void GristoView::editSelectedChord(const std::function<void(gristo::ChordSlot&)>& edit)
{
    const auto params = p::readEngineParams(processor_.state, processor_.modPoints);
    auto slot = params.chords.slots[static_cast<size_t>(selectedChordSlot_)];
    edit(slot);
    writeChordSlot(selectedChordSlot_, slot);
}

void GristoView::showChordLibrary()
{
    juce::PopupMenu menu;
    const auto& library = gristo::Chords::library();
    // One flat list with mood headings: nested submenus are fiddly on touch screens.
    juce::StringArray moods;
    for (const auto& entry : library)
        moods.addIfNotAlreadyThere(entry.mood);
    for (const auto& mood : moods)
    {
        menu.addSectionHeader(mood);
        for (int i = 0; i < static_cast<int>(library.size()); ++i)
            if (mood == library[static_cast<size_t>(i)].mood)
                menu.addItem(i + 1, juce::String(library[static_cast<size_t>(i)].name) + "  (" + gristo::chordScaleName(library[static_cast<size_t>(i)].scale) + ")");
    }
    juce::Component::SafePointer<GristoView> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&chordLibrary_), [safe](int result) {
        if (safe == nullptr || result <= 0)
            return;
        const auto& entry = gristo::Chords::library()[static_cast<size_t>(result - 1)];
        safe->setParameter(p::chordScale, static_cast<float>(entry.scale));
        for (int i = 0; i < gristo::kMaxChordSlots; ++i)
            safe->writeChordSlot(i, i < static_cast<int>(entry.slots.size()) ? entry.slots[static_cast<size_t>(i)] : gristo::ChordSlot {});
        safe->selectedChordSlot_ = 0;
    });
}

void GristoView::newChordProgression()
{
    const auto params = p::readEngineParams(processor_.state, processor_.modPoints);
    const auto seed = static_cast<std::uint32_t>(juce::Random::getSystemRandom().nextInt({ 1, 10000 }));
    const auto slots = gristo::Chords::generate(seed, params.chords.scale);
    for (int i = 0; i < gristo::kMaxChordSlots; ++i)
        writeChordSlot(i, slots[static_cast<size_t>(i)]);
    setParameter(p::chordSeed, static_cast<float>(seed));
    selectedChordSlot_ = 0;
}

void GristoView::updateChordPage(const gristo::EngineParams& params)
{
    const auto& c = params.chords;
    const auto& editing = c.slots[static_cast<size_t>(selectedChordSlot_)];
    chordSlots_.update(c, processor_.displayChordSlot.load(), selectedChordSlot_);
    chordPads_.update(c, editing);
    for (int t = 0; t < static_cast<int>(gristo::ChordType::Count); ++t)
        chordTypes_[t].setToggleState(editing.used && static_cast<int>(editing.type) == t, juce::dontSendNotification);
    // Only follow changes made elsewhere (another slot, a preset, the library): the
    // combo reports a pick asynchronously, and resetting it every frame could undo it.
    if (selectedChordSlot_ != shownQualitySlot_ || static_cast<int>(editing.quality) != shownQuality_)
    {
        shownQualitySlot_ = selectedChordSlot_;
        shownQuality_ = static_cast<int>(editing.quality);
        chordQuality_.setSelectedId(shownQuality_ + 1, juce::dontSendNotification);
    }
    chordFlat_.setToggleState(editing.used && editing.alter < 0, juce::dontSendNotification);
    chordSharp_.setToggleState(editing.used && editing.alter > 0, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i)
    {
        chordInversions_[i].setToggleState(editing.used && editing.inversion == i, juce::dontSendNotification);
        chordLengths_[i].setToggleState(editing.used && editing.lengthSteps == p::kChordLengthSteps[i], juce::dontSendNotification);
    }
    const bool drums = c.clock == gristo::ChordClock::Drums;
    for (juce::Component* comp : std::initializer_list<juce::Component*> { chordHits_.get(), &chordClockVoice_.box, &chordReset_.box })
        comp->setAlpha(drums ? 1.0f : 0.35f);
    const bool stabs = c.play == gristo::ChordPlay::Stabs;
    for (auto& t : chordStabs_)
        t.button.setAlpha(stabs ? 1.0f : 0.35f);
    chordGate_.slider.setAlpha(stabs ? 1.0f : 0.35f);
    // MIDI out range over every chord in the progression.
    int lo = 128, hi = -1;
    for (const auto& slot : c.slots)
        if (slot.used)
        {
            const auto chord = gristo::Chords::build(c, slot);
            for (int i = 0; i < chord.count; ++i)
            {
                const int n = gristo::Arp::outputNote(chord.notes[static_cast<size_t>(i)], c.shift);
                lo = std::min(lo, n);
                hi = std::max(hi, n);
            }
        }
    if (hi < 0)
        chordOutLabel_.setText("No chords yet: tap a slot, then a pad", juce::dontSendNotification);
    else
    {
        auto name = [](int note) { return juce::MidiMessage::getMidiNoteName(note, true, true, 3); };
        juce::String text = "MIDI out " + name(lo) + " - " + name(hi) + ".  Live: key zone " + name(lo) + " - " + name(hi);
        if (c.shift != 0)
            text << ", Pitch " << (c.shift > 0 ? "-" : "+") << std::abs(c.shift);
        chordOutLabel_.setText(text, juce::dontSendNotification);
    }
}

std::vector<juce::Component*> GristoView::soundPageComponents()
{
    std::vector<juce::Component*> list { &soundKit_.box, &fxTime_.box, &fxTime_.label };
    auto addKnob = [&list](Knob& k) {
        list.push_back(&k.slider);
        list.push_back(&k.label);
    };
    for (Knob* k : { &kitTune_, &kitDecay_, &kitTone_, &kitDrive_, &kitReverb_, &kitDelay_, &fxSize_, &fxFeedback_ })
        addKnob(*k);
    for (auto& k : drumLevels_)
        addKnob(k);
    for (auto& row : synthRows_)
    {
        list.push_back(&row.sound);
        for (Knob* k : { &row.cutoff, &row.resonance, &row.env, &row.attack, &row.release, &row.glide, &row.level, &row.reverb, &row.delay, &row.duck })
            addKnob(*k);
    }
    for (auto& h : soundLaneHandles_)
        list.push_back(&h);
    return list;
}

std::vector<juce::Component*> GristoView::arpPageComponents()
{
    std::vector<juce::Component*> list { &arpOn_.button, &arpLatch_.button, &arpMode_.box, &arpReset_.box, &arpOrder_.box,
                                         &arpDivide_.box, &arpDivide_.label,
                                         &arpOrder_.label, &arpScale_.box, &arpScale_.label, &arpNew_, &arpRootLabel_,
                                         arpChannel_.get(), arpRoot_.get(), arpVelocity_.get(), &arpView_,
                                         &arpShiftLabel_, arpShift_.get(), &arpOutLabel_ };
    for (auto& handle : arpLaneHandles_)
        list.push_back(&handle);
    for (auto& t : arpTriggers_)
        list.push_back(&t.button);
    list.push_back(&arpFollow_.button);
    for (Knob* k : { &arpOctaves_, &arpGate_, &arpLength_, &arpDensity_, &arpAccent_, &arpSlide_, &arpMutate_, &arpProb_, &arpProbRandom_ })
    {
        list.push_back(&k->slider);
        list.push_back(&k->label);
    }
    return list;
}

// ---------------------------------------------------------------------------------
// Part strip, pages

void GristoView::setupPartStrip()
{
    for (int part = 0; part <= gristo::kNumParts; ++part)
    {
        auto& b = partSelect_[part];
        const bool master = part == gristo::kNumParts;
        b.setButtonText(master ? "Master / FX" : gristo::partName(part));
        b.setClickingTogglesState(true);
        b.setRadioGroupId(4713);
        b.setColour(juce::TextButton::buttonOnColourId, master ? colours::accent : partColour(part));
        b.setColour(juce::TextButton::textColourOnId, colours::background);
        b.onClick = [this, part] {
            selectedPart_ = part;
            showPage(pageOfPart(part));
        };
        addAndMakeVisible(b);
        setHelp(b, master ? juce::String("Master / FX") : juce::String(gristo::partName(part)),
                master ? "Reverb, delay and output settings."
                       : part < gristo::kNumVoices ? "Shows the drums: map, voices, groove and kit."
                                                    : "Shows this part: its notes on top, its sound below.");
        if (master)
            continue;
        setupToggle(partMute_[part], p::voiceId(p::partMute, part), "M");
        setupToggle(partSolo_[part], p::voiceId(p::partSolo, part), "S");
        partSolo_[part].button.setColour(juce::TextButton::buttonOnColourId, colours::random);
        setHelp(partMute_[part].button, "Mute", "Silences this part. A muted drum still clocks the arps and chords that listen to it.");
        setHelp(partSolo_[part].button, "Solo", "Only soloed parts sound.");
        const juce::String levelId = part < gristo::kNumVoices ? p::voiceId(p::drumLevel, part)
                                                                : p::laneId(p::synthLevel, part - gristo::kNumVoices);
        setupSmallKnob(partLevel_[part], levelId, {});
        partLevel_[part].label.setVisible(false);
        setHelp(partLevel_[part], "Level", "Volume of this part's built-in sound.");
    }
    setupToggle(separateOuts_, p::separateOuts, "Separate outputs");
    setHelp(separateOuts_.button, "Separate outputs",
            "Sends Kit, Arp 1-3, Chords and FX to their own outputs of the plugin (enable them in your DAW) instead of the main mix.");

    tempoBar_.setSliderStyle(juce::Slider::LinearBar);
    tempoBar_.setTextValueSuffix(" bpm");
    tempoBar_.setColour(juce::Slider::trackColourId, colours::cool);
    addAndMakeVisible(tempoBar_);
    tempoAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor_.state, p::freeTempo, tempoBar_);
    setHelp(tempoBar_, "Tempo", "Tempo of Gristo's own clock (Play). In a DAW that is running, the DAW's tempo is used.");
    levelBar_.setSliderStyle(juce::Slider::LinearBar);
    levelBar_.setColour(juce::Slider::trackColourId, colours::cool);
    levelBar_.textFromValueFunction = [](double v) { return "Level " + juce::String(juce::roundToInt(v * 100.0)) + "%"; };
    addAndMakeVisible(levelBar_);
    levelAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor_.state, p::kitGain, levelBar_);
    levelBar_.textFromValueFunction = [](double v) { return "Level " + juce::String(juce::roundToInt(v * 100.0)) + "%"; };
    levelBar_.updateText();
    setHelp(levelBar_, "Level", "Volume of the built-in sounds.");

    // Drag the pattern to a DAW track as MIDI.
    for (int bars : { 1, 2, 4, 8, 16 })
        dragBars_.addItem(juce::String(bars) + (bars == 1 ? " bar" : " bars"), bars);
    dragBars_.setSelectedId(4, juce::dontSendNotification);
    addAndMakeVisible(dragBars_);
    setHelp(dragBars_, "MIDI length", "How many bars the dragged MIDI clip holds.");
    dragPart_.onDrag = [this] { dragMidi(selectedPart_ < gristo::kNumVoices ? GristoProcessor::kAllDrums
                                         : selectedPart_ < gristo::kNumParts ? selectedPart_ : GristoProcessor::kAllParts); };
    dragAll_.onDrag = [this] { dragMidi(GristoProcessor::kAllParts); };
    dragAll_.text = "All parts";
    addAndMakeVisible(dragPart_);
    addAndMakeVisible(dragAll_);
    setHelp(dragPart_, "Drag MIDI", "Drag this onto a track in your DAW: the selected part as a MIDI clip, exactly as it plays now.");
    setHelp(dragAll_, "Drag MIDI (all)", "Drag onto your DAW: every sounding part as its own MIDI track.");
}

void GristoView::setupPerformance()
{
    // Undo / redo / dice
    undoButton_.onClick = [this] { processor_.undo(); };
    redoButton_.onClick = [this] { processor_.redo(); };
    diceAllButton_.onClick = [this] { processor_.diceAll(); };
    for (auto* b : { &undoButton_, &redoButton_, &diceAllButton_ })
        addAndMakeVisible(*b);
    setHelp(undoButton_, "Undo", "Steps back through your edits, preset loads, scene recalls and dice rolls.");
    setHelp(redoButton_, "Redo", "Steps forward again.");
    setHelp(diceAllButton_, "Dice", "New values for every section that isn't set to Keep. Undo brings the old ones back.");
    const char* const sectionNames[GristoProcessor::NumSections] = { "the map and fills", "the groove", "the kit", "this arp's notes",
                                                                     "this arp's notes", "this arp's notes", "the progression",
                                                                     "this sound", "this sound", "this sound", "this sound" };
    for (int section = 0; section < GristoProcessor::NumSections; ++section)
    {
        auto& dice = diceButtons_[section];
        dice.setButtonText("Dice");
        dice.onClick = [this, section] { processor_.dice(section); };
        addAndMakeVisible(dice);
        setHelp(dice, "Dice", juce::String("Rolls new values for ") + sectionNames[section] + ". Undo brings the old ones back.");
        auto& keep = keepButtons_[section];
        keep.setButtonText("Keep");
        keep.setClickingTogglesState(true);
        keep.setColour(juce::TextButton::buttonOnColourId, colours::accent);
        keep.setColour(juce::TextButton::textColourOnId, colours::background);
        keep.setToggleState((processor_.diceLocks.load() & (1 << section)) != 0, juce::dontSendNotification);
        keep.onClick = [this, section] {
            const int bit = 1 << section;
            processor_.diceLocks.store(keepButtons_[section].getToggleState() ? (processor_.diceLocks.load() | bit)
                                                                              : (processor_.diceLocks.load() & ~bit));
        };
        addAndMakeVisible(keep);
        setHelp(keep, "Keep", "The big Dice button in the top bar leaves this section alone.");
    }

    // Scenes
    for (int i = 0; i < GristoProcessor::kNumScenes; ++i)
    {
        auto& b = sceneButtons_[i];
        b.setButtonText(juce::String(i + 1));
        b.onClick = [this, i] {
            if (juce::ModifierKeys::currentModifiers.isShiftDown() || !processor_.sceneUsed(i))
                processor_.storeScene(i);
            else
                processor_.recallScene(i);
        };
        b.addMouseListener(this, false);
        addAndMakeVisible(b);
        setHelp(b, "Scene " + juce::String(i + 1),
                "Empty: click to store everything as it is now. Stored: click to recall (on the next bar while playing). "
                "Right-click to store again or clear. MIDI: channel 16, C1 upwards.");
    }
    chainButton_.setClickingTogglesState(true);
    chainButton_.setColour(juce::TextButton::buttonOnColourId, colours::accent);
    chainButton_.setColour(juce::TextButton::textColourOnId, colours::background);
    chainButton_.setToggleState(processor_.chainScenes.load(), juce::dontSendNotification);
    chainButton_.onClick = [this] { processor_.chainScenes.store(chainButton_.getToggleState()); };
    addAndMakeVisible(chainButton_);
    setHelp(chainButton_, "Chain", "Plays the stored scenes one after the other, each for the number of bars next to this: a simple song.");
    for (int bars : { 1, 2, 4, 8, 16 })
        chainBars_.addItem(juce::String(bars) + (bars == 1 ? " bar" : " bars"), bars);
    chainBars_.setSelectedId(processor_.chainBars.load(), juce::dontSendNotification);
    chainBars_.onChange = [this] { processor_.chainBars.store(std::max(1, chainBars_.getSelectedId())); };
    addAndMakeVisible(chainBars_);
    setHelp(chainBars_, "Chain length", "How long each scene plays in a chain.");

    // Macros
    for (int m = 0; m < gristo::kNumMacros; ++m)
    {
        setupSmallKnob(macros_[m], p::laneId(p::macroValue, m), {});
        macros_[m].label.setVisible(false);
        macros_[m].slider.setColour(juce::Slider::rotarySliderFillColourId, colours::accent);
        macros_[m].slider.onContextMenu = [this, m] { showMacroMenu(m); };
        setHelp(macros_[m], "Macro " + juce::String(m + 1),
                "One knob for several settings: right-click any knob and choose Control with macro. Right-click here for MIDI learn.");
    }

    // Routing source: a lane or a macro
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
        sourceBox_.addItem("Lane " + juce::String(lane + 1), lane + 1);
    for (int m = 0; m < gristo::kNumMacros; ++m)
        sourceBox_.addItem("Macro " + juce::String(m + 1), gristo::kNumModLanes + m + 1);
    sourceBox_.onChange = [this] {
        const int source = sourceBox_.getSelectedId() - 1;
        if (source < 0 || source == selectedSource_)
            return;
        if (source < gristo::kNumModLanes)
            laneTabs_[source].triggerClick();
        else
            selectSource(source);
    };
    addAndMakeVisible(sourceBox_);
    setHelp(sourceBox_, "Routing of", "Which lane's or macro's destinations the four rows below show.");
    selectSource(selectedSource_);
}

void GristoView::showMacroMenu(int macro)
{
    juce::PopupMenu menu;
    const int cc = processor_.macroCc[static_cast<size_t>(macro)].load();
    menu.addSectionHeader("Macro " + juce::String(macro + 1) + (cc >= 0 ? "  (CC " + juce::String(cc) + ")" : juce::String()));
    menu.addItem(1, "Show its routing");
    menu.addItem(2, processor_.learnMacro.load() == macro ? "Waiting for a MIDI controller..." : "Learn MIDI controller (move one now)");
    menu.addItem(3, "Forget MIDI controller", cc >= 0);
    juce::Component::SafePointer<GristoView> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&macros_[macro].slider), [safe, macro](int result) {
        if (safe == nullptr)
            return;
        if (result == 1)
            safe->selectSource(gristo::kNumModLanes + macro);
        else if (result == 2)
            safe->processor_.learnMacro.store(macro);
        else if (result == 3)
            safe->processor_.macroCc[static_cast<size_t>(macro)].store(-1);
    });
}

void GristoView::showSceneMenu(int scene)
{
    juce::PopupMenu menu;
    menu.addSectionHeader("Scene " + juce::String(scene + 1));
    menu.addItem(1, "Store the current state here");
    menu.addItem(2, "Clear", processor_.sceneUsed(scene));
    juce::Component::SafePointer<GristoView> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&sceneButtons_[scene]), [safe, scene](int result) {
        if (safe == nullptr)
            return;
        if (result == 1)
            safe->processor_.storeScene(scene);
        else if (result == 2)
            safe->processor_.clearScene(scene);
    });
}

void GristoView::showSampleMenu(int voice)
{
    juce::PopupMenu menu;
    const auto current = processor_.drumSampleName(voice);
    menu.addSectionHeader(juce::String(gristo::voiceName(voice)) + (current.isEmpty() ? juce::String() : ": " + current));
    menu.addItem(1, "Load a sample...");
    menu.addItem(2, "Back to the kit's sound", current.isNotEmpty());
    // Samples used before, straight from the samples folder.
    const auto files = GristoProcessor::samplesFolder().findChildFiles(juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    if (!files.isEmpty())
        menu.addSeparator();
    for (int i = 0; i < std::min(files.size(), 40); ++i)
        menu.addItem(100 + i, files[i].getFileNameWithoutExtension(), true, files[i].getFileName() == current);
    juce::Component::SafePointer<GristoView> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&sampleButtons_[voice]), [safe, voice, files](int result) {
        if (safe == nullptr || result <= 0)
            return;
        auto failed = [safe] {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Sample",
                                                   "That file couldn't be read as audio (WAV, AIFF, FLAC, OGG or MP3).", "OK", safe.getComponent());
        };
        if (result == 2)
            safe->processor_.clearDrumSample(voice);
        else if (result >= 100)
        {
            if (!safe->processor_.loadDrumSample(voice, juce::URL(files[result - 100])))
                failed();
        }
        else if (result == 1)
        {
            safe->chooser_ = std::make_unique<juce::FileChooser>("Load a drum sample", juce::File(),
                                                                 kIsMobile ? juce::String() : juce::String("*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3"));
            safe->chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                        [safe, voice, failed](const juce::FileChooser& fc) {
                                            const auto url = fc.getURLResult();
                                            if (safe != nullptr && !url.isEmpty() && !safe->processor_.loadDrumSample(voice, url))
                                                failed();
                                        });
        }
    });
}

juce::Colour GristoView::partColour(int part)
{
    if (part < gristo::kNumVoices)
        return voiceColours[part];
    if (part < gristo::kChordVoice)
        return arpColours[part - gristo::kNumVoices];
    return colours::random;
}

int GristoView::pageOfPart(int part)
{
    if (part < gristo::kNumVoices)
        return 0;
    if (part < gristo::kChordVoice)
        return 1 + part - gristo::kNumVoices;
    return part == gristo::kChordVoice ? kChordsPage : kMasterPage;
}

void GristoView::dragMidi(int part)
{
    const auto file = processor_.exportMidiToTempFile(dragBars_.getSelectedId(), part);
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles({ file.getFullPathName() }, false, this);
}

std::vector<juce::Component*> GristoView::drumsPageComponents()
{
    std::vector<juce::Component*> list { &mapPad_, &nodeLabel_, &linear_.button, &lock_.button, &timing_.box, &timing_.label,
                                         &feeling_.box, &feeling_.label, &soundKit_.box };
    auto addKnob = [&list](Knob& k) {
        list.push_back(&k.slider);
        list.push_back(&k.label);
    };
    for (Knob* k : { &chaos_, &variation_, &mapRandom_, &shuffle_, &shuffleRandom_, &feelMs_, &feelRandom_, &humanize_, &dynamics_,
                     &kitTune_, &kitDecay_, &kitTone_, &kitDrive_, &kitReverb_, &kitDelay_ })
        addKnob(*k);
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        for (Knob* k : { &fills_[v], &fillRandoms_[v], &probabilities_[v], &probabilityRandoms_[v], &voiceHumanize_[v], &voiceDynamics_[v],
                         &voiceRatchet_[v], &voiceSwing_[v] })
            list.push_back(&k->slider);
        list.push_back(voiceLengths_[v].get());
        list.push_back(velocityRanges_[v].get());
        list.push_back(notes_[v].get());
        list.push_back(channels_[v].get());
        list.push_back(&sampleButtons_[v]);
    }
    list.push_back(&voiceTabs_[0]);
    list.push_back(&voiceTabs_[1]);
    return list;
}

void GristoView::showVoiceTab(int tab)
{
    voiceTab_ = tab;
    const bool drums = page_ == 0;
    for (int t = 0; t < 2; ++t)
        voiceTabs_[t].setColour(juce::TextButton::buttonColourId, t == tab ? colours::accent.withAlpha(0.55f) : colours::panel);
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        for (Knob* k : { &fills_[v], &fillRandoms_[v], &probabilities_[v], &probabilityRandoms_[v], &voiceRatchet_[v] })
            k->slider.setVisible(drums && tab == 0);
        voiceLengths_[v]->setVisible(drums && tab == 0);
        for (Knob* k : { &voiceHumanize_[v], &voiceDynamics_[v], &voiceSwing_[v] })
            k->slider.setVisible(drums && tab == 1);
        for (juce::Component* c : std::initializer_list<juce::Component*> { velocityRanges_[v].get(), notes_[v].get(), channels_[v].get(), &sampleButtons_[v] })
            c->setVisible(drums && tab == 1);
    }
    repaint();
}

std::vector<juce::Component*> GristoView::masterPageComponents()
{
    return { &fxSize_.slider, &fxSize_.label, &fxFeedback_.slider, &fxFeedback_.label, &fxTime_.box, &fxTime_.label,
             &noteLength_.slider, &noteLength_.label, &separateOuts_.button };
}

std::vector<juce::Component*> GristoView::synthComponents(int synth)
{
    auto& row = synthRows_[synth];
    std::vector<juce::Component*> list { &row.sound };
    for (Knob* k : { &row.cutoff, &row.resonance, &row.env, &row.attack, &row.release, &row.glide, &row.level, &row.reverb, &row.delay, &row.duck })
    {
        list.push_back(&k->slider);
        list.push_back(&k->label);
    }
    return list;
}

void GristoView::showPage(int page)
{
    page_ = page;
    if (isArpPage())
    {
        lastArpPage_ = page;
        bindArp(page - 1);
    }
    for (auto* c : drumsPageComponents())
        c->setVisible(page == 0);
    for (auto* c : arpPageComponents())
        if (c != nullptr)
            c->setVisible(isArpPage());
    for (auto* c : chordPageComponents())
        if (c != nullptr)
            c->setVisible(page == kChordsPage);
    for (auto* c : masterPageComponents())
        c->setVisible(page == kMasterPage);
    for (int s = 0; s < gristo::sound::kNumSynths; ++s)
        for (auto* c : synthComponents(s))
            c->setVisible(isArpPage() ? s == page - 1 : page == kChordsPage && s == gristo::sound::kChordSynth);
    // Not part of this layout: the part strip and the timeline replace them.
    for (auto& t : pageTabs_)
        t.setVisible(false);
    for (auto& m : mutes_)
        m.button.setVisible(false);
    for (auto& k : drumLevels_)
    {
        k.slider.setVisible(false);
        k.label.setVisible(false);
    }
    for (auto* handles : { arpLaneHandles_, soundLaneHandles_ })
        for (int i = 0; i < gristo::kNumModLanes; ++i)
            handles[i].setVisible(false);
    kitGain_.slider.setVisible(false);
    kitGain_.label.setVisible(false);
    freeTempo_.slider.setVisible(false);
    freeTempo_.label.setVisible(false);
    for (int section = 0; section < GristoProcessor::NumSections; ++section)
    {
        const bool show = section <= GristoProcessor::DiceKit ? page == 0
                        : section <= GristoProcessor::DiceArp3 ? page == 1 + section - GristoProcessor::DiceArp1
                        : section == GristoProcessor::DiceChords ? page == kChordsPage
                        : section == GristoProcessor::DiceSound4 ? page == kChordsPage
                                                                 : page == 1 + section - GristoProcessor::DiceSound1;
        diceButtons_[section].setVisible(show);
        keepButtons_[section].setVisible(show);
    }
    dragPart_.text = selectedPart_ < gristo::kNumVoices ? juce::String("Drums")
                   : selectedPart_ < gristo::kNumParts ? juce::String(gristo::partName(selectedPart_)) : juce::String("All parts");
    showVoiceTab(voiceTab_);
    resized();
    repaint();
}

void GristoView::setupArpPage()
{
    setupToggle(arpOn_, p::laneId(p::arpOn, 0), "On");
    setupToggle(arpLatch_, p::laneId(p::arpLatch, 0), "Latch");
    for (int v = 0; v < gristo::kNumVoices; ++v)
        setupToggle(arpTriggers_[v], p::slotId(p::arpTrigger, 0, v), kTriggerNames[v]);
    setupChoice(arpMode_, p::laneId(p::arpMode, 0), "Mode");
    setupChoice(arpReset_, p::laneId(p::arpReset, 0), "Reset");
    setupChoice(arpOrder_, p::laneId(p::arpOrder, 0), "Order");
    setupChoice(arpScale_, p::laneId(p::arpScale, 0), "Scale");
    setupChoice(arpDivide_, p::laneId(p::arpDivide, 0), "Step on");
    setHelp(arpDivide_.box, "Step on", "The arp plays on every trigger hit, or only on every 2nd, 3rd... hit: a slower line over the same drums.");
    arpMode_.label.setVisible(false);
    arpReset_.label.setVisible(false);
    setupKnob(arpOctaves_, p::laneId(p::arpOctaves, 0), "Octaves");
    setupKnob(arpGate_, p::laneId(p::arpGate, 0), "Gate");
    setupKnob(arpLength_, p::laneId(p::arpLength, 0), "Length");
    setupKnob(arpDensity_, p::laneId(p::arpDensity, 0), "Density");
    setupKnob(arpAccent_, p::laneId(p::arpAccent, 0), "Accent");
    setupKnob(arpSlide_, p::laneId(p::arpSlide, 0), "Slide");
    setupKnob(arpMutate_, p::laneId(p::arpMutate, 0), "Mutate");
    setupKnob(arpProb_, p::laneId(p::arpProbability, 0), "Prob");
    setupRandomKnob(arpProbRandom_, p::laneId(p::arpProbabilityRandom, 0), "Random");
    setupToggle(arpFollow_, p::laneId(p::arpFollow, 0), "Chords");
    arpNew_.setButtonText("New");
    arpNew_.onClick = [this] {
        // Roll a new pattern: a different seed for the generator.
        if (isArpPage())
            setParameter(p::laneId(p::arpSeed, page_ - 1), static_cast<float>(juce::Random::getSystemRandom().nextInt({ 1, 10000 })));
    };
    addAndMakeVisible(arpNew_);
    arpRootLabel_.setText("Root", juce::dontSendNotification);
    arpRootLabel_.setFont(juce::FontOptions(12.0f));
    arpRootLabel_.setColour(juce::Label::textColourId, colours::dim);
    addAndMakeVisible(arpRootLabel_);
    arpShiftLabel_.setText("Shift", juce::dontSendNotification);
    arpShiftLabel_.setFont(juce::FontOptions(12.0f));
    arpShiftLabel_.setColour(juce::Label::textColourId, colours::dim);
    addAndMakeVisible(arpShiftLabel_);
    arpOutLabel_.setFont(juce::FontOptions(11.5f));
    addAndMakeVisible(arpOutLabel_);
    addAndMakeVisible(arpView_);
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
    {
        arpLaneHandles_[lane].setLane(lane);
        arpLaneHandles_[lane].setCompact(true);
        addAndMakeVisible(arpLaneHandles_[lane]);
        setHelp(arpLaneHandles_[lane], "Lane " + juce::String(lane + 1),
                "Drag onto Prob, Density, Accent, Slide, Gate, Octaves, Length, Velocity, or the note view (Transpose). Draw the curve on the Modulation tab.");
    }

    setHelp(arpOn_.button, "Arp on", "Switch this arp on. It plays on its own MIDI channel; the drums keep playing.");
    setHelp(arpMode_.box, "Mode", "Arp: plays the notes held on MIDI in. Generate: a Sting-style pattern over the held notes, or the Root + Scale when nothing is held.");
    for (int v = 0; v < gristo::kNumVoices; ++v)
        setHelp(arpTriggers_[v].button, juce::String("Trigger: ") + gristo::voiceName(v),
                "Hits of this drum voice step the arp forward. Several voices can drive it; hits on the same step count once.");
    setHelp(arpReset_.box, "Reset", "The arp starts its phrase over every N bars (counted from the song start).");
    setHelp(arpLatch_.button, "Latch", "On: keeps playing the last chord after you let go. Off: plays only while keys are held.");
    setHelp(arpOrder_.box, "Order", "Arp mode: the order the held notes are played in.");
    setHelp(arpScale_.box, "Scale", "Generate mode: the scale used when no notes are held.");
    setHelp(arpShiftLabel_, "Out Shift", "Moves this arp's MIDI out into its own note range, so Live can split it from the drums and the other arps by key zone.");
    setHelp(arpOutLabel_, "MIDI out range", "Where this arp's notes land, and how to set up the Instrument Rack chain in Live. Red = it overlaps the drums, another arp or the chords.");
    setHelp(arpRootLabel_, "Root", "Generate mode: key root and lowest note when no notes are held.");
    setHelp(arpOctaves_, "Octaves", "How many octaves the notes are spread over.");
    setHelp(arpGate_, "Gate", "Note length in 16ths. Slides hold until the next note.");
    setHelp(arpLength_, "Length", "Generate: number of steps in the pattern before it repeats (within the reset).");
    setHelp(arpDensity_, "Density", "Generate: share of steps that play. Lower = more rests, even if the drum is busy.");
    setHelp(arpAccent_, "Accent", "Generate: share of steps played at full velocity.");
    setHelp(arpSlide_, "Slide", "Generate: share of steps that overlap into the next note (glide on a mono synth).");
    setHelp(arpMutate_, "Mutate", "Generate: share of steps re-rolled at every reset, so the pattern slowly evolves.");
    setHelp(arpNew_, "New pattern", "Roll a new generated pattern. It is saved with the project.");
    setHelp(arpProb_, "Probability", "Chance each arp step plays, independent of the drums' Probability (the arp follows the drum pattern as written). Skipped steps still move it on.");
    setHelp(arpProbRandom_, "Probability Random", "Moves Probability up and down from bar to bar (up to +-50%).");
    setHelp(arpFollow_.button, "Follow chords", "Use the Chords page's progression as this arp's notes, instead of the keys / MIDI in.");
    setHelp(arpView_, "Arp notes", "Bars = pitch, outlines = rests, dots = accents, lines = slides; bright column = current step. Drop a lane here to transpose (up to an octave).");
}

void GristoView::bindArp(int arp)
{
    auto& st = processor_.state;
    auto bindToggle = [&](Toggle& t, const juce::String& id) {
        t.attachment.reset();
        t.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(st, id, t.button);
    };
    auto bindChoice = [&](Choice& c, const juce::String& id) {
        c.attachment.reset();
        c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(st, id, c.box);
    };
    auto bindKnob = [&](Knob& k, const juce::String& id) {
        k.attachment.reset();
        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(st, id, k.slider);
    };
    bindToggle(arpOn_, p::laneId(p::arpOn, arp));
    bindToggle(arpLatch_, p::laneId(p::arpLatch, arp));
    for (int v = 0; v < gristo::kNumVoices; ++v)
        bindToggle(arpTriggers_[v], p::slotId(p::arpTrigger, arp, v));
    bindChoice(arpMode_, p::laneId(p::arpMode, arp));
    bindChoice(arpReset_, p::laneId(p::arpReset, arp));
    bindChoice(arpDivide_, p::laneId(p::arpDivide, arp));
    bindChoice(arpOrder_, p::laneId(p::arpOrder, arp));
    bindChoice(arpScale_, p::laneId(p::arpScale, arp));
    bindKnob(arpOctaves_, p::laneId(p::arpOctaves, arp));
    bindKnob(arpGate_, p::laneId(p::arpGate, arp));
    bindKnob(arpLength_, p::laneId(p::arpLength, arp));
    bindKnob(arpDensity_, p::laneId(p::arpDensity, arp));
    bindKnob(arpAccent_, p::laneId(p::arpAccent, arp));
    bindKnob(arpSlide_, p::laneId(p::arpSlide, arp));
    bindKnob(arpMutate_, p::laneId(p::arpMutate, arp));
    bindKnob(arpProb_, p::laneId(p::arpProbability, arp));
    bindKnob(arpProbRandom_, p::laneId(p::arpProbabilityRandom, arp));
    bindToggle(arpFollow_, p::laneId(p::arpFollow, arp));

    // Widgets with a ParameterAttachment are rebuilt for the new arp.
    arpChannel_ = std::make_unique<NumberSelector>(*st.getParameter(p::laneId(p::arpChannel, arp)), "Ch ", 1, 16);
    arpShift_ = std::make_unique<NumberSelector>(*st.getParameter(p::laneId(p::arpShift, arp)), "", -60, 60, " st", 12, true);
    arpRoot_ = std::make_unique<NoteSelector>(*st.getParameter(p::laneId(p::arpRoot, arp)));
    arpVelocity_ = std::make_unique<VelocityRange>(*st.getParameter(p::laneId(p::arpVelMin, arp)),
                                                   *st.getParameter(p::laneId(p::arpVelMax, arp)), arpColours[arp]);
    addAndMakeVisible(*arpChannel_);
    addAndMakeVisible(*arpShift_);
    addAndMakeVisible(*arpRoot_);
    addAndMakeVisible(*arpVelocity_);
    setHelp(*arpChannel_, "Arp MIDI channel", "MIDI channel this arp is sent on, for DAWs that keep channels. Ableton merges them: use Shift there.");
    setHelp(*arpShift_, "Out Shift", "Moves this arp's MIDI out up or down (semitones) into its own note range. In Live, undo it with a Pitch device before the synth.");
    setHelp(*arpRoot_, "Root", "Generate mode: key root and lowest note when no notes are held. Click for a list, scroll to step.");
    setHelp(*arpVelocity_, "Arp velocity", "Velocity range. Notes follow the triggering drum hit (accents at the top); generator accents play at the top.");
    arpView_.setArp(arp);
    bindArpTargets(arp);
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        arpTriggers_[v].button.setColour(juce::ToggleButton::tickColourId, voiceColours[v]);
    }
}

void GristoView::paint(juce::Graphics& g)
{
    g.fillAll(colours::background);
    g.setColour(colours::text);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("GRISTO", 16, 0, 100, 44, juce::Justification::centredLeft);
    g.setColour(colours::dim);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("SCENES", 16, 44, 60, 28, juce::Justification::centredLeft);

    // Help line
    const juce::Rectangle<int> help { 524, 44, 748, 28 };
    g.setColour(colours::panel);
    g.fillRoundedRectangle(help.toFloat(), 6.0f);
    auto helpArea = help.reduced(10, 0);
    g.setColour(colours::accent);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    const int titleWidth = juce::GlyphArrangement::getStringWidthInt(juce::FontOptions(12.0f, juce::Font::bold), helpTitle_) + 12;
    g.drawText(helpTitle_, helpArea.removeFromLeft(titleWidth), juce::Justification::centredLeft);
    g.setColour(colours::text);
    g.setFont(juce::FontOptions(11.5f));
    g.drawFittedText(helpText_, helpArea, juce::Justification::centredLeft, 2, 0.8f);

    // Part strip
    drawPanel(g, { 8, kPageY, 184, kPageH }, {});
    for (int part = 0; part < gristo::kNumParts; ++part)
    {
        g.setColour(partColour(part));
        g.fillRoundedRectangle(juce::Rectangle<float>(12.0f, static_cast<float>(partRowY(part)) + 3.0f, 3.0f, 26.0f), 1.5f);
    }
    g.setColour(colours::dim);
    g.setFont(juce::FontOptions(9.5f));
    for (int m = 0; m < gristo::kNumMacros; ++m) // macro numbers under their knobs
        g.drawText("M" + juce::String(m + 1), 14 + (m % 4) * 44, kMacroY + (m / 4) * 38 + 26, 44, 10, juce::Justification::centred);

    const int px = kPageX, py = kPageY;
    if (page_ == 0)
    {
        drawPanel(g, { px, py, 300, kPageH }, "Map");
        drawPanel(g, { px + 308, py, 504, 392 }, "Voices");
        drawPanel(g, { px + 820, py, 252, 392 }, "Kit");
        drawPanel(g, { px + 308, py + 400, 764, 100 }, {});
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        for (int v = 0; v < gristo::kNumVoices; ++v)
        {
            g.setColour(voiceColours[v]);
            g.drawText(gristo::voiceName(v), kVoiceX0 + v * kVoiceColW, py + 22, kVoiceColW, 16, juce::Justification::centred);
            if (v == selectedPart_) // the drum picked in the strip
            {
                g.setColour(voiceColours[v].withAlpha(0.08f));
                g.fillRoundedRectangle(juce::Rectangle<int>(kVoiceX0 + v * kVoiceColW + 1, py + 20, kVoiceColW - 2, 366).toFloat(), 5.0f);
            }
        }
        g.setFont(juce::FontOptions(11.5f));
        static const char* const patternRows[] = { "Fill", "Random", "Probability", "Random", "Ratchet", "Length" };
        static const char* const playRows[] = { "Humanize", "Dynamics", "Swing", "Velocity", "Note", "Channel", "Sample" };
        const int count = voiceTab_ == 0 ? 6 : 7;
        for (int r = 0; r < count; ++r)
        {
            const int y = voiceTab_ == 0 ? kVoiceRowY[r] : kPlayRowY[r];
            g.setColour(voiceTab_ == 0 && (r == 1 || r == 3) ? colours::random : colours::dim);
            g.drawText(voiceTab_ == 0 ? patternRows[r] : playRows[r], px + 318, y + 17, 66, 16, juce::Justification::centredLeft);
            if (r > 0)
            {
                g.setColour(colours::outline.withAlpha(0.35f));
                g.drawHorizontalLine(y - 1, static_cast<float>(px + 318), static_cast<float>(px + 804));
            }
        }
    }
    else if (page_ == kMasterPage)
    {
        drawPanel(g, { px, py, 520, 200 }, "Reverb / Delay");
        drawPanel(g, { px + 528, py, 544, 200 }, "Output");
        g.setColour(colours::dim);
        g.setFont(juce::FontOptions(12.0f));
        g.drawFittedText("MIDI to your DAW: drag the handles in the bar below the pages onto a track. "
                         "Separate outputs: Kit, Arp 1-3, Chords and FX each get their own plugin output.",
                         { px + 548, py + 136, 504, 52 }, juce::Justification::topLeft, 3);
    }
    else
    {
        drawPanel(g, { px, py, kPageW, 292 }, {});
        drawPanel(g, { px, py + 300, kPageW, 200 }, "Sound");
        g.setColour(colours::dim);
        g.setFont(juce::FontOptions(11.5f));
        if (page_ == kChordsPage)
            g.drawText("Stab on", kBlockDx + 28, kModBottomY, 50, 22, juce::Justification::centredLeft);
        else
        {
            g.drawText("Triggers", kBlockDx + 204, kModRowY, 56, 26, juce::Justification::centredLeft);
            g.drawText("Velocity", kBlockDx + 764, kModRowY + 170, 56, 24, juce::Justification::centredLeft);
        }
        g.setColour(page_ == kChordsPage ? colours::random : arpColours[page_ - 1]);
        g.fillRoundedRectangle(juce::Rectangle<float>(static_cast<float>(px) + 6.0f, static_cast<float>(py) + 12.0f, 3.0f, 268.0f), 1.5f);
    }

    // Timeline
    drawPanel(g, { 8, kTimelineY, kWidth - 16, kHeight - kTimelineY - 6 }, {});
    g.setColour(colours::dim);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("ROUTING OF", kSlotX, kTimelineY + 8, 80, 24, juce::Justification::centredLeft);
    g.drawText("MIDI", 670, kTimelineY + 8, 30, 24, juce::Justification::centredLeft);
    g.setColour(sourceColour(selectedSource_));
    g.fillRoundedRectangle(juce::Rectangle<float>(static_cast<float>(kSlotX) - 8.0f, static_cast<float>(kSlotRowY[0]), 3.0f, 148.0f), 1.5f);
}

void GristoView::paintOverChildren(juce::Graphics& g)
{
    // Activity lights on the part strip.
    for (int part = 0; part < gristo::kNumParts; ++part)
    {
        const float a = activity_[static_cast<size_t>(part)];
        if (a < 0.02f)
            continue;
        g.setColour(juce::Colours::white.withAlpha(juce::jlimit(0.0f, 1.0f, a)));
        g.fillEllipse(86.0f, static_cast<float>(partRowY(part)) + 4.0f, 6.0f, 6.0f);
    }
}

std::vector<juce::Component*> GristoView::headerControls()
{
    return { &presetPrev_, &presetName_, &presetNext_, &presetSave_, &presetShare_, &keysButton_, &sizeButton_ };
}

std::vector<LaneHandle*> GristoView::laneHandles()
{
    std::vector<LaneHandle*> list { &laneHandle_ };
    for (auto& h : arpLaneHandles_)
        list.push_back(&h);
    for (auto& h : soundLaneHandles_)
        list.push_back(&h);
    return list;
}

void GristoView::armLane(int lane)
{
    armedLane_ = lane;
    armCatcher_.colour = laneColours[lane];
    armCatcher_.setBounds(getLocalBounds());
    armCatcher_.setVisible(true);
    armCatcher_.toFront(false);
    for (auto* h : laneHandles())
        h->setArmed(h->lane() == lane);
    helpTitle_ = "Route lane " + juce::String(lane + 1);
    helpText_ = "Tap a knob, a Velocity range or the map to route this lane there. Tap anywhere else to cancel.";
    repaint(520, 40, 760, 34);
    if (onHelpChanged)
        onHelpChanged();
    if (onArmChanged)
        onArmChanged();
}

void GristoView::disarmLane()
{
    armedLane_ = -1;
    armCatcher_.setVisible(false);
    for (auto* h : laneHandles())
        h->setArmed(false);
    if (onArmChanged)
        onArmChanged();
}

void GristoView::showBottomPage(int page)
{
    for (int pg = 0; pg <= kSoundsPage; ++pg)
        pageTabs_[pg].setToggleState(pg == page, juce::dontSendNotification);
    showPage(page);
}

void GristoView::resized()
{
    auto placeLabeled = [](Knob& k, juce::Rectangle<int> area) {
        k.label.setBounds(area.removeFromBottom(14));
        k.slider.setBounds(area);
    };
    // Top bar, row 1: presets and transport
    presetPrev_.setBounds(118, 8, 26, 28);
    presetName_.setBounds(146, 8, 170, 28);
    presetNext_.setBounds(318, 8, 26, 28);
    presetSave_.setBounds(348, 8, 48, 28);
    presetShare_.setBounds(400, 8, 52, 28);
    freeRun_.button.setBounds(466, 8, 64, 28);
    tempoBar_.setBounds(536, 8, 104, 28);
    levelBar_.setBounds(652, 8, 110, 28);
    kitOn_.button.setBounds(768, 8, 66, 28);
    undoButton_.setBounds(856, 8, 54, 28);
    redoButton_.setBounds(914, 8, 54, 28);
    diceAllButton_.setBounds(984, 8, 60, 28);
    keysButton_.setBounds(1112, 8, 56, 28);
    sizeButton_.setBounds(1174, 8, 96, 28);
    // Row 2: scenes (the help line is painted to their right)
    for (int i = 0; i < GristoProcessor::kNumScenes; ++i)
        sceneButtons_[i].setBounds(78 + i * 34, 45, 30, 26);
    chainButton_.setBounds(360, 45, 60, 26);
    chainBars_.setBounds(424, 45, 88, 26);

    // Part strip
    for (int part = 0; part < gristo::kNumParts; ++part)
    {
        const int y = partRowY(part);
        partSelect_[part].setBounds(18, y, 78, 32);
        partMute_[part].button.setBounds(100, y + 4, 24, 24);
        partSolo_[part].button.setBounds(126, y + 4, 24, 24);
        partLevel_[part].slider.setBounds(152, y, 36, 32);
    }
    partSelect_[gristo::kNumParts].setBounds(18, partRowY(gristo::kNumParts), 170, 28);
    for (int m = 0; m < gristo::kNumMacros; ++m)
        macros_[m].slider.setBounds(18 + (m % 4) * 44, kMacroY + (m / 4) * 38 - 4, 36, 32);

    const int px = kPageX, py = kPageY, gy0 = kPageY + 400;
    // Drums page: map
    mapPad_.setBounds(px + 12, py + 24, 276, 276);
    nodeLabel_.setBounds(px + 12, py + 304, 276, 18);
    placeKnob(chaos_, { px + 12, py + 326, 92, 108 });
    placeKnob(mapRandom_, { px + 104, py + 338, 92, 92 });
    placeKnob(variation_, { px + 196, py + 326, 92, 108 });
    linear_.button.setBounds(px + 12, py + 452, 100, 28);
    auto placeDice = [this](int section, int x, int y) {
        diceButtons_[section].setBounds(x, y, 54, 28);
        keepButtons_[section].setBounds(x + 58, y, 54, 28);
    };
    placeDice(GristoProcessor::DicePattern, px + 172, py + 452);
    placeDice(GristoProcessor::DiceGroove, px + 952, gy0 + 8);
    placeDice(GristoProcessor::DiceKit, px + 952, py + 26);
    for (int a = 0; a < gristo::kNumArps; ++a)
    {
        placeDice(GristoProcessor::DiceArp1 + a, px + 24, py + 252);
        placeDice(GristoProcessor::DiceSound1 + a, px + 24, py + 392);
    }
    placeDice(GristoProcessor::DiceChords, px + 24, py + 252);
    placeDice(GristoProcessor::DiceSound4, px + 24, py + 392);

    voiceTabs_[0].setBounds(px + 308 + 76, py + 5, 70, 18);
    voiceTabs_[1].setBounds(px + 308 + 150, py + 5, 56, 18);
    // Voices matrix: one column per voice, one row per parameter
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        const int x = kVoiceX0 + v * kVoiceColW;
        // Pattern tab
        Knob* pattern[] = { &fills_[v], &fillRandoms_[v], &probabilities_[v], &probabilityRandoms_[v], &voiceRatchet_[v] };
        for (int r = 0; r < 5; ++r)
            pattern[r]->slider.setBounds(x + 2, kVoiceRowY[r] + 3, kVoiceColW - 4, kVoiceRowH);
        voiceLengths_[v]->setBounds(x + 5, kVoiceRowY[5] + 13, kVoiceColW - 10, 24);
        // Play tab
        Knob* play[] = { &voiceHumanize_[v], &voiceDynamics_[v], &voiceSwing_[v] };
        for (int r = 0; r < 3; ++r)
            play[r]->slider.setBounds(x + 2, kPlayRowY[r] + 3, kVoiceColW - 4, kVoiceRowH);
        velocityRanges_[v]->setBounds(x + 3, kPlayRowY[3] + 13, kVoiceColW - 6, 24);
        notes_[v]->setBounds(x + 3, kPlayRowY[4] + 14, kVoiceColW - 6, 22);
        channels_[v]->setBounds(x + 10, kPlayRowY[5] + 14, kVoiceColW - 20, 22);
        sampleButtons_[v].setBounds(x + 3, kPlayRowY[6] + 14, kVoiceColW - 6, 22);
    }

    // Kit
    soundKit_.box.setBounds(px + 832, py + 26, 114, 28);
    {
        Knob* kitKnobs[] = { &kitTune_, &kitDecay_, &kitTone_, &kitDrive_, &kitReverb_, &kitDelay_ };
        for (int i = 0; i < 6; ++i)
            placeLabeled(*kitKnobs[i], { px + 838 + (i % 2) * 112, py + 66 + (i / 2) * 106, 104, 96 });
    }

    // Groove
    const int gy = py + 400;
    timing_.label.setBounds(px + 320, gy + 6, 100, 14);
    timing_.box.setBounds(px + 320, gy + 20, 104, 24);
    feeling_.label.setBounds(px + 320, gy + 50, 100, 14);
    feeling_.box.setBounds(px + 320, gy + 64, 104, 24);
    placeKnob(shuffle_, { px + 440, gy + 2, 84, 96 });
    placeKnob(shuffleRandom_, { px + 522, gy + 14, 64, 82 });
    placeKnob(feelMs_, { px + 600, gy + 2, 84, 96 });
    placeKnob(feelRandom_, { px + 682, gy + 14, 64, 82 });
    placeKnob(humanize_, { px + 770, gy + 2, 84, 96 });
    placeKnob(dynamics_, { px + 860, gy + 2, 84, 96 });
    lock_.button.setBounds(px + 952, gy + 56, 112, 28);

    // Master page
    placeLabeled(fxSize_, { px + 24, py + 40, 110, 130 });
    placeLabeled(fxFeedback_, { px + 144, py + 40, 110, 130 });
    fxTime_.label.setBounds(px + 280, py + 70, 90, 14);
    fxTime_.box.setBounds(px + 280, py + 86, 100, 28);
    placeKnob(noteLength_, { px + 960, py + 30, 96, 110 });
    separateOuts_.button.setBounds(px + 548, py + 40, 190, 28);

    // Synth of the shown arp / the chords
    for (int s = 0; s < gristo::sound::kNumSynths; ++s)
    {
        auto& row = synthRows_[s];
        row.sound.setBounds(px + 24, py + 352, 190, 30);
        Knob* knobs[] = { &row.cutoff, &row.resonance, &row.env, &row.attack, &row.release, &row.glide, &row.level, &row.reverb, &row.delay, &row.duck };
        for (int i = 0; i < 10; ++i)
            placeLabeled(*knobs[i], { px + 232 + i * 82 + (i >= 7 ? 14 : 0), py + 332, 78, 142 });
    }

    // Timeline: lanes, the pattern, routing
    const int ty = kTimelineY + 8;
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
        laneTabs_[lane].setBounds(20 + lane * 60, ty, 56, 24);
    laneHandle_.setBounds(262, ty, 120, 24);
    laneLength_.setBounds(388, ty, 78, 24);
    laneWave_.setBounds(470, ty, 112, 24);
    shapesButton_.setBounds(586, ty, 66, 24); // Drawn: shapes; LFO: its rate
    laneRate_.setBounds(586, ty, 76, 24);
    dragBars_.setBounds(702, ty, 76, 24);
    dragPart_.setBounds(782, ty, 96, 24);
    dragAll_.setBounds(882, ty, 78, 24);
    timelineView_.setBounds(20, ty + 30, 940, 90);
    laneEditor_.setBounds(20 + TimelineView::kLabelWidth, ty + 124, 940 - TimelineView::kLabelWidth, 58); // same time axis
    sourceBox_.setBounds(kSlotX + 84, kTimelineY + 8, 120, 24);
    for (int s = 0; s < gristo::kModSlotsPerLane; ++s)
    {
        slots_[s].target.setBounds(kSlotX, kSlotRowY[s], 190, 28);
        slots_[s].amount.setBounds(kSlotX + 196, kSlotRowY[s], 98, 28);
    }

    // Arp and chord blocks (their own coordinates, then moved into the page)
    arpOn_.button.setBounds(28, kModRowY, 56, 26);
    arpMode_.box.setBounds(92, kModRowY, 104, 26);
    for (int v = 0; v < gristo::kNumVoices; ++v)
        arpTriggers_[v].button.setBounds(262 + v * 40, kModRowY, 38, 26);
    arpReset_.box.setBounds(510, kModRowY, 86, 26);
    arpLatch_.button.setBounds(604, kModRowY, 60, 26);
    arpDivide_.label.setBounds(256, kModRowY + 170, 54, 24);
    arpDivide_.box.setBounds(310, kModRowY + 170, 132, 24);
    arpFollow_.button.setBounds(670, kModRowY, 76, 26);
    if (arpChannel_ != nullptr)
        arpChannel_->setBounds(24, kModBottomY - 2, 58, 22);
    const int y1 = kModRowY + 38;
    arpOrder_.label.setBounds(28, y1, 104, 16);
    arpOrder_.box.setBounds(28, y1 + 18, 104, 26);
    arpScale_.label.setBounds(28, y1 + 50, 104, 16);
    arpScale_.box.setBounds(28, y1 + 68, 104, 26);
    arpRootLabel_.setBounds(140, y1, 96, 16);
    if (arpRoot_ != nullptr)
        arpRoot_->setBounds(140, y1 + 18, 96, 26);
    arpShiftLabel_.setBounds(140, y1 + 50, 96, 16);
    if (arpShift_ != nullptr)
        arpShift_->setBounds(140, y1 + 68, 96, 26);
    arpOutLabel_.setBounds(88, kModBottomY - 2, 726, 22);
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
        arpLaneHandles_[lane].setBounds(820 + lane * 38, kModBottomY - 1, 36, 20);
    {
        Knob* knobs[] = { &arpOctaves_, &arpGate_, &arpLength_, &arpDensity_, &arpAccent_, &arpSlide_, &arpMutate_, &arpProb_ };
        for (int i = 0; i < 8; ++i)
            placeKnob(*knobs[i], { 244 + i * 56, y1, 56, 100 });
    }
    arpNew_.setBounds(700, y1 + 2, 56, 26);
    placeKnob(arpProbRandom_, { 700, y1 + 34, 56, 66 });
    arpView_.setBounds(764, kModRowY, 208, 162);
    if (arpVelocity_ != nullptr)
        arpVelocity_->setBounds(820, kModRowY + 170, 152, 24);
    // Chords page
    {
        const int y = kModRowY;
        chordOn_.button.setBounds(28, y, 48, 26);
        chordKey_.box.setBounds(82, y, 58, 26);
        chordScale_.box.setBounds(146, y, 118, 26);
        chordClock_.box.setBounds(270, y, 98, 26);
        if (chordHits_ != nullptr)
            chordHits_->setBounds(374, y + 1, 78, 24);
        chordClockVoice_.box.setBounds(458, y, 84, 26);
        chordReset_.box.setBounds(548, y, 80, 26);
        chordPlay_.box.setBounds(634, y, 80, 26);
        chordOctave_.box.setBounds(720, y, 58, 26);
        chordLibrary_.setBounds(784, y, 70, 26);
        chordNew_.setBounds(860, y, 50, 26);
        chordSlots_.setBounds(24, y + 32, 840, 48);
        placeSmallKnob(chordMutate_, 874, y + 32);
        placeSmallKnob(chordGate_, 922, y + 32);
        placeSmallKnob(chordStrum_, 970, y + 32);
        chordVoicing_.label.setBounds(170, kModBottomY + 32, 52, 24);
        chordVoicing_.box.setBounds(222, kModBottomY + 32, 136, 24);
        chordStrumDir_.label.setBounds(372, kModBottomY + 32, 44, 24);
        chordStrumDir_.box.setBounds(416, kModBottomY + 32, 104, 24);
        chordPads_.setBounds(24, y + 88, 420, 104);
        for (int t = 0; t < static_cast<int>(gristo::ChordType::Count); ++t)
            chordTypes_[t].setBounds(456 + t * 58, y + 88, 54, 48);
        const int y2 = y + 144;
        for (int i = 0; i < 3; ++i)
        {
            chordInversions_[i].setBounds(456 + i * 46, y2, 42, 48);
            chordLengths_[i].setBounds(596 + i * 52, y2, 48, 48);
        }
        chordQuality_.setBounds(752, y2 + 9, 96, 30);
        chordFlat_.setBounds(854, y2, 30, 48);
        chordSharp_.setBounds(888, y2, 30, 48);
        chordClear_.setBounds(924, y2, 50, 48);
        for (int v = 0; v < gristo::kNumVoices; ++v)
            chordStabs_[v].button.setBounds(80 + v * 36, kModBottomY, 34, 22);
        if (chordVelocity_ != nullptr)
            chordVelocity_->setBounds(304, kModBottomY, 140, 22);
        if (chordChannel_ != nullptr)
            chordChannel_->setBounds(452, kModBottomY, 54, 22);
        if (chordShift_ != nullptr)
            chordShift_->setBounds(512, kModBottomY, 66, 22);
        chordOutLabel_.setBounds(584, kModBottomY, 392, 22);
    }
    for (auto* c : arpPageComponents())
        if (c != nullptr)
            c->setBounds(c->getBounds().translated(kBlockDx, 0));
    arpView_.setBounds(arpView_.getBounds().withRight(kPageX + kPageW - 12)); // the note view takes the spare width
    for (auto* c : chordPageComponents())
        if (c != nullptr)
            c->setBounds(c->getBounds().translated(kBlockDx, 0));
}

// ---------------------------------------------------------------------------------
// Editor: hosts the view and scales it

GristoEditor::GristoEditor(GristoProcessor& proc)
    : AudioProcessorEditor(proc), processor_(proc), view_(proc),
      keyboard_(proc.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    addAndMakeVisible(view_);
    view_.onScaleChosen = [this](float s) {
        processor_.uiScale.store(s);
        applyScale(s);
    };
    view_.onKeysToggled = [this] {
        if (kIsMobile)
            resized();
        else
            applyScale(scale_);
    };

    keyboard_.setAvailableRange(24, 96);
    keyboard_.setLowestVisibleKey(36);
    keyboard_.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffe9e4d8));
    keyboard_.setColour(juce::MidiKeyboardComponent::blackNoteColourId, colours::panel);
    keyboard_.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::accent.withAlpha(0.7f));
    keyboard_.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::accent.withAlpha(0.25f));
    keyboard_.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, colours::outline);
    keyboard_.setWantsKeyboardFocus(false);
    addChildComponent(keyboard_);
    addChildComponent(scrollBar_);
    scrollBar_.addListener(this);
    view_.onSettingsRequested = [this] { showSettings(); };

    if (kIsMobile)
    {
        // The standalone window resizes us to the screen; the view scales to fit.
        setSize(GristoView::kWidth, GristoView::kHeight);
        return;
    }
    float scale = processor_.uiScale.load();
    if (scale <= 0.0f)
    {
        // First open: the largest size that fits the screen (but not above 100 %).
        scale = 1.0f;
        if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            const float fit = std::min((static_cast<float>(display->userArea.getHeight()) - 90.0f) / static_cast<float>(GristoView::kHeight),
                                       (static_cast<float>(display->userArea.getWidth()) - 20.0f) / static_cast<float>(GristoView::kWidth));
            scale = kScales[0];
            for (float sc : kScales)
                if (sc <= std::min(1.0f, fit))
                    scale = sc;
        }
    }
    applyScale(scale);
}

GristoEditor::~GristoEditor()
{
    scrollBar_.removeListener(this);
}

int GristoEditor::keysHeight() const
{
    if (!processor_.showKeys.load())
        return 0;
    return kIsMobile ? 96 : juce::roundToInt(84.0f * scale_);
}

void GristoEditor::applyScale(float scale)
{
    scale_ = scale;
    view_.setScaleLabel(scale);
    setSize(juce::roundToInt(GristoView::kWidth * scale), juce::roundToInt(GristoView::kHeight * scale) + keysHeight());
    resized();
}

void GristoEditor::paint(juce::Graphics& g) { g.fillAll(colours::background); }

void GristoEditor::resized()
{
    const int keys = keysHeight();
    keyboard_.setVisible(keys > 0);
    auto area = getLocalBounds();
    if (keys > 0)
    {
        auto strip = area.removeFromBottom(keys).reduced(4, 2);
        keyboard_.setBounds(strip);
        // Four octaves across the strip.
        keyboard_.setKeyWidth(static_cast<float>(strip.getWidth()) / 29.0f);
    }
    if (settings_ != nullptr)
        settings_->setBounds(getLocalBounds());
    if (kIsMobile) // fit the whole view on the screen
        scale_ = std::min(static_cast<float>(area.getWidth()) / static_cast<float>(GristoView::kWidth),
                          static_cast<float>(area.getHeight()) / static_cast<float>(GristoView::kHeight));
    viewArea_ = area;
    updateViewTransform();
}

void GristoEditor::mouseEnter(const juce::MouseEvent& e)
{
    view_.showHelp(e.originalComponent);
}

namespace {
// Full-screen audio / MIDI settings with a big Done button (on mobile the standalone's
// own dialog has no way to close).
class SettingsOverlay : public juce::Component
{
public:
    SettingsOverlay(juce::AudioDeviceManager& manager, std::function<void()> onDone)
        : selector_(manager, 0, 0, 1, 2, true, false, true, false), done_("Done")
    {
        addAndMakeVisible(selector_);
        addAndMakeVisible(done_);
        done_.setColour(juce::TextButton::buttonColourId, theme::colours::accent);
        done_.setColour(juce::TextButton::textColourOffId, theme::colours::background);
        done_.onClick = std::move(onDone);
        title_.setText("Audio / MIDI settings", juce::dontSendNotification);
        title_.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        title_.setColour(juce::Label::textColourId, theme::colours::text);
        addAndMakeVisible(title_);
    }
    void paint(juce::Graphics& g) override { g.fillAll(theme::colours::background.withAlpha(0.97f)); }
    void resized() override
    {
        auto r = getLocalBounds().reduced(16);
        auto top = r.removeFromTop(48);
        done_.setBounds(top.removeFromRight(140).reduced(0, 2));
        title_.setBounds(top);
        r.removeFromTop(8);
        selector_.setBounds(r.withWidth(std::min(r.getWidth(), 640)).withX(r.getCentreX() - std::min(r.getWidth(), 640) / 2));
    }

private:
    juce::AudioDeviceSelectorComponent selector_;
    juce::TextButton done_;
    juce::Label title_;
};
} // namespace

void GristoEditor::showSettings()
{
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
        return;
    juce::Component::SafePointer<GristoEditor> safe(this);
    settings_ = std::make_unique<SettingsOverlay>(holder->deviceManager, [safe] {
        if (safe == nullptr)
            return;
        if (auto* h = juce::StandalonePluginHolder::getInstance())
            h->saveAudioDeviceState();
        juce::MessageManager::callAsync([safe] {
            if (safe != nullptr)
                safe->settings_.reset();
        });
    });
    addAndMakeVisible(*settings_);
    settings_->setBounds(getLocalBounds());
}

void GristoEditor::scrollBarMoved(juce::ScrollBar*, double newRangeStart)
{
    scrollY_ = newRangeStart;
    updateViewTransform();
}

void GristoEditor::updateViewTransform()
{
    const float x = static_cast<float>(viewArea_.getX())
                    + std::max(0.0f, (static_cast<float>(viewArea_.getWidth()) - GristoView::kWidth * scale_) * 0.5f);
    const float y = std::max(0.0f, (static_cast<float>(viewArea_.getHeight()) - GristoView::kHeight * scale_) * 0.5f);
    view_.setTransform(juce::AffineTransform::scale(scale_).translated(x, y - static_cast<float>(scrollY_ * scale_)));
}
