#pragma once

#include "Dsp.h"
#include "gristo/sound/Sound.h"

namespace gristo::sound {

enum class NoiseFilter { Highpass, Bandpass, Lowpass };

// One drum sound, built from a few layers. Times are seconds to -60 dB unless noted.
struct DrumPatch
{
    // Tonal body: a sine sweeping from f0 to f1 (pitchTime = time constant).
    float body = 0.0f, f0 = 60.0f, f1 = 50.0f, pitchTime = 0.02f, bodyDecay = 0.3f;
    float body2 = 0.0f, body2Ratio = 1.6f; // a second partial (snare / timbale ring)
    // Filtered noise, optionally as hand-clap bursts before the tail.
    float noise = 0.0f, noiseAttack = 0.0f, noiseDecay = 0.1f, noiseHz = 5000.0f, noiseQ = 0.2f;
    NoiseFilter noiseFilter = NoiseFilter::Highpass;
    int claps = 0;
    float clapSpacing = 0.010f;
    // 808-style metallic layer: six detuned squares through a bandpass.
    float metal = 0.0f, metalDecay = 0.1f, metalHz = 8000.0f;
    float click = 0.0f; // short transient
    float drive = 1.0f; // >1 saturates the voice
    float pan = 0.0f;   // -1..1
    float gain = 1.0f;
};

// A kit: six patches plus its bus colour.
struct KitPatch
{
    DrumPatch voices[kNumDrumVoices];
    float busLowpassHz = 20000.0f;
    int crush = 1;        // sample-and-hold factor (lo-fi)
    float busDrive = 1.0f;
    float gain = 1.0f;
};

const KitPatch& kitPatch(Kit kit);

class DrumVoice
{
public:
    void prepare(float sampleRate) { sampleRate_ = sampleRate; }
    // Copies the patch (with tune/decay macros applied) so a kit change mid-note is clean.
    void trigger(const DrumPatch& patch, float velocity, float tuneSemis, float decayScale);
    void choke() { choke_ = true; }
    bool active() const { return active_; }
    // Mono sample; the caller pans.
    float tick();
    float pan() const { return patch_.pan; }

private:
    DrumPatch patch_;
    float sampleRate_ = 48000.0f;
    bool active_ = false, choke_ = false;
    float chokeGain_ = 1.0f;
    float velocity_ = 0.0f;
    float t_ = 0.0f; // seconds since trigger

    float phase_ = 0.0f, phase2_ = 0.0f, freq_ = 0.0f;
    float bodyEnv_ = 0.0f, bodyCoef_ = 0.0f, pitchCoef_ = 0.0f;
    float noiseEnv_ = 0.0f, noiseCoef_ = 0.0f;
    float metalEnv_ = 0.0f, metalCoef_ = 0.0f;
    float clickEnv_ = 0.0f, clickCoef_ = 0.0f;
    float metalPhase_[6] {};
    int clapIndex_ = 0;
    float nextClap_ = 0.0f;
    dsp::Noise noiseGen_ { 0x1234567u };
    dsp::Svf noiseFilter_, metalFilter_;
    dsp::OnePole metalHp_, clickLp_;
};

} // namespace gristo::sound
