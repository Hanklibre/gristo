#include "TestFramework.h"

#include "gristo/Engine.h"
#include "gristo/Macros.h"
#include "gristo/sound/Sound.h"

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

ChordParams cMajor()
{
    ChordParams c;
    c.enabled = true;
    c.key = 0;
    c.minor = false;
    c.lowestNote = 48;
    c.voicing = ChordVoicing::Close;
    c.wander = 0.0f;
    return c;
}

// Home on a cell of the map.
void home(ChordParams& c, int column, int row = 0)
{
    c.homeX = Chords::columnToX(column);
    c.homeY = Chords::rowToY(row);
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


TEST("harmony: the map is a chain of thirds around the tonic, richer towards the top")
{
    ChordParams c = cMajor();
    const char* const row0[] = { "Gm", "Bb", "Dm", "F", "Am", "C", "Em", "G", "Bm", "D", "F#m" };
    for (int col = -5; col <= 5; ++col)
    {
        CHECK_MSG(Chords::name(c, { col, 0 }) == row0[col + 5], "column " << col << ": " << Chords::name(c, { col, 0 }));
        if (col < 5) // neighbours share two notes
        {
            std::set<int> a, b, common;
            for (int n : notesOf(Chords::build(c, { col, 0 })))
                a.insert(n % 12);
            for (int n : notesOf(Chords::build(c, { col + 1, 0 })))
                b.insert(n % 12);
            for (int n : a)
                if (b.count(n))
                    common.insert(n);
            CHECK_MSG(common.size() == 2, "columns " << col << " and " << col + 1);
        }
    }
    CHECK(notesOf(Chords::build(c, { 0, 0 })) == (std::vector<int> { 48, 52, 55 }));
    CHECK(Chords::name(c, { 0, 1 }) == "Cadd9");
    CHECK(Chords::name(c, { 0, 2 }) == "Cmaj7");
    CHECK(Chords::name(c, { 2, 2 }) == "G7"); // the dominant pulls
    CHECK(Chords::name(c, { -2, 3 }) == "Fmaj9");
    CHECK(Chords::name(c, { -1, 3 }) == "Am9");
    CHECK(Chords::name(c, { 2, 4 }) == "G9sus");
    CHECK(Chords::name(c, { 0, 4 }) == "C6/9");
    CHECK(Chords::name(c, { -1, 4 }) == "Am11");
    CHECK(notesOf(Chords::build(c, { 2, 2 })) == (std::vector<int> { 55, 59, 62, 65 }));

    // A minor key has its own tonic in the middle, on the same chain.
    ChordParams m = cMajor();
    m.key = 9;
    m.minor = true;
    CHECK(Chords::name(m, { 0, 0 }) == "Am");
    CHECK(Chords::name(m, { 1, 0 }) == "C");
    CHECK(Chords::name(m, { -1, 0 }) == "F");
    CHECK(Chords::name(m, { 2, 2 }) == "Em7");

    // The pad's position picks a cell, and every cell has a position.
    for (int col = -5; col <= 5; ++col)
        for (int row = 0; row < kHarmonyRows; ++row)
        {
            home(c, col, row);
            CHECK((Chords::homeCell(c) == HarmonyCell { col, row }));
            const Chord chord = Chords::build(c, { col, row });
            CHECK(chord.count >= 3 && chord.count <= kMaxChordNotes);
        }
}

TEST("harmony: Wander walks in small steps from home, a different way every phrase")
{
    ChordParams c = cMajor();
    home(c, 0, 1);
    c.chordSteps = 16;
    c.phraseBars = 4;
    // No Wander: home, always.
    for (std::int64_t s = 0; s < 640; s += 8)
        CHECK((Chords::positionAt(c, s).cell == HarmonyCell { 0, 1 }));

    double distance[2] = { 0.0, 0.0 };
    int index = 0;
    for (float wander : { 0.3f, 1.0f })
    {
        c.wander = wander;
        std::set<std::vector<int>> walks;
        for (int phrase = 0; phrase < 60; ++phrase)
        {
            std::vector<int> walk;
            HarmonyCell previous = Chords::positionAt(c, phrase * 64).cell;
            CHECK((previous == HarmonyCell { 0, 1 })); // every phrase starts at home
            for (int chord = 1; chord < 4; ++chord)
            {
                const ChordPosition pos = Chords::positionAt(c, phrase * 64 + chord * 16);
                CHECK(pos.valid);
                CHECK(std::abs(pos.cell.column - previous.column) <= 2);
                CHECK(std::abs(pos.cell.row - previous.row) <= 1);
                CHECK(std::abs(pos.cell.column) <= 5 && pos.cell.row >= 0 && pos.cell.row < kHarmonyRows);
                CHECK((Chords::positionAt(c, phrase * 64 + chord * 16 + 15).cell == pos.cell)); // holds for its length
                CHECK(Chords::positionAt(c, phrase * 64 + chord * 16 + 15).serial == pos.serial);
                CHECK(pos.serial != Chords::positionAt(c, phrase * 64 + chord * 16 - 1).serial);
                distance[index] += std::abs(pos.cell.column) + std::abs(pos.cell.row - 1);
                walk.push_back(pos.cell.column * 10 + pos.cell.row);
                previous = pos.cell;
            }
            walks.insert(walk);
        }
        CHECK(walks.size() > 10);
        ++index;
    }
    CHECK(distance[0] > 10.0 && distance[1] > distance[0] * 1.5);

    // Catch: every phrase repeats the walk of the caught one, until released.
    c.wander = 0.8f;
    std::vector<HarmonyCell> caught;
    for (int chord = 0; chord < 4; ++chord)
        caught.push_back(Chords::positionAt(c, 7 * 64 + chord * 16).cell);
    c.frozen = true;
    c.frozenPhrase = 7;
    for (int phrase = 0; phrase < 20; ++phrase)
        for (int chord = 0; chord < 4; ++chord)
            CHECK((Chords::positionAt(c, phrase * 64 + chord * 16).cell == caught[static_cast<size_t>(chord)]));
    // Moving home moves the whole loop along the map.
    home(c, 2, 1);
    for (int chord = 0; chord < 4; ++chord)
        CHECK(std::abs(Chords::positionAt(c, chord * 16).cell.column - (caught[static_cast<size_t>(chord)].column + 2)) <= 4);
}

TEST("chords: held chords change with the harmony, every note is released")
{
    EngineParams p = fourOnFloor();
    p.chords = cMajor();
    p.chords.wander = 1.0f;
    const auto evs = render(p, 256, kBar * 8);
    std::map<std::int64_t, std::set<int>> onsAt;
    std::map<int, int> balance;
    for (const auto& e : evs)
        if (e.voice == kChordVoice)
        {
            if (e.on)
                onsAt[e.sample].insert(e.note);
            balance[e.note] += e.on ? 1 : -1;
        }
    CHECK(onsAt.size() == 8); // one chord per bar
    CHECK(onsAt[0] == (std::set<int> { 48, 52, 55 }));
    CHECK(onsAt[kBar * 4] == (std::set<int> { 48, 52, 55 })); // home again
    std::set<std::set<int>> different;
    for (const auto& [at, notes] : onsAt)
    {
        CHECK(at % kBar == 0);
        different.insert(notes);
    }
    CHECK(different.size() >= 3);
    for (const auto& [note, b] : balance)
        CHECK_MSG(b == 0, "note " << note << " balance " << b);
    // Block size doesn't matter.
    const auto other = render(p, 1000, kBar * 8);
    CHECK(other.size() == evs.size());
}

TEST("chords: stabs play on the chosen drum hits")
{
    EngineParams p = fourOnFloor();
    p.chords = cMajor();
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
}

TEST("chords: an arp following the chords plays chord tones")
{
    EngineParams p = fourOnFloor();
    p.chords = cMajor();
    p.chords.play = ChordPlay::Off; // only feeding the arp
    home(p.chords, -1); // Am
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
            const std::set<int> pcs { 9, 0, 4 };
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
    CHECK(mod::targetName(mod::synthTarget(mod::kChordSynthIndex, mod::kSynthLevel)) == "Chords Level");
    CHECK(mod::targetName(mod::synthTarget(0, mod::kSynthReso)) == "Arp 1 Resonance");
    CHECK(mod::targetName(mod::kChordCutoffTarget) == "Chords Cutoff");
    CHECK(mod::targetName(mod::kFxFeedbackTarget) == "Delay Feedback");
    for (int t = 1; t < mod::kNumTargets; ++t)
        CHECK_MSG(mod::targetName(t) != "None", "target " << t);
}

TEST("modulation: every arp and synth target has one number and a name")
{
    CHECK(mod::targetName(mod::arpTarget(2, mod::kArpTranspose)) == "Arp 3 Transpose");
    CHECK(mod::targetName(mod::arpCutoffTarget(2)) == "Arp 3 Cutoff");
    CHECK(mod::targetName(mod::arpProbTarget(2)) == "Arp 3 Probability");
    CHECK(mod::targetName(mod::synthTarget(mod::synthOfArp(2), mod::kSynthLevel)) == "Arp 3 Level");
    CHECK(mod::targetRange(mod::arpTarget(2, mod::kArpGate)) == 7.75f);
    CHECK(mod::kChordSynthIndex == gristo::sound::kChordSynth);
    // Every target is reachable exactly once.
    std::vector<int> seen(static_cast<size_t>(mod::kNumTargets), 0);
    for (int a = 0; a < kNumArps; ++a)
    {
        for (int w = 0; w < mod::kArpTargetsPerArp; ++w)
            ++seen[static_cast<size_t>(mod::arpTarget(a, w))];
    }
    for (int s = 0; s < gristo::sound::kNumSynths; ++s)
        for (int w = 0; w < mod::kSynthTargets; ++w)
            ++seen[static_cast<size_t>(mod::synthTarget(s, w))];
    for (int t = mod::kArpBase; t < mod::kNumTargets; ++t)
    {
        int arp = 0, synth = 0, which = 0;
        const bool isArp = mod::decodeArpTarget(t, arp, which), isSynth = mod::decodeSynthTarget(t, synth, which);
        CHECK(!(isArp && isSynth));
        if (isArp || isSynth)
            CHECK_MSG(seen[static_cast<size_t>(t)] == 1, "target " << t);
    }
}


namespace {

// Every note-on is followed by its note-off before the same note starts again, and no
// note-off arrives for a note that isn't on.
struct PairCheck
{
    std::map<std::pair<int, int>, int> on;
    int violations = 0;
    void add(const OutputEvent& ev)
    {
        int& n = on[{ ev.voice, ev.note }];
        if (ev.noteOn ? n > 0 : n == 0)
            ++violations;
        n = ev.noteOn ? 1 : 0;
    }
};

} // namespace

TEST("arps: slides never re-open a note that is still sounding (rolls, slides)")
{
    int violations = 0;
    for (std::uint32_t seed = 1; seed < 80; ++seed)
    {
        EngineParams p;
        p.pattern.fill = { 0, 0, 0, 1, 0, 0 }; // open hat on every 16th...
        p.groove.feeling = Feeling::Rolls;     // ...played as rolls
        ArpParams& a = p.arps[0];
        a.enabled = true;
        a.triggers[HiHat2] = true;
        a.mode = ArpMode::Generate;
        a.density = 1.0f;
        a.slide = 0.6f;
        a.seed = seed;
        Engine e;
        e.setParams(p);
        const int held[] = { 60, 67 };
        e.setArpInput(0, held, 2);
        PairCheck check;
        std::vector<OutputEvent> out;
        double ppq = 0.0;
        for (int b = 0; b < 300; ++b)
        {
            out.clear();
            e.process({ true, ppq, 120.0, 48000.0, 256 }, out);
            for (const auto& ev : out)
                if (ev.voice == kArpVoiceBase)
                    check.add(ev);
            ppq += 256 * 120.0 / (60.0 * 48000.0);
        }
        violations += check.violations;
    }
    CHECK_MSG(violations == 0, violations << " unpaired arp notes");
}

TEST("chords: switching Stabs to Held while a stab rings keeps notes paired")
{
    EngineParams p;
    p.pattern.fill = { 0, 1, 0, 0, 0, 0 }; // snare on every 16th
    ChordParams& c = p.chords;
    c = cMajor();
    c.wander = 0.7f;
    c.chordSteps = 8;
    c.play = ChordPlay::Stabs;
    c.stabTriggers = { false, true, false, false, false, false };
    c.stabGateSteps = 8.0f;
    Engine e;
    PairCheck check;
    std::vector<OutputEvent> out;
    double ppq = 0.0;
    while (ppq < 8.0)
    {
        if (ppq >= 3.6)
            c.play = ChordPlay::Held;
        e.setParams(p);
        out.clear();
        e.process({ true, ppq, 120.0, 48000.0, 256 }, out);
        for (const auto& ev : out)
            if (ev.voice == kChordVoice)
                check.add(ev);
        ppq += 256 * 120.0 / (60.0 * 48000.0);
    }
    CHECK_MSG(check.violations == 0, check.violations << " unpaired chord notes");
}

TEST("arps: Mutate stays cheap however far into the song")
{
    // Used to replay every cycle since the start: hours at this position.
    const auto pattern = Arp::pattern(7, 0.3f, 1'000'000'000);
    CHECK(pattern.steps[0].pitch >= 0.0f && pattern.steps[0].pitch <= 1.0f);
    // Mutations still accumulate: a later cycle differs from the first somewhere.
    const auto a = Arp::pattern(7, 0.3f, 1), b = Arp::pattern(7, 0.3f, 50);
    bool differs = false;
    for (int i = 0; i < kMaxArpSteps; ++i)
        differs = differs || a.steps[static_cast<size_t>(i)].pitch != b.steps[static_cast<size_t>(i)].pitch;
    CHECK(differs);
}

TEST("chords: Smooth voicing keeps every chord in one octave, with the root below if asked")
{
    ChordParams c = cMajor();
    c.voicing = ChordVoicing::Smooth;
    CHECK(notesOf(Chords::build(c, { 0, 0 })) == (std::vector<int> { 48, 52, 55 }));
    CHECK(notesOf(Chords::build(c, { -2, 0 })) == (std::vector<int> { 48, 53, 57 })); // F: C stays, E-F and G-A move a step
    CHECK(notesOf(Chords::build(c, { 2, 2 })) == (std::vector<int> { 50, 53, 55, 59 }));
    c.voicing = ChordVoicing::SmoothBass;
    CHECK(notesOf(Chords::build(c, { -2, 0 })) == (std::vector<int> { 41, 48, 53, 57 }));
    for (int col = -5; col <= 5; ++col)
        for (int row = 0; row < kHarmonyRows; ++row)
        {
            const Chord ch = Chords::build(c, { col, row });
            CHECK(ch.count >= 4 && ch.count <= kMaxChordNotes);
            CHECK(ch.notes[0] >= 36 && ch.notes[0] < 48 && ch.notes[ch.count - 1] < 60);
        }
}

TEST("chords: strum spreads the notes, in either direction, and stays paired")
{
    for (int play = 1; play <= 2; ++play)
        for (int dir = 0; dir < 3; ++dir)
        {
            EngineParams p = fourOnFloor();
            p.pattern.fill = { 0, 1, 0, 0, 0, 0 }; // snare on every 16th: stabs closer than the strum
            ChordParams& c = p.chords;
            c = cMajor();
            home(c, 0, 2); // sevenths: four notes
            c.wander = 0.9f;
            c.chordSteps = 8;
            c.play = static_cast<ChordPlay>(play);
            c.strumMs = 40.0f;
            c.strumDirection = static_cast<StrumDirection>(dir);
            c.shift = 0;
            Engine e;
            e.setParams(p);
            PairCheck check;
            std::vector<OutputEvent> out;
            std::vector<std::pair<double, int>> firstOns;
            double ppq = 0.0;
            for (int b = 0; b < 800; ++b)
            {
                out.clear();
                e.process({ true, ppq, 120.0, 48000.0, 256 }, out);
                for (const auto& ev : out)
                    if (ev.voice == kChordVoice)
                    {
                        check.add(ev);
                        if (ev.noteOn && firstOns.size() < 4)
                            firstOns.push_back({ ppq + ev.sampleOffset * 120.0 / (60.0 * 48000.0), ev.note });
                    }
                ppq += 256 * 120.0 / (60.0 * 48000.0);
            }
            out.clear();
            e.process({ false, 0.0, 120.0, 48000.0, 256 }, out);
            for (const auto& ev : out)
                if (ev.voice == kChordVoice)
                    check.add(ev);
            int left = 0;
            for (const auto& kv : check.on)
                left += kv.second;
            CHECK_MSG(check.violations == 0 && left == 0, "play " << play << " dir " << dir << ": " << check.violations << " unpaired, " << left << " hanging");
            if (play == 1) // held: the first chord's four notes, 40 ms = 0.08 beats apart
            {
                CHECK(firstOns.size() == 4);
                if (firstOns.size() == 4)
                {
                    CHECK(std::abs(firstOns[1].first - firstOns[0].first - 0.08) < 0.002);
                    CHECK(dir == 1 ? firstOns[0].second > firstOns[3].second : firstOns[0].second < firstOns[3].second);
                }
            }
        }
}

TEST("chords: Keys mode plays the chord of the key, at once, and feeds the arps")
{
    EngineParams p = fourOnFloor();
    ChordParams& c = p.chords;
    c = cMajor();
    c.mode = HarmonyMode::Keys; // seven keys = the seven columns around home: Dm F Am C Em G Bm
    c.play = ChordPlay::Held;
    c.shift = 0;
    Engine e;
    e.setParams(p);
    std::vector<OutputEvent> out;
    std::set<int> sounding;
    double ppq = 0.3; // off the grid
    auto block = [&] {
        out.clear();
        e.process({ true, ppq, 120.0, 48000.0, 256 }, out);
        int ons = 0;
        for (const auto& ev : out)
            if (ev.voice == kChordVoice)
            {
                if (ev.noteOn) { sounding.insert(ev.note); ++ons; }
                else sounding.erase(ev.note);
            }
        ppq += 256 * 120.0 / (60.0 * 48000.0);
        return ons;
    };
    CHECK(block() == 0 && sounding.empty());
    e.setChordKey(1);
    CHECK(block() == 3); // in the very next block
    CHECK(sounding == (std::set<int> { 53, 57, 60 }));
    CHECK(notesOf(e.chordAt(5)) == (std::vector<int> { 53, 57, 60 }));
    for (int i = 0; i < 50; ++i)
        CHECK(block() == 0);
    e.setChordKey(3);
    block();
    CHECK(sounding == (std::set<int> { 48, 52, 55 }));
    CHECK((Chords::keyCell(c, 10) == HarmonyCell { 0, 1 })); // the next octave is one row richer
    e.setChordKey(-1);
    block();
    CHECK(sounding.empty());
    CHECK(e.chordAt(5).count == 0);
}

TEST("drums: a shorter voice length loops against the others (polymeter)")
{
    Engine e;
    EngineParams p;
    p.pattern.fill = { 0.6f, 0.6f, 0.8f, 0.5f, 0.5f, 0.5f };
    p.pattern.length[HiHat1] = 5;
    e.setParams(p);
    int differs = 0;
    for (std::int64_t s = 0; s < 320; ++s)
    {
        const StepHits now = e.evaluateStep(s), later = e.evaluateStep(s + 5), bar = e.evaluateStep(s + kPatternSteps);
        CHECK(now[HiHat1].on == later[HiHat1].on); // the hat repeats every 5 steps
        CHECK(now[Kick].on == bar[Kick].on);       // the kick keeps the full pattern
        differs += now[Kick].on != e.evaluateStep(s + 5)[Kick].on ? 1 : 0;
    }
    CHECK(differs > 0);
}

TEST("drums: ratchets double or triple hits; Swing per voice scales the shuffle")
{
    EngineParams p = fourOnFloor();
    p.pattern.fill = { 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f };
    p.groove.shuffle = 4.0f;
    p.groove.ratchet[HiHat1] = 0.5f;
    Engine e;
    e.setParams(p);
    int plain = 0, twos = 0, threes = 0;
    for (std::int64_t s = 0; s < 640; ++s)
    {
        const HitInfo h = e.describe(s, HiHat1, 120.0, false);
        if (!h.on)
            continue;
        (h.rollCount == 1 ? plain : h.rollCount == 2 ? twos : threes)++;
        CHECK(h.rollSpacingBeats > 0.0);
        CHECK(e.describe(s, Kick, 120.0, false).rollCount == 1);
    }
    CHECK(plain > 200 && twos > 100 && threes > 30);

    // Swing: 0 = on the straight grid, 0.5 = halfway to the shuffled position.
    double moved = 0.0;
    for (std::int64_t s = 0; s < 32; ++s)
    {
        const double straight = static_cast<double>(s) / kStepsPerBeat;
        const double full = e.describe(s, HiHat1, 120.0, false).timeBeats;
        p.groove.swing[HiHat1] = 0.0f;
        e.setParams(p);
        CHECK(std::abs(e.describe(s, HiHat1, 120.0, false).timeBeats - straight) < 1e-9);
        CHECK(std::abs(e.describe(s, Kick, 120.0, false).timeBeats - full) < 1e-9);
        p.groove.swing[HiHat1] = 0.5f;
        e.setParams(p);
        CHECK(std::abs(e.describe(s, HiHat1, 120.0, false).timeBeats - (straight + full) / 2) < 1e-9);
        p.groove.swing[HiHat1] = 1.0f;
        e.setParams(p);
        moved += std::abs(full - straight);
    }
    CHECK(moved > 0.1);
    // Ratcheted notes stay paired whatever the block size.
    for (int block : { 64, 480 })
    {
        std::map<std::pair<int, int>, int> on;
        int bad = 0;
        for (const Ev& ev : render(p, block, 8 * kBar))
        {
            int& n = on[{ ev.voice, ev.note }];
            bad += (ev.on ? n > 0 : n == 0) ? 1 : 0;
            n = ev.on ? 1 : 0;
        }
        CHECK_MSG(bad == 0, bad << " unpaired at block " << block);
    }
}

TEST("arps: Divide plays every Nth trigger, the same from any start point")
{
    auto count = [](int divide, double startPpq) {
        EngineParams p = fourOnFloor();
        p.pattern.fill = { 0, 0, 1.0f, 0, 0, 0 }; // hat on every 16th
        ArpParams& a = p.arps[0];
        a.enabled = true;
        a.triggers[HiHat1] = true;
        a.divide = divide;
        a.resetBars = 1;
        a.gateSteps = 0.5f;
        Engine e;
        e.setParams(p);
        const int held[] = { 60, 64, 67 };
        e.setArpInput(0, held, 3);
        std::vector<std::pair<long, int>> ons;
        std::vector<OutputEvent> out;
        double ppq = startPpq;
        while (ppq < 8.0)
        {
            out.clear();
            e.process({ true, ppq, 120.0, 48000.0, 256 }, out);
            for (const auto& ev : out)
                if (ev.voice == kArpVoiceBase && ev.noteOn)
                {
                    const double at = ppq + ev.sampleOffset * 120.0 / (60.0 * 48000.0);
                    if (at >= 4.0 - 1e-6)
                        ons.push_back({ std::lround(at * 1000), ev.note });
                }
            ppq += 256 * 120.0 / (60.0 * 48000.0);
        }
        return ons;
    };
    CHECK(count(1, 0.0).size() == 16);
    CHECK(count(3, 0.0).size() == 6); // steps 0, 3, 6, 9, 12, 15 of the bar
    CHECK(count(4, 0.0).size() == 4);
    CHECK(count(3, 0.0) == count(3, 256 * 120.0 / (60.0 * 48000.0) * 300)); // started mid-song (2.5 beats... on the block grid)
}

TEST("macros: Busy, Loose and Wild at rest change nothing; turned, they move their families")
{
    EngineParams base = fourOnFloor();
    base.pattern.fill = { 0.6f, 0.5f, 0.7f, 0.3f, 0.4f, 0.2f };
    EngineParams p = base;
    applyMacros(p, MacroParams {});
    CHECK(p.pattern.fill == base.pattern.fill);
    CHECK(p.groove.shuffle == base.groove.shuffle && p.humanize.amount == base.humanize.amount);
    CHECK(p.pattern.chaos == base.pattern.chaos && p.groove.ratchet == base.groove.ratchet);

    auto hits = [&](float busy) {
        EngineParams q = base;
        MacroParams m;
        m.busy = busy;
        applyMacros(q, m);
        Engine e;
        e.setParams(q);
        int n = 0;
        for (std::int64_t s = 0; s < 64; ++s)
            for (const VoiceHit& h : e.evaluateStep(s))
                n += h.on ? 1 : 0;
        return n;
    };
    CHECK(hits(0.0f) == 0);
    CHECK(hits(0.25f) < hits(0.5f) && hits(0.5f) < hits(0.75f) && hits(0.75f) < hits(1.0f));

    p = base;
    MacroParams m;
    m.loose = 1.0f;
    m.wild = 1.0f;
    m.busy = 1.0f;
    applyMacros(p, m);
    CHECK(p.groove.shuffle > 3.0f && p.groove.shuffle <= 6.0f);
    CHECK(p.humanize.amount > 0.5f && p.humanize.amount <= 1.0f);
    CHECK(p.pattern.chaos > 0.4f && p.pattern.mapRandom > 0.5f && p.arps[0].mutate > 0.5f);
    CHECK(p.groove.ratchet[HiHat1] > 0.2f && p.groove.ratchet[Kick] == 0.0f);
    for (float f : p.pattern.fill)
        CHECK(f >= 0.0f && f <= 1.0f);
}
