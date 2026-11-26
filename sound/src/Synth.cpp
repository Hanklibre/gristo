#include "Synth.h"

#include <algorithm>
#include <cmath>

namespace gristo::sound {

namespace {

// ---------------------------------------------------------------------------------
// The sounds.

// Each sound's level trim, so they sit at about the same loudness (measured: RMS of a
// chord and of a bass line, see tests/SoundTests.cpp). In SynthSound order.
constexpr float kLoudnessTrim[static_cast<int>(SynthSound::Count)] = {
    0.95f, 1.70f, 1.35f, 1.23f, 0.79f, 0.65f, 1.46f, 0.77f, 0.83f, 0.84f, 0.91f, 0.47f,
    0.88f, 0.60f, 0.69f, 0.89f, 0.56f, 0.84f, 0.84f, 0.91f, 0.94f, 0.79f, 0.59f, 0.67f,
    1.64f, 0.60f, 0.66f, 0.65f, 0.84f, 1.16f, 2.45f, 1.95f, 1.11f, 1.38f, 0.80f, 5.13f,
    0.44f, 1.53f, 0.47f, 1.19f, 0.98f, 0.51f, 0.49f, 1.88f, 0.50f, 1.05f, 1.19f, 1.29f,
    0.65f, 1.07f, 0.72f, 0.90f, 0.61f, 1.64f, 1.70f, 0.79f, 1.11f, 1.01f,
};

SynthPatch& analog(SynthPatch& x, float shape, float cutoffHz, float reso, FilterModel model)
{
    x.osc = Osc::Analog;
    x.shape1 = shape;
    x.cutoffHz = cutoffHz;
    x.resonance = reso;
    x.filter = model;
    return x;
}

SynthPatch& fm(SynthPatch& x, int alg, float c, float a, float b1, float b2)
{
    x.osc = Osc::Fm4;
    x.fmAlg = alg;
    x.ratioC = c;
    x.ratioA = a;
    x.ratioB1 = b1;
    x.ratioB2 = b2;
    x.filter = FilterModel::Sem;
    x.cutoffHz = 12000.0f;
    x.resonance = 0.05f;
    x.keyTrack = 0.0f;
    x.driftCents = 0.5f;
    return x;
}

void amp(SynthPatch& x, float a, float d, float s, float r)
{
    x.a = a;
    x.d = d;
    x.s = s;
    x.r = r;
}

void fenv(SynthPatch& x, float octaves, float a, float d, float s, float r)
{
    x.envOctaves = octaves;
    x.fa = a;
    x.fd = d;
    x.fs = s;
    x.fr = r;
}

const SynthPatch* buildPatches()
{
    static SynthPatch p[static_cast<int>(SynthSound::Count)];
    auto set = [](SynthSound s) -> SynthPatch& { return p[static_cast<int>(s)]; };
    using F = FilterModel;

    // ---- Basses
    { // Sub: a sine through the ladder, just warm enough to read on small speakers.
        auto& x = analog(set(SynthSound::SubBass), -1.0f, 900.0f, 0.0f, F::Ladder);
        x.keyTrack = 0.3f; x.drive = 1.5f; amp(x, 0.003f, 0.4f, 0.9f, 0.08f); x.mono = true; x.driftCents = 0.5f;
    }
    { // Acid: a saw into the resonant ladder, the envelope snapping it open; accents squelch.
        auto& x = analog(set(SynthSound::AcidBass), 0.5f, 330.0f, 0.86f, F::Ladder);
        x.keyTrack = 0.35f; x.drive = 1.8f; fenv(x, 4.3f, 0.001f, 0.2f, 0.0f, 0.1f);
        amp(x, 0.002f, 0.6f, 0.85f, 0.05f); x.mono = true; x.velocityToFilter = 0.8f; x.driftCents = 1.0f;
    }
    { // Reese: two saws a few cents apart beating against each other, a sine sub, the OTA filter.
        auto& x = analog(set(SynthSound::ReeseBass), 0.5f, 700.0f, 0.18f, F::Ota);
        x.unison = 2; x.spread = 17.0f; x.sub = 0.45f; x.drive = 1.5f; fenv(x, 1.0f, 0.01f, 0.8f, 0.4f, 0.3f);
        amp(x, 0.005f, 0.5f, 1.0f, 0.15f); x.mono = true; x.freePhase = true;
    }
    { // FM bass: a 1:1 pair that bites and settles, a little feedback for growl.
        auto& x = fm(set(SynthSound::FmBass), 1, 1.0f, 1.0f, 1.0f, 1.0f);
        x.levelA = 0.62f; x.envA = { 0.001f, 0.25f, 0.22f }; x.levelB = 0.0f; x.fmFeedback = 0.15f;
        x.sub = 0.25f; x.cutoffHz = 5000.0f; x.keyTrack = 0.4f; amp(x, 0.002f, 0.5f, 0.65f, 0.1f); x.mono = true;
    }
    { // 808: a sine that drops onto its note and drives the ladder.
        auto& x = analog(set(SynthSound::Bass808), -1.0f, 1300.0f, 0.0f, F::Ladder);
        x.pitchDrop = 12.0f; x.keyTrack = 0.0f; x.drive = 3.2f; amp(x, 0.002f, 3.5f, 0.0f, 0.3f); x.mono = true; x.driftCents = 0.0f;
    }
    { // Upright: a woody 1:1 pluck with a finger thump that dies fast.
        auto& x = fm(set(SynthSound::UprightBass), 2, 1.0f, 1.0f, 3.0f, 1.0f);
        x.levelA = 0.5f; x.envA = { 0.001f, 0.14f, 0.06f }; x.levelB = 0.22f; x.envB = { 0.001f, 0.05f, 0.0f };
        x.hammer = 0.12f; x.cutoffHz = 1300.0f; x.keyTrack = 0.35f; amp(x, 0.004f, 1.2f, 0.0f, 0.12f); x.mono = true;
    }
    { // DX bass: a stack with feedback on top, the classic solid FM bass.
        auto& x = fm(set(SynthSound::DxBass), 0, 1.0f, 1.0f, 1.0f, 1.0f);
        x.levelA = 0.6f; x.envA = { 0.001f, 0.35f, 0.3f }; x.levelB = 0.42f; x.envB = { 0.001f, 0.12f, 0.05f };
        x.fmFeedback = 0.35f; x.cutoffHz = 6000.0f; x.keyTrack = 0.3f; amp(x, 0.001f, 1.0f, 0.8f, 0.1f); x.mono = true;
    }
    { // Mono bass: a three-oscillator monosynth bass: two saws and a square an octave down, the ladder.
        auto& x = analog(set(SynthSound::MonoBass), 0.5f, 520.0f, 0.35f, F::Ladder);
        x.osc2 = 0.8f; x.shape2 = 1.0f; x.interval = -12.0f; x.detune2 = 5.0f; x.sub = 0.25f; x.subSquare = true;
        x.drive = 1.7f; x.keyTrack = 0.4f; fenv(x, 2.8f, 0.002f, 0.32f, 0.15f, 0.12f);
        amp(x, 0.003f, 0.8f, 0.8f, 0.08f); x.mono = true; x.driftCents = 1.5f;
    }

    // ---- Keys
    { // E-piano: the bark of a 1:1 pair and a 14:1 tine on the attack; harder notes bark more.
        auto& x = fm(set(SynthSound::EPiano), 2, 1.0f, 1.0f, 14.0f, 1.0f);
        x.levelA = 0.48f; x.envA = { 0.001f, 1.3f, 0.08f }; x.levelB = 0.17f; x.envB = { 0.001f, 0.22f, 0.0f };
        x.fmVel = 0.8f; x.cutoffHz = 8000.0f; amp(x, 0.002f, 3.0f, 0.0f, 0.4f); x.hammer = 0.04f; x.chorus = 0.35f;
    }
    { // Bell: an inharmonic pair and a second, detuned one: they beat as it rings.
        auto& x = fm(set(SynthSound::Bell), 1, 1.0f, 3.5f, 2.0f, 5.4f);
        x.levelA = 0.6f; x.envA = { 0.001f, 2.2f, 0.06f }; x.levelB = 0.35f; x.envB = { 0.001f, 1.6f, 0.0f };
        x.fmMix = 0.35f; x.fmDetune = 0.35f; amp(x, 0.001f, 4.0f, 0.0f, 2.0f); x.chorus = 0.2f; x.hammer = 0.02f;
    }
    { // Pluck: a saw and a pulse an octave up, a fast OTA filter decay.
        auto& x = analog(set(SynthSound::Pluck), 0.5f, 320.0f, 0.3f, F::Ota);
        x.osc2 = 0.35f; x.shape2 = 0.85f; x.interval = 12.0f; x.detune2 = 4.0f; fenv(x, 5.0f, 0.001f, 0.24f, 0.0f, 0.2f);
        amp(x, 0.001f, 0.6f, 0.0f, 0.25f); x.chorus = 0.25f;
    }
    { // Organ: drawbars, a little vibrato and the rotary speaker.
        auto& x = set(SynthSound::Organ);
        x.osc = Osc::Organ; x.filter = F::Sem; x.cutoffHz = 9000.0f; x.keyTrack = 0.0f; x.drive = 1.2f;
        amp(x, 0.004f, 0.1f, 1.0f, 0.06f); x.vibratoHz = 6.2f; x.vibratoSemis = 0.05f; x.hammer = 0.04f;
        x.tremolo = 0.2f; x.tremoloHz = 6.2f; x.tremoloPan = true; x.chorus = 0.2f; // the rotary speaker
    }
    { // Felt piano: a soft, rounded tone with the hammer muffled in felt.
        auto& x = fm(set(SynthSound::FeltPiano), 3, 1.0f, 1.0f, 2.0f, 1.0f);
        x.levelA = 0.36f; x.envA = { 0.003f, 0.55f, 0.05f }; x.levelB = 0.12f; x.envB = { 0.002f, 0.3f, 0.0f };
        x.fmVel = 0.7f; x.cutoffHz = 2400.0f; x.keyTrack = 0.4f; amp(x, 0.006f, 3.5f, 0.0f, 0.9f); x.hammer = 0.1f; x.chorus = 0.15f;
    }
    { // Celesta: a glassy 4:1 shimmer that fades fast into a pure tone.
        auto& x = fm(set(SynthSound::Celesta), 1, 1.0f, 4.0f, 2.0f, 7.0f);
        x.levelA = 0.36f; x.envA = { 0.001f, 0.4f, 0.0f }; x.levelB = 0.12f; x.envB = { 0.001f, 0.3f, 0.0f };
        x.fmMix = 0.25f; amp(x, 0.002f, 2.2f, 0.0f, 1.2f); x.chorus = 0.25f;
    }
    { // Tape keys: an e-piano gone soft and wobbly on tape.
        auto& x = fm(set(SynthSound::TapeKeys), 2, 1.0f, 1.0f, 14.0f, 1.0f);
        x.levelA = 0.4f; x.envA = { 0.002f, 1.0f, 0.1f }; x.levelB = 0.1f; x.envB = { 0.001f, 0.2f, 0.0f };
        x.cutoffHz = 3200.0f; x.vibratoHz = 0.6f; x.vibratoSemis = 0.12f; x.drive = 1.4f; amp(x, 0.003f, 2.2f, 0.0f, 0.5f);
        x.hammer = 0.05f; x.chorus = 0.3f;
    }
    { // Suitcase keys: brighter and barkier, with the stereo tremolo.
        auto& x = fm(set(SynthSound::SuitcaseKeys), 2, 1.0f, 1.0f, 14.0f, 1.0f);
        x.levelA = 0.6f; x.envA = { 0.001f, 0.9f, 0.12f }; x.levelB = 0.22f; x.envB = { 0.001f, 0.25f, 0.0f };
        x.fmVel = 0.85f; x.cutoffHz = 7000.0f; x.drive = 1.3f; amp(x, 0.002f, 2.6f, 0.0f, 0.45f); x.hammer = 0.07f;
        x.tremolo = 0.45f; x.tremoloHz = 4.6f; x.tremoloPan = true; x.chorus = 0.2f;
    }
    { // Vibraphone: a mallet's 4:1 attack and the motor's tremolo.
        auto& x = fm(set(SynthSound::Vibraphone), 1, 1.0f, 4.0f, 10.0f, 1.0f);
        x.levelA = 0.3f; x.envA = { 0.001f, 0.25f, 0.0f }; x.levelB = 0.15f; x.envB = { 0.001f, 0.08f, 0.0f };
        x.fmMix = 0.1f; amp(x, 0.002f, 3.0f, 0.0f, 1.0f); x.hammer = 0.05f; x.tremolo = 0.3f; x.tremoloHz = 5.5f; x.chorus = 0.15f;
    }
    { // Jazz organ: drawbars pulled for a percussive, gritty tone and a fast rotary.
        auto& x = set(SynthSound::JazzOrgan);
        x.osc = Osc::Organ; x.filter = F::Sem; x.cutoffHz = 5000.0f; x.keyTrack = 0.0f; x.drive = 1.9f;
        const float bars[6] { 1.0f, 0.8f, 0.5f, 0.15f, 0.1f, 0.3f };
        std::copy(bars, bars + 6, x.organ);
        amp(x, 0.003f, 0.1f, 1.0f, 0.05f); x.vibratoHz = 6.8f; x.vibratoSemis = 0.08f; x.hammer = 0.06f;
        x.tremolo = 0.3f; x.tremoloHz = 6.8f; x.tremoloPan = true; x.chorus = 0.3f;
    }
    { // FM Rhodes: a richer tine piano: the bark pair, and the tine ringing on its own pair.
        auto& x = fm(set(SynthSound::FmRhodes), 1, 1.0f, 1.0f, 1.0f, 14.0f);
        x.levelA = 0.5f; x.envA = { 0.001f, 1.2f, 0.1f }; x.levelB = 0.3f; x.envB = { 0.001f, 0.25f, 0.02f };
        x.fmMix = 0.42f; x.fmDetune = 0.25f; x.fmVel = 0.8f; amp(x, 0.001f, 3.2f, 0.0f, 0.5f); x.chorus = 0.35f; x.hammer = 0.03f;
    }
    { // FM bells: one modulator fanned out to three carriers, all a little apart: long and beating.
        auto& x = fm(set(SynthSound::FmBells), 4, 1.0f, 2.0f, 1.0f, 3.5f);
        x.levelA = 0.45f; x.envA = { 0.001f, 4.0f, 0.0f }; x.levelB = 0.5f; x.envB = { 0.001f, 3.0f, 0.05f };
        x.fmMix = 0.5f; x.fmDetune = 0.45f; amp(x, 0.001f, 8.0f, 0.0f, 3.0f); x.chorus = 0.2f;
    }
    { // FM marimba: a hollow wooden knock that dies at once.
        auto& x = fm(set(SynthSound::FmMarimba), 1, 1.0f, 4.0f, 3.0f, 10.0f);
        x.levelA = 0.45f; x.envA = { 0.001f, 0.05f, 0.0f }; x.levelB = 0.2f; x.envB = { 0.001f, 0.03f, 0.0f };
        x.fmMix = 0.3f; amp(x, 0.001f, 1.0f, 0.0f, 0.3f); x.hammer = 0.03f;
    }
    { // FM pluck: a bright, tight Digitone-style pluck with the modulators closing fast.
        auto& x = fm(set(SynthSound::FmPluck), 0, 1.0f, 2.0f, 1.0f, 3.0f);
        x.levelA = 0.55f; x.envA = { 0.001f, 0.18f, 0.0f }; x.levelB = 0.35f; x.envB = { 0.001f, 0.08f, 0.0f };
        x.fmHarm = 0.25f; x.fmFeedback = 0.15f; x.fmVel = 0.7f; amp(x, 0.001f, 0.9f, 0.0f, 0.3f); x.chorus = 0.25f;
    }

    // ---- Pads
    { // Warm pad: stacked saws and a pulse with PWM, the OTA filter swelling, the ensemble.
        auto& x = analog(set(SynthSound::WarmPad), 0.5f, 1350.0f, 0.15f, F::Ota);
        x.unison = 2; x.spread = 11.0f; x.osc2 = 0.6f; x.shape2 = 0.9f; x.detune2 = 6.0f; x.pwmDepth = 0.5f; x.pwmHz = 0.6f;
        fenv(x, 1.0f, 0.9f, 2.0f, 0.5f, 1.5f); amp(x, 0.7f, 1.0f, 0.85f, 1.6f); x.chorus = 0.65f; x.freePhase = true; x.driftCents = 4.0f;
    }
    { // Glass pad: a steady 3:1 shimmer and a second pair high up.
        auto& x = fm(set(SynthSound::GlassPad), 1, 1.0f, 3.0f, 2.0f, 4.0f);
        x.levelA = 0.3f; x.envA = { 1.0f, 3.0f, 0.3f }; x.levelB = 0.15f; x.envB = { 1.4f, 3.0f, 0.12f };
        x.fmMix = 0.3f; x.fmDetune = 0.3f; x.vibratoHz = 5.0f; x.vibratoSemis = 0.04f; amp(x, 0.9f, 1.0f, 0.9f, 2.0f);
        x.chorus = 0.45f; x.freePhase = true;
    }
    { // Dark pad: low saws and a sub through a slow ladder.
        auto& x = analog(set(SynthSound::DarkPad), 0.5f, 450.0f, 0.3f, F::Ladder);
        x.unison = 2; x.spread = 9.0f; x.sub = 0.3f; fenv(x, 0.8f, 2.5f, 3.0f, 0.6f, 2.0f);
        amp(x, 1.2f, 1.0f, 0.9f, 2.5f); x.chorus = 0.5f; x.freePhase = true; x.driftCents = 3.5f;
    }
    { // Breath pad: an airy 2:1 FM tone with breath noise and a slow vibrato.
        auto& x = fm(set(SynthSound::BreathPad), 1, 1.0f, 2.0f, 1.0f, 1.0f);
        x.levelA = 0.26f; x.envA = { 1.2f, 3.0f, 0.25f }; x.levelB = 0.0f; x.noise = 0.1f; x.cutoffHz = 2200.0f; x.resonance = 0.15f;
        x.keyTrack = 0.3f; x.vibratoHz = 4.5f; x.vibratoSemis = 0.07f; amp(x, 1.5f, 1.0f, 0.9f, 2.2f); x.chorus = 0.5f;
    }
    { // Slow strings: a soft ensemble of saws swelling in, a little bow noise.
        auto& x = analog(set(SynthSound::SlowStrings), 0.5f, 2600.0f, 0.1f, F::Ota);
        x.unison = 3; x.spread = 12.0f; x.noise = 0.015f; fenv(x, 0.5f, 1.2f, 2.0f, 0.7f, 2.0f);
        x.vibratoHz = 5.5f; x.vibratoSemis = 0.05f; amp(x, 1.2f, 1.0f, 0.9f, 2.0f); x.chorus = 0.7f; x.freePhase = true; x.driftCents = 4.0f;
    }
    { // Poly strings: a bright string machine with octave strings and a deep ensemble.
        auto& x = analog(set(SynthSound::PolyStrings), 0.5f, 3500.0f, 0.05f, F::Ota);
        x.unison = 3; x.spread = 7.0f; x.osc2 = 0.4f; x.interval = 12.0f; x.noise = 0.01f;
        x.vibratoHz = 5.8f; x.vibratoSemis = 0.04f; amp(x, 0.25f, 1.0f, 0.9f, 0.8f); x.chorus = 0.85f; x.freePhase = true; x.driftCents = 3.0f;
    }
    { // PWM pad: one pulse wave breathing under a slow PWM and a deep chorus (the 80s polysynth pad).
        auto& x = analog(set(SynthSound::PwmPad), 1.0f, 1700.0f, 0.12f, F::Ota);
        x.pw = 0.5f; x.pwmDepth = 0.85f; x.pwmHz = 0.45f; x.sub = 0.15f; x.subSquare = true; x.hpHz = 90.0f;
        fenv(x, 0.6f, 0.6f, 2.0f, 0.6f, 1.5f); amp(x, 0.5f, 1.0f, 0.9f, 1.8f); x.chorus = 0.9f; x.freePhase = true; x.driftCents = 3.0f;
    }
    { // SEM pad: two saws through the band-pass, resonance singing, a slow sweep.
        auto& x = analog(set(SynthSound::SemPad), 0.5f, 900.0f, 0.55f, F::Sem);
        x.filterMode = 1; x.osc2 = 0.8f; x.detune2 = 9.0f; fenv(x, 1.5f, 1.8f, 3.5f, 0.4f, 2.0f);
        amp(x, 0.9f, 1.0f, 0.9f, 2.0f); x.chorus = 0.5f; x.freePhase = true; x.driftCents = 4.0f;
    }
    { // FM pad: a warm, slowly opening FM pad with the modulators drifting apart.
        auto& x = fm(set(SynthSound::FmPad), 5, 1.0f, 1.0f, 2.0f, 1.0f);
        x.levelA = 0.35f; x.envA = { 1.5f, 4.0f, 0.3f }; x.levelB = 0.25f; x.envB = { 2.0f, 4.0f, 0.2f };
        x.fmMix = 0.4f; x.fmDetune = 0.5f; x.fmHarm = -0.2f; x.cutoffHz = 5000.0f; amp(x, 1.0f, 1.0f, 0.9f, 2.5f);
        x.chorus = 0.6f; x.freePhase = true;
    }
    { // FM choir: a vowel-like FM tone, odd harmonics on the carriers, a slow vibrato.
        auto& x = fm(set(SynthSound::FmChoir), 2, 1.0f, 2.0f, 1.0f, 3.0f);
        x.levelA = 0.22f; x.envA = { 0.8f, 3.0f, 0.2f }; x.levelB = 0.18f; x.envB = { 0.9f, 3.0f, 0.15f };
        x.fmMix = 0.35f; x.fmHarm = -0.45f; x.fmDetune = 0.3f; x.cutoffHz = 3500.0f; x.vibratoHz = 5.0f; x.vibratoSemis = 0.08f;
        amp(x, 0.7f, 1.0f, 0.9f, 2.0f); x.chorus = 0.6f; x.noise = 0.02f;
    }
    { // FM glass: harmonic and odd pairs fading in slowly, with feedback shimmer.
        auto& x = fm(set(SynthSound::FmGlass), 1, 1.0f, 2.0f, 3.0f, 5.0f);
        x.levelA = 0.22f; x.envA = { 1.5f, 3.0f, 0.6f }; x.levelB = 0.14f; x.envB = { 1.6f, 3.0f, 0.5f };
        x.fmMix = 0.4f; x.fmDetune = 0.4f; x.fmFeedback = 0.15f; amp(x, 0.3f, 1.0f, 1.0f, 2.5f); x.chorus = 0.6f; x.freePhase = true;
    }

    // ---- Leads
    { // Square lead: a hollow square through the ladder, vibrato.
        auto& x = analog(set(SynthSound::SquareLead), 1.0f, 2600.0f, 0.25f, F::Ladder);
        fenv(x, 1.5f, 0.002f, 0.3f, 0.4f, 0.2f); x.vibratoHz = 5.5f; x.vibratoSemis = 0.12f;
        amp(x, 0.005f, 0.3f, 0.9f, 0.15f); x.mono = true;
    }
    { // Sync lead: a saw hard-synced to the note, the envelope sweeping it.
        auto& x = analog(set(SynthSound::SyncLead), 0.5f, 5000.0f, 0.15f, F::Ladder);
        x.osc1 = 0.3f; x.osc2 = 1.0f; x.shape2 = 0.5f; x.interval = 7.0f; x.sync = true; x.osc2Env = 19.0f;
        fenv(x, 0.6f, 0.001f, 0.45f, 0.3f, 0.2f); amp(x, 0.003f, 0.3f, 0.85f, 0.12f); x.mono = true;
    }
    { // Soft lead: a sine with a touch of FM and a singing vibrato.
        auto& x = fm(set(SynthSound::SoftLead), 1, 1.0f, 1.0f, 1.0f, 1.0f);
        x.levelA = 0.22f; x.envA = { 0.02f, 1.0f, 0.18f }; x.levelB = 0.0f; x.vibratoHz = 5.0f; x.vibratoSemis = 0.1f;
        amp(x, 0.03f, 0.3f, 0.9f, 0.25f); x.mono = true;
    }

    // ---- Brass
    { // Synth brass: two saws, the OTA filter blooming on the attack.
        auto& x = analog(set(SynthSound::SynthBrass), 0.5f, 600.0f, 0.15f, F::Ota);
        x.unison = 2; x.spread = 7.0f; x.noise = 0.02f; x.drive = 1.4f; fenv(x, 3.0f, 0.03f, 0.35f, 0.4f, 0.15f);
        amp(x, 0.02f, 0.4f, 0.8f, 0.15f); x.chorus = 0.35f; x.freePhase = true; x.driftCents = 3.5f;
    }
    { // Poly synth: a saw and a square a few cents apart, a short filter movement.
        auto& x = analog(set(SynthSound::PolySynth), 0.5f, 1800.0f, 0.2f, F::Ota);
        x.osc2 = 0.75f; x.shape2 = 1.0f; x.detune2 = 7.0f; fenv(x, 1.5f, 0.005f, 0.6f, 0.3f, 0.4f);
        amp(x, 0.005f, 1.0f, 0.6f, 0.5f); x.chorus = 0.5f; x.freePhase = true; x.driftCents = 3.0f;
    }
    { // FM brass: the modulators swell in after the carrier, feedback for edge.
        auto& x = fm(set(SynthSound::FmBrass), 0, 1.0f, 1.0f, 1.0f, 1.0f);
        x.levelA = 0.55f; x.envA = { 0.08f, 1.0f, 0.45f }; x.levelB = 0.3f; x.envB = { 0.12f, 1.0f, 0.3f };
        x.fmFeedback = 0.5f; x.fmDetune = 0.2f; x.cutoffHz = 3800.0f; fenv(x, 1.2f, 0.08f, 0.6f, 0.5f, 0.2f);
        amp(x, 0.05f, 2.0f, 0.85f, 0.25f); x.chorus = 0.3f;
    }

    // ---- The special engines
    { // Monk choir: low voices on "om", the vowel slowly closing and opening.
        auto& x = set(SynthSound::MonkChoir);
        x.osc = Osc::Voice; x.spread = 9.0f; x.filter = F::Sem; x.cutoffHz = 6000.0f; x.keyTrack = 0.0f;
        amp(x, 0.5f, 1.0f, 0.9f, 1.8f); x.vibratoHz = 4.8f; x.vibratoSemis = 0.07f;
        x.noise = 0.02f; x.chorus = 0.6f; x.driftCents = 5.0f; x.freePhase = true;
    }
    { // Vox pad: airy "ah" to "oh", higher and breathier.
        auto& x = set(SynthSound::VoxPad);
        x.osc = Osc::Voice; x.spread = 12.0f; x.filter = F::Sem; x.cutoffHz = 8000.0f; x.keyTrack = 0.0f;
        const float a[3] { 800.0f, 1200.0f, 2800.0f }, b[3] { 500.0f, 900.0f, 2700.0f };
        for (int i = 0; i < 3; ++i) { x.formantA[i] = a[i]; x.formantB[i] = b[i]; }
        amp(x, 0.9f, 1.0f, 0.9f, 2.5f); x.vibratoHz = 5.2f; x.vibratoSemis = 0.05f;
        x.noise = 0.08f; x.chorus = 0.7f; x.driftCents = 4.0f; x.freePhase = true;
    }
    { // Singing bowl: inharmonic partials that beat slowly, the high ones dying first.
        auto& x = set(SynthSound::SingingBowl);
        x.osc = Osc::Bowl; x.bowlDecay = 3.0f; x.filter = F::Sem; x.cutoffHz = 10000.0f; x.keyTrack = 0.0f;
        amp(x, 0.004f, 9.0f, 0.0f, 4.0f); x.hammer = 0.05f; x.chorus = 0.2f; x.driftCents = 0.5f;
    }
    { // Nylon guitar: a soft plucked string.
        auto& x = set(SynthSound::NylonGuitar);
        x.osc = Osc::String; x.stringDamp = 0.38f; x.stringDecay = 2.5f; x.filter = F::Sem; x.cutoffHz = 5000.0f; x.keyTrack = 0.3f;
        amp(x, 0.001f, 6.0f, 0.0f, 0.25f); x.chorus = 0.1f; x.driftCents = 0.5f;
    }
    { // Koto: a bright, long plucked string.
        auto& x = set(SynthSound::Koto);
        x.osc = Osc::String; x.stringDamp = 0.12f; x.stringDecay = 4.5f; x.filter = F::Sem; x.cutoffHz = 9000.0f; x.keyTrack = 0.2f;
        amp(x, 0.001f, 8.0f, 0.0f, 0.6f); x.hammer = 0.05f; x.driftCents = 0.5f;
    }
    // PHASE (Felucca): phase distortion shapes its own tone, no filter after it.
    auto phase = [&](SynthSound sound, int wave, float depth, float env) -> SynthPatch& {
        auto& x = set(sound);
        x.osc = Osc::Phase; x.pdWave = wave; x.pdDepth = depth; x.pdEnv = env; x.filter = F::None; x.keyTrack = 0.0f;
        return x;
    };
    { auto& x = phase(SynthSound::PhaseBass, 0, 0.25f, 0.7f); x.sub = 0.4f; fenv(x, 0.0f, 0.001f, 0.25f, 0.1f, 0.1f);
      amp(x, 0.002f, 0.6f, 0.8f, 0.08f); x.mono = true; }
    { auto& x = phase(SynthSound::ResoLead, 5, 0.2f, 0.8f); x.pdDetune = 7.0f; fenv(x, 0.0f, 0.005f, 0.5f, 0.3f, 0.2f);
      amp(x, 0.004f, 0.4f, 0.9f, 0.15f); x.mono = true; }
    { auto& x = phase(SynthSound::PhasePad, 3, 0.2f, 0.5f); x.pdWave2 = 0; x.pdDetune = 11.0f; fenv(x, 0.0f, 1.5f, 3.0f, 0.5f, 2.0f);
      amp(x, 0.8f, 1.0f, 0.9f, 2.0f); x.chorus = 0.6f; x.driftCents = 4.0f; x.freePhase = true; }
    { auto& x = phase(SynthSound::PhaseKeys, 6, 0.15f, 0.9f); x.pdDetune = 1200.0f; x.pdRing = true; fenv(x, 0.0f, 0.001f, 0.9f, 0.0f, 0.4f);
      amp(x, 0.002f, 2.5f, 0.0f, 0.6f); x.chorus = 0.25f; x.hammer = 0.04f; }
    { auto& x = phase(SynthSound::FoldDrone, 4, 0.3f, 0.3f); x.pdWave2 = 3; x.pdDetune = 5.0f; x.pdFold = 0.35f; x.sub = 0.3f;
      fenv(x, 0.0f, 2.0f, 4.0f, 0.6f, 2.0f); amp(x, 1.2f, 1.0f, 1.0f, 3.0f); x.chorus = 0.4f; x.driftCents = 3.0f; x.freePhase = true; }
    // TRIO (Felucca): three oscillators, ring and sync, through the SEM filter (the chips' own 12 dB filter).
    auto trio = [&](SynthSound sound, int setIndex, float int2, float int3, float detune, int mode, float cutoffHz, float reso, float pw) -> SynthPatch& {
        auto& x = set(sound);
        x.osc = Osc::Trio; x.trioSet = setIndex; x.trioInt2 = int2; x.trioInt3 = int3; x.trioDetune = detune;
        x.filter = F::Sem; x.filterMode = mode; x.cutoffHz = cutoffHz; x.resonance = reso; x.trioPw = pw; x.keyTrack = 0.4f; x.drive = 1.3f;
        return x;
    };
    { auto& x = trio(SynthSound::TrioBass, 0, 0.0f, -12.0f, 9.0f, 0, 700.0f, 0.35f, 0.5f);
      fenv(x, 2.5f, 0.001f, 0.3f, 0.2f, 0.1f); amp(x, 0.002f, 0.6f, 0.6f, 0.1f); x.mono = true; }
    { auto& x = trio(SynthSound::TrioSync, 9, 9.0f, 0.0f, 0.0f, 0, 2500.0f, 0.25f, 0.5f);
      fenv(x, 1.5f, 0.005f, 0.5f, 0.4f, 0.2f); amp(x, 0.004f, 0.5f, 0.9f, 0.2f); x.mono = true; }
    { auto& x = trio(SynthSound::RingBell, 13, 0.0f, 18.0f, 6.0f, 1, 4000.0f, 0.25f, 0.5f);
      amp(x, 0.001f, 2.5f, 0.0f, 1.2f); x.chorus = 0.2f; }
    { auto& x = trio(SynthSound::ChipChoir, 1, 0.0f, 12.0f, 7.0f, 1, 900.0f, 0.75f, 0.35f);
      fenv(x, 0.8f, 0.6f, 1.5f, 0.7f, 1.0f); amp(x, 0.4f, 1.0f, 0.9f, 1.2f); x.chorus = 0.5f; x.freePhase = true; }
    { auto& x = trio(SynthSound::TrioArp, 2, 12.0f, 0.0f, 4.0f, 0, 3000.0f, 0.2f, 0.25f);
      fenv(x, 1.0f, 0.001f, 0.25f, 0.3f, 0.15f); amp(x, 0.001f, 0.5f, 0.7f, 0.15f); }
    // LOFI (Felucca): the chips' own crunch, no filter.
    auto lofi = [&](SynthSound sound, int chip, int wave, float duty) -> SynthPatch& {
        auto& x = set(sound);
        x.osc = Osc::Lofi; x.lofiChip = chip; x.lofiWave = wave; x.lofiDuty = duty; x.filter = F::None; x.driftCents = 0.0f;
        return x;
    };
    { auto& x = lofi(SynthSound::ChipLead, 0, 0, 0.3f); x.lofiVib = 0.15f; amp(x, 0.001f, 0.6f, 0.7f, 0.12f); x.mono = true; }
    { auto& x = lofi(SynthSound::ChipBass, 1, 1, 0.0f); x.lofiTone = 0.7f; amp(x, 0.001f, 0.5f, 0.6f, 0.08f); x.mono = true; }
    { auto& x = lofi(SynthSound::ChipArp, 2, 0, 0.75f); x.lofiArp = 2; x.lofiTone = 0.85f; amp(x, 0.001f, 0.6f, 0.6f, 0.15f); }
    { auto& x = lofi(SynthSound::WaveVox, 0, 4, 11.5f / 16.0f); x.lofiVib = 0.12f; x.lofiTone = 0.9f; amp(x, 0.02f, 0.8f, 0.8f, 0.25f); x.chorus = 0.3f; }

    // Each sound at about the same loudness (measured: a chord and a bass line, RMS).
    for (int i = 0; i < static_cast<int>(SynthSound::Count); ++i)
        p[i].level *= kLoudnessTrim[i];
    return p;
}

} // namespace

const SynthPatch& synthPatch(SynthSound sound)
{
    static const SynthPatch* patches = buildPatches();
    const int i = std::clamp(static_cast<int>(sound), 0, static_cast<int>(SynthSound::Count) - 1);
    return patches[i];
}

const char* synthSoundName(SynthSound sound)
{
    switch (sound)
    {
        case SynthSound::SubBass: return "Sub Bass";
        case SynthSound::AcidBass: return "Acid Bass";
        case SynthSound::ReeseBass: return "Reese Bass";
        case SynthSound::FmBass: return "FM Bass";
        case SynthSound::EPiano: return "E-Piano";
        case SynthSound::Bell: return "Bell";
        case SynthSound::Pluck: return "Pluck";
        case SynthSound::Organ: return "Organ";
        case SynthSound::WarmPad: return "Warm Pad";
        case SynthSound::GlassPad: return "Glass Pad";
        case SynthSound::DarkPad: return "Dark Pad";
        case SynthSound::SquareLead: return "Square Lead";
        case SynthSound::SyncLead: return "Sync Lead";
        case SynthSound::SoftLead: return "Soft Lead";
        case SynthSound::FeltPiano: return "Felt Piano";
        case SynthSound::Celesta: return "Celesta";
        case SynthSound::BreathPad: return "Breath Pad";
        case SynthSound::SlowStrings: return "Slow Strings";
        case SynthSound::PolyStrings: return "Poly Strings";
        case SynthSound::SynthBrass: return "Synth Brass";
        case SynthSound::PolySynth: return "Poly Synth";
        case SynthSound::TapeKeys: return "Tape Keys";
        case SynthSound::SuitcaseKeys: return "Suitcase Keys";
        case SynthSound::Vibraphone: return "Vibraphone";
        case SynthSound::UprightBass: return "Upright Bass";
        case SynthSound::JazzOrgan: return "Jazz Organ";
        case SynthSound::Bass808: return "808 Bass";
        case SynthSound::MonkChoir: return "Monk Choir";
        case SynthSound::VoxPad: return "Vox Pad";
        case SynthSound::SingingBowl: return "Singing Bowl";
        case SynthSound::NylonGuitar: return "Nylon Guitar";
        case SynthSound::Koto: return "Koto";
        case SynthSound::PhaseBass: return "Phase Bass";
        case SynthSound::ResoLead: return "Reso Lead";
        case SynthSound::PhasePad: return "Phase Pad";
        case SynthSound::PhaseKeys: return "Phase Keys";
        case SynthSound::FoldDrone: return "Fold Drone";
        case SynthSound::TrioBass: return "Trio Bass";
        case SynthSound::TrioSync: return "Trio Sync";
        case SynthSound::RingBell: return "Ring Bell";
        case SynthSound::ChipChoir: return "Chip Choir";
        case SynthSound::TrioArp: return "Trio Arp";
        case SynthSound::ChipLead: return "Chip Lead";
        case SynthSound::ChipBass: return "Chip Bass";
        case SynthSound::ChipArp: return "Chip Arp";
        case SynthSound::WaveVox: return "Wave Vox";
        case SynthSound::FmRhodes: return "FM Rhodes";
        case SynthSound::DxBass: return "DX Bass";
        case SynthSound::FmBells: return "FM Bells";
        case SynthSound::FmMarimba: return "FM Marimba";
        case SynthSound::FmBrass: return "FM Brass";
        case SynthSound::FmGlass: return "FM Glass";
        case SynthSound::PwmPad: return "PWM Pad";
        case SynthSound::MonoBass: return "Mono Bass";
        case SynthSound::SemPad: return "SEM Pad";
        case SynthSound::FmPluck: return "FM Pluck";
        case SynthSound::FmPad: return "FM Pad";
        case SynthSound::FmChoir: return "FM Choir";
        case SynthSound::Count: break;
    }
    return "";
}

const char* synthCategoryName(SynthSound sound)
{
    switch (sound)
    {
        case SynthSound::SubBass:
        case SynthSound::AcidBass:
        case SynthSound::ReeseBass:
        case SynthSound::FmBass:
        case SynthSound::Bass808:
        case SynthSound::PhaseBass:
        case SynthSound::TrioBass:
        case SynthSound::ChipBass:
        case SynthSound::DxBass:
        case SynthSound::MonoBass:
        case SynthSound::UprightBass: return "Bass";
        case SynthSound::WarmPad:
        case SynthSound::GlassPad:
        case SynthSound::DarkPad:
        case SynthSound::BreathPad:
        case SynthSound::SlowStrings:
        case SynthSound::MonkChoir:
        case SynthSound::PhasePad:
        case SynthSound::ChipChoir:
        case SynthSound::FmGlass:
        case SynthSound::FoldDrone:
        case SynthSound::VoxPad:
        case SynthSound::PwmPad:
        case SynthSound::SemPad:
        case SynthSound::FmPad:
        case SynthSound::FmChoir:
        case SynthSound::PolyStrings: return "Pads";
        case SynthSound::SquareLead:
        case SynthSound::SyncLead:
        case SynthSound::ResoLead:
        case SynthSound::TrioSync:
        case SynthSound::ChipLead:
        case SynthSound::SoftLead: return "Leads";
        case SynthSound::FmBrass:
        case SynthSound::SynthBrass: return "Brass";
        default: return "Keys";
    }
}

// ---------------------------------------------------------------------------------
// The Tone knobs.

namespace {

// From the design at d = 0 towards lo (d = -1) or hi (d = 1).
float around(float value, float lo, float hi, float d)
{
    return d < 0.0f ? value + (value - lo) * d : value + (hi - value) * d;
}
float scaled(float value, float octaves, float d) { return value * std::exp2(octaves * d); }
int stepped(int value, int lo, int hi, float steps, float d)
{
    return std::clamp(value + static_cast<int>(std::lround(steps * d)), lo, hi);
}

// The FM ratios, as on the Digitone: the Ratio knobs step through them.
constexpr float kRatios[] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, 5.4f,
                              6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 14.0f, 16.0f };
constexpr int kNumRatios = static_cast<int>(sizeof(kRatios) / sizeof(kRatios[0]));
float stepRatio(float ratio, float d)
{
    int nearest = 0;
    for (int i = 1; i < kNumRatios; ++i)
        if (std::abs(std::log2(kRatios[i] / ratio)) < std::abs(std::log2(kRatios[nearest] / ratio)))
            nearest = i;
    const int steps = static_cast<int>(std::lround(6.0f * d));
    if (steps == 0)
        return ratio; // as designed (it may sit between the steps)
    return kRatios[std::clamp(nearest + steps, 0, kNumRatios - 1)];
}

// The formant voices' vowels: "u" .. "a" .. "i".
void vowel(float* formants, float d)
{
    constexpr float u[3] { 300.0f, 700.0f, 2300.0f }, a[3] { 800.0f, 1200.0f, 2800.0f }, i[3] { 280.0f, 2250.0f, 3000.0f };
    for (int k = 0; k < 3; ++k)
    {
        const float f = formants[k];
        formants[k] = d < 0.0f ? f + (u[k] - f) * -d : d < 0.5f ? f + (a[k] - f) * 2.0f * d : a[k] + (i[k] - a[k]) * (2.0f * d - 1.0f);
    }
}

void vibrato(SynthPatch& x, float d)
{
    if (x.vibratoHz <= 0.0f)
        x.vibratoHz = 5.2f;
    x.vibratoSemis = around(x.vibratoSemis, 0.0f, 0.4f, d);
}

enum class Engine { Analog, Fm, Organ, Voice, String, Bowl, Phase, Trio, Lofi };
Engine engineOf(Osc osc)
{
    switch (osc)
    {
        case Osc::Fm4: return Engine::Fm;
        case Osc::Organ: return Engine::Organ;
        case Osc::Voice: return Engine::Voice;
        case Osc::String: return Engine::String;
        case Osc::Bowl: return Engine::Bowl;
        case Osc::Phase: return Engine::Phase;
        case Osc::Trio: return Engine::Trio;
        case Osc::Lofi: return Engine::Lofi;
        default: return Engine::Analog;
    }
}

} // namespace

const char* synthToneName(SynthSound sound, int knob)
{
    static const char* const names[][kSynthToneKnobs] = {
        { "Wave", "Osc 2", "Detune", "Sub", "Noise", "PWM", "Drive", "Analog" },             // analog
        { "Ratio A", "Ratio B", "Harm", "Detune", "Feedback", "Mix", "Level A", "Level B" }, // FM
        { "16'", "8'", "4'", "2 2/3'", "2'", "High", "Click", "Drive" },                     // organ
        { "Vowel A", "Vowel B", "Spread", "Breath", "Sub", "Vibrato", "Drive", "Analog" },  // voice
        { "Damp", "Decay", "Pick", "Sub", "Drive", "Vibrato", "Chorus", "Analog" },         // string
        { "Decay", "Strike", "Sub", "Noise", "Drive", "Vibrato", "Chorus", "Analog" },      // bowl
        { "Wave", "Wave 2", "Depth", "Env", "Detune", "Fold", "Ring", "Sub" },              // PHASE
        { "Set", "Int 2", "Int 3", "Detune", "PW", "Drive", "Noise", "Sub" },               // TRIO
        { "Chip", "Wave", "Duty", "Crush", "Sweep", "Vibrato", "Arp", "Tone" },             // LOFI
    };
    if (knob < 0 || knob >= kSynthToneKnobs)
        return "";
    return names[static_cast<int>(engineOf(synthPatch(sound).osc))][knob];
}

SynthPatch applyTone(const SynthPatch& patch, const SynthParams& params)
{
    SynthPatch x = patch;
    float d[kSynthToneKnobs];
    for (int i = 0; i < kSynthToneKnobs; ++i)
        d[i] = std::clamp(2.0f * (params.tone[static_cast<size_t>(i)] - 0.5f), -1.0f, 1.0f);
    auto drive = [&](float v) { x.drive = std::clamp(scaled(x.drive, 1.5f, v), 0.35f, 8.0f); };
    auto analogDrift = [&](float v) { x.driftCents = around(x.driftCents, 0.0f, 12.0f, v); };
    auto sub = [&](float v) { x.sub = around(x.sub, 0.0f, 0.8f, v); };
    switch (engineOf(patch.osc))
    {
        case Engine::Analog:
        {
            const float w = d[0];
            x.shape1 = std::clamp(around(x.shape1, -1.0f, 1.0f, w), -1.0f, 1.0f);
            x.shape2 = std::clamp(around(x.shape2, -1.0f, 1.0f, w), -1.0f, 1.0f);
            if (patch.osc2 <= 0.0f && patch.interval == 0.0f && patch.detune2 == 0.0f)
                x.detune2 = 7.0f; // brought in from nothing: a few cents apart, so it beats
            x.osc2 = around(x.osc2, 0.0f, 1.0f, d[1]);
            x.detune2 += 20.0f * d[2];
            if (x.unison > 1)
                x.spread = scaled(std::max(x.spread, 1.0f), 1.5f, d[2]);
            else if (d[2] > 0.0f)
            {
                x.unison = 2; // a single oscillator doubles, drifting apart as the knob turns
                x.spread = 25.0f * d[2];
            }
            sub(d[3]);
            x.noise = around(x.noise, 0.0f, 0.3f, d[4]);
            x.pwmDepth = around(x.pwmDepth, 0.0f, 1.0f, d[5]);
            drive(d[6]);
            analogDrift(d[7]);
            break;
        }
        case Engine::Fm:
            x.ratioA = stepRatio(x.ratioA, d[0]);
            x.ratioB1 = stepRatio(x.ratioB1, d[1]);
            x.ratioB2 = stepRatio(x.ratioB2, d[1]);
            x.fmHarm = std::clamp(x.fmHarm + d[2], -1.0f, 1.0f);
            x.fmDetune = around(x.fmDetune, 0.0f, 1.0f, d[3]);
            x.fmFeedback = around(x.fmFeedback, 0.0f, 1.0f, d[4]);
            x.fmMix = around(x.fmMix, 0.0f, 1.0f, d[5]);
            x.levelA = around(x.levelA, 0.0f, 1.0f, d[6]);
            x.levelB = around(x.levelB, 0.0f, 1.0f, d[7]);
            break;
        case Engine::Organ:
            sub(d[0]);
            for (int i = 0; i < 4; ++i)
                x.organ[i] = around(x.organ[i], 0.0f, 1.0f, d[i + 1]);
            x.organ[4] = around(x.organ[4], 0.0f, 1.0f, d[5]);
            x.organ[5] = around(x.organ[5], 0.0f, 1.0f, d[5]);
            x.hammer = around(x.hammer, 0.0f, 0.3f, d[6]);
            drive(d[7]);
            break;
        case Engine::Voice:
            vowel(x.formantA, d[0]);
            vowel(x.formantB, d[1]);
            x.spread = around(x.spread, 0.0f, 30.0f, d[2]);
            x.noise = around(x.noise, 0.0f, 0.3f, d[3]);
            sub(d[4]);
            vibrato(x, d[5]);
            drive(d[6]);
            analogDrift(d[7]);
            break;
        case Engine::String:
            x.stringDamp = around(x.stringDamp, 0.02f, 0.9f, d[0]);
            x.stringDecay = scaled(x.stringDecay, 2.0f, d[1]);
            x.d = scaled(x.d, 2.0f, d[1]);
            x.hammer = around(x.hammer, 0.0f, 0.4f, d[2]);
            sub(d[3]);
            drive(d[4]);
            vibrato(x, d[5]);
            x.chorus = around(x.chorus, 0.0f, 1.0f, d[6]);
            analogDrift(d[7]);
            break;
        case Engine::Bowl:
            x.bowlDecay = scaled(x.bowlDecay, 2.0f, d[0]);
            x.d = scaled(x.d, 2.0f, d[0]);
            x.hammer = around(x.hammer, 0.0f, 0.4f, d[1]);
            sub(d[2]);
            x.noise = around(x.noise, 0.0f, 0.3f, d[3]);
            drive(d[4]);
            vibrato(x, d[5]);
            x.chorus = around(x.chorus, 0.0f, 1.0f, d[6]);
            analogDrift(d[7]);
            break;
        case Engine::Phase:
            x.pdWave = stepped(x.pdWave, 0, 7, 3.5f, d[0]);
            x.pdWave2 = stepped(x.pdWave2 + 1, 0, 8, 4.5f, d[1]) - 1;
            x.pdDepth = around(x.pdDepth, 0.0f, 1.0f, d[2]);
            x.pdEnv = around(x.pdEnv, 0.0f, 1.0f, d[3]);
            x.pdDetune += 25.0f * d[4];
            x.pdFold = around(x.pdFold, 0.0f, 1.0f, d[5]);
            if (d[6] > 0.5f)
                x.pdRing = true;
            else if (d[6] < -0.5f)
                x.pdRing = false;
            sub(d[7]);
            break;
        case Engine::Trio:
            x.trioSet = stepped(x.trioSet, 0, 15, 7.5f, d[0]);
            x.trioInt2 += std::round(12.0f * d[1]);
            x.trioInt3 += std::round(12.0f * d[2]);
            x.trioDetune = around(x.trioDetune, 0.0f, 30.0f, d[3]);
            x.trioPw = around(x.trioPw, 0.05f, 0.95f, d[4]);
            drive(d[5]);
            x.noise = around(x.noise, 0.0f, 0.3f, d[6]);
            sub(d[7]);
            break;
        case Engine::Lofi:
            x.lofiChip = stepped(x.lofiChip, 0, 3, 3.0f, d[0]);
            x.lofiWave = stepped(x.lofiWave, 0, 4, 4.0f, d[1]);
            x.lofiDuty = around(x.lofiDuty, 0.0f, 1.0f, d[2]);
            x.lofiCrush = around(x.lofiCrush, 0.0f, 1.0f, d[3]);
            x.lofiSweep = around(x.lofiSweep, 0.0f, 1.0f, d[4]);
            x.lofiVib = around(x.lofiVib, 0.0f, 1.0f, d[5]);
            x.lofiArp = stepped(x.lofiArp, 0, 3, 3.0f, d[6]);
            x.lofiTone = around(x.lofiTone, 0.0f, 1.0f, d[7]);
            break;
    }
    // The filter and the algorithm, when chosen over the design.
    switch (params.filterChoice)
    {
        case 1: x.filter = FilterModel::Ladder; break;
        case 2: x.filter = FilterModel::Ota; break;
        case 3: case 4: case 5:
            x.filter = FilterModel::Sem;
            x.filterMode = params.filterChoice - 3;
            break;
        default: break;
    }
    if (params.filterChoice > 0 && patch.filter == FilterModel::None && patch.cutoffHz == SynthPatch {}.cutoffHz)
        x.cutoffHz = 4000.0f; // the unfiltered engines: a filter brought in starts open
    if (params.fmAlgorithm > 0 && x.osc == Osc::Fm4)
        x.fmAlg = std::clamp(params.fmAlgorithm - 1, 0, 7);
    return x;
}

// ---------------------------------------------------------------------------------
// Phase distortion, after the PHASE engine of Felucca-Engines
// (Copyright (C) 2026 Leo Kuroshita, Hügelton Instruments; MIT licence): its eight
// waves and its fold, in floating point.

namespace {
float negCos(float p) { return -dsp::SineTable::at(p + 0.25f); }

float pdWave(int wave, float ph, float dcw)
{
    float pd = ph;
    switch (wave)
    {
        case 0: // saw
        {
            const float x = 0.5f + dcw * 0.5f;
            pd = ph < x ? ph * 0.5f / x : 0.5f + (ph - x) * 0.5f / std::max(1.0f - x, 1e-4f);
            break;
        }
        case 1: // square
        {
            const float x = 0.5f - dcw * 0.4749f;
            pd = ph < x ? ph * 0.5f / x : ph < 0.5f ? 0.5f : ph < 0.5f + x ? 0.5f + (ph - 0.5f) * 0.5f / x : 1.0f;
            break;
        }
        case 2: // pulse
        {
            const float x = 1.0f - dcw * 0.9687f;
            pd = ph < x ? ph / x : 1.0f;
            break;
        }
        case 3: // double sine
        {
            const float x = 1.0f - dcw * 0.75f;
            pd = ph < x ? ph / x : (ph - x) / std::max(1.0f - x, 1e-4f);
            break;
        }
        case 4: // saw-pulse
        {
            const float x = 1.0f - dcw * 0.5f, peak = std::max(1e-4f, x * (0.5f + dcw * 0.45f));
            pd = ph >= x ? 1.0f : ph < peak ? ph * 0.5f / peak : 0.5f + (ph - peak) * 0.5f / std::max(x - peak, 1e-4f);
            break;
        }
        default: // resonance: a sine whose pitch rises with the bend, under a saw, triangle or trapezoid window
        {
            float rp = ph + ph * dcw * 0.6714f * 7.0f;
            rp -= std::floor(rp);
            const float window = wave == 5 ? 1.0f - ph : wave == 6 ? (ph < 0.5f ? ph * 2.0f : (1.0f - ph) * 2.0f)
                                                                   : std::min(1.0f, ph < 0.25f ? ph * 4.0f : ph > 0.75f ? (1.0f - ph) * 4.0f : 1.0f);
            return negCos(rp) * 0.625f * window;
        }
    }
    return negCos(pd);
}

// ---------------------------------------------------------------------------------
// TRIO and LOFI, after the engines of Felucca-Engines (Copyright (C) 2026 Leo Kuroshita,
// Hügelton Instruments; MIT licence), ported to floating point; envelopes and patches
// are Gristo's.

enum TrioWave { TwOff, TwTri, TwSaw, TwPls, TwNoi, TwSt, TwPs };
struct TrioSet
{
    TrioWave w[3];
    bool ring, sync;
    float g[3];
};
constexpr float G3 = 11000.0f / 32768.0f, G2 = 15000.0f / 32768.0f, G1 = 24000.0f / 32768.0f, Gx = 1000.0f / 32768.0f;
const TrioSet kTrioSets[16] = {
    { { TwSaw, TwSaw, TwSaw }, false, false, { G3, G3, G3 } },                   // SAW3
    { { TwPls, TwPls, TwPls }, false, false, { G3, G3, G3 } },                   // PLS3
    { { TwPls, TwPls, TwTri }, false, false, { G3, G3, G3 + 3 * Gx } },          // PPT
    { { TwSaw, TwSaw, TwTri }, false, false, { G3, G3, G3 + 3 * Gx } },          // SST
    { { TwTri, TwTri, TwTri }, false, false, { G3 + 2 * Gx, G3 + 2 * Gx, G3 + 2 * Gx } }, // TRI3
    { { TwSt, TwSt, TwTri }, false, false, { G3 + 3 * Gx, G3 + 3 * Gx, G3 } },   // S&T
    { { TwPs, TwPs, TwTri }, false, false, { G3 + 3 * Gx, G3 + 3 * Gx, G3 } },   // P&S
    { { TwPls, TwOff, TwNoi }, false, false, { G2, 0.0f, G2 - 4 * Gx } },        // P+N
    { { TwNoi, TwOff, TwOff }, false, false, { G1, 0.0f, 0.0f } },               // NOIS
    { { TwSaw, TwSaw, TwOff }, false, true, { G2 - 5 * Gx, G2 + 5 * Gx, 0.0f } }, // SYNC
    { { TwPls, TwPls, TwOff }, false, true, { G2 - 5 * Gx, G2 + 5 * Gx, 0.0f } }, // SYNCP
    { { TwSaw, TwSaw, TwTri }, false, true, { G3 - 3 * Gx, G3 + 4 * Gx, G3 } },  // SYNC3
    { { TwTri, TwOff, TwOff }, true, false, { G1, 0.0f, 0.0f } },                // RING
    { { TwTri, TwOff, TwTri }, true, false, { G2 + 2 * Gx, 0.0f, G2 - 3 * Gx } }, // RING3
    { { TwTri, TwSaw, TwOff }, true, true, { G2 + 2 * Gx, G2, 0.0f } },          // R+S
    { { TwTri, TwPls, TwOff }, true, true, { G2 + 2 * Gx, G2, 0.0f } },          // R+SP
};

float triWave(float ph) { return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; }
// The combined waves: the bitwise AND of two 16-bit waves, as the chips mix them.
int tri16(float ph) { return static_cast<int>((ph < 0.5f ? ph * 2.0f : 2.0f - ph * 2.0f) * 65535.0f) & 0xFFFF; }
float sawAndTri(float ph) { return static_cast<float>((static_cast<int>(ph * 65535.0f) & tri16(ph)) - 16384) / 32768.0f; }
float pulseAndSaw(float ph, float pw) { return (ph >= pw ? ph : 0.0f) - (1.0f - pw * pw) * 0.5f; }

// LOFI wave RAM: 16 tables x 32 four-bit samples (Felucca-Engines eng_lofi.c, MIT).
const std::uint8_t kWram[16][16] = {
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x89, 0xAC, 0xDE, 0xEF, 0xFF, 0xEE, 0xDC, 0xA9, 0x86, 0x53, 0x21, 0x10, 0x00, 0x11, 0x23, 0x56 },
    { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 },
    { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF },
    { 0x01, 0x12, 0x33, 0x45, 0x56, 0x77, 0x89, 0x9A, 0x56, 0x67, 0x88, 0x9A, 0xAB, 0xCC, 0xDE, 0xEF },
    { 0xEE, 0xEF, 0xFF, 0xFF, 0xFF, 0xEE, 0xED, 0xDC, 0x11, 0x10, 0x00, 0x00, 0x00, 0x11, 0x12, 0x23 },
    { 0xCE, 0xFF, 0xED, 0xBA, 0x9A, 0xBD, 0xEF, 0xFE, 0xC9, 0x64, 0x21, 0x00, 0x00, 0x01, 0x24, 0x69 },
    { 0x8B, 0xEF, 0xED, 0xBA, 0x9A, 0xAA, 0x98, 0x77, 0x88, 0x87, 0x65, 0x55, 0x65, 0x42, 0x10, 0x14 },
    { 0x8D, 0xFE, 0xDD, 0xDD, 0xCD, 0xDD, 0xDE, 0xFD, 0x82, 0x01, 0x22, 0x22, 0x32, 0x22, 0x21, 0x02 },
    { 0x8F, 0xFC, 0xA7, 0x67, 0x89, 0x88, 0x88, 0x87, 0x88, 0x77, 0x77, 0x76, 0x78, 0x98, 0x53, 0x00 },
    { 0x8F, 0xEE, 0xFD, 0xDB, 0x98, 0x66, 0x66, 0x77, 0x88, 0x89, 0x99, 0x97, 0x64, 0x22, 0x01, 0x10 },
    { 0x7F, 0x96, 0xCF, 0xBB, 0xCC, 0xBB, 0xAA, 0x98, 0x77, 0x65, 0x54, 0x43, 0x34, 0x40, 0x39, 0x60 },
    { 0xF3, 0x00, 0x12, 0x22, 0x11, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22 },
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0x00, 0x00, 0x00, 0x00 },
};
float wramMean(int table)
{
    float sum = 0.0f;
    for (int i = 0; i < 16; ++i)
        sum += static_cast<float>((kWram[table][i] >> 4) + (kWram[table][i] & 15)) - 15.0f;
    return sum / 32.0f / 7.5f;
}

// ---------------------------------------------------------------------------------
// FM4: the operators in the order they are computed (modulators first): B2, B1, A, C.
// Per algorithm: which operators modulate each one, and the X and Y outputs.

enum { OpB2 = 0, OpB1 = 1, OpA = 2, OpC = 3 };
constexpr int bit(int op) { return 1 << op; }
struct FmAlgorithm
{
    int mod[4]; // per operator: the operators that modulate it (bits)
    int x, y;   // the operators heard on the X and Y outputs (bits)
    int fb;     // the operator with the feedback loop: the top of the algorithm's main path
};
constexpr FmAlgorithm kFmAlgorithms[8] = {
    { { 0, bit(OpB2), bit(OpB1), bit(OpA) }, bit(OpC), bit(OpA), OpB2 },                         // 1 stack: B2 > B1 > A > C
    { { 0, bit(OpB2), 0, bit(OpA) }, bit(OpC), bit(OpB1), OpA },                                 // 2 pairs: B2 > B1, A > C
    { { 0, bit(OpB2), 0, bit(OpA) | bit(OpB1) }, bit(OpC), bit(OpA), OpA },                      // 3 Y: B2 > B1, A + B1 > C
    { { 0, bit(OpB2), bit(OpB1), bit(OpB1) }, bit(OpC), bit(OpA), OpB2 },                        // 4 branch: B2 > B1 > A and C
    { { 0, bit(OpB2), bit(OpB2), bit(OpB2) }, bit(OpC), bit(OpA) | bit(OpB1), OpB2 },            // 5 fan: B2 > B1, A, C
    { { 0, 0, 0, bit(OpB2) | bit(OpB1) | bit(OpA) }, bit(OpC), bit(OpA), OpA },                  // 6 three into C
    { { 0, 0, bit(OpB2) | bit(OpB1), bit(OpA) }, bit(OpC), bit(OpA), OpB2 },                     // 7 B2 + B1 > A > C
    { { 0, bit(OpB2), 0, 0 }, bit(OpC) | bit(OpA), bit(OpB1), OpA },                             // 8 C and A heard, B2 > B1
};
int popcount4(int bits) { return (bits & 1) + ((bits >> 1) & 1) + ((bits >> 2) & 1) + ((bits >> 3) & 1); }

// One cycle of an operator: a sine, with harmonics grown in (odd ones: hollow, square-like;
// all of them: towards a saw) while they stay below the oversampled Nyquist.
float opWave(float phase, float harm, bool odd, float inc)
{
    float y = dsp::SineTable::at(phase);
    if (harm <= 0.0f)
        return y;
    if (odd)
    {
        if (inc * 3.0f < 0.45f)
            y += harm * 0.42f * dsp::SineTable::at(3.0f * phase);
        if (inc * 5.0f < 0.45f)
            y += harm * 0.2f * dsp::SineTable::at(5.0f * phase);
        return y / (1.0f + 0.35f * harm);
    }
    if (inc * 2.0f < 0.45f)
        y += harm * 0.5f * dsp::SineTable::at(2.0f * phase);
    if (inc * 3.0f < 0.45f)
        y += harm * 0.33f * dsp::SineTable::at(3.0f * phase);
    return y / (1.0f + 0.4f * harm);
}

constexpr float kOrganHarmonics[6] = { 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f };

float attackTime(const SynthPatch& p, const SynthParams& m) { return dsp::macroLog(m.attack, 0.001f, std::max(p.a, 0.001f), 6.0f); }
float releaseTime(const SynthPatch& p, const SynthParams& m) { return dsp::macroLog(m.release, 0.01f, std::max(p.r, 0.01f), 8.0f); }
float random01(dsp::Noise& n) { return 0.5f + 0.5f * n.next(); }

} // namespace

// ---------------------------------------------------------------------------------
// The voice.

void SynthVoice::prepare(float sampleRate, std::uint32_t seed)
{
    sampleRate_ = sampleRate;
    drift_ = dsp::Noise(seed);
    string_.assign(static_cast<size_t>(sampleRate / 20.0f) + 8, 0.0f);
    // This voice's own tolerances, as the parts of one voice card differ from the next.
    tolCutoff_ = std::exp2(0.08f * drift_.next());
    tolDetune_ = 2.5f * drift_.next(); // cents
    tolEnv_ = 1.0f + 0.08f * drift_.next();
    tolOsc2_ = 1.5f * drift_.next(); // cents
    pwmPhase_ = random01(drift_);
}

void SynthVoice::derive()
{
    const float slop = std::min(1.0f, patch_.driftCents); // in-tune designs keep their voices in tune too
    // Unison: two oscillators either side of the note, or three with one on it; each
    // note's spread a little different.
    const int n = std::clamp(patch_.unison, 1, 3);
    const float s = patch_.spread * spreadJitter_ / 1200.0f;
    spreadRatio_[0] = spreadRatio_[1] = spreadRatio_[2] = 1.0f;
    if (n == 2)
    {
        spreadRatio_[0] = std::exp2(-0.5f * s);
        spreadRatio_[1] = std::exp2(0.5f * s);
    }
    else if (n == 3)
    {
        spreadRatio_[0] = std::exp2(-s);
        spreadRatio_[2] = std::exp2(s);
    }
    interval_ = std::exp2((patch_.interval + (patch_.detune2 + tolOsc2_ * slop) / 100.0f) / 12.0f);
    {
        constexpr float spread[5] = { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f };
        for (int i = 0; i < 5; ++i)
            detuneRatio_[i] = std::exp2(spread[i] * patch_.spread / 1200.0f);
    }
    // FM: detune spreads A and the B pair away from C, in different directions.
    const float det = patch_.fmDetune * 14.0f / 1200.0f;
    opRatio_[OpC] = patch_.ratioC;
    opRatio_[OpA] = patch_.ratioA * std::exp2(det);
    opRatio_[OpB1] = patch_.ratioB1 * std::exp2(-0.8f * det);
    opRatio_[OpB2] = patch_.ratioB2 * std::exp2(0.5f * det);
    fmVelScale_ = 1.0f - patch_.fmVel * (1.0f - velocity_);
    pdDetuneRatio_ = std::exp2(patch_.pdDetune / 1200.0f);
    trioRatio_[0] = std::exp2((patch_.trioInt2 + patch_.trioDetune / 100.0f) / 12.0f);
    trioRatio_[1] = std::exp2((patch_.trioInt3 - patch_.trioDetune / 100.0f) / 12.0f);
    hp_.setLowpass(patch_.hpHz, sampleRate_);
    bowlCoef_ = dsp::decayCoef(std::max(0.05f, patch_.bowlDecay), sampleRate_);
    // The string's loss filter: a one-pole low-pass, matched at 2x to the 2-tap average
    // the string was tuned with at the host's rate.
    {
        const float d = std::clamp(patch_.stringDamp, 0.0f, 0.95f);
        const float q = 4.0f * d * (1.0f - d);
        stringLoopA_ = q < 1e-4f ? 1.0f : (-1.0f + std::sqrt(1.0f + 4.0f * q)) / (2.0f * q);
    }
    if (patch_.osc == Osc::Lofi)
    {
        // TONE: a one-pole low-pass from about 640 Hz (0) to open (1), set at 44.1 kHz in the original.
        const float a = std::min(0.9999f, (3000.0f + patch_.lofiTone * 127.0f * 256.0f) / 32768.0f);
        const float hz = -std::log(1.0f - a) * 44100.0f / dsp::kTwoPi;
        lofiLpCoef_ = 1.0f - std::exp(-dsp::kTwoPi * std::min(hz, sampleRate_ * 0.45f) / sampleRate_);
    }
}

void SynthVoice::retone(const SynthPatch& patch)
{
    patch_ = patch;
    derive();
}

void SynthVoice::start(const SynthPatch& patch, const SynthParams& macros, int id, float pitch, float velocity)
{
    const bool wasActive = active();
    patch_ = patch;
    id_ = id;
    pitch_ = targetPitch_ = pitch;
    velocity_ = dsp::clamp01(velocity);
    const float slop = std::min(1.0f, patch_.driftCents);
    detune_ = (tolDetune_ * slop + (patch_.driftCents * (0.5f + alive_) + 3.0f * alive_ * slop) * 0.6f * drift_.next()) / 100.0f;
    spreadJitter_ = 1.0f + 0.15f * drift_.next();
    derive();

    const float envTol = 1.0f + (tolEnv_ - 1.0f) * slop;
    amp_.set(attackTime(patch_, macros) * envTol, patch_.d * envTol * (1.0f + 0.15f * alive_ * drift_.next()), patch_.s,
             releaseTime(patch_, macros), sampleRate_);
    filterEnv_.set(patch_.fa * envTol, patch_.fd * envTol, patch_.fs, patch_.fr, sampleRate_);
    amp_.gateOn();
    filterEnv_.gateOn();
    // FM: the A and B envelopes restart with every note; Env scales their decay.
    const float time = dsp::macroLog(macros.envAmount, 0.3f, 1.0f, 3.0f);
    envA_.set(patch_.envA.a, patch_.envA.d * time, patch_.envA.end, sampleRate_);
    envB_.set(patch_.envB.a, patch_.envB.d * time, patch_.envB.end, sampleRate_);
    envA_.start();
    envB_.start();
    hammerEnv_ = patch_.hammer > 0.0f ? 0.4f + 0.6f * velocity_ : 0.0f; // harder notes, harder hammer
    hammerCoef_ = dsp::decayCoef(0.008f, sampleRate_);
    pitchEnv_ = patch_.pitchDrop > 0.0f ? 1.0f : 0.0f;
    pitchEnvCoef_ = dsp::decayCoef(0.05f, sampleRate_);
    bowlEnv_ = 1.0f;
    gate_ = true;
    if (patch_.osc == Osc::Lofi)
    {
        lofiSweepSemis_ = lofiArpTime_ = 0.0f;
        lofiHoldCount_ = 0.0f;
    }
    excite_ = patch_.osc == Osc::String ? static_cast<int>(sampleRate_ / std::max(20.0f, dsp::midiToHz(pitch))) : 0;
    if (!wasActive)
    {
        // From silence: the oscillators start together (a consistent attack), or anywhere
        // for the free-running sounds; the filters and the FM feedback empty.
        for (int i = 0; i < 3; ++i)
            osc1_[static_cast<size_t>(i)].reset(patch_.freePhase ? random01(drift_) : 0.31f * static_cast<float>(i));
        osc2_.reset(patch_.freePhase ? random01(drift_) : 0.0f);
        subPhase_ = 0.0f;
        for (int i = 0; i < 4; ++i)
            opPhase_[i] = patch_.freePhase ? random01(drift_) : 0.0f;
        fb_[0] = fb_[1] = 0.0f;
        hp_.reset();
        ladder_.reset();
        ota_.reset();
        sem_.reset();
        filterPrimed_ = false;
        std::fill(string_.begin(), string_.end(), 0.0f);
        stringPrev_ = exciteLp_ = 0.0f;
        vowelPhase_ = random01(drift_);
        for (auto& f : formant_)
            f.reset();
        for (int i = 0; i < 5; ++i)
            phase_[i] = patch_.freePhase ? random01(drift_) : 0.2f * static_cast<float>(i);
        if (patch_.osc == Osc::Trio)
        {
            phase_[0] = 0.0f;
            phase_[1] = 0.109375f; // a little apart: equal saws would cancel each other's harmonics
            phase_[2] = 0.21875f;
        }
        pdToggle_[0] = pdToggle_[1] = false;
        foldLast_ = foldOut_ = 0.0f;
    }
    controlCounter_ = 0;
}

void SynthVoice::legato(int id, float pitch, float velocity)
{
    id_ = id;
    targetPitch_ = pitch;
    velocity_ = dsp::clamp01(velocity);
}

void SynthVoice::release(const SynthParams& macros)
{
    amp_.setRelease(releaseTime(patch_, macros), sampleRate_);
    amp_.gateOff();
    filterEnv_.gateOff();
    gate_ = false;
}

float SynthVoice::analog(float hz)
{
    const float dt = hz / sampleRate_;
    pwmPhase_ += patch_.pwmHz / sampleRate_;
    pwmPhase_ -= std::floor(pwmPhase_);
    const float pw = patch_.pwmDepth > 0.0f ? std::clamp(patch_.pw + patch_.pwmDepth * 0.42f * dsp::SineTable::at(pwmPhase_), 0.06f, 0.94f)
                                            : patch_.pw;
    const int n = std::clamp(patch_.unison, 1, 3);
    constexpr float unisonGain[3] = { 1.0f, 0.75f, 0.62f };
    float out = 0.0f;
    if (patch_.osc1 > 0.0f || patch_.sync)
    {
        float o1 = 0.0f;
        for (int i = 0; i < n; ++i)
            o1 += osc1_[static_cast<size_t>(i)].tick(dt * spreadRatio_[i], pw, patch_.shape1);
        out += o1 * unisonGain[n - 1] * patch_.osc1;
    }
    if (patch_.osc2 > 0.0f)
    {
        const float f2 = dt * interval_ * drift2Ratio_ * osc2Sweep_;
        out += osc2_.tick(f2, pw, patch_.shape2, patch_.sync ? dt : 0.0f) * patch_.osc2;
    }
    return out / (1.0f + 0.5f * patch_.osc2);
}

float SynthVoice::fm4(float hz)
{
    const FmAlgorithm& alg = kFmAlgorithms[std::clamp(patch_.fmAlg, 0, 7)];
    const float dt = hz / sampleRate_;
    const float eA = envA_.tick(), eB = envB_.tick();
    // Modulation depth in cycles of phase: the level's square (fine control at the low
    // end, where FM is subtle), the envelope, velocity, key scaling and the Cutoff knob.
    constexpr float kDepth = 1.6f;
    const float scale = fmVelScale_ * fmKeyScale_ * fmBright_ * kDepth;
    const float depthA = patch_.levelA * patch_.levelA * eA * scale;
    const float depthB = patch_.levelB * patch_.levelB * eB * scale;
    const float depth[4] = { depthB * opFade_[OpB2], depthB * opFade_[OpB1], depthA * opFade_[OpA], 0.0f };
    // Heard: C at full level, the others at their level on their envelope.
    const float heard[4] = { patch_.levelB * eB, patch_.levelB * eB, patch_.levelA * eA, 1.0f };
    const float harmOdd = std::max(0.0f, -patch_.fmHarm), harmAll = std::max(0.0f, patch_.fmHarm);
    float y[4];
    for (int op = 0; op < 4; ++op)
    {
        const float inc = std::min(dt * opRatio_[op], 0.45f);
        float phase = opPhase_[op];
        if (op == alg.fb)
            phase += fmFb_ * 0.35f * 0.5f * (fb_[0] + fb_[1]);
        for (int m = 0; m < op; ++m)
            if (alg.mod[op] & bit(m))
                phase += y[m] * depth[m];
        const bool carrierSide = op == OpC || op == OpA;
        y[op] = opWave(phase, carrierSide ? harmOdd : harmAll, carrierSide, inc);
        opPhase_[op] += inc;
        opPhase_[op] -= std::floor(opPhase_[op]);
    }
    fb_[1] = fb_[0];
    fb_[0] = y[alg.fb];
    auto output = [&](int bits) {
        float sum = 0.0f;
        for (int op = 0; op < 4; ++op)
            if (bits & bit(op))
                sum += y[op] * heard[op];
        return sum / std::sqrt(static_cast<float>(std::max(1, popcount4(bits))));
    };
    const float mix = dsp::clamp01(patch_.fmMix);
    return output(alg.x) * (1.0f - mix) + output(alg.y) * mix * 1.4f;
}

float SynthVoice::special(float hz)
{
    const float sr = sampleRate_;
    const float dt = std::min(hz / sr, 0.45f);
    auto advance = [](float& phase, float inc) {
        phase += inc;
        phase -= std::floor(phase);
    };
    float out = 0.0f;
    switch (patch_.osc)
    {
        case Osc::Organ:
            advance(phase_[0], dt);
            for (int i = 0; i < 6; ++i)
                if (patch_.organ[i] > 0.0f && dt * kOrganHarmonics[i] < 0.45f)
                    out += patch_.organ[i] * dsp::SineTable::at(phase_[0] * kOrganHarmonics[i]);
            return out * 0.4f;
        case Osc::Voice:
        {
            float source = 0.0f;
            for (int i = 0; i < 3; ++i)
            {
                const float d = std::min(dt * detuneRatio_[i + 1], 0.45f);
                advance(phase_[i], d);
                source += dsp::sawBlep(phase_[i], d);
            }
            source = source * 0.4f + patch_.noise * drift_.next();
            constexpr float gains[3] = { 1.0f, 0.6f, 0.3f };
            for (int i = 0; i < 3; ++i)
                out += formant_[i].tick(source).bp * gains[i] * 0.48f;
            return out + source * 0.08f;
        }
        case Osc::String:
        {
            const auto size = string_.size();
            const float delay = std::clamp(sr / hz - (1.0f - stringLoopA_) / stringLoopA_, 2.0f, static_cast<float>(size) - 3.0f);
            const float pos = static_cast<float>(stringWrite_ + size) - delay;
            const auto i0 = static_cast<size_t>(pos);
            const float frac = pos - static_cast<float>(i0);
            const float read = string_[i0 % size] + (string_[(i0 + 1) % size] - string_[i0 % size]) * frac;
            stringPrev_ += stringLoopA_ * (read - stringPrev_);
            float y = stringPrev_ * stringFb_;
            if (excite_ > 0) // the pluck: a period of noise, brighter when played harder
            {
                exciteLp_ += (0.08f + 0.5f * velocity_) * (drift_.next() - exciteLp_);
                y += exciteLp_ * 0.9f;
                --excite_;
            }
            string_[stringWrite_] = y;
            stringWrite_ = (stringWrite_ + 1) % size;
            return y;
        }
        case Osc::Bowl:
        {
            constexpr float ratios[4] = { 1.0f, 2.76f, 5.2f, 8.4f }, levels[4] = { 1.0f, 0.55f, 0.3f, 0.15f };
            advance(phase_[4], 0.7f / sr);
            bowlEnv_ *= bowlCoef_;
            float e = 1.0f;
            for (int i = 0; i < 4; ++i)
            {
                if (dt * ratios[i] < 0.45f)
                {
                    advance(phase_[i], dt * ratios[i]);
                    const float beat = 1.0f + 0.35f * dsp::SineTable::at(phase_[4] * static_cast<float>(i + 1) + 0.2f * static_cast<float>(i));
                    out += levels[i] * e * beat * dsp::SineTable::at(phase_[i]);
                }
                e *= bowlEnv_; // the higher partials die first
            }
            return out * 0.5f;
        }
        case Osc::Phase:
        {
            auto line = [&](int index, float inc) {
                float& ph = phase_[index];
                const int wave = pdToggle_[index] && patch_.pdWave2 >= 0 ? patch_.pdWave2 : patch_.pdWave;
                const float y = pdWave(wave, ph, pdBend_);
                ph += inc;
                if (ph >= 1.0f)
                {
                    ph -= 1.0f;
                    pdToggle_[index] = !pdToggle_[index]; // the second wave every other cycle
                }
                return y;
            };
            out = line(0, dt);
            if (patch_.pdDetune != 0.0f || patch_.pdRing)
            {
                const float second = line(1, std::min(dt * pdDetuneRatio_, 0.45f));
                out = patch_.pdRing ? out * second : (out + second) * 0.5f;
            }
            if (patch_.pdFold > 0.0f)
            {
                // A sine fold (no corners), less of it above A4; then a DC blocker.
                float gain = 0.64f + patch_.pdFold * 7.36f;
                if (hz > 440.0f)
                    gain = 0.64f + (gain - 0.64f) * 440.0f / hz;
                const float folded = dsp::SineTable::at(0.25f * out * gain);
                foldOut_ += folded - foldLast_ - foldOut_ * 0.001f;
                foldLast_ = folded;
                out = foldOut_;
            }
            return out;
        }
        case Osc::Trio:
        {
            const TrioSet& set = kTrioSets[patch_.trioSet & 15];
            const float pw = std::clamp(patch_.trioPw, 0.03f, 0.97f);
            const float incs[3] = { dt, std::min(dt * trioRatio_[0], 0.45f), std::min(dt * trioRatio_[1], 0.45f) };
            auto wave = [&](TrioWave w, int i, float inc) -> float {
                const float ph = phase_[i];
                switch (w)
                {
                    case TwTri: return triWave(ph);
                    case TwSaw: return dsp::sawBlep(ph, inc);
                    case TwPls: return dsp::squareBlep(ph, inc, pw) - (2.0f * pw - 1.0f);
                    case TwNoi: return trioNoise_;
                    case TwSt: return sawAndTri(ph);
                    case TwPs: return pulseAndSaw(ph, pw);
                    default: return 0.0f;
                }
            };
            // Osc 3 first: RING reads its top phase bit.
            const bool flip = phase_[2] >= 0.5f;
            float mix = wave(set.w[2], 2, incs[2]) * set.g[2];
            const float o1 = wave(set.w[0], 0, incs[0]);
            mix += (set.ring ? (flip ? -1.0f : 1.0f) * triWave(phase_[0]) : o1) * set.g[0];
            mix += wave(set.w[1], 1, incs[1]) * set.g[1];
            bool wrapped = false;
            for (int i = 0; i < 3; ++i)
            {
                const float before = phase_[i];
                phase_[i] += incs[i];
                if (phase_[i] >= 1.0f)
                {
                    phase_[i] -= 1.0f;
                    if (i == 0)
                        wrapped = true;
                }
                // The pitched noise: a new random value 16 times a cycle.
                if ((set.w[i] == TwNoi) && static_cast<int>(before * 16.0f) != static_cast<int>(phase_[i] * 16.0f))
                    trioNoise_ = drift_.next();
            }
            if (set.sync && wrapped)
                phase_[1] = phase_[0] * incs[1] / std::max(incs[0], 1e-6f); // osc 2 restarts with osc 1
            return mix * 1.6f;
        }
        case Osc::Lofi:
        {
            // The pitch: the sweep falls after the note-on, the arpeggio steps at ~50 Hz, a ~5.4 Hz vibrato.
            static const int arps[4][3] = { { 0, 0, 0 }, { 0, 12, 0 }, { 0, 4, 7 }, { 0, 3, 7 } };
            lofiSweepSemis_ = std::min(96.0f, lofiSweepSemis_ + patch_.lofiSweep * 81.0f / sr);
            lofiArpTime_ += 1.0f / sr;
            const int step = static_cast<int>(lofiArpTime_ * 50.0f) % 3;
            lofiVibPhase_ += 5.4f / sr;
            lofiVibPhase_ -= std::floor(lofiVibPhase_);
            const float semis = static_cast<float>(arps[patch_.lofiArp & 3][step]) - lofiSweepSemis_
                                + patch_.lofiVib * 0.26f * dsp::SineTable::at(lofiVibPhase_);
            const float inc = std::min(dt * std::exp2(semis / 12.0f), 0.45f);
            const int bits = patch_.lofiChip == 2 ? 8 : patch_.lofiChip == 3 ? 1 : 4;
            // The held output: the sample rate drops with CRUSH (and halves again for 4-bit / 2).
            const float hold = (1.0f + std::floor(patch_.lofiCrush * 127.0f / 8.0f) + (patch_.lofiChip == 1 ? 1.0f : 0.0f)) * sr / 44100.0f;
            lofiHoldCount_ -= 1.0f;
            if (lofiHoldCount_ <= 0.0f)
            {
                lofiHoldCount_ += hold;
                const float ph = phase_[0];
                float v = 0.0f;
                bool quantise = true;
                switch (patch_.lofiWave)
                {
                    case 1: v = triWave(ph); break;
                    case 2: v = 2.0f * ph - 1.0f; break;
                    case 3:
                        lofiLfsr_ = (lofiLfsr_ >> 1) | (((lofiLfsr_ ^ (lofiLfsr_ >> 1)) & 1u) << 14);
                        v = (lofiLfsr_ & 1u) ? 1.0f : -1.0f;
                        break;
                    case 4:
                    {
                        const int table = std::clamp(static_cast<int>(patch_.lofiDuty * 15.99f), 0, 15);
                        const int k = static_cast<int>(ph * 32.0f) & 31;
                        const int nibble = (k & 1) ? kWram[table][k >> 1] & 15 : kWram[table][k >> 1] >> 4;
                        v = bits == 1 ? (nibble >= 8 ? 1.0f : -1.0f) : (static_cast<float>(nibble) * 2.0f - 15.0f) / 15.0f - wramMean(table);
                        quantise = false;
                        break;
                    }
                    default:
                    {
                        const float duty = 0.125f * (1.0f + std::floor(patch_.lofiDuty * 3.99f)); // 12.5 .. 50 %
                        v = (ph < duty ? 1.0f : -1.0f) - (2.0f * duty - 1.0f);
                        break;
                    }
                }
                if (quantise)
                {
                    const float steps = std::exp2(static_cast<float>(bits - 1));
                    v = std::round(v * steps) / steps;
                }
                if (patch_.lofiCrush > 0.5f)
                    v = std::round(v * 8.0f) / 8.0f;
                lofiHeld_ = v;
            }
            phase_[0] += inc;
            phase_[0] -= std::floor(phase_[0]);
            lofiLp_ += (lofiHeld_ - lofiLp_) * lofiLpCoef_;
            return lofiLp_ * 0.8f;
        }
        default: return 0.0f;
    }
}

void SynthVoice::control(const SynthParams& macros)
{
    const float rate = sampleRate_ / static_cast<float>(kControlRate);
    if (--wanderCounter_ <= 0) // a new direction two or three times a second
    {
        for (auto& w : wanderTarget_)
            w = drift_.next();
        wanderCounter_ = static_cast<int>(rate * (0.3f + 0.2f * drift_.next()));
    }
    for (int w = 0; w < 3; ++w)
        wander_[w] += (wanderTarget_[w] - wander_[w]) * (8.0f / rate);
    // The slow wandering of an analog oscillator: wider on the designs that ask for it.
    const float cents = alive_ * std::min(12.0f, 1.0f + 1.5f * patch_.driftCents);
    driftRatio_ = std::exp2(wander_[0] * cents / 1200.0f);
    drift2Ratio_ = std::exp2(wander_[2] * cents * 0.7f / 1200.0f);
    cutoffDrift_ = std::exp2(wander_[1] * alive_ * 0.12f);

    const float knob = dsp::clamp01(macros.cutoff + macros.cutoffMod);
    const float hz = std::max(hz_, 1.0f);
    float cutoff;
    if (patch_.osc == Osc::Fm4)
    {
        // FM: Cutoff is the brightness (the modulators' depth), and moves the filter with it.
        fmBright_ = std::exp2((knob - 0.5f) * 2.0f);
        fmFb_ = dsp::clamp01(patch_.fmFeedback + (macros.resonance - 0.5f) * 0.8f);
        cutoff = patch_.cutoffHz * std::exp2((knob - 0.5f) * 3.0f);
        fmKeyScale_ = hz > 1000.0f ? std::sqrt(1000.0f / hz) : 1.0f; // gentler FM up high: less harsh
        // A modulator far above hearing only folds back as grit: it fades out above ~16 kHz.
        for (int op = 0; op < 4; ++op)
            opFade_[op] = dsp::clamp01((24000.0f - hz * opRatio_[op]) / 8000.0f);
    }
    else
        cutoff = dsp::macroLog(knob, 40.0f, patch_.cutoffHz, 16000.0f);
    cutoff *= std::exp2(patch_.keyTrack * std::log2(hz / 261.63f));
    float envOct = patch_.envOctaves * macros.envAmount * 2.0f;
    if (patch_.envOctaves < 0.5f)
        envOct += std::max(0.0f, macros.envAmount - 0.5f) * 6.0f; // lets static patches get an envelope
    const float softer = 1.0f - velocity_;
    envOct *= 1.0f - 0.6f * patch_.velocityToFilter * softer;
    const float tolerance = 1.0f + (tolCutoff_ - 1.0f) * std::min(1.0f, patch_.driftCents);
    cutoff *= std::exp2(envOct * filterEnv_.value() - patch_.velocityToFilter * softer * 0.8f) * cutoffDrift_ * tolerance;
    osc2Sweep_ = patch_.osc2Env != 0.0f ? std::exp2(patch_.osc2Env * filterEnv_.value() * (0.5f + 0.5f * macros.envAmount * 2.0f) / 12.0f) : 1.0f;

    const float r = dsp::clamp01(macros.resonance);
    reso_ = r <= 0.5f ? patch_.resonance * r * 2.0f : patch_.resonance + (1.0f - patch_.resonance) * (r - 0.5f) * 2.0f;
    float g = dsp::filterG(std::clamp(cutoff, 20.0f, 0.45f * sampleRate_), sampleRate_);
    if (patch_.osc == Osc::Trio) // the slightly unstable filter of the chips
    {
        trioDrift_ += (0.5f * drift_.next() - trioDrift_) * 0.12f;
        g *= 1.0f + 0.05f * trioDrift_;
    }
    if (!filterPrimed_)
    {
        g_ = g;
        gStep_ = 0.0f;
        filterPrimed_ = true;
    }
    else
        gStep_ = (g - g_) / static_cast<float>(kControlRate);

    switch (patch_.osc)
    {
        case Osc::Voice: // the vowel breathes between its two shapes
        {
            vowelPhase_ += 0.09f / rate;
            vowelPhase_ -= std::floor(vowelPhase_);
            const float v = dsp::clamp01(0.5f + 0.5f * dsp::SineTable::at(vowelPhase_) + 0.3f * alive_ * wander_[1]);
            for (int i = 0; i < 3; ++i)
                formant_[i].set(patch_.formantA[i] + (patch_.formantB[i] - patch_.formantA[i]) * v, 0.92f, sampleRate_);
            break;
        }
        case Osc::Phase:
        {
            // The Cutoff knob and the filter envelope bend the phase; high notes bend less (less aliasing).
            const float bend = patch_.pdDepth + (knob - 0.5f) + patch_.pdEnv * macros.envAmount * 2.0f * filterEnv_.value() * (0.5f + 0.5f * velocity_);
            pdBend_ = dsp::clamp01(bend) * 0.8545f * std::min(1.0f, 3000.0f / hz);
            break;
        }
        case Osc::String: stringFb_ = std::pow(0.001f, 1.0f / (std::max(0.05f, patch_.stringDecay) * hz)); break;
        default: break;
    }
}

float SynthVoice::filter(float x)
{
    x = hp_.highpass(x);
    const float drive = std::max(0.1f, patch_.drive);
    switch (patch_.filter)
    {
        case FilterModel::Ladder: return ladder_.tick(x * 0.5f * drive, g_, reso_) * 2.0f / std::sqrt(drive);
        case FilterModel::Ota: return ota_.tick(x * 0.5f * drive, g_, reso_) * 2.0f / std::sqrt(drive);
        case FilterModel::Sem: // a resonant 12 dB peak can run far above the input: rounded off past ~1
        {
            const float y = sem_.tick(x * 0.5f * drive, g_, reso_, patch_.filterMode) * 2.0f / std::sqrt(drive);
            return 1.5f * dsp::fastTanh(y * (1.0f / 1.5f));
        }
        case FilterModel::None: break;
    }
    return drive > 1.0f ? dsp::drive(x, drive) : x;
}

float SynthVoice::tick(const SynthParams& macros, float glideCoef)
{
    if (!amp_.active())
        return 0.0f;
    pitch_ = targetPitch_ + (pitch_ - targetPitch_) * glideCoef;
    float pitch = pitch_ + detune_;
    if (pitchEnv_ > 1e-4f)
    {
        pitch += patch_.pitchDrop * pitchEnv_;
        pitchEnv_ *= pitchEnvCoef_;
    }
    if (patch_.vibratoHz > 0.0f)
    {
        vibPhase_ += patch_.vibratoHz / sampleRate_;
        vibPhase_ -= std::floor(vibPhase_);
        pitch += patch_.vibratoSemis * dsp::SineTable::at(vibPhase_);
    }
    if (pitch != hzPitch_)
    {
        hzPitch_ = pitch;
        hz_ = dsp::midiToHz(pitch);
    }
    filterEnv_.tick();
    if (--controlCounter_ <= 0)
    {
        control(macros);
        controlCounter_ = kControlRate;
    }
    g_ += gStep_;

    const float hz = hz_ * driftRatio_;
    float x = patch_.osc == Osc::Analog ? analog(hz) : patch_.osc == Osc::Fm4 ? fm4(hz) : special(hz);
    if (patch_.sub > 0.0f)
    {
        const float inc = std::min(0.5f * hz / sampleRate_, 0.45f);
        subPhase_ += inc;
        subPhase_ -= std::floor(subPhase_);
        x += patch_.sub * (patch_.subSquare ? 0.7f * dsp::squareBlep(subPhase_, inc) : dsp::SineTable::at(subPhase_));
    }
    if (hammerEnv_ > 1e-4f)
    {
        x += patch_.hammer * hammerEnv_ * drift_.next();
        hammerEnv_ *= hammerCoef_;
    }
    if (patch_.noise > 0.0f && patch_.osc != Osc::Voice) // the voices breathe through their formants
        x += patch_.noise * drift_.next();
    const float y = filter(x);
    return y * amp_.tick() * (0.35f + 0.65f * velocity_) * patch_.level;
}

// ---------------------------------------------------------------------------------

void Synth::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    std::uint32_t seed = 0x51ed27u;
    for (auto& v : voices_)
        v.prepare(2.0f * sampleRate, seed += 0x9e3779b9u);
    smoothCoef_ = 1.0f - std::exp(-1.0f / (0.012f * sampleRate)); // ~12 ms
    decimator_.reset();
    started_ = false;
    setParams(params_);
    reset();
}

void Synth::setAlive(float alive)
{
    for (auto& v : voices_)
        v.setAlive(dsp::clamp01(alive));
}

void Synth::setParams(const SynthParams& params)
{
    const SynthParams previous = params_;
    const SynthParams glided = live_;
    params_ = params;
    live_ = params;
    if (started_)
    {
        live_.level = glided.level;
        live_.cutoff = glided.cutoff;
        live_.cutoffMod = glided.cutoffMod;
        live_.resonance = glided.resonance;
    }
    const bool retone = started_ && params.sound == previous.sound
                        && (params.tone != previous.tone || params.filterChoice != previous.filterChoice
                            || params.fmAlgorithm != previous.fmAlgorithm);
    if (!started_ || params.sound != previous.sound || retone)
        patch_ = applyTone(synthPatch(params_.sound), params_);
    if (retone) // the sounding notes follow the knobs
        for (auto& v : voices_)
            if (v.active())
                v.retone(patch_);
    started_ = true;
    const float glideSeconds = 0.5f * params_.glide * params_.glide;
    glideCoef_ = glideSeconds < 0.001f ? 0.0f : std::exp(-1.0f / (glideSeconds * 2.0f * sampleRate_ / 3.0f));
    chorus_ = patch_.chorus;
}

void Synth::reset()
{
    for (auto& v : voices_)
        v.kill();
    decimator_.reset();
}

void Synth::noteOn(int id, float pitch, float velocity)
{
    // A note that starts while another is still held is a slide: glide, no retrigger.
    if (!poly_)
        for (auto& v : voices_)
            if (v.gated())
            {
                v.legato(id, pitch, velocity);
                return;
            }
    SynthVoice* target = nullptr;
    // Mono patches restart the voice still ringing out: no overlapping tails in the low
    // end, and the oscillators keep their phase (no click), like a monosynth.
    if (!poly_ && patch_.mono && last_ != nullptr && last_->active())
        target = last_;
    for (auto& v : voices_)
        if (target == nullptr && !v.active())
            target = &v;
    if (target == nullptr)
    {
        // All busy: steal the quietest of the released voices; if every voice is held,
        // the oldest. Never a voice just started (a chord's own notes would steal
        // each other: it starts silent, so it looks the quietest).
        for (auto& v : voices_)
            if (!v.gated() && (target == nullptr || v.level() < target->level()))
                target = &v;
        if (target == nullptr)
            for (auto& v : voices_)
                if (target == nullptr || v.order < target->order)
                    target = &v;
    }
    target->start(patch_, live_, id, pitch, velocity);
    target->order = ++starts_;
    last_ = target;
}

void Synth::noteOff(int id)
{
    for (auto& v : voices_)
        if (v.gated() && v.id() == id)
            v.release(live_);
}

void Synth::allNotesOff()
{
    for (auto& v : voices_)
        if (v.gated())
            v.release(live_);
}

float Synth::tick()
{
    auto glide = [this](float& value, float target) { value += (target - value) * smoothCoef_; };
    glide(live_.level, params_.level);
    glide(live_.cutoff, params_.cutoff);
    glide(live_.cutoffMod, params_.cutoffMod);
    glide(live_.resonance, params_.resonance);
    float a = 0.0f, b = 0.0f;
    for (auto& v : voices_)
    {
        if (!v.active())
            continue;
        a += v.tick(live_, glideCoef_);
        b += v.tick(live_, glideCoef_);
    }
    return decimator_.process(a, b); // the mixer applies level() after the part's inserts (a pre-fader tap)
}

int Synth::activeVoices() const
{
    int n = 0;
    for (const auto& v : voices_)
        n += v.active() ? 1 : 0;
    return n;
}

} // namespace gristo::sound

namespace gristo::sound {
int Synth::heldVoices() const
{
    int n = 0;
    for (const auto& v : voices_)
        n += v.gated() ? 1 : 0;
    return n;
}
} // namespace gristo::sound
