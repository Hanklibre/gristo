#include "PluginEditor.h"

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
constexpr int kModY = 868;
constexpr int kSlotX = 664;
constexpr int kModRowY = kModY + 34;      // lane tabs / arp first row
constexpr int kSlotRowY[] = { kModY + 54, kModY + 92, kModY + 130, kModY + 168 };
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
    setupToggle(kitOn_, p::kitOn, "Kit");
    setupKnob(kitGain_, p::kitGain, "Kit Level");
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
    for (int pg = 0; pg <= gristo::kNumArps; ++pg)
    {
        auto& tab = pageTabs_[pg];
        tab.setButtonText(pg == 0 ? "Modulation" : "Arp " + juce::String(pg));
        tab.setClickingTogglesState(true);
        tab.setRadioGroupId(4712);
        tab.setColour(juce::TextButton::buttonOnColourId, pg == 0 ? colours::accent : arpColours[pg - 1]);
        tab.setColour(juce::TextButton::textColourOnId, colours::background);
        tab.onClick = [this, pg] { showPage(pg); };
        addAndMakeVisible(tab);
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
    setHelp(kitOn_.button, "Kit", "Built-in preview drums. Turn off when you play your own drums from the MIDI output.");
    setHelp(kitGain_, "Kit Level", "Volume of the built-in preview drums.");
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
                repaint(372, 4, 520, 42);
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
        else if (target >= mod::kArpBase && (target - mod::kArpBase) % mod::kArpTargetsPerArp == mod::kArpVelocity)
            start = -0.5f;
        else if (target >= mod::kArpBase && (target - mod::kArpBase) % mod::kArpTargetsPerArp == mod::kArpTranspose)
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
    const int shownArp = page_ > 0 ? page_ - 1 : -1;
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
        if (shownArp >= 0)
        {
            const auto& ap = ep.arps[static_cast<size_t>(shownArp)];
            show(mod::arpTarget(shownArp, mod::kArpDensity), ap.density);
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
    if (page_ > 0)
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
    if (!clashes.isEmpty())
        text << ".  Overlaps " << clashes.joinIntoString(" and ") << ": change Shift";
    arpOutLabel_.setColour(juce::Label::textColourId, clashes.isEmpty() ? colours::dim : colours::hot);
    arpOutLabel_.setText(text, juce::dontSendNotification);
}

void GristoView::bindArpTargets(int arp)
{
    // The arp page has one set of knobs; route drops and rings to the arp shown.
    for (int a = 0; a < gristo::kNumArps; ++a)
        for (int w = 0; w < mod::kArpTargetsPerArp; ++w)
            targetSliders_[static_cast<size_t>(mod::arpTarget(a, w))] = nullptr;
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
    for (Knob* k : { &arpOctaves_, &arpGate_, &arpLength_, &arpDensity_, &arpAccent_, &arpSlide_, &arpMutate_ })
    {
        list.push_back(&k->slider);
        list.push_back(&k->label);
    }
    return list;
}

void GristoView::showPage(int page)
{
    page_ = page;
    if (page > 0)
        bindArp(page - 1);
    for (auto* c : modulationPageComponents())
        c->setVisible(page == 0);
    for (auto* c : arpPageComponents())
        if (c != nullptr)
            c->setVisible(page > 0);
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
    arpNew_.setButtonText("New");
    arpNew_.onClick = [this] {
        // Roll a new pattern: a different seed for the generator.
        if (page_ > 0)
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
                "Drag onto Density, Accent, Slide, Gate, Octaves, Length, Velocity, or the note view (Transpose). Draw the curve on the Modulation tab.");
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
    g.drawFittedText("topographic drums,\ntripping over themselves", header.removeFromLeft(220), juce::Justification::centredLeft, 2);

    // Help box
    const juce::Rectangle<int> help { 372, 6, 520, 38 };
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

void GristoView::resized()
{
    sizeButton_.setBounds(904, 12, 80, 26);

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
    for (int pg = 0; pg <= gristo::kNumArps; ++pg)
        pageTabs_[pg].setBounds(24 + pg * 96, kModY + 6, 92, 22);
    arpOn_.button.setBounds(28, kModRowY, 56, 26);
    arpMode_.box.setBounds(92, kModRowY, 104, 26);
    for (int v = 0; v < gristo::kNumVoices; ++v)
        arpTriggers_[v].button.setBounds(262 + v * 40, kModRowY, 38, 26);
    arpReset_.box.setBounds(510, kModRowY, 86, 26);
    arpLatch_.button.setBounds(604, kModRowY, 64, 26);
    if (arpChannel_ != nullptr)
        arpChannel_->setBounds(676, kModRowY + 1, 66, 24);
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
    arpOutLabel_.setBounds(24, kModY + 188, 790, 22);
    for (int lane = 0; lane < gristo::kNumModLanes; ++lane)
        arpLaneHandles_[lane].setBounds(820 + lane * 38, kModY + 189, 36, 20);
    placeKnob(arpOctaves_, { 244, y1, 64, 100 });
    placeKnob(arpGate_, { 308, y1, 64, 100 });
    placeKnob(arpLength_, { 372, y1, 64, 100 });
    placeKnob(arpDensity_, { 436, y1, 64, 100 });
    placeKnob(arpAccent_, { 500, y1, 64, 100 });
    placeKnob(arpSlide_, { 564, y1, 64, 100 });
    placeKnob(arpMutate_, { 628, y1, 64, 100 });
    arpNew_.setBounds(700, y1 + 36, 56, 28);
    arpView_.setBounds(764, kModRowY, 208, 116);
    if (arpVelocity_ != nullptr)
        arpVelocity_->setBounds(820, kModRowY + 124, 152, 24);
    for (int s = 0; s < gristo::kModSlotsPerLane; ++s)
    {
        slots_[s].target.setBounds(kSlotX, kSlotRowY[s], 196, 28);
        slots_[s].amount.setBounds(kSlotX + 204, kSlotRowY[s], 104, 28);
    }
}

// ---------------------------------------------------------------------------------
// Editor: hosts the view and scales it

GristoEditor::GristoEditor(GristoProcessor& proc) : AudioProcessorEditor(proc), processor_(proc), view_(proc)
{
    addAndMakeVisible(view_);
    view_.onScaleChosen = [this](float s) {
        processor_.uiScale.store(s);
        applyScale(s);
    };

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

void GristoEditor::applyScale(float scale)
{
    view_.setTransform(juce::AffineTransform::scale(scale));
    view_.setScaleLabel(scale);
    setSize(juce::roundToInt(GristoView::kWidth * scale), juce::roundToInt(GristoView::kHeight * scale));
}
