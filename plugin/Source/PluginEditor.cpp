#include "PluginEditor.h"

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace p = gristo::params;
namespace mod = gristo::mod;
using namespace theme;

namespace {

// Voices matrix geometry
constexpr int kVoiceX0 = 452;
constexpr int kVoiceColW = 88;
constexpr int kVoiceRowY[] = { 340, 384, 428, 472, 516, 560 };
constexpr int kVoiceRowH = 44;
constexpr int kVelocityRowY = 608;
constexpr int kNoteRowY = 636;
constexpr int kMuteRowY = 664;
const char* const kVoiceRowNames[] = { "Fill", "Random", "Probability", "Random", "Humanize", "Dynamics" };

// Modulation panel geometry
#if JUCE_ANDROID || JUCE_IOS || GRISTO_SIMULATE_MOBILE
constexpr bool kIsMobile = true;
#else
constexpr bool kIsMobile = false;
#endif
constexpr int kModY = 868;
constexpr int kSlotX = 664;
constexpr int kModRowY = kModY + 34;      // lane tabs / arp first row
constexpr int kSlotRowY[] = { kModY + 54, kModY + 92, kModY + 130, kModY + 168 };
constexpr int kSoundRowY[] = { kModY + 32, kModY + 78, kModY + 124, kModY + 170 }; // kit, arp 1, arp 2, chords
constexpr int kSoundKnobX = 212, kSoundKnobStep = 48;
const char* const kTriggerNames[] = { "BD", "SD", "H1", "H2", "P1", "P2" };

const float kScales[] = { 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.25f };

} // namespace

// ---------------------------------------------------------------------------------
// View

GristoView::GristoView(GristoProcessor& proc)
    : processor_(proc), mapPad_(proc.state, previewEngine_.map()), patternView_(previewEngine_), arpView_(proc), laneEditor_(proc)
{
    setLookAndFeel(&lookAndFeel_);

    addAndMakeVisible(mapPad_);
    addAndMakeVisible(patternView_);
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
        velocityRanges_[v] = std::make_unique<VelocityRange>(*proc.state.getParameter(p::voiceId(p::velMin, v)),
                                                             *proc.state.getParameter(p::voiceId(p::velMax, v)), voiceColours[v]);
        addAndMakeVisible(*velocityRanges_[v]);
        notes_[v] = std::make_unique<NoteSelector>(*proc.state.getParameter(p::voiceId(p::note, v)));
        addAndMakeVisible(*notes_[v]);
        setupToggle(mutes_[v], p::voiceId(p::mute, v), "Mute");
    }
    setupChoice(timing_, p::timing, "Timing");
    setupKnob(shuffle_, p::shuffle, "Shuffle");
    setupChoice(feeling_, p::feeling, "Feeling");
    setupKnob(feelMs_, p::feelMs, "Flam ms");
    setupRandomKnob(shuffleRandom_, p::shuffleRandom, "Random");
    setupRandomKnob(feelRandom_, p::feelRandom, "Random");
    setupKnob(humanize_, p::humanize, "Humanize");
    setupKnob(dynamics_, p::dynamics, "Dynamics");
    setupToggle(lock_, p::humanizeLock, "Lock");
    setupToggle(linear_, p::linear, "Linear");
    setupToggle(kitOn_, p::kitOn, "Sound");
    setupKnob(kitGain_, p::kitGain, "Level");
    setupToggle(freeRun_, p::freeRun, "Free Run");
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
        tab.onClick = [this, pg] { showPage(pg); };
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
        setHelp(voiceDynamics_[v], name + " Dynamics", "Scales the master Dynamics for this voice: how much softer its weaker notes are.");
        setHelp(*velocityRanges_[v], name + " Velocity", "MIDI velocity range. Accents play at the top, ghost notes near the bottom. Drag the ends; double-click = 1-127.");
        setHelp(*notes_[v], name + " Note", "MIDI note this voice sends. Defaults are GM three octaves down (Pitch +36 in Live restores GM). Click for a list, scroll to step.");
        setHelp(mutes_[v].button, name + " Mute", "Silence this voice. In Linear mode its slots go to the next voice in line.");
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
    pageTabs_[0].setToggleState(true, juce::dontSendNotification);
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
                repaint(406, 4, 440, 42);
                if (onHelpChanged)
                    onHelpChanged();
            }
            return;
        }
    }
}

void GristoView::mouseMove(const juce::MouseEvent& e) { showHelpFor(e.originalComponent); }
void GristoView::mouseEnter(const juce::MouseEvent& e) { showHelpFor(e.originalComponent); }

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

void GristoView::assignLane(int lane, int target)
{
    auto targetOf = [&](int slot) {
        return static_cast<int>(processor_.state.getRawParameterValue(p::slotId(p::modTarget, lane, slot))->load());
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
    const float amount = processor_.state.getRawParameterValue(p::slotId(p::modAmount, lane, slot))->load();
    setParameter(p::slotId(p::modTarget, lane, slot), static_cast<float>(target));
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
        else if (target >= mod::kArpBase && target < mod::kArpCutoffBase && (target - mod::kArpBase) % mod::kArpTargetsPerArp == mod::kArpVelocity)
            start = -0.5f;
        else if (target >= mod::kArpBase && target < mod::kArpCutoffBase && (target - mod::kArpBase) % mod::kArpTargetsPerArp == mod::kArpTranspose)
            start = 7.0f / 12.0f; // up a fifth
        setParameter(p::slotId(p::modAmount, lane, slot), start);
    }
    laneTabs_[lane].setToggleState(true, juce::dontSendNotification);
    selectLane(lane);
}

void GristoView::selectLane(int lane)
{
    selectedLane_ = lane;
    laneEditor_.setLane(lane);
    laneHandle_.setLane(lane);
    laneLengthAttachment_.reset();
    laneLengthAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.state, p::laneId(p::modBars, lane), laneLength_);
    for (int s = 0; s < gristo::kModSlotsPerLane; ++s)
    {
        auto& slot = slots_[s];
        slot.targetAttachment.reset();
        slot.amountAttachment.reset();
        slot.targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor_.state, p::slotId(p::modTarget, lane, s), slot.target);
        slot.amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor_.state, p::slotId(p::modAmount, lane, s), slot.amount);
        slot.amount.setColour(juce::Slider::trackColourId, laneColours[lane]);
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
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
        for (const auto& slot : params.modulation.lanes[static_cast<size_t>(lane)].slots)
        {
            if (slot.target <= mod::kNone || slot.target >= mod::kNumTargets || std::abs(slot.amount) < 1.0e-6f)
                continue;
            const auto colour = laneColours[lane];
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
            show(mod::arpCutoffTarget(a), juce::jlimit(0.0, 1.0, synthRows_[a].cutoff.slider.getValue() + ep.arps[static_cast<size_t>(a)].cutoffOffset));
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
            pm.loadFile(files[result - 2000]);
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
    mapPad_.repaint();
    nodeLabel_.setText("Near: " + mapPad_.nearestNodeName(), juce::dontSendNotification);
    patternView_.setPlayhead(processor_.displayPpq.load(), processor_.displayBpm.load(), processor_.displayPlaying.load());
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
    // land and to warn when they collide with the drums or the other arp.
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
    const int other = 1 - arp;
    int otherLo = 0, otherHi = 0;
    if (params.arps[static_cast<size_t>(other)].enabled && outRange(other, otherLo, otherHi) && otherLo <= hi && otherHi >= lo)
        clashes.add("Arp " + juce::String(other + 1));
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
    setHelp(soundKit_.box, "Drum kit", "Classic, 808, Trap, Lo-fi, Minimal or Reggaeton. All synthesized; the knobs shape the whole kit.");
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
        juce::String lastCategory;
        for (int i = 0; i < static_cast<int>(gristo::sound::SynthSound::Count); ++i)
        {
            const auto sound = static_cast<gristo::sound::SynthSound>(i);
            const juce::String category = gristo::sound::synthCategoryName(sound);
            if (category != lastCategory)
                row.sound.addSectionHeading(category);
            lastCategory = category;
            row.sound.addItem(gristo::sound::synthSoundName(sound), i + 1);
        }
        addAndMakeVisible(row.sound);
        row.soundAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor_.state, p::laneId(p::synthSound, a), row.sound);
        const juce::String name = a == gristo::sound::kChordSynth ? juce::String("Chords ") : "Arp " + juce::String(a + 1) + " ";
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
        if (a < gristo::kNumArps)
        {
            makeTarget(row.cutoff.slider, row.cutoff.slider, mod::arpCutoffTarget(a));
            targetSliders_[static_cast<size_t>(mod::arpCutoffTarget(a))] = &row.cutoff.slider;
        }
    }

    setupSmallKnob(fxSize_, p::fxSize, "Size");
    setupSmallKnob(fxFeedback_, p::fxFeedback, "Feedback");
    setupChoice(fxTime_, p::fxTime, "Delay");
    fxTime_.label.setFont(juce::FontOptions(10.5f));
    setHelp(fxSize_, "Reverb Size", "Small room to big hall.");
    setHelp(fxFeedback_, "Delay Feedback", "How many echoes. The delay ping-pongs left and right and follows the tempo.");
    setHelp(fxTime_.box, "Delay Time", "Echo spacing in note values, synced to the tempo.");

    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
    {
        soundLaneHandles_[lane].setLane(lane);
        soundLaneHandles_[lane].setCompact(true);
        addAndMakeVisible(soundLaneHandles_[lane]);
        setHelp(soundLaneHandles_[lane], "Lane " + juce::String(lane + 1),
                "Drag onto an arp's Cutoff knob for filter sweeps. Draw the curve on the Modulation tab.");
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
    chordOutLabel_.setFont(juce::FontOptions(11.0f));
    chordOutLabel_.setColour(juce::Label::textColourId, colours::dim);

    for (int t = 0; t < static_cast<int>(gristo::ChordType::Count); ++t)
    {
        chordTypes_[t].setButtonText(gristo::chordTypeName(static_cast<gristo::ChordType>(t)));
        chordTypes_[t].onClick = [this, t] { editSelectedChord([t](gristo::ChordSlot& s) { s.type = static_cast<gristo::ChordType>(t); }); };
        addAndMakeVisible(chordTypes_[t]);
        setHelp(chordTypes_[t], "Chord type", "Triad, 7th, 9th or suspended, built from the scale. Applies to the selected slot.");
    }
    const char* inversionNames[] = { "Root", "1st", "2nd" };
    const char* lengthNames[] = { "1/2 bar", "1 bar", "2 bars" };
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
    for (auto* b : { &chordTypes_[0], &chordTypes_[1], &chordTypes_[2], &chordTypes_[3], &chordTypes_[4], &chordInversions_[0],
                     &chordInversions_[1], &chordInversions_[2], &chordLengths_[0], &chordLengths_[1], &chordLengths_[2] })
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
        });
        // Filling the progression left to right: move on to the next slot.
        selectedChordSlot_ = std::min(selectedChordSlot_ + 1, gristo::kMaxChordSlots - 1);
    };

    setHelp(chordOn_.button, "Chords on", "Switch the chord sequencer on. Arps with Chords on their page follow it.");
    setHelp(chordKey_.box, "Key", "The key's tonic.");
    setHelp(chordScale_.box, "Scale", "The scale the chords are built from. Borrowed chords come from the parallel major / minor.");
    setHelp(chordClock_.box, "Clock", "Bars: each chord lasts its length. Drum hits: the chord changes every N hits of a drum voice.");
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
                                         &chordGate_.slider, &chordGate_.label };
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
    if (!slot.used)
        return; // an empty slot's other settings don't matter
    set(p::voiceId(p::chordDegree, index), static_cast<float>(slot.degree));
    set(p::voiceId(p::chordBorrowed, index), slot.borrowed ? 1.0f : 0.0f);
    set(p::voiceId(p::chordType, index), static_cast<float>(slot.type));
    set(p::voiceId(p::chordInversion, index), static_cast<float>(slot.inversion));
    set(p::voiceId(p::chordLength, index), slot.lengthSteps <= 8 ? 0.0f : slot.lengthSteps >= 32 ? 2.0f : 1.0f);
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
        for (Knob* k : { &row.cutoff, &row.resonance, &row.env, &row.attack, &row.release, &row.glide, &row.level, &row.reverb, &row.delay })
            addKnob(*k);
    }
    for (auto& h : soundLaneHandles_)
        list.push_back(&h);
    return list;
}

std::vector<juce::Component*> GristoView::arpPageComponents()
{
    std::vector<juce::Component*> list { &arpOn_.button, &arpLatch_.button, &arpMode_.box, &arpReset_.box, &arpOrder_.box,
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

void GristoView::showPage(int page)
{
    page_ = page;
    if (isArpPage())
        bindArp(page - 1);
    for (auto* c : modulationPageComponents())
        c->setVisible(page == 0);
    for (auto* c : arpPageComponents())
        if (c != nullptr)
            c->setVisible(isArpPage());
    for (auto* c : soundPageComponents())
        c->setVisible(page == kSoundsPage);
    for (auto* c : chordPageComponents())
        if (c != nullptr)
            c->setVisible(page == kChordsPage);
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
    setHelp(arpShiftLabel_, "Out Shift", "Moves this arp's MIDI out into its own note range, so Live can split it from the drums and the other arp by key zone.");
    setHelp(arpOutLabel_, "MIDI out range", "Where this arp's notes land, and how to set up the Instrument Rack chain in Live. Red = it overlaps the drums or the other arp.");
    setHelp(arpRootLabel_, "Root", "Generate mode: key root and lowest note when no notes are held.");
    setHelp(arpOctaves_, "Octaves", "How many octaves the notes are spread over.");
    setHelp(arpGate_, "Gate", "Note length in 16ths. Slides hold until the next note.");
    setHelp(arpLength_, "Length", "Generate: number of steps in the pattern before it repeats (within the reset).");
    setHelp(arpDensity_, "Density", "Generate: share of steps that play. Lower = more rests, even if the drum is busy.");
    setHelp(arpAccent_, "Accent", "Generate: share of steps played at full velocity.");
    setHelp(arpSlide_, "Slide", "Generate: share of steps that overlap into the next note (glide on a mono synth).");
    setHelp(arpMutate_, "Mutate", "Generate: share of steps re-rolled at every reset, so the pattern slowly evolves.");
    setHelp(arpNew_, "New pattern", "Roll a new generated pattern. It is saved with the project.");
    setHelp(arpProb_, "Probability", "Chance each step plays. Skipped steps still move the arp on, so the pattern keeps its place.");
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
    auto header = getLocalBounds().removeFromTop(48).reduced(16, 0);
    g.setColour(colours::text);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("GRISTO", header.removeFromLeft(120), juce::Justification::centredLeft);
    g.setColour(colours::dim);
    g.setFont(juce::FontOptions(11.0f));

    // Help box
    const juce::Rectangle<int> help = kIsMobile ? juce::Rectangle<int> { 466, 6, 378, 38 } : juce::Rectangle<int> { 412, 6, 432, 38 };
    g.setColour(colours::panel);
    g.fillRoundedRectangle(help.toFloat(), 6.0f);
    g.setColour(colours::outline);
    g.drawRoundedRectangle(help.toFloat().reduced(0.5f), 6.0f, 1.0f);
    auto helpArea = help.reduced(10, 3);
    g.setColour(colours::accent);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText(helpTitle_, helpArea.removeFromTop(15), juce::Justification::centredLeft);
    g.setColour(colours::text);
    g.setFont(juce::FontOptions(11.5f));
    g.drawFittedText(helpText_, helpArea, juce::Justification::topLeft, 1, 0.8f);

    drawPanel(g, { 16, 52, 344, 508 }, "Map");
    drawPanel(g, { 16, 568, 344, 148 }, "Humanize / Dynamics");
    drawPanel(g, { 372, 52, 612, 248 }, "Pattern");
    drawPanel(g, { 372, 308, 612, 408 }, "Voices");
    drawPanel(g, { 16, 724, 544, 136 }, "Groove");
    drawPanel(g, { 572, 724, 412, 136 }, "Output");
    drawPanel(g, { 16, kModY, 968, 216 }, {});

    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        g.setColour(voiceColours[v]);
        g.drawText(gristo::voiceName(v), kVoiceX0 + v * kVoiceColW, 322, kVoiceColW, 16, juce::Justification::centred);
    }
    g.setFont(juce::FontOptions(11.5f));
    for (int r = 0; r < 6; ++r)
    {
        g.setColour((r == 1 || r == 3) ? colours::random : colours::dim);
        g.drawText(kVoiceRowNames[r], 382, kVoiceRowY[r] + 14, 68, 16, juce::Justification::centredLeft);
        if (r > 0)
        {
            g.setColour(colours::outline.withAlpha(0.35f));
            g.drawHorizontalLine(kVoiceRowY[r] - 1, 382.0f, 974.0f);
        }
    }
    g.setColour(colours::dim);
    g.drawText("Velocity", 382, kVelocityRowY + 3, 68, 16, juce::Justification::centredLeft);
    g.drawText("Note", 382, kNoteRowY + 3, 68, 16, juce::Justification::centredLeft);
    g.drawText("Mute / Ch", 382, kMuteRowY + 5, 68, 16, juce::Justification::centredLeft);

    if (page_ == 0)
    {
        g.setColour(colours::dim);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("ROUTING  (LANE " + juce::String(selectedLane_ + 1) + ")", kSlotX, kModRowY + 4, 300, 14, juce::Justification::centredLeft);
        g.setColour(laneColours[selectedLane_]);
        g.fillRoundedRectangle(juce::Rectangle<float>(static_cast<float>(kSlotX) - 8.0f, static_cast<float>(kSlotRowY[0]), 3.0f, 142.0f), 1.5f);
    }
    else if (page_ == kChordsPage)
    {
        g.setColour(colours::dim);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("Stab on", 28, kModY + 190, 50, 22, juce::Justification::centredLeft);
    }
    else if (page_ == kSoundsPage)
    {
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        const juce::String names[] = { "KIT", "ARP 1", "ARP 2", "CHORDS" };
        const juce::Colour rowColours[] = { colours::accent, arpColours[0], arpColours[1], colours::random };
        for (int row = 0; row < 4; ++row)
        {
            g.setColour(rowColours[row]);
            g.drawText(names[row], 28, kSoundRowY[row] + 8, 50, 16, juce::Justification::centredLeft);
        }
        g.setColour(colours::dim);
        g.drawText("FX", 812, kSoundRowY[1] - 2, 60, 12, juce::Justification::centredLeft);
        g.setColour(colours::outline);
        g.drawVerticalLine(800, static_cast<float>(kSoundRowY[0]), static_cast<float>(kSoundRowY[3] + 44));
    }
    else
    {
        g.setColour(colours::dim);
        g.setFont(juce::FontOptions(11.5f));
        g.drawText("Triggers", 204, kModRowY, 56, 26, juce::Justification::centredLeft);
        g.drawText("Velocity", 764, kModRowY + 124, 56, 24, juce::Justification::centredLeft);
        g.setColour(arpColours[page_ - 1]);
        g.fillRoundedRectangle(juce::Rectangle<float>(20.0f, static_cast<float>(kModRowY), 3.0f, 160.0f), 1.5f);
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
    repaint(406, 4, 440, 42);
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
    if (!headerExternal_)
    {
        sizeButton_.setBounds(904, 12, 80, 26);
        keysButton_.setBounds(850, 12, 50, 26);
        presetPrev_.setBounds(140, 12, 26, 26);
        presetName_.setBounds(168, 12, 160, 26);
        presetNext_.setBounds(330, 12, 26, 26);
        presetSave_.setBounds(358, 12, 46, 26);
        presetShare_.setBounds(408, 12, 52, 26);
    }

    // Left column: map, then the humanize / dynamics masters
    mapPad_.setBounds(28, 78, 320, 320);
    nodeLabel_.setBounds(28, 404, 320, 20);
    placeKnob(chaos_, { 28, 436, 104, 112 });
    placeKnob(mapRandom_, { 136, 448, 104, 96 });
    placeKnob(variation_, { 244, 436, 104, 112 });

    placeKnob(humanize_, { 28, 592, 96, 104 });
    placeKnob(dynamics_, { 128, 592, 96, 104 });
    lock_.button.setBounds(236, 610, 104, 26);
    linear_.button.setBounds(236, 652, 104, 26);

    patternView_.setBounds(384, 74, 588, 216);

    // Voices matrix: one column per voice, one row per parameter
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        const int x = kVoiceX0 + v * kVoiceColW;
        Knob* rows[] = { &fills_[v], &fillRandoms_[v], &probabilities_[v], &probabilityRandoms_[v], &voiceHumanize_[v], &voiceDynamics_[v] };
        for (int r = 0; r < 6; ++r)
            rows[r]->slider.setBounds(x + 2, kVoiceRowY[r], kVoiceColW - 4, kVoiceRowH);
        velocityRanges_[v]->setBounds(x + 4, kVelocityRowY, kVoiceColW - 8, 24);
        notes_[v]->setBounds(x + 6, kNoteRowY, kVoiceColW - 12, 22);
        mutes_[v].button.setBounds(x + 2, kMuteRowY, 46, 26);
        channels_[v]->setBounds(x + 50, kMuteRowY + 2, kVoiceColW - 54, 22);
    }

    // Groove
    timing_.label.setBounds(28, 744, 80, 16);
    timing_.box.setBounds(28, 762, 100, 26);
    feeling_.label.setBounds(28, 796, 80, 16);
    feeling_.box.setBounds(28, 814, 100, 26);
    placeKnob(shuffle_, { 150, 744, 90, 104 });
    placeKnob(shuffleRandom_, { 238, 764, 64, 84 });
    placeKnob(feelMs_, { 320, 744, 90, 104 });
    placeKnob(feelRandom_, { 408, 764, 64, 84 });

    // Output
    kitOn_.button.setBounds(584, 764, 72, 26);
    freeRun_.button.setBounds(584, 806, 72, 26);
    placeKnob(kitGain_, { 680, 744, 90, 104 });
    placeKnob(freeTempo_, { 776, 744, 90, 104 });
    placeKnob(noteLength_, { 872, 744, 90, 104 });

    // Modulation
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
        laneTabs_[lane].setBounds(28 + lane * 68, kModRowY, 64, 24);
    laneHandle_.setBounds(304, kModRowY, 170, 24);
    laneLength_.setBounds(482, kModRowY, 84, 24);
    shapesButton_.setBounds(572, kModRowY, 76, 24);
    laneEditor_.setBounds(28, kModRowY + 30, 620, 146);

    // Page tabs and the arp page
    for (int pg = 0; pg <= kSoundsPage; ++pg)
        pageTabs_[pg].setBounds(24 + pg * 96, kModY + 6, 92, 22);
    arpOn_.button.setBounds(28, kModRowY, 56, 26);
    arpMode_.box.setBounds(92, kModRowY, 104, 26);
    for (int v = 0; v < gristo::kNumVoices; ++v)
        arpTriggers_[v].button.setBounds(262 + v * 40, kModRowY, 38, 26);
    arpReset_.box.setBounds(510, kModRowY, 86, 26);
    arpLatch_.button.setBounds(604, kModRowY, 60, 26);
    arpFollow_.button.setBounds(670, kModRowY, 76, 26);
    if (arpChannel_ != nullptr)
        arpChannel_->setBounds(24, kModY + 188, 58, 22);
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
    arpOutLabel_.setBounds(88, kModY + 188, 726, 22);
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
        arpLaneHandles_[lane].setBounds(820 + lane * 38, kModY + 189, 36, 20);
    {
        Knob* knobs[] = { &arpOctaves_, &arpGate_, &arpLength_, &arpDensity_, &arpAccent_, &arpSlide_, &arpMutate_, &arpProb_ };
        for (int i = 0; i < 8; ++i)
            placeKnob(*knobs[i], { 244 + i * 56, y1, 56, 100 });
    }
    arpNew_.setBounds(700, y1 + 2, 56, 26);
    placeKnob(arpProbRandom_, { 700, y1 + 34, 56, 66 });
    arpView_.setBounds(764, kModRowY, 208, 116);
    if (arpVelocity_ != nullptr)
        arpVelocity_->setBounds(820, kModRowY + 124, 152, 24);
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
        chordPads_.setBounds(24, y + 86, 500, 74);
        for (int t = 0; t < static_cast<int>(gristo::ChordType::Count); ++t)
            chordTypes_[t].setBounds(536 + t * 52, y + 86, 48, 34);
        for (int i = 0; i < 3; ++i)
        {
            chordInversions_[i].setBounds(536 + i * 52, y + 126, 48, 34);
            chordLengths_[i].setBounds(696 + i * 58, y + 126, 54, 34);
        }
        chordClear_.setBounds(880, y + 126, 64, 34);
        for (int v = 0; v < gristo::kNumVoices; ++v)
            chordStabs_[v].button.setBounds(80 + v * 36, kModY + 190, 34, 22);
        if (chordVelocity_ != nullptr)
            chordVelocity_->setBounds(304, kModY + 190, 140, 22);
        if (chordChannel_ != nullptr)
            chordChannel_->setBounds(452, kModY + 190, 54, 22);
        if (chordShift_ != nullptr)
            chordShift_->setBounds(512, kModY + 190, 66, 22);
        chordOutLabel_.setBounds(584, kModY + 190, 392, 22);
    }
    // Sounds page
    {
        const int y0 = kSoundRowY[0];
        soundKit_.box.setBounds(80, y0 + 4, 120, 26);
        Knob* kitKnobs[] = { &kitTune_, &kitDecay_, &kitTone_, &kitDrive_ };
        for (int i = 0; i < 4; ++i)
            placeSmallKnob(*kitKnobs[i], kSoundKnobX + i * kSoundKnobStep, y0);
        for (int v = 0; v < gristo::kNumVoices; ++v)
            placeSmallKnob(drumLevels_[v], 420 + v * 44, y0);
        placeSmallKnob(kitReverb_, 696, y0);
        placeSmallKnob(kitDelay_, 744, y0);
        for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
            soundLaneHandles_[lane].setBounds(812 + lane * 40, y0 + 8, 36, 20);
        for (int a = 0; a < gristo::sound::kNumSynths; ++a)
        {
            auto& row = synthRows_[a];
            const int y = kSoundRowY[1 + a];
            row.sound.setBounds(80, y + 4, 120, 26);
            Knob* knobs[] = { &row.cutoff, &row.resonance, &row.env, &row.attack, &row.release, &row.glide, &row.level };
            for (int i = 0; i < 7; ++i)
                placeSmallKnob(*knobs[i], kSoundKnobX + i * kSoundKnobStep, y);
            placeSmallKnob(row.reverb, 696, y);
            placeSmallKnob(row.delay, 744, y);
        }
        const int fy = kSoundRowY[1] + 12;
        placeSmallKnob(fxSize_, 812, fy);
        placeSmallKnob(fxFeedback_, 860, fy);
        fxTime_.label.setBounds(908, fy - 2, 64, 14);
        fxTime_.box.setBounds(908, fy + 12, 66, 24);
    }
    for (int s = 0; s < gristo::kModSlotsPerLane; ++s)
    {
        slots_[s].target.setBounds(kSlotX, kSlotRowY[s], 196, 28);
        slots_[s].amount.setBounds(kSlotX + 204, kSlotRowY[s], 104, 28);
    }
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

    scrollBar_.setAutoHide(false);
    scrollBar_.addListener(this);
    scrollBar_.setColour(juce::ScrollBar::thumbColourId, colours::accent.withAlpha(0.6f));
    scrollBar_.setColour(juce::ScrollBar::trackColourId, colours::panel);
    addChildComponent(scrollBar_);

    if (kIsMobile)
    {
        // Pages instead of one tall window: the header controls move to a top bar here.
        removeChildComponent(&view_);
        addAndMakeVisible(clip_);
        clip_.addAndMakeVisible(view_);
        view_.setHeaderExternal(true);
        for (auto* c : view_.headerControls())
        {
            addAndMakeVisible(c);
            c->addMouseListener(this, false);
        }
        const char* names[NumMobilePages] = { "Beat", "Groove", "Arps", "Chords", "Sounds" };
        for (int pg = 0; pg < NumMobilePages; ++pg)
        {
            auto& tab = mobileTabs_[pg];
            tab.setButtonText(names[pg]);
            tab.setClickingTogglesState(true);
            tab.setRadioGroupId(5120);
            tab.setColour(juce::TextButton::buttonOnColourId, colours::accent);
            tab.setColour(juce::TextButton::textColourOnId, colours::background);
            tab.onClick = [this, pg] { showMobilePage(pg); };
            addAndMakeVisible(tab);
        }
        for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
        {
            auto& chip = barLanes_[lane];
            chip.setLane(lane);
            chip.setCompact(true);
            chip.onTap = [this](int l) {
                if (view_.armedLane() == l)
                    view_.disarmLane();
                else
                    view_.armLane(l);
            };
            chip.getProperties().set("helpTitle", "Lane " + juce::String(lane + 1));
            chip.getProperties().set("helpText", "Drag onto a knob on any page, or tap it and then tap the knob. Draw the curve on the Groove page.");
            chip.addMouseListener(this, false);
            addAndMakeVisible(chip);
        }
        view_.onArmChanged = [this] {
            for (auto& chip : barLanes_)
                chip.setArmed(chip.lane() == view_.armedLane());
        };
        view_.onSettingsRequested = [this] { showSettings(); };
        view_.onHelpChanged = [this] { repaint(helpBox_); };
        mobileTabs_[Beat].setToggleState(true, juce::dontSendNotification);
        setSize(1200, 752); // a 16:10 tablet in landscape; the standalone window resizes us to the screen
        return;
    }
    float scale = processor_.uiScale.load();
    if (scale <= 0.0f)
    {
        // First open: the largest size that fits the screen (but not above 100 %).
        scale = 1.0f;
        if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            const float fit = (static_cast<float>(display->userArea.getHeight()) - 90.0f) / static_cast<float>(GristoView::kHeight);
            scale = kScales[0];
            for (float s : kScales)
                if (s <= std::min(1.0f, fit))
                    scale = s;
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

void GristoEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::background);
    if (kIsMobile && !helpBox_.isEmpty())
    {
        g.setColour(colours::panel);
        g.fillRoundedRectangle(helpBox_.toFloat(), 6.0f);
        auto r = helpBox_.reduced(10, 0);
        g.setColour(colours::accent);
        g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
        const auto title = view_.helpTitle();
        const int titleWidth = juce::GlyphArrangement::getStringWidthInt(juce::FontOptions(12.5f, juce::Font::bold), title) + 12;
        g.drawText(title, r.removeFromLeft(titleWidth), juce::Justification::centredLeft);
        g.setColour(colours::text);
        g.setFont(juce::FontOptions(12.0f));
        g.drawFittedText(view_.helpText(), r, juce::Justification::centredLeft, 1, 0.8f);
    }
}

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

    if (kIsMobile)
    {
        // Top bar: presets, lane chips, pages, Keys, Settings; the help line below it.
        auto bar = area.removeFromTop(46).reduced(6, 6);
        auto headers = view_.headerControls(); // prev, name, next, save, share, keys, settings
        headers[0]->setBounds(bar.removeFromLeft(34));
        bar.removeFromLeft(4);
        headers[1]->setBounds(bar.removeFromLeft(150));
        bar.removeFromLeft(4);
        headers[2]->setBounds(bar.removeFromLeft(34));
        bar.removeFromLeft(6);
        headers[3]->setBounds(bar.removeFromLeft(56));
        bar.removeFromLeft(4);
        headers[4]->setBounds(bar.removeFromLeft(60));
        bar.removeFromLeft(14);
        for (auto& chip : barLanes_)
        {
            chip.setBounds(bar.removeFromLeft(40).reduced(0, 3));
            bar.removeFromLeft(4);
        }
        headers[6]->setBounds(bar.removeFromRight(84));
        bar.removeFromRight(6);
        headers[5]->setBounds(bar.removeFromRight(56));
        bar.removeFromRight(10);
        for (int pg = NumMobilePages - 1; pg >= 0; --pg)
        {
            mobileTabs_[pg].setBounds(bar.removeFromRight(74));
            bar.removeFromRight(4);
        }
        helpBox_ = area.removeFromTop(26).reduced(8, 2);
        if (settings_ != nullptr)
            settings_->setBounds(getLocalBounds());

        clip_.setBounds(area);
        const auto region = mobileRegion();
        scale_ = std::min(static_cast<float>(area.getWidth()) / static_cast<float>(region.getWidth()),
                          static_cast<float>(area.getHeight()) / static_cast<float>(region.getHeight()));
        const float x = (static_cast<float>(area.getWidth()) - region.getWidth() * scale_) * 0.5f - region.getX() * scale_;
        const float y = -region.getY() * scale_ + 4.0f;
        view_.setTransform(juce::AffineTransform::scale(scale_).translated(x, y));
        scrollBar_.setVisible(false);
        return;
    }
    viewArea_ = area;
    updateViewTransform();
}

juce::Rectangle<int> GristoEditor::mobileRegion() const
{
    // Regions of the 1000 x 1096 view (see GristoView::paint for the panels).
    if (mobilePage_ == Beat)
        return { 10, 50, 980, 670 };   // map, humanize, pattern, voices
    return { 10, 718, 980, 370 };      // groove + output, and the bottom panel
}

void GristoEditor::showMobilePage(int page)
{
    mobilePage_ = page;
    const int bottom = page == Arps ? 1 : page == Chords ? GristoView::kChordsPageIndex
                     : page == Sounds ? GristoView::kSoundsPageIndex : 0;
    if (page != Beat)
        view_.showBottomPage(bottom);
    resized();
    repaint();
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
    view_.setTransform(juce::AffineTransform::scale(scale_).translated(x, static_cast<float>(-scrollY_ * scale_)));
}
