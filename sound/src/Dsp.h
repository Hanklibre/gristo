#pragma once

// Small DSP building blocks shared by the drums, the synth and the effects.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gristo::sound::dsp {

constexpr float kPi = 3.14159265359f;
constexpr float kTwoPi = 6.28318530718f;

inline float clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }
inline float midiToHz(float note) { return 440.0f * std::exp2((note - 69.0f) / 12.0f); }

// Multiplier per sample that falls by 60 dB over `seconds`.
inline float decayCoef(float seconds, float sampleRate)
{
    return seconds <= 0.0f ? 0.0f : std::pow(0.001f, 1.0f / (seconds * sampleRate));
}

// Maps a 0..1 macro knob onto a log scale with 0.5 = `mid`.
inline float macroLog(float knob, float lo, float mid, float hi)
{
    knob = clamp01(knob);
    if (knob <= 0.5f)
        return std::exp(std::log(lo) + (std::log(mid) - std::log(lo)) * knob * 2.0f);
    return std::exp(std::log(mid) + (std::log(hi) - std::log(mid)) * (knob - 0.5f) * 2.0f);
}

// Saturation that stays at unity gain for small signals.
inline float softClip(float x) { return std::tanh(x); }
inline float drive(float x, float amount) // amount >= 1
{
    return amount <= 1.0f ? x : std::tanh(x * amount) / std::tanh(amount);
}

class Noise
{
public:
    explicit Noise(std::uint32_t seed = 0x9e3779b9u) : state_(seed ? seed : 1u) {}
    float next() // -1..1
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return static_cast<float>(state_) * (2.0f / 4294967295.0f) - 1.0f;
    }

private:
    std::uint32_t state_;
};

// Topology-preserving state variable filter (Zavalishin / Cytomic). Stable under
// fast modulation; resonance 0..~0.98.
class Svf
{
public:
    void reset() { ic1_ = ic2_ = 0.0f; }
    void set(float cutoffHz, float resonance, float sampleRate)
    {
        const float fc = std::clamp(cutoffHz, 10.0f, sampleRate * 0.45f);
        const float g = std::tan(kPi * fc / sampleRate);
        k_ = 2.0f - 2.0f * std::clamp(resonance, 0.0f, 0.98f);
        a1_ = 1.0f / (1.0f + g * (g + k_));
        a2_ = g * a1_;
        a3_ = g * a2_;
    }
    struct Out
    {
        float lp, bp, hp;
    };
    Out tick(float v0)
    {
        const float v3 = v0 - ic2_;
        const float v1 = a1_ * ic1_ + a2_ * v3;
        const float v2 = ic2_ + a2_ * ic1_ + a3_ * v3;
        ic1_ = 2.0f * v1 - ic1_;
        ic2_ = 2.0f * v2 - ic2_;
        if (std::abs(ic1_) < 1e-20f) ic1_ = 0.0f;
        if (std::abs(ic2_) < 1e-20f) ic2_ = 0.0f;
        return { v2, v1, v0 - k_ * v1 - v2 };
    }

private:
    float ic1_ = 0.0f, ic2_ = 0.0f;
    float k_ = 2.0f, a1_ = 1.0f, a2_ = 0.0f, a3_ = 0.0f;
};

class OnePole
{
public:
    void reset() { z_ = 0.0f; }
    void setLowpass(float cutoffHz, float sampleRate)
    {
        a_ = 1.0f - std::exp(-kTwoPi * std::clamp(cutoffHz, 5.0f, sampleRate * 0.49f) / sampleRate);
    }
    float lowpass(float x)
    {
        z_ += a_ * (x - z_);
        if (std::abs(z_) < 1e-20f) z_ = 0.0f;
        return z_;
    }
    float highpass(float x) { return x - lowpass(x); }

private:
    float a_ = 1.0f, z_ = 0.0f;
};

// PolyBLEP residual for band-limited saw / square.
inline float polyBlep(float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

inline float sawBlep(float phase, float dt) { return 2.0f * phase - 1.0f - polyBlep(phase, dt); }
inline float squareBlep(float phase, float dt, float width = 0.5f)
{
    float other = phase + (1.0f - width);
    other -= std::floor(other);
    return (phase < width ? 1.0f : -1.0f) + polyBlep(phase, dt) - polyBlep(other, dt);
}

// Linear-attack, exponential decay/release envelope. Times in seconds.
class Adsr
{
public:
    void set(float attack, float decay, float sustain, float release, float sampleRate)
    {
        attackStep_ = attack <= 0.0005f ? 1.0f : 1.0f / (attack * sampleRate);
        decayCoef_ = decayCoef(std::max(decay, 0.001f), sampleRate);
        sustain_ = clamp01(sustain);
        releaseCoef_ = decayCoef(std::max(release, 0.001f), sampleRate);
    }
    void setRelease(float release, float sampleRate) { releaseCoef_ = decayCoef(std::max(release, 0.001f), sampleRate); }
    void gateOn() { stage_ = Stage::Attack; }
    void gateOff()
    {
        if (stage_ != Stage::Idle)
            stage_ = Stage::Release;
    }
    void kill() { stage_ = Stage::Idle; value_ = 0.0f; }
    bool active() const { return stage_ != Stage::Idle; }
    bool gated() const { return stage_ == Stage::Attack || stage_ == Stage::Decay; }
    float value() const { return value_; }
    float tick()
    {
        switch (stage_)
        {
            case Stage::Attack:
                value_ += attackStep_;
                if (value_ >= 1.0f)
                {
                    value_ = 1.0f;
                    stage_ = Stage::Decay;
                }
                break;
            case Stage::Decay:
                value_ = sustain_ + (value_ - sustain_) * decayCoef_;
                break;
            case Stage::Release:
                value_ *= releaseCoef_;
                if (value_ < 1e-5f)
                {
                    value_ = 0.0f;
                    stage_ = Stage::Idle;
                }
                break;
            case Stage::Idle:
                break;
        }
        return value_;
    }

private:
    enum class Stage { Idle, Attack, Decay, Release };
    Stage stage_ = Stage::Idle;
    float value_ = 0.0f;
    float attackStep_ = 1.0f, decayCoef_ = 0.0f, sustain_ = 1.0f, releaseCoef_ = 0.0f;
};

} // namespace gristo::sound::dsp
