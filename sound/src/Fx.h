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

// Gentle bus compressor before the final soft clip: holds loud passages together
// instead of letting them distort.
class Glue
{
public:
    void prepare(float sampleRate);
    void reset();
    void process(float& l, float& r);

private:
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
