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
