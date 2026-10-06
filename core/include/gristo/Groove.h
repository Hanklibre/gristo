#pragma once

#include "gristo/Types.h"

namespace gristo {

// Timing models: how the sixteenth grid is displaced.
enum class Timing : int
{
    Trip = 0, // sixteenths snap onto n-tuplet grids (5, 6, 7, 8, 9 per beat)
    Shake,    // MPC-style sixteenth swing with a lazy second eighth
    Push,     // notes lean early inside each beat, leaving room before downbeats
    Clave,    // 3:3:2 anchors stay put, the steps between them swing towards the next anchor
};

// Feelings: per-voice variations on top of the timing model.
enum class Feeling : int
{
    Tight = 0, // everything shuffles together
    Rolls,     // Hihat 2 hits become short rolls that fill the gap to the next step
    Flam,      // kick and snare sit slightly behind
    Slop,      // like Flam, and the hihats follow kick and snare
};

constexpr int kNumTimings = 4;
constexpr int kNumFeelings = 4;
constexpr int kNumShuffleRows = 7; // shuffle 0 = straight ... 6 = most pronounced

struct GrooveParams
{
    Timing timing = Timing::Shake;
    float shuffle = 0.0f; // continuous 0..6; fractional values morph between rows
    Feeling feeling = Feeling::Tight;
    float feelMs = 9.0f;  // Flam / Slop delay

    // Beads-style random modulation amounts (0..1), sampled and held per beat for
    // Shuffle and per hit for the Flam / Slop delay.
    float shuffleRandom = 0.0f; // up to +-3 shuffle rows
    float feelRandom = 0.0f;    // Flam/Slop: delay varies (1 +- r) x feelMs; Rolls: 2..5 hits
    std::uint32_t seed = 0x67726f76u;
};

class Groove
{
public:
    // Displaced position of a sixteenth, in beats from song start (before Feeling and
    // humanization). Monotonic in absoluteStep for every timing and shuffle value.
    static double gridPosition(const GrooveParams& p, std::int64_t absoluteStep);

    // Shuffle value in effect for the cell containing absoluteStep (Shuffle plus its
    // random modulation), clamped to 0..6.
    static float shuffleAt(const GrooveParams& p, std::int64_t absoluteStep);

    // Per-voice Feeling offset in beats for the hit at absoluteStep.
    static double feelOffset(const GrooveParams& p, int voice, std::int64_t absoluteStep, double bpm);

    static constexpr float kShuffleRandomRows = 3.0f;

    // Number of hits a single step expands into (Rolls feeling on Hihat 2): 3, or
    // 2..5 per roll when feelRandom is up.
    static int rollCount(const GrooveParams& p, int voice, std::int64_t absoluteStep);

    // Position of a step inside its cell, in steps (cell = 4 steps, or 8 for Clave),
    // for a single shuffle row. Exposed for tests and the UI.
    static double cellPosition(Timing timing, int row, int stepInCell);
    static int cellSteps(Timing timing) { return timing == Timing::Clave ? 8 : 4; }
};

} // namespace gristo
