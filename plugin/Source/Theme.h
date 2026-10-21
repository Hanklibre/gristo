#pragma once

#include <gristo/Arp.h>
#include <gristo/Modulation.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace theme {

namespace colours {
inline const juce::Colour background { 0xff15161a };
inline const juce::Colour panel { 0xff1f2127 };
inline const juce::Colour outline { 0xff33363f };
inline const juce::Colour text { 0xffd9d4c7 };
inline const juce::Colour dim { 0xff7d7a72 };
inline const juce::Colour accent { 0xfff2b544 }; // warm amber, like backlit encoder rings
inline const juce::Colour hot { 0xffe8553b };
inline const juce::Colour cool { 0xff2b3a5c };
inline const juce::Colour random { 0xff5fb8aa }; // Beads-style random amounts
} // namespace colours

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

} // namespace theme
