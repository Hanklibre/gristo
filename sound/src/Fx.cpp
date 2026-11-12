#include "Fx.h"

namespace gristo::sound {

namespace {
// Dattorro's lengths, in samples at his 29761 Hz.
constexpr float kPlateRate = 29761.0f;
constexpr int kDiffuser[4] = { 142, 107, 379, 277 };
constexpr float kDiffuserGain[4] = { 0.75f, 0.75f, 0.625f, 0.625f };
constexpr int kTankAp1[2] = { 672, 908 }, kTankDelay1[2] = { 4453, 4217 };
constexpr int kTankAp2[2] = { 1800, 2656 }, kTankDelay2[2] = { 3720, 3163 };
constexpr float kDecayDiffusion1 = 0.7f;
constexpr float kExcursion = 16.0f;
// Output taps (his table 2). Left: from the right tank mostly, right: from the left.
constexpr int kTapL[7] = { 266, 2974, 1913, 1996, 1990, 187, 1066 };
constexpr int kTapR[7] = { 353, 3627, 1228, 2673, 2111, 335, 121 };
constexpr float kOutputGain = 0.6f;
constexpr float kLevel = 1.1f; // matches the level of the previous reverb at middle size
} // namespace

void Reverb::Line::init(int maxDelay)
{
    std::uint32_t n = 1;
    while (n < static_cast<std::uint32_t>(maxDelay) + 4u)
        n <<= 1;
    buffer.assign(n, 0.0f);
    mask = n - 1;
    write = 0;
}

void Reverb::Line::clear()
{
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    write = 0;
}

float Reverb::Line::atFrac(float delay) const
{
    const int i = static_cast<int>(delay);
    const float f = delay - static_cast<float>(i);
    const float a = at(i), b = at(i + 1);
    return a + (b - a) * f;
}

float Reverb::allpass(Line& line, int delay, float gain, float x)
{
    const float z = line.at(delay);
    const float w = x + gain * z;
    line.push(w);
    return z - gain * w;
}

float Reverb::allpassMod(Line& line, float delay, float gain, float x)
{
    const float z = line.atFrac(delay);
    const float w = x + gain * z;
    line.push(w);
    return z - gain * w;
}

void Reverb::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    const float scale = sampleRate / kPlateRate;
    auto len = [scale](int n) { return std::max(1, static_cast<int>(std::lround(static_cast<float>(n) * scale))); };
    preDelayLength_ = std::max(1, static_cast<int>(0.018f * sampleRate)); // 18 ms
    preDelay_.init(preDelayLength_);
    lowCut_.setLowpass(180.0f, sampleRate);
    bandwidth_.setLowpass(10000.0f, sampleRate);
    for (int i = 0; i < 4; ++i)
    {
        diffuserLength_[i] = len(kDiffuser[i]);
        diffusers_[i].init(diffuserLength_[i]);
    }
    excursion_ = kExcursion * scale;
    for (int side = 0; side < 2; ++side)
    {
        ap1Length_[side] = len(kTankAp1[side]);
        delay1Length_[side] = len(kTankDelay1[side]);
        ap2Length_[side] = len(kTankAp2[side]);
        delay2Length_[side] = len(kTankDelay2[side]);
        tankAp1_[side].init(ap1Length_[side] + static_cast<int>(excursion_) + 2);
        tankDelay1_[side].init(delay1Length_[side]);
        tankAp2_[side].init(ap2Length_[side]);
        tankDelay2_[side].init(delay2Length_[side]);
    }
    for (int i = 0; i < 7; ++i)
    {
        tapsL_[i] = len(kTapL[i]);
        tapsR_[i] = len(kTapR[i]);
    }
    lfoInc_[0] = 0.6f / sampleRate; // slow chorus of the tank, at two unrelated rates
    lfoInc_[1] = 0.77f / sampleRate;
    setSize(0.5f);
    reset();
}

void Reverb::reset()
{
    preDelay_.clear();
    lowCut_.reset();
    bandwidth_.reset();
    for (auto& d : diffusers_)
        d.clear();
    for (int side = 0; side < 2; ++side)
    {
        tankAp1_[side].clear();
        tankDelay1_[side].clear();
        tankAp2_[side].clear();
        tankDelay2_[side].clear();
        damping_[side].reset();
        tankOut_[side] = 0.0f;
    }
    lfoPhase_[0] = 0.0f;
    lfoPhase_[1] = 0.25f;
}

void Reverb::setSize(float size)
{
    size = dsp::clamp01(size);
    // Fitted to the decay times of the earlier reverb, so saved settings keep their
    // length: RT60 about 0.5 s at 0, 0.9 s at 0.5, 1.8 s at 0.75, 8.7 s at 1.
    // Longer tails a little darker.
    decay_ = 0.885f * std::pow(size, 1.65f);
    decayDiffusion2_ = std::clamp(decay_ + 0.15f, 0.25f, 0.5f);
    const float dampHz = 9000.0f - 3500.0f * size;
    if (std::abs(dampHz - dampHz_) > 1.0f)
    {
        dampHz_ = dampHz;
        for (auto& d : damping_)
            d.setLowpass(dampHz_, sampleRate_);
    }
}

void Reverb::process(float in, float& outL, float& outR)
{
    if (preDelay_.buffer.empty())
    {
        outL = outR = 0.0f;
        return;
    }
    // Input: low cut, pre-delay, bandwidth, four diffusers.
    float x = preDelay_.at(preDelayLength_);
    preDelay_.push(lowCut_.highpass(in));
    x = bandwidth_.lowpass(x);
    for (int i = 0; i < 4; ++i)
        x = allpass(diffusers_[i], diffuserLength_[i], kDiffuserGain[i], x);

    // Tank: each side is fed by the input and the other side's end.
    const float feed[2] = { x + decay_ * tankOut_[1], x + decay_ * tankOut_[0] };
    for (int side = 0; side < 2; ++side)
    {
        lfoPhase_[side] += lfoInc_[side];
        if (lfoPhase_[side] >= 1.0f)
            lfoPhase_[side] -= 1.0f;
        const float mod = excursion_ * (0.5f + 0.5f * std::sin(dsp::kTwoPi * lfoPhase_[side]));
        float v = allpassMod(tankAp1_[side], static_cast<float>(ap1Length_[side]) + mod, -kDecayDiffusion1, feed[side]);
        const float d1 = tankDelay1_[side].at(delay1Length_[side]);
        tankDelay1_[side].push(v);
        v = damping_[side].lowpass(d1) * decay_;
        v = allpass(tankAp2_[side], ap2Length_[side], decayDiffusion2_, v);
        float end = tankDelay2_[side].at(delay2Length_[side]);
        tankDelay2_[side].push(v);
        if (std::abs(end) < 1e-20f)
            end = 0.0f;
        tankOut_[side] = end;
    }

    // Stereo taps around the tank.
    const float l = tankDelay1_[1].at(tapsL_[0]) + tankDelay1_[1].at(tapsL_[1]) - tankAp2_[1].at(tapsL_[2])
                  + tankDelay2_[1].at(tapsL_[3]) - tankDelay1_[0].at(tapsL_[4]) - tankAp2_[0].at(tapsL_[5])
                  - tankDelay2_[0].at(tapsL_[6]);
    const float r = tankDelay1_[0].at(tapsR_[0]) + tankDelay1_[0].at(tapsR_[1]) - tankAp2_[0].at(tapsR_[2])
                  + tankDelay2_[0].at(tapsR_[3]) - tankDelay1_[1].at(tapsR_[4]) - tankAp2_[1].at(tapsR_[5])
                  - tankDelay2_[1].at(tapsR_[6]);
    outL = l * kOutputGain * kLevel;
    outR = r * kOutputGain * kLevel;
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
    glideCoef_ = 1.0f - std::exp(-1.0f / (0.04f * sampleRate)); // time changes glide over ~40 ms
    wowInc_ = 0.45f / sampleRate;
    wowDepth_ = 0.0004f * sampleRate; // +-0.4 ms
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
    time_ += (targetTime_ - time_) * glideCoef_; // glide time changes instead of clicking
    wowPhase_ += wowInc_;
    wowPhase_ -= std::floor(wowPhase_);
    const float wow = wowDepth_ * std::sin(dsp::kTwoPi * wowPhase_);
    const float l = read(left_, std::max(1.0f, time_ + wow));
    const float r = read(right_, std::max(1.0f, time_ - wow));
    // Ping-pong: the input enters on the left, each repeat crosses sides. The repeats
    // saturate gently, like tape.
    auto tape = [](float x) { return dsp::fastTanh(x * 1.5f) * (1.0f / 1.5f); };
    const float toLeft = in + tape(lowCut_[0].highpass(damp_[0].lowpass(r)) * feedback_);
    const float toRight = tape(lowCut_[1].highpass(damp_[1].lowpass(l)) * feedback_);
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

// ---------------------------------------------------------------------------------

void Wobble::prepare(float sampleRate, float hz, std::uint32_t seed)
{
    noise_ = dsp::Noise(seed);
    period_ = std::max(16, static_cast<int>(sampleRate / std::max(hz, 0.01f)));
    coef_ = 1.0f - std::exp(-3.0f / static_cast<float>(period_));
    counter_ = 0;
    value_ = target_ = 0.0f;
}

void Crusher::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    reset();
    set(0.0f);
}

void Crusher::reset()
{
    phase_ = 1.0f;
    heldL_ = heldR_ = 0.0f;
    for (auto& channel : smooth_)
        for (auto& f : channel)
            f.reset();
}

void Crusher::set(float amount)
{
    const float a = dsp::clamp01(amount);
    wet_ = std::min(1.0f, a * 4.0f);
    // 28 kHz (an SP-1200 runs at 26) down to 8 kHz; 12 bits down to 7.
    const float rate = 28000.0f * std::pow(8000.0f / 28000.0f, a);
    step_ = std::min(1.0f, rate / sampleRate_);
    levels_ = std::exp2(11.0f - 5.0f * a);
    for (auto& channel : smooth_)
        for (auto& f : channel)
            f.setLowpass(std::min(rate * 0.6f, 16000.0f), sampleRate_);
}

void Crusher::process(float& l, float& r)
{
    phase_ += step_;
    if (phase_ >= 1.0f)
    {
        phase_ -= 1.0f;
        heldL_ = std::round(l * levels_) / levels_;
        heldR_ = std::round(r * levels_) / levels_;
    }
    const float cl = smooth_[0][1].lowpass(smooth_[0][0].lowpass(heldL_));
    const float cr = smooth_[1][1].lowpass(smooth_[1][0].lowpass(heldR_));
    l += (cl - l) * wet_;
    r += (cr - r) * wet_;
}

void Overdrive::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    reset();
    set(0.0f);
}

void Overdrive::reset()
{
    for (auto& f : tone_)
        f.reset();
}

void Overdrive::set(float amount)
{
    const float a = dsp::clamp01(amount);
    mix_ = std::min(1.0f, a * 4.0f);
    gain_ = 1.0f + 2.0f * a + 9.0f * a * a;
    bias_ = 0.18f * a;
    offset_ = dsp::fastTanh(bias_);
    // Quiet material comes up, loud material is held: about the same loudness all the way.
    makeup_ = std::pow(gain_, -0.45f);
    for (auto& f : tone_)
        f.setLowpass(17000.0f - 9000.0f * a, sampleRate_);
}

void Overdrive::process(float& l, float& r)
{
    const float dl = tone_[0].lowpass((dsp::fastTanh(l * gain_ + bias_) - offset_) * makeup_);
    const float dr = tone_[1].lowpass((dsp::fastTanh(r * gain_ + bias_) - offset_) * makeup_);
    l += (dl - l) * mix_;
    r += (dr - r) * mix_;
}

void Tape::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    const auto size = static_cast<size_t>(sampleRate * 0.02f) + 16;
    for (auto& line : line_)
        line.assign(size, 0.0f);
    baseDelay_ = 0.003f * sampleRate;
    drift_.prepare(sampleRate, 0.7f, 0x51a7e5u);
    level_.prepare(sampleRate, 3.0f, 0x1e7e1u);
    hissRelease_ = std::exp(-1.0f / (1.2f * sampleRate));
    hissTone_.setLowpass(2500.0f, sampleRate);
    reset();
    set(amount_);
}

void Tape::reset()
{
    for (auto& line : line_)
        std::fill(line.begin(), line.end(), 0.0f);
    write_ = 0;
    hissEnv_ = 0.0f;
    for (auto* f : { &highs_[0], &highs_[1], &bump_[0], &bump_[1], &hissTone_ })
        f->reset();
}

void Tape::set(float amount)
{
    const float a = dsp::clamp01(amount);
    amount_ = a;
    mix_ = std::min(1.0f, a * 3.0f);
    gain_ = 1.0f + 3.5f * a;
    makeup_ = std::pow(gain_, 0.45f) / gain_;
    wowDepth_ = a * a * 0.0011f * sampleRate_;  // up to about 8 cents at 0.6 Hz
    flutterDepth_ = a * 0.00004f * sampleRate_; // a faster, smaller shimmer on top
    hissLevel_ = a * a * 0.006f;
    for (int ch = 0; ch < 2; ++ch)
    {
        highs_[ch].setLowpass(20000.0f * std::pow(5500.0f / 20000.0f, a), sampleRate_);
        bump_[ch].setLowpass(110.0f, sampleRate_);
    }
}

float Tape::read(const std::vector<float>& line, float delaySamples) const
{
    const float pos = static_cast<float>(write_) - delaySamples + static_cast<float>(line.size());
    const auto i0 = static_cast<size_t>(pos);
    const float frac = pos - static_cast<float>(i0);
    const float a = line[i0 % line.size()], b = line[(i0 + 1) % line.size()];
    return a + (b - a) * frac;
}

void Tape::process(float& l, float& r)
{
    line_[0][write_] = l;
    line_[1][write_] = r;
    // The transport is never quite steady: slow wow, fast flutter, and a random drift.
    wowPhase_ += 0.6f / sampleRate_;
    wowPhase_ -= std::floor(wowPhase_);
    flutterPhase_ += 7.3f / sampleRate_;
    flutterPhase_ -= std::floor(flutterPhase_);
    const float wander = drift_.tick(), level = level_.tick();
    const float delay = baseDelay_ + wowDepth_ * (std::sin(dsp::kTwoPi * wowPhase_) + 0.6f * wander)
                        + flutterDepth_ * std::sin(dsp::kTwoPi * flutterPhase_);
    float tl = read(line_[0], delay), tr = read(line_[1], delay);
    write_ = (write_ + 1) % line_[0].size();

    const float peak = std::max(std::abs(tl), std::abs(tr));
    hissEnv_ = peak > hissEnv_ ? peak : hissEnv_ * hissRelease_;
    if (mix_ <= 0.0f)
    {
        l = tl;
        r = tr;
        return;
    }
    const float sl = dsp::fastTanh(tl * gain_) * makeup_, sr = dsp::fastTanh(tr * gain_) * makeup_;
    tl += (sl - tl) * mix_;
    tr += (sr - tr) * mix_;
    tl = highs_[0].lowpass(tl);
    tr = highs_[1].lowpass(tr);
    tl += bump_[0].lowpass(tl) * 0.3f * amount_;
    tr += bump_[1].lowpass(tr) * 0.3f * amount_;
    const float wobble = 1.0f + 0.05f * amount_ * level;
    // Hiss only while there is music: it fades out with the tails instead of sitting in silence.
    const float hiss = hissTone_.highpass(hiss_.next()) * hissLevel_ * std::min(1.0f, hissEnv_ * 25.0f);
    l = tl * wobble + hiss;
    r = tr * wobble + hiss;
}

void Glue::prepare(float sampleRate)
{
    attack_ = 1.0f - std::exp(-1.0f / (0.004f * sampleRate));  // 4 ms
    release_ = 1.0f - std::exp(-1.0f / (0.15f * sampleRate));  // 150 ms
    dcL_.setLowpass(8.0f, sampleRate);
    dcR_.setLowpass(8.0f, sampleRate);
    gainGlide_ = 1.0f - std::exp(-1.0f / (0.0002f * sampleRate)); // ~0.2 ms
    reset();
}

void Glue::setPump(float amount)
{
    const float a = dsp::clamp01(amount);
    threshold_ = 0.6f - 0.42f * a;
    ratio_ = 3.0f + 7.0f * a;
    makeup_ = 1.0f + 0.9f * a;
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
        target_ = (env_ > threshold_ ? std::pow(env_ / threshold_, 1.0f / ratio_ - 1.0f) : 1.0f) * makeup_;
        counter_ = 8;
    }
    gain_ += (target_ - gain_) * gainGlide_;
    l *= gain_;
    r *= gain_;
}

} // namespace gristo::sound
