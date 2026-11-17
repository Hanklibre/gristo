// Granular processor after Mutable Instruments Beads.
// Copyright 2017 Emilie Gillet (MIT licence); ported and adapted for Gristo.
#include "Beads.h"

namespace gristo::sound {

namespace {
constexpr float kMaxGrainFrequency = 130.81278f; // Hz
constexpr int kSafety = 257;                      // samples kept clear of the recording head
const int kDivisions[8] = { 1, 2, 3, 4, 6, 8, 12, 16 };

float smoothStep(float t) { return t * t * (3.0f - 2.0f * t); }
float semitones(float s) { return std::exp2(s / 12.0f); }
} // namespace

void Beads::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    size_ = static_cast<int>(sampleRate * 4.0f);
    for (auto& b : buffer_)
        b.assign(static_cast<size_t>(size_), 0.0f);
    for (auto& f : dcIn_)
        f.setLowpass(10.0f, sampleRate);
    for (auto& f : dcOut_)
        f.setLowpass(10.0f, sampleRate);
    reset();
}

void Beads::reset()
{
    for (auto& b : buffer_)
        std::fill(b.begin(), b.end(), 0.0f);
    write_ = 0;
    for (auto& g : grains_)
        g = Grain {};
    pendingGate_ = false;
    clockPhase_ = 0.0f;
    ratio_ = 1.0f;
    sinceGate_ = static_cast<int>(sampleRate_ / 2.0f);
    gateCount_ = 0;
    sinceGrain_ = 0;
    lastWet_[0] = lastWet_[1] = held_[0] = held_[1] = 0.0f;
    for (auto* f : { &tone_[0], &tone_[1], &dcIn_[0], &dcIn_[1], &dcOut_[0], &dcOut_[1] })
        f->reset();
}

int Beads::activeGrains() const
{
    int n = 0;
    for (const auto& g : grains_)
        n += g.active ? 1 : 0;
    return n;
}

// Density, as on the module:
//   no gates: left of noon a steady clock (fast at the far left), right of noon a random
//   one (sparse near noon, dense at the far right), noon itself silent;
//   gated (drum hits): left of noon divides the hits (every hit at the far left, every
//   16th near noon), right of noon lets each one through by chance (always at the far
//   right), noon silent.
bool Beads::trigger()
{
    const float d = dsp::clamp01(settings_.density);
    const bool gate = pendingGate_;
    pendingGate_ = false;
    ++sinceGate_;
    if (settings_.gated)
    {
        if (!gate)
            return false;
        sinceGate_ = 0;
        const int count = gateCount_++;
        if (d < 0.48f)
        {
            const int division = kDivisions[std::clamp(static_cast<int>(d * 2.15f * 8.0f), 0, 7)];
            regularity_ = 1.0f;
            return count % division == 0;
        }
        const float p = std::max((d - 0.52f) * 2.15f, 0.0f);
        regularity_ = p * p >= 1.0f ? 1.0f : 0.0f;
        return random() < p * p;
    }
    float frequency;
    bool randomise;
    if (d < 0.48f)
    {
        frequency = kMaxGrainFrequency * semitones(-d * 192.0f);
        randomise = false;
    }
    else if (d > 0.52f)
    {
        frequency = 0.5f * kMaxGrainFrequency * semitones(-(1.0f - d) * 192.0f);
        randomise = true;
    }
    else
        return false;
    const float f = std::clamp(frequency / sampleRate_ * (randomise ? ratio_ : 1.0f), 1e-6f, 0.25f);
    clockPhase_ += f;
    if (clockPhase_ < 1.0f)
        return false;
    clockPhase_ -= 1.0f;
    if (randomise) // Poisson-like intervals
    {
        const float x = 0.1f + 0.9f * random();
        ratio_ = -1.0f / (-x * (1.0f + x * (0.5f + x * (0.3333f + x * 0.25f))));
    }
    regularity_ = randomise ? 0.0f : 1.0f;
    return true;
}

void Beads::startGrain(float fraction)
{
    const BeadsSettings& s = settings_;
    auto randomised = [this](float base, float amount) { // a random target in 0..1, blended in by the amount
        return amount > 0.0f ? dsp::clamp01(base + (random() - base) * dsp::clamp01(amount)) : dsp::clamp01(base);
    };
    const float time = randomised(s.time, s.timeRandom);
    const float sizeValue = randomised(s.size, s.sizeRandom);
    const float shape = randomised(s.shape, s.shapeRandom);
    float pitch = s.pitch;
    if (s.pitchRandom > 0.0f)
    {
        const float reach = 24.0f * dsp::clamp01(s.pitchRandom);
        if (s.musicalPitch)
        {
            // The intervals within reach, each as likely: octaves, fifths and fourths.
            static const float intervals[] = { 0.0f, 12.0f, -12.0f, 7.0f, -5.0f, 19.0f, -17.0f, 24.0f, -24.0f, 5.0f, -7.0f };
            int count = 0;
            for (float i : intervals)
                count += std::abs(i) <= reach + 0.01f ? 1 : 0;
            int pick = std::min(count - 1, static_cast<int>(random() * static_cast<float>(count)));
            for (float i : intervals)
                if (std::abs(i) <= reach + 0.01f && pick-- == 0)
                {
                    pitch += i;
                    break;
                }
        }
        else
            pitch += (2.0f * random() - 1.0f) * reach;
    }

    // Duration: short grains above 1/3, and below it the same lengths played backwards.
    const bool reverse = sizeValue < 0.3333333f;
    const float clamped = reverse ? sizeValue * 3.0f * (2.0f - sizeValue * 3.0f) : 1.0f - 1.5f * (sizeValue - 0.3333333f);
    const float shortness = std::min(1.0f, 1.025f * clamped);
    float duration = semitones(shortness * -108.0f) * sampleRate_ * 4.0f;

    const float speed = (reverse ? -1.0f : 1.0f) * semitones(pitch);
    const float consumption = speed - (s.freeze ? 0.0f : 1.0f);
    const int maxDelay = size_ - 2 * kSafety;
    float delay = time * static_cast<float>(maxDelay) + static_cast<float>(kSafety) - fraction * consumption;
    if (consumption != 0.0f)
        duration = std::min(duration, static_cast<float>(size_ - kSafety) / std::abs(consumption));
    if (consumption > 0.0f)
        delay = std::max(delay, duration * consumption + kSafety);
    else if (consumption < 0.0f)
        delay = std::min(delay, static_cast<float>(size_ - kSafety) + duration * consumption);
    duration = std::max(duration, 16.0f);

    // A slot: a free one, else steal the grain closest to its end (and hurry the next ones).
    int active = 0;
    Grain* slot = nullptr;
    for (auto& g : grains_)
    {
        if (g.active)
            ++active;
        else if (slot == nullptr)
            slot = &g;
    }
    if (slot == nullptr)
    {
        std::array<Grain*, kGrains> order {};
        for (int i = 0; i < kGrains; ++i)
            order[static_cast<size_t>(i)] = &grains_[static_cast<size_t>(i)];
        std::partial_sort(order.begin(), order.begin() + 4, order.end(), [](const Grain* a, const Grain* b) { return a->remaining < b->remaining; });
        slot = order[0];
        const float fades[3] = { 0.005f, 0.005f, 0.1f };
        for (int i = 1; i < 4; ++i)
        {
            Grain* g = order[static_cast<size_t>(i)];
            const int samples = static_cast<int>(fades[i - 1] * sampleRate_);
            if (g->remaining > samples)
            {
                g->stealStep = g->steal / static_cast<float>(samples);
                g->remaining = samples;
            }
        }
    }

    // Level and position: quieter with more grains, spread wider the more is randomised.
    const float amplitude = 1.0f / std::sqrt(static_cast<float>(active + 1));
    const float spread = std::min(1.0f, s.timeRandom + s.sizeRandom + s.shapeRandom + s.pitchRandom + (1.0f - regularity_));
    const float panSpread = 0.5f * static_cast<float>(active + 1) * spread;
    float pan = 0.5f + (random() - 0.5f) * std::min(panSpread * random(), 1.0f);

    Grain& g = *slot;
    g = Grain {};
    g.active = true;
    g.delay = delay;
    g.speed = speed;
    g.remaining = static_cast<int>(duration);
    if (pan < 0.5f)
    {
        pan *= 2.0f;
        g.gain[0][0] = amplitude;
        g.gain[1][0] = 0.0f;
        g.gain[1][1] = pan * (2.0f - pan) * amplitude;
        g.gain[0][1] = (1.0f - pan) * (1.0f + pan) * amplitude;
    }
    else
    {
        pan = (1.0f - pan) * 2.0f;
        g.gain[1][1] = amplitude;
        g.gain[0][1] = 0.0f;
        g.gain[0][0] = pan * (2.0f - pan) * amplitude;
        g.gain[1][0] = (1.0f - pan) * (1.0f + pan) * amplitude;
    }

    // The envelope (Beads' GrainEnvelope): shape 0 square .. 1/3 decay .. 2/3 bell .. 1 reverse ramp.
    const float sustain = std::max(1.0f - 3.0f * shape, 0.0f);
    float a = (shape - 0.33333333f) * 1.5f;
    if (a < 0.0f)
        a = 0.0f;
    else if (a < 0.5f)
        a = a * a * a * 4.0f;
    const float release = (1.0f - a) * (1.0f - sustain);
    const float attackTime = std::max(duration * a, 0.0005f * sampleRate_ * (1.0f + 3.0f * sustain));
    const float sustainTime = std::max(duration * sustain, 2.0f);
    const float releaseTime = std::max(duration * release, 0.010f * sampleRate_);
    g.inc[0] = 1.0f / attackTime;
    g.inc[1] = 1.0f / sustainTime;
    g.inc[2] = 1.0f / releaseTime;
    g.inc[3] = 0.0f;
    g.smoothness = a < 0.5f ? 2.0f * a : 2.0f - 2.0f * a;
    g.stage = 0;
    g.phase = g.inc[0] * fraction;
}

float Beads::envelope(Grain& g)
{
    if (g.stage >= 3)
        return 0.0f;
    float value;
    if (g.stage == 1)
        value = 1.0f;
    else
    {
        const bool attack = g.stage == 0;
        const float t = attack ? g.phase : 1.0f - g.phase;
        const float base = attack ? t : (t * t) * (t * t);
        value = base + (smoothStep(dsp::clamp01(t)) - base) * g.smoothness;
    }
    g.phase += g.inc[g.stage];
    if (g.phase >= 1.0f)
    {
        const float over = (g.phase - 1.0f) / g.inc[g.stage];
        ++g.stage;
        g.phase = g.stage < 3 ? over * g.inc[g.stage] : 0.0f;
    }
    return value;
}

void Beads::read(double delay, float& l, float& r) const
{
    // Hermite interpolation, delay samples behind the head.
    const double pos = static_cast<double>(write_) - delay + static_cast<double>(size_) * 2.0;
    const int i = static_cast<int>(pos);
    const float t = static_cast<float>(pos - static_cast<double>(i));
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto& b = buffer_[ch];
        const float xm1 = b[static_cast<size_t>((i - 1) % size_)], x0 = b[static_cast<size_t>(i % size_)];
        const float x1 = b[static_cast<size_t>((i + 1) % size_)], x2 = b[static_cast<size_t>((i + 2) % size_)];
        const float c = (x1 - xm1) * 0.5f, v = x0 - x1, w = c + v, a = w + v + (x2 - x0) * 0.5f, bb = w + a;
        (ch == 0 ? l : r) = (((a * t) - bb) * t + c) * t + x0;
    }
}

void Beads::process(float inL, float inR, float& outL, float& outR)
{
    outL = outR = 0.0f;
    if (size_ <= 0)
        return;
    const BeadsSettings& s = settings_;
    const int quality = std::clamp(s.quality, 0, 3);

    // Recording: the input plus the feedback, through the chosen medium.
    if (!s.freeze)
    {
        const float fb = dsp::clamp01(s.feedback);
        float in[2] = { inL + lastWet_[0] * fb * fb * 1.5f, inR + lastWet_[1] * fb * fb * 1.5f };
        for (int ch = 0; ch < 2; ++ch)
        {
            float x = dsp::fastTanh(dcIn_[ch].highpass(in[ch]));
            if (quality == 1) // vintage digital: 12 bits
                x = std::round(x * 2048.0f) / 2048.0f;
            else if (quality == 3) // dirty tape: an 8-bit mu-law converter
            {
                const float m = std::log1p(255.0f * std::min(1.0f, std::abs(x))) / std::log(256.0f);
                const float q = std::round(m * 127.0f) / 127.0f;
                x = std::copysign((std::pow(256.0f, q) - 1.0f) / 255.0f, x);
            }
            in[ch] = x;
        }
        // The lower-rate media: hold samples (32 kHz vintage, 24 kHz tape).
        const float rate = quality == 1 ? 32000.0f : quality >= 2 ? 24000.0f : sampleRate_;
        holdPhase_ += std::min(1.0f, rate / sampleRate_);
        if (holdPhase_ >= 1.0f || quality == 0)
        {
            holdPhase_ -= std::floor(holdPhase_);
            held_[0] = in[0];
            held_[1] = in[1];
        }
        buffer_[0][static_cast<size_t>(write_)] = held_[0];
        buffer_[1][static_cast<size_t>(write_)] = held_[1];
        write_ = (write_ + 1) % size_;
    }

    if (trigger())
    {
        startGrain(0.0f);
        sinceGrain_ = 0;
    }
    ++sinceGrain_;

    // Tape: the read heads wander (wow and flutter).
    double wow = 0.0;
    if (quality >= 2)
    {
        wowPhase_ += 0.6f / sampleRate_;
        wowPhase_ -= std::floor(wowPhase_);
        flutterPhase_ += 7.3f / sampleRate_;
        flutterPhase_ -= std::floor(flutterPhase_);
        const float depth = quality == 3 ? 1.0f : 0.25f;
        wow = depth * sampleRate_ * (0.0011f * std::sin(dsp::kTwoPi * wowPhase_) + 0.00006f * std::sin(dsp::kTwoPi * flutterPhase_));
    }

    const double tail = static_cast<double>(size_);
    constexpr double fadeZone = 256.0;
    for (auto& g : grains_)
    {
        if (!g.active)
            continue;
        g.delay += (s.freeze ? 0.0 : 1.0) - static_cast<double>(g.speed);
        if (g.delay < 0.0 || g.delay >= tail) // ran into the head: stop
        {
            g.active = false;
            continue;
        }
        const double edge = std::min(g.delay, tail - g.delay);
        const float ramp = edge < fadeZone ? smoothStep(static_cast<float>(edge / fadeZone)) : 1.0f;
        float l, r;
        read(std::clamp(g.delay + wow, 2.0, tail - 3.0), l, r);
        const float e = envelope(g) * ramp * g.steal;
        g.steal = std::max(0.0f, g.steal - g.stealStep);
        outL += (l * g.gain[0][0] + r * g.gain[0][1]) * e;
        outR += (l * g.gain[1][0] + r * g.gain[1][1]) * e;
        if (--g.remaining <= 0 || g.stage >= 3 || (g.stealStep > 0.0f && g.steal <= 0.0f))
            g.active = false;
    }

    if (quality >= 2) // tape: duller
    {
        const float hz = quality == 3 ? 6000.0f : 11000.0f;
        for (auto& f : tone_)
            f.setLowpass(hz, sampleRate_);
        outL = tone_[0].lowpass(outL);
        outR = tone_[1].lowpass(outR);
    }
    outL = dcOut_[0].highpass(outL);
    outR = dcOut_[1].highpass(outR);
    lastWet_[0] = outL;
    lastWet_[1] = outR;
}

} // namespace gristo::sound
