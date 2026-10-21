#include "TestFramework.h"

#include "gristo/Engine.h"

#include <algorithm>
#include <map>
#include <set>
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

constexpr double kSr = 48000.0;
constexpr double kBpm = 120.0;
constexpr std::int64_t kBar = 96000; // samples per bar at 120 bpm

std::vector<Ev> render(const EngineParams& p, int blockSize, std::int64_t total, bool stopAtEnd = true)
{
    Engine e;
    e.setParams(p);
    std::vector<Ev> all;
    std::vector<OutputEvent> out;
    for (std::int64_t pos = 0; pos < total; pos += blockSize)
    {
        BlockInfo b { true, static_cast<double>(pos) * kBpm / (60.0 * kSr), kBpm, kSr,
                      static_cast<int>(std::min<std::int64_t>(blockSize, total - pos)) };
        out.clear();
        e.process(b, out);
        for (const auto& ev : out)
            all.push_back({ pos + ev.sampleOffset, ev.voice, ev.note, ev.velocity, ev.noteOn });
    }
    if (stopAtEnd)
    {
        out.clear();
        e.process(BlockInfo { false, 0.0, kBpm, kSr, blockSize }, out);
        for (const auto& ev : out)
            all.push_back({ total, ev.voice, ev.note, ev.velocity, ev.noteOn });
    }
    return all;
}

ChordSlot slot(int degree, ChordType type = ChordType::Triad, int len = 16, bool borrowed = false, int inversion = 0)
{
    ChordSlot s;
    s.used = true;
    s.degree = degree;
    s.type = type;
    s.lengthSteps = len;
    s.borrowed = borrowed;
    s.inversion = inversion;
    return s;
}

ChordParams cMajor()
{
    ChordParams c;
    c.enabled = true;
    c.key = 0;
    c.scale = ChordScale::Major;
    c.lowestNote = 48;
    return c;
}

EngineParams fourOnFloor()
{
    EngineParams p;
    p.pattern.mapX = 0.0f; // disco node
    p.pattern.mapY = 0.0f;
    p.pattern.fill = { 0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f };
    p.groove.shuffle = 0.0f;
    p.humanize.amount = 0.0f;
    return p;
}

std::vector<int> notesOf(const Chord& c) { return std::vector<int>(c.notes.begin(), c.notes.begin() + c.count); }

} // namespace

TEST("chords: built from the scale, named and numbered like Scaler")
{
    const ChordParams c = cMajor();
    CHECK(notesOf(Chords::build(c, slot(0))) == (std::vector<int> { 48, 52, 55 }));
    CHECK(notesOf(Chords::build(c, slot(5, ChordType::Seventh))) == (std::vector<int> { 57, 60, 64, 67 }));
    CHECK(Chords::name(c, slot(0)) == "C");
    CHECK(Chords::name(c, slot(5, ChordType::Seventh)) == "Am7");
    CHECK(Chords::roman(c, slot(5, ChordType::Seventh)) == "vi7");
    CHECK(Chords::name(c, slot(4, ChordType::Seventh)) == "G7");
    CHECK(Chords::roman(c, slot(4, ChordType::Seventh)) == "V7");
    CHECK(Chords::name(c, slot(3, ChordType::Seventh)) == "Fmaj7");
    CHECK(Chords::roman(c, slot(3, ChordType::Seventh)) == "IVmaj7");
    CHECK(Chords::name(c, slot(6)) == "Bdim");
    CHECK(Chords::roman(c, slot(6)) == "viio");
    CHECK(Chords::name(c, slot(6, ChordType::Seventh)) == "Bm7b5");
    CHECK(Chords::name(c, slot(1, ChordType::Ninth)) == "Dm9");
    CHECK(Chords::name(c, slot(4, ChordType::Ninth)) == "G9");
    CHECK(Chords::name(c, slot(3, ChordType::Ninth)) == "Fmaj9");
    CHECK(Chords::name(c, slot(2, ChordType::Ninth)) == "Em7b9");
    CHECK(Chords::name(c, slot(6, ChordType::Ninth)) == "Bm7b5b9");
    CHECK(Chords::name(c, slot(0, ChordType::Sus4)) == "Csus4");
    // Borrowed from C minor: iv and bVI.
    CHECK(Chords::name(c, slot(3, ChordType::Triad, 16, true)) == "Fm");
    CHECK(Chords::roman(c, slot(3, ChordType::Triad, 16, true)) == "iv");
    CHECK(Chords::name(c, slot(5, ChordType::Triad, 16, true)) == "Ab");
    CHECK(Chords::roman(c, slot(5, ChordType::Triad, 16, true)) == "bVI");
    // A minor, first inversion.
    ChordParams a = c;
    a.key = 9;
    a.scale = ChordScale::Minor;
    CHECK(Chords::name(a, slot(0)) == "Am");
    CHECK(Chords::name(a, slot(0, ChordType::Triad, 16, false, 1)) == "Am/C");
    const auto inv = notesOf(Chords::build(a, slot(0, ChordType::Triad, 16, false, 1)));
    CHECK(inv.size() == 3 && inv[0] % 12 == 0); // C in the bass
}

TEST("chords: bars and drum-hit positions step through the used slots")
{
    ChordParams c = cMajor();
    c.slots[0] = slot(0, ChordType::Triad, 16);
    c.slots[2] = slot(4, ChordType::Triad, 16); // a gap in the slots is skipped
    c.slots[3] = slot(5, ChordType::Triad, 8);
    CHECK(Chords::usedCount(c) == 3);
    CHECK(Chords::barsPosition(c, 0).slot == 0);
    CHECK(Chords::barsPosition(c, 15).slot == 0);
    CHECK(Chords::barsPosition(c, 16).slot == 2);
    CHECK(Chords::barsPosition(c, 32).slot == 3);
    CHECK(Chords::barsPosition(c, 40).slot == 0);
    CHECK(Chords::barsPosition(c, 40).loop == 1);
    CHECK(Chords::barsPosition(c, 40).serial != Chords::barsPosition(c, 0).serial);
    CHECK(Chords::barsPosition(c, -1).slot == 3); // before the song start it wraps

    c.hitsPerChord = 4;
    CHECK(Chords::drumsPosition(c, 0, 0).slot == 0); // before the first hit
    CHECK(Chords::drumsPosition(c, 0, 4).slot == 0);
    CHECK(Chords::drumsPosition(c, 0, 5).slot == 2); // the 5th hit starts the next chord
    CHECK(Chords::drumsPosition(c, 0, 9).slot == 3);
    CHECK(Chords::drumsPosition(c, 0, 13).slot == 0);
}

TEST("chords: generator and library give usable, deterministic progressions")
{
    for (int sc = 0; sc < static_cast<int>(ChordScale::Count); ++sc)
        for (std::uint32_t seed = 1; seed < 40; ++seed)
        {
            const auto a = Chords::generate(seed, static_cast<ChordScale>(sc));
            const auto b = Chords::generate(seed, static_cast<ChordScale>(sc));
            int used = 0;
            for (int i = 0; i < kMaxChordSlots; ++i)
            {
                used += a[static_cast<size_t>(i)].used ? 1 : 0;
                CHECK(a[static_cast<size_t>(i)].degree == b[static_cast<size_t>(i)].degree);
            }
            CHECK(used >= 4);
        }
    for (const auto& entry : Chords::library())
    {
        CHECK(!entry.slots.empty());
        ChordParams c = cMajor();
        c.scale = entry.scale;
        for (const auto& s : entry.slots)
            CHECK_MSG(!Chords::name(c, s).empty(), entry.name);
    }
}

TEST("chords: held chords change on the slot boundaries, every note is released")
{
    EngineParams p = fourOnFloor();
    p.chords = cMajor();
    p.chords.slots[0] = slot(0);
    p.chords.slots[1] = slot(3);
    const auto evs = render(p, 256, kBar * 4);
    std::map<std::int64_t, std::set<int>> onsAt;
    std::map<int, int> balance;
    for (const auto& e : evs)
        if (e.voice == kChordVoice)
        {
            if (e.on)
                onsAt[e.sample].insert(e.note);
            balance[e.note] += e.on ? 1 : -1;
        }
    CHECK(onsAt.size() == 4);
    CHECK(onsAt[0] == (std::set<int> { 48, 52, 55 }));
    CHECK(onsAt[kBar] == (std::set<int> { 53, 57, 60 }));
    CHECK(onsAt[kBar * 2] == (std::set<int> { 48, 52, 55 }));
    for (const auto& [note, b] : balance)
        CHECK_MSG(b == 0, "note " << note << " balance " << b);
    // Block size doesn't matter.
    const auto other = render(p, 1000, kBar * 4);
    CHECK(other.size() == evs.size());
}

TEST("chords: stabs play on the chosen drum hits; drum clock follows the kick")
{
    EngineParams p = fourOnFloor();
    p.chords = cMajor();
    p.chords.slots[0] = slot(0);
    p.chords.slots[1] = slot(4);
    p.chords.play = ChordPlay::Stabs;
    p.chords.stabTriggers = { false, true, false, false, false, false };
    const auto evs = render(p, 256, kBar * 2);
    int snares = 0, stabNotes = 0;
    std::map<int, int> balance;
    for (const auto& e : evs)
    {
        if (e.on && e.voice == Snare)
            ++snares;
        if (e.voice == kChordVoice)
        {
            stabNotes += e.on ? 1 : 0;
            balance[e.note] += e.on ? 1 : -1;
        }
    }
    CHECK(snares > 0);
    CHECK(stabNotes == snares * 3);
    for (const auto& [note, b] : balance)
        CHECK(b == 0);

    // Drum clock: 4 kicks per chord on a four-on-the-floor = one chord per bar.
    p.chords.play = ChordPlay::Held;
    p.chords.clock = ChordClock::Drums;
    p.chords.clockVoice = Kick;
    p.chords.hitsPerChord = 4;
    Engine e;
    e.setParams(p);
    CHECK(e.chordPositionAt(0).slot == 0);
    CHECK(e.chordPositionAt(15).slot == 0);
    CHECK(e.chordPositionAt(16).slot == 1);
    CHECK(e.chordPositionAt(32).slot == 0);
}

TEST("chords: an arp following the chords plays chord tones")
{
    EngineParams p = fourOnFloor();
    p.chords = cMajor();
    p.chords.play = ChordPlay::Off; // only feeding the arp
    p.chords.slots[0] = slot(0);
    p.chords.slots[1] = slot(5);
    ArpParams& a = p.arps[0];
    a.enabled = true;
    a.followChords = true;
    a.triggers = { true, false, false, false, false, false };
    a.resetBars = 1;
    const auto evs = render(p, 256, kBar * 2);
    int arpNotes = 0;
    for (const auto& e : evs)
        if (e.on && e.voice == kArpVoiceBase)
        {
            ++arpNotes;
            const std::set<int> pcs = e.sample < kBar ? std::set<int> { 0, 4, 7 } : std::set<int> { 9, 0, 4 };
            CHECK_MSG(pcs.count(e.note % 12) == 1, "note " << e.note << " at " << e.sample);
        }
        else if (e.voice == kChordVoice)
        {
            CHECK(false); // Off: nothing played by the chord part
        }
    CHECK(arpNotes == 8);
}

TEST("arps: Probability skips steps, deterministic and block-size independent")
{
    EngineParams p = fourOnFloor();
    ArpParams& a = p.arps[0];
    a.enabled = true;
    a.triggers = { true, false, false, false, false, false };
    a.mode = ArpMode::Generate;
    a.density = 1.0f;
    auto count = [](const std::vector<Ev>& evs) {
        int n = 0;
        for (const auto& e : evs)
            n += e.on && e.voice == kArpVoiceBase ? 1 : 0;
        return n;
    };
    const int all = count(render(p, 256, kBar * 16));
    a.probability = 0.0f;
    CHECK(count(render(p, 256, kBar * 16)) == 0);
    a.probability = 0.5f;
    const auto half = render(p, 256, kBar * 16);
    const int n = count(half);
    CHECK_MSG(n > all / 4 && n < all * 3 / 4, n << " of " << all);
    CHECK(count(render(p, 999, kBar * 16)) == n);
    CHECK(mod::targetName(mod::arpProbTarget(1)) == "Arp 2 Probability");
}

TEST("arps: the drums' Probability doesn't thin out the arp")
{
    EngineParams p = fourOnFloor();
    ArpParams& a = p.arps[0];
    a.enabled = true;
    a.triggers = { true, false, false, false, false, false };
    a.mode = ArpMode::Generate;
    a.density = 1.0f;
    auto counts = [](const std::vector<Ev>& evs) {
        int kicks = 0, arps = 0;
        for (const auto& e : evs)
            if (e.on)
            {
                kicks += e.voice == Kick ? 1 : 0;
                arps += e.voice == kArpVoiceBase ? 1 : 0;
            }
        return std::make_pair(kicks, arps);
    };
    const auto full = counts(render(p, 256, kBar * 4));
    p.pattern.probability[Kick] = 0.0f;
    const auto silent = counts(render(p, 256, kBar * 4));
    CHECK(full.first > 0);
    CHECK(silent.first == 0);            // the kick no longer plays...
    CHECK(silent.second == full.second); // ...but the arp still steps on its pattern
}

TEST("modulation: sound targets have names and ranges")
{
    CHECK(mod::targetName(mod::kitTarget(mod::kKitTune)) == "Kit Tune");
    CHECK(mod::targetRange(mod::kitTarget(mod::kKitTune)) == 24.0f);
    CHECK(mod::targetName(mod::synthTarget(2, mod::kSynthLevel)) == "Chords Level");
    CHECK(mod::targetName(mod::synthTarget(0, mod::kSynthReso)) == "Arp 1 Resonance");
    CHECK(mod::targetName(mod::kChordCutoffTarget) == "Chords Cutoff");
    CHECK(mod::targetName(mod::kFxFeedbackTarget) == "Delay Feedback");
    CHECK(mod::kNumTargets == mod::kFxFeedbackTarget + 1);
    for (int t = 1; t < mod::kNumTargets; ++t)
        CHECK_MSG(mod::targetName(t) != "None", "target " << t);
}

