#pragma once

#include "Dsp.h"

#include <vector>

namespace gristo::sound {

// Juno-style stereo ensemble: two delay lines swept in opposite directions by a slow
// triangle LFO. Mono in, stereo out.
class Chorus
{
public:
    void prepare(float sampleRate, float lfoPhase);
    void reset();
    void process(float in, float amount, float& outL, float& outR);

private:
    float read(float delaySamples) const;
    std::vector<float> line_;
    size_t write_ = 0;
    float sampleRate_ = 48000.0f, phase_ = 0.0f, startPhase_ = 0.0f;
};

// Slow random movement (-1..1): a new random target a few times a second at most,
// glided to. Ticked once per sample.
class Wobble
{
public:
    void prepare(float sampleRate, float hz, std::uint32_t seed);
    float tick()
    {
        if (--counter_ <= 0)
        {
            target_ = noise_.next();
            counter_ = period_ + static_cast<int>(0.5f * static_cast<float>(period_) * noise_.next());
        }
        value_ += (target_ - value_) * coef_;
        return value_;
    }

private:
    dsp::Noise noise_;
    float value_ = 0.0f, target_ = 0.0f, coef_ = 0.001f;
    int counter_ = 0, period_ = 4800;
};

// Old-sampler grit: the signal is re-sampled at a low rate without a clean anti-alias
// filter (so highs fold back as grain) and through coarse converters, then smoothed a
// little, like the output filter of a 12-bit drum machine.
class Crusher
{
public:
    void prepare(float sampleRate);
    void reset();
    void set(float amount); // 0..1
    bool on() const { return wet_ > 0.0f; }
    void process(float& l, float& r);

private:
    float sampleRate_ = 48000.0f, wet_ = 0.0f, step_ = 1.0f, levels_ = 2048.0f, phase_ = 1.0f;
    float heldL_ = 0.0f, heldR_ = 0.0f;
    dsp::OnePole smooth_[2][2];
};

// A channel pushed too hard: asymmetric saturation (even and odd harmonics) that
// squashes peaks and lifts what is underneath.
class Overdrive
{
public:
    void prepare(float sampleRate);
    void reset();
    void set(float amount); // 0..1
    bool on() const { return mix_ > 0.0f; }
    void process(float& l, float& r);

private:
    float sampleRate_ = 48000.0f, mix_ = 0.0f, gain_ = 1.0f, makeup_ = 1.0f;
    dsp::OnePole tone_[2];
    dsp::SoftSaturator sat_[2];
};

// The whole mix through a worn tape machine: saturation, wow and flutter, duller
// highs, a little low-end bump, hiss that comes and goes with the music. Blended in
// with the amount; the lows below ~100 Hz stay clean (a kick or an 808 keeps its
// weight and punch, only what is above them takes the colour).
class Tape
{
public:
    void prepare(float sampleRate);
    void reset();
    void set(float amount); // 0..1
    void process(float& l, float& r);

private:
    float read(const std::vector<float>& line, float delaySamples) const;
    std::vector<float> line_[2];
    size_t write_ = 0;
    float sampleRate_ = 48000.0f, amount_ = 0.0f, mix_ = 0.0f, gain_ = 1.0f, makeup_ = 1.0f;
    float wowPhase_ = 0.0f, flutterPhase_ = 0.0f, wowDepth_ = 0.0f, flutterDepth_ = 0.0f, baseDelay_ = 64.0f;
    float hissLevel_ = 0.0f, hissEnv_ = 0.0f, hissRelease_ = 0.0f;
    Wobble drift_, level_;
    dsp::Noise hiss_ { 0x7a9e11u };
    dsp::OnePole highs_[2], bump_[2], hissTone_, lowDry_[2], lowWet_[2];
    dsp::SoftSaturator sat_[2];
};

// Tuned comb resonator: a short delay line with damped feedback. Fed with a drum it
// rings at its pitch like a shell or a string; fed with a synth it adds a body.
class Resonator
{
public:
    void prepare(float sampleRate);
    void reset();
    void set(float hz, float decaySeconds, float damping); // damping 0 = bright .. 1 = dull
    float process(float in)
    {
        const auto size = line_.size();
        const float pos = static_cast<float>(write_ + size) - delay_;
        const auto i0 = static_cast<size_t>(pos);
        const float frac = pos - static_cast<float>(i0);
        const float read = line_[i0 % size] + (line_[(i0 + 1) % size] - line_[i0 % size]) * frac;
        lp_ += damp_ * (read - lp_);
        if (std::abs(lp_) < 1e-20f)
            lp_ = 0.0f;
        line_[write_] = in + dsp::fastTanh(lp_ * feedback_);
        write_ = (write_ + 1) % size;
        return read;
    }

private:
    std::vector<float> line_ { 0.0f, 0.0f, 0.0f, 0.0f };
    size_t write_ = 0;
    float sampleRate_ = 48000.0f, delay_ = 2.0f, feedback_ = 0.0f, damp_ = 1.0f, lp_ = 0.0f;
};

// A drum's resonant body: eight decaying modes the drum excites, after the modal
// models of drums and bodies (struck membranes and shells, cajon-like boxes, metal plates).
// Each type has its own mode ratios, levels and decays; higher modes die away faster,
// as in real materials. Every strike moves the modes a little (Alive), so no two hits
// ring quite alike. Tube is a harmonic series: the tuned-pipe sound of the old body.
class DrumBody
{
public:
    enum Type : int { Shell = 0, Box, Metal, Tube, Count };
    void prepare(float sampleRate);
    void reset();
    // hz: the body's pitch (its first mode); decaySeconds: of that mode.
    void strike(float hz, float decaySeconds, int type, float variation, std::uint32_t seed);
    float process(float in)
    {
        float out = 0.0f;
        for (int i = 0; i < count_; ++i)
        {
            const float y = b1_[i] * y1_[i] - b2_[i] * y2_[i] + in * g_[i];
            y2_[i] = y1_[i];
            y1_[i] = std::abs(y) < 1e-20f ? 0.0f : y;
            out += y;
        }
        return dsp::fastTanh(out);
    }

private:
    static constexpr int kModes = 8;
    float sampleRate_ = 48000.0f;
    int count_ = 0;
    float b1_[kModes] {}, b2_[kModes] {}, g_[kModes] {}, y1_[kModes] {}, y2_[kModes] {};
};

// Octave-up pitch shifter (two crossfaded taps on a short delay line): fed from the
// reverb's output back into its input, the tail climbs and glows.
class Shimmer
{
public:
    void prepare(float sampleRate);
    void reset();
    float process(float in);

private:
    std::vector<float> line_;
    size_t write_ = 0;
    float window_ = 4096.0f, phase_ = 0.0f;
    dsp::OnePole low_, high_;
};

// Gentle bus compressor before the final soft clip: holds loud passages together
// instead of letting them distort. Pump makes it grab harder and breathe.
class Glue
{
public:
    void prepare(float sampleRate);
    void reset();
    void setPump(float amount); // 0..1
    void process(float& l, float& r);

private:
    float threshold_ = 0.6f, ratio_ = 3.0f, makeup_ = 1.0f;
    float env_ = 0.0f, gain_ = 1.0f, target_ = 1.0f, attack_ = 0.0f, release_ = 0.0f, gainGlide_ = 0.125f;
    int counter_ = 0;
    dsp::OnePole dcL_, dcR_;
};

// Tempo-synced ping-pong delay with a darkening feedback path, a little tape wow and
// soft saturation on the repeats (warmer echoes that can't run away).
class Delay
{
public:
    void prepare(float sampleRate);
    void reset();
    void set(float seconds, float feedback);
    void process(float in, float& outL, float& outR);

private:
    float read(const std::vector<float>& line, float delaySamples) const;
    std::vector<float> left_, right_;
    size_t write_ = 0;
    float sampleRate_ = 48000.0f;
    float time_ = 0.0f, targetTime_ = 0.0f, feedback_ = 0.35f;
    float glideCoef_ = 0.0005f, wowPhase_ = 0.0f, wowInc_ = 0.0f, wowDepth_ = 0.0f;
    dsp::OnePole damp_[2], lowCut_[2];
};

} // namespace gristo::sound
