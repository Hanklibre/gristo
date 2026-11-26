#pragma once

// Chord progressions by style, to start from on the chord chart: each chord in plain
// musical terms (how far its root is above the key's tonic, major or minor, how rich),
// placed on the harmony map of whatever key you are in. And, per style, which chords
// tend to follow which: the chart lights them up as you build a progression.

#include "gristo/Chords.h"

#include <vector>

namespace gristo {

struct StyleChord
{
    int degree = 0;     // semitones above the tonic, 0..11
    bool minor = false; // the chord's quality
    int row = 0;        // the map's rows: 0 triad, 1 add9, 2 7th, 3 9th, 4 11th / 6-9 / sus
    int beats = 4;      // quarter notes
};

struct StyleProgression
{
    const char* name = "";
    bool minorKey = false; // played in the key's minor (or major) mode
    std::vector<StyleChord> chords;
};

struct ChordStyle
{
    const char* name = "";
    const char* about = "";
    std::vector<StyleProgression> progressions;
};

const std::vector<ChordStyle>& chordStyles();

// A style chord on the map of `p` (its key and mode): the cell with that root and
// quality nearest home, at the chord's row; -1 if the map has no such chord.
int styleChordCell(const ChordParams& p, const StyleChord& chord);
// The cells (codes) of a progression on the map of `p`, with each one's beats.
// Chords the map can't place are left out.
std::vector<std::pair<int, int>> styleProgressionCells(const ChordParams& p, const StyleProgression& progression);
// Per cell code, how strongly a style moves to it from `fromCell` (0..1, 0 = never),
// from all of the style's progressions (in the mode of `p`).
std::vector<float> styleSuggestions(const ChordParams& p, int styleIndex, int fromCell);

} // namespace gristo
