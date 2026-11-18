#pragma once

#include "SynthDsp.h"
#include "gristo/sound/Sound.h"

#include <array>
#include <vector>

namespace gristo::sound {

// The engines a sound is made with.
//   Analog: two oscillators that morph sine - triangle - saw - square (Plaits' variable
//           shape oscillator), unison, hard sync, a sub, noise and PWM, through one of
//           three analog filter models.
//   Fm4:    four-operator FM after the Digitone's FM Tone: operators C, A, B1, B2, eight
//           algorithms with two outputs mixed, harmonics, detune, feedback, and the A
//           and B modulators each on their own envelope; then the filter.
//   The rest are the special ones: drawbar organ, formant voices, a plucked string, a
//   singing bowl, and Felucca's PHASE (phase distortion), TRIO and LOFI.
enum class Osc { Analog, Fm4, Organ, Voice, String, Bowl, Phase, Trio, Lofi };
enum class FilterModel { Ladder, Ota, Sem, None };

struct Ade
{
    float a = 0.001f, d = 0.5f, end = 0.0f; // seconds, seconds, 0..1
};

struct SynthPatch
{
    Osc osc = Osc::Analog;
    // Analog: oscillator 1 (shape -1 sine .. 0 triangle .. 0.5 saw .. 1 square, pw its
    // pulse width), stacked `unison` times `spread` cents apart; oscillator 2 at
    // `interval` semitones and `detune2` cents, mixed in at `osc2`, optionally synced
    // to oscillator 1; a sub an octave down (sine, or square); noise; PWM from a slow
    // LFO (both oscillators' pulse width when square).
    float shape1 = 0.5f, pw = 0.5f, spread = 0.0f, osc1 = 1.0f;
    int unison = 1;
    float shape2 = 0.5f, osc2 = 0.0f, interval = 0.0f, detune2 = 0.0f;
    bool sync = false;
    float osc2Env = 0.0f; // semitones the filter envelope sweeps oscillator 2 (a sync sweep)
    float sub = 0.0f;
    bool subSquare = false;
    float noise = 0.0f, pwmDepth = 0.0f, pwmHz = 0.7f;
    float drive = 1.0f; // into the filter: above 1 it saturates the model
    float hpHz = 15.0f; // a gentle high-pass before the filter (the polysynths' HPF)
    FilterModel filter = FilterModel::Ladder;
    int filterMode = 0; // SEM: 0 low-pass, 1 band-pass, 2 high-pass, 3 notch
    float cutoffHz = 2000.0f, resonance = 0.2f, envOctaves = 0.0f, keyTrack = 0.5f;
    float a = 0.005f, d = 0.3f, s = 1.0f, r = 0.2f;     // amp envelope
    float fa = 0.001f, fd = 0.3f, fs = 0.0f, fr = 0.2f; // filter envelope
    float vibratoHz = 0.0f, vibratoSemis = 0.0f;
    float level = 1.0f;
    float chorus = 0.0f;     // stereo ensemble amount (applied per synth by the engine)
    float driftCents = 2.0f; // the oscillators' slow wandering, and how far voices differ
    bool mono = false;       // basses / leads: a new note reuses the sounding voice (no stacked tails)
    bool freePhase = false;  // oscillators start at random phases (pads, strings): no two notes alike
    float tremolo = 0.0f, tremoloHz = 0.0f; // amplitude tremolo (vibes motor, suitcase / rotary)
    bool tremoloPan = false; // stereo: the two sides swap level (suitcase e-piano, rotary speaker)
    float hammer = 0.0f;     // noise burst at the attack: hammer, mallet, finger, organ key click
    float pitchDrop = 0.0f;  // semitones above the note at the attack, falling in ~50 ms (808)
    float velocityToFilter = 0.5f; // how much harder notes open the filter

    // FM4: algorithm 0..7; ratios of C, A, B1, B2; A's and B's levels (the modulation
    // depth, or loudness when they are heard) on their envelopes; harmonics -1..1
    // (negative: C and A grow odd harmonics; positive: B1 and B2 grow all of them);
    // detune spreads A and B against C; feedback on the top of the algorithm's main path; mix from the
    // algorithm's X output (0) to its Y output (1); how much velocity reaches A and B.
    int fmAlg = 0;
    float ratioC = 1.0f, ratioA = 1.0f, ratioB1 = 2.0f, ratioB2 = 1.0f;
    float levelA = 0.4f, levelB = 0.2f, fmHarm = 0.0f, fmDetune = 0.0f, fmFeedback = 0.0f, fmMix = 0.0f, fmVel = 0.5f;
    Ade envA, envB;

    // Organ / Voice / String / Bowl.
    float organ[6] { 1.0f, 0.7f, 0.45f, 0.3f, 0.18f, 0.12f }; // drawbars: 8', 4', 2 2/3', 2', 1 1/3', 1'
    float formantA[3] { 430.0f, 820.0f, 2600.0f }, formantB[3] { 300.0f, 700.0f, 2300.0f };
    float stringDamp = 0.3f, stringDecay = 3.0f, bowlDecay = 3.0f;
    // PHASE (Felucca): wave 0..7, an optional second wave every other cycle (-1 = none),
    // bend depth (Cutoff and the filter envelope move it), a detuned second line mixed or
    // ring-modulated, a sine wavefolder.
    int pdWave = 0, pdWave2 = -1;
    float pdDepth = 0.5f, pdEnv = 0.0f, pdDetune = 0.0f, pdFold = 0.0f;
    bool pdRing = false;
    // TRIO (Felucca): one of 16 oscillator sets, osc 2 / 3 intervals, detune (cents), pulse width.
    int trioSet = 0;
    float trioInt2 = 0.0f, trioInt3 = -12.0f, trioDetune = 6.0f, trioPw = 0.5f;
    // LOFI (Felucca): chip 0 4-bit, 1 4-bit / 2, 2 8-bit, 3 1-bit; wave 0 pulse, 1 triangle,
    // 2 saw, 3 noise, 4 wave RAM; duty / table, crush, sweep, vibrato, arpeggio, tone.
    int lofiChip = 0, lofiWave = 0, lofiArp = 0;
    float lofiDuty = 0.5f, lofiCrush = 0.0f, lofiSweep = 0.0f, lofiVib = 0.0f, lofiTone = 1.0f;
};

const SynthPatch& synthPatch(SynthSound sound);
// The eight Tone knobs move a sound away from its design (0.5 each = as designed): what
// they do depends on the engine. The labels for the panel.
SynthPatch applyTone(const SynthPatch& patch, const SynthParams& params);

class SynthVoice
{
public:
    static constexpr int kControlRate = 16; // samples between filter / envelope-shape updates
    void prepare(float sampleRate, std::uint32_t seed);
    void start(const SynthPatch& patch, const SynthParams& macros, int id, float pitch, float velocity);
    void retone(const SynthPatch& patch); // the Tone knobs moved while it sounds
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
    float analog(float hz);
    float fm4(float hz);
    float special(float hz);
    void control(const SynthParams& macros);
    void derive(); // the ratios and coefficients that follow from the patch
    float filter(float x);

    SynthPatch patch_;
    float sampleRate_ = 96000.0f;
    int id_ = -1;
    float pitch_ = 60.0f, targetPitch_ = 60.0f, detune_ = 0.0f;
    float hzPitch_ = -1000.0f, hz_ = 0.0f;
    float velocity_ = 1.0f;
    // The voice's own analog tolerances: fixed per voice, as in a real polysynth.
    float tolCutoff_ = 1.0f, tolDetune_ = 0.0f, tolEnv_ = 1.0f, tolOsc2_ = 0.0f;
    // Analog
    std::array<dsp::ShapeOsc, 3> osc1_;
    dsp::ShapeOsc osc2_;
    float subPhase_ = 0.0f, pwmPhase_ = 0.0f, spreadRatio_[3] { 1.0f, 1.0f, 1.0f }, interval_ = 1.0f;
    dsp::OnePole hp_;
    dsp::LadderFilter ladder_;
    dsp::OtaFilter ota_;
    dsp::SemFilter sem_;
    float g_ = 0.0f, gStep_ = 0.0f, reso_ = 0.0f, osc2Sweep_ = 1.0f, spreadJitter_ = 1.0f;
    bool filterPrimed_ = false;
    // FM4
    float opPhase_[4] {}, fb_[2] {}, opRatio_[4] { 1.0f, 1.0f, 1.0f, 1.0f }, fmKeyScale_ = 1.0f, fmBright_ = 1.0f, fmFb_ = 0.0f, fmVelScale_ = 1.0f, opFade_[4] { 1.0f, 1.0f, 1.0f, 1.0f };
    dsp::AdeEnv envA_, envB_;
    // Shared
    dsp::AnalogEnv amp_, filterEnv_;
    dsp::Noise drift_;
    int controlCounter_ = 0;
    float alive_ = 0.5f, wander_[3] {}, wanderTarget_[3] {}, driftRatio_ = 1.0f, drift2Ratio_ = 1.0f, cutoffDrift_ = 1.0f;
    int wanderCounter_ = 0;
    float vibPhase_ = 0.0f, pitchEnv_ = 0.0f, pitchEnvCoef_ = 0.0f, hammerEnv_ = 0.0f, hammerCoef_ = 0.0f;
    bool gate_ = false;
    // Organ / Voice / String / Bowl / PHASE / TRIO / LOFI
    float phase_[5] {}, detuneRatio_[5] {};
    dsp::Svf formant_[3];
    float vowelPhase_ = 0.0f, bowlEnv_ = 1.0f, bowlCoef_ = 0.0f;
    std::vector<float> string_;
    size_t stringWrite_ = 0;
    float stringPrev_ = 0.0f, stringFb_ = 0.99f, exciteLp_ = 0.0f, stringLoopA_ = 1.0f;
    int excite_ = 0;
    float pdBend_ = 0.0f, pdDetuneRatio_ = 1.0f, foldLast_ = 0.0f, foldOut_ = 0.0f;
    bool pdToggle_[2] {};
    float trioNoise_ = 0.0f, trioDrift_ = 0.0f, trioRatio_[2] { 1.0f, 1.0f };
    float lofiHeld_ = 0.0f, lofiLp_ = 0.0f, lofiHoldCount_ = 0.0f, lofiSweepSemis_ = 0.0f, lofiArpTime_ = 0.0f, lofiVibPhase_ = 0.0f, lofiLpCoef_ = 1.0f;
    std::uint32_t lofiLfsr_ = 0x7FFF;
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
    const SynthPatch& patch() const { return patch_; }
    void noteOn(int id, float pitch, float velocity);
    void noteOff(int id);
    void allNotesOff();
    float tick(); // one host-rate sample, before the level: see level()
    float level() const { return live_.level; } // the part's fader, glided
    int activeVoices() const;

private:
    SynthVoice voices_[kVoices];
    SynthVoice* last_ = nullptr; // the voice of the latest note (mono patches reuse it)
    SynthPatch patch_;           // the sound with the Tone knobs applied
    SynthParams params_, live_;
    dsp::Decimator2 decimator_;
    float sampleRate_ = 48000.0f;
    float smoothCoef_ = 0.0017f, glideCoef_ = 0.0f, chorus_ = 0.0f;
    bool poly_ = false, started_ = false;
};

} // namespace gristo::sound
