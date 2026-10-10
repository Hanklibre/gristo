#include "TestFramework.h"

#include "gristo/Engine.h"

#include <algorithm>
#include <map>
#include <vector>

using namespace gristo;

namespace {

struct Ev
{
    std::int64_t sample;
    int voice;
    int note;
    float velocity;
    bool on;
};

std::vector<Ev> renderArp(const EngineParams& p, const std::vector<int>& held, int blockSize, std::int64_t total,
                          double bpm = 120.0, double sr = 48000.0)
{
    Engine e;
    e.setParams(p);
    for (int a = 0; a < kNumArps; ++a)
        e.setArpInput(a, held.data(), static_cast<int>(held.size()));
    std::vector<Ev> all;
    std::vector<OutputEvent> out;
    for (std::int64_t pos = 0; pos < total; pos += blockSize)
    {
        BlockInfo b { true, static_cast<double>(pos) * bpm / (60.0 * sr), bpm, sr,
                      static_cast<int>(std::min<std::int64_t>(blockSize, total - pos)) };
        out.clear();
        e.process(b, out);
        for (const auto& ev : out)
            all.push_back({ pos + ev.sampleOffset, ev.voice, ev.note, ev.velocity, ev.noteOn });
    }
    return all;
}

std::vector<Ev> arpOns(const std::vector<Ev>& evs, int arp)
{
    std::vector<Ev> r;
    for (const auto& e : evs)
        if (e.on && e.voice == kArpVoiceBase + arp)
            r.push_back(e);
    return r;
}

EngineParams fourOnFloorKickArp()
{
    EngineParams p;
    p.pattern.mapX = 0.0f; // disco node: kick on every beat
    p.pattern.mapY = 0.0f;
    p.pattern.fill = { 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    p.groove.shuffle = 0.0f;
    ArpParams& a = p.arps[0];
    a.enabled = true;
    a.triggers[Kick] = true;
    a.resetBars = 1;
    return p;
}

} // namespace

TEST("arp: pool ordering (up, down, up/down, as played) and octaves")
{
    ArpParams p;
    const int held[] = { 64, 60, 67 };
    ArpPool pool = Arp::pool(p, held, 3);
    CHECK(pool.size == 3 && pool.notes[0] == 60 && pool.notes[1] == 64 && pool.notes[2] == 67);
    ArpPattern none {};
    auto seq = [&](ArpOrder order, int count) {
        p.order = order;
        ArpPool pl = Arp::pool(p, held, 3);
        std::vector<int> r;
        for (int k = 0; k < count; ++k)
            r.push_back(Arp::noteFor(p, pl, none, k, 0).note);
        return r;
    };
    CHECK((seq(ArpOrder::Up, 4) == std::vector<int> { 60, 64, 67, 60 }));
    CHECK((seq(ArpOrder::Down, 4) == std::vector<int> { 67, 64, 60, 67 }));
    CHECK((seq(ArpOrder::UpDown, 6) == std::vector<int> { 60, 64, 67, 64, 60, 64 }));
    CHECK((seq(ArpOrder::AsPlayed, 3) == std::vector<int> { 64, 60, 67 }));
    p.order = ArpOrder::Up;
    p.octaves = 2;
    pool = Arp::pool(p, held, 3);
    CHECK(pool.size == 6 && pool.notes[3] == 72 && pool.notes[5] == 79);
}

TEST("arp: generate mode uses the scale when nothing is held, held notes otherwise")
{
    ArpParams p;
    p.mode = ArpMode::Generate;
    p.root = 48;
    p.scale = ArpScale::MinorPentatonic;
    p.octaves = 1;
    ArpPool pool = Arp::pool(p, nullptr, 0);
    CHECK(pool.size == 6); // 5 notes + top root
    CHECK(pool.notes[0] == 48 && pool.notes[1] == 51 && pool.notes[5] == 60);
    const int held[] = { 62, 65 };
    pool = Arp::pool(p, held, 2);
    CHECK(pool.size == 2 && pool.notes[0] == 62);
    p.mode = ArpMode::Arp;
    CHECK(Arp::pool(p, nullptr, 0).size == 0); // arp mode with nothing held is silent
}

TEST("arp: generator is deterministic, mutate evolves it, density makes rests")
{
    const ArpPattern a = Arp::pattern(7, 0.0f, 0);
    const ArpPattern b = Arp::pattern(7, 0.0f, 5);
    const ArpPattern c = Arp::pattern(8, 0.0f, 0);
    int sameAsB = 0, sameAsC = 0;
    for (int i = 0; i < kMaxArpSteps; ++i)
    {
        sameAsB += a.steps[static_cast<size_t>(i)].pitch == b.steps[static_cast<size_t>(i)].pitch;
        sameAsC += a.steps[static_cast<size_t>(i)].pitch == c.steps[static_cast<size_t>(i)].pitch;
    }
    CHECK(sameAsB == kMaxArpSteps); // no mutate: identical every cycle
    CHECK(sameAsC < kMaxArpSteps);  // new seed: new pattern

    const ArpPattern m1 = Arp::pattern(7, 0.3f, 1);
    const ArpPattern m4 = Arp::pattern(7, 0.3f, 4);
    int changed1 = 0, changed4 = 0;
    for (int i = 0; i < kMaxArpSteps; ++i)
    {
        changed1 += a.steps[static_cast<size_t>(i)].pitch != m1.steps[static_cast<size_t>(i)].pitch;
        changed4 += a.steps[static_cast<size_t>(i)].pitch != m4.steps[static_cast<size_t>(i)].pitch;
    }
    CHECK(changed1 > 0 && changed1 < kMaxArpSteps);
    CHECK(changed4 >= changed1);

    ArpParams p;
    p.mode = ArpMode::Generate;
    p.length = 16;
    ArpPool pool = Arp::pool(p, nullptr, 0);
    auto plays = [&](float density) {
        p.density = density;
        int n = 0;
        for (int k = 0; k < 16; ++k)
            n += Arp::noteFor(p, pool, a, k, 0).play;
        return n;
    };
    CHECK(plays(1.0f) == 16);
    CHECK(plays(0.0f) == 0);
    CHECK(plays(0.5f) > 0 && plays(0.5f) < 16);
}

TEST("arp: kick steps the arp, the step resets every N bars")
{
    EngineParams p = fourOnFloorKickArp();
    const std::vector<int> held { 60, 64, 67, 71, 74 };
    const auto evs = renderArp(p, held, 256, 48000 * 8); // 4 bars at 120 bpm
    const auto ons = arpOns(evs, 0);
    std::vector<Ev> kicks;
    for (const auto& e : evs)
        if (e.on && e.voice == Kick)
            kicks.push_back(e);
    CHECK(!ons.empty());
    CHECK(ons.size() == kicks.size());
    // 4 kicks per bar, 5 held notes, reset every bar: each bar plays 60 64 67 71.
    for (size_t i = 0; i < ons.size(); ++i)
    {
        const int expected = held[i % 4];
        CHECK_MSG(ons[i].note == expected, "hit " << i << " note " << ons[i].note);
        CHECK(std::abs(ons[i].sample - kicks[i].sample) <= 1); // lands with the kick
    }
}

TEST("arp: shift moves the MIDI out into its own range, folding at the edges")
{
    CHECK(Arp::outputNote(60, 36) == 96);
    CHECK(Arp::outputNote(100, 36) == 124);
    CHECK(Arp::outputNote(110, 36) == 134 - 12);
    CHECK(Arp::outputNote(5, -12) == 5 - 12 + 12);
    EngineParams p = fourOnFloorKickArp();
    const std::vector<int> held { 60, 64, 67, 71, 74 };
    const auto plain = arpOns(renderArp(p, held, 256, 48000 * 4), 0);
    p.arps[0].shift = 36;
    const auto evs = renderArp(p, held, 256, 48000 * 4);
    const auto shifted = arpOns(evs, 0);
    CHECK(!plain.empty());
    CHECK(plain.size() == shifted.size());
    for (size_t i = 0; i < std::min(plain.size(), shifted.size()); ++i)
    {
        CHECK(shifted[i].note == plain[i].note + 36);
        CHECK(shifted[i].sample == plain[i].sample);
    }
    // Every shifted note is released with the same (shifted) note number.
    std::map<int, int> open;
    for (const auto& e : evs)
        if (e.voice == kArpVoiceBase)
            open[e.note] += e.on ? 1 : -1;
    for (const auto& [note, count] : open)
        CHECK_MSG(count == 0, "note " << note << " left " << count);
}

TEST("arp: a transport loop restarts the phrase in the same place")
{
    EngineParams p = fourOnFloorKickArp();
    p.arps[0].resetBars = 2;
    const std::vector<int> held { 60, 62, 64, 65, 67, 69, 71 };
    Engine e;
    e.setParams(p);
    e.setArpInput(0, held.data(), static_cast<int>(held.size()));
    // Play bars 2..3 (beats 4..12) twice by looping, and compare.
    const double bpm = 120.0, sr = 48000.0, bps = bpm / (60.0 * sr);
    std::vector<std::vector<int>> loops(3);
    std::vector<OutputEvent> out;
    for (int loop = 0; loop < 3; ++loop)
        for (double ppq = 4.0; ppq < 12.0 - 1e-9; ppq += 512 * bps)
        {
            out.clear();
            e.process({ true, ppq, bpm, sr, 512 }, out);
            for (const auto& ev : out)
                if (ev.noteOn && ev.voice == kArpVoiceBase)
                    loops[static_cast<size_t>(loop)].push_back(ev.note);
        }
    CHECK(!loops[0].empty());
    CHECK(loops[0] == loops[1]);
    CHECK(loops[1] == loops[2]);
    // Beat 4 is bar 2, in the middle of the 2-bar cycle that started at beat 0: four
    // kicks have already advanced the arp, so after the jump it continues at note 5.
    CHECK(loops[0][0] == held[4]);
    CHECK(loops[0][4] == held[0]); // bar 3 starts a new cycle: back to the first note
}

TEST("arp: velocity follows the drum hit inside the arp's range; output is block-size independent")
{
    EngineParams p;
    p.pattern.fill.fill(0.8f);
    p.humanize.amount = 0.5f;
    ArpParams& a = p.arps[1];
    a.enabled = true;
    a.triggers[HiHat1] = true;
    a.triggers[Snare] = true;
    a.mode = ArpMode::Generate;
    a.slide = 0.4f;
    a.accent = 0.3f;
    a.mutate = 0.2f;
    a.resetBars = 2;
    a.velMin = 30 / 127.0f;
    a.velMax = 100 / 127.0f;
    const std::vector<int> none;
    const auto r1 = renderArp(p, none, 64, 48000 * 10, 100.0);
    const auto r2 = renderArp(p, none, 999, 48000 * 10, 100.0);
    const auto o1 = arpOns(r1, 1), o2 = arpOns(r2, 1);
    CHECK(o1.size() > 10);
    CHECK(o1.size() == o2.size());
    for (size_t i = 0; i < std::min(o1.size(), o2.size()); ++i)
        CHECK(o1[i].note == o2[i].note && std::abs(o1[i].sample - o2[i].sample) <= 1);
    for (const auto& e : o1)
        CHECK(e.velocity >= a.velMin - 1e-6f && e.velocity <= a.velMax + 1e-6f);
}

TEST("arp: monophonic output, slides overlap, every note is released")
{
    EngineParams p;
    p.pattern.fill.fill(0.9f);
    ArpParams& a = p.arps[0];
    a.enabled = true;
    a.triggers[HiHat1] = true;
    a.mode = ArpMode::Generate;
    a.slide = 0.5f;
    a.density = 0.8f;
    a.gateSteps = 1.0f;
    Engine e;
    e.setParams(p);
    std::vector<OutputEvent> out;
    std::map<int, int> open;
    int maxOpen = 0, overlaps = 0;
    for (int i = 0; i < 400; ++i)
    {
        out.clear();
        e.process({ true, i * 512 * 120.0 / (60.0 * 48000.0), 120.0, 48000.0, 512 }, out);
        for (const auto& ev : out)
        {
            if (ev.voice != kArpVoiceBase) continue;
            open[ev.note] += ev.noteOn ? 1 : -1;
            CHECK(open[ev.note] >= 0 && open[ev.note] <= 1);
            int total = 0;
            for (const auto& kv : open) total += kv.second;
            overlaps += total > 1;
            maxOpen = std::max(maxOpen, total);
        }
    }
    CHECK(maxOpen <= 2);  // at most a short slide overlap
    CHECK(overlaps > 0);  // slides do overlap
    out.clear();
    e.process({ false, 0.0, 120.0, 48000.0, 512 }, out); // stop releases everything
    for (const auto& ev : out)
        if (ev.voice == kArpVoiceBase) open[ev.note] += ev.noteOn ? 1 : -1;
    for (const auto& kv : open)
        CHECK(kv.second == 0);
}
