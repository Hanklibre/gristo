#include "gristo/Groove.h"

#include "gristo/Random.h"

#include <algorithm>
#include <cmath>

namespace gristo {

namespace {

// Trip: each beat's four sixteenths snap to slots of an n-tuplet grid.
// Rows are ordered by how far they pull away from straight time.
struct TupletRow
{
    int divisions;
    int slots[4];
};

constexpr TupletRow kTripRows[kNumShuffleRows] = {
    { 4, { 0, 1, 2, 3 } }, // straight
    { 7, { 0, 2, 4, 5 } }, // septuplet: lazy middle, last step tucked in
    { 9, { 0, 2, 5, 7 } }, // gap in the middle
    { 5, { 0, 1, 3, 4 } }, // quintuplet: wider gap in the middle
    { 6, { 0, 2, 3, 5 } }, // sextuplet: triplet swing
    { 8, { 0, 1, 5, 6 } }, // pairs: rushed start, late end
    { 9, { 0, 1, 6, 7 } }, // pairs, more extreme
};

// Shake: position of the swung sixteenth inside each eighth (0.5 = straight).
constexpr double kShakeSwing[kNumShuffleRows] = { 0.50, 0.54, 0.56, 0.58, 0.60, 0.63, 0.66 };
// ...and how far the second eighth of the beat lags (in steps).
constexpr double kShakeLag[kNumShuffleRows] = { 0.0, 0.0, 0.0, 0.02, 0.04, 0.06, 0.08 };

// Push: positions inside a beat are warped by p^gamma, pulling notes early.
constexpr double kPushGamma[kNumShuffleRows] = { 1.0, 1.06, 1.12, 1.18, 1.25, 1.33, 1.42 };

// Clave: steps between the 3:3:2 anchors are warped towards the next anchor.
constexpr double kClaveGamma[kNumShuffleRows] = { 1.0, 1.12, 1.25, 1.4, 1.55, 1.72, 1.9 };
constexpr int kClaveAnchors[4] = { 0, 3, 6, 8 };

constexpr std::uint32_t kShuffleRandomStream = 0x5A0FFu;
constexpr std::uint32_t kFeelRandomStream = 0xF1A70u;
constexpr std::uint32_t kRollRandomStream = 0x2011Fu;

} // namespace

double Groove::cellPosition(Timing timing, int row, int k)
{
    row = std::clamp(row, 0, kNumShuffleRows - 1);
    switch (timing)
    {
        case Timing::Trip:
        {
            const TupletRow& r = kTripRows[row];
            return 4.0 * r.slots[k] / r.divisions;
        }
        case Timing::Shake:
        {
            const double swing = kShakeSwing[row];
            const double lag = kShakeLag[row];
            switch (k)
            {
                case 0: return 0.0;
                case 1: return 2.0 * swing;
                case 2: return 2.0 + lag;
                default: return 2.0 + lag + (2.0 - lag) * swing;
            }
        }
        case Timing::Push:
            return 4.0 * std::pow(k / 4.0, kPushGamma[row]);
        case Timing::Clave:
        {
            int seg = 0;
            while (k >= kClaveAnchors[seg + 1])
                ++seg;
            const double a = kClaveAnchors[seg];
            const double len = kClaveAnchors[seg + 1] - a;
            const double frac = (k - a) / len;
            return a + std::pow(frac, 1.0 / kClaveGamma[row]) * len;
        }
    }
    return k;
}

double Groove::gridPosition(const GrooveParams& p, std::int64_t absoluteStep)
{
    const int cell = cellSteps(p.timing);
    const std::int64_t cellIndex = floorDiv(absoluteStep, cell);
    const int k = floorMod(absoluteStep, cell);

    // Shuffle is sampled once per cell, so every cell still starts on its straight
    // position and positions stay monotonic however much the shuffle jumps.
    const float shuffle = shuffleAt(p, absoluteStep);
    const int r0 = static_cast<int>(std::floor(shuffle));
    const int r1 = std::min(r0 + 1, kNumShuffleRows - 1);
    const double f = shuffle - static_cast<float>(r0);

    const double pos = cellPosition(p.timing, r0, k) * (1.0 - f) + cellPosition(p.timing, r1, k) * f;
    return (static_cast<double>(cellIndex) * cell + pos) / kStepsPerBeat;
}

float Groove::shuffleAt(const GrooveParams& p, std::int64_t absoluteStep)
{
    float shuffle = p.shuffle;
    if (p.shuffleRandom > 0.0f)
    {
        const std::int64_t cellIndex = floorDiv(absoluteStep, cellSteps(p.timing));
        shuffle += p.shuffleRandom * kShuffleRandomRows * rnd::bipolar(rnd::hash(p.seed, kShuffleRandomStream, cellIndex));
    }
    return std::clamp(shuffle, 0.0f, static_cast<float>(kNumShuffleRows - 1));
}

double Groove::feelOffset(const GrooveParams& p, int voice, std::int64_t absoluteStep, double bpm)
{
    // Random flam: each hit gets its own delay. Kick (feet) and snare (hands) roll
    // independently; in Slop the hats follow the snare's hand.
    auto delayMs = [&](int roller) {
        double ms = p.feelMs;
        if (p.feelRandom > 0.0f)
        {
            const float u = rnd::bipolar(rnd::hash(p.seed, kFeelRandomStream + static_cast<std::uint32_t>(roller), absoluteStep));
            ms *= 1.0 + std::clamp(p.feelRandom, 0.0f, 1.0f) * u;
        }
        return ms * bpm / 60000.0;
    };
    switch (p.feeling)
    {
        case Feeling::Flam:
            return (voice == Kick || voice == Snare) ? delayMs(voice) : 0.0;
        case Feeling::Slop:
            if (voice == Kick || voice == Snare) return delayMs(voice);
            if (voice == HiHat1 || voice == HiHat2) return 0.75 * delayMs(Snare);
            return 0.0;
        case Feeling::Tight:
        case Feeling::Rolls:
            break;
    }
    return 0.0;
}

int Groove::rollCount(const GrooveParams& p, int voice, std::int64_t absoluteStep)
{
    if (p.feeling != Feeling::Rolls || voice != HiHat2)
        return 1;
    if (p.feelRandom <= 0.0f)
        return 3;
    const float u = rnd::bipolar(rnd::hash(p.seed, kRollRandomStream, absoluteStep));
    return std::clamp(3 + static_cast<int>(std::lround(std::clamp(p.feelRandom, 0.0f, 1.0f) * 2.49f * u)), 2, 5);
}

} // namespace gristo
