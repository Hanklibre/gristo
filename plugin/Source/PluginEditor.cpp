#include "PluginEditor.h"

namespace p = gristo::params;

namespace {

namespace colours {
const juce::Colour background { 0xff15161a };
const juce::Colour panel { 0xff1f2127 };
const juce::Colour outline { 0xff33363f };
const juce::Colour text { 0xffd9d4c7 };
const juce::Colour dim { 0xff7d7a72 };
const juce::Colour accent { 0xfff2b544 };  // warm amber, like backlit encoder rings
const juce::Colour hot { 0xffe8553b };
const juce::Colour cool { 0xff2b3a5c };
const juce::Colour random { 0xff5fb8aa };   // Beads-style random amounts
} // namespace colours

const juce::Colour kVoiceColours[gristo::kNumVoices] = {
    juce::Colour(0xffe8553b), juce::Colour(0xfff08a3a), juce::Colour(0xffefe6d2),
    juce::Colour(0xffc9b458), juce::Colour(0xfff08a3a), juce::Colour(0xffe8553b),
};

constexpr int kWidth = 1000;
constexpr int kHeight = 780;

void drawPanel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    g.setColour(colours::panel);
    g.fillRoundedRectangle(area.toFloat(), 8.0f);
    g.setColour(colours::outline);
    g.drawRoundedRectangle(area.toFloat().reduced(0.5f), 8.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour(colours::dim);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(title.toUpperCase(), area.reduced(12, 6).removeFromTop(14), juce::Justification::topLeft);
    }
}

} // namespace

// ---------------------------------------------------------------------------------
// Look and feel

GristoLookAndFeel::GristoLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, colours::text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, colours::text);
    setColour(juce::ComboBox::backgroundColourId, colours::background);
    setColour(juce::ComboBox::outlineColourId, colours::outline);
    setColour(juce::ComboBox::textColourId, colours::text);
    setColour(juce::ComboBox::arrowColourId, colours::accent);
    setColour(juce::PopupMenu::backgroundColourId, colours::panel);
    setColour(juce::PopupMenu::textColourId, colours::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha(0.3f));
    setColour(juce::ToggleButton::textColourId, colours::text);
}

void GristoLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const bool isRandom = slider.getProperties()["random"];
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    g.setColour(colours::background);
    g.fillEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre));

    juce::Path track, value;
    track.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f, startAngle, endAngle, true);
    value.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f, startAngle, angle, true);
    const juce::PathStrokeType stroke(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour(colours::outline);
    g.strokePath(track, stroke);
    g.setColour(isRandom ? colours::random : colours::accent);
    g.strokePath(value, stroke);

    const auto tip = centre.getPointOnCircumference(radius * 0.62f, angle);
    g.setColour(colours::text);
    g.drawLine({ centre.getPointOnCircumference(radius * 0.2f, angle), tip }, 2.0f);
}

void GristoLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto area = b.getLocalBounds().toFloat().reduced(2.0f);
    const bool on = b.getToggleState();
    g.setColour(on ? colours::accent : colours::background);
    g.fillRoundedRectangle(area, 5.0f);
    g.setColour(highlighted ? colours::text : colours::outline);
    g.drawRoundedRectangle(area, 5.0f, 1.0f);
    g.setColour(on ? colours::background : colours::text);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText(b.getButtonText(), area, juce::Justification::centred);
}

// ---------------------------------------------------------------------------------
// Note selector

NoteSelector::NoteSelector(juce::RangedAudioParameter& parameter)
    : parameter_(parameter),
      attachment_(parameter, [this](float value) {
          note_ = juce::roundToInt(value);
          repaint();
      })
{
    attachment_.sendInitialUpdate();
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void NoteSelector::setNote(int note)
{
    attachment_.setValueAsCompleteGesture(static_cast<float>(juce::jlimit(0, 127, note)));
}

void NoteSelector::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(colours::background);
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(isMouseOver() ? colours::text : colours::outline);
    g.drawRoundedRectangle(area, 4.0f, 1.0f);
    g.setColour(colours::text);
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(gristo::params::noteDisplayName(note_), area, juce::Justification::centred);
}

void NoteSelector::mouseDown(const juce::MouseEvent&)
{
    juce::PopupMenu menu;
    for (int octave = -2; octave <= 8; ++octave)
    {
        juce::PopupMenu sub;
        bool containsCurrent = false;
        for (int pc = 0; pc < 12; ++pc)
        {
            const int n = (octave + 2) * 12 + pc;
            if (n > 127)
                break;
            juce::String text = gristo::params::noteDisplayName(n);
            if (const char* gm = juce::MidiMessage::getRhythmInstrumentName(n))
                text << "   " << gm;
            sub.addItem(n + 1, text, true, n == note_);
            containsCurrent |= n == note_;
        }
        menu.addSubMenu("Octave " + juce::String(octave), sub, true, nullptr, containsCurrent);
    }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [safe = juce::Component::SafePointer<NoteSelector>(this)](int result) {
        if (safe != nullptr && result > 0)
            safe->setNote(result - 1);
    });
}

void NoteSelector::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const float delta = std::abs(wheel.deltaY) > std::abs(wheel.deltaX) ? wheel.deltaY : -wheel.deltaX;
    if (delta > 0.0f)
        setNote(note_ + 1);
    else if (delta < 0.0f)
        setNote(note_ - 1);
}

// ---------------------------------------------------------------------------------
// Map pad

MapPad::MapPad(juce::AudioProcessorValueTreeState& state, const gristo::DrumMap& map) : state_(state), map_(map) {}

void MapPad::setRandomDisplay(float radius, std::optional<juce::Point<float>> livePosition)
{
    randomRadius_ = radius;
    livePosition_ = livePosition;
}

void MapPad::refreshHeat(const gristo::EngineParams& params)
{
    // Only recompute when something that changes density changed.
    juce::String key;
    for (int v = 0; v < gristo::kNumVoices; ++v)
        key << juce::String(params.pattern.fill[static_cast<size_t>(v)], 3) << (params.pattern.mute[static_cast<size_t>(v)] ? "m" : "") << ",";
    key << (params.pattern.linear ? "L" : "");
    if (key == heatKey_ && heat_.isValid())
        return;
    heatKey_ = key;

    constexpr int res = 64;
    gristo::PatternGenerator gen(map_);
    gristo::PatternParams pp = params.pattern;
    pp.chaos = 0.0f;

    std::vector<float> density(res * res);
    float maxDensity = 1e-3f;
    for (int iy = 0; iy < res; ++iy)
        for (int ix = 0; ix < res; ++ix)
        {
            pp.mapX = ix / float(res - 1);
            pp.mapY = iy / float(res - 1);
            int count = 0;
            for (int s = 0; s < gristo::kPatternSteps; ++s)
                for (const auto& h : gen.evaluate(pp, s))
                    count += h.on ? 1 : 0;
            const float d = count / float(gristo::kPatternSteps * gristo::kNumVoices);
            density[static_cast<size_t>(iy * res + ix)] = d;
            maxDensity = std::max(maxDensity, d);
        }

    heat_ = juce::Image(juce::Image::RGB, res, res, false);
    for (int iy = 0; iy < res; ++iy)
        for (int ix = 0; ix < res; ++ix)
        {
            const float t = std::sqrt(density[static_cast<size_t>(iy * res + ix)] / maxDensity);
            const auto c = t < 0.5f ? colours::background.interpolatedWith(colours::cool, t * 2.0f)
                                    : colours::cool.interpolatedWith(colours::hot.withMultipliedBrightness(0.8f), (t - 0.5f) * 2.0f);
            heat_.setPixelAt(ix, iy, c);
        }
    repaint();
}

juce::String MapPad::nearestNodeName() const
{
    const float x = state_.getRawParameterValue(p::mapX)->load();
    const float y = state_.getRawParameterValue(p::mapY)->load();
    const int nx = juce::roundToInt(x * float(map_.width() - 1));
    const int ny = juce::roundToInt(y * float(map_.height() - 1));
    return juce::String(map_.nodeName(nx, ny)).replace("_", " ");
}

void MapPad::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    if (heat_.isValid())
    {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(heat_, area);
    }

    g.setColour(colours::text.withAlpha(0.25f));
    for (int ny = 0; ny < map_.height(); ++ny)
        for (int nx = 0; nx < map_.width(); ++nx)
        {
            const float px = area.getX() + area.getWidth() * nx / float(map_.width() - 1);
            const float py = area.getY() + area.getHeight() * ny / float(map_.height() - 1);
            g.fillEllipse(px - 2.0f, py - 2.0f, 4.0f, 4.0f);
        }

    const float x = state_.getRawParameterValue(p::mapX)->load();
    const float y = state_.getRawParameterValue(p::mapY)->load();
    const juce::Point<float> c { area.getX() + x * area.getWidth(), area.getY() + y * area.getHeight() };
    g.setColour(colours::accent.withAlpha(0.5f));
    g.drawHorizontalLine(juce::roundToInt(c.y), area.getX(), area.getRight());
    g.drawVerticalLine(juce::roundToInt(c.x), area.getY(), area.getBottom());
    if (randomRadius_ > 0.0f)
    {
        // Map Random range: X and Y each move up to +-radius around the crosshair.
        const auto range = juce::Rectangle<float>(randomRadius_ * 2.0f * area.getWidth(), randomRadius_ * 2.0f * area.getHeight())
                               .withCentre(c)
                               .getIntersection(area);
        g.setColour(colours::random.withAlpha(0.12f));
        g.fillRoundedRectangle(range, 6.0f);
        g.setColour(colours::random.withAlpha(0.6f));
        g.drawRoundedRectangle(range, 6.0f, 1.0f);
    }
    if (livePosition_.has_value())
    {
        const juce::Point<float> lp { area.getX() + livePosition_->x * area.getWidth(), area.getY() + livePosition_->y * area.getHeight() };
        g.setColour(colours::random);
        g.fillEllipse(lp.x - 4.0f, lp.y - 4.0f, 8.0f, 8.0f);
    }
    g.setColour(colours::accent);
    g.drawEllipse(c.x - 7.0f, c.y - 7.0f, 14.0f, 14.0f, 2.0f);

    g.setColour(colours::outline);
    g.drawRect(area, 1.0f);
    g.setColour(colours::dim);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("straight", area.reduced(6).removeFromBottom(12), juce::Justification::bottomLeft);
    g.drawText("syncopated", area.reduced(6).removeFromBottom(12), juce::Justification::bottomRight);
    g.drawText("four on the floor", area.reduced(6).removeFromTop(12), juce::Justification::topRight);
}

void MapPad::setFromMouse(juce::Point<float> pos)
{
    const float x = juce::jlimit(0.0f, 1.0f, pos.x / float(getWidth()));
    const float y = juce::jlimit(0.0f, 1.0f, pos.y / float(getHeight()));
    state_.getParameter(p::mapX)->setValueNotifyingHost(x);
    state_.getParameter(p::mapY)->setValueNotifyingHost(y);
    repaint();
}

void MapPad::mouseDown(const juce::MouseEvent& e)
{
    state_.getParameter(p::mapX)->beginChangeGesture();
    state_.getParameter(p::mapY)->beginChangeGesture();
    setFromMouse(e.position);
}

void MapPad::mouseDrag(const juce::MouseEvent& e) { setFromMouse(e.position); }

void MapPad::mouseUp(const juce::MouseEvent&)
{
    state_.getParameter(p::mapX)->endChangeGesture();
    state_.getParameter(p::mapY)->endChangeGesture();
}

// ---------------------------------------------------------------------------------
// Pattern view

void PatternView::setPlayhead(double ppq, double bpm, bool playing)
{
    ppq_ = ppq;
    bpm_ = bpm;
    playing_ = playing;
    repaint();
}

void PatternView::paint(juce::Graphics& g)
{
    using namespace gristo;
    auto area = getLocalBounds().toFloat();
    const float labelW = 64.0f;
    auto grid = area.withTrimmedLeft(labelW);
    const float laneH = grid.getHeight() / kNumVoices;
    const float stepW = grid.getWidth() / kPatternSteps;

    // Show the pattern cycle that is currently playing (cycle 0 when stopped).
    const std::int64_t cycle = playing_ ? floorDiv(static_cast<std::int64_t>(std::floor(ppq_ * kStepsPerBeat)), kPatternSteps) : 0;
    const double cycleStartBeats = static_cast<double>(cycle * kPatternSteps) / kStepsPerBeat;
    auto xOfBeats = [&](double beats) { return grid.getX() + float((beats - cycleStartBeats) * kStepsPerBeat) * stepW; };

    for (int s = 0; s <= kPatternSteps; ++s)
    {
        const bool bar = s % kStepsPerBar == 0;
        const bool beat = s % kStepsPerBeat == 0;
        g.setColour(colours::outline.withAlpha(bar ? 1.0f : beat ? 0.6f : 0.2f));
        const float x = grid.getX() + s * stepW;
        g.drawVerticalLine(juce::roundToInt(x), grid.getY(), grid.getBottom());
    }

    for (int v = 0; v < kNumVoices; ++v)
    {
        auto lane = grid.withY(grid.getY() + v * laneH).withHeight(laneH);
        g.setColour(colours::dim);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(voiceName(v), juce::Rectangle<float>(area.getX(), lane.getY(), labelW - 6.0f, laneH),
                   juce::Justification::centredRight);
        g.setColour(colours::outline.withAlpha(0.35f));
        g.drawHorizontalLine(juce::roundToInt(lane.getBottom()), grid.getX(), grid.getRight());

        for (int s = 0; s < kPatternSteps; ++s)
        {
            const std::int64_t step = cycle * kPatternSteps + s;
            const HitInfo h = engine_.describe(step, v, bpm_, true);
            if (h.dropped)
            {
                // Removed by Probability this time round: hollow outline at the groove slot.
                const float x = xOfBeats(h.timeBeats);
                const float barH = (laneH - 8.0f) * (0.3f + 0.7f * h.level);
                g.setColour(kVoiceColours[v].withAlpha(0.45f));
                g.drawRoundedRectangle(x, lane.getBottom() - 5.0f - barH, std::max(3.0f, stepW * 0.55f), barH, 2.0f, 1.0f);
                continue;
            }
            if (!h.on)
                continue;

            // Straight position tick.
            const float straightX = grid.getX() + s * stepW;
            g.setColour(colours::dim.withAlpha(0.5f));
            g.fillRect(straightX + 1.0f, lane.getBottom() - 3.0f, stepW - 2.0f, 2.0f);

            const auto colour = kVoiceColours[v].withAlpha(h.accent ? 1.0f : 0.45f + 0.4f * h.velocity);
            for (int k = 0; k < h.rollCount; ++k)
            {
                const float x = xOfBeats(h.timeBeats + k * h.rollSpacingBeats);
                const float barW = h.rollCount > 1 ? std::max(2.0f, stepW * 0.25f) : std::max(3.0f, stepW * 0.55f);
                const float barH = (laneH - 8.0f) * (0.3f + 0.7f * h.velocity);
                g.setColour(colour);
                g.fillRoundedRectangle(x, lane.getBottom() - 5.0f - barH, barW, barH, 2.0f);
            }
        }
    }

    if (playing_)
    {
        const float x = xOfBeats(ppq_);
        g.setColour(colours::accent);
        g.drawVerticalLine(juce::roundToInt(x), grid.getY(), grid.getBottom());
    }
}

// ---------------------------------------------------------------------------------
// Editor

GristoEditor::GristoEditor(GristoProcessor& proc)
    : AudioProcessorEditor(proc), processor_(proc), mapPad_(proc.state, previewEngine_.map()), patternView_(previewEngine_)
{
    setLookAndFeel(&lookAndFeel_);

    addAndMakeVisible(mapPad_);
    addAndMakeVisible(patternView_);
    nodeLabel_.setJustificationType(juce::Justification::centredLeft);
    nodeLabel_.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(nodeLabel_);

    setupKnob(chaos_, p::chaos, "Chaos");
    setupKnob(variation_, p::variation, "Variation");
    setupRandomKnob(mapRandom_, p::mapRandom, "Map Random");
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        setupKnob(fills_[v], p::voiceId(p::fill, v), "Fill");
        setupKnob(probabilities_[v], p::voiceId(p::probability, v), "Probability");
        setupToggle(mutes_[v], p::voiceId(p::mute, v), "Mute");
        notes_[v] = std::make_unique<NoteSelector>(*proc.state.getParameter(p::voiceId(p::note, v)));
        addAndMakeVisible(*notes_[v]);
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

    setSize(kWidth, kHeight);
    timerCallback();
    startTimerHz(30);
}

GristoEditor::~GristoEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void GristoEditor::setupKnob(Knob& k, const juce::String& paramId, const juce::String& text)
{
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

void GristoEditor::setupRandomKnob(Knob& k, const juce::String& paramId, const juce::String& text)
{
    setupKnob(k, paramId, text);
    k.slider.getProperties().set("random", true);
    k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
    k.label.setFont(juce::FontOptions(11.0f));
    k.label.setColour(juce::Label::textColourId, colours::random);
}

void GristoEditor::setupToggle(Toggle& t, const juce::String& paramId, const juce::String& text)
{
    t.button.setButtonText(text);
    addAndMakeVisible(t.button);
    t.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor_.state, paramId, t.button);
}

void GristoEditor::setupChoice(Choice& c, const juce::String& paramId, const juce::String& text)
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

void GristoEditor::placeKnob(Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds(area.removeFromTop(16));
    k.slider.setBounds(area);
}

void GristoEditor::timerCallback()
{
    const gristo::EngineParams params = p::readEngineParams(processor_.state);
    previewEngine_.setParams(params);
    mapPad_.refreshHeat(params);
    std::optional<juce::Point<float>> live;
    if (params.pattern.mapRandom > 0.0f && processor_.displayPlaying.load())
    {
        const auto step = static_cast<std::int64_t>(std::floor(processor_.displayPpq.load() * gristo::kStepsPerBeat));
        const auto [lx, ly] = gristo::PatternGenerator::mapPositionAt(params.pattern, step);
        live = juce::Point<float>(lx, ly);
    }
    mapPad_.setRandomDisplay(params.pattern.mapRandom * 0.5f, live);
    mapPad_.repaint();
    nodeLabel_.setText("Near: " + mapPad_.nearestNodeName(), juce::dontSendNotification);
    patternView_.setPlayhead(processor_.displayPpq.load(), processor_.displayBpm.load(), processor_.displayPlaying.load());
}

void GristoEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::background);
    auto header = getLocalBounds().removeFromTop(44).reduced(16, 0);
    g.setColour(colours::text);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("GRISTO", header, juce::Justification::centredLeft);
    g.setColour(colours::dim);
    g.setFont(juce::FontOptions(12.0f));
    g.drawText("topographic drums, tripping over themselves", header.withTrimmedLeft(110), juce::Justification::centredLeft);

    drawPanel(g, { 16, 52, 344, 568 }, "Map");
    drawPanel(g, { 372, 52, 612, 268 }, "Pattern");
    drawPanel(g, { 372, 328, 612, 292 }, "Voices");
    drawPanel(g, { 16, 628, 424, 136 }, "Groove");
    drawPanel(g, { 452, 628, 260, 136 }, "Humanize / Dynamics");
    drawPanel(g, { 724, 628, 260, 136 }, "Output");

    const int colW = 600 / gristo::kNumVoices;
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        g.setColour(kVoiceColours[v]);
        g.drawText(gristo::voiceName(v), 378 + v * colW, 344, colW, 16, juce::Justification::centred);
    }
}

void GristoEditor::resized()
{
    mapPad_.setBounds(28, 78, 320, 320);
    nodeLabel_.setBounds(28, 404, 320, 20);
    placeKnob(chaos_, { 28, 440, 104, 112 });
    placeKnob(mapRandom_, { 136, 452, 104, 96 });
    placeKnob(variation_, { 244, 440, 104, 112 });

    patternView_.setBounds(384, 74, 588, 236);

    const int colW = 600 / gristo::kNumVoices;
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        const int x = 378 + v * colW;
        placeKnob(fills_[v], { x, 362, colW, 88 });
        placeKnob(probabilities_[v], { x, 452, colW, 88 });
        notes_[v]->setBounds(x + 8, 548, colW - 16, 24);
        mutes_[v].button.setBounds(x + colW / 2 - 28, 582, 56, 26);
    }

    // Groove
    timing_.label.setBounds(28, 648, 80, 16);
    timing_.box.setBounds(28, 666, 92, 26);
    feeling_.label.setBounds(28, 700, 80, 16);
    feeling_.box.setBounds(28, 718, 92, 26);
    placeKnob(shuffle_, { 128, 648, 84, 104 });
    placeKnob(shuffleRandom_, { 210, 668, 60, 84 });
    placeKnob(feelMs_, { 276, 648, 84, 104 });
    placeKnob(feelRandom_, { 358, 668, 60, 84 });

    // Humanize
    placeKnob(humanize_, { 460, 648, 84, 104 });
    placeKnob(dynamics_, { 544, 648, 84, 104 });
    lock_.button.setBounds(634, 668, 70, 26);
    linear_.button.setBounds(634, 710, 70, 26);

    // Output
    kitOn_.button.setBounds(734, 668, 70, 26);
    freeRun_.button.setBounds(734, 710, 70, 26);
    placeKnob(kitGain_, { 806, 648, 84, 104 });
    placeKnob(freeTempo_, { 890, 648, 84, 104 });
}
