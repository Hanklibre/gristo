#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

#include <juce_audio_utils/juce_audio_utils.h>

// The whole UI at its design size (1000 x 1142). GristoEditor scales it.
class GristoView : public juce::Component, public juce::DragAndDropContainer, private juce::Timer
{
public:
    static constexpr int kWidth = 1000;
    static constexpr int kHeight = 1142;

    explicit GristoView(GristoProcessor&);
    ~GristoView() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void(float)> onScaleChosen;
    std::function<void()> onKeysToggled;
    std::function<void()> onSettingsRequested; // mobile: the editor shows its settings overlay
    void setScaleLabel(float scale);

    // Mobile pages: the editor hosts the header controls itself and shows one region
    // of the view at a time.
    std::vector<juce::Component*> headerControls();
    void setHeaderExternal(bool external) { headerExternal_ = external; resized(); }
    void showBottomPage(int page);
    int lastArpPage() const { return lastArpPage_; } // the arp page shown last (1..kNumArps)
    std::function<void(int)> onBottomPageChanged;     // the panel's own tabs changed the page
    void showHelp(juce::Component* component) { showHelpFor(component); }
    juce::String helpTitle() const { return helpTitle_; }
    juce::String helpText() const { return helpText_; }
    std::function<void()> onHelpChanged;

    // Tap-to-route: after a lane chip is tapped, the next tap on a target routes it.
    void armLane(int lane);
    void disarmLane();
    int armedLane() const { return armedLane_; }
    std::function<void()> onArmChanged;
    static constexpr int kChordsPageIndex = 1 + gristo::kNumArps;
    static constexpr int kSoundsPageIndex = 2 + gristo::kNumArps;

private:
    struct Knob
    {
        ModSlider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };
    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };
    struct SlotRow
    {
        juce::ComboBox target;
        ModSlider amount;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    };

    void timerCallback() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent& e) override { showHelpFor(e.originalComponent); } // touch has no hover
    void showHelpFor(juce::Component* component);
    static void setHelp(juce::Component& component, const juce::String& title, const juce::String& text);
    static void setHelp(Knob& knob, const juce::String& title, const juce::String& text);

    void setupKnob(Knob&, const juce::String& paramId, const juce::String& text);
    void setupRandomKnob(Knob&, const juce::String& paramId, const juce::String& text);
    void setupCellKnob(Knob&, const juce::String& paramId, bool isRandom);
    void setupToggle(Toggle&, const juce::String& paramId, const juce::String& text);
    void setupChoice(Choice&, const juce::String& paramId, const juce::String& text);
    static void placeKnob(Knob&, juce::Rectangle<int> area);

    // Modulation
    void selectLane(int lane);
    void assignLane(int lane, int target);
    void makeTarget(juce::Component& widget, ModDropTarget& dropTarget, int target);
    void updateModulationDisplay(const gristo::EngineParams& params);
    void setParameter(const juce::String& id, float value);
    void showSizeMenu();

    GristoProcessor& processor_;
    GristoLookAndFeel lookAndFeel_;
    gristo::Engine previewEngine_;

    MapPad mapPad_;
    PatternView patternView_;
    juce::Label nodeLabel_;
    juce::TextButton sizeButton_, keysButton_ { "Keys" };

    Knob chaos_, variation_, mapRandom_;
    Knob fills_[gristo::kNumVoices];
    Knob fillRandoms_[gristo::kNumVoices];
    Knob probabilities_[gristo::kNumVoices];
    Knob probabilityRandoms_[gristo::kNumVoices];
    Knob voiceHumanize_[gristo::kNumVoices];
    Knob voiceDynamics_[gristo::kNumVoices];
    std::unique_ptr<VelocityRange> velocityRanges_[gristo::kNumVoices];
    std::unique_ptr<NoteSelector> notes_[gristo::kNumVoices];
    Toggle mutes_[gristo::kNumVoices];

    Choice timing_, feeling_;
    Knob shuffle_, shuffleRandom_, feelMs_, feelRandom_;
    Knob humanize_, dynamics_;
    Toggle lock_, linear_;
    Toggle kitOn_, freeRun_;
    Knob kitGain_, freeTempo_, noteLength_;
    std::unique_ptr<NumberSelector> channels_[gristo::kNumVoices];

    // Bottom panel pages: 0 = modulation, 1..kNumArps = arps, then chords and sounds
    static constexpr int kChordsPage = 1 + gristo::kNumArps;
    static constexpr int kSoundsPage = 2 + gristo::kNumArps;
    juce::TextButton pageTabs_[3 + gristo::kNumArps];
    bool isArpPage() const { return page_ >= 1 && page_ <= gristo::kNumArps; }
    int page_ = 0, lastArpPage_ = 1;
    void showPage(int page);
    void setupArpPage();
    void bindArp(int arp);
    void updateArpOutput(const gristo::EngineParams& params, int arp);
    std::vector<juce::Component*> modulationPageComponents();
    std::vector<juce::Component*> arpPageComponents();
    std::vector<juce::Component*> soundPageComponents();
    void setupSoundPage();
    void setupSmallKnob(Knob&, const juce::String& paramId, const juce::String& text);
    static void placeSmallKnob(Knob&, int x, int y);

    // Sounds page
    struct SynthRow
    {
        juce::ComboBox sound;
        std::unique_ptr<ComboIdAttachment> soundAttachment; // the menu is grouped by category
        Knob cutoff, resonance, env, attack, release, glide, level, reverb, delay;
    };
    Choice soundKit_, fxTime_;
    Knob kitTune_, kitDecay_, kitTone_, kitDrive_, kitReverb_, kitDelay_, fxSize_, fxFeedback_;
    Knob drumLevels_[gristo::kNumVoices];
    SynthRow synthRows_[gristo::sound::kNumSynths];

    // Chords page
    void setupChordPage();
    std::vector<juce::Component*> chordPageComponents();
    void updateChordPage(const gristo::EngineParams& params);
    void writeChordSlot(int index, const gristo::ChordSlot& slot);
    void editSelectedChord(const std::function<void(gristo::ChordSlot&)>& edit);
    void showChordLibrary();
    void newChordProgression();
    Toggle chordOn_;
    Toggle chordStabs_[gristo::kNumVoices];
    Choice chordKey_, chordScale_, chordClock_, chordClockVoice_, chordReset_, chordPlay_, chordOctave_;
    std::unique_ptr<NumberSelector> chordHits_, chordChannel_, chordShift_;
    std::unique_ptr<VelocityRange> chordVelocity_;
    juce::TextButton chordLibrary_ { "Library" }, chordNew_ { "New" }, chordClear_ { "Clear" };
    juce::TextButton chordTypes_[static_cast<int>(gristo::ChordType::Count)];
    juce::TextButton chordInversions_[3], chordLengths_[3];
    juce::ComboBox chordQuality_;
    int shownQualitySlot_ = -1, shownQuality_ = -1; // what the combo was last set to by the timer
    juce::TextButton chordFlat_ { "b" }, chordSharp_ { "#" };
    Knob chordMutate_, chordGate_;
    juce::Label chordOutLabel_;
    ChordSlotsView chordSlots_;
    ChordPadsView chordPads_;
    int selectedChordSlot_ = 0;
    LaneHandle soundLaneHandles_[gristo::kNumModLanes];
    std::vector<std::pair<Knob*, int>> soundTargets_; // Sounds-page knob -> modulation target

    // Arp page: one set of controls, bound to the arp being shown
    Toggle arpOn_, arpLatch_;
    Toggle arpTriggers_[gristo::kNumVoices];
    Choice arpMode_, arpReset_, arpOrder_, arpScale_;
    Knob arpOctaves_, arpGate_, arpLength_, arpDensity_, arpAccent_, arpSlide_, arpMutate_, arpProb_, arpProbRandom_;
    Toggle arpFollow_;
    juce::TextButton arpNew_;
    juce::Label arpRootLabel_, arpShiftLabel_, arpOutLabel_;
    std::unique_ptr<NumberSelector> arpChannel_, arpShift_;
    std::unique_ptr<NoteSelector> arpRoot_;
    std::unique_ptr<VelocityRange> arpVelocity_;
    ArpView arpView_;
    LaneHandle arpLaneHandles_[gristo::kNumModLanes]; // drag sources on the arp page
    void bindArpTargets(int arp);

    // Modulation panel
    juce::TextButton laneTabs_[gristo::kNumModLanes];
    LaneHandle laneHandle_;
    LaneEditor laneEditor_;
    juce::ComboBox laneLength_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> laneLengthAttachment_;
    juce::TextButton shapesButton_;
    SlotRow slots_[gristo::kModSlotsPerLane];
    int selectedLane_ = 0;

    // Target -> widget that shows its modulation
    std::array<ModSlider*, gristo::mod::kNumTargets> targetSliders_ {};

    juce::String helpTitle_, helpText_;
    bool headerExternal_ = false;
    int armedLane_ = -1;
    struct ArmCatcher : public juce::Component
    {
        std::function<void(juce::Point<int>)> onTap;
        juce::Colour colour;
        void paint(juce::Graphics& g) override
        {
            g.fillAll(colour.withAlpha(0.06f));
            g.setColour(colour);
            g.drawRect(getLocalBounds(), 3);
        }
        void mouseDown(const juce::MouseEvent& e) override
        {
            if (onTap)
                onTap(e.getPosition());
        }
    } armCatcher_;
    std::vector<LaneHandle*> laneHandles();

    // Preset bar
    juce::TextButton presetPrev_ { "<" }, presetName_, presetNext_ { ">" }, presetSave_ { "Save" }, presetShare_ { "Share" };
    std::unique_ptr<juce::FileChooser> chooser_;
    int presetRefresh_ = 0;
    void setupPresetBar();
    void showPresetMenu();
    void savePreset(bool askForName);
    void askPresetName(const juce::String& suggestion, std::function<void(juce::String)> done);
    void confirm(const juce::String& title, const juce::String& message, const juce::String& yes, std::function<void()> onYes);
    void importPreset();
    void exportPreset();
    void sharePreset(); // Android / iOS share sheet
    juce::ScopedMessageBox shareBox_;
    void presetLoaded();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GristoView)
};

// Hosts the view. Desktop: a fixed zoom chosen in the Size menu. Mobile: the view
// fills the screen width and scrolls vertically. The on-screen keyboard is a strip
// along the bottom that stays put while the view scrolls.
class GristoEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::ScrollBar::Listener
{
public:
    explicit GristoEditor(GristoProcessor&);
    ~GristoEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseEnter(const juce::MouseEvent&) override;

private:
    // Mobile: pages show one region of the view, scaled to fill the screen.
    enum MobilePage { Beat = 0, Groove, Arps, Chords, Sounds, NumMobilePages };
    void showMobilePage(int page);
    void showSettings();
    juce::Rectangle<int> mobileRegion() const;
    juce::Component clip_;
    juce::TextButton mobileTabs_[NumMobilePages];
    LaneHandle barLanes_[gristo::kNumModLanes]; // always visible, so lanes reach targets on any page
    int mobilePage_ = Beat;
    std::unique_ptr<juce::Component> settings_;
    juce::Rectangle<int> helpBox_;

    void applyScale(float scale);
    void scrollBarMoved(juce::ScrollBar*, double newRangeStart) override;
    void updateViewTransform();
    int keysHeight() const;

    GristoProcessor& processor_;
    GristoView view_;
    juce::MidiKeyboardComponent keyboard_;
    juce::ScrollBar scrollBar_ { true };
    float scale_ = 1.0f;
    double scrollY_ = 0.0; // view units
    juce::Rectangle<int> viewArea_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GristoEditor)
};
