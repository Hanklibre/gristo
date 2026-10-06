#pragma once

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <optional>

class GristoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GristoLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                          float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

// XY pad over the drum map. Brightness shows how busy the pattern is at the
// current Fill settings; dots mark the corpus nodes.
// Per-voice MIDI note: click for a menu (octaves, GM drum names), scroll to step.
class NoteSelector : public juce::Component
{
public:
    explicit NoteSelector(juce::RangedAudioParameter& parameter);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void setNote(int note);

    juce::RangedAudioParameter& parameter_;
    juce::ParameterAttachment attachment_;
    int note_ = 36;
};

class MapPad : public juce::Component
{
public:
    MapPad(juce::AudioProcessorValueTreeState& state, const gristo::DrumMap& map);
    // Map Random range (radius in map units) and the position used by the current bar.
    void setRandomDisplay(float radius, std::optional<juce::Point<float>> livePosition);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    void refreshHeat(const gristo::EngineParams& params);
    juce::String nearestNodeName() const;

private:
    void setFromMouse(juce::Point<float> pos);

    juce::AudioProcessorValueTreeState& state_;
    const gristo::DrumMap& map_;
    juce::Image heat_;
    juce::String heatKey_;
    float randomRadius_ = 0.0f;
    std::optional<juce::Point<float>> livePosition_;
};

// Six lanes x 32 steps. Each hit is drawn where it will actually sound (groove,
// feeling and humanize applied); the faint tick shows its straight position.
class PatternView : public juce::Component
{
public:
    explicit PatternView(const gristo::Engine& engine) : engine_(engine) {}
    void paint(juce::Graphics&) override;
    void setPlayhead(double ppq, double bpm, bool playing);

private:
    const gristo::Engine& engine_;
    double ppq_ = 0.0;
    double bpm_ = 120.0;
    bool playing_ = false;
};

class GristoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GristoEditor(GristoProcessor&);
    ~GristoEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
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

    void timerCallback() override;
    void setupKnob(Knob&, const juce::String& paramId, const juce::String& text);
    void setupRandomKnob(Knob&, const juce::String& paramId, const juce::String& text);
    void setupToggle(Toggle&, const juce::String& paramId, const juce::String& text);
    void setupChoice(Choice&, const juce::String& paramId, const juce::String& text);
    static void placeKnob(Knob&, juce::Rectangle<int> area);

    GristoProcessor& processor_;
    GristoLookAndFeel lookAndFeel_;
    gristo::Engine previewEngine_;

    MapPad mapPad_;
    PatternView patternView_;
    juce::Label nodeLabel_;

    Knob chaos_, variation_;
    Knob fills_[gristo::kNumVoices];
    Knob probabilities_[gristo::kNumVoices];
    Toggle mutes_[gristo::kNumVoices];
    std::unique_ptr<NoteSelector> notes_[gristo::kNumVoices];
    Knob mapRandom_, shuffleRandom_, feelRandom_;
    Choice timing_, feeling_;
    Knob shuffle_, feelMs_;
    Knob humanize_, dynamics_;
    Toggle lock_, linear_;
    Toggle kitOn_, freeRun_;
    Knob kitGain_, freeTempo_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GristoEditor)
};
