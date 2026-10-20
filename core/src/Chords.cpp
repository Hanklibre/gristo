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
constexpr int kMajor[7] = { 0, 2, 4, 5, 7, 9, 11 };
const char* const kPitchNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
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
        case ChordType::Count: break;
    }
    return { 0, 2, 4 };
}

// Intervals above the chord root (before inversion).
std::vector<int> intervals(const ChordParams& p, const ChordSlot& slot)
{
    const int* scale = scaleOf(p, slot);
    const int degree = std::clamp(slot.degree, 0, 6);
    const int root = semitone(scale, degree);
    std::vector<int> out;
    for (int i : stack(slot.type))
        out.push_back(semitone(scale, degree + i) - root);
    return out;
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
        case ChordType::Count: break;
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
    const int* scale = scaleOf(p, s);
    const int degree = std::clamp(s.degree, 0, 6);
    // First tonic at or above lowestNote, then the chord root above it.
    int tonic = floorMod(p.key, 12);
    while (tonic < p.lowestNote)
        tonic += 12;
    const int root = tonic + semitone(scale, degree);
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
    const int* scale = scaleOf(p, s);
    const int rootPc = floorMod(p.key + scale[std::clamp(s.degree, 0, 6)], 12);
    const auto iv = intervals(p, s);
    std::string quality;
    const bool major3 = has(iv, 4), minor3 = has(iv, 3), dim5 = has(iv, 6), aug5 = has(iv, 8);
    if (s.type == ChordType::Sus2 || s.type == ChordType::Sus4)
        quality = std::string(s.type == ChordType::Sus2 ? "sus2" : "sus4") + (dim5 ? "b5" : "");
    else
    {
        std::string triad = major3 ? (aug5 ? "aug" : "") : (dim5 ? "dim" : "m");
        if (minor3 && !dim5)
            triad = "m";
        if (s.type == ChordType::Triad)
            quality = triad;
        else
        {
            const bool maj7 = has(iv, 11), min7 = has(iv, 10), dim7 = has(iv, 9) && dim5;
            const std::string ext = s.type == ChordType::Ninth ? "9" : "7";
            if (major3 && maj7) quality = (aug5 ? "maj" + ext + "#5" : "maj" + ext);
            else if (major3 && min7) quality = ext;
            else if (minor3 && dim5 && dim7) quality = "dim7";
            else if (minor3 && dim5) quality = "m" + ext + "b5";
            else if (minor3 && maj7) quality = "mMaj" + ext;
            else quality = "m" + ext;
            if (s.type == ChordType::Ninth && has(iv, 13))
                quality = (quality == "9" ? "7" : quality.substr(0, quality.size() - 1) + "7") + "b9";
        }
    }
    std::string out = std::string(kPitchNames[rootPc]) + quality;
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
    const int* scale = scaleOf(p, s);
    std::string accidental;
    if (s.borrowed)
    {
        const int diff = scale[degree] - own[degree];
        accidental = diff < 0 ? "b" : diff > 0 ? "#" : "";
    }
    (void) kMajor;
    const auto iv = intervals(p, s);
    std::string r = kRomans[degree];
    const bool minor = has(iv, 3) && !has(iv, 4);
    if (minor)
        for (char& ch : r)
            ch = static_cast<char>(ch - 'A' + 'a');
    std::string suffix;
    if (has(iv, 6) && (s.type == ChordType::Triad || s.type == ChordType::Seventh))
        suffix = has(iv, 10) ? "m7b5" : "o";
    else if (has(iv, 8) && has(iv, 4))
        suffix = "+";
    if (s.type == ChordType::Seventh && suffix.empty())
        suffix = has(iv, 11) ? "maj7" : "7";
    if (s.type == ChordType::Ninth)
        suffix += "9";
    if (s.type == ChordType::Sus2) suffix += "sus2";
    if (s.type == ChordType::Sus4) suffix += "sus4";
    return accidental + r + suffix;
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
    };
    return entries;
}

} // namespace gristo
