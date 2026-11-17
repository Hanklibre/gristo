#pragma once

#include "Dsp.h"
#include "gristo/sound/Sound.h"

#include <vector>

namespace gristo::sound {

enum class Osc { Saw, Square, Supersaw, Reese, Sine, Fm, Organ, Sync, Voice, String, Bowl, Phase, Trio, Lofi, Fm6 };

// One operator of the six-operator FM: a ratio of the note (or a fixed pitch), its output
// level, its own envelope (seconds; sustain 0..1) and how much velocity reaches it.
struct FmOperator
{
    float ratio = 1.0f, detuneCents = 0.0f, level = 0.0f;
    float a = 0.001f, d = 1.0f, s = 1.0f, r = 0.3f;
    float velocity = 0.5f;
    float fixedHz = 0.0f; // > 0: a fixed frequency instead of a ratio
};

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
    bool fourPole = false; // the saturating ladder (24 dB) instead of the 12 dB state-variable filter
    float a = 0.005f, d = 0.3f, s = 1.0f, r = 0.2f;     // amp envelope
    float fa = 0.001f, fd = 0.3f, fs = 0.0f, fr = 0.2f; // filter envelope
    float vibratoHz = 0.0f, vibratoSemis = 0.0f;
    float drive = 1.0f;
    float level = 1.0f;
    float chorus = 0.0f;   // stereo ensemble amount (applied per synth by the engine)
    float driftCents = 0.0f; // random detune per note, for analog warmth
    float velocityToFm = 0.45f; // how much soft notes darken an FM tone
    bool mono = false;      // basses / leads: a new note reuses the sounding voice (no stacked tails)
    bool freePhase = false; // oscillators start at random phases (pads, strings): no two notes alike
    float tremolo = 0.0f, tremoloHz = 0.0f; // amplitude tremolo (vibes motor, suitcase / rotary)
    bool tremoloPan = false; // stereo: the two sides swap level (suitcase e-piano, rotary speaker)
    float hammer = 0.0f;   // noise burst at the attack: hammer, mallet, finger, organ key click
    float noise = 0.0f;    // steady noise under the tone: breath, bow
    float formantA[3] { 430.0f, 820.0f, 2600.0f }, formantB[3] { 300.0f, 700.0f, 2300.0f }; // Voice: the vowel drifts between the two
    float pitchDrop = 0.0f; // semitones above the note at the attack, falling in ~50 ms (808)
    // Phase distortion: wave 0..7 (saw, square, pulse, double sine, saw-pulse, and three
    // resonant windows), an optional second wave every other cycle (-1 = none), the bend
    // depth (the Cutoff knob and the filter envelope move it), a detuned second line
    // mixed or ring-modulated, and a sine wavefolder.
    int pdWave = 0, pdWave2 = -1;
    float pdDepth = 0.5f, pdEnv = 0.0f, pdDetune = 0.0f, pdFold = 0.0f;
    bool pdRing = false;
    // The filter: 0 low-pass, 1 band-pass, 2 high-pass, 3 notch; or none (the engine shapes its own tone).
    int filterMode = 0;
    bool noFilter = false;
    // TRIO: one of 16 oscillator sets, osc 2 / 3 intervals (semitones), detune (cents), pulse width.
    int trioSet = 0;
    float trioInt2 = 0.0f, trioInt3 = -12.0f, trioDetune = 6.0f, trioPw = 0.5f;
    // LOFI: chip 0 4-bit, 1 4-bit / 2, 2 8-bit, 3 1-bit; wave 0 pulse, 1 triangle, 2 saw, 3 noise, 4 wave RAM;
    // duty / table 0..1, crush, pitch sweep, vibrato, arpeggio (0 off, 1 octave, 2 major, 3 minor), tone.
    int lofiChip = 0, lofiWave = 0, lofiArp = 0;
    float lofiDuty = 0.5f, lofiCrush = 0.0f, lofiSweep = 0.0f, lofiVib = 0.0f, lofiTone = 1.0f;
    // FM6: one of the 32 classic algorithms, feedback 0..1, the six operators (1 first).
    int fmAlg = 0;
    float fmFeedback = 0.0f;
    FmOperator fm[6];
    float stringDamp = 0.3f, stringDecay = 3.0f; // String: 0 = bright .. 0.5 = soft; seconds of ring
};

const SynthPatch& synthPatch(SynthSound sound);

class SynthVoice
{
public:
    static constexpr int kControlRate = 16; // samples between filter / envelope-shape updates
    void prepare(float sampleRate, std::uint32_t seed)
    {
        sampleRate_ = sampleRate;
        drift_ = dsp::Noise(seed);
        string_.assign(static_cast<size_t>(sampleRate / 20.0f) + 8, 0.0f);
    }
    void start(const SynthPatch& patch, const SynthParams& macros, int id, float pitch, float velocity);
    void setAlive(float alive) { alive_ = alive; }
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
    float syncHeld_ = 0.0f;   // hard sync runs one sample late so its reset can be band-limited
    float detuneRatio_[5] {}; // supersaw / reese partial ratios (fixed per patch)
    float fmKeyScale_ = 1.0f; // FM index scaled down for high notes (DX-style level scaling)
    float hammerEnv_ = 0.0f, hammerCoef_ = 0.0f;
    float vibPhase_ = 0.0f, fmEnv_ = 1.0f, fmCoef_ = 0.0f, tineEnv_ = 0.0f, tineCoef_ = 0.0f;
    dsp::Adsr amp_, filterEnv_;
    dsp::Svf filter1_;
    dsp::Ladder ladder_;
    // The cutoff is computed at control rate and glides to it every sample.
    float g_ = 0.0f, gStep_ = 0.0f, k_ = 0.0f;
    bool filterPrimed_ = false;
    dsp::Noise drift_;
    int controlCounter_ = 0;
    // Alive: the pitch and the filter never sit quite still (slow random movement).
    float alive_ = 0.5f, wander_[2] {}, wanderTarget_[2] {}, driftRatio_ = 1.0f, cutoffDrift_ = 1.0f;
    int wanderCounter_ = 0;
    dsp::Svf formant_[3];
    float vowelPhase_ = 0.0f, pitchEnv_ = 0.0f, pitchEnvCoef_ = 0.0f;
    std::vector<float> string_; // plucked string: allocated in prepare
    size_t stringWrite_ = 0;
    float stringPrev_ = 0.0f, stringFb_ = 0.99f, exciteLp_ = 0.0f;
    int excite_ = 0;
    float pdBend_ = 0.0f, pdDetuneRatio_ = 1.0f, foldLast_ = 0.0f, foldOut_ = 0.0f;
    // TRIO
    float trioNoise_ = 0.0f, trioDrift_ = 0.0f, trioRatio_[2] { 1.0f, 1.0f };
    // LOFI
    float lofiHeld_ = 0.0f, lofiLp_ = 0.0f, lofiHoldCount_ = 0.0f, lofiSweepSemis_ = 0.0f, lofiArpTime_ = 0.0f, lofiVibPhase_ = 0.0f, lofiLpCoef_ = 1.0f;
    std::uint32_t lofiLfsr_ = 0x7FFF;
    // FM6
    float fmPhase_[6] {}, opEnv_[6] {}, fmFb_[2] {}, fmLevel_[6] {}, fmDecay_[6] {}, fmRelease_[6] {}, fmAttack_[6] {}, fmTarget_[6] {};
    int fmStage_[6] {};
    float fmBright_ = 1.0f, fmFbAmount_ = 0.0f;
    bool gate_ = false;
    bool pdToggle_[2] {};
};

class Synth
{
public:
    static constexpr int kVoices = 12;
    void prepare(float sampleRate);
    // Poly: every note gets its own voice (chords). Otherwise a note that starts while
    // another is held is a slide (arps).
    void setPoly(bool poly) { poly_ = poly; }
    void setAlive(float alive);
    void reset();
    // Level, cutoff and resonance glide to new settings (no zipper noise when lanes
    // move them block by block); the rest applies at once.
    void setParams(const SynthParams& params);
    float chorus() const { return chorus_; }
    const SynthPatch& patch() const { return patch_ != nullptr ? *patch_ : synthPatch(params_.sound); }
    void noteOn(int id, float pitch, float velocity);
    void noteOff(int id);
    void allNotesOff();
    float tick(); // before the level: see level()
    float level() const { return live_.level; } // the part's fader, glided
    int activeVoices() const;

private:
    SynthVoice voices_[kVoices];
    SynthVoice* last_ = nullptr; // the voice of the latest note (mono patches reuse it)
    const SynthPatch* patch_ = nullptr;
    SynthParams params_, live_;
    float sampleRate_ = 48000.0f;
    float smoothCoef_ = 0.0017f, glideCoef_ = 0.0f, chorus_ = 0.0f; // smoothing: ~12 ms at 48 kHz until prepare()
    bool poly_ = false, started_ = false;
};

} // namespace gristo::sound
