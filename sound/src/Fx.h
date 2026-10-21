#pragma once

#include "Dsp.h"

#include <vector>

namespace gristo::sound {

// Freeverb-style stereo reverb (8 damped combs + 4 allpasses per side).
class Reverb
{
public:
    void prepare(float sampleRate);
    void reset();
    void setSize(float size); // 0..1
    void process(float in, float& outL, float& outR);

private:
    struct Comb
    {
        std::vector<float> buffer;
        size_t index = 0;
        float store = 0.0f;
    };
    struct Allpass
    {
        std::vector<float> buffer;
        size_t index = 0;
    };
    Comb combs_[2][8];
    Allpass allpasses_[2][4];
    float feedback_ = 0.84f, damp_ = 0.35f;
    // Pre-delay keeps the dry attack clear; the low cut keeps basses from muddying the tail.
    std::vector<float> preDelay_;
    size_t preIndex_ = 0;
    dsp::OnePole lowCut_;
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
    float env_ = 0.0f, gain_ = 1.0f, target_ = 1.0f, attack_ = 0.0f, release_ = 0.0f;
    int counter_ = 0;
    dsp::OnePole dcL_, dcR_;
};

// Tempo-synced ping-pong delay with a darkening feedback path.
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
    dsp::OnePole damp_[2], lowCut_[2];
};

} // namespace gristo::sound
