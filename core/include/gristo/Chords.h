#pragma once

#include "gristo/Types.h"

#include <array>
#include <cstdint>
#include <string>

namespace gristo {

// Harmony map: chords laid out so that any move sounds related.
//
// Columns are a chain of thirds through the key: every chord shares two notes with its
// neighbours (... Dm F Am [C] Em G Bm ... in C major). The tonic sits in the middle,
// the flat side (darker, borrowed chords) to the left, the sharp side (brighter) to the
// right. Rows add colour: triad, add9, 7th, 9th and 11th-type chords from bottom to top.
//
// The pad sets a home cell. Wander lets the harmony walk to neighbouring cells by
// itself, one chord at a time, and returns home at the start of every phrase. Every
// phrase walks a different way, until one is frozen ("caught") and loops.

constexpr int kHarmonyColumns = 11;
constexpr int kHarmonyRows = 5;
constexpr int kMaxChordNotes = 6;
constexpr int kNumChordKeys = 14; // Keys mode: two octaves of white keys

enum class ChordPlay : int { Off = 0, Held, Stabs };
// Wander: the harmony moves by itself around home. Keys: you play it, one key per chord.
enum class HarmonyMode : int { Wander = 0, Keys };
// Close: root position. Smooth folds every chord into one octave so the notes move as
// little as possible from chord to chord; SmoothBass adds the root underneath.
enum class ChordVoicing : int { Close = 0, Smooth, SmoothBass };
enum class StrumDirection : int { Up = 0, Down, Alternate };

const char* pitchClassName(int pitchClass); // "C", "C#", ...
const char* harmonyRowName(int row);        // "Triad", "Add9", ...

struct HarmonyCell
{
    int column = 0; // -5..5 along the chain of thirds, 0 = the tonic chord
    int row = 0;    // 0..4
    bool operator==(const HarmonyCell& o) const { return column == o.column && row == o.row; }
    bool operator!=(const HarmonyCell& o) const { return !(*this == o); }
};

struct ChordParams
{
    bool enabled = false;
    int key = 9;       // pitch class of the tonic (9 = A)
    bool minor = true;
    float homeX = 0.5f; // pad position: 0..1 across the columns
    float homeY = 0.0f; //               0..1 up the rows
    int lowestNote = 48; // the voicing starts at or above this

    HarmonyMode mode = HarmonyMode::Wander;
    float wander = 0.3f; // 0 = stays home ... 1 = moves on every chord, sometimes far
    int chordSteps = 16; // length of one chord: 8, 16 or 32 sixteenths
    int phraseBars = 4;  // the walk starts again from home every N bars
    bool frozen = false; // every phrase repeats the walk of phrase `frozenPhrase`
    int frozenPhrase = 0;
    std::uint32_t seed = 1;

    ChordPlay play = ChordPlay::Held;
    std::array<bool, kNumVoices> stabTriggers { false, true, false, false, false, false };
    float stabGateSteps = 2.0f;
    float velMin = 50.0f / 127.0f, velMax = 100.0f / 127.0f;
    int shift = 0; // semitones added to the MIDI out (see ArpParams::shift)

    ChordVoicing voicing = ChordVoicing::SmoothBass;
    float strumMs = 0.0f; // time between the notes of a chord (0 = together)
    StrumDirection strumDirection = StrumDirection::Up;
};

struct Chord
{
    int count = 0;
    std::array<int, kMaxChordNotes> notes {}; // MIDI notes, low to high
};

// Where the harmony is at a step.
struct ChordPosition
{
    bool valid = false;
    HarmonyCell cell;
    std::int64_t serial = 0; // changes whenever a new chord starts (even the same one again)
};

class Chords
{
public:
    static HarmonyCell homeCell(const ChordParams& p);
    // Pad coordinates of a cell's centre (the inverse of homeCell).
    static float columnToX(int column);
    static float rowToY(int row);

    static int phraseSteps(const ChordParams& p);
    // Index of the phrase a step is in (what Catch stores to freeze it).
    static std::int64_t phraseAt(const ChordParams& p, std::int64_t step);
    // Wander mode: where the walk is at a step. A pure function of the step.
    static ChordPosition positionAt(const ChordParams& p, std::int64_t step);
    // Keys mode: the cell a chord key plays (0..kNumChordKeys-1): seven columns
    // around home, the second octave one row richer.
    static HarmonyCell keyCell(const ChordParams& p, int key);

    static int rootPitchClass(const ChordParams& p, int column);
    static bool isMinorChord(const ChordParams& p, int column);
    static Chord build(const ChordParams& p, HarmonyCell cell);
    static std::string name(const ChordParams& p, HarmonyCell cell); // "Am7", "G9sus"
};

} // namespace gristo
