#pragma once

#include "PluginProcessor.h"
#include "Theme.h"

#include <functional>
#include <optional>

// One modulation assignment drawn on a target: an arc from the knob's value to
// where the lane can push it, in the lane's colour.
struct ModRing
{
    juce::Colour colour;
    float from = 0.0f;
    float to = 0.0f;
};

class GristoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GristoLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                          float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

// Accepts a modulation lane dragged from a LaneHandle (Massive-style assignment).
class ModDropTarget : public juce::DragAndDropTarget
{
public:
    static constexpr const char* kDragPrefix = "gristo-lane:";

    std::function<void(int lane)> onLaneDropped;

    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails&) override { setDropHover(true); }
    void itemDragExit(const SourceDetails&) override { setDropHover(false); }
    void itemDropped(const SourceDetails& details) override;

protected:
    virtual void setDropHover(bool hover) = 0;
};

// Rotary knob that can show modulation rings and a live-value dot, and accepts lanes.
class ModSlider : public juce::Slider, public ModDropTarget
{
public:
    std::vector<ModRing> rings;
    float live = -1.0f; // proportion of the current modulated value, -1 = hidden
    bool dropHover = false;

    void paint(juce::Graphics&) override;

protected:
    void setDropHover(bool hover) override;
};

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

    juce::ParameterAttachment attachment_;
    int note_ = 36;
};

// Small integer selector shown as "<prefix><value>" (e.g. MIDI channel): click for a
// menu, scroll to step.
class NumberSelector : public juce::Component
{
public:
    NumberSelector(juce::RangedAudioParameter& parameter, juce::String prefix, int minValue, int maxValue,
                   juce::String suffix = {}, int menuStep = 1, bool showSign = false);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void setValue(int value);
    juce::String text(int value) const;

    juce::ParameterAttachment attachment_;
    juce::String prefix_, suffix_;
    int min_, max_, value_ = 1;
    int menuStep_;
    bool showSign_;
};

// Shows an arp's notes: the generated pattern (steps, rests, accents, slides) or the
// held chord, with the current position while playing.
class ArpView : public juce::Component, public ModDropTarget
{
public:
    explicit ArpView(GristoProcessor& processor) : processor_(processor) {}
    void setArp(int arp) { arp_ = arp; repaint(); }
    void update(const gristo::ArpParams& params) { params_ = params; repaint(); }
    void paint(juce::Graphics&) override;

    std::vector<juce::Colour> transposeLanes; // lanes routed to this arp's Transpose
    int liveTranspose = 0;
    bool showLive = false;

protected:
    void setDropHover(bool hover) override { dropHover_ = hover; repaint(); }

private:
    bool dropHover_ = false;
    GristoProcessor& processor_;
    int arp_ = 0;
    gristo::ArpParams params_;
};

// Two-handle MIDI velocity range (1..127) for one voice. Accepts lanes (Velocity target).
class VelocityRange : public juce::Component, public ModDropTarget
{
public:
    VelocityRange(juce::RangedAudioParameter& minParam, juce::RangedAudioParameter& maxParam, juce::Colour colour);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    std::vector<ModRing> rings; // only the colours are used: one dot per routed lane
    float liveOffset = 0.0f;    // current modulation offset (normalized), shown as a marker
    bool showLive = false;

protected:
    void setDropHover(bool hover) override;

private:
    juce::Rectangle<float> barArea() const;
    int velocityAt(float x) const;

    juce::Colour colour_;
    juce::ParameterAttachment minAttachment_, maxAttachment_;
    int min_ = 1, max_ = 127;
    int dragging_ = -1; // 0 = min, 1 = max
    bool dropHover_ = false;
};

// XY pad over the drum map. Brightness shows how busy the pattern is at the
// current Fill settings; dots mark the corpus nodes.
class MapPad : public juce::Component, public ModDropTarget
{
public:
    MapPad(juce::AudioProcessorValueTreeState& state, const gristo::DrumMap& map);
    // Map Random range (radius in map units) and the position used right now.
    void setRandomDisplay(float radius, std::optional<juce::Point<float>> livePosition);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    void refreshHeat(const gristo::EngineParams& params);
    juce::String nearestNodeName() const;

    // Called with (lane, mod::kMapX or mod::kMapY) after the user picks an axis.
    std::function<void(int lane, int target)> onLaneAxisChosen;

protected:
    void setDropHover(bool hover) override;

private:
    void setFromMouse(juce::Point<float> pos);

    juce::AudioProcessorValueTreeState& state_;
    const gristo::DrumMap& map_;
    juce::Image heat_;
    juce::String heatKey_;
    float randomRadius_ = 0.0f;
    std::optional<juce::Point<float>> livePosition_;
    bool dropHover_ = false;
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

// Massive-style performer lane: draw one value per 16th with the mouse.
class LaneEditor : public juce::Component
{
public:
    explicit LaneEditor(GristoProcessor& processor);
    void setLane(int lane);
    int lane() const { return lane_; }
    void applyShape(gristo::ModShape shape);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    int numPoints() const;
    juce::Rectangle<float> drawArea() const;
    void paintAt(juce::Point<float> pos, bool connectToLast);

    GristoProcessor& processor_;
    int lane_ = 0;
    int lastIndex_ = -1;
    float lastValue_ = 0.0f;
};

// Drag source for a lane: drop it on a knob to route the lane there.
class LaneHandle : public juce::Component
{
public:
    void setLane(int lane);
    void setCompact(bool compact) { compact_ = compact; repaint(); } // small "L1" chip
    void paint(juce::Graphics&) override;
    void mouseDrag(const juce::MouseEvent&) override;

private:
    int lane_ = 0;
    bool compact_ = false;
};
