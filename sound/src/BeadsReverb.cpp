// Reverb of Mutable Instruments Beads. Copyright 2017 Emilie Gillet (MIT licence);
// ported to floating point for Gristo.
#include "BeadsReverb.h"

#include <algorithm>
#include <cmath>

namespace gristo::sound {

namespace {
// ap1..ap4, dap1a, dap1b, del1, dap2a, dap2b, del2 (samples at 48 kHz)
constexpr int kLengths[10] = { 150, 214, 319, 527, 2182, 2690, 4501, 2525, 2197, 6312 };
} // namespace

void BeadsReverb::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    // Room for the longest layout (Clean tape: 1.5 times the module's 48 kHz delays).
    int total = 0;
    for (int length : kLengths)
        total += static_cast<int>(std::ceil(static_cast<float>(length) * 1.5f * sampleRate / 48000.0f)) + 1;
    int size = 1;
    while (size < total + 8 + static_cast<int>(150.0f * sampleRate / 48000.0f)) // room for the smear's reads below line 0
        size <<= 1;
    buffer_.assign(static_cast<size_t>(size), 0.0f);
    mask_ = size - 1;
    layout();
}

void BeadsReverb::layout()
{
    scale_ = sampleRate_ * stretch_ / 48000.0f;
    int base = 0;
    for (int i = 0; i < kLines; ++i)
    {
        length_[i] = std::max(2, static_cast<int>(std::lround(static_cast<float>(kLengths[i]) * scale_)));
        base_[i] = base;
        base += length_[i] + 1;
    }
    lfoInc_[0] = 0.5f / (sampleRate_ * stretch_);
    lfoInc_[1] = 0.3f / (sampleRate_ * stretch_);
    lpFor_ = -1.0f;
    // The 32 kHz converter keeps below ~14.5 kHz.
    band_ = stretch_ > 1.0f ? 1.0f - std::exp(-dsp::kTwoPi * std::min(14500.0f, 0.45f * sampleRate_) / sampleRate_) : 1.0f;
    reset();
}

void BeadsReverb::setMedium(int quality)
{
    const float stretch = quality == 2 ? 1.5f : 1.0f;
    if (stretch == stretch_)
        return;
    stretch_ = stretch;
    if (!buffer_.empty())
        layout();
}

void BeadsReverb::reset()
{
    std::fill(buffer_.begin(), buffer_.end(), 0.0f);
    writePtr_ = 0;
    lp1_ = lp2_ = 0.0f;
    lfoPhase_[0] = lfoPhase_[1] = 0.0f;
    lfo_[0] = lfo_[1] = 1.0f;
    lfoCount_ = 0;
    wetGain_ = send_ = 0.0f;
    dryGain_ = 1.0f;
    bandL_[0] = bandL_[1] = bandR_[0] = bandR_[1] = 0.0f;
}

float BeadsReverb::read(int line, float offset) const
{
    const int o = offset < 0.0f ? length_[line] - 1 : static_cast<int>(offset);
    return buffer_[static_cast<size_t>((writePtr_ + base_[line] + o) & mask_)];
}

float BeadsReverb::interpolate(int line, float offset) const
{
    // As on the module, an offset below the line's start reads the memory before it
    // (for the first line, unused memory at the end of the buffer: silence).
    offset = std::min(offset, static_cast<float>(length_[line] - 2));
    const int i = static_cast<int>(std::floor(offset));
    const float f = offset - static_cast<float>(i);
    const float a = buffer_[static_cast<size_t>((writePtr_ + base_[line] + i) & mask_)];
    const float b = buffer_[static_cast<size_t>((writePtr_ + base_[line] + i + 1) & mask_)];
    return a + (b - a) * f;
}

void BeadsReverb::write(int line, float value, int offset)
{
    // 16-bit memory, as on the module: it clips at full scale.
    value = std::clamp(value, -1.0f, 32767.0f / 32768.0f);
    buffer_[static_cast<size_t>((writePtr_ + base_[line] + offset) & mask_)] = std::abs(value) < 1e-20f ? 0.0f : value;
}

void BeadsReverb::process(float& l, float& r)
{
    if (buffer_.empty())
        return;
    if (--writePtr_ < 0)
        writePtr_ += mask_ + 1;
    if ((lfoCount_++ & 31) == 0) // the LFOs move every 32 samples, as on the module
        for (int i = 0; i < 2; ++i)
        {
            lfoPhase_[i] += lfoInc_[i] * 32.0f;
            lfoPhase_[i] -= std::floor(lfoPhase_[i]);
            lfo_[i] = 0.5f + 0.5f * std::cos(dsp::kTwoPi * lfoPhase_[i]); // 0..1, as stmlib's CosineOscillator
        }

    constexpr float kap = 0.625f;
    if (lp_ != lpFor_) // the module's damping per sample at its own rate, per sample at ours
    {
        lpFor_ = lp_;
        const float moduleRate = 48000.0f / stretch_;
        klp_ = 1.0f - std::pow(1.0f - std::clamp(lp_, 0.0f, 0.999f), moduleRate / sampleRate_);
    }
    const float krt = time_, klp = klp_;
    const float inputGain = 0.2f * (1.0f - 0.333f * krt);
    const float outputGain = 0.4f / inputGain;
    const float sendSqrt = mix_ * (2.0f - mix_);
    const float sendAmount = sendSqrt * (2.0f - sendSqrt);
    const float dry = (1.0f - mix_) * (1.0f + mix_);
    // Smooth the gains (the module interpolates them over each block).
    constexpr float glide = 0.002f;
    wetGain_ += (outputGain - wetGain_) * glide;
    dryGain_ += (dry - dryGain_) * glide;
    send_ += (sendAmount * inputGain - send_) * glide;

    // Smear ap1 inside its loop.
    float acc = interpolate(0, (10.0f + 80.0f * lfo_[0]) * scale_);
    write(0, acc, std::min(length_[0] - 1, static_cast<int>(std::lround(100.0f * scale_))));
    acc = 0.0f;
    acc += (l + r) * send_;
    // Four diffusers.
    for (int line = 0; line < 4; ++line)
    {
        const float tail = readTail(line);
        acc += tail * kap;
        write(line, acc);
        acc = acc * -kap + tail;
    }
    const float apout = acc;

    // Branch 1
    acc = apout + interpolate(9, (6261.0f + 50.0f * lfo_[1]) * scale_) * krt;
    lp1_ += klp * (acc - lp1_);
    acc = lp1_;
    {
        const float tail = readTail(4);
        acc += tail * -kap;
        write(4, acc);
        acc = acc * kap + tail;
    }
    {
        const float tail = readTail(5);
        acc += tail * kap;
        write(5, acc);
        acc = acc * -kap + tail;
    }
    write(6, acc);
    const float wetL = acc;

    // Branch 2
    acc = apout + interpolate(6, (4460.0f + 40.0f * lfo_[0]) * scale_) * krt;
    lp2_ += klp * (acc - lp2_);
    acc = lp2_;
    {
        const float tail = readTail(7);
        acc += tail * kap;
        write(7, acc);
        acc = acc * -kap + tail;
    }
    {
        const float tail = readTail(8);
        acc += tail * -kap;
        write(8, acc);
        acc = acc * kap + tail;
    }
    write(9, acc);
    const float wetR = acc;

    float outL = wetL, outR = wetR;
    if (band_ < 1.0f)
    {
        bandL_[0] += band_ * (outL - bandL_[0]);
        bandL_[1] += band_ * (bandL_[0] - bandL_[1]);
        bandR_[0] += band_ * (outR - bandR_[0]);
        bandR_[1] += band_ * (bandR_[0] - bandR_[1]);
        outL = bandL_[1];
        outR = bandR_[1];
    }
    l = l * dryGain_ + outL * wetGain_;
    r = r * dryGain_ + outR * wetGain_;
}

} // namespace gristo::sound
