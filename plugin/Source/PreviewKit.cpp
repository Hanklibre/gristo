#include "PreviewKit.h"

#include <cmath>

namespace gristo {

namespace {
constexpr float kTwoPi = 6.28318530718f;
inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
constexpr float kPan[kNumVoices] = { 0.0f, 0.0f, 0.3f, 0.3f, -0.35f, -0.2f };
} // namespace

void PreviewKit::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    reset();
}

void PreviewKit::reset()
{
    for (auto& v : voices_)
        v = VoiceState {};
    for (auto& s : synths_)
        s = SynthState {};
}

void PreviewKit::arpNoteOn(int arp, int note, float velocity)
{
    if (arp < 0 || arp >= kNumArps)
        return;
    SynthState& s = synths_[arp];
    s.target = 440.0f * std::pow(2.0f, static_cast<float>(note - 69) / 12.0f);
    if (!s.gate)
    {
        s.freq = s.target; // new phrase: no glide, retrigger the envelopes
        s.filterEnv = 1.0f;
    }
    else
    {
        s.filterEnv = std::max(s.filterEnv, 0.5f); // legato: glide, softer filter re-accent
    }
    s.note = note;
    s.gate = true;
    s.velocity = velocity;
}

void PreviewKit::arpNoteOff(int arp, int note)
{
    if (arp >= 0 && arp < kNumArps && synths_[arp].note == note)
        synths_[arp].gate = false;
}

float PreviewKit::decayFor(float seconds) const
{
    // Multiplier that reaches -60 dB after `seconds`.
    return std::pow(0.001f, 1.0f / (seconds * static_cast<float>(sampleRate_)));
}

float PreviewKit::noise()
{
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_) * (2.0f / 4294967295.0f) - 1.0f;
}

void PreviewKit::trigger(int voice, float velocity)
{
    if (voice < 0 || voice >= kNumVoices)
        return;
    VoiceState& v = voices_[voice];
    v.velocity = velocity * velocity; // perceptual curve
    v.phase = 0.0f;
    v.burstCounter = 0;
    switch (voice)
    {
        case Kick:
            v.amp = 1.0f; v.ampDecay = decayFor(0.45f);
            v.freq = 160.0f; v.freqEnd = 46.0f; v.freqDecay = decayFor(0.06f);
            v.noiseAmp = 0.15f; v.noiseDecay = decayFor(0.004f);
            break;
        case Snare:
            v.amp = 0.55f; v.ampDecay = decayFor(0.12f);
            v.freq = 210.0f; v.freqEnd = 185.0f; v.freqDecay = decayFor(0.03f);
            v.noiseAmp = 0.7f; v.noiseDecay = decayFor(0.2f);
            break;
        case HiHat1:
            voices_[HiHat2].noiseAmp *= 0.05f; // closed hat chokes the open one
            v.amp = 0.0f;
            v.noiseAmp = 0.45f; v.noiseDecay = decayFor(0.045f);
            break;
        case HiHat2:
            v.amp = 0.0f;
            v.noiseAmp = 0.35f; v.noiseDecay = decayFor(0.32f);
            break;
        case Perc1: // clap: three quick bursts, then a tail
            v.amp = 0.0f;
            v.noiseAmp = 0.6f; v.noiseDecay = decayFor(0.012f);
            v.burstCounter = 1;
            break;
        case Perc2: // low tom / conga
            v.amp = 0.8f; v.ampDecay = decayFor(0.28f);
            v.freq = 240.0f; v.freqEnd = 150.0f; v.freqDecay = decayFor(0.08f);
            v.noiseAmp = 0.05f; v.noiseDecay = decayFor(0.01f);
            break;
        default:
            break;
    }
}

void PreviewKit::render(float* left, float* right, int numSamples, float gain)
{
    const float invSr = 1.0f / static_cast<float>(sampleRate_);
    const int burstSpacing = static_cast<int>(0.011 * sampleRate_);

    for (int i = 0; i < numSamples; ++i)
    {
        float l = 0.0f, r = 0.0f;
        for (int vi = 0; vi < kNumVoices; ++vi)
        {
            VoiceState& v = voices_[vi];
            if (v.amp < 1e-5f && v.noiseAmp < 1e-5f && v.burstCounter == 0)
                continue;

            float s = 0.0f;
            if (v.amp > 1e-5f)
            {
                s += std::sin(v.phase) * v.amp;
                v.phase += kTwoPi * v.freq * invSr;
                if (v.phase > kTwoPi) v.phase -= kTwoPi;
                v.freq = v.freqEnd + (v.freq - v.freqEnd) * v.freqDecay;
                v.amp *= v.ampDecay;
            }

            float n = noise();
            if (vi == HiHat1 || vi == HiHat2 || vi == Snare || vi == Perc1)
            {
                // One-pole high-pass to brighten the noise.
                const float hp = 0.85f * (v.hpState + n - v.hpPrev);
                v.hpPrev = n;
                v.hpState = hp;
                n = hp;
            }
            s += n * v.noiseAmp;
            v.noiseAmp *= v.noiseDecay;

            if (v.burstCounter > 0 && ++v.burstCounter % burstSpacing == 0)
            {
                const int burst = v.burstCounter / burstSpacing;
                if (burst < 3)
                {
                    v.noiseAmp = 0.6f;
                }
                else
                {
                    v.noiseAmp = 0.5f;
                    v.noiseDecay = decayFor(0.16f);
                    v.burstCounter = 0;
                }
            }

            s *= v.velocity;
            l += s * (1.0f - kPan[vi]);
            r += s * (1.0f + kPan[vi]);
        }
        // Arp preview synths: saw (arp 1) and square (arp 2) through a decaying lowpass.
        const float sr = static_cast<float>(sampleRate_);
        for (int a = 0; a < kNumArps; ++a)
        {
            SynthState& s = synths_[a];
            if (!s.gate && s.env < 1e-4f)
                continue;
            s.freq += (s.target - s.freq) * (1.0f - std::exp(-1.0f / (0.045f * sr))); // ~45 ms glide
            s.phase += s.freq * invSr;
            if (s.phase >= 1.0f) s.phase -= 1.0f;
            const float osc = a == 0 ? 2.0f * s.phase - 1.0f : (s.phase < 0.5f ? 0.7f : -0.7f);
            s.env = s.gate ? s.env + (1.0f - s.env) * 0.01f : s.env * (1.0f - 30.0f * invSr * 4.0f);
            s.filterEnv *= 1.0f - 8.0f * invSr;
            const float cutoff = clamp01(0.02f + 0.35f * s.filterEnv * (0.4f + 0.6f * s.velocity));
            s.lowpass += (osc - s.lowpass) * cutoff;
            const float out = s.lowpass * s.env * s.velocity * 0.35f;
            const float pan = a == 0 ? -0.2f : 0.25f;
            l += out * (1.0f - pan);
            r += out * (1.0f + pan);
        }
        left[i] += std::tanh(l * gain) * 0.8f;
        right[i] += std::tanh(r * gain) * 0.8f;
    }
}

} // namespace gristo
