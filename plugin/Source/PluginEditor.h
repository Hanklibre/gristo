#pragma once

#include "PluginProcessor.h"
#include "Widgets.h"

#include <juce_audio_utils/juce_audio_utils.h>

// The whole UI at its design size (1000 x 1096). GristoEditor scales it.
class GristoView : public juce::Component, public juce::DragAndDropContainer, private juce::Timer
{
public:
    static constexpr int kWidth = 1000;
    static constexpr int kHeight = 1096;

    explicit GristoView(GristoProcessor&);
    ~GristoView() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void(float)> onScaleChosen;
    void setScaleLabel(float scale);

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
    juce::TextButton sizeButton_;

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

    // Bottom panel pages: 0 = modulation, 1.. = arps
    juce::TextButton pageTabs_[1 + gristo::kNumArps];
    int page_ = 0;
    void showPage(int page);
    void setupArpPage();
    void bindArp(int arp);
    void updateArpOutput(const gristo::EngineParams& params, int arp);
    std::vector<juce::Component*> modulationPageComponents();
    std::vector<juce::Component*> arpPageComponents();

    // Arp page: one set of controls, bound to the arp being shown
    Toggle arpOn_, arpLatch_;
    Toggle arpTriggers_[gristo::kNumVoices];
    Choice arpMode_, arpReset_, arpOrder_, arpScale_;
    Knob arpOctaves_, arpGate_, arpLength_, arpDensity_, arpAccent_, arpSlide_, arpMutate_;
    juce::TextButton arpNew_;
    juce::Label arpRootLabel_, arpShiftLabel_, arpOutLabel_;
    std::unique_ptr<NumberSelector> arpChannel_, arpShift_;
    std::unique_ptr<NoteSelector> arpRoot_;
    std::unique_ptr<VelocityRange> arpVelocity_;
    ArpView arpView_;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GristoView)
};

class GristoEditor : public juce::AudioProcessorEditor
{
public:
    explicit GristoEditor(GristoProcessor&);
    void resized() override {}

private:
    void applyScale(float scale);

    GristoProcessor& processor_;
    GristoView view_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GristoEditor)
};
