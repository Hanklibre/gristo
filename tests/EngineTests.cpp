#include "TestFramework.h"

#include "gristo/Engine.h"

#include <algorithm>
#include <iterator>
#include <map>

using namespace gristo;

namespace {

struct AbsEvent
{
    std::int64_t sample;
    int voice;
    float velocity;
    bool noteOn;
};

EngineParams busyParams()
{
    EngineParams p;
    p.pattern.mapX = 0.62f;
    p.pattern.mapY = 0.41f;
    p.pattern.chaos = 0.35f;
    p.pattern.fill = { 0.85f, 0.85f, 0.9f, 0.7f, 0.8f, 0.7f };
    p.groove.timing = Timing::Trip;
    p.groove.shuffle = 4.5f;
    p.groove.feeling = Feeling::Rolls;
    p.humanize.amount = 0.8f;
    p.groove.shuffleRandom = 0.4f;
    p.pattern.mapRandom = 0.3f;
    p.pattern.probability = { 1.0f, 0.9f, 0.8f, 0.7f, 0.9f, 0.8f };
    return p;
}

// Renders `totalSamples` of continuous playback starting at ppq 0.
std::vector<AbsEvent> render(const EngineParams& params, int blockSize, std::int64_t totalSamples,
                             double bpm = 93.0, double sr = 48000.0)
{
    Engine e;
    e.setParams(params);
    std::vector<AbsEvent> all;
    std::vector<OutputEvent> out;
    for (std::int64_t pos = 0; pos < totalSamples; pos += blockSize)
    {
        BlockInfo b;
        b.playing = true;
        b.bpm = bpm;
        b.sampleRate = sr;
        b.numSamples = static_cast<int>(std::min<std::int64_t>(blockSize, totalSamples - pos));
        b.ppqStart = static_cast<double>(pos) * bpm / (60.0 * sr);
        out.clear();
        e.process(b, out);
        for (const auto& ev : out)
            all.push_back({ pos + ev.sampleOffset, ev.voice, ev.velocity, ev.noteOn });
    }
    return all;
}

std::vector<AbsEvent> noteOns(const std::vector<AbsEvent>& events)
{
    std::vector<AbsEvent> ons;
    std::copy_if(events.begin(), events.end(), std::back_inserter(ons), [](const AbsEvent& e) { return e.noteOn; });
    return ons;
}

} // namespace

TEST("engine: output is independent of block size")
{
    const EngineParams p = busyParams();
    const std::int64_t total = 48000 * 12;
    const auto ref = noteOns(render(p, 32, total));
    CHECK(ref.size() > 50);
    for (int bs : { 64, 441, 512, 4096 })
    {
        const auto got = noteOns(render(p, bs, total));
        CHECK_MSG(got.size() == ref.size(), "block " << bs << ": " << got.size() << " vs " << ref.size());
        for (size_t i = 0; i < std::min(got.size(), ref.size()); ++i)
        {
            CHECK(got[i].voice == ref[i].voice);
            CHECK(std::abs(got[i].sample - ref[i].sample) <= 1);
            CHECK_NEAR(got[i].velocity, ref[i].velocity, 1e-6);
        }
    }
}

TEST("engine: every note-on gets exactly one note-off, per voice in order")
{
    const auto events = render(busyParams(), 256, 48000 * 10);
    std::array<int, kNumVoices> open {};
    for (const auto& e : events)
    {
        if (e.noteOn)
        {
            CHECK_MSG(open[static_cast<size_t>(e.voice)] == 0, "overlapping notes on voice " << e.voice);
            ++open[static_cast<size_t>(e.voice)];
        }
        else
        {
            CHECK_MSG(open[static_cast<size_t>(e.voice)] == 1, "stray note-off on voice " << e.voice);
            open[static_cast<size_t>(e.voice)] = 0;
        }
    }
}

TEST("engine: events within a block are sorted and in range")
{
    Engine e;
    e.setParams(busyParams());
    std::vector<OutputEvent> out;
    for (int i = 0; i < 400; ++i)
    {
        BlockInfo b { true, i * 512 * 93.0 / (60.0 * 48000.0), 93.0, 48000.0, 512 };
        out.clear();
        e.process(b, out);
        for (size_t k = 0; k < out.size(); ++k)
        {
            CHECK(out[k].sampleOffset >= 0 && out[k].sampleOffset < 512);
            if (k > 0) CHECK(out[k].sampleOffset >= out[k - 1].sampleOffset);
        }
    }
}

TEST("engine: straight groove without humanize lands exactly on the grid")
{
    EngineParams p;
    p.groove.shuffle = 0.0f;
    p.pattern.fill.fill(1.0f);
    const double bpm = 120.0, sr = 48000.0;
    const auto ons = noteOns(render(p, 512, 48000 * 4, bpm, sr));
    const double samplesPerStep = sr * 60.0 / bpm / 4.0; // 6000
    CHECK(!ons.empty());
    for (const auto& e : ons)
    {
        const double phase = std::fmod(static_cast<double>(e.sample), samplesPerStep);
        CHECK(std::min(phase, samplesPerStep - phase) < 1.0);
    }
}

TEST("engine: Rolls expands Hihat 2 into bursts")
{
    EngineParams p;
    p.pattern.mapX = 0.0f; // disco node: open hats on the off-beats
    p.pattern.mapY = 0.0f;
    p.pattern.fill = { 0.0f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f };
    const auto plain = noteOns(render(p, 512, 48000 * 4));
    p.groove.feeling = Feeling::Rolls;
    const auto rolled = noteOns(render(p, 512, 48000 * 4));
    CHECK(!plain.empty());
    CHECK(rolled.size() >= plain.size() * 3 - 3);
}

TEST("engine: a transport loop replays the same notes without duplicates")
{
    EngineParams p = busyParams();
    p.humanize.lock = true;
    p.pattern.chaos = 0.0f;
    Engine e;
    e.setParams(p);
    const double bpm = 100.0, sr = 44100.0;
    const int bs = 300;
    const double loopBeats = 8.0;
    const double bps = bpm / (60.0 * sr);
    std::vector<std::vector<std::pair<int, int>>> loops(4);
    double ppq = 0.0;
    int loop = 0;
    std::vector<OutputEvent> out;
    std::int64_t sampleInLoop = 0;
    while (loop < 4)
    {
        BlockInfo b { true, ppq, bpm, sr, bs };
        out.clear();
        e.process(b, out);
        for (const auto& ev : out)
            if (ev.noteOn)
                loops[static_cast<size_t>(loop)].push_back({ static_cast<int>(sampleInLoop + ev.sampleOffset), ev.voice });
        ppq += bs * bps;
        sampleInLoop += bs;
        if (ppq >= loopBeats)
        {
            ppq = 0.0;
            sampleInLoop = 0;
            ++loop;
        }
    }
    // Loops 1..3 start with a jump back to 0 and must match each other exactly.
    CHECK(!loops[1].empty());
    CHECK(loops[1].size() == loops[2].size());
    CHECK(loops[2].size() == loops[3].size());
    for (size_t i = 0; i < std::min(loops[1].size(), loops[3].size()); ++i)
        CHECK(loops[1][i] == loops[3][i]);
}

TEST("engine: stopping releases sounding notes")
{
    EngineParams p;
    p.noteLengthBeats = 4.0f; // long notes so some are still sounding
    p.pattern.fill.fill(1.0f);
    Engine e;
    e.setParams(p);
    std::vector<OutputEvent> out;
    for (int i = 0; i < 20; ++i)
    {
        BlockInfo b { true, i * 512 * 120.0 / (60.0 * 48000.0), 120.0, 48000.0, 512 };
        e.process(b, out);
    }
    int balance = 0;
    for (const auto& ev : out) balance += ev.noteOn ? 1 : -1;
    CHECK(balance > 0);
    BlockInfo stop { false, 0.0, 120.0, 48000.0, 512 };
    e.process(stop, out);
    balance = 0;
    for (const auto& ev : out) balance += ev.noteOn ? 1 : -1;
    CHECK(balance == 0);
}

TEST("engine: rushed notes right after a jump are not lost")
{
    EngineParams p;
    p.pattern.fill.fill(1.0f);
    p.groove.timing = Timing::Push;
    p.groove.shuffle = 6.0f; // step 3 of each beat is early
    Engine e;
    e.setParams(p);
    // Jump to just after the straight position of step 3's early onset.
    const double early = Groove::gridPosition(p.groove, 7); // beat 1, step 3 (early)
    const double ppq = 7.0 / 4.0 - 0.001;                   // just before the straight step
    CHECK(early < ppq);
    std::vector<OutputEvent> out;
    e.process({ true, ppq, 120.0, 48000.0, 256 }, out);
    bool sawStep7 = false;
    for (const auto& ev : out)
        sawStep7 |= ev.noteOn && ev.sampleOffset == 0;
    CHECK(sawStep7);
}

TEST("engine: velocities follow accents and level")
{
    EngineParams p;
    p.pattern.fill.fill(1.0f);
    p.dynamics.dynamics = 1.0f;
    Engine e;
    e.setParams(p);
    for (std::int64_t s = 0; s < kPatternSteps; ++s)
        for (int v = 0; v < kNumVoices; ++v)
        {
            const HitInfo h = e.describe(s, v, 120.0, false);
            if (!h.on) continue;
            const auto i = static_cast<size_t>(v);
            const float normalTop = p.dynamics.velMin[i] + (p.dynamics.velMax[i] - p.dynamics.velMin[i]) * kNormalVelocity;
            if (h.accent)
                CHECK_NEAR(h.velocity, p.dynamics.velMax[i], 1e-6);
            else
                CHECK(h.velocity <= normalTop + 1e-6);
        }
}

TEST("engine: random flam renders identically at any block size")
{
    EngineParams p = busyParams();
    p.groove.feeling = Feeling::Slop;
    p.groove.feelMs = 14.0f;
    p.groove.feelRandom = 1.0f;
    const auto ref = noteOns(render(p, 64, 48000 * 6));
    const auto got = noteOns(render(p, 1024, 48000 * 6));
    CHECK(ref.size() == got.size());
    for (size_t i = 0; i < std::min(ref.size(), got.size()); ++i)
        CHECK(ref[i].voice == got[i].voice && std::abs(ref[i].sample - got[i].sample) <= 1);
}

TEST("engine: per-voice dynamics 0 makes non-accented hits flat")
{
    EngineParams p;
    p.pattern.fill.fill(1.0f);
    p.dynamics.dynamics = 1.0f;
    p.dynamics.voiceDynamics[HiHat1] = 0.0f;
    Engine e;
    e.setParams(p);
    bool sawShapedSnare = false;
    for (std::int64_t s = 0; s < kPatternSteps; ++s)
    {
        const HitInfo h = e.describe(s, HiHat1, 120.0, false);
        if (h.on && !h.accent)
            CHECK_NEAR(h.velocity, e.mapVelocity(HiHat1, kNormalVelocity), 1e-6);
        const HitInfo sn = e.describe(s, Snare, 120.0, false);
        if (sn.on && !sn.accent && sn.velocity < e.mapVelocity(Snare, kNormalVelocity) - 0.01f)
            sawShapedSnare = true;
    }
    CHECK(sawShapedSnare);
}

TEST("engine: random roll counts render consistently")
{
    EngineParams p;
    p.pattern.mapX = 0.0f;
    p.pattern.mapY = 0.0f;
    p.pattern.fill = { 0.0f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f };
    p.groove.feeling = Feeling::Rolls;
    p.groove.feelRandom = 1.0f;
    const auto a = noteOns(render(p, 128, 48000 * 4));
    const auto b = noteOns(render(p, 2048, 48000 * 4));
    CHECK(a.size() == b.size());
    CHECK(!a.empty());
}

// ------------------------------------------------------------------ velocity range

TEST("velocity range: every note of a voice lands inside its range, accents at max")
{
    EngineParams p = busyParams();
    p.groove.feeling = Feeling::Rolls;
    p.groove.feelRandom = 1.0f;
    p.humanize.amount = 1.0f;
    p.dynamics.velMin = { 40 / 127.0f, 60 / 127.0f, 20 / 127.0f, 30 / 127.0f, 50 / 127.0f, 10 / 127.0f };
    p.dynamics.velMax = { 100 / 127.0f, 110 / 127.0f, 70 / 127.0f, 90 / 127.0f, 80 / 127.0f, 127 / 127.0f };
    const auto ons = noteOns(render(p, 256, 48000 * 8));
    CHECK(ons.size() > 50);
    for (const auto& e : ons)
    {
        const auto i = static_cast<size_t>(e.voice);
        CHECK_MSG(e.velocity >= p.dynamics.velMin[i] - 1e-6f && e.velocity <= p.dynamics.velMax[i] + 1e-6f,
                  "voice " << e.voice << " velocity " << e.velocity * 127.0f);
    }

    p.humanize.amount = 0.0f;
    Engine eng;
    eng.setParams(p);
    for (std::int64_t s = 0; s < kPatternSteps; ++s)
        for (int v = 0; v < kNumVoices; ++v)
        {
            const HitInfo h = eng.describe(s, v, 120.0, false);
            if (h.on && h.accent)
                CHECK_NEAR(h.velocity, p.dynamics.velMax[static_cast<size_t>(v)], 1e-6);
        }
}

// ------------------------------------------------------------------ modulation

namespace {
EngineParams withLane(int target, float amount, ModShape shape, int bars = 1)
{
    EngineParams p;
    p.pattern.fill.fill(0.9f);
    ModLane& lane = p.modulation.lanes[0];
    lane.bars = bars;
    fillModShape(lane.points.data(), lane.numPoints(), shape);
    lane.slots[0] = { target, amount };
    return p;
}
} // namespace

TEST("modulation: no routing leaves parameters untouched")
{
    EngineParams p = busyParams();
    fillModShape(p.modulation.lanes[1].points.data(), 16, ModShape::Sine);
    Engine e;
    e.setParams(p);
    for (std::int64_t s = 0; s < 64; ++s)
    {
        const EngineParams ep = e.effectiveParams(s);
        CHECK(ep.pattern.fill == p.pattern.fill && ep.pattern.probability == p.pattern.probability);
        CHECK(ep.groove.shuffle == p.groove.shuffle && ep.pattern.mapX == p.pattern.mapX);
    }
}

TEST("modulation: a drawn lane removes a voice's notes in part of the bar (probability)")
{
    // Square = 1 in the second half of every bar; amount -100 % on Hihat 1 probability.
    EngineParams p = withLane(mod::kProbability + HiHat1, -1.0f, ModShape::Square);
    Engine e;
    e.setParams(p);
    Engine plain;
    EngineParams base = p;
    base.modulation = {};
    plain.setParams(base);
    int firstHalf = 0, firstHalfPlain = 0;
    for (std::int64_t bar = 0; bar < 8; ++bar)
        for (int k = 0; k < kStepsPerBar; ++k)
        {
            const std::int64_t s = bar * kStepsPerBar + k;
            const bool on = e.describe(s, HiHat1, 120.0, false).on;
            const bool onPlain = plain.describe(s, HiHat1, 120.0, false).on;
            if (k >= 8)
                CHECK(!on);
            else
            {
                CHECK(on == onPlain);
                firstHalf += on;
                firstHalfPlain += onPlain;
            }
            CHECK(e.describe(s, Kick, 120.0, false).on == plain.describe(s, Kick, 120.0, false).on);
        }
    CHECK(firstHalf > 0 && firstHalf == firstHalfPlain);
}

TEST("modulation: lanes loop over their length")
{
    EngineParams p = withLane(mod::kFill + Kick, 0.5f, ModShape::RampUp, 2);
    Engine e;
    e.setParams(p);
    for (std::int64_t s = 0; s < 32; ++s)
    {
        CHECK_NEAR(e.modulationAt(mod::kFill + Kick, s), 0.5f * s / 31.0f, 1e-6);
        CHECK(e.modulationAt(mod::kFill + Kick, s) == e.modulationAt(mod::kFill + Kick, s + 32));
    }
}

TEST("modulation: fill, map, chaos and flam targets move the effective values")
{
    EngineParams p;
    p.pattern.fill.fill(0.2f);
    p.pattern.mapX = 0.1f;
    p.groove.feelMs = 5.0f;
    ModLane& lane = p.modulation.lanes[2];
    fillModShape(lane.points.data(), 16, ModShape::Full);
    lane.slots = { ModSlot { mod::kFill + Snare, 0.5f }, ModSlot { mod::kMapX, 1.0f }, ModSlot { mod::kFlamMs, 0.5f },
                   ModSlot { mod::kChaos, -1.0f } };
    Engine e;
    e.setParams(p);
    const EngineParams ep = e.effectiveParams(3);
    CHECK_NEAR(ep.pattern.fill[Snare], 0.7f, 1e-6);
    CHECK_NEAR(ep.pattern.fill[Kick], 0.2f, 1e-6);
    CHECK_NEAR(ep.pattern.mapX, 1.0f, 1e-6);    // clamped
    CHECK_NEAR(ep.groove.feelMs, 20.0f, 1e-5);  // 5 + 0.5 x 30
    CHECK_NEAR(ep.pattern.chaos, 0.0f, 1e-6);   // clamped at 0
}

TEST("modulation: velocity target shifts velocity but stays inside the range")
{
    EngineParams up = withLane(mod::kVelocity + Snare, 1.0f, ModShape::Full);
    EngineParams down = withLane(mod::kVelocity + Snare, -1.0f, ModShape::Full);
    up.dynamics.velMin[Snare] = down.dynamics.velMin[Snare] = 30 / 127.0f;
    up.dynamics.velMax[Snare] = down.dynamics.velMax[Snare] = 90 / 127.0f;
    Engine a, b;
    a.setParams(up);
    b.setParams(down);
    int checked = 0;
    for (std::int64_t s = 0; s < kPatternSteps; ++s)
    {
        const HitInfo ha = a.describe(s, Snare, 120.0, false);
        const HitInfo hb = b.describe(s, Snare, 120.0, false);
        if (!ha.on) continue;
        CHECK_NEAR(ha.velocity, 90 / 127.0f, 1e-6);
        CHECK_NEAR(hb.velocity, 30 / 127.0f, 1e-6);
        ++checked;
    }
    CHECK(checked > 0);
}

TEST("modulation: shuffle is held per groove cell and positions stay monotonic")
{
    for (Timing t : { Timing::Trip, Timing::Shake, Timing::Push, Timing::Clave })
    {
        EngineParams p = withLane(mod::kShuffle, 1.0f, ModShape::Random, 4);
        p.groove.timing = t;
        p.groove.shuffle = 1.0f;
        Engine e;
        e.setParams(p);
        for (std::int64_t s = 0; s < 128; ++s)
        {
            const int cell = Groove::cellSteps(t);
            const std::int64_t start = floorDiv(s, cell) * cell;
            CHECK(e.effectiveParams(s).groove.shuffle == e.effectiveParams(start).groove.shuffle);
            const double a = e.describe(s, Kick, 120.0, false).gridBeats;
            const double b = e.describe(s + 1, Kick, 120.0, false).gridBeats;
            CHECK(b > a + 0.01);
        }
    }
}

TEST("modulation: output is block-size independent")
{
    EngineParams p = busyParams();
    ModLane& lane = p.modulation.lanes[0];
    lane.bars = 2;
    fillModShape(lane.points.data(), lane.numPoints(), ModShape::Triangle);
    lane.slots = { ModSlot { mod::kProbability + HiHat1, -0.8f }, ModSlot { mod::kShuffle, 0.5f },
                   ModSlot { mod::kMapY, 0.4f }, ModSlot { mod::kVelocity + Kick, -0.3f } };
    const auto ref = noteOns(render(p, 64, 48000 * 8));
    const auto got = noteOns(render(p, 777, 48000 * 8));
    CHECK(ref.size() == got.size());
    for (size_t i = 0; i < std::min(ref.size(), got.size()); ++i)
        CHECK(ref[i].voice == got[i].voice && std::abs(ref[i].sample - got[i].sample) <= 1
              && std::abs(ref[i].velocity - got[i].velocity) < 1e-6f);
}

TEST("modulation: shapes fill the lane in range")
{
    std::array<float, kMaxModPoints> pts {};
    for (int s = 0; s < static_cast<int>(ModShape::Count); ++s)
    {
        fillModShape(pts.data(), 32, static_cast<ModShape>(s));
        for (int i = 0; i < 32; ++i)
            CHECK(pts[static_cast<size_t>(i)] >= 0.0f && pts[static_cast<size_t>(i)] <= 1.0f);
    }
    fillModShape(pts.data(), 16, ModShape::RampUp);
    CHECK(pts[0] == 0.0f && pts[15] == 1.0f);
    CHECK(mod::targetName(mod::kProbability + Snare) == "Snare Probability");
}
