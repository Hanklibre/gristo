#pragma once

// Mutable Instruments Rings (Copyright 2015 Emilie Gillet, MIT licence), its original
// code in third_party/eurorack, played by Gristo's notes. The module runs at 48 kHz in
// blocks of 24 samples; so does this, resampled to the host's rate. Each note queued
// with strum() is one strum of the next block (a chord rolls over a few milliseconds,
// as fast as the module can strum). The left output is the module's ODD output, the
// right its EVEN output (with polyphony, odd and even voices; otherwise the two pickups).

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace gristo::sound {

struct RingsSettings
{
    int model = 0;          // 0 modal, 1 sympathetic strings, 2 string, 3 FM, 4 quantized sympathetic, 5 string + reverb
    int polyphony = 1;      // 1, 2 or 4
    float structure = 0.4f; // 0..1, as the module's knobs
    float brightness = 0.5f;
    float damping = 0.6f;
    float position = 0.3f;
    float tonic = 0.0f;     // pitch class of the song's key (the quantized model's chords sit on it)
    bool pluck = true;      // the internal exciter (a pluck / noise burst on every strum)
};

class Rings
{
public:
    Rings();
    ~Rings();
    void prepare(float sampleRate);
    void reset();
    void set(const RingsSettings& settings) { settings_ = settings; }
    void strum(float midiNote); // queued; one strum per block
    // One host-rate sample: `in` excites the resonator (silence: only the plucks do).
    void process(float in, float& outL, float& outR);
    // Recently made sound (cheap: lets the engine skip it when silent).
    bool active() const { return quietBlocks_ < kQuietBlocks; }
    bool idle() const { return !active() && pending_.empty(); }

private:
    static constexpr int kBlock = 24;
    static constexpr int kQuietBlocks = 48000 / kBlock * 8; // 8 s of near silence: sleep
    static constexpr int kRing = 256;                        // 48 kHz output history
    void renderBlock();

    struct Engine; // the module's Part and its reverb memory
    std::unique_ptr<Engine> engine_;
    RingsSettings settings_;
    float hostRate_ = 48000.0f;
    double ratio_ = 1.0;     // module samples per host sample
    // Host -> 48 kHz: the input, linearly interpolated at the module's sample times.
    double nextIn_ = 0.0;    // host time of the next module sample, relative to the last host sample
    float lastIn_ = 0.0f;
    std::array<float, kBlock> inBlock_ {};
    int inCount_ = 0;
    // 48 kHz -> host: cubic interpolation of the module's output, a little behind.
    std::array<float, kRing> outL_ {}, outR_ {};
    std::int64_t produced_ = 0; // module samples rendered so far
    double readPos_ = 0.0;      // module time of the next host output sample
    std::vector<float> pending_; // notes waiting for a block
    int quietBlocks_ = kQuietBlocks;
    float note_ = 60.0f; // the note the resonator is tuned to
    int chord_ = 0;       // the sympathetic models' chord (from Structure, with hysteresis)
    int model_ = -1, polyphony_ = -1;
};

} // namespace gristo::sound
