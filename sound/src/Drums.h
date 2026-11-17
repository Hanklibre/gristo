#pragma once

#include "Dsp.h"
#include "gristo/sound/Sound.h"

#include <array>
#include <vector>

namespace gristo::sound {

// One drum: an engine and its five controls (all 0..1 except the pitch), as a kit
// defines it. The drum's own settings (DrumTweak) move these at each hit.
struct DrumSpec
{
    DrumEngine engine = DrumEngine::AnalogKick;
    float hz = 55.0f;    // the engine's pitch
    float tone = 0.5f;
    float decay = 0.5f;
    float punch = 0.5f;  // the attack: FM, click, pitch drop, burst count
    float snap = 0.5f;   // the second character: self FM, snappy, wires, dirt, noisiness
    float drive = 1.0f;  // >= 1
    float gain = 1.0f;
    float pan = 0.0f;
};

struct KitPatch
{
    DrumSpec voices[kNumDrumVoices];
    float busLowpassHz = 20000.0f;
    int crush = 1;
    float busDrive = 1.0f;
    float gain = 1.0f;
};
const KitPatch& kitPatch(Kit kit);
// The spec of an engine picked for a drum instead of the kit's (voice: which drum).
DrumSpec engineDefault(DrumEngine engine, int voice);

// A second-order state-variable filter with fixed coefficients (set at the hit).
struct Resonance
{
    float g = 0.0f, k = 2.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
    void set(float fNorm, float q) // fNorm = Hz / sample rate
    {
        g = std::tan(dsp::kPi * std::clamp(fNorm, 1e-5f, 0.49f));
        k = 1.0f / std::max(q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void setDirty(float fNorm, float q) // per sample: cheaper g (Plaits' FREQUENCY_DIRTY)
    {
        g = dsp::kPi * std::clamp(fNorm, 1e-5f, 0.4f);
        k = 1.0f / std::max(q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0.0f; }
    void tick(float x, float& lp, float& bp, float& hp)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        if (std::abs(ic1) < 1e-20f) ic1 = 0.0f;
        if (std::abs(ic2) < 1e-20f) ic2 = 0.0f;
        lp = v2;
        bp = v1;
        hp = x - k * v1 - v2;
    }
    float bandpass(float x) { float l, b, h; tick(x, l, b, h); return b; }
    float lowpass(float x) { float l, b, h; tick(x, l, b, h); return l; }
    float highpass(float x) { float l, b, h; tick(x, l, b, h); return h; }
};

class DrumVoice
{
public:
    void prepare(float sampleRate);
    // filter: 0 off, 1 LP, 2 BP, 3 HP; cutoff 0..1. alive: how much hits differ.
    void trigger(const DrumSpec& spec, float velocity, float alive, int filter = 0, float cutoff = 0.5f);
    void choke() { choke_ = true; }
    bool active() const { return active_; }
    float pan() const { return spec_.pan; }
    float pitch() const { return spec_.hz; } // what a Body resonator tunes to
    bool tonal() const;
    float tick();

private:
    float render();
    float k48(float c) const { return 1.0f - std::pow(1.0f - c, 48000.0f / sampleRate_); } // a 48 kHz one-pole coefficient here
    float d48(float c) const { return std::pow(c, 48000.0f / sampleRate_); }              // a 48 kHz per-sample decay here
    float random() { return 0.5f + 0.5f * noise_.next(); }

    DrumSpec spec_;
    float sampleRate_ = 48000.0f, accent_ = 1.0f, t_ = 0.0f;
    bool active_ = false, choke_ = false;
    float chokeGain_ = 1.0f, chokeCoef_ = 0.995f, lastOut_ = 0.0f, declick_ = 0.0f, declickCoef_ = 0.0f;
    int silent_ = 0;
    dsp::Noise noise_ { 0x1234567u };

    // Shared engine state (each engine uses what it needs).
    int pulseRemaining_ = 0, fmPulseRemaining_ = 0, hold_ = 0, bursts_ = 0;
    float pulse_ = 0.0f, pulseHeight_ = 0.0f, pulseLp_ = 0.0f, fmPulseLp_ = 0.0f, retrig_ = 0.0f;
    float lpOut_ = 0.0f, toneLp_ = 0.0f, phase_[6] {}, phaseNoise_ = 0.0f;
    float env_[4] {}, coef_[4] {}, envLp_[3] {};
    float q_ = 1.0f, toneF_ = 1.0f, leak_ = 0.0f, scale_ = 1.0f, gains_[5] {}, freqs_[6] {}, nextBurst_ = 0.0f;
    float clickLp_ = 0.0f, clickHp_ = 0.0f, noiseLp_ = 0.0f, noiseHp_ = 0.0f, noiseClock_ = 0.0f, noiseSample_ = 0.0f;
    Resonance res_[5], noiseFilter_, colour_, hp_;
    dsp::OnePole onePole_[3];
    // Acoustic heads (after OneTrick URCHIN): two heads of five modes, wires, a shell.
    float headPhase_[2][5] {}, headEnv_[2][5] {}, headCoef_[2][5] {}, headGain_[5] {};
    float energy_ = 0.0f, energyCoef_ = 0.0f, wireEnv_ = 0.0f, wireAttack_ = 0.0f, wireRelease_ = 0.0f;
    float shellPhase_ = 0.0f, shellFm_ = 0.0f, shellEnv_ = 0.0f, shellCoef_ = 0.0f;
    // Cymbal (after OneTrick URCHIN): four feedback combs.
    static constexpr int kCombLength = 2400;
    std::vector<float> comb_;
    int combWrite_ = 0, control_ = 0;
    float combFb_[4] {}, combDelay_[4] {}, strike_ = 0.0f, strikeCoef_ = 0.0f, lfo_[2] {};
    // Karplus-Strong (the Crack snare)
    std::vector<float> string_;
    int stringWrite_ = 0;
    float stringDelay_ = 100.0f, stringFb_ = 0.9f, stringLp_ = 0.0f, stringDamp_ = 0.5f;
    // After the engine: the drum's own filter, drive, a DC blocker.
    int filterMode_ = 0;
    Resonance filter_;
    dsp::OnePole dc_, velocityTone_;
    bool velocityToneOn_ = false;
};

} // namespace gristo::sound
