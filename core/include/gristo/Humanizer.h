#pragma once

#include "gristo/Types.h"

namespace gristo {

struct HumanizeParams
{
    float amount = 0.0f; // 0..1
    bool lock = false;   // repeat the same deviations every pattern cycle (2 bars)
    std::uint32_t seed = 0x68756d61u;
};

// Everything the humanizer needs to know about one hit and its rhythmic context.
struct HumanizeInput
{
    int voice = 0;
    std::int64_t step = 0;     // absolute sixteenth
    float level = 0.0f;        // map level after chaos
    bool accent = false;
    double gridBeats = 0.0;    // groove-displaced position of this step
    double nominalBeats = 0.0; // straight position (step / 4)
    double prevGapBeats = 0.25; // distance to the previous step's groove position
    double nextGapBeats = 0.25; // distance to the next step's groove position
    double bpm = 120.0;
};

struct HumanizeResult
{
    double offsetBeats = 0.0;
    float velocityScale = 1.0f;
};

// Groove-aware humanization: small, tasteful, deterministic.
//
//  * Structural notes (downbeats, backbeats, accents) stay tight; ghost notes and
//    weak sixteenths get more freedom.
//  * Timing is a blend of a slow ensemble drift, a per-limb drift (hands vs. feet)
//    and a small per-hit jitter, so neighbouring notes move together like a player
//    rather than independently like noise.
//  * Weak notes lean further in the direction the groove already pushes them, which
//    deepens the shuffle instead of smearing it.
//  * The deviation is capped at a fraction of the gap to the neighbouring groove
//    slots, so tightly packed tuplets never swap order or collapse into flams.
//  * Velocity gets per-hit variation plus a "hand" accent hierarchy for hats and
//    percussion (on-beat strokes slightly stronger than off-beat ones).
class Humanizer
{
public:
    static HumanizeResult apply(const HumanizeParams& p, const HumanizeInput& in);

    // 0 = ghost / ornament, 1 = structural anchor.
    static float structuralWeight(int voice, int stepInBar, float level, bool accent);

    // Largest timing deviation, as a fraction of the smaller neighbour gap.
    static constexpr double kMaxGapFraction = 0.35;
};

} // namespace gristo
