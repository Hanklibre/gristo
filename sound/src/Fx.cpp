#include "Fx.h"

namespace gristo::sound {

// ---------------------------------------------------------------------------------

void Delay::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    const auto length = static_cast<size_t>(sampleRate * 3.0f) + 4; // 1/4 note at 20 bpm
    left_.assign(length, 0.0f);
    right_.assign(length, 0.0f);
    glideCoef_ = 1.0f - std::exp(-1.0f / (0.04f * sampleRate)); // time changes glide over ~40 ms
    envRelease_ = std::exp(-1.0f / (0.08f * sampleRate));
    drift_.prepare(sampleRate, 0.3f, 0xd71f7u);
    voicedType_ = -1;
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
        damp2_[i].reset();
        lowCut_[i].reset();
        inFilter_[i].reset();
        outFilter_[i].reset();
        sat_[i].reset();
    }
    env_ = 0.0f;
    time_ = targetTime_;
}

void Delay::set(float seconds, float feedback, int type, float age, bool pingPong)
{
    const float maxSamples = static_cast<float>(left_.size()) - 4.0f;
    targetTime_ = std::clamp(seconds * sampleRate_, 1.0f, std::max(1.0f, maxSamples));
    if (time_ <= 0.0f)
        time_ = targetTime_;
    type_ = std::clamp(type, 0, static_cast<int>(Count) - 1);
    age_ = dsp::clamp01(age);
    pingPong_ = pingPong;
    // A BBD can be pushed further: its clipping keeps it from running away.
    feedback_ = std::clamp(feedback, 0.0f, type_ == Bbd ? 0.95f : 0.9f);
}

void Delay::voice()
{
    const float seconds = time_ / sampleRate_;
    float hz = 12000.0f, low = 60.0f;
    switch (type_)
    {
        case Tape:
            hz = 6500.0f * std::pow(0.4f, age_); // 6.5 kHz new, 2.6 kHz worn
            low = 90.0f + 120.0f * age_;
            loopDrive_ = 1.3f + 1.7f * age_;
            noise_ = 0.0006f + 0.004f * age_ * age_;
            break;
        case Bbd:
        {
            // A 4096-stage chip: clock = stages / (2 * time); the audio band ends near clock / 2.5.
            const float clock = 4096.0f / (2.0f * std::max(0.02f, seconds));
            hz = std::clamp(clock / 2.5f, 1200.0f, 10000.0f) * (1.0f - 0.35f * age_);
            low = 70.0f;
            loopDrive_ = 1.8f + 1.5f * age_;
            noise_ = 0.002f + 0.006f * age_;
            break;
        }
        default:
            hz = 12000.0f;
            low = 40.0f;
            loopDrive_ = 1.0f;
            noise_ = 0.0f;
            break;
    }
    hz = std::min(hz, sampleRate_ * 0.45f);
    for (int i = 0; i < 2; ++i)
    {
        damp_[i].setLowpass(hz, sampleRate_);
        damp2_[i].setLowpass(hz * 1.2f, sampleRate_);
        inFilter_[i].setLowpass(type_ == Bbd ? hz * 1.4f : 20000.0f, sampleRate_);
        outFilter_[i].setLowpass(std::min(hz * 1.3f, sampleRate_ * 0.45f), sampleRate_);
        lowCut_[i].setLowpass(low, sampleRate_);
        sat_[i].setBias(type_ == Tape ? 0.1f : 0.0f);
    }
    voicedTime_ = time_;
    voicedAge_ = age_;
    voicedType_ = type_;
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
    if (type_ != voicedType_ || age_ != voicedAge_ || std::abs(time_ - voicedTime_) > 0.002f * voicedTime_)
        voice();

    // The heads (tape) or the clock (BBD) wander.
    float mod = 0.0f;
    if (type_ == Tape)
    {
        wowPhase_ += 0.45f / sampleRate_;
        wowPhase_ -= std::floor(wowPhase_);
        flutterPhase_ += 6.1f / sampleRate_;
        flutterPhase_ -= std::floor(flutterPhase_);
        mod = sampleRate_ * ((0.0003f + 0.0012f * age_) * (std::sin(dsp::kTwoPi * wowPhase_) + 0.7f * drift_.tick())
                             + (0.00003f + 0.0001f * age_) * std::sin(dsp::kTwoPi * flutterPhase_));
    }
    else if (type_ == Bbd)
    {
        lfoPhase_ += 0.25f / sampleRate_;
        lfoPhase_ -= std::floor(lfoPhase_);
        const float tri = 4.0f * std::abs(lfoPhase_ - 0.5f) - 1.0f;
        mod = sampleRate_ * (0.0002f + 0.0012f * age_) * tri;
    }
    const float spread = pingPong_ ? 1.0f : 1.013f; // not ping-pong: the right side a touch later, for width
    const float l = read(left_, std::max(1.0f, time_ + mod));
    const float r = read(right_, std::max(1.0f, time_ * spread - mod));

    // Each repeat: darker, thinner, softly saturated (Clean: only a gentle limit).
    const float peak = std::max(std::abs(l), std::abs(r));
    env_ = peak > env_ ? peak : env_ * envRelease_;
    // Hiss only while echoes sound (a BBD's compander breathes with them), never in silence.
    const float breath = env_ > 1e-5f ? noise_ * std::min(1.0f, env_ * 4.0f) : 0.0f;
    auto loop = [&](int i, float x) {
        x = damp2_[i].lowpass(damp_[i].lowpass(x));
        x = lowCut_[i].highpass(x);
        x = sat_[i].process(x * feedback_ * loopDrive_) / loopDrive_;
        return x + hiss_.next() * breath * feedback_;
    };
    const float fromL = loop(0, l), fromR = loop(1, r);
    const float input = inFilter_[0].lowpass(in);
    if (pingPong_)
    {
        left_[write_] = input + fromR; // the input enters on the left, each repeat crosses sides
        right_[write_] = fromL;
    }
    else
    {
        left_[write_] = input + fromL;
        right_[write_] = input + fromR;
    }
    if (++write_ >= left_.size())
        write_ = 0;
    // What comes out has been through the tape's (or the chip's) filters too, the first echo included.
    outL = type_ == Clean ? l : outFilter_[0].lowpass(l);
    outR = type_ == Clean ? r : outFilter_[1].lowpass(r);
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
    wet_ = std::min(1.0f, a * 2.0f);
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
    for (auto& s : sat_)
        s.reset();
}

void Overdrive::set(float amount)
{
    const float a = dsp::clamp01(amount);
    mix_ = std::min(1.0f, a * 1.6f);
    gain_ = 1.0f + 2.0f * a + 6.0f * a * a;
    for (auto& s : sat_)
        s.setBias(0.15f * a);
    // Peaks are held, quiet material comes up only a little.
    makeup_ = std::pow(gain_, -0.65f);
    for (auto& f : tone_)
        f.setLowpass(18000.0f - 7000.0f * a, sampleRate_);
}

void Overdrive::process(float& l, float& r)
{
    const float dl = tone_[0].lowpass(sat_[0].process(l * gain_) * makeup_);
    const float dr = tone_[1].lowpass(sat_[1].process(r * gain_) * makeup_);
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
    hissRelease_ = std::exp(-1.0f / (0.4f * sampleRate));
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
    for (auto* f : { &highs_[0], &highs_[1], &bump_[0], &bump_[1], &hissTone_, &lowDry_[0], &lowDry_[1], &lowWet_[0], &lowWet_[1] })
        f->reset();
    for (auto& s : sat_)
        s.reset();
}

void Tape::set(float amount)
{
    const float a = dsp::clamp01(amount);
    amount_ = a;
    // Blended in with the amount (all of it only at full), driven gently: tape rounds
    // the peaks off rather than lifting the floor.
    mix_ = std::min(1.0f, a * 1.25f);
    gain_ = 1.0f + 2.5f * a;
    makeup_ = std::pow(gain_, -0.8f);
    wowDepth_ = a * a * 0.0008f * sampleRate_;  // up to about 6 cents at 0.6 Hz
    flutterDepth_ = a * 0.00003f * sampleRate_; // a faster, smaller shimmer on top
    hissLevel_ = a * a * 0.004f;
    for (int ch = 0; ch < 2; ++ch)
    {
        highs_[ch].setLowpass(20000.0f * std::pow(7000.0f / 20000.0f, a), sampleRate_);
        bump_[ch].setLowpass(110.0f, sampleRate_);
        lowDry_[ch].setLowpass(100.0f, sampleRate_);
        lowWet_[ch].setLowpass(100.0f, sampleRate_);
        sat_[ch].setBias(0.12f * a); // a little asymmetry: even harmonics, warmth
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
    // Saturated above ~100 Hz, clean below: the saturated lows are swapped for the clean ones.
    float sl = sat_[0].process(tl * gain_) * makeup_, sr = sat_[1].process(tr * gain_) * makeup_;
    sl += lowDry_[0].lowpass(tl) - lowWet_[0].lowpass(sl);
    sr += lowDry_[1].lowpass(tr) - lowWet_[1].lowpass(sr);
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

void Resonator::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    line_.assign(static_cast<size_t>(sampleRate / 25.0f) + 8, 0.0f);
    reset();
}

void Resonator::reset()
{
    std::fill(line_.begin(), line_.end(), 0.0f);
    write_ = 0;
    lp_ = 0.0f;
}

void Resonator::set(float hz, float decaySeconds, float damping)
{
    hz = std::clamp(hz, 30.0f, sampleRate_ * 0.25f);
    damp_ = 1.0f - 0.85f * dsp::clamp01(damping);
    // The loop filter delays by about (1 - a) / a samples: take it off the line.
    delay_ = std::clamp(sampleRate_ / hz - (1.0f - damp_) / damp_, 1.0f, std::max(1.0f, static_cast<float>(line_.size()) - 3.0f));
    feedback_ = std::min(0.995f, std::pow(0.001f, 1.0f / (std::max(0.01f, decaySeconds) * hz)));
}

namespace {
struct BodyMode
{
    float ratio, gain, decay; // pitch against the first mode, level, decay against the first mode's
};
// Shell: membrane modes (Bessel ratios) over a wooden shell, quick to lose their highs.
// Box: a low air (Helmholtz) mode under dull panel modes.
// Metal: the partials of a struck metal plate, inharmonic and long. Tube: harmonics, like a tuned pipe.
constexpr BodyMode kBodyModes[DrumBody::Count][8] = {
    { { 1.00f, 0.60f, 1.00f }, { 1.59f, 0.70f, 0.70f }, { 2.14f, 0.52f, 0.52f }, { 2.30f, 0.45f, 0.46f },
      { 2.65f, 0.36f, 0.38f }, { 2.92f, 0.30f, 0.33f }, { 3.16f, 0.25f, 0.29f }, { 3.50f, 0.20f, 0.25f } },
    { { 0.50f, 1.20f, 1.30f }, { 1.00f, 0.80f, 0.75f }, { 1.43f, 0.50f, 0.50f }, { 1.83f, 0.40f, 0.40f },
      { 2.37f, 0.30f, 0.30f }, { 2.95f, 0.22f, 0.24f }, { 3.60f, 0.15f, 0.19f }, { 4.40f, 0.10f, 0.15f } },
    { { 1.00f, 0.80f, 1.00f }, { 1.47f, 0.80f, 0.95f }, { 1.81f, 0.70f, 0.90f }, { 2.81f, 0.55f, 0.85f },
      { 3.31f, 0.45f, 0.80f }, { 4.25f, 0.35f, 0.70f }, { 5.06f, 0.30f, 0.65f }, { 6.65f, 0.25f, 0.60f } },
    { { 1.0f, 1.00f, 1.00f }, { 2.0f, 0.62f, 0.71f }, { 3.0f, 0.46f, 0.58f }, { 4.0f, 0.38f, 0.50f },
      { 5.0f, 0.32f, 0.45f }, { 6.0f, 0.28f, 0.41f }, { 7.0f, 0.25f, 0.38f }, { 8.0f, 0.23f, 0.35f } },
    // Wood: an acoustic instrument's body: the air (Helmholtz) mode, the top and back
    // plates, then denser, quicker plate modes.
    { { 1.00f, 1.00f, 1.00f }, { 1.95f, 0.85f, 0.80f }, { 2.25f, 0.70f, 0.70f }, { 2.90f, 0.50f, 0.55f },
      { 3.60f, 0.40f, 0.45f }, { 4.50f, 0.30f, 0.38f }, { 5.60f, 0.24f, 0.30f }, { 7.10f, 0.18f, 0.25f } },
    // Glass: a free bar's modes (1, 2.76, 5.40, 8.93) and a few in between, bright and long.
    { { 1.00f, 1.00f, 1.00f }, { 2.76f, 0.80f, 0.95f }, { 5.40f, 0.65f, 0.90f }, { 8.93f, 0.50f, 0.85f },
      { 1.50f, 0.25f, 0.80f }, { 3.90f, 0.30f, 0.80f }, { 6.80f, 0.25f, 0.70f }, { 11.0f, 0.20f, 0.65f } },
};
} // namespace

void DrumBody::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    reset();
}

void DrumBody::reset()
{
    for (int i = 0; i < kModes; ++i)
        y1_[i] = y2_[i] = 0.0f;
}

void DrumBody::strike(float hz, float decaySeconds, int type, float variation, std::uint32_t seed)
{
    const auto& modes = kBodyModes[std::clamp(type, 0, static_cast<int>(Count) - 1)];
    std::uint32_t r = seed * 747796405u + 2891336453u;
    auto wobble = [&r] { // -1..1
        r = r * 1664525u + 1013904223u;
        return static_cast<float>(r >> 8) / 8388608.0f - 1.0f;
    };
    const float spread = 0.004f * dsp::clamp01(variation);
    int n = 0;
    for (int i = 0; i < kModes; ++i)
    {
        const float f = hz * modes[i].ratio * (1.0f + spread * wobble());
        if (f < 20.0f || f > sampleRate_ * 0.45f)
            continue;
        const float w = dsp::kTwoPi * f / sampleRate_;
        const float tau = std::max(0.01f, decaySeconds * modes[i].decay) / 6.91f; // 60 dB over the decay
        const float radius = std::exp(-1.0f / (tau * sampleRate_));
        // Input gain: a ring that grows with Q, but slower (the square root) than a plain
        // resonator's, so long, tuned bodies don't run away on a drum at their pitch.
        const float q = 3.14159265f * f * tau;
        const float gain = modes[i].gain * (1.0f + 0.15f * dsp::clamp01(variation) * wobble()); // strikes differ a little
        // Keep the state of a mode that is still ringing: a new strike adds to it.
        if (n != i)
        {
            y1_[n] = y1_[i];
            y2_[n] = y2_[i];
        }
        b1_[n] = 2.0f * radius * std::cos(w);
        b2_[n] = radius * radius;
        g_[n] = gain * 2.0f * (1.0f - radius) * std::sin(w) * std::sqrt(q) * 2.0f;
        ++n;
    }
    for (int i = n; i < kModes; ++i)
        y1_[i] = y2_[i] = 0.0f;
    count_ = n;
}

void Shimmer::prepare(float sampleRate)
{
    window_ = 0.085f * sampleRate;
    line_.assign(static_cast<size_t>(window_) + 8, 0.0f);
    low_.setLowpass(6000.0f, sampleRate);
    high_.setLowpass(300.0f, sampleRate);
    reset();
}

void Shimmer::reset()
{
    std::fill(line_.begin(), line_.end(), 0.0f);
    write_ = 0;
    phase_ = 0.0f;
    low_.reset();
    high_.reset();
}

float Shimmer::process(float in)
{
    if (line_.empty())
        return 0.0f;
    const auto size = line_.size();
    line_[write_] = high_.highpass(low_.lowpass(in));
    // Each tap's delay shrinks by a sample per sample: it reads at double speed.
    phase_ += 1.0f / window_;
    phase_ -= std::floor(phase_);
    float out = 0.0f;
    for (int tap = 0; tap < 2; ++tap)
    {
        float ph = phase_ + 0.5f * static_cast<float>(tap);
        ph -= std::floor(ph);
        const float pos = static_cast<float>(write_ + size) - window_ * (1.0f - ph);
        const auto i0 = static_cast<size_t>(pos);
        const float frac = pos - static_cast<float>(i0);
        const float a = line_[i0 % size], b = line_[(i0 + 1) % size];
        const float w = std::sin(dsp::kPi * ph);
        out += (a + (b - a) * frac) * w * w;
    }
    write_ = (write_ + 1) % size;
    return out;
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
    makeup_ = 1.0f + 1.3f * a;
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
