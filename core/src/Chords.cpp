#include "gristo/Chords.h"

#include "gristo/Random.h"

#include <algorithm>
#include <vector>

namespace gristo {

namespace {

const char* const kPitchNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
const char* const kRowNames[kHarmonyRows] = { "Triad", "Add9", "7th", "9th", "11th" };
constexpr int kHalfColumns = (kHarmonyColumns - 1) / 2;
constexpr std::uint32_t kWalkStream = 0x7A1C0u;

// Position in the chain of thirds, counted from the key's major tonic (the relative
// major of a minor key): even = a major chord, odd = the minor chord between two majors.
int chainIndex(const ChordParams& p, int column) { return column - (p.minor ? 1 : 0); }

// Fifths from the major tonic of the major chord at (or just right of) a chain index.
int fifthsOf(int chain) { return floorDiv(chain + 1, 2); }

enum class Seventh { Major, Dominant, Minor };

Seventh seventhOf(const ChordParams& p, int column)
{
    if (Chords::isMinorChord(p, column))
        return Seventh::Minor;
    // I and IV (and the borrowed flat-side majors) are lush major sevenths; V and the
    // sharp-side majors are dominants, pulling onwards.
    return fifthsOf(chainIndex(p, column)) >= 1 ? Seventh::Dominant : Seventh::Major;
}

std::vector<int> intervals(const ChordParams& p, HarmonyCell cell)
{
    const Seventh seventh = seventhOf(p, cell.column);
    const int third = seventh == Seventh::Minor ? 3 : 4;
    const int seven = seventh == Seventh::Major ? 11 : 10;
    switch (std::clamp(cell.row, 0, kHarmonyRows - 1))
    {
        case 0: return { 0, third, 7 };
        case 1: return { 0, third, 7, 14 };
        case 2: return { 0, third, 7, seven };
        case 3: return { 0, third, 7, seven, 14 };
        default:
            if (seventh == Seventh::Minor)
                return { 0, 3, 7, 10, 14, 17 }; // m11
            if (seventh == Seventh::Dominant)
                return { 0, 5, 7, 10, 14 };     // 9sus4: the eleventh instead of the third
            return { 0, 4, 7, 9, 14 };          // 6/9: open and bright, no avoid note
    }
}

} // namespace

const char* pitchClassName(int pitchClass) { return kPitchNames[floorMod(pitchClass, 12)]; }
const char* harmonyRowName(int row) { return kRowNames[std::clamp(row, 0, kHarmonyRows - 1)]; }

float Chords::columnToX(int column)
{
    return static_cast<float>(std::clamp(column, -kHalfColumns, kHalfColumns) + kHalfColumns) / static_cast<float>(kHarmonyColumns - 1);
}

float Chords::rowToY(int row)
{
    return static_cast<float>(std::clamp(row, 0, kHarmonyRows - 1)) / static_cast<float>(kHarmonyRows - 1);
}

HarmonyCell Chords::homeCell(const ChordParams& p)
{
    HarmonyCell cell;
    cell.column = static_cast<int>(std::lround(std::clamp(p.homeX, 0.0f, 1.0f) * static_cast<float>(kHarmonyColumns - 1))) - kHalfColumns;
    cell.row = static_cast<int>(std::lround(std::clamp(p.homeY, 0.0f, 1.0f) * static_cast<float>(kHarmonyRows - 1)));
    return cell;
}

int Chords::phraseSteps(const ChordParams& p) { return std::clamp(p.phraseBars, 1, 16) * kStepsPerBar; }

std::int64_t Chords::phraseAt(const ChordParams& p, std::int64_t step) { return floorDiv(step, phraseSteps(p)); }

ChordPosition Chords::positionAt(const ChordParams& p, std::int64_t step)
{
    const int length = std::clamp(p.chordSteps, 1, 64);
    const std::int64_t phrase = phraseAt(p, step);
    const int index = floorMod(step, phraseSteps(p)) / length;
    const std::int64_t walk = p.frozen ? p.frozenPhrase : phrase;
    const HarmonyCell home = homeCell(p);
    const float wander = std::clamp(p.wander, 0.0f, 1.0f);

    HarmonyCell cell = home;
    for (int i = 1; i <= index && wander > 0.0f; ++i)
    {
        auto roll = [&](int which) { return rnd::unit(rnd::hash(p.seed, kWalkStream + static_cast<std::uint32_t>(which), walk, i)); };
        if (roll(0) < wander)
        {
            // Mostly one step along the chain (two shared notes); sometimes two (a fifth
            // or a relative). The further from home, the stronger the pull back.
            const int away = cell.column - home.column;
            const float towards = away == 0 ? 0.5f : std::min(0.95f, 0.5f + 0.15f * static_cast<float>(std::abs(away)));
            int direction = roll(1) < towards ? (away > 0 ? -1 : 1) : (away > 0 ? 1 : -1);
            if (away == 0)
                direction = roll(1) < 0.5f ? -1 : 1;
            const int size = roll(2) < wander * 0.35f ? 2 : 1;
            int column = cell.column + direction * size;
            if (column < -kHalfColumns || column > kHalfColumns) // off the map: go the other way
                column = cell.column - direction * size;
            cell.column = std::clamp(column, -kHalfColumns, kHalfColumns);
        }
        if (roll(3) < wander * 0.45f)
        {
            const int above = cell.row - home.row;
            int direction = roll(4) < 0.5f ? -1 : 1;
            if (above != 0 && roll(4) < 0.7f)
                direction = above > 0 ? -1 : 1;
            cell.row = std::clamp(cell.row + direction, 0, kHarmonyRows - 1);
        }
    }

    ChordPosition pos;
    pos.valid = true;
    pos.cell = cell;
    pos.serial = phrase * 4096 + index;
    return pos;
}

HarmonyCell Chords::keyCell(const ChordParams& p, int key)
{
    const HarmonyCell home = homeCell(p);
    key = std::clamp(key, 0, kNumChordKeys - 1);
    HarmonyCell cell;
    // Seven columns that always fit on the map, centred on home where possible.
    const int first = std::clamp(home.column - 3, -kHalfColumns, kHalfColumns - 6);
    cell.column = first + key % 7;
    cell.row = std::clamp(home.row + key / 7, 0, kHarmonyRows - 1);
    return cell;
}

bool Chords::isMinorChord(const ChordParams& p, int column) { return floorMod(chainIndex(p, column), 2) != 0; }

int Chords::rootPitchClass(const ChordParams& p, int column)
{
    const int chain = chainIndex(p, column);
    const int majorTonic = p.key + (p.minor ? 3 : 0);
    const int majorRoot = majorTonic + 7 * fifthsOf(chain);
    // The minor chord left of a major one is its relative minor.
    return floorMod(floorMod(chain, 2) != 0 ? majorRoot + 9 : majorRoot, 12);
}

Chord Chords::build(const ChordParams& p, HarmonyCell cell)
{
    Chord c;
    int root = rootPitchClass(p, cell.column);
    while (root < p.lowestNote)
        root += 12;
    std::vector<int> notes;
    for (int i : intervals(p, cell))
        notes.push_back(root + i);

    if (p.voicing != ChordVoicing::Close)
    {
        // One fixed octave for every chord: shared notes stay put, the rest move by steps.
        for (int& n : notes)
            n = p.lowestNote + floorMod(n - p.lowestNote, 12);
        std::sort(notes.begin(), notes.end());
        notes.erase(std::unique(notes.begin(), notes.end()), notes.end());
        if (p.voicing == ChordVoicing::SmoothBass)
        {
            if (static_cast<int>(notes.size()) >= kMaxChordNotes)
                notes.pop_back();
            notes.insert(notes.begin(), root - 12);
        }
    }
    std::sort(notes.begin(), notes.end());
    c.count = std::min(static_cast<int>(notes.size()), kMaxChordNotes);
    for (int i = 0; i < c.count; ++i)
        c.notes[static_cast<size_t>(i)] = std::clamp(notes[static_cast<size_t>(i)], 0, 127);
    return c;
}

std::string Chords::name(const ChordParams& p, HarmonyCell cell)
{
    const Seventh seventh = seventhOf(p, cell.column);
    std::string name = pitchClassName(rootPitchClass(p, cell.column));
    const bool minor = seventh == Seventh::Minor;
    switch (std::clamp(cell.row, 0, kHarmonyRows - 1))
    {
        case 0: return name + (minor ? "m" : "");
        case 1: return name + (minor ? "m" : "") + "add9";
        case 2: return name + (minor ? "m7" : seventh == Seventh::Major ? "maj7" : "7");
        case 3: return name + (minor ? "m9" : seventh == Seventh::Major ? "maj9" : "9");
        default: return name + (minor ? "m11" : seventh == Seventh::Major ? "6/9" : "9sus");
    }
}

} // namespace gristo
