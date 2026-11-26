#include "gristo/ChordStyles.h"

#include <algorithm>
#include <cmath>

namespace gristo {

namespace {
constexpr bool M = false, m = true;
constexpr int kHalf = (kHarmonyColumns - 1) / 2;
// Triad, add9, 7th, 9th, 11th / 6-9 / sus: the map's rows.
constexpr int T = 0, A9 = 1, S7 = 2, N9 = 3, X = 4;

std::vector<ChordStyle> build()
{
    // Degrees are semitones above the tonic of the progression's own mode.
    return {
        { "Jazz", "ii-V-Is, turnarounds, rich sevenths and ninths",
          { { "ii - V - I", M, { { 2, m, N9, 4 }, { 7, M, N9, 4 }, { 0, M, N9, 8 } } },
            { "I - vi - ii - V (turnaround)", M, { { 0, M, S7, 4 }, { 9, m, S7, 4 }, { 2, m, S7, 4 }, { 7, M, S7, 4 } } },
            { "iii - vi - ii - V - I", M, { { 4, m, S7, 4 }, { 9, m, S7, 4 }, { 2, m, N9, 4 }, { 7, M, N9, 4 }, { 0, M, N9, 16 } } },
            { "II7 - V7 - I (bridge)", M, { { 2, M, S7, 8 }, { 7, M, S7, 8 }, { 0, M, N9, 16 } } },
            { "Minor blues i - iv - i - v", m, { { 0, m, S7, 8 }, { 5, m, S7, 8 }, { 0, m, S7, 4 }, { 7, m, S7, 4 } } } } },
        { "Neo-soul", "Lush ninths that drift, the IV and the vi at the heart",
          { { "IVmaj9 - iii7 - vi9", M, { { 5, M, N9, 4 }, { 4, m, S7, 4 }, { 9, m, N9, 8 } } },
            { "ii9 - Imaj9", M, { { 2, m, N9, 8 }, { 0, M, N9, 8 } } },
            { "IV - V - iii - vi (royal road)", M, { { 5, M, S7, 4 }, { 7, M, X, 4 }, { 4, m, S7, 4 }, { 9, m, N9, 4 } } },
            { "vi9 - ii9 - Imaj9", M, { { 9, m, N9, 4 }, { 2, m, N9, 4 }, { 0, M, N9, 8 } } },
            { "i9 - iv9 (minor vamp)", m, { { 0, m, N9, 8 }, { 5, m, N9, 8 } } } } },
        { "Lo-fi", "Dusty sevenths walking down, a bit wistful",
          { { "IVmaj7 - iii7 - ii7 - Imaj7", M, { { 5, M, S7, 4 }, { 4, m, S7, 4 }, { 2, m, S7, 4 }, { 0, M, S7, 4 } } },
            { "ii7 - V - Imaj7 - vi7", M, { { 2, m, S7, 4 }, { 7, M, N9, 4 }, { 0, M, S7, 4 }, { 9, m, S7, 4 } } },
            { "i9 - VImaj7 - III - VII", m, { { 0, m, N9, 4 }, { 8, M, S7, 4 }, { 3, M, S7, 4 }, { 10, M, A9, 4 } } } } },
        { "Trap", "Minor, dark, simple: triads over the 808",
          { { "i - VI - III - VII", m, { { 0, m, T, 4 }, { 8, M, T, 4 }, { 3, M, T, 4 }, { 10, M, T, 4 } } },
            { "i - iv (dark)", m, { { 0, m, A9, 8 }, { 5, m, A9, 8 } } },
            { "i - VI - iv - v", m, { { 0, m, T, 4 }, { 8, M, T, 4 }, { 5, m, T, 4 }, { 7, m, T, 4 } } },
            { "i - bII (Phrygian)", m, { { 0, m, T, 8 }, { 1, M, T, 8 } } },
            { "i - v - VI - iv", m, { { 0, m, A9, 4 }, { 7, m, A9, 4 }, { 8, M, A9, 4 }, { 5, m, A9, 4 } } } } },
        { "Jungle", "Liquid and atmospheric: minor ninths, long pads",
          { { "i9 - VImaj7", m, { { 0, m, N9, 8 }, { 8, M, S7, 8 } } },
            { "i - VII - VI - VII", m, { { 0, m, A9, 4 }, { 10, M, A9, 4 }, { 8, M, A9, 4 }, { 10, M, A9, 4 } } },
            { "iv9 - i9", m, { { 5, m, N9, 8 }, { 0, m, N9, 8 } } },
            { "VImaj9 - VII - i9 (liquid)", m, { { 8, M, N9, 8 }, { 10, M, A9, 8 }, { 0, m, N9, 16 } } },
            { "iv9 - VII9 - IIImaj9 - VImaj9", m, { { 5, m, N9, 4 }, { 10, M, N9, 4 }, { 3, M, N9, 4 }, { 8, M, N9, 4 } } } } },
        { "House", "Vamps and stabs: minor sevenths, deep ninths",
          { { "i7 - iv7 (vamp)", m, { { 0, m, S7, 8 }, { 5, m, S7, 8 } } },
            { "i9 - VImaj9 - VII (deep)", m, { { 0, m, N9, 8 }, { 8, M, N9, 4 }, { 10, M, A9, 4 } } },
            { "IVmaj7 - Imaj7 (classic)", M, { { 5, M, S7, 8 }, { 0, M, S7, 8 } } },
            { "ii9 - V - iii7 - vi9 (2-step)", M, { { 2, m, N9, 4 }, { 7, M, X, 4 }, { 4, m, S7, 4 }, { 9, m, N9, 4 } } } } },
        { "Melancholic", "Minor and bittersweet, falling back home",
          { { "i - iv - VII - III", m, { { 0, m, A9, 4 }, { 5, m, A9, 4 }, { 10, M, A9, 4 }, { 3, M, A9, 4 } } },
            { "VI - VII - i", m, { { 8, M, S7, 4 }, { 10, M, S7, 4 }, { 0, m, A9, 8 } } },
            { "vi - IV - I - V", M, { { 9, m, A9, 4 }, { 5, M, A9, 4 }, { 0, M, A9, 4 }, { 7, M, A9, 4 } } },
            { "i - VI - III - VII (slow)", m, { { 0, m, N9, 8 }, { 8, M, N9, 8 }, { 3, M, N9, 8 }, { 10, M, N9, 8 } } },
            { "i - v", m, { { 0, m, A9, 8 }, { 7, m, A9, 8 } } } } },
        { "Ethereal", "Ambient, after Harold Budd and Eno: slow major sevenths and ninths that float",
          { { "Imaj9 - II (Lydian float)", M, { { 0, M, N9, 16 }, { 2, M, A9, 16 } } },
            { "Imaj7 - bVIImaj7 (planing)", M, { { 0, M, S7, 16 }, { 10, M, S7, 16 } } },
            { "Imaj7 - vi9 - IVmaj9 - iii7", M, { { 0, M, S7, 8 }, { 9, m, N9, 8 }, { 5, M, N9, 8 }, { 4, m, S7, 8 } } },
            { "vi11 - IVmaj9", M, { { 9, m, X, 16 }, { 5, M, N9, 16 } } },
            { "IVmaj9 - I6/9", M, { { 5, M, N9, 16 }, { 0, M, X, 16 } } },
            { "i9 - VImaj9 (fog)", m, { { 0, m, N9, 16 }, { 8, M, N9, 16 } } } } },
    };
}

int degreeOf(const ChordParams& p, int column) { return floorMod(Chords::rootPitchClass(p, column) - p.key, 12); }
} // namespace

const std::vector<ChordStyle>& chordStyles()
{
    static const std::vector<ChordStyle> styles = build();
    return styles;
}

int styleChordCell(const ChordParams& p, const StyleChord& chord)
{
    const int home = Chords::homeCell(p).column;
    int best = -100;
    for (int column = -kHalf; column <= kHalf; ++column)
        if (degreeOf(p, column) == floorMod(chord.degree, 12) && Chords::isMinorChord(p, column) == chord.minor
            && (best == -100 || std::abs(column - home) < std::abs(best - home)))
            best = column;
    if (best == -100)
        return -1;
    return cellCode(best, std::clamp(chord.row, 0, kHarmonyRows - 1));
}

std::vector<std::pair<int, int>> styleProgressionCells(const ChordParams& p, const StyleProgression& progression)
{
    std::vector<std::pair<int, int>> cells;
    for (const auto& chord : progression.chords)
        if (const int cell = styleChordCell(p, chord); cell >= 0 && static_cast<int>(cells.size()) < kMaxProgression)
            cells.emplace_back(cell, std::clamp(chord.beats, 1, kMaxSlotBeats));
    return cells;
}

std::vector<float> styleSuggestions(const ChordParams& p, int styleIndex, int fromCell)
{
    std::vector<float> weight(static_cast<size_t>(kHarmonyColumns * 8), 0.0f);
    const auto& styles = chordStyles();
    if (styleIndex < 0 || styleIndex >= static_cast<int>(styles.size()) || fromCell < 0)
        return weight;
    const HarmonyCell from = Chords::cellFromCode(fromCell);
    const int fromDegree = degreeOf(p, from.column);
    const bool fromMinor = Chords::isMinorChord(p, from.column);
    for (const auto& progression : styles[static_cast<size_t>(styleIndex)].progressions)
    {
        if (progression.minorKey != p.minor)
            continue;
        const auto& chords = progression.chords;
        for (size_t i = 0; i < chords.size(); ++i)
        {
            if (chords[i].degree != fromDegree || chords[i].minor != fromMinor)
                continue;
            const StyleChord& next = chords[(i + 1) % chords.size()];
            for (int column = -kHalf; column <= kHalf; ++column)
                if (degreeOf(p, column) == next.degree && Chords::isMinorChord(p, column) == next.minor)
                    for (int row = 0; row < kHarmonyRows; ++row)
                        weight[static_cast<size_t>(cellCode(column, row))] += row == next.row ? 1.0f : 0.35f;
        }
    }
    const float top = *std::max_element(weight.begin(), weight.end());
    if (top > 0.0f)
        for (auto& w : weight)
            w /= top;
    return weight;
}

} // namespace gristo
