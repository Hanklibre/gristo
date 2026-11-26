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

// One shared instance (juce::SharedResourcePointer) while any Gristo window is open:
// it is also the default look-and-feel then, so every font in Gristo is Inter.
class GristoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GristoLookAndFeel();
    ~GristoLookAndFeel() override;
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& background, bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool down, int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    juce::Font getPopupMenuFont() override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;

private:
    juce::Typeface::Ptr regular_, semibold_;
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                          float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

// Accepts a modulation lane dragged from a LaneHandle (Massive-style assignment).
// Like APVTS::ComboBoxAttachment, but matched by item id (id = choice index + 1)
// instead of position, so the menu can be ordered and grouped freely.
class ComboIdAttachment
{
public:
    ComboIdAttachment(juce::RangedAudioParameter& param, juce::ComboBox& box)
        : box_(box), attachment_(param, [this](float value) { box_.setSelectedId(juce::roundToInt(value) + 1, juce::dontSendNotification); })
    {
        box_.onChange = [this] {
            if (const int id = box_.getSelectedId(); id > 0)
                attachment_.setValueAsCompleteGesture(static_cast<float>(id - 1));
        };
        attachment_.sendInitialUpdate();
    }
    ~ComboIdAttachment() { box_.onChange = nullptr; }

private:
    juce::ComboBox& box_;
    juce::ParameterAttachment attachment_;
};

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
class ModSlider : public juce::Slider, public ModDropTarget, private juce::Timer
{
public:
    std::vector<ModRing> rings;
    float live = -1.0f; // proportion of the current modulated value, -1 = hidden
    bool dropHover = false;
    std::function<void()> onContextMenu; // right-click: "Modulate with lane..."

    void paint(juce::Graphics&) override;
    // Right-click, or press and hold without moving (touch), opens the menu.
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onContextMenu)
        {
            onContextMenu();
            return;
        }
        if (onContextMenu)
            startTimer(600);
        juce::Slider::mouseDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() > 5)
            stopTimer();
        juce::Slider::mouseDrag(e);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        stopTimer();
        juce::Slider::mouseUp(e);
    }

protected:
    void setDropHover(bool hover) override;

private:
    void timerCallback() override
    {
        stopTimer();
        if (onContextMenu && isMouseButtonDown())
            onContextMenu();
    }
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

// The harmony map: columns are a chain of thirds around the tonic (neighbours share
// two notes; darker to the left, brighter to the right), rows add colour towards the
// top. Drag to move home; the cell sounding now glows and leaves a short trail.
class HarmonyPad : public juce::Component
{
public:
    explicit HarmonyPad(juce::AudioProcessorValueTreeState& state);
    // playing: (column + 5) * 8 + row of the cell sounding now, -1 = none.
    void update(const gristo::ChordParams& params, int playing);
    std::function<void()> onTouched; // a press on the pad (to switch the chords on)
    // A press on a cell; return true to take it (the home cell then stays where it is).
    // A taken press can be dragged onto the progression (see kChordDragPrefix).
    std::function<bool(gristo::HarmonyCell)> onCellPressed;
    std::function<void()> onCellReleased;
    std::function<void(gristo::HarmonyCell)> onCellDoubleClicked;
    void setCandidate(int cellCode); // the chord last heard, marked on the map (-1 none)
    void mouseDoubleClick(const juce::MouseEvent&) override;
    static constexpr const char* kChordDragPrefix = "gristo-chord:";
    juce::String playingName() const; // "Am9", or the home chord while nothing sounds
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> cellArea(gristo::HarmonyCell cell) const;
    void setFromMouse(juce::Point<float> pos);
    juce::AudioProcessorValueTreeState& state_;
    gristo::ChordParams params_;
    int playing_ = -1;
    std::array<int, 3> trail_ { -1, -1, -1 }; // the cells before the current one
    bool taken_ = false; // this press was taken by onCellPressed
    bool dragged_ = false;
    int pressedCode_ = -1, candidate_ = -1;
    gristo::HarmonyCell cellAt(juce::Point<float> pos) const;
};

// The chord progression as a little timeline: up to sixteen slots, each as wide as it
// lasts (in quarter notes), the one playing lit with a playhead. In Progression mode
// they are the chords you picked: click one to select it (and hear it), drag its right
// edge to make it longer or shorter in quarter notes, right-click for its length,
// Duplicate and Remove. In Wander mode they show, dimmed, the walk being played.
class ProgressionStrip : public juce::Component, public juce::DragAndDropTarget
{
public:
    using Slots = std::array<int, gristo::kMaxProgression>;
    // A chord dragged from the map onto a slot (the slot after the last chord: appended).
    std::function<void(int slot, int cellCode)> onDrop;
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDragMove(const SourceDetails&) override;
    void itemDragExit(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    // beats: each slot's length in quarter notes (already resolved, never 0).
    void update(const gristo::ChordParams& params, const Slots& slots, const Slots& beats, bool editable, int playing, float playFraction);
    int selected() const { return selected_; }
    void clearSelection();
    std::function<void(int slot)> onRemove;
    std::function<void(int slot)> onDuplicate;
    std::function<void(int slot, int beats)> onLength;
    std::function<void(int slot)> onSelect; // -1: none
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    static juce::String lengthName(int beats); // "1/4", "3/4", "1 bar", "2 bars"...

private:
    int count() const;                         // the slots that play
    float beatWidth() const;                   // pixels per quarter note
    juce::Rectangle<float> slotArea(int slot) const; // slot == count(): the "+" slot
    int slotAt(float x) const;
    int edgeAt(float x) const; // the slot whose right edge is under x, or -1
    gristo::ChordParams params_;
    Slots slots_ {}, beats_ {};
    bool editable_ = false;
    int playing_ = -1, selected_ = -1, dropSlot_ = -1;
    float playFraction_ = 0.0f;
    int resizing_ = -1, resizeStartBeats_ = 0;
    float resizeScale_ = 1.0f;
    static constexpr float kMinSlot = 30.0f, kPlusSlot = 36.0f;
};

// Circles (after Patterning): one ring of steps per drum, the kick outermost, each
// as many steps as the drum's Length, the step playing lit. Click a step to set or
// clear it; drag up / down on a set step to change what the edit mode shows (its
// velocity, chance, nudge or repeats), drawn as how far the step is filled.
class CircleView : public juce::Component
{
public:
    enum class Edit { Velocity = 0, Chance, Nudge, Repeat };
    struct Ring
    {
        int length = 16;
        std::array<int, gristo::kPatternSteps> steps {}; // encodeCircleStep codes
        bool muted = false;
        bool operator==(const Ring& o) const { return length == o.length && steps == o.steps && muted == o.muted; }
    };
    using Rings = std::array<Ring, gristo::kNumVoices>;
    void update(const Rings& rings, const std::array<int, gristo::kNumVoices>& playing, int selected);
    void setEdit(Edit edit);
    Edit edit() const { return edit_; }
    std::function<void(int voice, int step, int code)> onStep;
    std::function<void(int voice)> onSelect;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    static float editValue(const gristo::CircleStep& s, Edit edit); // 0..1, as drawn

private:
    struct Hit
    {
        int voice = -1, step = -1;
    };
    Hit hitAt(juce::Point<float> p) const;
    void ringRadii(int voice, float& inner, float& outer) const;
    Rings rings_ {};
    std::array<int, gristo::kNumVoices> playing_ {};
    int selected_ = 0;
    Edit edit_ = Edit::Velocity;
    Hit pressed_;
    gristo::CircleStep pressedStep_;
    bool dragged_ = false, wasOn_ = false;
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
    juce::uint32 heatTime_ = 0;
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

// All ten parts over the bars the selected lane spans, each note where it will
// sound, with the playhead. Sits right above the lane editor, on the same time axis.
class TimelineView : public juce::Component
{
public:
    static constexpr int kLabelWidth = 64;
    explicit TimelineView(GristoProcessor& processor) : processor_(processor) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    // Called from the editor's timer.
    void update(double ppq, bool playing, int bars, int selectedPart);
    std::function<void(int part)> onSelect;

private:
    int loopStepOf(int voice, std::int64_t step) const; // -1: off the drum's clock
    GristoProcessor& processor_;
    std::vector<GristoProcessor::NoteSpan> spans_;
    // Drum step editing: the cell under the mouse, and what a drag paints.
    bool cellAt(juce::Point<float> pos, int& voice, std::int64_t& step) const;
    bool hitAt(int voice, std::int64_t step) const;
    int dragEdit_ = -1, dragVoice_ = -1;
    std::int64_t dragStep_ = -1;
    double ppq_ = 0.0, windowStart_ = 0.0;
    bool playing_ = false;
    int bars_ = 1, selectedPart_ = 0;
    std::uint32_t generation_ = 0;
    std::int64_t inputKey_ = -1;
    juce::uint32 renderedAt_ = 0;
    bool stale_ = true;
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
    bool drawn() const; // false while the lane runs as an LFO
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
    void setArmed(bool armed) { if (armed_ != armed) { armed_ = armed; repaint(); } }
    int lane() const { return lane_; }
    void paint(juce::Graphics&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    // A tap (no drag): route by tapping a target next instead of dragging.
    std::function<void(int lane)> onTap;

private:
    int lane_ = 0;
    bool compact_ = false;
    bool armed_ = false;
};

// A grip that starts a drag out of the plugin (MIDI to the DAW).
class DragOutHandle : public juce::Component
{
public:
    juce::String text;
    std::function<void()> onDrag;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override { dragging_ = false; }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragging_ && e.getDistanceFromDragStart() > 6 && onDrag)
        {
            dragging_ = true;
            onDrag();
        }
    }

private:
    bool dragging_ = false;
};
