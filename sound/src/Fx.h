#pragma once

#include "Dsp.h"

#include <vector>

namespace gristo::sound {

// Plate reverb after Dattorro ("Effect Design, Part 1", 1997): input diffusion into
// a figure-of-eight tank of two modulated allpass + delay branches, with stereo taps
// spread around the tank. Smooth and dense, without the metallic ring of comb banks;
// the slow modulation keeps long tails from sounding static. Size sets decay and
// damping only (no delay lengths change), so lanes can sweep it without artefacts.
class Reverb
{
public:
    void prepare(float sampleRate);
    void reset();
    void setSize(float size); // 0..1
    void process(float in, float& outL, float& outR);

private:
    struct Line // power-of-two circular delay line, read before write
    {
        std::vector<float> buffer;
        std::uint32_t mask = 0, write = 0;
        void init(int maxDelay);
        void clear();
        float at(int delay) const { return buffer[(write - static_cast<std::uint32_t>(delay)) & mask]; }
        float atFrac(float delay) const;
        void push(float x)
        {
            buffer[write] = x;
            write = (write + 1) & mask;
        }
    };
    float allpass(Line& line, int delay, float gain, float x);
    float allpassMod(Line& line, float delay, float gain, float x);

    float sampleRate_ = 48000.0f;
    // Pre-delay keeps the dry attack clear; the low cut keeps basses from muddying the tail.
    Line preDelay_;
    int preDelayLength_ = 1;
    dsp::OnePole lowCut_, bandwidth_;
    Line diffusers_[4];
    int diffuserLength_[4] {};
    Line tankAp1_[2], tankDelay1_[2], tankAp2_[2], tankDelay2_[2];
    int ap1Length_[2] {}, delay1Length_[2] {}, ap2Length_[2] {}, delay2Length_[2] {};
    float excursion_ = 0.0f, lfoPhase_[2] {}, lfoInc_[2] {};
    dsp::OnePole damping_[2];
    float tankOut_[2] {};
    int tapsL_[7] {}, tapsR_[7] {};
    float decay_ = 0.5f, decayDiffusion2_ = 0.5f, dampHz_ = 7000.0f;
};

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
    float sampleRate_ = 48000.0f, mix_ = 0.0f, gain_ = 1.0f, bias_ = 0.0f, offset_ = 0.0f, makeup_ = 1.0f;
    dsp::OnePole tone_[2];
};

// The whole mix through a worn tape machine: saturation, wow and flutter, duller
// highs, a little low-end bump, hiss that comes and goes with the music.
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
    dsp::OnePole highs_[2], bump_[2], hissTone_;
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
