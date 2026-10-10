#include "Widgets.h"

namespace p = gristo::params;
using namespace theme;

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
    setColour(juce::TextButton::buttonColourId, colours::background);
    setColour(juce::TextButton::textColourOffId, colours::text);
}

void GristoLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const bool isRandom = slider.getProperties()["random"];
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    auto angleOf = [&](float proportion) { return startAngle + juce::jlimit(0.0f, 1.0f, proportion) * (endAngle - startAngle); };
    const float angle = angleOf(pos);

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

    // Modulation rings (inside the track), one per routed lane, plus the live value.
    if (auto* ms = dynamic_cast<ModSlider*>(&slider))
    {
        float r = radius - 6.5f;
        for (const ModRing& ring : ms->rings)
        {
            if (r < 4.0f)
                break;
            juce::Path arc;
            const float a0 = angleOf(std::min(ring.from, ring.to));
            const float a1 = angleOf(std::max(ring.from, ring.to));
            arc.addCentredArc(centre.x, centre.y, r, r, 0.0f, a0, std::max(a1, a0 + 0.02f), true);
            g.setColour(ring.colour);
            g.strokePath(arc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            r -= 2.5f;
        }
        if (ms->live >= 0.0f)
        {
            const auto dot = centre.getPointOnCircumference(radius - 2.0f, angleOf(ms->live));
            g.setColour(juce::Colours::white);
            g.fillEllipse(dot.x - 2.5f, dot.y - 2.5f, 5.0f, 5.0f);
        }
    }

    const auto tip = centre.getPointOnCircumference(radius * 0.62f, angle);
    g.setColour(colours::text);
    g.drawLine({ centre.getPointOnCircumference(radius * 0.2f, angle), tip }, 2.0f);
}

void GristoLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle style,
                                         juce::Slider& slider)
{
    if (style != juce::Slider::LinearBar)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }
    // Bipolar bar (modulation amounts): fills from the centre.
    const auto area = juce::Rectangle<int>(x, y, width, height).toFloat();
    g.setColour(colours::background);
    g.fillRoundedRectangle(area, 4.0f);
    const juce::Colour fill = slider.findColour(juce::Slider::trackColourId);
    const float centre = area.getCentreX();
    const float lo = std::min(centre, sliderPos), hi = std::max(centre, sliderPos);
    g.setColour(fill.withAlpha(0.75f));
    g.fillRect(juce::Rectangle<float>(lo, area.getY() + 2.0f, std::max(1.0f, hi - lo), area.getHeight() - 4.0f));
    g.setColour(colours::outline);
    g.drawRoundedRectangle(area.reduced(0.5f), 4.0f, 1.0f);
    g.drawVerticalLine(juce::roundToInt(centre), area.getY() + 2.0f, area.getBottom() - 2.0f);
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
// Drop targets

bool ModDropTarget::isInterestedInDragSource(const SourceDetails& details)
{
    return onLaneDropped != nullptr && details.description.toString().startsWith(kDragPrefix);
}

void ModDropTarget::itemDropped(const SourceDetails& details)
{
    setDropHover(false);
    const int lane = details.description.toString().fromFirstOccurrenceOf(kDragPrefix, false, false).getIntValue();
    if (onLaneDropped && lane >= 0 && lane < gristo::kNumModLanes)
        onLaneDropped(lane);
}

void ModSlider::paint(juce::Graphics& g)
{
    juce::Slider::paint(g);
    if (dropHover)
    {
        g.setColour(colours::text.withAlpha(0.8f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 6.0f, 1.5f);
    }
}

void ModSlider::setDropHover(bool hover)
{
    dropHover = hover;
    repaint();
}

// ---------------------------------------------------------------------------------
// Note selector

NoteSelector::NoteSelector(juce::RangedAudioParameter& parameter)
    : attachment_(parameter, [this](float value) {
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
    g.drawText(p::noteDisplayName(note_), area, juce::Justification::centred);
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
            juce::String text = p::noteDisplayName(n);
            if (const char* gm = juce::MidiMessage::getRhythmInstrumentName(n))
                text << "   " << gm;
            sub.addItem(n + 1, text, true, n == note_);
            containsCurrent |= n == note_;
        }
        menu.addSubMenu("Octave " + juce::String(octave), sub, true, nullptr, containsCurrent);
    }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                       [safe = juce::Component::SafePointer<NoteSelector>(this)](int result) {
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
// Number selector

NumberSelector::NumberSelector(juce::RangedAudioParameter& parameter, juce::String prefix, int minValue, int maxValue)
    : attachment_(parameter, [this](float v) { value_ = juce::roundToInt(v); repaint(); }),
      prefix_(std::move(prefix)), min_(minValue), max_(maxValue)
{
    attachment_.sendInitialUpdate();
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void NumberSelector::setValue(int value)
{
    attachment_.setValueAsCompleteGesture(static_cast<float>(juce::jlimit(min_, max_, value)));
}

void NumberSelector::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(colours::background);
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(isMouseOver() ? colours::text : colours::outline);
    g.drawRoundedRectangle(area, 4.0f, 1.0f);
    g.setColour(colours::text);
    g.setFont(juce::FontOptions(11.5f));
    g.drawText(prefix_ + juce::String(value_), area, juce::Justification::centred);
}

void NumberSelector::mouseDown(const juce::MouseEvent&)
{
    juce::PopupMenu menu;
    for (int v = min_; v <= max_; ++v)
        menu.addItem(v - min_ + 1, prefix_ + juce::String(v), true, v == value_);
    juce::Component::SafePointer<NumberSelector> safe(this);
    const int base = min_;
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [safe, base](int result) {
        if (safe != nullptr && result > 0)
            safe->setValue(base + result - 1);
    });
}

void NumberSelector::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const float delta = std::abs(wheel.deltaY) > std::abs(wheel.deltaX) ? wheel.deltaY : -wheel.deltaX;
    if (delta > 0.0f)
        setValue(value_ + 1);
    else if (delta < 0.0f)
        setValue(value_ - 1);
}

// ---------------------------------------------------------------------------------
// Arp view

void ArpView::paint(juce::Graphics& g)
{
    using namespace gristo;
    const auto bounds = getLocalBounds().toFloat();
    const auto colour = arpColours[arp_];
    g.setColour(colours::background);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(colours::outline);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    const auto a = static_cast<size_t>(arp_);
    std::array<int, kMaxHeldNotes> held {};
    const int heldCount = std::clamp(processor_.displayArpInputCount[a].load(), 0, kMaxHeldNotes);
    for (int i = 0; i < heldCount; ++i)
        held[static_cast<size_t>(i)] = processor_.displayArpInput[a][static_cast<size_t>(i)].load();
    const ArpPool pool = Arp::pool(params_, held.data(), heldCount);

    auto area = bounds.reduced(8.0f, 6.0f);
    auto caption = area.removeFromBottom(14.0f);
    g.setFont(juce::FontOptions(10.5f));
    g.setColour(colours::dim);
    juce::String text;
    if (!params_.enabled)
        text = "Off - switch On to play";
    else if (heldCount > 0)
        text = juce::String(heldCount) + (heldCount == 1 ? " note" : " notes") + " from MIDI in";
    else if (params_.mode == ArpMode::Generate)
        text = juce::MidiMessage::getMidiNoteName(params_.root, true, true, 3) + " " + arpScaleName(params_.scale);
    else
        text = "Hold notes on Gristo's MIDI input";
    g.drawText(text, caption, juce::Justification::centredLeft);
    if (pool.size == 0)
        return;

    const bool playing = processor_.displayPlaying.load();
    const std::int64_t count = processor_.displayArpCount[a].load();
    const int resetLen = Engine::resetSteps(params_);
    const auto step = static_cast<std::int64_t>(std::floor(processor_.displayPpq.load() * kStepsPerBeat));
    const std::int64_t cycle = playing ? floorDiv(step, resetLen) : 0;
    const ArpPattern pattern = Arp::pattern(params_.seed, params_.mutate, cycle);
    const int columns = params_.mode == ArpMode::Generate ? std::clamp(params_.length, 1, kMaxArpSteps)
                                                          : std::min(pool.size, kMaxArpSteps);
    const int current = playing && count > 0 ? static_cast<int>((count - 1) % columns) : -1;
    const float colW = area.getWidth() / static_cast<float>(columns);
    auto heightOf = [&](int note) {
        const int lo = pool.notes[0], hi = pool.notes[static_cast<size_t>(pool.size - 1)];
        return hi > lo ? 0.15f + 0.85f * static_cast<float>(note - lo) / static_cast<float>(hi - lo) : 0.6f;
    };

    juce::Point<float> lastTop;
    bool lastSlide = false;
    for (int i = 0; i < columns; ++i)
    {
        const auto col = juce::Rectangle<float>(area.getX() + i * colW + 1.0f, area.getY(), colW - 2.0f, area.getHeight());
        if (i == current)
        {
            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.fillRect(col);
        }
        const ArpNote n = params_.mode == ArpMode::Generate ? Arp::noteFor(params_, pool, pattern, i, cycle)
                                                            : ArpNote { true, pool.notes[static_cast<size_t>(i)], false, false };
        if (!n.play)
        {
            g.setColour(colour.withAlpha(0.35f));
            g.drawRect(col.withTop(col.getBottom() - 4.0f), 1.0f);
            lastSlide = false;
            continue;
        }
        const float h = col.getHeight() * heightOf(n.note);
        const auto bar = col.withTop(col.getBottom() - h);
        g.setColour(colour.withAlpha(n.accent ? 1.0f : 0.6f));
        g.fillRoundedRectangle(bar, 2.0f);
        if (n.accent)
        {
            g.setColour(juce::Colours::white);
            g.fillEllipse(bar.getCentreX() - 2.0f, bar.getY() - 6.0f, 4.0f, 4.0f);
        }
        if (lastSlide)
        {
            g.setColour(colours::text);
            g.drawLine({ lastTop, { bar.getX(), bar.getY() } }, 1.5f);
        }
        lastTop = { bar.getRight(), bar.getY() };
        lastSlide = n.slide;
    }
}

// ---------------------------------------------------------------------------------
// Velocity range

VelocityRange::VelocityRange(juce::RangedAudioParameter& minParam, juce::RangedAudioParameter& maxParam, juce::Colour colour)
    : colour_(colour),
      minAttachment_(minParam, [this](float v) { min_ = juce::roundToInt(v); repaint(); }),
      maxAttachment_(maxParam, [this](float v) { max_ = juce::roundToInt(v); repaint(); })
{
    minAttachment_.sendInitialUpdate();
    maxAttachment_.sendInitialUpdate();
}

juce::Rectangle<float> VelocityRange::barArea() const
{
    return getLocalBounds().toFloat().reduced(4.0f, 2.0f);
}

int VelocityRange::velocityAt(float x) const
{
    const auto bar = barArea();
    return juce::jlimit(1, 127, 1 + juce::roundToInt((x - bar.getX()) / bar.getWidth() * 126.0f));
}

void VelocityRange::paint(juce::Graphics& g)
{
    const auto bar = barArea();
    g.setColour(colours::background);
    g.fillRoundedRectangle(bar, 4.0f);
    auto xOf = [&](int v) { return bar.getX() + bar.getWidth() * static_cast<float>(v - 1) / 126.0f; };
    const int lo = std::min(min_, max_), hi = std::max(min_, max_);
    const auto range = juce::Rectangle<float>(xOf(lo), bar.getY(), std::max(2.0f, xOf(hi) - xOf(lo)), bar.getHeight());
    g.setColour(colour_.withAlpha(0.55f));
    g.fillRoundedRectangle(range, 3.0f);
    g.setColour(colour_);
    g.fillRect(range.withWidth(2.0f));
    g.fillRect(range.withX(range.getRight() - 2.0f).withWidth(2.0f));

    if (showLive && !rings.empty())
    {
        const float pos = juce::jlimit(0.0f, 1.0f, gristo::kNormalVelocity + liveOffset);
        const float x = range.getX() + range.getWidth() * pos;
        g.setColour(juce::Colours::white);
        g.drawVerticalLine(juce::roundToInt(x), bar.getY() + 1.0f, bar.getBottom() - 1.0f);
    }

    g.setColour(colours::text);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(juce::String(lo) + juce::String::fromUTF8(" \xe2\x80\x93 ") + juce::String(hi), bar, juce::Justification::centred);

    float dx = bar.getRight() - 5.0f;
    for (const ModRing& ring : rings)
    {
        g.setColour(ring.colour);
        g.fillEllipse(dx - 2.5f, bar.getY() + 2.0f, 5.0f, 5.0f);
        dx -= 7.0f;
    }
    g.setColour(dropHover_ ? colours::text : colours::outline);
    g.drawRoundedRectangle(bar, 4.0f, dropHover_ ? 1.5f : 1.0f);
}

void VelocityRange::mouseDown(const juce::MouseEvent& e)
{
    const int v = velocityAt(e.position.x);
    dragging_ = std::abs(v - min_) <= std::abs(v - max_) ? 0 : 1;
    if (min_ == max_)
        dragging_ = v < min_ ? 0 : 1;
    (dragging_ == 0 ? minAttachment_ : maxAttachment_).beginGesture();
    mouseDrag(e);
}

void VelocityRange::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging_ < 0)
        return;
    const int v = velocityAt(e.position.x);
    if (dragging_ == 0)
        minAttachment_.setValueAsPartOfGesture(static_cast<float>(std::min(v, max_)));
    else
        maxAttachment_.setValueAsPartOfGesture(static_cast<float>(std::max(v, min_)));
}

void VelocityRange::mouseUp(const juce::MouseEvent&)
{
    if (dragging_ >= 0)
        (dragging_ == 0 ? minAttachment_ : maxAttachment_).endGesture();
    dragging_ = -1;
}

void VelocityRange::mouseDoubleClick(const juce::MouseEvent&)
{
    minAttachment_.setValueAsCompleteGesture(1.0f);
    maxAttachment_.setValueAsCompleteGesture(127.0f);
}

void VelocityRange::setDropHover(bool hover)
{
    dropHover_ = hover;
    repaint();
}

// ---------------------------------------------------------------------------------
// Map pad

MapPad::MapPad(juce::AudioProcessorValueTreeState& state, const gristo::DrumMap& map) : state_(state), map_(map)
{
    onLaneDropped = [this](int lane) {
        juce::PopupMenu menu;
        menu.addSectionHeader("Route lane " + juce::String(lane + 1) + " to");
        menu.addItem(1, "Map X (straight - syncopated)");
        menu.addItem(2, "Map Y (four on the floor - broken)");
        // (MSVC rejects init-captures in nested lambdas, so build the pointer first.)
        juce::Component::SafePointer<MapPad> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                           [safe, lane](int result) {
                               if (safe != nullptr && result > 0 && safe->onLaneAxisChosen)
                                   safe->onLaneAxisChosen(lane, result == 1 ? gristo::mod::kMapX : gristo::mod::kMapY);
                           });
    };
}

void MapPad::setRandomDisplay(float radius, std::optional<juce::Point<float>> livePosition)
{
    randomRadius_ = radius;
    livePosition_ = livePosition;
}

void MapPad::setDropHover(bool hover)
{
    dropHover_ = hover;
    repaint();
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
    pp.mapRandom = 0.0f;
    pp.fillRandom = {};
    pp.probability.fill(1.0f);
    pp.probabilityRandom = {};

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
        g.setColour(juce::Colours::white);
        g.fillEllipse(lp.x - 4.0f, lp.y - 4.0f, 8.0f, 8.0f);
    }
    g.setColour(colours::accent);
    g.drawEllipse(c.x - 7.0f, c.y - 7.0f, 14.0f, 14.0f, 2.0f);

    g.setColour(dropHover_ ? colours::text : colours::outline);
    g.drawRect(area, dropHover_ ? 2.0f : 1.0f);
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
                g.setColour(voiceColours[v].withAlpha(0.45f));
                g.drawRoundedRectangle(x, lane.getBottom() - 5.0f - barH, std::max(3.0f, stepW * 0.55f), barH, 2.0f, 1.0f);
                continue;
            }
            if (!h.on)
                continue;

            // Straight position tick.
            const float straightX = grid.getX() + s * stepW;
            g.setColour(colours::dim.withAlpha(0.5f));
            g.fillRect(straightX + 1.0f, lane.getBottom() - 3.0f, stepW - 2.0f, 2.0f);

            const auto colour = voiceColours[v].withAlpha(h.accent ? 1.0f : 0.45f + 0.4f * h.velocity);
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
// Lane editor

LaneEditor::LaneEditor(GristoProcessor& processor) : processor_(processor)
{
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
}

void LaneEditor::setLane(int lane)
{
    lane_ = juce::jlimit(0, gristo::kNumModLanes - 1, lane);
    repaint();
}

int LaneEditor::numPoints() const
{
    const int choice = static_cast<int>(processor_.state.getRawParameterValue(p::laneId(p::modBars, lane_))->load());
    return p::barsForChoice(choice) * gristo::kModPointsPerBar;
}

juce::Rectangle<float> LaneEditor::drawArea() const
{
    return getLocalBounds().toFloat().reduced(1.0f, 4.0f);
}

void LaneEditor::applyShape(gristo::ModShape shape)
{
    float pts[gristo::kMaxModPoints] {};
    const int n = numPoints();
    gristo::fillModShape(pts, n, shape, static_cast<std::uint32_t>(juce::Random::getSystemRandom().nextInt()));
    // Fill all 4 bars, repeating the lane, so changing the length later keeps the curve.
    for (int k = 0; k < gristo::kMaxModPoints; ++k)
        p::lanePoint(processor_.modPoints, lane_, k).store(pts[k % n]);
    repaint();
}

void LaneEditor::paintAt(juce::Point<float> pos, bool connectToLast)
{
    const auto area = drawArea();
    const int n = numPoints();
    const int index = juce::jlimit(0, n - 1, static_cast<int>((pos.x - area.getX()) / area.getWidth() * static_cast<float>(n)));
    const float value = juce::jlimit(0.0f, 1.0f, (area.getBottom() - pos.y) / area.getHeight());
    auto set = [&](int i, float v) {
        // Write the point in every repetition of the lane inside the 4-bar store.
        for (int k = i; k < gristo::kMaxModPoints; k += n)
            p::lanePoint(processor_.modPoints, lane_, k).store(v);
    };
    if (connectToLast && lastIndex_ >= 0 && lastIndex_ != index)
    {
        // Fast mouse moves skip steps: interpolate so the drawn curve has no holes.
        const int dir = index > lastIndex_ ? 1 : -1;
        for (int i = lastIndex_ + dir; i != index; i += dir)
        {
            const float t = static_cast<float>(i - lastIndex_) / static_cast<float>(index - lastIndex_);
            set(i, lastValue_ + (value - lastValue_) * t);
        }
    }
    set(index, value);
    lastIndex_ = index;
    lastValue_ = value;
    repaint();
}

void LaneEditor::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        for (int s = 0; s < static_cast<int>(gristo::ModShape::Count); ++s)
            menu.addItem(s + 1, gristo::modShapeName(static_cast<gristo::ModShape>(s)));
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                           [safe = juce::Component::SafePointer<LaneEditor>(this)](int result) {
                               if (safe != nullptr && result > 0)
                                   safe->applyShape(static_cast<gristo::ModShape>(result - 1));
                           });
        return;
    }
    lastIndex_ = -1;
    paintAt(e.position, false);
}

void LaneEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (!e.mods.isPopupMenu())
        paintAt(e.position, true);
}

void LaneEditor::mouseUp(const juce::MouseEvent&) { lastIndex_ = -1; }

void LaneEditor::paint(juce::Graphics& g)
{
    const auto area = drawArea();
    const auto colour = laneColours[lane_];
    const int n = numPoints();
    const float stepW = area.getWidth() / static_cast<float>(n);

    g.setColour(colours::background);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);

    for (int i = 0; i <= n; ++i)
    {
        const bool bar = i % gristo::kStepsPerBar == 0;
        const bool beat = i % gristo::kStepsPerBeat == 0;
        if (!beat && n > 32)
            continue;
        g.setColour(colours::outline.withAlpha(bar ? 1.0f : beat ? 0.55f : 0.2f));
        g.drawVerticalLine(juce::roundToInt(area.getX() + i * stepW), area.getY(), area.getBottom());
    }
    g.setColour(colours::outline.withAlpha(0.4f));
    g.drawHorizontalLine(juce::roundToInt(area.getCentreY()), area.getX(), area.getRight());

    juce::Path line;
    for (int i = 0; i < n; ++i)
    {
        const float v = p::lanePoint(processor_.modPoints, lane_, i).load();
        const float x = area.getX() + i * stepW;
        const float y = area.getBottom() - v * area.getHeight();
        g.setColour(colour.withAlpha(0.28f));
        g.fillRect(juce::Rectangle<float>(x + 1.0f, y, std::max(1.0f, stepW - 2.0f), area.getBottom() - y));
        if (i == 0)
            line.startNewSubPath(x, y);
        else
            line.lineTo(x, y);
        line.lineTo(x + stepW, y);
    }
    g.setColour(colour);
    g.strokePath(line, juce::PathStrokeType(2.0f));

    if (processor_.displayPlaying.load())
    {
        const auto step = static_cast<std::int64_t>(std::floor(processor_.displayPpq.load() * gristo::kStepsPerBeat));
        const int i = gristo::floorMod(step, n);
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.fillRect(juce::Rectangle<float>(area.getX() + i * stepW, area.getY(), stepW, area.getHeight()));
    }

    g.setColour(colours::dim);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("draw with the mouse  -  right-click for shapes", getLocalBounds().reduced(8, 4), juce::Justification::topRight);
}

// ---------------------------------------------------------------------------------
// Lane handle

void LaneHandle::setLane(int lane)
{
    lane_ = lane;
    repaint();
}

void LaneHandle::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced(1.0f);
    const auto colour = laneColours[lane_];
    g.setColour(colour.withAlpha(isMouseOver() ? 0.35f : 0.2f));
    g.fillRoundedRectangle(area, 12.0f);
    g.setColour(colour);
    g.drawRoundedRectangle(area, 12.0f, 1.5f);
    const auto dot = area.withWidth(area.getHeight()).reduced(5.0f);
    g.fillEllipse(dot);
    g.setColour(colours::text);
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawText("Drag lane " + juce::String(lane_ + 1) + " to a knob", area.withTrimmedLeft(area.getHeight()), juce::Justification::centredLeft);
}

void LaneHandle::mouseDrag(const juce::MouseEvent&)
{
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        if (!container->isDragAndDropActive())
            container->startDragging(juce::String(ModDropTarget::kDragPrefix) + juce::String(lane_), this);
}
