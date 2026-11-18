#pragma once

// The synth's building blocks: the oscillators, the analog filter models, the
// envelopes and the 2x oversampling. The voices run at twice the host's rate (so the
// filters' saturation, hard sync, FM and phase distortion fold far less back down as
// grit), and the synth decimates their sum once.

#include "Dsp.h"

#include <array>
#include <cmath>

namespace gristo::sound::dsp {

// ---------------------------------------------------------------------------------
// The oscillator of Mutable Instruments Plaits' virtual analog engine
// (plaits/dsp/oscillator/variable_shape_oscillator.h, Copyright 2016 Emilie Gillet,
// MIT licence), one sample at a time: a continuous morph from triangle (0) through saw
// (0.5) to square / pulse (1), with band-limited steps (polyBLEP and integrated BLEP
// for the triangle's corners) and a hard sync whose reset is band-limited too. Below
// 0 the triangle fades into a sine (Gristo's addition).

inline float thisBlep(float t) { return 0.5f * t * t; }
inline float nextBlep(float t)
{
    t = 1.0f - t;
    return -0.5f * t * t;
}
inline float nextIntegratedBlep(float t)
{
    const float t1 = 0.5f * t, t2 = t1 * t1, t4 = t2 * t2;
    return 0.1875f - t1 + 1.5f * t2 - t4;
}
inline float thisIntegratedBlep(float t) { return nextIntegratedBlep(1.0f - t); }

class ShapeOsc
{
public:
    void reset(float phase = 0.0f)
    {
        master_ = 0.0f;
        slave_ = phase;
        next_ = 0.0f;
        previousPw_ = 0.5f;
        high_ = phase >= 0.5f;
    }
    // frequency: cycles per sample (< 0.5); shape: -1 sine .. 0 triangle .. 0.5 saw ..
    // 1 square (pw its width); master > 0: hard-synced to a master at that frequency.
    float tick(float frequency, float pw, float shape, float master = 0.0f)
    {
        frequency = std::min(frequency, 0.25f);
        const float sine = shape < 0.0f ? std::min(1.0f, -shape) : 0.0f;
        const float waveshape = std::max(shape, 0.0f);
        pw = frequency >= 0.25f ? 0.5f : std::clamp(pw, frequency * 2.0f, 1.0f - 2.0f * frequency);
        const float squareAmount = std::max(waveshape - 0.5f, 0.0f) * 2.0f;
        const float triangleAmount = std::max(1.0f - waveshape * 2.0f, 0.0f);
        const float slopeUp = 1.0f / pw, slopeDown = 1.0f / (1.0f - pw);

        float thisSample = next_;
        float nextSample = 0.0f;
        bool reset = false, transitionDuringReset = false;
        float resetTime = 0.0f;
        if (master > 0.0f)
        {
            master = std::min(master, 0.25f);
            master_ += master;
            if (master_ >= 1.0f)
            {
                master_ -= 1.0f;
                resetTime = master_ / master;
                float atReset = slave_ + (1.0f - resetTime) * frequency;
                reset = true;
                if (atReset >= 1.0f)
                {
                    atReset -= 1.0f;
                    transitionDuringReset = true;
                }
                if (!high_ && atReset >= pw)
                    transitionDuringReset = true;
                const float value = naive(atReset, pw, slopeUp, slopeDown, triangleAmount, squareAmount);
                thisSample -= value * thisBlep(resetTime);
                nextSample -= value * nextBlep(resetTime);
            }
        }
        slave_ += frequency;
        while (transitionDuringReset || !reset)
        {
            if (!high_)
            {
                if (slave_ < pw)
                    break;
                const float t = (slave_ - pw) / (previousPw_ - pw + frequency);
                const float triangleStep = (slopeUp + slopeDown) * frequency * triangleAmount;
                thisSample += squareAmount * thisBlep(t);
                nextSample += squareAmount * nextBlep(t);
                thisSample -= triangleStep * thisIntegratedBlep(t);
                nextSample -= triangleStep * nextIntegratedBlep(t);
                high_ = true;
            }
            if (high_)
            {
                if (slave_ < 1.0f)
                    break;
                slave_ -= 1.0f;
                const float t = slave_ / frequency;
                const float triangleStep = (slopeUp + slopeDown) * frequency * triangleAmount;
                thisSample -= (1.0f - triangleAmount) * thisBlep(t);
                nextSample -= (1.0f - triangleAmount) * nextBlep(t);
                thisSample += triangleStep * thisIntegratedBlep(t);
                nextSample += triangleStep * nextIntegratedBlep(t);
                high_ = false;
            }
        }
        if (reset)
        {
            slave_ = resetTime * frequency;
            high_ = false;
        }
        nextSample += naive(slave_, pw, slopeUp, slopeDown, triangleAmount, squareAmount);
        previousPw_ = pw;
        next_ = nextSample;
        float out = 2.0f * thisSample - 1.0f;
        if (sine > 0.0f) // towards a sine: the triangle's own phase, through a sine
            out += (std::sin(kTwoPi * (slave_ - 0.25f)) - out) * sine;
        return out;
    }
    float phase() const { return slave_; }

private:
    static float naive(float phase, float pw, float slopeUp, float slopeDown, float triangleAmount, float squareAmount)
    {
        float saw = phase;
        const float square = phase < pw ? 0.0f : 1.0f;
        const float triangle = phase < pw ? phase * slopeUp : 1.0f - (phase - pw) * slopeDown;
        saw += (square - saw) * squareAmount;
        saw += (triangle - saw) * triangleAmount;
        return saw;
    }
    float master_ = 0.0f, slave_ = 0.0f, next_ = 0.0f, previousPw_ = 0.5f;
    bool high_ = false;
};

// A sine by table (fast, for FM operators): phase in cycles.
class SineTable
{
public:
    static float at(float phase)
    {
        static const std::array<float, kSize + 1> table = [] {
            std::array<float, kSize + 1> t {};
            for (int i = 0; i <= kSize; ++i)
                t[static_cast<size_t>(i)] = std::sin(kTwoPi * static_cast<float>(i) / kSize);
            return t;
        }();
        phase -= std::floor(phase);
        const float x = phase * kSize;
        const int i = static_cast<int>(x);
        const float f = x - static_cast<float>(i);
        return table[static_cast<size_t>(i)] + (table[static_cast<size_t>(i + 1)] - table[static_cast<size_t>(i)]) * f;
    }

private:
    static constexpr int kSize = 4096;
};

// tanh(x) / x: the transconductance of a saturating stage, for the "cheap non-linear
// zero-delay filter" method (evaluated from the previous sample, the filter is then
// solved exactly as a linear one: tuning and resonance stay right as it saturates).
inline float tanhRatio(float x)
{
    const float a = std::abs(x);
    return a < 1e-3f ? 1.0f : fastTanh(a) / a;
}

// ---------------------------------------------------------------------------------
// The analog filter models. All take g = tan(pi fc / fs) (filterG) and a resonance 0..1.

// Transistor ladder (the Moog family): four saturating stages and a saturating input
// with the resonance feedback around them; part of the bass lost to resonance is
// given back, as most ladder synths do. Fat, warm, self-oscillates at the top.
class LadderFilter
{
public:
    void reset() { s_.fill(0.0f), y_.fill(0.0f), u_ = 0.0f; }
    float tick(float x, float g, float resonance)
    {
        const float k = 4.2f * resonance;
        // The stages' gains from the last sample's levels.
        const float tIn = tanhRatio(u_);
        float alpha[4], beta[4];
        float A = 1.0f, B = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            const float a = i == 0 ? tIn : tanhRatio(y_[static_cast<size_t>(i - 1)]);
            const float b = tanhRatio(y_[static_cast<size_t>(i)]);
            const float d = 1.0f / (1.0f + g * b);
            alpha[i] = g * a * d;
            beta[i] = s_[static_cast<size_t>(i)] * d;
            B = alpha[i] * B + beta[i];
            A *= alpha[i];
        }
        const float in = x * (1.0f + 0.45f * k);
        const float y4 = (A * in + B) / (1.0f + k * A);
        u_ = in - k * y4;
        float v = u_;
        for (int i = 0; i < 4; ++i)
        {
            const float y = alpha[i] * v + beta[i];
            s_[static_cast<size_t>(i)] = flush(2.0f * y - s_[static_cast<size_t>(i)]);
            y_[static_cast<size_t>(i)] = y;
            v = y;
        }
        return v;
    }

private:
    static float flush(float v) { return std::abs(v) < 1e-20f ? 0.0f : v; }
    std::array<float, 4> s_ {}, y_ {};
    float u_ = 0.0f;
};

// Four OTA stages (the integrated filters of the classic polysynths: Juno, Jupiter,
// Prophet): each stage saturates on the difference it is fed, not its input, which
// keeps them rounder and less buzzy than the ladder; resonance is gentler at the bottom.
class OtaFilter
{
public:
    void reset() { s_.fill(0.0f), y_.fill(0.0f), u_ = 0.0f; }
    float tick(float x, float g, float resonance)
    {
        const float k = 4.3f * resonance; // a little past 4: it rings on at the top, as the chips do
        float alpha[4], beta[4];
        float A = 1.0f, B = 0.0f;
        float previous = u_;
        for (int i = 0; i < 4; ++i)
        {
            // d y = g * tanh(in - y): linearised as t * (in - y).
            const float t = tanhRatio(previous - y_[static_cast<size_t>(i)]);
            const float d = 1.0f / (1.0f + g * t);
            alpha[i] = g * t * d;
            beta[i] = s_[static_cast<size_t>(i)] * d;
            B = alpha[i] * B + beta[i];
            A *= alpha[i];
            previous = y_[static_cast<size_t>(i)];
        }
        const float in = x * (1.0f + 0.3f * k);
        const float y4 = (A * in + B) / (1.0f + k * A);
        u_ = fastTanh(in - k * y4);
        float v = u_;
        for (int i = 0; i < 4; ++i)
        {
            const float y = alpha[i] * v + beta[i];
            s_[static_cast<size_t>(i)] = std::abs(2.0f * y - s_[static_cast<size_t>(i)]) < 1e-20f ? 0.0f : 2.0f * y - s_[static_cast<size_t>(i)];
            y_[static_cast<size_t>(i)] = y;
            v = y;
        }
        return v;
    }

private:
    std::array<float, 4> s_ {}, y_ {};
    float u_ = 0.0f;
};

// A 12 dB state variable filter with a saturating band-pass integrator (the SEM
// family): low-pass, band-pass, high-pass or notch; smooth and vocal when resonant.
class SemFilter
{
public:
    void reset() { ic1_ = ic2_ = 0.0f; }
    float tick(float x, float g, float resonance, int mode)
    {
        const float k = 2.0f - 1.96f * resonance;
        const float t = tanhRatio(ic1_ * 0.6f); // the band-pass saturates: resonance self-limits
        const float gt = g * t;
        const float a1 = 1.0f / (1.0f + gt * (gt + k));
        const float a2 = gt * a1, a3 = gt * a2;
        const float v3 = x - ic2_;
        const float v1 = a1 * ic1_ + a2 * v3;
        const float v2 = ic2_ + a2 * ic1_ + a3 * v3;
        ic1_ = 2.0f * v1 - ic1_;
        ic2_ = 2.0f * v2 - ic2_;
        if (std::abs(ic1_) < 1e-20f) ic1_ = 0.0f;
        if (std::abs(ic2_) < 1e-20f) ic2_ = 0.0f;
        switch (mode)
        {
            case 1: return v1 * k;               // band-pass, the peak at unity
            case 2: return x - k * v1 - v2;      // high-pass
            case 3: return x - k * v1;           // notch
            default: return v2;                  // low-pass
        }
    }

private:
    float ic1_ = 0.0f, ic2_ = 0.0f;
};

// ---------------------------------------------------------------------------------

// An analog-style envelope: the attack charges towards 1.3 (as an RC towards a level
// above the comparator's), so it rises fast and rounds off into the top; decay and
// release fall exponentially. Times in seconds.
class AnalogEnv
{
public:
    void set(float attack, float decay, float sustain, float release, float sampleRate)
    {
        // Reaching 1 from 0 aiming at 1.3 takes ln(1.3 / 0.3) = 1.466 time constants.
        attackCoef_ = attack <= 0.0005f ? 1.0f : 1.0f - std::exp(-1.466f / (attack * sampleRate));
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
    void kill() { stage_ = Stage::Idle, value_ = 0.0f; }
    bool active() const { return stage_ != Stage::Idle; }
    bool gated() const { return stage_ == Stage::Attack || stage_ == Stage::Decay; }
    float value() const { return value_; }
    float tick()
    {
        switch (stage_)
        {
            case Stage::Attack:
                value_ += (1.3f - value_) * attackCoef_;
                if (value_ >= 1.0f)
                {
                    value_ = 1.0f;
                    stage_ = Stage::Decay;
                }
                break;
            case Stage::Decay: value_ = sustain_ + (value_ - sustain_) * decayCoef_; break;
            case Stage::Release:
                value_ *= releaseCoef_;
                if (value_ < 1e-5f)
                {
                    value_ = 0.0f;
                    stage_ = Stage::Idle;
                }
                break;
            case Stage::Idle: break;
        }
        return value_;
    }

private:
    enum class Stage { Idle, Attack, Decay, Release };
    Stage stage_ = Stage::Idle;
    float value_ = 0.0f, attackCoef_ = 1.0f, decayCoef_ = 0.0f, sustain_ = 1.0f, releaseCoef_ = 0.0f;
};

// Attack, decay, end: the envelope of an FM operator group (A or B). It rises to 1,
// falls to its end level and stays there while the key is held; after the key, it
// falls on with the voice.
class AdeEnv
{
public:
    void set(float attack, float decay, float end, float sampleRate)
    {
        attackStep_ = attack <= 0.0005f ? 1.0f : 1.0f / (attack * sampleRate);
        decayCoef_ = decayCoef(std::max(decay, 0.001f), sampleRate);
        end_ = clamp01(end);
    }
    void start()
    {
        value_ = attackStep_ >= 1.0f ? 1.0f : 0.0f;
        rising_ = attackStep_ < 1.0f;
    }
    float tick()
    {
        if (rising_)
        {
            value_ += attackStep_;
            if (value_ >= 1.0f)
            {
                value_ = 1.0f;
                rising_ = false;
            }
        }
        else
            value_ = end_ + (value_ - end_) * decayCoef_;
        return value_;
    }

private:
    float value_ = 0.0f, attackStep_ = 1.0f, decayCoef_ = 0.0f, end_ = 0.0f;
    bool rising_ = false;
};

// ---------------------------------------------------------------------------------

// Half-band decimator, 2x down: a 47-tap linear-phase FIR (Kaiser window), flat to
// ~20 kHz at a 48 kHz output, with everything folding back below 20 kHz at least
// ~85 dB down. Push two samples, get one.
class Decimator2
{
public:
    Decimator2()
    {
        static const std::array<float, kTaps> taps = [] {
            std::array<float, kTaps> h {};
            const int mid = kTaps / 2;
            const double beta = 8.6;
            auto bessel = [](double x) {
                double sum = 1.0, term = 1.0;
                for (int k = 1; k < 30; ++k)
                {
                    term *= (x / (2.0 * k)) * (x / (2.0 * k));
                    sum += term;
                }
                return sum;
            };
            double total = 0.0;
            for (int n = 0; n < kTaps; ++n)
            {
                const int m = n - mid;
                const double sinc = m == 0 ? 0.5 : std::sin(3.14159265358979 * m / 2.0) / (3.14159265358979 * m);
                const double r = static_cast<double>(m) / mid;
                const double w = bessel(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / bessel(beta);
                h[static_cast<size_t>(n)] = static_cast<float>(sinc * w);
                total += h[static_cast<size_t>(n)];
            }
            for (auto& v : h)
                v = static_cast<float>(v / total);
            return h;
        }();
        taps_ = &taps;
        reset();
    }
    void reset()
    {
        history_.fill(0.0f);
        write_ = 0;
    }
    float process(float a, float b)
    {
        push(a);
        push(b);
        // The newest sample is at write_ - 1; the half-band's odd taps (but the centre) are zero.
        const auto& h = *taps_;
        float sum = 0.0f;
        for (int n = 0; n < kTaps; n += 2)
            sum += h[static_cast<size_t>(n)] * history_[static_cast<size_t>((write_ - 1 - n) & kMask)];
        sum += h[kTaps / 2] * history_[static_cast<size_t>((write_ - 1 - kTaps / 2) & kMask)];
        return sum;
    }

private:
    static constexpr int kTaps = 47, kMask = 63;
    void push(float x)
    {
        history_[static_cast<size_t>(write_ & kMask)] = x;
        ++write_;
    }
    const std::array<float, kTaps>* taps_ = nullptr;
    std::array<float, kMask + 1> history_ {};
    int write_ = 0;
};

} // namespace gristo::sound::dsp
