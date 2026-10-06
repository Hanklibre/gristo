#include "TestFramework.h"

#include "gristo/Engine.h"

#include <cmath>

using namespace gristo;

namespace {

HumanizeInput inputFor(const GrooveParams& g, int voice, std::int64_t step, float level, bool accent)
{
    HumanizeInput in;
    in.voice = voice;
    in.step = step;
    in.level = level;
    in.accent = accent;
    in.gridBeats = Groove::gridPosition(g, step);
    in.nominalBeats = step / 4.0;
    in.prevGapBeats = in.gridBeats - Groove::gridPosition(g, step - 1);
    in.nextGapBeats = Groove::gridPosition(g, step + 1) - in.gridBeats;
    in.bpm = 95.0;
    return in;
}

} // namespace

TEST("humanize: amount 0 changes nothing")
{
    HumanizeParams h;
    GrooveParams g;
    for (std::int64_t s = 0; s < 64; ++s)
    {
        const HumanizeResult r = Humanizer::apply(h, inputFor(g, s % kNumVoices, s, 0.5f, false));
        CHECK(r.offsetBeats == 0.0);
        CHECK(r.velocityScale == 1.0f);
    }
}

TEST("humanize: deviations stay within the groove's neighbour gaps")
{
    HumanizeParams h;
    h.amount = 1.0f;
    for (Timing t : { Timing::Trip, Timing::Shake, Timing::Push, Timing::Clave })
        for (float shuffle : { 0.0f, 3.0f, 6.0f })
        {
            GrooveParams g;
            g.timing = t;
            g.shuffle = shuffle;
            for (std::int64_t s = 0; s < 256; ++s)
                for (int v = 0; v < kNumVoices; ++v)
                {
                    const HumanizeInput in = inputFor(g, v, s, 0.4f, false);
                    const HumanizeResult r = Humanizer::apply(h, in);
                    const double cap = Humanizer::kMaxGapFraction * std::min(in.prevGapBeats, in.nextGapBeats);
                    CHECK(std::abs(r.offsetBeats) <= cap + 1e-12);
                    CHECK(r.velocityScale >= 0.5f && r.velocityScale <= 1.3f);
                }
        }
}

TEST("humanize: deviations are audible but tasteful (a few ms)")
{
    HumanizeParams h;
    h.amount = 1.0f;
    GrooveParams g;
    double sumAbsMs = 0.0, maxAbsMs = 0.0;
    int n = 0;
    for (std::int64_t s = 0; s < 1024; ++s)
    {
        const HumanizeInput in = inputFor(g, HiHat1, s, 0.6f, false);
        const double ms = std::abs(Humanizer::apply(h, in).offsetBeats) * 60000.0 / in.bpm;
        sumAbsMs += ms;
        maxAbsMs = std::max(maxAbsMs, ms);
        ++n;
    }
    const double meanMs = sumAbsMs / n;
    CHECK_MSG(meanMs > 1.0 && meanMs < 8.0, "mean " << meanMs << " ms");
    CHECK_MSG(maxAbsMs < 20.0, "max " << maxAbsMs << " ms");
}

TEST("humanize: anchors are tighter than ghost notes")
{
    HumanizeParams h;
    h.amount = 1.0f;
    GrooveParams g;
    double anchor = 0.0, ghost = 0.0;
    double anchorVel = 0.0, ghostVel = 0.0;
    for (std::int64_t bar = 0; bar < 1000; ++bar)
    {
        // An accented hat on the downbeat vs. a ghost hat on the "e" of the same beat.
        const HumanizeResult a = Humanizer::apply(h, inputFor(g, HiHat1, bar * kStepsPerBar, 0.95f, true));
        const HumanizeResult b = Humanizer::apply(h, inputFor(g, HiHat1, bar * kStepsPerBar + 1, 0.25f, false));
        anchor += a.offsetBeats * a.offsetBeats;
        ghost += b.offsetBeats * b.offsetBeats;
        anchorVel += (a.velocityScale - 1.0) * (a.velocityScale - 1.0);
        ghostVel += (b.velocityScale - 1.0) * (b.velocityScale - 1.0);
    }
    CHECK_MSG(ghost > anchor * 1.5, "ghost rms^2 " << ghost << " vs anchor " << anchor);
    CHECK(ghostVel > anchorVel);
}

TEST("humanize: weak notes lean in the direction of the groove")
{
    HumanizeParams h;
    h.amount = 1.0f;
    GrooveParams g;
    g.timing = Timing::Shake;
    g.shuffle = 6.0f; // off-beat sixteenths are late
    double sum = 0.0;
    int n = 0;
    for (std::int64_t beat = 0; beat < 2000; ++beat)
    {
        const std::int64_t s = beat * 4 + 1;
        sum += Humanizer::apply(h, inputFor(g, HiHat1, s, 0.3f, false)).offsetBeats;
        ++n;
    }
    CHECK_MSG(sum / n > 0.0, "mean offset " << sum / n);
}

TEST("humanize: lock repeats every pattern cycle, unlocked does not")
{
    HumanizeParams h;
    h.amount = 1.0f;
    GrooveParams g;
    int unlockedDiffs = 0;
    for (std::int64_t s = 0; s < kPatternSteps * 4; ++s)
    {
        h.lock = true;
        const HumanizeResult a = Humanizer::apply(h, inputFor(g, Snare, s, 0.5f, false));
        const HumanizeResult b = Humanizer::apply(h, inputFor(g, Snare, s + kPatternSteps, 0.5f, false));
        CHECK_NEAR(a.offsetBeats, b.offsetBeats, 1e-9);
        CHECK_NEAR(a.velocityScale, b.velocityScale, 1e-6);
        h.lock = false;
        const HumanizeResult c = Humanizer::apply(h, inputFor(g, Snare, s, 0.5f, false));
        const HumanizeResult d = Humanizer::apply(h, inputFor(g, Snare, s + kPatternSteps, 0.5f, false));
        unlockedDiffs += std::abs(c.offsetBeats - d.offsetBeats) > 1e-9 ? 1 : 0;
    }
    CHECK(unlockedDiffs > kPatternSteps);
}

TEST("humanize: neighbouring notes are correlated (drift, not white noise)")
{
    HumanizeParams h;
    h.amount = 1.0f;
    GrooveParams g;
    // Correlation between offsets of consecutive hats should be clearly positive.
    double sxy = 0.0, sxx = 0.0, syy = 0.0, sx = 0.0, sy = 0.0;
    const int n = 4000;
    for (std::int64_t s = 0; s < n; ++s)
    {
        const double x = Humanizer::apply(h, inputFor(g, HiHat1, 2 * s, 0.5f, false)).offsetBeats;
        const double y = Humanizer::apply(h, inputFor(g, HiHat1, 2 * s + 2, 0.5f, false)).offsetBeats;
        sx += x; sy += y; sxy += x * y; sxx += x * x; syy += y * y;
    }
    const double cov = sxy / n - (sx / n) * (sy / n);
    const double corr = cov / std::sqrt((sxx / n - (sx / n) * (sx / n)) * (syy / n - (sy / n) * (sy / n)));
    CHECK_MSG(corr > 0.2, "correlation " << corr);
}

TEST("humanize: per-voice amount scales the master")
{
    GrooveParams g;
    HumanizeParams master;
    master.amount = 1.0f;
    HumanizeParams half = master;
    half.amount = 0.5f;
    half.voiceAmount[Snare] = 2.0f;
    HumanizeParams off = master;
    off.voiceAmount[Kick] = 0.0f;
    for (std::int64_t s = 0; s < 128; ++s)
    {
        const HumanizeResult a = Humanizer::apply(master, inputFor(g, Snare, s, 0.4f, false));
        const HumanizeResult b = Humanizer::apply(half, inputFor(g, Snare, s, 0.4f, false));
        CHECK_NEAR(a.offsetBeats, b.offsetBeats, 1e-12);
        CHECK_NEAR(a.velocityScale, b.velocityScale, 1e-6);
        const HumanizeResult k = Humanizer::apply(off, inputFor(g, Kick, s, 0.4f, false));
        CHECK(k.offsetBeats == 0.0 && k.velocityScale == 1.0f);
    }
}
