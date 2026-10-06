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
            if (h.accent)
                CHECK_NEAR(h.velocity, p.dynamics.velHigh[static_cast<size_t>(v)], 1e-6);
            else
                CHECK(h.velocity <= p.dynamics.velLow[static_cast<size_t>(v)] + 1e-6);
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
