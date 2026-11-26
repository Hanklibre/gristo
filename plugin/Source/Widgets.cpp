#include "Widgets.h"

#include "BinaryData.h"

namespace p = gristo::params;
using namespace theme;

// ---------------------------------------------------------------------------------
// Look and feel

GristoLookAndFeel::GristoLookAndFeel()
{
    regular_ = juce::Typeface::createSystemTypefaceFor(GristoFonts::InterRegular_ttf, GristoFonts::InterRegular_ttfSize);
    semibold_ = juce::Typeface::createSystemTypefaceFor(GristoFonts::InterSemiBold_ttf, GristoFonts::InterSemiBold_ttfSize);
    setColour(juce::ResizableWindow::backgroundColourId, colours::background);
    setColour(juce::Slider::textBoxTextColourId, colours::text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, colours::text);
    setColour(juce::ComboBox::backgroundColourId, colours::surface);
    setColour(juce::ComboBox::outlineColourId, colours::outline);
    setColour(juce::ComboBox::textColourId, colours::text);
    setColour(juce::ComboBox::arrowColourId, colours::dim);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1b1c21));
    setColour(juce::PopupMenu::textColourId, colours::text);
    setColour(juce::PopupMenu::headerTextColourId, colours::dim);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha(0.16f));
    setColour(juce::PopupMenu::highlightedTextColourId, colours::accent);
    setColour(juce::ToggleButton::textColourId, colours::text);
    setColour(juce::TextButton::buttonColourId, colours::surface);
    setColour(juce::TextButton::textColourOffId, colours::text);
    setColour(juce::TextEditor::backgroundColourId, colours::surface);
    setColour(juce::TextEditor::outlineColourId, colours::outline);
    setColour(juce::TextEditor::focusedOutlineColourId, colours::accent.withAlpha(0.6f));
    setColour(juce::TextEditor::textColourId, colours::text);
    setColour(juce::TextEditor::highlightColourId, colours::accent.withAlpha(0.3f));
    setColour(juce::CaretComponent::caretColourId, colours::accent);
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff1b1c21));
    setColour(juce::TooltipWindow::textColourId, colours::text);
    setColour(juce::ScrollBar::thumbColourId, colours::outline);
    setColour(juce::AlertWindow::backgroundColourId, colours::panel);
    setColour(juce::AlertWindow::textColourId, colours::text);
    juce::LookAndFeel::setDefaultLookAndFeel(this); // so fonts made anywhere resolve to Inter
}

GristoLookAndFeel::~GristoLookAndFeel()
{
    if (&juce::LookAndFeel::getDefaultLookAndFeel() == this)
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

juce::Typeface::Ptr GristoLookAndFeel::getTypefaceForFont(const juce::Font& font)
{
    if (font.getTypefaceName() == juce::Font::getDefaultSansSerifFontName() && regular_ != nullptr)
        return font.isBold() ? semibold_ : regular_;
    return LookAndFeel_V4::getTypefaceForFont(font);
}

void GristoLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const bool isRandom = slider.getProperties()["random"];
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    auto angleOf = [&](float proportion) { return startAngle + juce::jlimit(0.0f, 1.0f, proportion) * (endAngle - startAngle); };
    const float angle = angleOf(pos);
    const bool small = radius < 18.0f;
    const float stroke = small ? 2.0f : 2.6f;
    const juce::Colour valueColour = isRandom ? colours::random : colours::accent;
    const bool over = slider.isMouseOverOrDragging() && slider.isEnabled();

    // The track and the value: thin arcs.
    const float arcR = radius - stroke * 0.5f;
    // "centred" knobs (50% = as designed) draw their value out from the top.
    const bool centred = slider.getProperties()["centred"];
    const float from = centred ? angleOf(0.5f) : startAngle;
    juce::Path track, value;
    track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    value.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, std::min(from, angle), std::max(from, angle), true);
    const juce::PathStrokeType arcStroke(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour(colours::outline);
    g.strokePath(track, arcStroke);
    g.setColour(slider.isEnabled() ? valueColour : valueColour.withAlpha(0.35f));
    if (centred ? std::abs(pos - 0.5f) > 0.004f : pos > 0.001f)
        g.strokePath(value, arcStroke);
    if (centred) // the design's own setting: a small mark at the top
    {
        g.setColour(colours::outline.brighter(0.5f));
        const auto top = centre.getPointOnCircumference(arcR, from);
        g.fillEllipse(juce::Rectangle<float>(stroke * 1.4f, stroke * 1.4f).withCentre(top));
    }

    // The cap: a quiet disc, lit softly from above.
    const float capR = radius * (small ? 0.58f : 0.66f);
    const auto cap = juce::Rectangle<float>(capR * 2.0f, capR * 2.0f).withCentre(centre);
    g.setGradientFill(juce::ColourGradient(colours::surface.brighter(over ? 0.22f : 0.12f), centre.x, cap.getY(),
                                           colours::surface.darker(0.25f), centre.x, cap.getBottom(), false));
    g.fillEllipse(cap);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawEllipse(cap.reduced(0.5f), 1.0f);

    // Modulation rings (between the cap and the track), one per routed lane, plus the live value.
    if (auto* ms = dynamic_cast<ModSlider*>(&slider))
    {
        float r = arcR - stroke - 1.5f;
        for (const ModRing& ring : ms->rings)
        {
            if (r < capR + 1.0f)
                break;
            juce::Path arc;
            const float a0 = angleOf(std::min(ring.from, ring.to));
            const float a1 = angleOf(std::max(ring.from, ring.to));
            arc.addCentredArc(centre.x, centre.y, r, r, 0.0f, a0, std::max(a1, a0 + 0.02f), true);
            g.setColour(ring.colour);
            g.strokePath(arc, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            r -= 2.4f;
        }
        if (ms->live >= 0.0f)
        {
            const auto dot = centre.getPointOnCircumference(arcR, angleOf(ms->live));
            g.setColour(juce::Colours::white);
            g.fillEllipse(dot.x - 2.2f, dot.y - 2.2f, 4.4f, 4.4f);
        }
    }

    // The pointer: a short line on the cap.
    g.setColour(slider.isEnabled() ? colours::text : colours::dim);
    g.drawLine({ centre.getPointOnCircumference(capR * 0.35f, angle), centre.getPointOnCircumference(capR * 0.85f, angle) },
               small ? 1.6f : 2.0f);
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
    const auto area = juce::Rectangle<int>(x, y, width, height).toFloat();
    g.setColour(colours::surface);
    g.fillRoundedRectangle(area, 7.0f);
    const juce::Colour fill = slider.findColour(juce::Slider::trackColourId);
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float from = bipolar ? area.getCentreX() : area.getX();
    const float lo = std::min(from, sliderPos), hi = std::max(from, sliderPos);
    {
        juce::Graphics::ScopedSaveState clip(g);
        juce::Path rounded;
        rounded.addRoundedRectangle(area, 7.0f);
        g.reduceClipRegion(rounded);
        g.setColour(fill.withAlpha(0.32f));
        g.fillRect(juce::Rectangle<float>(lo, area.getY(), std::max(1.0f, hi - lo), area.getHeight()));
        g.setColour(fill.withAlpha(0.9f));
        g.fillRect(juce::Rectangle<float>(sliderPos - 1.0f, area.getY(), 2.0f, area.getHeight()));
    }
    g.setColour(colours::outline);
    g.drawRoundedRectangle(area.reduced(0.5f), 7.0f, 1.0f);
    if (bipolar) // the centre: two small ticks, clear of the value text
    {
        g.setColour(colours::dim.withAlpha(0.6f));
        g.drawVerticalLine(juce::roundToInt(area.getCentreX()), area.getY() + 2.0f, area.getY() + 5.0f);
        g.drawVerticalLine(juce::roundToInt(area.getCentreX()), area.getBottom() - 5.0f, area.getBottom() - 2.0f);
    }
}

void GristoLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool down)
{
    // A soft pill: off is a quiet surface; on glows in its colour without shouting.
    auto area = b.getLocalBounds().toFloat().reduced(1.5f);
    const bool on = b.getToggleState();
    const juce::Colour colour = b.isColourSpecified(juce::TextButton::buttonOnColourId) ? b.findColour(juce::TextButton::buttonOnColourId) : colours::accent;
    const float corner = std::min(7.0f, area.getHeight() * 0.5f);
    g.setColour(on ? colour.withAlpha(down ? 0.32f : 0.2f) : (highlighted ? colours::surface.brighter(0.08f) : colours::surface));
    g.fillRoundedRectangle(area, corner);
    g.setColour(on ? colour.withAlpha(0.65f) : colours::outline);
    g.drawRoundedRectangle(area.reduced(0.5f), corner, 1.0f);
    g.setColour(on ? colour.brighter(0.15f) : (highlighted ? colours::text : colours::dim.brighter(0.25f)));
    g.setFont(juce::FontOptions(area.getHeight() < 22.0f ? 11.0f : 12.0f, juce::Font::bold));
    g.drawFittedText(b.getButtonText(), area.toNearestInt().reduced(4, 0), juce::Justification::centred, 1, 0.8f);
}

void GristoLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& background, bool highlighted, bool down)
{
    auto area = b.getLocalBounds().toFloat().reduced(1.0f);
    const float corner = std::min(8.0f, area.getHeight() * 0.5f);
    const bool on = b.getToggleState();
    if (on)
    {
        // Active: tinted in its own colour (the part's, or amber), not a solid block.
        const juce::Colour colour = b.findColour(juce::TextButton::buttonOnColourId);
        g.setColour(colours::surface.interpolatedWith(colour, down ? 0.36f : (highlighted ? 0.3f : 0.24f)));
        g.fillRoundedRectangle(area, corner);
        g.setColour(colour.withAlpha(0.6f));
        g.drawRoundedRectangle(area.reduced(0.5f), corner, 1.0f);
        return;
    }
    g.setColour(highlighted ? background.brighter(down ? 0.12f : 0.07f) : background);
    g.fillRoundedRectangle(area, corner);
    g.setColour(juce::Colours::white.withAlpha(highlighted ? 0.09f : 0.05f));
    g.drawRoundedRectangle(area.reduced(0.5f), corner, 1.0f);
}

juce::Font GristoLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(juce::FontOptions(std::min(13.5f, static_cast<float>(buttonHeight) * 0.5f), juce::Font::bold));
}

void GristoLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool highlighted, bool)
{
    const bool on = b.getToggleState();
    juce::Colour colour = on ? b.findColour(juce::TextButton::buttonOnColourId).brighter(0.35f) : b.findColour(juce::TextButton::textColourOffId);
    if (!b.isEnabled())
        colour = colours::faint;
    else if (!on && !highlighted)
        colour = colour.withMultipliedAlpha(0.88f);
    g.setColour(colour);
    g.setFont(getTextButtonFont(b, b.getHeight()));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(6, 2), juce::Justification::centred, 1, 0.75f);
}

void GristoLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto area = juce::Rectangle<int>(width, height).toFloat().reduced(0.5f);
    const bool over = box.isMouseOver(true) && box.isEnabled();
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId).brighter(over ? 0.07f : 0.0f));
    g.fillRoundedRectangle(area, 7.0f);
    g.setColour(juce::Colours::white.withAlpha(over ? 0.09f : 0.05f));
    g.drawRoundedRectangle(area, 7.0f, 1.0f);
    // A thin chevron.
    const float cx = static_cast<float>(width) - 13.0f, cy = static_cast<float>(height) * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath(cx - 4.0f, cy - 1.8f);
    chevron.lineTo(cx, cy + 2.2f);
    chevron.lineTo(cx + 4.0f, cy - 1.8f);
    g.setColour(box.isEnabled() ? box.findColour(juce::ComboBox::arrowColourId) : colours::faint);
    g.strokePath(chevron, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font GristoLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return juce::Font(juce::FontOptions(std::min(13.5f, static_cast<float>(box.getHeight()) * 0.55f)));
}

void GristoLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(4, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}

void GristoLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawRect(0, 0, width, height);
}

juce::Font GristoLookAndFeel::getPopupMenuFont() { return juce::Font(juce::FontOptions(14.0f)); }

void GristoLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto area = juce::Rectangle<int>(width, height).toFloat();
    g.setColour(findColour(juce::TooltipWindow::backgroundColourId));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(area.reduced(0.5f), 6.0f, 1.0f);
    g.setColour(findColour(juce::TooltipWindow::textColourId));
    g.setFont(juce::FontOptions(12.5f));
    g.drawFittedText(text, area.toNearestInt().reduced(8, 4), juce::Justification::centredLeft, 4);
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

NumberSelector::NumberSelector(juce::RangedAudioParameter& parameter, juce::String prefix, int minValue, int maxValue,
                               juce::String suffix, int menuStep, bool showSign)
    : attachment_(parameter, [this](float v) { value_ = juce::roundToInt(v); repaint(); }),
      prefix_(std::move(prefix)), suffix_(std::move(suffix)), min_(minValue), max_(maxValue),
      menuStep_(std::max(1, menuStep)), showSign_(showSign)
{
    attachment_.sendInitialUpdate();
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void NumberSelector::setValue(int value)
{
    attachment_.setValueAsCompleteGesture(static_cast<float>(juce::jlimit(min_, max_, value)));
}

juce::String NumberSelector::text(int value) const
{
    return prefix_ + (showSign_ && value > 0 ? "+" : "") + juce::String(value) + suffix_;
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
    g.drawText(text(value_), area, juce::Justification::centred);
}

void NumberSelector::mouseDown(const juce::MouseEvent&)
{
    juce::PopupMenu menu;
    for (int v = min_; v <= max_; ++v)
        if ((v - min_) % menuStep_ == 0 || v == value_)
            menu.addItem(v - min_ + 1, text(v), true, v == value_);
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
    g.setColour(dropHover_ ? colours::accent : colours::outline);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, dropHover_ ? 2.0f : 1.0f);

    // Lanes routed to Transpose: a dot per lane and the current offset.
    if (!transposeLanes.empty())
    {
        auto tag = bounds.reduced(8.0f, 5.0f).removeFromTop(12.0f);
        g.setFont(juce::FontOptions(10.0f));
        g.setColour(colours::dim);
        const juce::String text = showLive ? "Transpose " + juce::String(liveTranspose > 0 ? "+" : "") + juce::String(liveTranspose)
                                           : juce::String("Transpose");
        g.drawText(text, tag, juce::Justification::centredRight);
        float x = tag.getRight() - static_cast<float>(text.length()) * 5.6f - 8.0f;
        for (const auto& c : transposeLanes)
        {
            g.setColour(c);
            g.fillEllipse(x - 6.0f, tag.getCentreY() - 3.0f, 6.0f, 6.0f);
            x -= 9.0f;
        }
    }

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
    const bool generate = params_.mode == ArpMode::Generate;
    // Arp mode: one column per step of the order's cycle (Up/Down: each note once,
    // Up/Down: there and back), showing the window that holds the current step.
    const int period = params_.order == ArpOrder::UpDown ? std::max(1, 2 * pool.size - 2) : pool.size;
    const int columns = std::min(generate ? std::clamp(params_.length, 1, kMaxArpSteps) : period, kMaxArpSteps);
    const std::int64_t k = playing && count > 0 ? count - 1 : -1;
    const std::int64_t base = k >= 0 && !generate ? k - k % columns : 0;
    const int current = k >= 0 ? static_cast<int>(k % columns) : -1;
    const float colW = area.getWidth() / static_cast<float>(columns);
    // As played keeps the held order, so the range comes from the notes themselves.
    const auto [loIt, hiIt] = std::minmax_element(pool.notes.begin(), pool.notes.begin() + pool.size);
    const int lo = *loIt, hi = *hiIt;
    auto heightOf = [lo, hi](int note) {
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
        const ArpNote n = Arp::noteFor(params_, pool, pattern, generate ? i : base + i, cycle);
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
        key << juce::String(params.pattern.fill[static_cast<size_t>(v)], 3) << ",";
    key << (params.pattern.linear ? "L" : "");
    if (key == heatKey_ && heat_.isValid())
        return;
    // It takes a while (tens of ms on a tablet): while Fill knobs are being dragged or
    // a preset loads, redraw at most a few times a second instead of every frame.
    const auto now = juce::Time::getMillisecondCounter();
    if (heat_.isValid() && now - heatTime_ < 350)
        return;
    heatTime_ = now;
    heatKey_ = key;

#if JUCE_ANDROID || JUCE_IOS
    constexpr int res = 40;
#else
    constexpr int res = 64;
#endif
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

    // A topographic map of how busy the pattern gets: a quiet field from deep slate
    // (sparse) to warm umber (dense), with fine contour lines every tenth of the range
    // and a stronger one every half. The field is smoothed first, like terrain.
    for (int pass = 0; pass < 2; ++pass)
    {
        std::vector<float> blurred(density.size());
        for (int y = 0; y < res; ++y)
            for (int x = 0; x < res; ++x)
            {
                float sum = 0.0f, weight = 0.0f;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const int xx = std::clamp(x + dx, 0, res - 1), yy = std::clamp(y + dy, 0, res - 1);
                        const float w = (dx == 0 ? 2.0f : 1.0f) * (dy == 0 ? 2.0f : 1.0f);
                        sum += density[static_cast<size_t>(yy * res + xx)] * w;
                        weight += w;
                    }
                blurred[static_cast<size_t>(y * res + x)] = sum / weight;
            }
        density.swap(blurred);
    }
    maxDensity = 1e-3f;
    for (float d : density)
        maxDensity = std::max(maxDensity, d);
    constexpr int scale = 5;
    const int size = res * scale;
    auto densityAt = [&](float fx, float fy) {
        const int x0 = std::clamp(static_cast<int>(fx), 0, res - 1), y0 = std::clamp(static_cast<int>(fy), 0, res - 1);
        const int x1 = std::min(x0 + 1, res - 1), y1 = std::min(y0 + 1, res - 1);
        const float ax = fx - static_cast<float>(x0), ay = fy - static_cast<float>(y0);
        auto d = [&](int x, int y) { return density[static_cast<size_t>(y * res + x)]; };
        const float top = d(x0, y0) + (d(x1, y0) - d(x0, y0)) * ax, bottom = d(x0, y1) + (d(x1, y1) - d(x0, y1)) * ax;
        return std::sqrt(std::max(0.0f, top + (bottom - top) * ay) / maxDensity);
    };
    const juce::Colour stops[4] = { juce::Colour(0xff101217), juce::Colour(0xff1a2332), juce::Colour(0xff35312a), juce::Colour(0xff6b4c27) };
    auto ramp = [&](float t) {
        t = std::clamp(t, 0.0f, 1.0f) * 3.0f;
        const int i = std::min(2, static_cast<int>(t));
        return stops[i].interpolatedWith(stops[i + 1], t - static_cast<float>(i));
    };
    std::vector<float> field(static_cast<size_t>(size * size));
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            field[static_cast<size_t>(y * size + x)] = densityAt((static_cast<float>(x) + 0.5f) / scale - 0.5f, (static_cast<float>(y) + 0.5f) / scale - 0.5f);
    const juce::Colour line(0xffe9dcc3);
    heat_ = juce::Image(juce::Image::RGB, size, size, false);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            auto f = [&](int xx, int yy) { return field[static_cast<size_t>(std::clamp(yy, 0, size - 1) * size + std::clamp(xx, 0, size - 1))]; };
            const float t = f(x, y);
            juce::Colour c = ramp(t);
            // Antialiased contours: the distance to the nearest level, in pixels, from the slope.
            const float levels = t * 10.0f;
            const float slope = 10.0f * std::hypot(f(x + 1, y) - f(x - 1, y), f(x, y + 1) - f(x, y - 1)) * 0.5f;
            const float frac = levels - std::floor(levels);
            const float distance = std::min(frac, 1.0f - frac) / std::max(slope, 1e-4f);
            const float alpha = std::clamp(1.0f - distance, 0.0f, 1.0f);
            if (alpha > 0.0f)
            {
                const bool major = static_cast<int>(std::lround(levels)) % 5 == 0;
                c = c.interpolatedWith(line, alpha * (major ? 0.34f : 0.15f));
            }
            heat_.setPixelAt(x, y, c);
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

    g.setColour(colours::text.withAlpha(0.22f));
    for (int ny = 0; ny < map_.height(); ++ny)
        for (int nx = 0; nx < map_.width(); ++nx)
        {
            const float px = area.getX() + area.getWidth() * nx / float(map_.width() - 1);
            const float py = area.getY() + area.getHeight() * ny / float(map_.height() - 1);
            g.fillEllipse(px - 1.5f, py - 1.5f, 3.0f, 3.0f);
        }

    const float x = state_.getRawParameterValue(p::mapX)->load();
    const float y = state_.getRawParameterValue(p::mapY)->load();
    const juce::Point<float> c { area.getX() + x * area.getWidth(), area.getY() + y * area.getHeight() };
    g.setColour(colours::accent.withAlpha(0.28f));
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
    // The puck: a soft halo and a ring.
    g.setColour(colours::accent.withAlpha(0.18f));
    g.fillEllipse(c.x - 13.0f, c.y - 13.0f, 26.0f, 26.0f);
    g.setColour(colours::accent);
    g.drawEllipse(c.x - 7.0f, c.y - 7.0f, 14.0f, 14.0f, 2.0f);
    g.fillEllipse(c.x - 2.0f, c.y - 2.0f, 4.0f, 4.0f);

    g.setColour(dropHover_ ? colours::text : juce::Colours::white.withAlpha(0.06f));
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
    if (!drawn())
        return;
    paintAt(e.position, false);
}

void LaneEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (!e.mods.isPopupMenu() && drawn())
        paintAt(e.position, true);
}

bool LaneEditor::drawn() const
{
    return static_cast<int>(processor_.state.getRawParameterValue(p::laneId(p::modWave, lane_))->load()) == 0;
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

    // An LFO lane shows its wave over the same bars (read-only).
    gristo::ModLane lfo;
    lfo.wave = static_cast<gristo::LaneWave>(static_cast<int>(processor_.state.getRawParameterValue(p::laneId(p::modWave, lane_))->load()));
    lfo.lfoRate = static_cast<int>(processor_.state.getRawParameterValue(p::laneId(p::modRate, lane_))->load());
    const auto playStep = static_cast<std::int64_t>(std::floor(processor_.displayPpq.load() * gristo::kStepsPerBeat));
    const std::int64_t windowStart = processor_.displayPlaying.load() ? gristo::floorDiv(playStep, n) * n : 0;
    juce::Path line;
    for (int i = 0; i < n; ++i)
    {
        const float v = lfo.wave == gristo::LaneWave::Drawn ? p::lanePoint(processor_.modPoints, lane_, i).load() : lfo.lfoAt(windowStart + i);
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
    g.drawText(lfo.wave == gristo::LaneWave::Drawn ? "draw with the mouse  -  right-click for shapes" : "LFO: set to Drawn to draw",
               getLocalBounds().reduced(8, 4), juce::Justification::topRight);
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
    g.setColour(colour.withAlpha(armed_ ? 0.9f : isMouseOver() ? 0.35f : 0.2f));
    g.fillRoundedRectangle(area, 12.0f);
    g.setColour(colour);
    g.drawRoundedRectangle(area, 12.0f, 1.5f);
    const auto dot = area.withWidth(area.getHeight()).reduced(5.0f);
    g.fillEllipse(dot);
    g.setColour(colours::text);
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    if (compact_)
    {
        g.drawText("L" + juce::String(lane_ + 1), area.withTrimmedLeft(area.getHeight() - 4.0f), juce::Justification::centredLeft);
        return;
    }
    g.drawText("Drag lane " + juce::String(lane_ + 1) + " to a knob", area.withTrimmedLeft(area.getHeight()), juce::Justification::centredLeft);
}

void LaneHandle::mouseUp(const juce::MouseEvent& e)
{
    if (!e.mouseWasDraggedSinceMouseDown() && onTap)
        onTap(lane_);
}

void LaneHandle::mouseDrag(const juce::MouseEvent&)
{
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        if (!container->isDragAndDropActive())
            container->startDragging(juce::String(ModDropTarget::kDragPrefix) + juce::String(lane_), this);
}

// ---------------------------------------------------------------------------------
// Harmony pad

namespace {
constexpr int kHalfColumns = (gristo::kHarmonyColumns - 1) / 2;

gristo::HarmonyCell decodeCell(int code) { return { code / 8 - kHalfColumns, code % 8 }; }
} // namespace

HarmonyPad::HarmonyPad(juce::AudioProcessorValueTreeState& state) : state_(state) {}

void HarmonyPad::update(const gristo::ChordParams& params, int playing)
{
    params_ = params;
    if (playing != playing_)
    {
        if (playing_ >= 0)
        {
            trail_[2] = trail_[1];
            trail_[1] = trail_[0];
            trail_[0] = playing_;
        }
        if (playing < 0)
            trail_.fill(-1);
        playing_ = playing;
    }
    repaint();
}

juce::String HarmonyPad::playingName() const
{
    return gristo::Chords::name(params_, playing_ >= 0 ? decodeCell(playing_) : gristo::Chords::homeCell(params_));
}

juce::Rectangle<float> HarmonyPad::cellArea(gristo::HarmonyCell cell) const
{
    const float w = static_cast<float>(getWidth()) / gristo::kHarmonyColumns;
    const float h = static_cast<float>(getHeight()) / gristo::kHarmonyRows;
    return { static_cast<float>(cell.column + kHalfColumns) * w, static_cast<float>(gristo::kHarmonyRows - 1 - cell.row) * h, w, h };
}

void HarmonyPad::paint(juce::Graphics& g)
{
    using namespace gristo;
    g.fillAll(colours::background);
    const HarmonyCell home = Chords::homeCell(params_);
    const bool on = params_.enabled;
    const bool keys = params_.mode == HarmonyMode::Keys;
    for (int column = -kHalfColumns; column <= kHalfColumns; ++column)
        for (int row = 0; row < kHarmonyRows; ++row)
        {
            const HarmonyCell cell { column, row };
            const auto area = cellArea(cell).reduced(1.5f);
            // Dark and cool on the flat side, warm and bright on the sharp side; richer rows glow more.
            const float t = static_cast<float>(column + kHalfColumns) / static_cast<float>(kHarmonyColumns - 1);
            juce::Colour colour = juce::Colour::fromHSV(0.68f - 0.58f * t, 0.36f, 0.24f + 0.12f * t + 0.04f * static_cast<float>(row), 1.0f);
            if (!on)
                colour = colour.withSaturation(0.2f).withMultipliedBrightness(0.8f);
            const int code = (column + kHalfColumns) * 8 + row;
            float glow = code == playing_ ? 1.0f : 0.0f;
            for (int k = 0; k < 3; ++k)
                if (trail_[static_cast<size_t>(k)] == code)
                    glow = std::max(glow, 0.45f - 0.14f * static_cast<float>(k));
            if (glow > 0.0f)
                colour = colour.interpolatedWith(colours::accent, glow);
            g.setColour(colour);
            g.fillRoundedRectangle(area, 6.0f);
            if (column == 0) // the tonic's column
            {
                g.setColour(colours::text.withAlpha(0.16f));
                g.drawRoundedRectangle(area.reduced(0.5f), 6.0f, 1.0f);
            }
            if (cell == home && params_.mode != HarmonyMode::Progression)
            {
                g.setColour(colours::text);
                g.drawRoundedRectangle(area.reduced(1.0f), 6.0f, 2.0f);
            }
            if (code == candidate_ && params_.mode == HarmonyMode::Progression) // the chord last heard
            {
                g.setColour(colours::accent);
                g.drawRoundedRectangle(area.reduced(1.0f), 6.0f, 2.0f);
            }
            if (keys && on) // the two octaves of chord keys
            {
                const int first = Chords::keyCell(params_, 0).column;
                if (column >= first && column < first + 7 && (row == home.row || row == home.row + 1))
                {
                    g.setColour(colours::text.withAlpha(0.6f));
                    g.fillEllipse(area.getCentreX() - 2.0f, area.getBottom() - 8.0f, 4.0f, 4.0f);
                }
            }
            if (params_.mode == HarmonyMode::Progression) // the slots this chord fills, as small numbers
            {
                juce::String slots;
                for (int k = 0; k < kMaxProgression; ++k)
                    if (params_.progression[static_cast<size_t>(k)] == code)
                        slots << (slots.isEmpty() ? "" : " ") << (k + 1);
                if (slots.isNotEmpty())
                {
                    g.setColour(colours::accent);
                    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
                    g.drawText(slots, area.reduced(4.0f, 2.0f).toNearestInt(), juce::Justification::topRight);
                }
            }
            g.setColour(glow > 0.6f ? colours::background : juce::Colours::white.withAlpha(on ? 0.86f : 0.5f));
            g.setFont(juce::FontOptions(area.getWidth() < 34.0f ? 9.5f : 11.0f, juce::Font::bold));
            g.drawFittedText(Chords::name(params_, cell), area.reduced(1.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.55f);
        }
    g.setColour(colours::dim);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("darker", 6, getHeight() - 14, 60, 12, juce::Justification::bottomLeft);
    g.drawText("brighter", getWidth() - 66, getHeight() - 14, 60, 12, juce::Justification::bottomRight);
    g.drawText("richer", 6, 2, 60, 12, juce::Justification::topLeft);
}

void HarmonyPad::setFromMouse(juce::Point<float> pos)
{
    const int column = juce::jlimit(0, gristo::kHarmonyColumns - 1, static_cast<int>(pos.x / (static_cast<float>(getWidth()) / gristo::kHarmonyColumns)));
    const int row = gristo::kHarmonyRows - 1
                    - juce::jlimit(0, gristo::kHarmonyRows - 1, static_cast<int>(pos.y / (static_cast<float>(getHeight()) / gristo::kHarmonyRows)));
    auto set = [this](const juce::String& id, float value) {
        auto* param = state_.getParameter(id);
        const float normalised = param->convertTo0to1(value);
        if (std::abs(param->getValue() - normalised) > 1e-4f)
            param->setValueNotifyingHost(normalised);
    };
    set(p::chordHomeX, gristo::Chords::columnToX(column - kHalfColumns));
    set(p::chordHomeY, gristo::Chords::rowToY(row));
}

gristo::HarmonyCell HarmonyPad::cellAt(juce::Point<float> pos) const
{
    const int column = juce::jlimit(0, gristo::kHarmonyColumns - 1, static_cast<int>(pos.x / (static_cast<float>(getWidth()) / gristo::kHarmonyColumns)));
    const int row = gristo::kHarmonyRows - 1
                    - juce::jlimit(0, gristo::kHarmonyRows - 1, static_cast<int>(pos.y / (static_cast<float>(getHeight()) / gristo::kHarmonyRows)));
    return { column - kHalfColumns, row };
}

void HarmonyPad::setCandidate(int cellCode)
{
    if (cellCode != candidate_)
    {
        candidate_ = cellCode;
        repaint();
    }
}

void HarmonyPad::mouseDown(const juce::MouseEvent& e)
{
    if (onTouched)
        onTouched();
    dragged_ = false;
    if (onCellPressed)
    {
        const auto cell = cellAt(e.position);
        pressedCode_ = gristo::cellCode(cell.column, cell.row);
        if (onCellPressed(cell))
        {
            taken_ = true;
            return;
        }
    }
    taken_ = false;
    state_.getParameter(p::chordHomeX)->beginChangeGesture();
    state_.getParameter(p::chordHomeY)->beginChangeGesture();
    setFromMouse(e.position);
}

void HarmonyPad::mouseDrag(const juce::MouseEvent& e)
{
    if (!taken_)
    {
        setFromMouse(e.position);
        return;
    }
    // A taken press dragged away: carry the chord to the progression.
    if (!dragged_ && e.getDistanceFromDragStart() > 8)
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        {
            dragged_ = true;
            const auto area = cellArea(gristo::Chords::cellFromCode(pressedCode_)).reduced(1.5f).toNearestInt();
            container->startDragging(juce::String(kChordDragPrefix) + juce::String(pressedCode_), this,
                                     juce::ScaledImage(createComponentSnapshot(area, true)));
        }
}

void HarmonyPad::mouseUp(const juce::MouseEvent&)
{
    if (taken_)
    {
        if (onCellReleased)
            onCellReleased();
        return;
    }
    if (onCellReleased)
        onCellReleased();
    state_.getParameter(p::chordHomeX)->endChangeGesture();
    state_.getParameter(p::chordHomeY)->endChangeGesture();
}

void HarmonyPad::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (taken_ && onCellDoubleClicked)
        onCellDoubleClicked(cellAt(e.position));
}

// ---------------------------------------------------------------------------------
// Progression strip

void ProgressionStrip::update(const gristo::ChordParams& params, const Slots& slots, const Slots& beats, bool editable, int playing, float playFraction)
{
    if (slots == slots_ && beats == beats_ && editable == editable_ && playing == playing_ && std::abs(playFraction - playFraction_) < 0.004f
        && params.key == params_.key && params.minor == params_.minor)
        return;
    params_ = params;
    slots_ = slots;
    beats_ = beats;
    editable_ = editable;
    playing_ = playing;
    playFraction_ = playFraction;
    if (!editable_)
        selected_ = -1;
    repaint();
}

void ProgressionStrip::clearSelection()
{
    selected_ = -1;
    repaint();
}

juce::String ProgressionStrip::lengthName(int beats)
{
    if (beats % 4 == 0)
        return juce::String(beats / 4) + (beats == 4 ? " bar" : " bars");
    return juce::String(beats) + "/4";
}

int ProgressionStrip::count() const
{
    int n = 0;
    while (n < gristo::kMaxProgression && slots_[static_cast<size_t>(n)] >= 0)
        ++n;
    return n;
}

float ProgressionStrip::beatWidth() const
{
    // Every slot at least kMinSlot wide, the rest shared by their beats; at most a
    // four-bar strip's scale, so short progressions don't stretch.
    const int n = count();
    int total = 0;
    for (int k = 0; k < n; ++k)
        total += beats_[static_cast<size_t>(k)];
    const float plus = editable_ && n < gristo::kMaxProgression ? kPlusSlot : 0.0f;
    const float free = static_cast<float>(getWidth()) - plus - kMinSlot * static_cast<float>(n);
    const float cap = (static_cast<float>(getWidth()) - plus) / 16.0f - kMinSlot / 4.0f;
    return total > 0 ? juce::jlimit(0.0f, std::max(0.0f, cap), free / static_cast<float>(total)) : 0.0f;
}

juce::Rectangle<float> ProgressionStrip::slotArea(int slot) const
{
    const float scale = beatWidth();
    float x = 0.0f;
    for (int k = 0; k < slot; ++k)
        x += kMinSlot + scale * static_cast<float>(beats_[static_cast<size_t>(k)]);
    const float w = slot < count() ? kMinSlot + scale * static_cast<float>(beats_[static_cast<size_t>(slot)]) : kPlusSlot;
    return { x, 0.0f, w, static_cast<float>(getHeight()) };
}

void ProgressionStrip::paint(juce::Graphics& g)
{
    const int n = count();
    const float scale = beatWidth();
    if (n == 0 && !editable_)
    {
        g.setColour(colours::faint);
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("No chords yet", getLocalBounds(), juce::Justification::centredLeft);
        return;
    }
    for (int k = 0; k < n; ++k)
    {
        const auto area = slotArea(k).reduced(1.5f, 0.0f);
        const int code = slots_[static_cast<size_t>(k)];
        const bool lit = k == playing_;
        const bool chosen = k == selected_ || k == dropSlot_ || k == resizing_;
        g.setColour(lit ? colours::accent.withAlpha(0.2f) : colours::surface);
        g.fillRoundedRectangle(area, 5.0f);
        if (lit) // the playhead
        {
            const float px = area.getX() + area.getWidth() * juce::jlimit(0.0f, 1.0f, playFraction_);
            g.setColour(colours::accent.withAlpha(0.18f));
            g.fillRoundedRectangle(area.withRight(px), 5.0f);
            g.setColour(colours::accent);
            g.fillRect(px - 0.75f, area.getY() + 3.0f, 1.5f, area.getHeight() - 6.0f);
        }
        g.setColour(chosen ? colours::accent : lit ? colours::accent.withAlpha(0.55f) : juce::Colours::white.withAlpha(0.06f));
        g.drawRoundedRectangle(area.reduced(0.5f), 5.0f, chosen ? 1.6f : 1.0f);
        // Beat ticks along the bottom: the quarter-note grid.
        const int beats = beats_[static_cast<size_t>(k)];
        const float beatW = (area.getWidth() - 2.0f) / static_cast<float>(std::max(1, beats));
        if (beatW >= 5.0f)
            for (int b = 1; b < beats; ++b)
            {
                g.setColour(juce::Colours::white.withAlpha(b % 4 == 0 ? 0.3f : 0.16f));
                const float tx = area.getX() + 1.0f + beatW * static_cast<float>(b);
                g.fillRect(tx - 0.5f, area.getBottom() - (b % 4 == 0 ? 9.0f : 6.0f), 1.0f, b % 4 == 0 ? 7.0f : 4.0f);
            }
        const auto text = area.toNearestInt().reduced(5, 2);
        g.setColour(editable_ ? colours::text : colours::dim);
        g.setFont(juce::FontOptions(std::clamp(area.getHeight() * 0.36f, 10.5f, 16.0f), juce::Font::bold));
        g.drawFittedText(gristo::Chords::name(params_, gristo::Chords::cellFromCode(code)), text.withTrimmedBottom(text.getHeight() / 3),
                         juce::Justification::centred, 1, 0.55f);
        if (area.getWidth() > 40.0f && area.getHeight() > 30.0f)
        {
            g.setColour(colours::dim);
            g.setFont(juce::FontOptions(10.0f));
            g.drawText(lengthName(beats), text.withTop(text.getBottom() - 14).translated(0, -4), juce::Justification::centred);
        }
        if (editable_ && (chosen || area.getWidth() > 44.0f)) // the handle to drag the length
        {
            g.setColour(chosen ? colours::accent : juce::Colours::white.withAlpha(0.18f));
            g.fillRoundedRectangle(area.getRight() - 4.0f, area.getCentreY() - 7.0f, 2.0f, 14.0f, 1.0f);
        }
    }
    juce::ignoreUnused(scale);
    if (editable_ && n < gristo::kMaxProgression) // where the next chord goes
    {
        const auto area = slotArea(n).reduced(1.5f, 0.0f);
        g.setColour(n == dropSlot_ ? colours::accent.withAlpha(0.2f) : colours::background);
        g.fillRoundedRectangle(area, 5.0f);
        g.setColour(n == dropSlot_ ? colours::accent : juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(area.reduced(0.5f), 5.0f, 1.0f);
        g.setColour(colours::faint);
        g.setFont(juce::FontOptions(16.0f));
        g.drawText("+", area, juce::Justification::centred);
    }
}

int ProgressionStrip::slotAt(float x) const
{
    const int n = count();
    for (int k = 0; k < n; ++k)
        if (x < slotArea(k).getRight())
            return k;
    return std::min(n, gristo::kMaxProgression - 1);
}

int ProgressionStrip::edgeAt(float x) const
{
    if (!editable_)
        return -1;
    for (int k = 0; k < count(); ++k)
        if (std::abs(x - slotArea(k).getRight()) < 6.0f)
            return k;
    return -1;
}

bool ProgressionStrip::isInterestedInDragSource(const SourceDetails& details)
{
    return editable_ && details.description.toString().startsWith(HarmonyPad::kChordDragPrefix);
}

void ProgressionStrip::itemDragMove(const SourceDetails& details)
{
    const int slot = slotAt(static_cast<float>(details.localPosition.x));
    if (slot != dropSlot_)
    {
        dropSlot_ = slot;
        repaint();
    }
}

void ProgressionStrip::itemDragExit(const SourceDetails&)
{
    dropSlot_ = -1;
    repaint();
}

void ProgressionStrip::itemDropped(const SourceDetails& details)
{
    const int slot = slotAt(static_cast<float>(details.localPosition.x));
    dropSlot_ = -1;
    repaint();
    const int code = details.description.toString().fromFirstOccurrenceOf(HarmonyPad::kChordDragPrefix, false, false).getIntValue();
    if (onDrop)
        onDrop(slot, code);
}

void ProgressionStrip::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(edgeAt(e.position.x) >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void ProgressionStrip::mouseDown(const juce::MouseEvent& e)
{
    if (!editable_)
        return;
    const int n = count();
    if (!e.mods.isPopupMenu())
        if (const int edge = edgeAt(e.position.x); edge >= 0)
        {
            resizing_ = edge;
            resizeStartBeats_ = beats_[static_cast<size_t>(edge)];
            resizeScale_ = std::max(4.0f, beatWidth());
            repaint();
            return;
        }
    const int slot = slotAt(e.position.x);
    if (e.mods.isPopupMenu())
    {
        if (slot >= n)
            return;
        selected_ = slot;
        repaint();
        juce::PopupMenu lengths;
        const int current = beats_[static_cast<size_t>(slot)];
        for (int b : { 1, 2, 3, 4, 5, 6, 7, 8, 12, 16 })
            lengths.addItem(100 + b, lengthName(b) + (b < 4 ? "  (" + juce::String(b) + (b == 1 ? " beat)" : " beats)") : juce::String()), true, b == current);
        juce::PopupMenu menu;
        menu.addSectionHeader(gristo::Chords::name(params_, gristo::Chords::cellFromCode(slots_[static_cast<size_t>(slot)])));
        menu.addSubMenu("Length", lengths);
        menu.addItem(2, "Duplicate", n < gristo::kMaxProgression);
        menu.addItem(3, "Remove");
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [this, slot](int result) {
            if (result >= 100 && onLength)
                onLength(slot, result - 100);
            else if (result == 2 && onDuplicate)
                onDuplicate(slot);
            else if (result == 3 && onRemove)
            {
                onRemove(slot);
                selected_ = -1;
            }
            repaint();
        });
        return;
    }
    selected_ = selected_ == slot || slot >= n ? -1 : slot;
    if (onSelect)
        onSelect(selected_);
    repaint();
}

void ProgressionStrip::mouseDrag(const juce::MouseEvent& e)
{
    if (resizing_ < 0)
        return;
    const int beats = juce::jlimit(1, gristo::kMaxSlotBeats,
                                   resizeStartBeats_ + juce::roundToInt(static_cast<float>(e.getDistanceFromDragStartX()) / resizeScale_));
    if (beats != beats_[static_cast<size_t>(resizing_)] && onLength)
    {
        beats_[static_cast<size_t>(resizing_)] = beats; // at once, before the parameter comes back
        onLength(resizing_, beats);
        repaint();
    }
}

void ProgressionStrip::mouseUp(const juce::MouseEvent&)
{
    if (resizing_ >= 0)
    {
        resizing_ = -1;
        repaint();
    }
}

// ---------------------------------------------------------------------------------
// Circles

void CircleView::update(const Rings& rings, const std::array<int, gristo::kNumVoices>& playing, int selected)
{
    if (rings == rings_ && playing == playing_ && selected == selected_)
        return;
    rings_ = rings;
    playing_ = playing;
    selected_ = selected;
    repaint();
}

void CircleView::setEdit(Edit edit)
{
    edit_ = edit;
    repaint();
}

float CircleView::editValue(const gristo::CircleStep& s, Edit edit)
{
    switch (edit)
    {
        case Edit::Chance: return static_cast<float>(s.chance) / 100.0f;
        case Edit::Nudge: return 0.5f + 0.5f * static_cast<float>(s.nudge) / static_cast<float>(gristo::kMaxNudge);
        case Edit::Repeat: return static_cast<float>(s.repeat) / static_cast<float>(gristo::kMaxRepeat);
        case Edit::Velocity: break;
    }
    return static_cast<float>(s.velocity) / 127.0f;
}

void CircleView::ringRadii(int voice, float& inner, float& outer) const
{
    const float radius = std::min(getWidth(), getHeight()) * 0.5f - 4.0f;
    const float hole = radius * 0.2f;
    const float band = (radius - hole) / static_cast<float>(gristo::kNumVoices);
    outer = radius - band * static_cast<float>(voice);
    inner = outer - band + std::max(2.0f, band * 0.12f);
}

void CircleView::paint(juce::Graphics& g)
{
    const auto centre = getLocalBounds().toFloat().getCentre();
    const float twoPi = juce::MathConstants<float>::twoPi;
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        const Ring& ring = rings_[static_cast<size_t>(v)];
        float inner = 0.0f, outer = 0.0f;
        ringRadii(v, inner, outer);
        const int n = std::clamp(ring.length, 1, gristo::kPatternSteps);
        const float gap = std::min(0.06f, 0.35f * twoPi / static_cast<float>(n)); // radians between steps
        const juce::Colour colour = voiceColours[v].withMultipliedAlpha(ring.muted ? 0.35f : 1.0f);
        const bool selected = v == selected_;
        for (int k = 0; k < n; ++k)
        {
            const float a0 = twoPi * static_cast<float>(k) / static_cast<float>(n) + gap * 0.5f;
            const float a1 = twoPi * static_cast<float>(k + 1) / static_cast<float>(n) - gap * 0.5f;
            auto segment = [&](float r0, float r1) {
                juce::Path path;
                path.addPieSegment(centre.x - r1, centre.y - r1, r1 * 2.0f, r1 * 2.0f, a0, a1, r0 / r1);
                return path;
            };
            const gristo::CircleStep step = gristo::decodeCircleStep(ring.steps[static_cast<size_t>(k)]);
            const bool playing = k == playing_[static_cast<size_t>(v)];
            g.setColour(selected ? colours::surface.brighter(0.12f) : colours::surface.withAlpha(0.6f));
            g.fillPath(segment(inner, outer));
            if (step.on)
            {
                // The step, its value (in the edit mode) as how far it is filled outwards.
                const float value = juce::jlimit(0.06f, 1.0f, editValue(step, edit_));
                const float alpha = 0.35f + 0.65f * static_cast<float>(step.chance) / 100.0f;
                g.setColour(colour.withMultipliedAlpha(0.28f * alpha));
                g.fillPath(segment(inner, outer));
                g.setColour(colour.withMultipliedAlpha(alpha));
                if (edit_ == Edit::Nudge) // a bar off the centre line, early or late
                {
                    const float mid = (a0 + a1) * 0.5f, offset = (value - 0.5f) * (a1 - a0);
                    juce::Path bar;
                    bar.addPieSegment(centre.x - outer, centre.y - outer, outer * 2.0f, outer * 2.0f, mid + std::min(0.0f, offset) - 0.02f,
                                      mid + std::max(0.0f, offset) + 0.02f, inner / outer);
                    g.fillPath(bar);
                }
                else
                    g.fillPath(segment(inner, inner + (outer - inner) * value));
                if (step.repeat > 1) // repeats: small dots
                    for (int r = 0; r < step.repeat; ++r)
                    {
                        const float a = a0 + (a1 - a0) * (static_cast<float>(r) + 0.5f) / static_cast<float>(step.repeat) - juce::MathConstants<float>::halfPi;
                        const float rr = outer - 3.5f;
                        g.setColour(colours::background.withAlpha(0.8f));
                        g.fillEllipse(centre.x + rr * std::cos(a) - 1.5f, centre.y + rr * std::sin(a) - 1.5f, 3.0f, 3.0f);
                    }
            }
            if (playing)
            {
                g.setColour(step.on ? juce::Colours::white.withAlpha(0.85f) : colour.withAlpha(0.6f));
                g.strokePath(segment(inner, outer), juce::PathStrokeType(step.on ? 2.0f : 1.2f));
            }
        }
        if (selected) // the selected ring: a thin line just outside it
        {
            juce::Path arc;
            arc.addCentredArc(centre.x, centre.y, outer + 1.5f, outer + 1.5f, 0.0f, 0.0f, twoPi, true);
            g.setColour(colour.withAlpha(0.7f));
            g.strokePath(arc, juce::PathStrokeType(1.0f));
        }
    }
    // The centre: what the drag changes.
    float inner = 0.0f, outer = 0.0f;
    ringRadii(gristo::kNumVoices - 1, inner, outer);
    const char* names[] = { "velocity", "chance", "nudge", "repeat" };
    g.setColour(colours::dim);
    g.setFont(juce::FontOptions(std::clamp(inner * 0.32f, 9.0f, 13.0f)));
    g.drawText(names[static_cast<int>(edit_)], juce::Rectangle<float>(inner * 2.0f, 20.0f).withCentre(centre), juce::Justification::centred);
}

CircleView::Hit CircleView::hitAt(juce::Point<float> p) const
{
    const auto centre = getLocalBounds().toFloat().getCentre();
    const float distance = p.getDistanceFrom(centre);
    Hit hit;
    for (int v = 0; v < gristo::kNumVoices; ++v)
    {
        float inner = 0.0f, outer = 0.0f;
        ringRadii(v, inner, outer);
        if (distance <= outer + 1.0f && distance >= inner - 2.0f)
        {
            const int n = std::clamp(rings_[static_cast<size_t>(v)].length, 1, gristo::kPatternSteps);
            float angle = std::atan2(p.x - centre.x, centre.y - p.y); // 0 at 12 o'clock, clockwise
            if (angle < 0.0f)
                angle += juce::MathConstants<float>::twoPi;
            hit.voice = v;
            hit.step = std::clamp(static_cast<int>(angle / juce::MathConstants<float>::twoPi * static_cast<float>(n)), 0, n - 1);
            return hit;
        }
    }
    return hit;
}

void CircleView::mouseDown(const juce::MouseEvent& e)
{
    pressed_ = hitAt(e.position);
    dragged_ = false;
    if (pressed_.voice < 0)
        return;
    if (pressed_.voice != selected_)
    {
        selected_ = pressed_.voice;
        if (onSelect)
            onSelect(selected_);
    }
    pressedStep_ = gristo::decodeCircleStep(rings_[static_cast<size_t>(pressed_.voice)].steps[static_cast<size_t>(pressed_.step)]);
    wasOn_ = pressedStep_.on;
    if (!wasOn_) // set it at once (clearing waits for the mouse up: a drag edits instead)
    {
        pressedStep_.on = true;
        rings_[static_cast<size_t>(pressed_.voice)].steps[static_cast<size_t>(pressed_.step)] = gristo::encodeCircleStep(pressedStep_);
        if (onStep)
            onStep(pressed_.voice, pressed_.step, gristo::encodeCircleStep(pressedStep_));
    }
    repaint();
}

void CircleView::mouseDrag(const juce::MouseEvent& e)
{
    if (pressed_.voice < 0 || (!dragged_ && std::abs(e.getDistanceFromDragStartY()) < 3))
        return;
    dragged_ = true;
    const float delta = -static_cast<float>(e.getDistanceFromDragStartY()) / 120.0f; // up: more
    gristo::CircleStep s = pressedStep_;
    const gristo::CircleStep from = gristo::decodeCircleStep(rings_[static_cast<size_t>(pressed_.voice)].steps[static_cast<size_t>(pressed_.step)]);
    s.on = true;
    s.velocity = from.velocity;
    s.chance = from.chance;
    s.nudge = from.nudge;
    s.repeat = from.repeat;
    switch (edit_)
    {
        case Edit::Velocity: s.velocity = juce::jlimit(1, 127, juce::roundToInt(pressedStep_.velocity + delta * 127.0f)); break;
        case Edit::Chance: s.chance = juce::jlimit(0, 100, juce::roundToInt(pressedStep_.chance + delta * 100.0f)); break;
        case Edit::Nudge: s.nudge = juce::jlimit(-gristo::kMaxNudge, gristo::kMaxNudge, juce::roundToInt(pressedStep_.nudge + delta * 2.0f * gristo::kMaxNudge)); break;
        case Edit::Repeat: s.repeat = juce::jlimit(1, gristo::kMaxRepeat, juce::roundToInt(pressedStep_.repeat + delta * 4.0f)); break;
    }
    const int code = gristo::encodeCircleStep(s);
    if (code != rings_[static_cast<size_t>(pressed_.voice)].steps[static_cast<size_t>(pressed_.step)])
    {
        rings_[static_cast<size_t>(pressed_.voice)].steps[static_cast<size_t>(pressed_.step)] = code;
        if (onStep)
            onStep(pressed_.voice, pressed_.step, code);
        repaint();
    }
}

void CircleView::mouseUp(const juce::MouseEvent&)
{
    if (pressed_.voice >= 0 && wasOn_ && !dragged_) // a click on a set step clears it
    {
        gristo::CircleStep s = pressedStep_;
        s.on = false;
        const int code = gristo::encodeCircleStep(s);
        rings_[static_cast<size_t>(pressed_.voice)].steps[static_cast<size_t>(pressed_.step)] = code;
        if (onStep)
            onStep(pressed_.voice, pressed_.step, code);
        repaint();
    }
    pressed_ = {};
}

void DragOutHandle::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(colours::panel.brighter(isMouseOver() ? 0.12f : 0.05f));
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(colours::accent.withAlpha(0.8f));
    g.drawRoundedRectangle(r, 5.0f, 1.0f);
    // Grip dots, then the label.
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 2; ++col)
            g.fillEllipse(7.0f + col * 5.0f, r.getCentreY() - 6.0f + row * 5.0f, 2.5f, 2.5f);
    g.setColour(colours::text);
    g.setFont(juce::FontOptions(12.0f));
    g.drawFittedText(text, getLocalBounds().withTrimmedLeft(20).withTrimmedRight(4), juce::Justification::centredLeft, 1, 0.8f);
}

// ---------------------------------------------------------------------------------

void TimelineView::update(double ppq, bool playing, int bars, int selectedPart)
{
    bars = std::max(1, bars);
    const double length = bars * 4.0;
    const double windowStart = playing ? std::floor(ppq / length) * length : 0.0;
    // Notes held on MIDI in change what the arps play.
    std::int64_t inputKey = 0;
    for (int a = 0; a < gristo::kNumArps; ++a)
    {
        const int count = processor_.displayArpInputCount[static_cast<size_t>(a)].load();
        inputKey = inputKey * 31 + count;
        for (int i = 0; i < std::min(count, static_cast<int>(gristo::kMaxHeldNotes)); ++i)
            inputKey = inputKey * 131 + processor_.displayArpInput[static_cast<size_t>(a)][static_cast<size_t>(i)].load();
    }
    const auto generation = processor_.parameterGeneration();
    if (generation != generation_ || inputKey != inputKey_ || bars != bars_ || !juce::exactlyEqual(windowStart, windowStart_))
        stale_ = true;
    generation_ = generation;
    inputKey_ = inputKey;
    const bool moved = !juce::exactlyEqual(ppq, ppq_) || playing != playing_ || selectedPart != selectedPart_;
    ppq_ = ppq;
    playing_ = playing;
    bars_ = bars;
    selectedPart_ = selectedPart;
    windowStart_ = windowStart;
    // Re-rendering the window is cheap but not free: at most a few times a second
    // while knobs move (a new window while playing is rendered at once).
    const auto now = juce::Time::getMillisecondCounter();
    if (stale_ && (now - renderedAt_ > 200 || spans_.empty()))
    {
        spans_ = processor_.renderSpans(windowStart_, windowStart_ + length);
        renderedAt_ = now;
        stale_ = false;
        repaint();
    }
    else if (moved)
        repaint();
}

bool TimelineView::cellAt(juce::Point<float> pos, int& voice, std::int64_t& step) const
{
    const auto grid = getLocalBounds().toFloat().withTrimmedLeft(static_cast<float>(kLabelWidth));
    if (!grid.contains(pos))
        return false;
    voice = static_cast<int>((pos.y - grid.getY()) / (grid.getHeight() / gristo::kNumParts));
    if (voice < 0 || voice >= gristo::kNumVoices)
        return false;
    const int steps = bars_ * gristo::kStepsPerBar;
    const int cell = juce::jlimit(0, steps - 1, static_cast<int>((pos.x - grid.getX()) / grid.getWidth() * static_cast<float>(steps)));
    step = static_cast<std::int64_t>(std::llround(windowStart_ * gristo::kStepsPerBeat)) + cell;
    return true;
}

bool TimelineView::hitAt(int voice, std::int64_t step) const
{
    const double start = static_cast<double>(step) / gristo::kStepsPerBeat;
    for (const auto& span : spans_) // a hit near its step (the groove moves it a little)
        if (span.part == voice && span.start > start - 0.12 && span.start < start + 0.2)
            return true;
    return false;
}

// Drum rows: click a step to force a hit there (or a rest, where one plays); drag to
// paint the same; right-click clears the step back to what the map generates.
void TimelineView::mouseDown(const juce::MouseEvent& e)
{
    const int part = juce::jlimit(0, gristo::kNumParts - 1, e.y * gristo::kNumParts / std::max(1, getHeight()));
    if (onSelect)
        onSelect(part);
    int voice = -1;
    std::int64_t step = 0;
    dragEdit_ = -1;
    if (!cellAt(e.position, voice, step))
        return;
    // The step in the drum's own loop (its Length and clock divider); none off its clock.
    const int loopStep = loopStepOf(voice, step);
    if (loopStep < 0)
        return;
    const int current = processor_.stepEdit(voice, loopStep);
    if (e.mods.isPopupMenu())
        dragEdit_ = 0;
    else if (current != 0)
        dragEdit_ = current == 1 ? 2 : 1; // flip a hand edit
    else
        dragEdit_ = hitAt(voice, step) ? 2 : 1;
    dragVoice_ = voice;
    dragStep_ = loopStep;
    processor_.setStepEdit(voice, loopStep, dragEdit_);
    stale_ = true;
    renderedAt_ = 0;
}

void TimelineView::mouseDrag(const juce::MouseEvent& e)
{
    int voice = -1;
    std::int64_t step = 0;
    if (dragEdit_ < 0 || !cellAt(e.position, voice, step) || voice != dragVoice_)
        return;
    const int loopStep = loopStepOf(voice, step);
    if (loopStep < 0 || loopStep == dragStep_)
        return;
    dragStep_ = loopStep;
    processor_.setStepEdit(voice, loopStep, dragEdit_);
    stale_ = true;
    renderedAt_ = 0;
}

int TimelineView::loopStepOf(int voice, std::int64_t step) const
{
    auto value = [&](const juce::String& id) { return static_cast<int>(processor_.state.getRawParameterValue(gristo::params::voiceId(id, voice))->load()); };
    return gristo::voiceLoopStep(step, value(gristo::params::voiceLength), value(gristo::params::voiceDivide));
}

void TimelineView::paint(juce::Graphics& g)
{
    using namespace gristo;
    const auto area = getLocalBounds().toFloat();
    const auto grid = area.withTrimmedLeft(static_cast<float>(kLabelWidth));
    const float rowH = grid.getHeight() / kNumParts;
    const double length = bars_ * 4.0;
    auto xOf = [&](double beats) { return grid.getX() + static_cast<float>((beats - windowStart_) / length) * grid.getWidth(); };
    auto colourOf = [](int part) {
        return part < kNumVoices ? voiceColours[part] : part < kChordVoice ? arpColours[part - kNumVoices] : colours::random;
    };

    g.setColour(colours::background.brighter(0.02f));
    g.fillRect(grid);
    for (int part = 0; part < kNumParts; ++part)
    {
        const auto row = grid.withY(grid.getY() + part * rowH).withHeight(rowH);
        if (part == selectedPart_)
        {
            g.setColour(colourOf(part).withAlpha(0.10f));
            g.fillRect(row);
        }
        g.setColour(part == selectedPart_ ? colourOf(part) : colours::dim);
        g.setFont(juce::FontOptions(std::min(10.5f, rowH + 1.5f)));
        g.drawText(partName(part), juce::Rectangle<float>(area.getX(), row.getY(), kLabelWidth - 6.0f, rowH), juce::Justification::centredRight);
        if (part == kNumVoices || part == kChordVoice)
        {
            g.setColour(colours::outline.withAlpha(0.6f));
            g.drawHorizontalLine(juce::roundToInt(row.getY()), grid.getX(), grid.getRight());
        }
    }
    const int steps = bars_ * kStepsPerBar;
    for (int s = 0; s <= steps; ++s)
    {
        const bool bar = s % kStepsPerBar == 0, beat = s % kStepsPerBeat == 0;
        if (!beat && bars_ > 2)
            continue;
        g.setColour(colours::outline.withAlpha(bar ? 1.0f : beat ? 0.55f : 0.18f));
        g.drawVerticalLine(juce::roundToInt(grid.getX() + grid.getWidth() * static_cast<float>(s) / static_cast<float>(steps)), grid.getY(), grid.getBottom());
    }

    const float minW = std::max(2.0f, grid.getWidth() / static_cast<float>(steps) * 0.45f);
    for (const auto& span : spans_)
    {
        if (span.part < 0 || span.part >= kNumParts)
            continue;
        const float y = grid.getY() + span.part * rowH;
        const float x = xOf(span.start);
        const bool drum = span.part < kNumVoices;
        const float w = drum ? minW : std::max(minW, xOf(span.end) - x - 1.0f);
        g.setColour(colourOf(span.part).withAlpha(0.4f + 0.6f * juce::jlimit(0.0f, 1.0f, span.velocity)));
        g.fillRoundedRectangle(juce::Rectangle<float>(x, y + 1.0f, w, std::max(2.0f, rowH - 2.0f)).getIntersection(grid), 1.5f);
    }

    // Hand-edited drum steps: a ring on a forced hit, a cross on a forced rest.
    for (int v = 0; v < kNumVoices; ++v)
    {
        const auto first = static_cast<std::int64_t>(std::llround(windowStart_ * kStepsPerBeat));
        for (int s = 0; s < steps; ++s)
        {
            const int loopStep = loopStepOf(v, first + s);
            const int edit = loopStep < 0 ? 0 : processor_.stepEdit(v, loopStep);
            if (edit == 0)
                continue;
            const float x = grid.getX() + grid.getWidth() * static_cast<float>(s) / static_cast<float>(steps);
            const float w = grid.getWidth() / static_cast<float>(steps);
            const auto cell = juce::Rectangle<float>(x, grid.getY() + v * rowH, w, rowH).reduced(1.0f, 0.5f);
            g.setColour(edit == 1 ? colours::text : colours::hot);
            if (edit == 1)
                g.drawRoundedRectangle(cell, 1.5f, 1.2f);
            else
            {
                g.drawLine(cell.getX(), cell.getY(), cell.getRight(), cell.getBottom(), 1.2f);
                g.drawLine(cell.getX(), cell.getBottom(), cell.getRight(), cell.getY(), 1.2f);
            }
        }
    }

    if (playing_)
    {
        g.setColour(colours::accent);
        g.drawVerticalLine(juce::roundToInt(xOf(ppq_)), grid.getY(), grid.getBottom());
    }
}
