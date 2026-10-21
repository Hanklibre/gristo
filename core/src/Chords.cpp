#include "gristo/Chords.h"

#include "gristo/Random.h"

#include <algorithm>

namespace gristo {

namespace {

constexpr int kScales[static_cast<int>(ChordScale::Count)][7] = {
    { 0, 2, 4, 5, 7, 9, 11 }, // Major
    { 0, 2, 3, 5, 7, 8, 10 }, // Minor
    { 0, 2, 3, 5, 7, 9, 10 }, // Dorian
    { 0, 1, 3, 5, 7, 8, 10 }, // Phrygian
    { 0, 2, 4, 6, 7, 9, 11 }, // Lydian
    { 0, 2, 4, 5, 7, 9, 10 }, // Mixolydian
    { 0, 2, 3, 5, 7, 8, 11 }, // Harmonic minor
};
const char* const kPitchNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
const char* const kFlatNames[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
const char* const kSharpNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char* const kRomans[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };

const int* scaleOf(const ChordParams& p, const ChordSlot& slot)
{
    const auto s = slot.borrowed ? Chords::parallel(p.scale) : p.scale;
    return kScales[std::clamp(static_cast<int>(s), 0, static_cast<int>(ChordScale::Count) - 1)];
}

int semitone(const int* scale, int index) // scale degree index (may exceed 6) -> semitones
{
    return scale[index % 7] + 12 * (index / 7);
}

// Scale degree indices stacked for a chord type (relative to the root degree).
std::vector<int> stack(ChordType type)
{
    switch (type)
    {
        case ChordType::Triad: return { 0, 2, 4 };
        case ChordType::Seventh: return { 0, 2, 4, 6 };
        case ChordType::Ninth: return { 0, 2, 4, 6, 8 };
        case ChordType::Sus2: return { 0, 1, 4 };
        case ChordType::Sus4: return { 0, 3, 4 };
        case ChordType::Sixth: return { 0, 2, 4, 5 };
        case ChordType::SixNine: return { 0, 2, 4, 5, 8 };
        case ChordType::Add9: return { 0, 2, 4, 8 };
        case ChordType::Eleventh: return { 0, 2, 6, 8, 10 }; // the 5th left out: five notes max
        case ChordType::Count: break;
    }
    return { 0, 2, 4 };
}

// A quality's chord tones in semitones above the root.
struct Tones
{
    int third, fifth, seventh, ninth, eleventh, sixth;
};

Tones tonesOf(ChordQuality q)
{
    switch (q)
    {
        case ChordQuality::Minor: return { 3, 7, 10, 14, 17, 9 };
        case ChordQuality::Dominant: return { 4, 7, 10, 14, 18, 9 };
        case ChordQuality::Diminished: return { 3, 6, 9, 14, 17, 9 };
        case ChordQuality::HalfDiminished: return { 3, 6, 10, 14, 17, 9 };
        case ChordQuality::Augmented: return { 4, 8, 10, 14, 18, 9 };
        default: return { 4, 7, 11, 14, 18, 9 }; // Major: maj7, #11 (Lydian colour)
    }
}

int alterOf(const ChordSlot& slot) { return std::clamp(slot.alter, -1, 1); }

// The quality actually used: an altered root has no scale chord, so it plays major.
ChordQuality qualityOf(const ChordSlot& slot)
{
    const auto q = static_cast<ChordQuality>(std::clamp(static_cast<int>(slot.quality), 0, static_cast<int>(ChordQuality::Count) - 1));
    return q == ChordQuality::Scale && alterOf(slot) != 0 ? ChordQuality::Major : q;
}

// Intervals above the chord root (before inversion).
std::vector<int> intervals(const ChordParams& p, const ChordSlot& slot)
{
    const auto quality = qualityOf(slot);
    if (quality == ChordQuality::Scale)
    {
        const int* scale = scaleOf(p, slot);
        const int degree = std::clamp(slot.degree, 0, 6);
        const int root = semitone(scale, degree);
        std::vector<int> out;
        for (int i : stack(slot.type))
            out.push_back(semitone(scale, degree + i) - root);
        return out;
    }
    const Tones t = tonesOf(quality);
    const bool dominant = quality == ChordQuality::Dominant;
    switch (slot.type)
    {
        case ChordType::Seventh: return { 0, t.third, t.fifth, t.seventh };
        case ChordType::Ninth: return { 0, t.third, t.fifth, t.seventh, t.ninth };
        case ChordType::Sus2: return dominant ? std::vector<int> { 0, 2, t.fifth, 10 } : std::vector<int> { 0, 2, t.fifth };
        case ChordType::Sus4: return dominant ? std::vector<int> { 0, 5, t.fifth, 10 } : std::vector<int> { 0, 5, t.fifth };
        case ChordType::Sixth: return { 0, t.third, t.fifth, t.sixth };
        case ChordType::SixNine: return { 0, t.third, t.fifth, t.sixth, t.ninth };
        case ChordType::Add9: return { 0, t.third, t.fifth, t.ninth };
        case ChordType::Eleventh: return { 0, t.third, t.seventh, t.ninth, t.eleventh };
        default: return { 0, t.third, t.fifth };
    }
}

// Semitones from the tonic to the chord root.
int rootOffset(const ChordParams& p, const ChordSlot& slot)
{
    return scaleOf(p, slot)[std::clamp(slot.degree, 0, 6)] + alterOf(slot);
}

bool has(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }

ChordSlot slot(int degree, ChordType type = ChordType::Triad, int lengthSteps = 16, bool borrowed = false, int inversion = 0)
{
    ChordSlot s;
    s.used = true;
    s.degree = degree;
    s.type = type;
    s.lengthSteps = lengthSteps;
    s.borrowed = borrowed;
    s.inversion = inversion;
    return s;
}

// The same slot with a chord quality and / or an altered root.
ChordSlot as(ChordSlot s, ChordQuality quality, int alter = 0)
{
    s.quality = quality;
    s.alter = alter;
    return s;
}

} // namespace

const char* chordScaleName(ChordScale scale)
{
    switch (scale)
    {
        case ChordScale::Major: return "Major";
        case ChordScale::Minor: return "Minor";
        case ChordScale::Dorian: return "Dorian";
        case ChordScale::Phrygian: return "Phrygian";
        case ChordScale::Lydian: return "Lydian";
        case ChordScale::Mixolydian: return "Mixolydian";
        case ChordScale::HarmonicMinor: return "Harmonic Minor";
        case ChordScale::Count: break;
    }
    return "";
}

const char* chordTypeName(ChordType type)
{
    switch (type)
    {
        case ChordType::Triad: return "Triad";
        case ChordType::Seventh: return "7th";
        case ChordType::Ninth: return "9th";
        case ChordType::Sus2: return "sus2";
        case ChordType::Sus4: return "sus4";
        case ChordType::Sixth: return "6";
        case ChordType::SixNine: return "6/9";
        case ChordType::Add9: return "add9";
        case ChordType::Eleventh: return "11";
        case ChordType::Count: break;
    }
    return "";
}

const char* chordQualityName(ChordQuality quality)
{
    switch (quality)
    {
        case ChordQuality::Scale: return "Scale";
        case ChordQuality::Major: return "Major";
        case ChordQuality::Minor: return "Minor";
        case ChordQuality::Dominant: return "Dominant";
        case ChordQuality::Diminished: return "Dim";
        case ChordQuality::HalfDiminished: return "Half-dim";
        case ChordQuality::Augmented: return "Aug";
        case ChordQuality::Count: break;
    }
    return "";
}

const char* pitchClassName(int pitchClass) { return kPitchNames[floorMod(pitchClass, 12)]; }

ChordScale Chords::parallel(ChordScale scale)
{
    switch (scale)
    {
        case ChordScale::Major:
        case ChordScale::Lydian:
        case ChordScale::Mixolydian: return ChordScale::Minor;
        default: return ChordScale::Major;
    }
}

Chord Chords::build(const ChordParams& p, const ChordSlot& s)
{
    Chord c;
    // First tonic at or above lowestNote, then the chord root above it.
    int tonic = floorMod(p.key, 12);
    while (tonic < p.lowestNote)
        tonic += 12;
    const int root = tonic + rootOffset(p, s);
    std::vector<int> notes;
    for (int i : intervals(p, s))
        notes.push_back(root + i);
    // Inversions move the lowest notes up an octave.
    const int inversions = std::clamp(s.inversion, 0, static_cast<int>(notes.size()) - 1);
    for (int i = 0; i < inversions; ++i)
    {
        notes.push_back(notes.front() + 12);
        notes.erase(notes.begin());
    }
    // Keep the voicing compact: high roots drop an octave so chords don't run away.
    if (notes.front() >= tonic + 12)
        for (int& n : notes)
            n -= 12;
    std::sort(notes.begin(), notes.end());
    c.count = std::min(static_cast<int>(notes.size()), kMaxChordNotes);
    for (int i = 0; i < c.count; ++i)
        c.notes[static_cast<size_t>(i)] = std::clamp(notes[static_cast<size_t>(i)], 0, 127);
    return c;
}

std::string Chords::name(const ChordParams& p, const ChordSlot& s)
{
    if (!s.used)
        return "";
    const int rootPc = floorMod(p.key + rootOffset(p, s), 12);
    const auto iv = intervals(p, s);
    const bool major3 = has(iv, 4), minor3 = has(iv, 3), dim5 = has(iv, 6), aug5 = has(iv, 8) && !has(iv, 7);
    std::string triad = major3 ? (aug5 ? "aug" : "") : (dim5 ? "dim" : "m");
    if (minor3 && !dim5)
        triad = "m";
    if (!major3 && !minor3)
        triad = "";
    // The 7th-chord name, also the base of 9ths and 11ths.
    auto seventh = [&] {
        const bool maj7 = has(iv, 11), min7 = has(iv, 10), dim7 = has(iv, 9) && dim5;
        if (major3 && maj7) return std::string(aug5 ? "maj7#5" : "maj7");
        if (major3 && min7) return std::string(aug5 ? "7#5" : "7");
        if (minor3 && dim5 && dim7) return std::string("dim7");
        if (minor3 && dim5) return std::string("m7b5");
        if (minor3 && maj7) return std::string("mMaj7");
        return std::string("m7");
    };
    auto ninth = [&](const std::string& q7) -> std::string {
        if (q7 == "7") return "9";
        if (q7 == "maj7") return "maj9";
        if (q7 == "m7") return "m9";
        if (q7 == "mMaj7") return "mMaj9";
        if (q7 == "m7b5") return "m9b5";
        return q7 + "add9";
    };
    std::string quality;
    switch (s.type)
    {
        case ChordType::Triad: quality = triad; break;
        case ChordType::Sus2:
        case ChordType::Sus4:
            quality = std::string(has(iv, 10) ? "7" : has(iv, 11) ? "maj7" : "") + (s.type == ChordType::Sus2 ? "sus2" : "sus4") + (dim5 ? "b5" : "");
            break;
        case ChordType::Seventh: quality = seventh(); break;
        case ChordType::Ninth:
            quality = has(iv, 13) ? seventh() + "b9" : ninth(seventh()); // flat 9: Phrygian / Locrian colours
            break;
        case ChordType::Eleventh:
        {
            const auto q7 = seventh();
            const bool sharp11 = has(iv, 18);
            if (has(iv, 13))
                quality = q7 + "(b9" + (sharp11 ? ",#11)" : ",11)");
            else if (sharp11)
                quality = ninth(q7) + "#11";
            else if (q7 == "7") quality = "11";
            else if (q7 == "maj7") quality = "maj11";
            else if (q7 == "m7") quality = "m11";
            else if (q7 == "mMaj7") quality = "mMaj11";
            else if (q7 == "m7b5") quality = "m11b5";
            else quality = ninth(q7) + "add11";
            break;
        }
        case ChordType::Sixth: quality = triad + (has(iv, 9) ? "6" : "b6"); break;
        case ChordType::SixNine: quality = triad + (has(iv, 9) ? "6" : "b6") + (has(iv, 13) ? "/b9" : "/9"); break;
        case ChordType::Add9: quality = triad + (has(iv, 13) ? "addb9" : "add9"); break;
        case ChordType::Count: break;
    }
    const auto* names = alterOf(s) < 0 ? kFlatNames : alterOf(s) > 0 ? kSharpNames : kPitchNames;
    std::string out = std::string(names[rootPc]) + quality;
    if (s.inversion > 0)
    {
        const auto c = build(p, s);
        out += "/" + std::string(kPitchNames[floorMod(c.notes[0], 12)]);
    }
    return out;
}

std::string Chords::roman(const ChordParams& p, const ChordSlot& s)
{
    if (!s.used)
        return "";
    const int degree = std::clamp(s.degree, 0, 6);
    const int* own = kScales[std::clamp(static_cast<int>(p.scale), 0, static_cast<int>(ChordScale::Count) - 1)];
    const int offset = rootOffset(p, s);
    const auto iv = intervals(p, s);
    const bool major3 = has(iv, 4), minor3 = has(iv, 3) && !major3, dim5 = has(iv, 6);
    const bool dimTriad = minor3 && dim5, augTriad = major3 && has(iv, 8) && !has(iv, 7);

    std::string sfx;
    switch (s.type)
    {
        case ChordType::Triad: sfx = dimTriad ? "o" : augTriad ? "+" : ""; break;
        case ChordType::Seventh: sfx = dimTriad ? (has(iv, 10) ? "m7b5" : "o7") : augTriad ? "+7" : has(iv, 11) ? "maj7" : "7"; break;
        case ChordType::Ninth: sfx = dimTriad ? "o9" : augTriad ? "+9" : has(iv, 11) ? "maj9" : "9"; break;
        case ChordType::Eleventh: sfx = std::string(dimTriad ? "o" : has(iv, 11) ? "maj" : "") + (has(iv, 18) ? "9#11" : "11"); break;
        case ChordType::Sixth: sfx = has(iv, 9) ? "6" : "b6"; break;
        case ChordType::SixNine: sfx = "6/9"; break;
        case ChordType::Add9: sfx = "add9"; break;
        case ChordType::Sus2: sfx = std::string(has(iv, 10) ? "7" : "") + "sus2"; break;
        case ChordType::Sus4: sfx = std::string(has(iv, 10) ? "7" : "") + "sus4"; break;
        case ChordType::Count: break;
    }

    auto numeral = [](int d, bool lower) {
        std::string r = kRomans[d];
        if (lower)
            for (char& ch : r)
                ch = static_cast<char>(ch - 'A' + 'a');
        return r;
    };

    // A dominant 7th / 9th a fifth above a chord of the key is a secondary dominant: V7/vi.
    // Only for chords from outside the scale: a Dorian IV7 stays IV7.
    const bool chromatic = qualityOf(s) != ChordQuality::Scale || s.borrowed;
    const bool dominant7 = major3 && has(iv, 10) && (s.type == ChordType::Seventh || s.type == ChordType::Ninth);
    if (chromatic && dominant7 && !(degree == 0 && alterOf(s) == 0))
    {
        const int target = floorMod(offset - 7, 12);
        for (int d = 1; d < 7; ++d)
            if (own[d] == target)
            {
                const bool targetMinor = floorMod(own[(d + 2) % 7] - own[d], 12) == 3;
                return "V" + sfx + "/" + numeral(d, targetMinor);
            }
    }

    // Accidental: how far the root sits from the key's own degree.
    const int diff = offset - own[degree];
    const std::string accidental = diff < 0 ? std::string(static_cast<size_t>(-diff), 'b') : std::string(static_cast<size_t>(diff), '#');
    return accidental + numeral(degree, minor3) + sfx;
}

int Chords::usedCount(const ChordParams& p)
{
    int n = 0;
    for (const auto& s : p.slots)
        n += s.used ? 1 : 0;
    return n;
}

ChordPosition Chords::barsPosition(const ChordParams& p, std::int64_t step)
{
    ChordPosition pos;
    std::int64_t total = 0;
    for (const auto& s : p.slots)
        if (s.used)
            total += std::max(1, s.lengthSteps);
    if (total == 0)
        return pos;
    pos.loop = floorDiv(step, total);
    std::int64_t t = floorMod(step, total);
    int k = 0;
    for (int i = 0; i < kMaxChordSlots; ++i)
    {
        const auto& s = p.slots[static_cast<size_t>(i)];
        if (!s.used)
            continue;
        const int len = std::max(1, s.lengthSteps);
        if (t < len)
        {
            pos.slot = i;
            pos.serial = pos.loop * kMaxChordSlots + k;
            return pos;
        }
        t -= len;
        ++k;
    }
    return pos;
}

ChordPosition Chords::drumsPosition(const ChordParams& p, std::int64_t step, std::int64_t hitsThrough)
{
    ChordPosition pos;
    const int used = usedCount(p);
    if (used == 0)
        return pos;
    const int resetLen = std::clamp(p.resetBars, 1, 16) * kStepsPerBar;
    const std::int64_t cycle = floorDiv(step, resetLen);
    const std::int64_t change = std::max<std::int64_t>(0, hitsThrough - 1) / std::max(1, p.hitsPerChord);
    const int k = static_cast<int>(change % used);
    int seen = 0;
    for (int i = 0; i < kMaxChordSlots; ++i)
        if (p.slots[static_cast<size_t>(i)].used && seen++ == k)
            pos.slot = i;
    pos.loop = cycle * 1024 + change / used;
    pos.serial = cycle * 4096 + change;
    return pos;
}

ChordSlot Chords::played(const ChordParams& p, const ChordPosition& pos)
{
    if (pos.slot < 0)
        return {};
    ChordSlot s = p.slots[static_cast<size_t>(pos.slot)];
    if (p.mutate > 0.0f && pos.loop > 0)
    {
        const auto loop = static_cast<std::uint32_t>(pos.loop);
        const auto slotIndex = static_cast<std::uint32_t>(pos.slot);
        if (rnd::unit(rnd::hash(p.seed, 0xC401u, loop, slotIndex)) < p.mutate)
        {
            // Related chords: the relative (+5), the mediant (+2), or the IV / V side.
            constexpr int moves[4] = { 5, 2, 3, 4 };
            const int move = moves[rnd::hash(p.seed, 0xC402u, loop, slotIndex) % 4u];
            s.degree = floorMod(s.degree + move, 7);
            s.borrowed = false;
            s.quality = ChordQuality::Scale;
            s.alter = 0;
        }
    }
    return s;
}

std::array<ChordSlot, kMaxChordSlots> Chords::generate(std::uint32_t seed, ChordScale scale)
{
    const bool minorish = scale == ChordScale::Minor || scale == ChordScale::Dorian || scale == ChordScale::Phrygian
                       || scale == ChordScale::HarmonicMinor;
    static const std::vector<std::vector<int>> majorShapes = {
        { 0, 4, 5, 3 }, { 0, 5, 3, 4 }, { 1, 4, 0, 0 }, { 0, 3, 0, 4 }, { 5, 3, 0, 4 }, { 0, 2, 3, 4 }, { 3, 4, 2, 5 },
    };
    static const std::vector<std::vector<int>> minorShapes = {
        { 0, 5, 2, 6 }, { 0, 3, 4, 0 }, { 0, 6, 5, 6 }, { 0, 5, 3, 4 }, { 0, 3, 6, 2 }, { 0, 0, 5, 6 }, { 5, 6, 0, 0 },
    };
    const auto& shapes = minorish ? minorShapes : majorShapes;
    const auto& shape = shapes[rnd::hash(seed, 0xC501u, 0, 0) % shapes.size()];
    const bool doubled = rnd::unit(rnd::hash(seed, 0xC502u, 0, 0)) < 0.35f; // 8 half-bar chords
    std::array<ChordSlot, kMaxChordSlots> out {};
    const int count = doubled ? 8 : 4;
    for (int i = 0; i < count; ++i)
    {
        const auto ui = static_cast<std::uint32_t>(i);
        ChordSlot s;
        s.used = true;
        s.degree = shape[static_cast<size_t>(doubled ? i / 2 : i)];
        if (doubled && i % 2 == 1) // second half: a related colour of the same chord
            s.degree = floorMod(s.degree + (rnd::unit(rnd::hash(seed, 0xC503u, ui, 0)) < 0.5f ? 2 : 0), 7);
        const float t = rnd::unit(rnd::hash(seed, 0xC504u, ui, 0));
        s.type = t < 0.5f ? ChordType::Triad : t < 0.82f ? ChordType::Seventh : t < 0.92f ? ChordType::Ninth
               : t < 0.96f ? ChordType::Sus2 : ChordType::Sus4;
        s.borrowed = i == count - 1 && rnd::unit(rnd::hash(seed, 0xC505u, ui, 0)) < 0.15f;
        s.inversion = rnd::unit(rnd::hash(seed, 0xC506u, ui, 0)) < 0.25f ? 1 : 0;
        s.lengthSteps = doubled ? 8 : 16;
        out[static_cast<size_t>(i)] = s;
    }
    return out;
}

const std::vector<Chords::LibraryEntry>& Chords::library()
{
    using T = ChordType;
    using Q = ChordQuality;
    static const std::vector<LibraryEntry> entries = {
        { "Pop Axis (I V vi IV)", "Pop", ChordScale::Major, { slot(0), slot(4), slot(5), slot(3) } },
        { "Doo-wop (I vi IV V)", "Pop", ChordScale::Major, { slot(0), slot(5), slot(3), slot(4) } },
        { "Jazz ii-V-I", "Jazz", ChordScale::Major, { slot(1, T::Seventh), slot(4, T::Seventh), slot(0, T::Seventh, 32) } },
        { "Gospel ii9-V9-I", "Soul", ChordScale::Major, { slot(1, T::Ninth), slot(4, T::Ninth), slot(0, T::Ninth, 32) } },
        { "Lo-fi Dreams", "Lo-fi", ChordScale::Major,
          { slot(3, T::Seventh), slot(2, T::Seventh), slot(5, T::Seventh), slot(0, T::Seventh) } },
        { "Neo Soul Dorian", "Soul", ChordScale::Dorian, { slot(0, T::Ninth, 32), slot(3, T::Ninth, 32) } },
        { "Epic Minor (i VI III VII)", "Cinematic", ChordScale::Minor, { slot(0), slot(5), slot(2), slot(6) } },
        { "Reggaeton (i VI III VII)", "Latin", ChordScale::Minor,
          { slot(0, T::Triad, 16, false, 1), slot(5), slot(2, T::Triad, 16, false, 1), slot(6) } },
        { "Dark Trap", "Trap", ChordScale::HarmonicMinor, { slot(0, T::Triad, 32), slot(5), slot(4) } },
        { "Sad Piano (i iv VI v)", "Ballad", ChordScale::Minor, { slot(0), slot(3), slot(5), slot(4) } },
        { "Deep House", "House", ChordScale::Minor, { slot(0, T::Seventh, 32), slot(5, T::Seventh, 32) } },
        { "Boom Bap Jazz", "Hip hop", ChordScale::Dorian,
          { slot(0, T::Seventh), slot(3, T::Seventh), slot(0, T::Seventh), slot(4, T::Seventh) } },
        { "Lydian Float (I II)", "Ambient", ChordScale::Lydian, { slot(0, T::Seventh, 32), slot(1, T::Triad, 32) } },
        { "Mixolydian Rock (I bVII IV)", "Rock", ChordScale::Mixolydian, { slot(0), slot(6), slot(3), slot(0) } },
        { "Borrowed iv", "Bittersweet", ChordScale::Major, { slot(0), slot(3), slot(0), slot(3, T::Triad, 16, true) } },
        { "Phrygian Tension", "Dark", ChordScale::Phrygian, { slot(0, T::Triad, 32), slot(1), slot(0) } },

        // Slow, unresolved major-7th / 9th colour in the spirit of Harold Budd.
        { "Pavilion Haze", "Budd-inspired", ChordScale::Major,
          { slot(0, T::Ninth, 32), slot(5, T::Ninth, 32), slot(3, T::Eleventh, 32), slot(5, T::Seventh, 32, true) } },
        { "Mirror Lydian", "Budd-inspired", ChordScale::Lydian,
          { slot(0, T::Eleventh, 32), slot(1, T::Add9, 32), slot(6, T::Seventh, 32), slot(4, T::Seventh, 32) } },
        { "Glass Planing (maj9s)", "Budd-inspired", ChordScale::Major,
          { slot(0, T::Ninth, 32), as(slot(6, T::Ninth, 32), Q::Major, -1), as(slot(5, T::Ninth, 32), Q::Major, -1),
            as(slot(6, T::Ninth, 32), Q::Major, -1) } },
        { "Felt Sixths", "Budd-inspired", ChordScale::Major,
          { slot(0, T::SixNine, 32), slot(2, T::Seventh, 32), slot(3, T::Ninth, 32), slot(3, T::Sixth, 32, true) } },

        { "Gymnopedie No. 1 vamp", "Satie", ChordScale::Major, { slot(3, T::Seventh), slot(0, T::Seventh) } },
        { "Gnossienne No. 1 mood", "Satie", ChordScale::HarmonicMinor,
          { slot(0, T::Triad, 32), slot(3), slot(0), slot(4), slot(0) } },
        { "Sarabande Ninths", "Satie", ChordScale::Major,
          { as(slot(1, T::Ninth), Q::Dominant), as(slot(2, T::Ninth), Q::Dominant, -1), as(slot(1, T::Ninth), Q::Dominant),
            slot(0, T::Ninth) } },

        { "Pachelbel Canon", "Classical", ChordScale::Major,
          { slot(0, T::Triad, 8), slot(4, T::Triad, 8), slot(5, T::Triad, 8), slot(2, T::Triad, 8), slot(3, T::Triad, 8),
            slot(0, T::Triad, 8), slot(3, T::Triad, 8), slot(4, T::Triad, 8) } },
        { "Bach Prelude in C (opening)", "Classical", ChordScale::Major,
          { slot(0), slot(1, T::Seventh), slot(4, T::Seventh, 16, false, 1), slot(0), slot(5, T::Triad, 16, false, 1),
            as(slot(1, T::Seventh), Q::Dominant), slot(4, T::Triad, 16, false, 1), slot(0, T::Seventh) } },
        { "Andalusian Cadence", "Classical", ChordScale::Minor, { slot(0), slot(6), slot(5), slot(4, T::Triad, 16, true) } },
        { "Romantic Minor Cadence", "Classical", ChordScale::Minor,
          { slot(0), slot(3), slot(5), slot(1, T::Seventh), as(slot(4, T::Seventh), Q::Dominant), slot(0) } },
        { "Debussy Planing (9ths)", "Classical", ChordScale::Major,
          { as(slot(0, T::Ninth), Q::Dominant), as(slot(6, T::Ninth), Q::Dominant, -1), as(slot(5, T::Ninth), Q::Dominant, -1),
            as(slot(6, T::Ninth), Q::Dominant, -1) } },

        { "Autumn Leaves", "Jazz standards", ChordScale::Minor,
          { slot(3, T::Seventh), slot(6, T::Seventh), slot(2, T::Seventh), slot(5, T::Seventh), slot(1, T::Seventh),
            as(slot(4, T::Seventh), Q::Dominant), as(slot(0, T::Sixth, 32), Q::Minor) } },
        { "Fly Me to the Moon (cycle)", "Jazz standards", ChordScale::Major,
          { slot(5, T::Seventh), slot(1, T::Seventh), slot(4, T::Seventh), slot(0, T::Seventh), slot(3, T::Seventh),
            slot(6, T::Seventh), as(slot(2, T::Seventh), Q::Dominant), slot(5, T::Seventh) } },
        { "Blue Bossa", "Jazz standards", ChordScale::Minor,
          { slot(0, T::Seventh, 32), slot(3, T::Seventh, 32), slot(1, T::Seventh), as(slot(4, T::Seventh), Q::Dominant),
            slot(0, T::Seventh, 32), as(slot(2, T::Seventh), Q::Minor), as(slot(5, T::Seventh), Q::Dominant),
            as(slot(1, T::Seventh, 32), Q::Major, -1) } },
        { "Rhythm Changes (A)", "Jazz standards", ChordScale::Major,
          { slot(0, T::Sixth, 8), slot(5, T::Seventh, 8), slot(1, T::Seventh, 8), slot(4, T::Seventh, 8), slot(2, T::Seventh, 8),
            as(slot(5, T::Seventh, 8), Q::Dominant), slot(1, T::Seventh, 8), slot(4, T::Seventh, 8) } },
        { "Take the A Train", "Jazz standards", ChordScale::Major,
          { slot(0, T::Sixth, 32), as(slot(1, T::Eleventh, 32), Q::Dominant), slot(1, T::Seventh), slot(4, T::Seventh),
            slot(0, T::Sixth, 32) } },
        { "So What (modal)", "Jazz standards", ChordScale::Dorian,
          { slot(0, T::Eleventh, 32), slot(0, T::Eleventh, 32), as(slot(0, T::Eleventh, 32), Q::Minor, 1), slot(0, T::Eleventh, 32) } },
        { "Maiden Voyage (7sus4)", "Jazz standards", ChordScale::Major,
          { as(slot(0, T::Sus4, 32), Q::Dominant), as(slot(2, T::Sus4, 32), Q::Dominant, -1), as(slot(0, T::Sus4, 32), Q::Dominant),
            as(slot(1, T::Sus4, 32), Q::Dominant, -1) } },
        { "Minor ii-V-i", "Jazz standards", ChordScale::Minor,
          { slot(1, T::Seventh), as(slot(4, T::Seventh), Q::Dominant), as(slot(0, T::SixNine, 32), Q::Minor) } },
        { "Tritone Sub ii-bII7-I", "Jazz standards", ChordScale::Major,
          { slot(1, T::Seventh), as(slot(1, T::Seventh), Q::Dominant, -1), slot(0, T::Ninth, 32) } },
        { "Jazz Blues (12 bars)", "Jazz standards", ChordScale::Major,
          { as(slot(0, T::Seventh, 32), Q::Dominant), as(slot(0, T::Seventh, 32), Q::Dominant), as(slot(3, T::Seventh, 32), Q::Dominant),
            as(slot(0, T::Seventh, 32), Q::Dominant), slot(1, T::Seventh), slot(4, T::Seventh), as(slot(0, T::Seventh), Q::Dominant),
            slot(4, T::Seventh) } },
    };
    return entries;
}

} // namespace gristo
