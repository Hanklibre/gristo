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
};

const SynthPatch& synthPatch(SynthSound sound);

class SynthVoice
{
public:
    void prepare(float sampleRate) { sampleRate_ = sampleRate; }
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
    float pitch_ = 60.0f, targetPitch_ = 60.0f;
    float velocity_ = 1.0f;
    float phase_[5] {}, subPhase_ = 0.0f, modPhase_ = 0.0f, tinePhase_ = 0.0f, syncPhase_ = 0.0f;
    float vibPhase_ = 0.0f, fmEnv_ = 1.0f, fmCoef_ = 0.0f, tineEnv_ = 0.0f, tineCoef_ = 0.0f;
    dsp::Adsr amp_, filterEnv_;
    dsp::Svf filter1_, filter2_;
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
    void setParams(const SynthParams& params) { params_ = params; }
    void noteOn(int id, float pitch, float velocity);
    void noteOff(int id);
    void allNotesOff();
    float tick();
    int activeVoices() const;

private:
    SynthVoice voices_[kVoices];
    SynthParams params_;
    float sampleRate_ = 48000.0f;
    bool poly_ = false;
};

} // namespace gristo::sound
