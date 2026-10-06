#pragma once

#include <array>
#include <cstdint>

namespace gristo {

// The six voices, in Stolperbeats order. Linear mode priorities:
//   Snare > Kick > HiHat2 > HiHat1   and   Snare > Kick > Perc2 > Perc1.
enum Voice : int
{
    Kick = 0,
    Snare,
    HiHat1, // closed
    HiHat2, // open
    Perc1,
    Perc2,
};

constexpr int kNumVoices = 6;
constexpr int kStepsPerBeat = 4;   // sixteenths
constexpr int kStepsPerBar = 16;   // 4/4
constexpr int kPatternSteps = 32;  // map patterns are two bars long

const char* voiceName(int voice);

// Floor division / modulo that behave for negative step indices (pre-roll).
inline std::int64_t floorDiv(std::int64_t a, std::int64_t b)
{
    std::int64_t q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

inline int floorMod(std::int64_t a, std::int64_t b)
{
    return static_cast<int>(a - floorDiv(a, b) * b);
}

// Metric weight of a sixteenth inside a 4/4 bar: 1 on the downbeat, down to 0.15 on
// the "e" and "a" positions. Shared by the humanizer and the map builder's model.
inline float metricWeight(int stepInBar)
{
    const int s = stepInBar % kStepsPerBar;
    if (s == 0) return 1.0f;
    if (s % 8 == 0) return 0.8f;
    if (s % 4 == 0) return 0.6f;
    if (s % 2 == 0) return 0.35f;
    return 0.15f;
}

} // namespace gristo
