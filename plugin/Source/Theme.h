#pragma once

#include <gristo/Arp.h>
#include <gristo/Modulation.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace theme {

// A quiet, warm-neutral dark: the window recedes, panels sit a step above it, controls
// a step above the panels. One warm accent (amber, like backlit encoder rings) marks
// what is active; everything else is greys with a touch of warmth.
namespace colours {
inline const juce::Colour background { 0xff0e0f12 }; // the window, and recessed wells (pads, timeline)
inline const juce::Colour panel { 0xff16171b };
inline const juce::Colour surface { 0xff1f2126 };    // controls: buttons, menus, knob caps
inline const juce::Colour outline { 0xff2a2c33 };    // hairlines and knob tracks
inline const juce::Colour text { 0xffedeae3 };
inline const juce::Colour dim { 0xff8b8880 };
inline const juce::Colour faint { 0xff57554f };
inline const juce::Colour accent { 0xfff3b34c };
inline const juce::Colour hot { 0xffef6a4c };
inline const juce::Colour cool { 0xff4f6fa6 };
inline const juce::Colour random { 0xff6cc4b4 }; // Beads-style random amounts
} // namespace colours

// Section titles: small, spaced capitals.
inline juce::Font titleFont() { return juce::Font(juce::FontOptions(10.5f, juce::Font::bold)).withExtraKerningFactor(0.09f); }

inline const juce::Colour voiceColours[gristo::kNumVoices] = {
    juce::Colour(0xffe8553b), juce::Colour(0xfff08a3a), juce::Colour(0xffefe6d2),
    juce::Colour(0xffc9b458), juce::Colour(0xfff08a3a), juce::Colour(0xffe8553b),
};

// Modulation lane colours (rings on knobs use the colour of the lane driving them).
inline const juce::Colour laneColours[gristo::kNumModLanes] = {
    juce::Colour(0xff4fc3f7), juce::Colour(0xffba68c8), juce::Colour(0xff81c784), juce::Colour(0xffff8a65),
};

// Distinct from the lane colours, so lane rings stay readable on every arp page.
inline const juce::Colour arpColours[gristo::kNumArps] = { juce::Colour(0xfff06292), juce::Colour(0xff9ccc65), juce::Colour(0xffffee58) };

inline void drawPanel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    const auto r = area.toFloat();
    g.setColour(colours::panel);
    g.fillRoundedRectangle(r, 10.0f);
    // A hairline edge, a touch lighter along the top: light from above, barely there.
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.07f), r.getX(), r.getY(),
                                           juce::Colours::white.withAlpha(0.025f), r.getX(), r.getY() + 60.0f, false));
    g.drawRoundedRectangle(r.reduced(0.5f), 10.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour(colours::dim);
        g.setFont(titleFont());
        g.drawText(title.toUpperCase(), area.reduced(14, 7).removeFromTop(14), juce::Justification::topLeft);
    }
}

} // namespace theme
