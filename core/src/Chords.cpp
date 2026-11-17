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
constexpr std::uint32_t kNoteChanceStream = 0xC4A9Eu;

// Position in the chain of thirds, counted from the key's major tonic (the relative
// major of a minor key): even = a major chord, odd = the minor chord between two majors.
int chainIndex(const ChordParams& p, int column) { return column - (p.minor ? 1 : 0); }

// Fifths from the major tonic of the major chord at (or just right of) a chain index.
int fifthsOf(int chain) { return static_cast<int>(floorDiv(chain + 1, 2)); }

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
    const std::vector<int> iv = intervals(p, cell);
    std::vector<int> notes;
    for (int i : iv)
        notes.push_back(root + i);

    // Folded into the one octave above lowestNote (shared notes stay put).
    auto fold = [&p](int n) { return p.lowestNote + floorMod(n - p.lowestNote, 12); };
    auto smooth = [&](const std::vector<int>& from) {
        std::vector<int> out;
        for (int n : from)
            out.push_back(fold(n));
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    };
    auto withBass = [&](std::vector<int> upper, std::vector<int> bass) {
        while (static_cast<int>(upper.size() + bass.size()) > kMaxChordNotes && !upper.empty())
            upper.pop_back();
        upper.insert(upper.begin(), bass.begin(), bass.end());
        return upper;
    };
    auto pitchesOf = [&](std::initializer_list<int> wanted) { // notes whose interval is in `wanted`
        std::vector<int> out;
        for (int i : iv)
            if (std::find(wanted.begin(), wanted.end(), i) != wanted.end())
                out.push_back(root + i);
        return out;
    };
    const int third = iv.size() > 1 ? iv[1] : 4;              // 3, 4 or (sus) 5
    const bool hasSeventh = std::find(iv.begin(), iv.end(), 10) != iv.end() || std::find(iv.begin(), iv.end(), 11) != iv.end();
    const int seventh = std::find(iv.begin(), iv.end(), 11) != iv.end() ? 11 : 10;

    switch (p.voicing)
    {
        case ChordVoicing::Close:
            break;
        case ChordVoicing::Smooth:
            notes = smooth(notes);
            break;
        case ChordVoicing::SmoothBass:
            notes = withBass(smooth(notes), { root - 12 });
            break;
        case ChordVoicing::Drop2:
        {
            notes = smooth(notes);
            if (notes.size() >= 3)
                notes[notes.size() - 2] -= 12;
            break;
        }
        case ChordVoicing::Open:
        {
            notes = smooth(notes);
            if (notes.size() >= 3)
                notes[notes.size() - 2] -= 12;
            if (notes.size() >= 5)
                notes[notes.size() - 4] -= 12;
            std::sort(notes.begin(), notes.end());
            if (floorMod(notes.front() - root, 12) != 0)
            {
                int bass = root - 12;
                while (bass >= notes.front())
                    bass -= 12;
                notes = withBass(notes, { bass });
            }
            break;
        }
        case ChordVoicing::Rootless:
        {
            std::vector<int> upper;
            for (int n : smooth(notes))
                if (floorMod(n - root, 12) != 0)
                    upper.push_back(n);
            notes = upper.size() >= 2 ? upper : smooth(notes);
            break;
        }
        case ChordVoicing::Shell:
        {
            // Root below, then third and seventh (the fifth for a triad), and the top colour.
            std::vector<int> shell = pitchesOf({ third, hasSeventh ? seventh : 7 });
            if (iv.back() > 11)
                shell.push_back(root + iv.back());
            notes = withBass(smooth(shell), { root - 12 });
            break;
        }
        case ChordVoicing::Upper:
        {
            // Root and fifth underneath, the colour notes above (root on top if nothing else).
            std::vector<int> colour;
            for (int i : iv)
                if (i != 0 && i != 7)
                    colour.push_back(root + i);
            if (colour.size() < 2)
                colour.push_back(root + 12);
            const std::vector<int> upper = smooth(colour);
            int bass = root - 12;
            while (bass + 7 >= upper.front())
                bass -= 12;
            notes = withBass(upper, { bass, bass + 7 });
            break;
        }
    }
    std::sort(notes.begin(), notes.end());
    notes.erase(std::unique(notes.begin(), notes.end()), notes.end());
    c.count = std::min(static_cast<int>(notes.size()), kMaxChordNotes);
    for (int i = 0; i < c.count; ++i)
        c.notes[static_cast<size_t>(i)] = std::clamp(notes[static_cast<size_t>(i)], 0, 127);
    return c;
}

Chord Chords::thin(const ChordParams& p, const Chord& chord, std::int64_t serial)
{
    const float chance = std::clamp(p.noteChance, 0.0f, 1.0f);
    if (chance >= 1.0f || chord.count == 0)
        return chord;
    Chord out;
    for (int i = 0; i < chord.count; ++i)
    {
        const int note = chord.notes[static_cast<size_t>(i)];
        if (rnd::unit(rnd::hash(p.seed, kNoteChanceStream, serial, note)) < chance)
            out.notes[static_cast<size_t>(out.count++)] = note;
    }
    if (out.count == 0)
    {
        out.count = 1;
        out.notes[0] = chord.notes[0];
    }
    return out;
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
