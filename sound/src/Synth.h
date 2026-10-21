#pragma once

#include "Dsp.h"
#include "gristo/sound/Sound.h"

namespace gristo::sound {

enum class Osc { Saw, Square, Supersaw, Reese, Sine, Fm, Organ, Sync };

struct SynthPatch
{
    Osc osc = Osc::Saw;
    float detuneCents = 0.0f; // supersaw / reese spread
    float sub = 0.0f;         // sine one octave down
    float pulseWidth = 0.5f;
    float fmRatio = 1.0f, fmIndex = 0.0f, fmIndexEnd = 0.0f, fmIndexTime = 0.3f; // index decays to fmIndexEnd
    float tine = 0.0f;        // e-piano attack partial
    float syncAmount = 0.0f;  // sync lead: slave ratio sweep (driven by the filter envelope)
    float cutoffHz = 2000.0f, resonance = 0.2f, envOctaves = 0.0f, keyTrack = 0.5f;
    bool fourPole = false;
    float a = 0.005f, d = 0.3f, s = 1.0f, r = 0.2f;     // amp envelope
    float fa = 0.001f, fd = 0.3f, fs = 0.0f, fr = 0.2f; // filter envelope
    float vibratoHz = 0.0f, vibratoSemis = 0.0f;
    float drive = 1.0f;
    float level = 1.0f;
    float chorus = 0.0f;   // stereo ensemble amount (applied per synth by the engine)
    float driftCents = 0.0f; // random detune per note, for analog warmth
    float velocityToFm = 0.45f; // how much soft notes darken an FM tone
};

const SynthPatch& synthPatch(SynthSound sound);

class SynthVoice
{
public:
    void prepare(float sampleRate, std::uint32_t seed)
    {
        sampleRate_ = sampleRate;
        drift_ = dsp::Noise(seed);
    }
    void start(const SynthPatch& patch, const SynthParams& macros, int id, float pitch, float velocity);
    void legato(int id, float pitch, float velocity); // glide to a new pitch, no retrigger
    void release(const SynthParams& macros);
    void kill() { amp_.kill(); }
    bool active() const { return amp_.active(); }
    bool gated() const { return amp_.gated(); }
    int id() const { return id_; }
    float level() const { return amp_.value(); }
    float tick(const SynthParams& macros, float glideCoef);

private:
    float oscillator(float hz);
    void updateFilter(const SynthParams& macros, float hz);

    SynthPatch patch_;
    float sampleRate_ = 48000.0f;
    int id_ = -1;
    float pitch_ = 60.0f, targetPitch_ = 60.0f, detune_ = 0.0f;
    float hzPitch_ = -1000.0f, hz_ = 0.0f; // midiToHz cache: only glides and vibrato recompute it
    float velocity_ = 1.0f;
    float phase_[5] {}, subPhase_ = 0.0f, modPhase_ = 0.0f, tinePhase_ = 0.0f, syncPhase_ = 0.0f;
    float vibPhase_ = 0.0f, fmEnv_ = 1.0f, fmCoef_ = 0.0f, tineEnv_ = 0.0f, tineCoef_ = 0.0f;
    dsp::Adsr amp_, filterEnv_;
    dsp::Svf filter1_, filter2_;
    dsp::Noise drift_;
    int controlCounter_ = 0;
};

class Synth
{
public:
    static constexpr int kVoices = 12;
    void prepare(float sampleRate);
    // Poly: every note gets its own voice (chords). Otherwise a note that starts while
    // another is held is a slide (arps).
    void setPoly(bool poly) { poly_ = poly; }
    void reset();
    // Level, cutoff and resonance glide to new settings (no zipper noise when lanes
    // move them block by block); the rest applies at once.
    void setParams(const SynthParams& params);
    float chorus() const { return chorus_; }
    void noteOn(int id, float pitch, float velocity);
    void noteOff(int id);
    void allNotesOff();
    float tick();
    int activeVoices() const;

private:
    SynthVoice voices_[kVoices];
    SynthParams params_, live_;
    float sampleRate_ = 48000.0f;
    float smoothCoef_ = 0.0017f, glideCoef_ = 0.0f, chorus_ = 0.0f; // smoothing: ~12 ms at 48 kHz until prepare()
    bool poly_ = false, started_ = false;
};

} // namespace gristo::sound
