// gristo_render: render the engine to a Standard MIDI File, for auditioning grooves
// in a DAW and for tuning the groove tables by ear.
//
//   gristo_render --x 0.4 --y 0.7 --timing shake --shuffle 3 --humanize 0.4 \
//                 --bars 8 --bpm 92 --out groove.mid

#include <gristo/Engine.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace gristo;

namespace {

void usage()
{
    std::puts(
        "usage: gristo_render [options] --out file.mid\n"
        "  --x F --y F            map position (0..1)\n"
        "  --fill F               fill for all voices (0..1)\n"
        "  --fill-voice N F       fill for voice N (0=kick .. 5=perc2)\n"
        "  --prob F               probability for all voices (0..1)\n"
        "  --prob-voice N F       probability for voice N\n"
        "  --chaos F              (0..1)\n"
        "  --timing NAME          trip | shake | push | clave\n"
        "  --shuffle F            0..6\n"
        "  --feeling NAME         tight | rolls | flam | slop\n"
        "  --humanize F           0..1\n"
        "  --lock                 humanize repeats every 2 bars\n"
        "  --linear               linear mode\n"
        "  --bars N --bpm F       length and tempo (default 8 bars, 95 bpm)\n"
        "  --seed N               variation\n");
}

int indexOf(const char* value, std::initializer_list<const char*> names)
{
    int i = 0;
    for (const char* n : names)
    {
        if (std::strcmp(value, n) == 0)
            return i;
        ++i;
    }
    std::fprintf(stderr, "unknown value '%s'\n", value);
    std::exit(2);
}

void writeVarLen(std::vector<unsigned char>& out, std::uint32_t v)
{
    unsigned char buf[5];
    int n = 0;
    buf[n++] = static_cast<unsigned char>(v & 0x7F);
    while ((v >>= 7) != 0)
        buf[n++] = static_cast<unsigned char>((v & 0x7F) | 0x80);
    while (n > 0)
        out.push_back(buf[--n]);
}

void writeBE(std::ofstream& f, std::uint32_t v, int bytes)
{
    for (int i = bytes - 1; i >= 0; --i)
        f.put(static_cast<char>((v >> (8 * i)) & 0xFF));
}

} // namespace

int main(int argc, char** argv)
{
    EngineParams params;
    params.groove.shuffle = 2.0f;
    int bars = 8;
    double bpm = 95.0;
    std::string outPath;

    for (int i = 1; i < argc; ++i)
    {
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) { usage(); std::exit(2); }
            return argv[++i];
        };
        const std::string a = argv[i];
        if (a == "--x") params.pattern.mapX = std::strtof(next(), nullptr);
        else if (a == "--y") params.pattern.mapY = std::strtof(next(), nullptr);
        else if (a == "--fill") params.pattern.fill.fill(std::strtof(next(), nullptr));
        else if (a == "--fill-voice")
        {
            const int v = std::atoi(next());
            const float f = std::strtof(next(), nullptr);
            if (v >= 0 && v < kNumVoices) params.pattern.fill[static_cast<size_t>(v)] = f;
        }
        else if (a == "--prob") params.pattern.probability.fill(std::strtof(next(), nullptr));
        else if (a == "--prob-voice")
        {
            const int v = std::atoi(next());
            const float f = std::strtof(next(), nullptr);
            if (v >= 0 && v < kNumVoices) params.pattern.probability[static_cast<size_t>(v)] = f;
        }
        else if (a == "--chaos") params.pattern.chaos = std::strtof(next(), nullptr);
        else if (a == "--timing") params.groove.timing = static_cast<Timing>(indexOf(next(), { "trip", "shake", "push", "clave" }));
        else if (a == "--shuffle") params.groove.shuffle = std::strtof(next(), nullptr);
        else if (a == "--feeling") params.groove.feeling = static_cast<Feeling>(indexOf(next(), { "tight", "rolls", "flam", "slop" }));
        else if (a == "--humanize") params.humanize.amount = std::strtof(next(), nullptr);
        else if (a == "--lock") params.humanize.lock = true;
        else if (a == "--linear") params.pattern.linear = true;
        else if (a == "--bars") bars = std::atoi(next());
        else if (a == "--bpm") bpm = std::strtod(next(), nullptr);
        else if (a == "--seed")
        {
            const auto s = static_cast<std::uint32_t>(std::atoi(next()));
            params.pattern.seed += s * 7919u;
            params.humanize.seed += s * 104729u;
        }
        else if (a == "--out") outPath = next();
        else { usage(); return a == "--help" || a == "-h" ? 0 : 2; }
    }
    if (outPath.empty() || bars <= 0 || bpm <= 0.0)
    {
        usage();
        return 2;
    }

    // Render through the real scheduler, block by block, like a host would.
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;
    constexpr int ticksPerBeat = 960;
    const int noteNumbers[kNumVoices] = { 36, 38, 42, 46, 39, 45 };

    Engine engine;
    engine.setParams(params);
    const auto totalSamples = static_cast<std::int64_t>(bars * 4 * 60.0 / bpm * sampleRate);
    const double beatsPerSample = bpm / (60.0 * sampleRate);

    struct TimedEvent { std::int64_t tick; unsigned char status, data1, data2; };
    std::vector<TimedEvent> events;
    std::vector<OutputEvent> block;
    for (std::int64_t pos = 0; pos < totalSamples; pos += blockSize)
    {
        BlockInfo b { true, static_cast<double>(pos) * beatsPerSample, bpm, sampleRate, blockSize };
        block.clear();
        engine.process(b, block);
        for (const auto& e : block)
        {
            const auto tick = static_cast<std::int64_t>((static_cast<double>(pos + e.sampleOffset) * beatsPerSample) * ticksPerBeat + 0.5);
            const auto note = static_cast<unsigned char>(noteNumbers[e.voice]);
            const auto vel = static_cast<unsigned char>(e.noteOn ? std::max(1, static_cast<int>(e.velocity * 127.0f + 0.5f)) : 0);
            events.push_back({ tick, static_cast<unsigned char>(e.noteOn ? 0x99 : 0x89), note, vel });
        }
    }
    block.clear();
    engine.process({ false, 0.0, bpm, sampleRate, blockSize }, block); // release
    for (const auto& e : block)
        events.push_back({ static_cast<std::int64_t>(bars) * 4 * ticksPerBeat, 0x89, static_cast<unsigned char>(noteNumbers[e.voice]), 0 });

    std::vector<unsigned char> track;
    const auto tempo = static_cast<std::uint32_t>(60000000.0 / bpm);
    track.insert(track.end(), { 0x00, 0xFF, 0x51, 0x03,
                                static_cast<unsigned char>(tempo >> 16), static_cast<unsigned char>(tempo >> 8), static_cast<unsigned char>(tempo) });
    std::int64_t lastTick = 0;
    for (const auto& e : events)
    {
        const std::int64_t tick = std::max(e.tick, lastTick);
        writeVarLen(track, static_cast<std::uint32_t>(tick - lastTick));
        lastTick = tick;
        track.insert(track.end(), { e.status, e.data1, e.data2 });
    }
    track.insert(track.end(), { 0x00, 0xFF, 0x2F, 0x00 });

    std::ofstream f(outPath, std::ios::binary);
    if (!f)
    {
        std::fprintf(stderr, "cannot write %s\n", outPath.c_str());
        return 1;
    }
    f.write("MThd", 4);
    writeBE(f, 6, 4);
    writeBE(f, 0, 2);
    writeBE(f, 1, 2);
    writeBE(f, ticksPerBeat, 2);
    f.write("MTrk", 4);
    writeBE(f, static_cast<std::uint32_t>(track.size()), 4);
    f.write(reinterpret_cast<const char*>(track.data()), static_cast<std::streamsize>(track.size()));

    int notes = 0;
    for (const auto& e : events) notes += (e.status & 0xF0) == 0x90 ? 1 : 0;
    std::printf("wrote %s: %d bars at %.1f bpm, %d notes\n", outPath.c_str(), bars, bpm, notes);
    return 0;
}
