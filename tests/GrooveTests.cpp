#include "TestFramework.h"

#include "gristo/Groove.h"

using namespace gristo;

namespace {
const Timing kTimings[] = { Timing::Trip, Timing::Shake, Timing::Push, Timing::Clave };
}

TEST("groove: shuffle 0 is straight time for every timing")
{
    for (Timing t : kTimings)
    {
        GrooveParams g;
        g.timing = t;
        g.shuffle = 0.0f;
        for (std::int64_t s = -40; s < 40; ++s)
            CHECK_NEAR(Groove::gridPosition(g, s), s / 4.0, 1e-12);
    }
}

TEST("groove: positions are monotonic and stay near the grid for all shuffle values")
{
    for (Timing t : kTimings)
        for (float shuffle = 0.0f; shuffle <= 6.0f; shuffle += 0.125f)
        {
            GrooveParams g;
            g.timing = t;
            g.shuffle = shuffle;
            for (std::int64_t s = -33; s < 65; ++s)
            {
                const double a = Groove::gridPosition(g, s);
                const double b = Groove::gridPosition(g, s + 1);
                CHECK_MSG(b > a + 0.01, "timing " << int(t) << " shuffle " << shuffle << " step " << s);
                CHECK_MSG(std::abs(a - s / 4.0) < 0.3, "timing " << int(t) << " displacement too large");
            }
        }
}

TEST("groove: downbeats never move")
{
    for (Timing t : kTimings)
        for (float shuffle = 0.0f; shuffle <= 6.0f; shuffle += 0.5f)
        {
            GrooveParams g;
            g.timing = t;
            g.shuffle = shuffle;
            for (std::int64_t bar = -2; bar < 4; ++bar)
                CHECK_NEAR(Groove::gridPosition(g, bar * kStepsPerBar), bar * 4.0, 1e-12);
        }
}

TEST("groove: clave anchors (3:3:2) stay fixed")
{
    GrooveParams g;
    g.timing = Timing::Clave;
    g.shuffle = 6.0f;
    for (int anchor : { 0, 3, 6, 8, 11, 14 })
        CHECK_NEAR(Groove::gridPosition(g, anchor), anchor / 4.0, 1e-12);
    CHECK(Groove::gridPosition(g, 1) > 0.25);
}

TEST("groove: shake swings the off-beat sixteenths late")
{
    GrooveParams g;
    g.timing = Timing::Shake;
    g.shuffle = 4.0f;
    CHECK(Groove::gridPosition(g, 1) > 0.25);
    CHECK(Groove::gridPosition(g, 3) > 0.75);
}

TEST("groove: push pulls notes early inside the beat")
{
    GrooveParams g;
    g.timing = Timing::Push;
    g.shuffle = 6.0f;
    for (int k = 1; k < 4; ++k)
        CHECK(Groove::gridPosition(g, k) < k / 4.0);
}

TEST("groove: fractional shuffle morphs between rows")
{
    GrooveParams g;
    g.timing = Timing::Trip;
    g.shuffle = 2.0f;
    const double a = Groove::gridPosition(g, 2);
    g.shuffle = 3.0f;
    const double b = Groove::gridPosition(g, 2);
    g.shuffle = 2.5f;
    CHECK_NEAR(Groove::gridPosition(g, 2), 0.5 * (a + b), 1e-12);
}

TEST("groove: feelings delay the right voices")
{
    GrooveParams g;
    g.feelMs = 10.0;
    const double bpm = 120.0;
    const double tenMs = 10.0 * bpm / 60000.0;
    g.feeling = Feeling::Flam;
    CHECK_NEAR(Groove::feelOffset(g, Kick, 0, bpm), tenMs, 1e-12);
    CHECK_NEAR(Groove::feelOffset(g, Snare, 0, bpm), tenMs, 1e-12);
    CHECK_NEAR(Groove::feelOffset(g, HiHat1, 0, bpm), 0.0, 1e-12);
    g.feeling = Feeling::Slop;
    CHECK(Groove::feelOffset(g, HiHat1, 0, bpm) > 0.0);
    CHECK_NEAR(Groove::feelOffset(g, Perc1, 0, bpm), 0.0, 1e-12);
    g.feeling = Feeling::Rolls;
    CHECK(Groove::rollCount(g, HiHat2) == 3);
    CHECK(Groove::rollCount(g, HiHat1) == 1);
}

TEST("random: shuffle random keeps downbeats fixed and positions monotonic")
{
    for (Timing t : kTimings)
        for (float shuffle : { 0.0f, 2.0f, 6.0f })
        {
            GrooveParams g;
            g.timing = t;
            g.shuffle = shuffle;
            g.shuffleRandom = 1.0f;
            for (std::int64_t s = -32; s < 256; ++s)
            {
                CHECK(Groove::gridPosition(g, s + 1) > Groove::gridPosition(g, s) + 0.01);
                CHECK(std::abs(Groove::gridPosition(g, s) - s / 4.0) < 0.3);
                const float sh = Groove::shuffleAt(g, s);
                CHECK(sh >= 0.0f && sh <= 6.0f);
            }
            for (std::int64_t bar = 0; bar < 8; ++bar)
                CHECK_NEAR(Groove::gridPosition(g, bar * kStepsPerBar), bar * 4.0, 1e-12);
        }
}

TEST("random: shuffle random is held per beat and varies between beats")
{
    GrooveParams g;
    g.timing = Timing::Shake;
    g.shuffle = 3.0f;
    g.shuffleRandom = 0.5f;
    int changes = 0;
    for (std::int64_t beat = 0; beat < 64; ++beat)
    {
        const float a = Groove::shuffleAt(g, beat * 4);
        for (int k = 1; k < 4; ++k)
            CHECK(Groove::shuffleAt(g, beat * 4 + k) == a);
        CHECK(std::abs(a - 3.0f) <= 0.5f * Groove::kShuffleRandomRows + 1e-6f);
        changes += Groove::shuffleAt(g, beat * 4 + 4) != a ? 1 : 0;
    }
    CHECK(changes > 50);
    g.shuffleRandom = 0.0f;
    for (std::int64_t s = 0; s < 64; ++s)
        CHECK(Groove::shuffleAt(g, s) == 3.0f);
}

TEST("random: flam random varies each hit within (1 +- r) x flam ms")
{
    GrooveParams g;
    g.feeling = Feeling::Flam;
    g.feelMs = 10.0f;
    g.feelRandom = 0.6f;
    const double bpm = 120.0;
    const double base = 10.0 * bpm / 60000.0;
    double minK = 1e9, maxK = -1e9;
    int kickSnareDiffer = 0;
    for (std::int64_t s = 0; s < 512; ++s)
    {
        const double k = Groove::feelOffset(g, Kick, s, bpm);
        const double sn = Groove::feelOffset(g, Snare, s, bpm);
        CHECK(k >= base * 0.4 - 1e-12 && k <= base * 1.6 + 1e-12);
        CHECK(k == Groove::feelOffset(g, Kick, s, bpm));
        CHECK_NEAR(Groove::feelOffset(g, HiHat1, s, bpm), 0.0, 1e-12);
        minK = std::min(minK, k);
        maxK = std::max(maxK, k);
        kickSnareDiffer += std::abs(k - sn) > 1e-9 ? 1 : 0;
    }
    CHECK(maxK - minK > base * 0.8);
    CHECK(kickSnareDiffer > 400);

    g.feeling = Feeling::Slop; // hats follow the snare's random delay
    for (std::int64_t s = 0; s < 64; ++s)
        CHECK_NEAR(Groove::feelOffset(g, HiHat1, s, bpm), 0.75 * Groove::feelOffset(g, Snare, s, bpm), 1e-12);

    g.feeling = Feeling::Tight;
    for (std::int64_t s = 0; s < 64; ++s)
        CHECK(Groove::feelOffset(g, Kick, s, bpm) == 0.0);
}
