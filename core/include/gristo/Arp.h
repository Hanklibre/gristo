#pragma once

#include "gristo/Types.h"

#include <cstddef>

namespace gristo {

// Drum-clocked arpeggiators / sequencers. Each arp is stepped by hits of one or more
// drum voices (the drums keep playing), so its notes inherit the groove, humanize and
// probability of those voices. The step counter restarts every N bars, counted from
// song position, so jumps, loops and bounces always line up.

constexpr int kNumArps = 2;
constexpr int kMaxArpSteps = 16;    // generator pattern length
constexpr int kMaxArpPool = 64;     // notes available to pick from (incl. octaves)
constexpr int kMaxHeldNotes = 32;

enum class ArpMode : int
{
    Arp = 0,  // play the held chord in an order
    Generate, // Sting-style generated pattern over held notes or a scale
};

enum class ArpOrder : int
{
    Up = 0,
    Down,
    UpDown,
    Random,
    AsPlayed,
    Count
};

enum class ArpScale : int
{
    Major = 0,
    Minor,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    HarmonicMinor,
    MinorPentatonic,
    MajorPentatonic,
    Blues,
    Chromatic,
    Count
};

const char* arpOrderName(ArpOrder order);
const char* arpScaleName(ArpScale scale);

struct ArpParams
{
    bool enabled = false;
    std::array<bool, kNumVoices> triggers {}; // drum voices that step the arp
    ArpMode mode = ArpMode::Arp;
    int resetBars = 4;                        // 1, 2, 4, 8 or 16

    // Arp mode
    ArpOrder order = ArpOrder::Up;
    int octaves = 1; // 1..4, also the range of the generator

    // Generate mode
    int root = 48;   // MIDI note: key root and lowest note when nothing is held
    ArpScale scale = ArpScale::Minor;
    int length = 8;  // 1..16 steps
    float density = 0.8f; // chance a step plays (others are rests)
    float accent = 0.25f; // chance a step is accented
    float slide = 0.15f;  // chance a step slides (legato) into the next
    float mutate = 0.0f;  // share of steps re-rolled at every reset (cumulative)
    std::uint32_t seed = 1;

    // Output
    float gateSteps = 2.0f; // note length in 16ths (slides overlap the next note)
    int shift = 0;          // semitones added to the MIDI out, to give the arp its own
                            // note range (undo it with a Pitch device in the DAW)
    float velMin = 40.0f / 127.0f;
    float velMax = 1.0f;

    // Set by modulation only (no knob)
    int transpose = 0;           // semitones added to every note before Shift
    float velocityOffset = 0.0f; // normalized, added to the drum hit's velocity
    float cutoffOffset = 0.0f;   // built-in synth cutoff (0..1 knob units)
};

// Raw random values of one generated step. Density / accent / slide amounts are
// thresholds applied when playing, so turning those knobs changes the pattern
// smoothly and reversibly.
struct ArpStepValues
{
    float pitch = 0.0f; // 0..1 position in the note pool
    float rest = 0.0f;
    float accent = 0.0f;
    float slide = 0.0f;
};

struct ArpPattern
{
    std::array<ArpStepValues, kMaxArpSteps> steps {};
};

// Notes to choose from, ascending (or as played).
struct ArpPool
{
    std::array<int, kMaxArpPool> notes {};
    int size = 0;
};

struct ArpNote
{
    bool play = false;
    int note = 60;
    bool accent = false;
    bool slide = false;
};

class Arp
{
public:
    // Generated pattern for a reset cycle: the base pattern for `seed`, with `mutate`
    // applied once per cycle (so cycle c has evolved c times).
    static ArpPattern pattern(std::uint32_t seed, float mutate, std::int64_t cycle);

    // Builds the pool: held notes (press order for AsPlayed, else ascending) spread
    // over `octaves`; in Generate mode with nothing held, the scale from `root`.
    static ArpPool pool(const ArpParams& p, const int* held, int heldCount);

    // Note for advance number k (0-based since the last reset) in a cycle.
    static ArpNote noteFor(const ArpParams& p, const ArpPool& pool, const ArpPattern& pattern, std::int64_t k,
                           std::int64_t cycle);

    // MIDI out note for a played note: note + shift, folded back by octaves into 0..127.
    static int outputNote(int note, int shift);
};

} // namespace gristo
