#pragma once

#include "gristo/Types.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace gristo {

// Scaler-inspired chord sequencer: a key + scale, up to 8 chord slots built from the
// scale (or borrowed from its parallel major/minor), played as a progression that
// either loops in bars or steps on drum hits. The current chord can feed the arps
// and/or play as its own part.

constexpr int kMaxChordSlots = 8;
constexpr int kMaxChordNotes = 5;

enum class ChordScale : int { Major = 0, Minor, Dorian, Phrygian, Lydian, Mixolydian, HarmonicMinor, Count };
enum class ChordType : int { Triad = 0, Seventh, Ninth, Sus2, Sus4, Count };
enum class ChordClock : int { Bars = 0, Drums };
enum class ChordPlay : int { Off = 0, Held, Stabs };

const char* chordScaleName(ChordScale scale);
const char* chordTypeName(ChordType type);
const char* pitchClassName(int pitchClass); // "C", "C#", ...

struct ChordSlot
{
    bool used = false;
    int degree = 0;          // 0..6 = I..VII of the key's scale
    bool borrowed = false;   // from the parallel major / minor
    ChordType type = ChordType::Triad;
    int inversion = 0;       // 0..2
    int lengthSteps = 16;    // Bars clock: 8 (half bar), 16 or 32
};

struct ChordParams
{
    bool enabled = false;
    int key = 9; // pitch class of the tonic (9 = A)
    ChordScale scale = ChordScale::Minor;
    std::array<ChordSlot, kMaxChordSlots> slots {};
    int lowestNote = 48; // the voicing starts at the first tonic at or above this

    ChordClock clock = ChordClock::Bars;
    int clockVoice = 0;   // Drums clock: the voice whose hits are counted
    int hitsPerChord = 4; // Drums clock: hits per chord change
    int resetBars = 4;    // Drums clock: the count restarts every N bars

    float mutate = 0.0f;  // chance per slot per loop to swap in a related chord
    std::uint32_t seed = 1;

    ChordPlay play = ChordPlay::Held;
    std::array<bool, kNumVoices> stabTriggers { false, true, false, false, false, false };
    float stabGateSteps = 2.0f;
    float velMin = 50.0f / 127.0f, velMax = 100.0f / 127.0f;
    int shift = 0; // semitones added to the MIDI out (see ArpParams::shift)
};

struct Chord
{
    int count = 0;
    std::array<int, kMaxChordNotes> notes {}; // MIDI notes, low to high
};

// Where the progression is at a step.
struct ChordPosition
{
    int slot = -1;              // index into params.slots, -1 = no chord
    std::int64_t serial = 0;    // changes whenever a new chord starts (even the same one again)
    std::int64_t loop = 0;      // how many times the progression has looped (for Mutate)
};

class Chords
{
public:
    static ChordScale parallel(ChordScale scale); // the scale borrowed chords come from
    static Chord build(const ChordParams& p, const ChordSlot& slot);
    static std::string name(const ChordParams& p, const ChordSlot& slot);  // "Am7", "F/A"
    static std::string roman(const ChordParams& p, const ChordSlot& slot); // "vi7", "bVI"
    static int usedCount(const ChordParams& p);

    // Bars clock: a pure function of the step.
    static ChordPosition barsPosition(const ChordParams& p, std::int64_t step);
    // Drums clock: from the number of clock hits since the reset bar, up to and
    // including `step`. The chord changes on the hit that starts a new group.
    static ChordPosition drumsPosition(const ChordParams& p, std::int64_t step, std::int64_t hitsThrough);

    // The slot actually played at a position: Mutate may swap in a related chord.
    static ChordSlot played(const ChordParams& p, const ChordPosition& pos);

    // A plausible progression for the scale, written over all slots.
    static std::array<ChordSlot, kMaxChordSlots> generate(std::uint32_t seed, ChordScale scale);

    struct LibraryEntry
    {
        const char* name;
        const char* mood;
        ChordScale scale;
        std::vector<ChordSlot> slots;
    };
    static const std::vector<LibraryEntry>& library();
};

} // namespace gristo
