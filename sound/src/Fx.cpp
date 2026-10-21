#include "Fx.h"

namespace gristo::sound {

namespace {
constexpr int kCombTuning[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
constexpr int kAllpassTuning[4] = { 556, 441, 341, 225 };
constexpr int kStereoSpread = 23;
} // namespace

void Reverb::prepare(float sampleRate)
{
    const float scale = sampleRate / 44100.0f;
    preDelay_.assign(static_cast<size_t>(0.018f * sampleRate) + 1, 0.0f); // 18 ms
    lowCut_.setLowpass(180.0f, sampleRate);
    for (int side = 0; side < 2; ++side)
    {
        const int spread = side == 0 ? 0 : kStereoSpread;
        for (int i = 0; i < 8; ++i)
            combs_[side][i].buffer.assign(static_cast<size_t>(static_cast<float>(kCombTuning[i] + spread) * scale) + 1, 0.0f);
        for (int i = 0; i < 4; ++i)
            allpasses_[side][i].buffer.assign(static_cast<size_t>(static_cast<float>(kAllpassTuning[i] + spread) * scale) + 1, 0.0f);
    }
    reset();
}

void Reverb::reset()
{
    std::fill(preDelay_.begin(), preDelay_.end(), 0.0f);
    preIndex_ = 0;
    lowCut_.reset();
    for (auto& side : combs_)
        for (auto& c : side)
        {
            std::fill(c.buffer.begin(), c.buffer.end(), 0.0f);
            c.index = 0;
            c.store = 0.0f;
        }
    for (auto& side : allpasses_)
        for (auto& a : side)
        {
            std::fill(a.buffer.begin(), a.buffer.end(), 0.0f);
            a.index = 0;
        }
}

void Reverb::setSize(float size)
{
    feedback_ = 0.7f + 0.28f * dsp::clamp01(size);
    damp_ = 0.45f - 0.2f * dsp::clamp01(size);
}

void Reverb::process(float in, float& outL, float& outR)
{
    float input = lowCut_.highpass(in) * 0.015f;
    if (!preDelay_.empty())
    {
        const float delayed = preDelay_[preIndex_];
        preDelay_[preIndex_] = input;
        if (++preIndex_ >= preDelay_.size())
            preIndex_ = 0;
        input = delayed;
    }
    float out[2] = { 0.0f, 0.0f };
    for (int side = 0; side < 2; ++side)
    {
        for (auto& c : combs_[side])
        {
            if (c.buffer.empty())
                continue;
            const float y = c.buffer[c.index];
            c.store = y * (1.0f - damp_) + c.store * damp_;
            if (std::abs(c.store) < 1e-20f) c.store = 0.0f;
            c.buffer[c.index] = input + c.store * feedback_;
            if (++c.index >= c.buffer.size()) c.index = 0;
            out[side] += y;
        }
        for (auto& a : allpasses_[side])
        {
            if (a.buffer.empty())
                continue;
            const float b = a.buffer[a.index];
            const float y = -out[side] + b;
            a.buffer[a.index] = out[side] + b * 0.5f;
            if (++a.index >= a.buffer.size()) a.index = 0;
            out[side] = y;
        }
    }
    outL = out[0] * 3.0f;
    outR = out[1] * 3.0f;
}

// ---------------------------------------------------------------------------------

void Delay::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    const auto length = static_cast<size_t>(sampleRate * 3.0f) + 4; // 1/4 note at 20 bpm
    left_.assign(length, 0.0f);
    right_.assign(length, 0.0f);
    for (int i = 0; i < 2; ++i)
    {
        damp_[i].setLowpass(4500.0f, sampleRate);
        lowCut_[i].setLowpass(120.0f, sampleRate);
    }
    reset();
}

void Delay::reset()
{
    std::fill(left_.begin(), left_.end(), 0.0f);
    std::fill(right_.begin(), right_.end(), 0.0f);
    write_ = 0;
    for (int i = 0; i < 2; ++i)
    {
        damp_[i].reset();
        lowCut_[i].reset();
    }
    time_ = targetTime_;
}

void Delay::set(float seconds, float feedback)
{
    const float maxSamples = static_cast<float>(left_.size()) - 4.0f;
    targetTime_ = std::clamp(seconds * sampleRate_, 1.0f, std::max(1.0f, maxSamples));
    if (time_ <= 0.0f)
        time_ = targetTime_;
    feedback_ = std::clamp(feedback, 0.0f, 0.9f);
}

float Delay::read(const std::vector<float>& line, float delaySamples) const
{
    const auto size = static_cast<float>(line.size());
    float pos = static_cast<float>(write_) - delaySamples;
    while (pos < 0.0f)
        pos += size;
    const auto i0 = static_cast<size_t>(pos);
    const size_t i1 = (i0 + 1) % line.size();
    const float frac = pos - static_cast<float>(i0);
    return line[i0 % line.size()] * (1.0f - frac) + line[i1] * frac;
}

void Delay::process(float in, float& outL, float& outR)
{
    if (left_.empty())
    {
        outL = outR = 0.0f;
        return;
    }
    time_ += (targetTime_ - time_) * 0.0005f; // glide time changes instead of clicking
    const float l = read(left_, time_);
    const float r = read(right_, time_);
    // Ping-pong: the input enters on the left, each repeat crosses sides.
    const float toLeft = in + lowCut_[0].highpass(damp_[0].lowpass(r)) * feedback_;
    const float toRight = lowCut_[1].highpass(damp_[1].lowpass(l)) * feedback_;
    left_[write_] = toLeft;
    right_[write_] = toRight;
    if (++write_ >= left_.size())
        write_ = 0;
    outL = l;
    outR = r;
}

// ---------------------------------------------------------------------------------

void Chorus::prepare(float sampleRate, float lfoPhase)
{
    sampleRate_ = sampleRate;
    line_.assign(static_cast<size_t>(0.02f * sampleRate) + 4, 0.0f); // up to 20 ms
    startPhase_ = lfoPhase;
    reset();
}

void Chorus::reset()
{
    std::fill(line_.begin(), line_.end(), 0.0f);
    write_ = 0;
    phase_ = startPhase_;
}

float Chorus::read(float delaySamples) const
{
    const auto size = static_cast<float>(line_.size());
    float pos = static_cast<float>(write_) - delaySamples;
    if (pos < 0.0f)
        pos += size;
    const auto i0 = static_cast<size_t>(pos) % line_.size();
    const size_t i1 = (i0 + 1) % line_.size();
    const float frac = pos - std::floor(pos);
    return line_[i0] * (1.0f - frac) + line_[i1] * frac;
}

void Chorus::process(float in, float amount, float& outL, float& outR)
{
    if (line_.empty())
    {
        outL = outR = in;
        return;
    }
    line_[write_] = in;
    phase_ += 0.55f / sampleRate_; // ~0.55 Hz, like Juno mode I
    phase_ -= std::floor(phase_);
    const float tri = 4.0f * std::abs(phase_ - 0.5f) - 1.0f; // -1..1
    const float centre = 0.0065f * sampleRate_, depth = 0.0028f * sampleRate_;
    const float wetL = read(centre + depth * tri);
    const float wetR = read(centre - depth * tri);
    if (++write_ >= line_.size())
        write_ = 0;
    const float dry = 1.0f - 0.3f * amount, wet = 0.75f * amount;
    outL = in * dry + wetL * wet;
    outR = in * dry + wetR * wet;
}

// ---------------------------------------------------------------------------------

void Glue::prepare(float sampleRate)
{
    attack_ = 1.0f - std::exp(-1.0f / (0.004f * sampleRate));  // 4 ms
    release_ = 1.0f - std::exp(-1.0f / (0.15f * sampleRate));  // 150 ms
    dcL_.setLowpass(8.0f, sampleRate);
    dcR_.setLowpass(8.0f, sampleRate);
    reset();
}

void Glue::reset()
{
    env_ = 0.0f;
    gain_ = target_ = 1.0f;
    counter_ = 0;
    dcL_.reset();
    dcR_.reset();
}

void Glue::process(float& l, float& r)
{
    l = dcL_.highpass(l);
    r = dcR_.highpass(r);
    const float peak = std::max(std::abs(l), std::abs(r));
    env_ += (peak - env_) * (peak > env_ ? attack_ : release_);
    if (--counter_ <= 0) // the gain curve is computed every 8 samples and glided in between
    {
        constexpr float threshold = 0.6f, ratio = 3.0f;
        target_ = env_ > threshold ? std::pow(env_ / threshold, 1.0f / ratio - 1.0f) : 1.0f;
        counter_ = 8;
    }
    gain_ += (target_ - gain_) * 0.125f;
    l *= gain_;
    r *= gain_;
}

} // namespace gristo::sound
