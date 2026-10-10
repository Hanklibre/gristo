#include "gristo/Arp.h"

#include "gristo/Random.h"

#include <algorithm>
#include <cmath>

namespace gristo {

namespace {

constexpr std::uint32_t kPitchStream = 0xA2901u;
constexpr std::uint32_t kRestStream = 0xA2902u;
constexpr std::uint32_t kAccentStream = 0xA2903u;
constexpr std::uint32_t kSlideStream = 0xA2904u;
constexpr std::uint32_t kWalkStream = 0xA2905u;
constexpr std::uint32_t kMutateStream = 0xA2906u;
constexpr std::uint32_t kOrderStream = 0xA2907u;

struct ScaleDef
{
    const char* name;
    int count;
    int intervals[12];
};

constexpr ScaleDef kScales[] = {
    { "Major", 7, { 0, 2, 4, 5, 7, 9, 11 } },
    { "Minor", 7, { 0, 2, 3, 5, 7, 8, 10 } },
    { "Dorian", 7, { 0, 2, 3, 5, 7, 9, 10 } },
    { "Phrygian", 7, { 0, 1, 3, 5, 7, 8, 10 } },
    { "Lydian", 7, { 0, 2, 4, 6, 7, 9, 11 } },
    { "Mixolydian", 7, { 0, 2, 4, 5, 7, 9, 10 } },
    { "Harmonic minor", 7, { 0, 2, 3, 5, 7, 8, 11 } },
    { "Minor pentatonic", 5, { 0, 3, 5, 7, 10 } },
    { "Major pentatonic", 5, { 0, 2, 4, 7, 9 } },
    { "Blues", 6, { 0, 3, 5, 6, 7, 10 } },
    { "Chromatic", 12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
};
static_assert(sizeof(kScales) / sizeof(kScales[0]) == static_cast<size_t>(ArpScale::Count), "scale table");

ArpStepValues randomStep(std::uint32_t seed, std::int64_t salt, int i)
{
    ArpStepValues s;
    s.pitch = rnd::unit(rnd::hash(seed, kPitchStream, salt, i));
    s.rest = rnd::unit(rnd::hash(seed, kRestStream, salt, i));
    s.accent = rnd::unit(rnd::hash(seed, kAccentStream, salt, i));
    s.slide = rnd::unit(rnd::hash(seed, kSlideStream, salt, i));
    return s;
}

} // namespace

const char* arpOrderName(ArpOrder order)
{
    switch (order)
    {
        case ArpOrder::Up: return "Up";
        case ArpOrder::Down: return "Down";
        case ArpOrder::UpDown: return "Up/Down";
        case ArpOrder::Random: return "Random";
        case ArpOrder::AsPlayed: return "As played";
        case ArpOrder::Count: break;
    }
    return "";
}

const char* arpScaleName(ArpScale scale)
{
    const int i = static_cast<int>(scale);
    return (i >= 0 && i < static_cast<int>(ArpScale::Count)) ? kScales[i].name : "";
}

ArpPattern Arp::pattern(std::uint32_t seed, float mutate, std::int64_t cycle)
{
    ArpPattern pat;
    // Base pattern: pitches follow a random walk (mostly small moves, sometimes a
    // leap), which sounds more like a line than independent random notes.
    float pitch = rnd::unit(rnd::hash(seed, kWalkStream, -1, 0));
    for (int i = 0; i < kMaxArpSteps; ++i)
    {
        ArpStepValues s = randomStep(seed, 0, i);
        const float move = rnd::unit(rnd::hash(seed, kWalkStream, 0, i));
        if (move < 0.65f)
            pitch = std::clamp(pitch + (s.pitch - 0.5f) * 0.45f, 0.0f, 0.999f);
        else
            pitch = s.pitch;
        s.pitch = pitch;
        pat.steps[static_cast<size_t>(i)] = s;
    }

    // Mutations accumulate: cycle c has been mutated c times.
    const float m = std::clamp(mutate, 0.0f, 1.0f);
    if (m > 0.0f)
        for (std::int64_t c = 1; c <= cycle; ++c)
            for (int i = 0; i < kMaxArpSteps; ++i)
                if (rnd::unit(rnd::hash(seed, kMutateStream, c, i)) < m)
                    pat.steps[static_cast<size_t>(i)] = randomStep(seed, c, i);
    return pat;
}

ArpPool Arp::pool(const ArpParams& p, const int* held, int heldCount)
{
    ArpPool out;
    const int octaves = std::clamp(p.octaves, 1, 4);
    auto add = [&](int note) {
        if (out.size < kMaxArpPool && note >= 0 && note <= 127)
            out.notes[static_cast<size_t>(out.size++)] = note;
    };

    if (heldCount > 0)
    {
        std::array<int, kMaxHeldNotes> base {};
        const int n = std::min(heldCount, kMaxHeldNotes);
        std::copy(held, held + n, base.begin());
        if (!(p.mode == ArpMode::Arp && p.order == ArpOrder::AsPlayed))
            std::sort(base.begin(), base.begin() + n);
        for (int o = 0; o < octaves; ++o)
            for (int i = 0; i < n; ++i)
                add(base[static_cast<size_t>(i)] + 12 * o);
        return out;
    }

    if (p.mode == ArpMode::Generate)
    {
        const ScaleDef& sc = kScales[std::clamp(static_cast<int>(p.scale), 0, static_cast<int>(ArpScale::Count) - 1)];
        for (int o = 0; o < octaves; ++o)
            for (int i = 0; i < sc.count; ++i)
                add(p.root + 12 * o + sc.intervals[i]);
        add(p.root + 12 * octaves); // top root closes the range
    }
    return out;
}

ArpNote Arp::noteFor(const ArpParams& p, const ArpPool& pool, const ArpPattern& pattern, std::int64_t k,
                     std::int64_t cycle)
{
    ArpNote out;
    if (pool.size <= 0 || k < 0)
        return out;
    const int n = pool.size;

    if (p.mode == ArpMode::Generate)
    {
        const int length = std::clamp(p.length, 1, kMaxArpSteps);
        const ArpStepValues& s = pattern.steps[static_cast<size_t>(k % length)];
        if (s.rest >= std::clamp(p.density, 0.0f, 1.0f))
            return out; // rest
        const int index = std::min(n - 1, static_cast<int>(s.pitch * static_cast<float>(n)));
        out.play = true;
        out.note = pool.notes[static_cast<size_t>(index)];
        out.accent = s.accent < std::clamp(p.accent, 0.0f, 1.0f);
        out.slide = s.slide < std::clamp(p.slide, 0.0f, 1.0f);
        return out;
    }

    int index = 0;
    switch (p.order)
    {
        case ArpOrder::Up:
        case ArpOrder::AsPlayed:
            index = static_cast<int>(k % n);
            break;
        case ArpOrder::Down:
            index = n - 1 - static_cast<int>(k % n);
            break;
        case ArpOrder::UpDown:
        {
            const int period = std::max(1, 2 * n - 2);
            const int pos = static_cast<int>(k % period);
            index = pos < n ? pos : period - pos;
            break;
        }
        case ArpOrder::Random:
            index = static_cast<int>(rnd::hash(p.seed, kOrderStream, cycle, k) % static_cast<std::uint32_t>(n));
            break;
        case ArpOrder::Count:
            break;
    }
    out.play = true;
    out.note = pool.notes[static_cast<size_t>(std::clamp(index, 0, n - 1))];
    return out;
}

} // namespace gristo
