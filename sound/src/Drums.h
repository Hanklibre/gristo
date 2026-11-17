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
    float bodyHold = 0.0f; // seconds at full level before the body decays (kick punch)
    float body2 = 0.0f, body2Ratio = 1.6f; // a second partial (snare / timbale ring)
    // Filtered noise, optionally as hand-clap bursts before the tail.
    float noise = 0.0f, noiseAttack = 0.0f, noiseDecay = 0.1f, noiseHz = 5000.0f, noiseQ = 0.2f;
    NoiseFilter noiseFilter = NoiseFilter::Highpass;
    int claps = 0;
    float clapSpacing = 0.010f;
    // 808-style metallic layer: six detuned squares through a bandpass.
    float metal = 0.0f, metalDecay = 0.1f, metalHz = 8000.0f;
    // Acoustic drum model: a struck head as a handful of decaying modes (the high ones
    // quieter and shorter), whose pitch sags as the hit dies away; a resonant head tuned
    // against it that rings longer; snare wires that buzz for as long as that head moves;
    // and the shell's own ring.
    int headModes = 0;          // 0 = off, up to 5
    float head = 0.0f, headHz = 120.0f, headDecay = 0.5f;
    float tension = 0.0f;       // semitones the pitch starts above the note
    float reso = 0.0f, resoInterval = 0.0f; // resonant head: level, semitones from the batter head
    float wires = 0.0f;         // snare wires under the resonant head
    float shell = 0.0f, shellHz = 600.0f;
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
    // `alive` (0..1) is how much one hit differs from the next.
    void trigger(const DrumPatch& patch, float velocity, float tuneSemis, float decayScale, float alive = 0.5f);
    void choke() { choke_ = true; }
    bool active() const { return active_; }
    // Mono sample; the caller pans.
    float tick();
    float pan() const { return patch_.pan; }

private:
    DrumPatch patch_;
    float sampleRate_ = 48000.0f;
    bool active_ = false, choke_ = false;
    float chokeGain_ = 1.0f, chokeCoef_ = 0.995f;
    float velocity_ = 0.0f;
    float t_ = 0.0f; // seconds since trigger
    float lastOut_ = 0.0f, declick_ = 0.0f, declickCoef_ = 0.0f; // a retrigger fades the old tail out

    float phase_ = 0.0f, phase2_ = 0.0f, freq_ = 0.0f;
    float bodyEnv_ = 0.0f, bodyCoef_ = 0.0f, pitchCoef_ = 0.0f;
    float noiseEnv_ = 0.0f, noiseCoef_ = 0.0f;
    float metalEnv_ = 0.0f, metalCoef_ = 0.0f;
    float clickEnv_ = 0.0f, clickCoef_ = 0.0f;
    float metalPhase_[6] {};
    int clapIndex_ = 0;
    float nextClap_ = 0.0f;
    static constexpr int kMaxModes = 5;
    float headPhase_[2][kMaxModes] {}, headEnv_[2][kMaxModes] {}, headCoef_[2][kMaxModes] {}, headGain_[kMaxModes] {};
    float energy_ = 0.0f, energyCoef_ = 0.0f, wireEnv_ = 0.0f, wireAttack_ = 0.0f, wireRelease_ = 0.0f;
    float shellPhase_ = 0.0f, shellFm_ = 0.0f, shellEnv_ = 0.0f, shellCoef_ = 0.0f;
    dsp::Svf wireFilter_;
    dsp::OnePole wireLp_;
    dsp::Noise noiseGen_ { 0x1234567u };
    dsp::Svf noiseFilter_, metalFilter_;
    dsp::OnePole metalHp_, clickLp_, velocityTone_;
    bool velocityToneOn_ = false;
};

} // namespace gristo::sound
