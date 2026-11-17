#include "Synth.h"

namespace gristo::sound {

namespace {

const SynthPatch* buildPatches()
{
    static SynthPatch p[static_cast<int>(SynthSound::Count)];
    auto set = [](SynthSound s) -> SynthPatch& { return p[static_cast<int>(s)]; };

    { // Sub: a clean sine with a little drive so it reads on small speakers.
        auto& x = set(SynthSound::SubBass);
        x.osc = Osc::Sine; x.cutoffHz = 800.0f; x.resonance = 0.0f; x.keyTrack = 0.0f;
        x.a = 0.003f; x.d = 0.4f; x.s = 0.9f; x.r = 0.08f; x.drive = 1.6f; x.level = 1.0f;
    }
    { // Acid: resonant saw with a snappy filter envelope; accents (velocity) squelch more.
        auto& x = set(SynthSound::AcidBass);
        x.osc = Osc::Saw; x.cutoffHz = 260.0f; x.resonance = 0.82f; x.envOctaves = 4.5f; x.keyTrack = 0.3f;
        x.fa = 0.001f; x.fd = 0.22f; x.fs = 0.0f; x.fr = 0.1f;
        x.a = 0.002f; x.d = 0.6f; x.s = 0.85f; x.r = 0.05f; x.drive = 1.6f; x.level = 0.8f;
    }
    { // Reese: two detuned saws + sub through a 4-pole lowpass.
        auto& x = set(SynthSound::ReeseBass);
        x.osc = Osc::Reese; x.detuneCents = 14.0f; x.sub = 0.5f; x.cutoffHz = 700.0f; x.resonance = 0.2f;
        x.envOctaves = 1.0f; x.fourPole = true; x.fa = 0.01f; x.fd = 0.8f; x.fs = 0.4f; x.fr = 0.3f;
        x.a = 0.005f; x.d = 0.5f; x.s = 1.0f; x.r = 0.15f; x.drive = 1.4f; x.level = 0.75f;
    }
    { // FM bass: bright attack that settles into a round tone.
        auto& x = set(SynthSound::FmBass);
        x.osc = Osc::Fm; x.fmRatio = 1.0f; x.fmIndex = 3.2f; x.fmIndexEnd = 0.7f; x.fmIndexTime = 0.2f; x.sub = 0.35f;
        x.cutoffHz = 3000.0f; x.resonance = 0.1f; x.a = 0.002f; x.d = 0.5f; x.s = 0.6f; x.r = 0.1f; x.level = 0.8f;
    }
    { // E-piano: 1:1 FM with a decaying index and a tine partial.
        auto& x = set(SynthSound::EPiano);
        x.osc = Osc::Fm; x.fmRatio = 1.0f; x.fmIndex = 1.6f; x.fmIndexEnd = 0.25f; x.fmIndexTime = 1.2f; x.tine = 0.18f;
        x.cutoffHz = 7000.0f; x.keyTrack = 0.0f; x.a = 0.002f; x.d = 2.4f; x.s = 0.0f; x.r = 0.4f; x.level = 0.8f;
    }
    { // Bell: inharmonic FM, long ring.
        auto& x = set(SynthSound::Bell);
        x.osc = Osc::Fm; x.fmRatio = 3.5f; x.fmIndex = 3.5f; x.fmIndexEnd = 0.4f; x.fmIndexTime = 1.5f;
        x.cutoffHz = 9000.0f; x.keyTrack = 0.0f; x.a = 0.001f; x.d = 3.0f; x.s = 0.0f; x.r = 1.5f; x.level = 0.6f;
    }
    { // Pluck: saw with a fast filter decay.
        auto& x = set(SynthSound::Pluck);
        x.osc = Osc::Saw; x.cutoffHz = 300.0f; x.resonance = 0.3f; x.envOctaves = 5.0f; x.keyTrack = 0.5f;
        x.fa = 0.001f; x.fd = 0.25f; x.fs = 0.0f; x.fr = 0.2f;
        x.a = 0.001f; x.d = 0.6f; x.s = 0.0f; x.r = 0.25f; x.level = 0.8f;
    }
    { // Organ: drawbar-style additive sines with a little vibrato.
        auto& x = set(SynthSound::Organ);
        x.osc = Osc::Organ; x.cutoffHz = 9000.0f; x.keyTrack = 0.0f;
        x.a = 0.004f; x.d = 0.1f; x.s = 1.0f; x.r = 0.06f; x.vibratoHz = 6.2f; x.vibratoSemis = 0.06f;
        x.drive = 1.2f; x.level = 0.5f;
    }
    { // Warm pad: detuned supersaw, slow filter swell.
        auto& x = set(SynthSound::WarmPad);
        x.osc = Osc::Supersaw; x.detuneCents = 18.0f; x.cutoffHz = 1300.0f; x.resonance = 0.15f; x.envOctaves = 1.0f;
        x.fourPole = true; x.fa = 0.9f; x.fd = 2.0f; x.fs = 0.5f; x.fr = 1.5f;
        x.a = 0.7f; x.d = 1.0f; x.s = 0.85f; x.r = 1.6f; x.level = 0.5f;
    }
    { // Glass pad: steady odd-ratio FM shimmer.
        auto& x = set(SynthSound::GlassPad);
        x.osc = Osc::Fm; x.fmRatio = 3.0f; x.fmIndex = 0.9f; x.fmIndexEnd = 0.9f; x.cutoffHz = 7000.0f; x.keyTrack = 0.0f;
        x.vibratoHz = 5.0f; x.vibratoSemis = 0.05f; x.a = 0.9f; x.d = 1.0f; x.s = 0.9f; x.r = 2.0f; x.level = 0.5f;
    }
    { // Dark pad: low supersaw + sub, slow filter.
        auto& x = set(SynthSound::DarkPad);
        x.osc = Osc::Supersaw; x.detuneCents = 10.0f; x.sub = 0.3f; x.cutoffHz = 450.0f; x.resonance = 0.3f;
        x.envOctaves = 0.8f; x.fourPole = true; x.fa = 2.5f; x.fd = 3.0f; x.fs = 0.6f; x.fr = 2.0f;
        x.a = 1.2f; x.d = 1.0f; x.s = 0.9f; x.r = 2.5f; x.level = 0.6f;
    }
    { // Square lead.
        auto& x = set(SynthSound::SquareLead);
        x.osc = Osc::Square; x.cutoffHz = 2600.0f; x.resonance = 0.2f; x.envOctaves = 1.5f;
        x.fa = 0.002f; x.fd = 0.3f; x.fs = 0.4f; x.fr = 0.2f;
        x.a = 0.005f; x.d = 0.3f; x.s = 0.9f; x.r = 0.15f; x.vibratoHz = 5.5f; x.vibratoSemis = 0.12f; x.level = 0.55f;
    }
    { // Sync lead: hard-synced saw swept by the filter envelope.
        auto& x = set(SynthSound::SyncLead);
        x.osc = Osc::Sync; x.syncAmount = 2.5f; x.cutoffHz = 4500.0f; x.resonance = 0.2f; x.envOctaves = 0.8f;
        x.fa = 0.001f; x.fd = 0.4f; x.fs = 0.3f; x.fr = 0.2f;
        x.a = 0.003f; x.d = 0.3f; x.s = 0.85f; x.r = 0.12f; x.level = 0.5f;
    }
    { // Soft lead: sine with a touch of FM and vibrato.
        auto& x = set(SynthSound::SoftLead);
        x.osc = Osc::Fm; x.fmRatio = 1.0f; x.fmIndex = 0.35f; x.fmIndexEnd = 0.35f; x.cutoffHz = 3500.0f;
        x.vibratoHz = 5.0f; x.vibratoSemis = 0.1f; x.a = 0.03f; x.d = 0.3f; x.s = 0.9f; x.r = 0.25f; x.level = 0.7f;
    }
    // Ambient (Harold Budd territory: soft attacks, long tails, lots of reverb).
    { // Felt piano: a muted, rounded piano-like tone with a soft hammer.
        auto& x = set(SynthSound::FeltPiano);
        x.osc = Osc::Fm; x.fmRatio = 1.0f; x.fmIndex = 1.1f; x.fmIndexEnd = 0.08f; x.fmIndexTime = 0.45f; x.tine = 0.05f;
        x.cutoffHz = 2200.0f; x.keyTrack = 0.4f; x.a = 0.006f; x.d = 3.5f; x.s = 0.0f; x.r = 0.9f; x.level = 0.85f;
    }
    { // Celesta: soft, glassy bell with a quick shimmer.
        auto& x = set(SynthSound::Celesta);
        x.osc = Osc::Fm; x.fmRatio = 4.0f; x.fmIndex = 1.4f; x.fmIndexEnd = 0.15f; x.fmIndexTime = 0.5f;
        x.cutoffHz = 8000.0f; x.keyTrack = 0.0f; x.a = 0.002f; x.d = 2.2f; x.s = 0.0f; x.r = 1.2f; x.level = 0.6f;
    }
    { // Breath pad: airy, choir-ish swell with a slow vibrato.
        auto& x = set(SynthSound::BreathPad);
        x.osc = Osc::Fm; x.fmRatio = 2.0f; x.fmIndex = 0.55f; x.fmIndexEnd = 0.55f; x.cutoffHz = 2000.0f; x.resonance = 0.1f;
        x.keyTrack = 0.3f; x.vibratoHz = 4.5f; x.vibratoSemis = 0.08f;
        x.a = 1.5f; x.d = 1.0f; x.s = 0.9f; x.r = 2.2f; x.level = 0.55f;
    }
    { // Slow strings: gentle ensemble that swells in.
        auto& x = set(SynthSound::SlowStrings);
        x.osc = Osc::Supersaw; x.detuneCents = 12.0f; x.cutoffHz = 2600.0f; x.resonance = 0.1f; x.fourPole = true;
        x.envOctaves = 0.5f; x.fa = 1.2f; x.fd = 2.0f; x.fs = 0.7f; x.fr = 2.0f; x.vibratoHz = 5.5f; x.vibratoSemis = 0.05f;
        x.a = 1.2f; x.d = 1.0f; x.s = 0.9f; x.r = 2.0f; x.level = 0.45f;
    }
    // Vintage analog.
    { // Poly strings: bright chorused string machine.
        auto& x = set(SynthSound::PolyStrings);
        x.osc = Osc::Supersaw; x.detuneCents = 8.0f; x.cutoffHz = 3500.0f; x.resonance = 0.05f;
        x.a = 0.25f; x.d = 1.0f; x.s = 0.9f; x.r = 0.8f; x.vibratoHz = 5.8f; x.vibratoSemis = 0.04f; x.level = 0.45f;
    }
    { // Synth brass: saw with a filter that blooms on the attack.
        auto& x = set(SynthSound::SynthBrass);
        x.osc = Osc::Reese; x.detuneCents = 6.0f; x.cutoffHz = 600.0f; x.resonance = 0.15f; x.envOctaves = 3.0f; x.keyTrack = 0.5f;
        x.fa = 0.03f; x.fd = 0.35f; x.fs = 0.4f; x.fr = 0.15f;
        x.a = 0.02f; x.d = 0.4f; x.s = 0.8f; x.r = 0.15f; x.drive = 1.3f; x.level = 0.55f;
    }
    { // Poly synth: two detuned saws, a little filter movement, medium decay.
        auto& x = set(SynthSound::PolySynth);
        x.osc = Osc::Reese; x.detuneCents = 9.0f; x.cutoffHz = 1800.0f; x.resonance = 0.2f; x.envOctaves = 1.5f;
        x.fa = 0.005f; x.fd = 0.6f; x.fs = 0.3f; x.fr = 0.4f;
        x.a = 0.005f; x.d = 1.0f; x.s = 0.6f; x.r = 0.5f; x.level = 0.5f;
    }
    { // Tape keys: e-piano with tape wow.
        auto& x = set(SynthSound::TapeKeys);
        x.osc = Osc::Fm; x.fmRatio = 1.0f; x.fmIndex = 1.2f; x.fmIndexEnd = 0.2f; x.fmIndexTime = 1.0f; x.tine = 0.1f;
        x.cutoffHz = 3000.0f; x.keyTrack = 0.0f; x.vibratoHz = 0.6f; x.vibratoSemis = 0.15f;
        x.a = 0.003f; x.d = 2.2f; x.s = 0.0f; x.r = 0.5f; x.drive = 1.3f; x.level = 0.8f;
    }
    // Jazz keys and bass.
    { // Suitcase keys: brighter, barkier tine piano.
        auto& x = set(SynthSound::SuitcaseKeys);
        x.osc = Osc::Fm; x.fmRatio = 1.0f; x.fmIndex = 2.4f; x.fmIndexEnd = 0.4f; x.fmIndexTime = 0.8f; x.tine = 0.3f;
        x.cutoffHz = 6000.0f; x.keyTrack = 0.0f; x.a = 0.002f; x.d = 2.6f; x.s = 0.0f; x.r = 0.45f; x.drive = 1.4f; x.level = 0.75f;
    }
    { // Vibraphone: mallet partials with motor vibrato.
        auto& x = set(SynthSound::Vibraphone);
        x.osc = Osc::Fm; x.fmRatio = 4.0f; x.fmIndex = 0.8f; x.fmIndexEnd = 0.1f; x.fmIndexTime = 0.3f;
        x.cutoffHz = 7000.0f; x.keyTrack = 0.0f; x.vibratoHz = 5.5f; x.vibratoSemis = 0.03f;
        x.a = 0.002f; x.d = 3.0f; x.s = 0.0f; x.r = 1.0f; x.level = 0.7f;
    }
    { // Upright bass: woody pluck that dies away.
        auto& x = set(SynthSound::UprightBass);
        x.osc = Osc::Fm; x.fmRatio = 1.0f; x.fmIndex = 1.5f; x.fmIndexEnd = 0.3f; x.fmIndexTime = 0.08f; x.sub = 0.2f;
        x.cutoffHz = 900.0f; x.keyTrack = 0.3f; x.a = 0.004f; x.d = 1.2f; x.s = 0.0f; x.r = 0.12f; x.level = 0.95f;
    }
    { // Jazz organ: drawbars with a faster vibrato and some grit.
        auto& x = set(SynthSound::JazzOrgan);
        x.osc = Osc::Organ; x.cutoffHz = 4500.0f; x.keyTrack = 0.0f;
        x.a = 0.003f; x.d = 0.1f; x.s = 1.0f; x.r = 0.05f; x.vibratoHz = 6.8f; x.vibratoSemis = 0.1f;
        x.drive = 1.8f; x.level = 0.45f;
    }
    { // 808: a sine that drops onto its note and saturates; overlapping notes slide.
        auto& x = set(SynthSound::Bass808);
        x.osc = Osc::Sine; x.pitchDrop = 12.0f; x.cutoffHz = 1400.0f; x.resonance = 0.0f; x.keyTrack = 0.0f;
        x.a = 0.002f; x.d = 3.5f; x.s = 0.0f; x.r = 0.3f; x.drive = 3.2f; x.level = 0.95f; x.mono = true;
    }
    { // Monk choir: low voices on "om", the vowel slowly closing and opening.
        auto& x = set(SynthSound::MonkChoir);
        x.osc = Osc::Voice; x.detuneCents = 9.0f; x.cutoffHz = 6000.0f; x.keyTrack = 0.0f;
        x.a = 0.5f; x.d = 1.0f; x.s = 0.9f; x.r = 1.8f; x.vibratoHz = 4.8f; x.vibratoSemis = 0.07f;
        x.noise = 0.02f; x.chorus = 0.6f; x.driftCents = 5.0f; x.freePhase = true; x.level = 0.9f;
    }
    { // Vox pad: airy "ah" to "oh", higher and breathier.
        auto& x = set(SynthSound::VoxPad);
        x.osc = Osc::Voice; x.detuneCents = 12.0f; x.cutoffHz = 8000.0f; x.keyTrack = 0.0f;
        const float a[3] { 800.0f, 1200.0f, 2800.0f }, b[3] { 500.0f, 900.0f, 2700.0f };
        for (int i = 0; i < 3; ++i) { x.formantA[i] = a[i]; x.formantB[i] = b[i]; }
        x.a = 0.9f; x.d = 1.0f; x.s = 0.9f; x.r = 2.5f; x.vibratoHz = 5.2f; x.vibratoSemis = 0.05f;
        x.noise = 0.08f; x.chorus = 0.7f; x.driftCents = 4.0f; x.freePhase = true; x.level = 0.85f;
    }
    { // Singing bowl: inharmonic partials that beat slowly, the high ones dying first.
        auto& x = set(SynthSound::SingingBowl);
        x.osc = Osc::Bowl; x.fmIndexTime = 3.0f; x.cutoffHz = 10000.0f; x.keyTrack = 0.0f;
        x.a = 0.004f; x.d = 9.0f; x.s = 0.0f; x.r = 4.0f; x.hammer = 0.05f; x.chorus = 0.2f; x.level = 0.7f;
    }
    { // Nylon guitar: a soft plucked string.
        auto& x = set(SynthSound::NylonGuitar);
        x.osc = Osc::String; x.stringDamp = 0.38f; x.stringDecay = 2.5f; x.cutoffHz = 5000.0f; x.keyTrack = 0.3f;
        x.a = 0.001f; x.d = 6.0f; x.s = 0.0f; x.r = 0.25f; x.chorus = 0.1f; x.level = 0.9f;
    }
    { // Koto: a bright, long plucked string.
        auto& x = set(SynthSound::Koto);
        x.osc = Osc::String; x.stringDamp = 0.12f; x.stringDecay = 4.5f; x.cutoffHz = 9000.0f; x.keyTrack = 0.2f;
        x.a = 0.001f; x.d = 8.0f; x.s = 0.0f; x.r = 0.6f; x.hammer = 0.05f; x.level = 0.8f;
    }
    { // Phase bass: a bent saw that snaps shut, with a sub.
        auto& x = set(SynthSound::PhaseBass);
        x.osc = Osc::Phase; x.pdWave = 0; x.pdDepth = 0.25f; x.pdEnv = 0.7f; x.sub = 0.4f; x.cutoffHz = 9000.0f; x.keyTrack = 0.0f;
        x.fa = 0.001f; x.fd = 0.25f; x.fs = 0.1f; x.fr = 0.1f;
        x.a = 0.002f; x.d = 0.6f; x.s = 0.8f; x.r = 0.08f; x.level = 0.8f; x.mono = true;
    }
    { // Reso lead: the resonant saw window, swept by the envelope.
        auto& x = set(SynthSound::ResoLead);
        x.osc = Osc::Phase; x.pdWave = 5; x.pdDepth = 0.2f; x.pdEnv = 0.8f; x.pdDetune = 7.0f; x.cutoffHz = 10000.0f; x.keyTrack = 0.0f;
        x.fa = 0.005f; x.fd = 0.5f; x.fs = 0.3f; x.fr = 0.2f;
        x.a = 0.004f; x.d = 0.4f; x.s = 0.9f; x.r = 0.15f; x.level = 0.75f; x.mono = true;
    }
    { // Phase pad: double sine and saw on alternate cycles, two detuned lines, a slow bend.
        auto& x = set(SynthSound::PhasePad);
        x.osc = Osc::Phase; x.pdWave = 3; x.pdWave2 = 0; x.pdDepth = 0.2f; x.pdEnv = 0.5f; x.pdDetune = 11.0f;
        x.cutoffHz = 9000.0f; x.keyTrack = 0.0f; x.fa = 1.5f; x.fd = 3.0f; x.fs = 0.5f; x.fr = 2.0f;
        x.a = 0.8f; x.d = 1.0f; x.s = 0.9f; x.r = 2.0f; x.chorus = 0.6f; x.driftCents = 4.0f; x.freePhase = true; x.level = 0.7f;
    }
    { // Phase keys: ring-modulated resonant triangles, a glassy struck tone.
        auto& x = set(SynthSound::PhaseKeys);
        x.osc = Osc::Phase; x.pdWave = 6; x.pdDepth = 0.15f; x.pdEnv = 0.9f; x.pdDetune = 1200.0f; x.pdRing = true;
        x.cutoffHz = 10000.0f; x.keyTrack = 0.0f; x.fa = 0.001f; x.fd = 0.9f; x.fs = 0.0f; x.fr = 0.4f;
        x.a = 0.002f; x.d = 2.5f; x.s = 0.0f; x.r = 0.6f; x.chorus = 0.25f; x.hammer = 0.04f; x.level = 0.9f;
    }
    { // Fold drone: a bent pulse through the wavefolder; Cutoff walks through the folds.
        auto& x = set(SynthSound::FoldDrone);
        x.osc = Osc::Phase; x.pdWave = 4; x.pdWave2 = 3; x.pdDepth = 0.3f; x.pdEnv = 0.3f; x.pdDetune = 5.0f; x.pdFold = 0.35f;
        x.sub = 0.3f; x.cutoffHz = 6000.0f; x.keyTrack = 0.0f; x.fa = 2.0f; x.fd = 4.0f; x.fs = 0.6f; x.fr = 2.0f;
        x.a = 1.2f; x.d = 1.0f; x.s = 1.0f; x.r = 3.0f; x.chorus = 0.4f; x.driftCents = 3.0f; x.freePhase = true; x.level = 0.55f;
    }
    // ---- TRIO
    auto trio = [&](SynthSound sound, int setIndex, float int2, float int3, float detune, int mode, float cutoffHz, float reso, float pw) -> SynthPatch& {
        auto& x = set(sound);
        x.osc = Osc::Trio; x.trioSet = setIndex; x.trioInt2 = int2; x.trioInt3 = int3; x.trioDetune = detune;
        x.filterMode = mode; x.cutoffHz = cutoffHz; x.resonance = reso; x.trioPw = pw; x.keyTrack = 0.4f;
        return x;
    };
    { auto& x = trio(SynthSound::TrioBass, 0, 0.0f, -12.0f, 9.0f, 0, 700.0f, 0.35f, 0.5f);
      x.envOctaves = 2.5f; x.fa = 0.001f; x.fd = 0.3f; x.fs = 0.2f; x.fr = 0.1f; x.a = 0.002f; x.d = 0.6f; x.s = 0.6f; x.r = 0.1f; x.mono = true; x.level = 0.8f; }
    { auto& x = trio(SynthSound::TrioSync, 9, 9.0f, 0.0f, 0.0f, 0, 2500.0f, 0.25f, 0.5f);
      x.syncAmount = 0.0f; x.envOctaves = 1.5f; x.fa = 0.005f; x.fd = 0.5f; x.fs = 0.4f; x.fr = 0.2f; x.a = 0.004f; x.d = 0.5f; x.s = 0.9f; x.r = 0.2f; x.mono = true; x.level = 0.7f; }
    { auto& x = trio(SynthSound::RingBell, 13, 0.0f, 18.0f, 6.0f, 1, 4000.0f, 0.25f, 0.5f);
      x.a = 0.001f; x.d = 2.5f; x.s = 0.0f; x.r = 1.2f; x.chorus = 0.2f; x.level = 0.8f; }
    { auto& x = trio(SynthSound::ChipChoir, 1, 0.0f, 12.0f, 7.0f, 1, 900.0f, 0.75f, 0.35f);
      x.envOctaves = 0.8f; x.fa = 0.6f; x.fd = 1.5f; x.fs = 0.7f; x.fr = 1.0f; x.a = 0.4f; x.d = 1.0f; x.s = 0.9f; x.r = 1.2f; x.chorus = 0.5f; x.freePhase = true; x.level = 0.6f; }
    { auto& x = trio(SynthSound::TrioArp, 2, 12.0f, 0.0f, 4.0f, 0, 3000.0f, 0.2f, 0.25f);
      x.envOctaves = 1.0f; x.fa = 0.001f; x.fd = 0.25f; x.fs = 0.3f; x.fr = 0.15f; x.a = 0.001f; x.d = 0.5f; x.s = 0.7f; x.r = 0.15f; x.level = 0.7f; }
    // ---- LOFI
    auto lofi = [&](SynthSound sound, int chip, int wave, float duty) -> SynthPatch& {
        auto& x = set(sound);
        x.osc = Osc::Lofi; x.lofiChip = chip; x.lofiWave = wave; x.lofiDuty = duty; x.noFilter = true;
        return x;
    };
    { auto& x = lofi(SynthSound::ChipLead, 0, 0, 0.3f); x.lofiVib = 0.15f; x.lofiTone = 1.0f;
      x.a = 0.001f; x.d = 0.6f; x.s = 0.7f; x.r = 0.12f; x.mono = true; x.level = 0.55f; }
    { auto& x = lofi(SynthSound::ChipBass, 1, 1, 0.0f); x.lofiTone = 0.7f;
      x.a = 0.001f; x.d = 0.5f; x.s = 0.6f; x.r = 0.08f; x.mono = true; x.level = 0.75f; }
    { auto& x = lofi(SynthSound::ChipArp, 2, 0, 0.75f); x.lofiArp = 2; x.lofiTone = 0.85f;
      x.a = 0.001f; x.d = 0.6f; x.s = 0.6f; x.r = 0.15f; x.level = 0.5f; }
    { auto& x = lofi(SynthSound::WaveVox, 0, 4, 11.5f / 16.0f); x.lofiVib = 0.12f; x.lofiTone = 0.9f;
      x.a = 0.02f; x.d = 0.8f; x.s = 0.8f; x.r = 0.25f; x.chorus = 0.3f; x.level = 0.6f; }
    // ---- FM6 (ops listed 1..6; level 0..1, ratio, a d s r, velocity)
    auto fm = [&](SynthSound sound, int alg, float feedback) -> SynthPatch& {
        auto& x = set(sound);
        x.osc = Osc::Fm6; x.fmAlg = alg; x.fmFeedback = feedback; x.noFilter = true; x.keyTrack = 0.0f;
        return x;
    };
    auto op = [](float ratio, float level, float a, float d, float s, float r, float velocity, float detune = 0.0f) {
        FmOperator o;
        o.ratio = ratio; o.level = level; o.a = a; o.d = d; o.s = s; o.r = r; o.velocity = velocity; o.detuneCents = detune;
        return o;
    };
    { // E-piano: algorithm 5, three pairs; the tine pair has a fast, bright attack.
        auto& x = fm(SynthSound::FmRhodes, 4, 0.3f);
        x.fm[0] = op(1.0f, 0.9f, 0.002f, 3.0f, 0.0f, 0.5f, 0.3f, 2.0f);  x.fm[1] = op(1.0f, 0.35f, 0.002f, 1.2f, 0.1f, 0.4f, 0.8f);
        x.fm[2] = op(1.0f, 0.9f, 0.002f, 3.5f, 0.0f, 0.5f, 0.3f, -2.0f); x.fm[3] = op(14.0f, 0.18f, 0.001f, 0.3f, 0.0f, 0.2f, 0.9f);
        x.fm[4] = op(1.0f, 0.6f, 0.002f, 2.5f, 0.0f, 0.5f, 0.4f);        x.fm[5] = op(1.0f, 0.2f, 0.002f, 1.5f, 0.0f, 0.3f, 0.8f);
        x.a = 0.001f; x.d = 1.0f; x.s = 1.0f; x.r = 0.5f; x.chorus = 0.35f; x.level = 0.8f;
    }
    { // Bass: algorithm 5, a punchy 1:1 pair and a 0.5 sub.
        auto& x = fm(SynthSound::DxBass, 4, 0.5f);
        x.fm[0] = op(0.5f, 0.95f, 0.001f, 1.5f, 0.6f, 0.1f, 0.2f);       x.fm[1] = op(0.5f, 0.55f, 0.001f, 0.25f, 0.15f, 0.1f, 0.7f);
        x.fm[2] = op(1.0f, 0.6f, 0.001f, 0.8f, 0.3f, 0.1f, 0.3f);        x.fm[3] = op(3.0f, 0.3f, 0.001f, 0.12f, 0.0f, 0.1f, 0.9f);
        x.fm[4] = op(0.5f, 0.0f, 0.001f, 1.0f, 1.0f, 0.1f, 0.0f);        x.fm[5] = op(1.0f, 0.0f, 0.001f, 1.0f, 1.0f, 0.1f, 0.0f);
        x.a = 0.001f; x.d = 1.0f; x.s = 1.0f; x.r = 0.1f; x.mono = true; x.level = 0.85f;
    }
    { // Tubular bells: algorithm 5, inharmonic ratios, long ring.
        auto& x = fm(SynthSound::FmBells, 4, 0.0f);
        x.fm[0] = op(1.0f, 0.9f, 0.001f, 6.0f, 0.0f, 3.0f, 0.3f);        x.fm[1] = op(3.5f, 0.45f, 0.001f, 4.0f, 0.0f, 2.0f, 0.6f);
        x.fm[2] = op(1.0f, 0.6f, 0.001f, 5.0f, 0.0f, 3.0f, 0.3f, 4.0f);  x.fm[3] = op(3.5f, 0.35f, 0.001f, 3.0f, 0.0f, 2.0f, 0.6f, 3.0f);
        x.fm[4] = op(2.0f, 0.4f, 0.001f, 2.0f, 0.0f, 1.5f, 0.4f);        x.fm[5] = op(7.07f, 0.3f, 0.001f, 1.0f, 0.0f, 1.0f, 0.7f);
        x.a = 0.001f; x.d = 8.0f; x.s = 1.0f; x.r = 3.0f; x.chorus = 0.2f; x.level = 0.6f;
    }
    { // Marimba: algorithm 7, short woody modulators.
        auto& x = fm(SynthSound::FmMarimba, 6, 0.0f);
        x.fm[0] = op(1.0f, 0.95f, 0.001f, 0.8f, 0.0f, 0.3f, 0.3f);       x.fm[1] = op(4.0f, 0.4f, 0.001f, 0.05f, 0.0f, 0.05f, 0.8f);
        x.fm[2] = op(3.0f, 0.4f, 0.001f, 0.6f, 0.0f, 0.2f, 0.4f);        x.fm[3] = op(1.0f, 0.2f, 0.001f, 0.1f, 0.0f, 0.1f, 0.7f);
        x.fm[4] = op(10.0f, 0.15f, 0.001f, 0.03f, 0.0f, 0.03f, 0.9f);    x.fm[5] = op(1.0f, 0.0f, 0.001f, 1.0f, 0.0f, 0.1f, 0.0f);
        x.a = 0.001f; x.d = 1.0f; x.s = 1.0f; x.r = 0.3f; x.level = 0.8f;
    }
    { // Brass: algorithm 22, the modulators swell in after the carriers.
        auto& x = fm(SynthSound::FmBrass, 21, 0.6f);
        x.fm[0] = op(1.0f, 0.8f, 0.06f, 2.0f, 0.85f, 0.2f, 0.3f);        x.fm[1] = op(1.0f, 0.45f, 0.12f, 1.0f, 0.6f, 0.2f, 0.6f);
        x.fm[2] = op(1.0f, 0.6f, 0.06f, 2.0f, 0.85f, 0.2f, 0.3f, 5.0f);  x.fm[3] = op(1.0f, 0.6f, 0.06f, 2.0f, 0.85f, 0.2f, 0.3f, -5.0f);
        x.fm[4] = op(1.0f, 0.6f, 0.06f, 2.0f, 0.85f, 0.2f, 0.3f, 9.0f);  x.fm[5] = op(1.0f, 0.5f, 0.15f, 1.0f, 0.55f, 0.2f, 0.6f);
        x.a = 0.001f; x.d = 1.0f; x.s = 1.0f; x.r = 0.25f; x.chorus = 0.3f; x.level = 0.55f;
    }
    { // Glass pad: algorithm 32 (all carriers) at harmonic and odd ratios, slow, with feedback shimmer.
        auto& x = fm(SynthSound::FmGlass, 4, 0.2f);
        x.fm[0] = op(1.0f, 0.8f, 1.2f, 3.0f, 0.8f, 2.5f, 0.2f, 3.0f);    x.fm[1] = op(2.0f, 0.25f, 1.5f, 3.0f, 0.6f, 2.5f, 0.3f);
        x.fm[2] = op(1.0f, 0.8f, 1.0f, 3.0f, 0.8f, 2.5f, 0.2f, -3.0f);   x.fm[3] = op(5.0f, 0.12f, 2.0f, 3.0f, 0.5f, 2.5f, 0.3f);
        x.fm[4] = op(2.0f, 0.4f, 1.4f, 3.0f, 0.8f, 2.5f, 0.2f, 7.0f);    x.fm[5] = op(3.0f, 0.15f, 1.6f, 3.0f, 0.5f, 2.5f, 0.3f);
        x.a = 0.3f; x.d = 1.0f; x.s = 1.0f; x.r = 2.5f; x.chorus = 0.6f; x.freePhase = true; x.level = 0.6f;
    }
    // Phase distortion shapes its own tone: no filter after it.
    for (auto s : { SynthSound::PhaseBass, SynthSound::ResoLead, SynthSound::PhasePad, SynthSound::PhaseKeys, SynthSound::FoldDrone })
        set(s).noFilter = true;
    // Stereo ensemble and analog drift, by sound (basses stay mono and in tune).
    struct Colour
    {
        SynthSound sound;
        float chorus, drift;
    };
    const Colour colours[] = {
        { SynthSound::EPiano, 0.35f, 0.0f },      { SynthSound::Bell, 0.2f, 0.0f },         { SynthSound::Pluck, 0.25f, 2.0f },
        { SynthSound::Organ, 0.2f, 0.0f },        { SynthSound::WarmPad, 0.6f, 4.0f },      { SynthSound::GlassPad, 0.4f, 0.0f },
        { SynthSound::DarkPad, 0.5f, 3.0f },      { SynthSound::SoftLead, 0.15f, 0.0f },    { SynthSound::FeltPiano, 0.15f, 0.0f },
        { SynthSound::Celesta, 0.25f, 0.0f },     { SynthSound::BreathPad, 0.5f, 0.0f },    { SynthSound::SlowStrings, 0.7f, 4.0f },
        { SynthSound::PolyStrings, 0.8f, 3.0f },  { SynthSound::SynthBrass, 0.35f, 4.0f },  { SynthSound::PolySynth, 0.5f, 3.0f },
        { SynthSound::TapeKeys, 0.3f, 0.0f },     { SynthSound::SuitcaseKeys, 0.45f, 0.0f }, { SynthSound::Vibraphone, 0.15f, 0.0f },
        { SynthSound::JazzOrgan, 0.3f, 0.0f },
    };
    for (const auto& c : colours)
    {
        set(c.sound).chorus = c.chorus;
        set(c.sound).driftCents = c.drift;
    }
    set(SynthSound::GlassPad).velocityToFm = 0.2f; // pads keep their tone
    set(SynthSound::BreathPad).velocityToFm = 0.2f;

    // Monosynth behaviour for basses and leads; free-running oscillators for pads.
    for (auto s : { SynthSound::SubBass, SynthSound::AcidBass, SynthSound::ReeseBass, SynthSound::FmBass, SynthSound::UprightBass,
                    SynthSound::SquareLead, SynthSound::SyncLead, SynthSound::SoftLead })
        set(s).mono = true;
    for (auto s : { SynthSound::ReeseBass, SynthSound::WarmPad, SynthSound::DarkPad, SynthSound::SlowStrings, SynthSound::PolyStrings,
                    SynthSound::PolySynth, SynthSound::SynthBrass })
        set(s).freePhase = true;
    // The ladder for the classic subtractive sounds (acid squelch, brass, poly).
    for (auto s : { SynthSound::AcidBass, SynthSound::SynthBrass, SynthSound::PolySynth })
        set(s).fourPole = true;
    set(SynthSound::AcidBass).cutoffHz = 320.0f; // the ladder's steeper slope sits darker
    // Attack noise (hammer, mallet, finger, key click) and breath / bow noise.
    const std::pair<SynthSound, float> hammers[] = { { SynthSound::EPiano, 0.06f },      { SynthSound::SuitcaseKeys, 0.08f },
                                                     { SynthSound::TapeKeys, 0.05f },    { SynthSound::FeltPiano, 0.12f },
                                                     { SynthSound::Vibraphone, 0.05f },  { SynthSound::Celesta, 0.04f },
                                                     { SynthSound::UprightBass, 0.15f }, { SynthSound::Organ, 0.04f },
                                                     { SynthSound::JazzOrgan, 0.06f },   { SynthSound::Bell, 0.03f } };
    for (const auto& [sound, level] : hammers)
        set(sound).hammer = level;
    set(SynthSound::BreathPad).noise = 0.12f;
    set(SynthSound::SlowStrings).noise = 0.015f;
    set(SynthSound::PolyStrings).noise = 0.01f;
    set(SynthSound::SynthBrass).noise = 0.02f;

    // Real-instrument tremolos: the vibraphone motor, the suitcase e-piano's stereo
    // vibrato, a rotary speaker on the organs.
    {
        auto& x = set(SynthSound::Vibraphone);
        x.vibratoHz = 0.0f; // the motor moves the level, not the pitch
        x.tremolo = 0.3f;
        x.tremoloHz = 5.5f;
    }
    {
        auto& x = set(SynthSound::SuitcaseKeys);
        x.tremolo = 0.45f;
        x.tremoloHz = 4.6f;
        x.tremoloPan = true;
        x.chorus = 0.2f;
    }
    for (auto s : { SynthSound::Organ, SynthSound::JazzOrgan })
    {
        auto& x = set(s);
        x.tremolo = s == SynthSound::JazzOrgan ? 0.3f : 0.2f;
        x.tremoloHz = x.vibratoHz;
        x.tremoloPan = true;
    }
    return p;
}

constexpr float kOrganLevels[6] = { 1.0f, 0.7f, 0.45f, 0.3f, 0.18f, 0.12f };
constexpr float kOrganHarmonics[6] = { 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f };

float attackTime(const SynthPatch& p, const SynthParams& m) { return dsp::macroLog(m.attack, 0.001f, std::max(p.a, 0.001f), 6.0f); }
float releaseTime(const SynthPatch& p, const SynthParams& m) { return dsp::macroLog(m.release, 0.01f, std::max(p.r, 0.01f), 8.0f); }

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
// Phase distortion, after the PHASE engine of Felucca-Engines
// (Copyright (C) 2026 Leo Kuroshita, Hügelton Instruments; MIT licence): its eight
// waves and its fold, in floating point.

namespace {
float pdWave(int wave, float ph, float dcw)
{
    auto negCos = [](float p) { return -std::cos(dsp::kTwoPi * p); };
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
} // namespace

// ---------------------------------------------------------------------------------
// TRIO, LOFI and FM6, after the engines of Felucca (Copyright (C) 2026 Leo Kuroshita,
// Hügelton Instruments): TRIO and LOFI from Felucca-Engines (MIT), FM6's algorithm table
// from msfa (Copyright 2012 Google Inc., 2016-2025 Pascal Gauthier; Apache-2.0) as
// Felucca carries it. Ported to floating point; envelopes and patches are Gristo's.

namespace {
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

// The 32 algorithms: per operator, the sixth first: in bus, out bus, add, feedback (msfa).
enum { FmOb1 = 1, FmOb2 = 2, FmOadd = 4, FmIb1 = 16, FmIb2 = 32 };
const std::uint8_t kFmAlg[32][6] = {
    { 0xc1, 0x11, 0x11, 0x14, 0x01, 0x14 }, { 0x01, 0x11, 0x11, 0x14, 0xc1, 0x14 }, { 0xc1, 0x11, 0x14, 0x01, 0x11, 0x14 },
    { 0xc1, 0x11, 0x94, 0x01, 0x11, 0x14 }, { 0xc1, 0x14, 0x01, 0x14, 0x01, 0x14 }, { 0xc1, 0x94, 0x01, 0x14, 0x01, 0x14 },
    { 0xc1, 0x11, 0x05, 0x14, 0x01, 0x14 }, { 0x01, 0x11, 0xc5, 0x14, 0x01, 0x14 }, { 0x01, 0x11, 0x05, 0x14, 0xc1, 0x14 },
    { 0x01, 0x05, 0x14, 0xc1, 0x11, 0x14 }, { 0xc1, 0x05, 0x14, 0x01, 0x11, 0x14 }, { 0x01, 0x05, 0x05, 0x14, 0xc1, 0x14 },
    { 0xc1, 0x05, 0x05, 0x14, 0x01, 0x14 }, { 0xc1, 0x05, 0x11, 0x14, 0x01, 0x14 }, { 0x01, 0x05, 0x11, 0x14, 0xc1, 0x14 },
    { 0xc1, 0x11, 0x02, 0x25, 0x05, 0x14 }, { 0x01, 0x11, 0x02, 0x25, 0xc5, 0x14 }, { 0x01, 0x11, 0x11, 0xc5, 0x05, 0x14 },
    { 0xc1, 0x14, 0x14, 0x01, 0x11, 0x14 }, { 0x01, 0x05, 0x14, 0xc1, 0x14, 0x14 }, { 0x01, 0x14, 0x14, 0xc1, 0x14, 0x14 },
    { 0xc1, 0x14, 0x14, 0x14, 0x01, 0x14 }, { 0xc1, 0x14, 0x14, 0x01, 0x14, 0x04 }, { 0xc1, 0x14, 0x14, 0x14, 0x04, 0x04 },
    { 0xc1, 0x14, 0x14, 0x04, 0x04, 0x04 }, { 0xc1, 0x05, 0x14, 0x01, 0x14, 0x04 }, { 0x01, 0x05, 0x14, 0xc1, 0x14, 0x04 },
    { 0x04, 0xc1, 0x11, 0x14, 0x01, 0x14 }, { 0xc1, 0x14, 0x01, 0x14, 0x04, 0x04 }, { 0x04, 0xc1, 0x11, 0x14, 0x04, 0x04 },
    { 0xc1, 0x14, 0x04, 0x04, 0x04, 0x04 }, { 0xc4, 0x04, 0x04, 0x04, 0x04, 0x04 } };
int fmCarrierCount(int alg)
{
    int n = 0;
    for (int k = 0; k < 6; ++k)
        n += (kFmAlg[alg & 31][k] & 3) == 0 ? 1 : 0;
    return std::max(n, 1);
}
} // namespace

void SynthVoice::start(const SynthPatch& patch, const SynthParams& macros, int id, float pitch, float velocity)
{
    const bool wasActive = active();
    patch_ = patch;
    id_ = id;
    pitch_ = targetPitch_ = pitch;
    velocity_ = dsp::clamp01(velocity);
    detune_ = (patch_.driftCents * (0.5f + alive_) + 5.0f * alive_) * drift_.next() / 100.0f; // semitones
    {
        constexpr float spread[5] = { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f };
        const bool reese = patch_.osc == Osc::Reese;
        for (int i = 0; i < 5; ++i)
            detuneRatio_[i] = std::exp2((reese ? (i == 0 ? -1.0f : 1.0f) : spread[i]) * patch_.detuneCents / 1200.0f);
    }
    amp_.set(attackTime(patch_, macros), patch_.d * (1.0f + 0.2f * alive_ * drift_.next()), patch_.s, releaseTime(patch_, macros), sampleRate_);
    filterEnv_.set(patch_.fa, patch_.fd, patch_.fs, patch_.fr, sampleRate_);
    amp_.gateOn();
    filterEnv_.gateOn();
    fmEnv_ = 1.0f;
    fmCoef_ = dsp::decayCoef(std::max(patch_.fmIndexTime, 0.01f), sampleRate_);
    tineEnv_ = patch_.tine > 0.0f ? 1.0f : 0.0f;
    hammerEnv_ = patch_.hammer > 0.0f ? 0.4f + 0.6f * velocity_ : 0.0f; // harder notes, harder hammer
    hammerCoef_ = dsp::decayCoef(0.008f, sampleRate_);
    tineCoef_ = dsp::decayCoef(0.25f, sampleRate_);
    pitchEnv_ = patch_.pitchDrop > 0.0f ? 1.0f : 0.0f;
    pitchEnvCoef_ = dsp::decayCoef(0.05f, sampleRate_);
    pdDetuneRatio_ = std::exp2(patch_.pdDetune / 1200.0f);
    gate_ = true;
    if (patch_.osc == Osc::Trio)
    {
        trioRatio_[0] = std::exp2((patch_.trioInt2 + patch_.trioDetune / 100.0f) / 12.0f);
        trioRatio_[1] = std::exp2((patch_.trioInt3 - patch_.trioDetune / 100.0f) / 12.0f);
        if (!wasActive)
        {
            phase_[0] = 0.0f;
            phase_[1] = 0.109375f; // a little apart: equal saws would cancel each other's harmonics
            phase_[2] = 0.21875f;
        }
    }
    if (patch_.osc == Osc::Lofi)
    {
        lofiSweepSemis_ = lofiArpTime_ = 0.0f;
        lofiHoldCount_ = 0.0f;
        // TONE: a one-pole low-pass from about 640 Hz (0) to open (1), set at 44.1 kHz in the original.
        const float a = std::min(0.9999f, (3000.0f + patch_.lofiTone * 127.0f * 256.0f) / 32768.0f);
        const float hz = -std::log(1.0f - a) * 44100.0f / dsp::kTwoPi;
        lofiLpCoef_ = 1.0f - std::exp(-dsp::kTwoPi * std::min(hz, sampleRate_ * 0.45f) / sampleRate_);
    }
    if (patch_.osc == Osc::Fm6)
    {
        // Harder notes open the modulators (their velocity amount), as on the original.
        for (int i = 0; i < 6; ++i)
        {
            const FmOperator& op = patch_.fm[i];
            fmTarget_[i] = op.level * (1.0f - op.velocity * (1.0f - velocity_));
            fmAttack_[i] = 1.0f / std::max(1.0f, op.a * sampleRate_);
            fmStage_[i] = 0;
            if (!wasActive)
            {
                fmPhase_[i] = 0.0f;
                opEnv_[i] = 0.0f;
            }
        }
        if (!wasActive)
            fmFb_[0] = fmFb_[1] = 0.0f;
    }
    if (!wasActive)
    {
        pdToggle_[0] = pdToggle_[1] = false;
        foldLast_ = foldOut_ = 0.0f;
    }
    excite_ = patch_.osc == Osc::String ? static_cast<int>(sampleRate_ / std::max(20.0f, dsp::midiToHz(pitch))) : 0;
    if (!wasActive)
    {
        std::fill(string_.begin(), string_.end(), 0.0f);
        stringPrev_ = exciteLp_ = 0.0f;
        vowelPhase_ = 0.5f + 0.5f * drift_.next();
        for (auto& f : formant_)
            f.reset();
        for (int i = 0; i < 5; ++i) // spread supersaw phases; free-running sounds start anywhere
            phase_[i] = patch_.freePhase ? 0.5f + 0.5f * drift_.next() : 0.2f * static_cast<float>(i);
        syncPhase_ = syncHeld_ = 0.0f;
        filter1_.reset();
        ladder_.reset();
        filterPrimed_ = false;
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

float SynthVoice::oscillator(float hz)
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
        default: break;
        case Osc::Saw:
            advance(phase_[0], dt);
            out = dsp::sawBlep(phase_[0], dt);
            break;
        case Osc::Square:
            advance(phase_[0], dt);
            out = dsp::squareBlep(phase_[0], dt, patch_.pulseWidth) * 0.8f;
            break;
        case Osc::Supersaw:
        {
            for (int i = 0; i < 5; ++i)
            {
                const float d = dt * detuneRatio_[i];
                advance(phase_[i], d);
                out += dsp::sawBlep(phase_[i], d);
            }
            out *= 0.45f;
            break;
        }
        case Osc::Reese:
            for (int i = 0; i < 2; ++i)
            {
                const float d = dt * detuneRatio_[i];
                advance(phase_[i], d);
                out += dsp::sawBlep(phase_[i], d);
            }
            out *= 0.6f;
            break;
        case Osc::Sine:
            advance(phase_[0], dt);
            out = std::sin(dsp::kTwoPi * phase_[0]);
            break;
        case Osc::Fm:
        {
            advance(phase_[0], dt);
            advance(modPhase_, dt * patch_.fmRatio);
            // Harder notes are brighter, like a real e-piano or mallet.
            const float index = (patch_.fmIndexEnd + (patch_.fmIndex - patch_.fmIndexEnd) * fmEnv_)
                              * (1.0f - patch_.velocityToFm * (1.0f - velocity_)) * fmKeyScale_;
            out = std::sin(dsp::kTwoPi * phase_[0] + index * std::sin(dsp::kTwoPi * modPhase_));
            break;
        }
        case Osc::Organ:
            advance(phase_[0], dt);
            for (int i = 0; i < 6; ++i)
                if (hz * kOrganHarmonics[i] < sr * 0.45f)
                    out += kOrganLevels[i] * std::sin(dsp::kTwoPi * phase_[0] * kOrganHarmonics[i]);
            out *= 0.4f;
            break;
        case Osc::Sync:
        {
            // Slave saw reset by the master. Each jump (the slave's own wrap or the
            // reset) gets a polyBLEP: half its correction lands on this sample, half on
            // the previous one, hence the one-sample delay.
            const float ratio = 1.0f + patch_.syncAmount * filterEnv_.value();
            const float dts = std::min(dt * ratio, 0.45f);
            float now = 0.0f, before = 0.0f; // corrections to this and the held sample
            auto jump = [&](float height, float f) { // f: samples since the jump, 0..1
                now -= 0.5f * height * (1.0f - f) * (1.0f - f);
                before += 0.5f * height * f * f;
            };
            phase_[0] += dt;
            if (phase_[0] >= 1.0f) // master wrapped: hard sync
            {
                phase_[0] -= 1.0f;
                const float f = phase_[0] / dt;
                float at = syncPhase_ + dts * (1.0f - f); // slave phase at the reset
                at -= std::floor(at);
                syncPhase_ = dts * f;
                jump(-2.0f * at, f); // from 2 * at - 1 down to -1
            }
            else
            {
                syncPhase_ += dts;
                if (syncPhase_ >= 1.0f)
                {
                    syncPhase_ -= 1.0f;
                    jump(-2.0f, syncPhase_ / dts);
                }
            }
            out = syncHeld_ + before;
            syncHeld_ = 2.0f * syncPhase_ - 1.0f + now;
            break;
        }
    }
    if (patch_.osc == Osc::Voice)
    {
        float source = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float d = dt * detuneRatio_[i + 1];
            advance(phase_[i], d);
            source += dsp::sawBlep(phase_[i], d);
        }
        source = source * 0.4f + patch_.noise * drift_.next();
        constexpr float gains[3] = { 1.0f, 0.6f, 0.3f };
        for (int i = 0; i < 3; ++i)
            out += formant_[i].tick(source).bp * gains[i] * 0.48f;
        out += source * 0.08f;
        return out;
    }
    if (patch_.osc == Osc::String)
    {
        const auto size = string_.size();
        const float delay = std::clamp(sr / hz - patch_.stringDamp, 2.0f, static_cast<float>(size) - 3.0f);
        const float pos = static_cast<float>(stringWrite_ + size) - delay;
        const auto i0 = static_cast<size_t>(pos);
        const float frac = pos - static_cast<float>(i0);
        const float read = string_[i0 % size] + (string_[(i0 + 1) % size] - string_[i0 % size]) * frac;
        float y = ((1.0f - patch_.stringDamp) * read + patch_.stringDamp * stringPrev_) * stringFb_;
        stringPrev_ = read;
        if (excite_ > 0) // the pluck: a period of noise, brighter when played harder
        {
            exciteLp_ += (0.15f + 0.8f * velocity_) * (drift_.next() - exciteLp_);
            y += exciteLp_ * 0.9f;
            --excite_;
        }
        string_[stringWrite_] = y;
        stringWrite_ = (stringWrite_ + 1) % size;
        if (hammerEnv_ > 1e-4f)
        {
            y += patch_.hammer * hammerEnv_ * drift_.next();
            hammerEnv_ *= hammerCoef_;
        }
        return y;
    }
    if (patch_.osc == Osc::Phase)
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
            const float folded = std::sin(0.5f * dsp::kPi * out * gain);
            foldOut_ += folded - foldLast_ - foldOut_ * 0.002f;
            foldLast_ = folded;
            out = foldOut_;
        }
    }
    if (patch_.osc == Osc::Trio)
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
        out = mix * 1.6f;
    }
    if (patch_.osc == Osc::Lofi)
    {
        // The pitch: the sweep falls after the note-on, the arpeggio steps at ~50 Hz, a ~5.4 Hz vibrato.
        static const int arps[4][3] = { { 0, 0, 0 }, { 0, 12, 0 }, { 0, 4, 7 }, { 0, 3, 7 } };
        lofiSweepSemis_ = std::min(96.0f, lofiSweepSemis_ + patch_.lofiSweep * 81.0f / sr);
        lofiArpTime_ += 1.0f / sr;
        const int step = static_cast<int>(lofiArpTime_ * 50.0f) % 3;
        lofiVibPhase_ += 5.4f / sr;
        lofiVibPhase_ -= std::floor(lofiVibPhase_);
        const float semis = static_cast<float>(arps[patch_.lofiArp & 3][step]) - lofiSweepSemis_
                            + patch_.lofiVib * 0.26f * std::sin(dsp::kTwoPi * lofiVibPhase_);
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
        out = lofiLp_ * 0.8f;
    }
    if (patch_.osc == Osc::Fm6)
    {
        // The operators in the algorithm's order (the sixth first), each modulating through two buses.
        const std::uint8_t* alg = kFmAlg[patch_.fmAlg & 31];
        float bus[3] = { 0.0f, 0.0f, 0.0f };
        bool has[3] = { false, false, false };
        for (int k = 0; k < 6; ++k)
        {
            const int i = 5 - k; // operator i + 1
            const FmOperator& op = patch_.fm[i];
            // The operator's envelope.
            float& env = opEnv_[i];
            if (!gate_)
                env *= fmRelease_[i];
            else if (fmStage_[i] == 0)
            {
                env += fmAttack_[i];
                if (env >= 1.0f)
                {
                    env = 1.0f;
                    fmStage_[i] = 1;
                }
            }
            else
                env = op.s + (env - op.s) * fmDecay_[i];
            const float gain = env * fmLevel_[i];
            const int flags = alg[k], inBus = (flags >> 4) & 3, outBus = flags & 3;
            const bool add = (flags & FmOadd) != 0 && has[outBus];
            float phase = fmPhase_[i];
            float y;
            if ((flags & 0xc0) == 0xc0 && fmFbAmount_ > 0.0f)
            {
                phase += (fmFb_[0] + fmFb_[1]) * 0.25f * fmFbAmount_;
                y = std::sin(dsp::kTwoPi * phase) * gain;
                fmFb_[0] = fmFb_[1];
                fmFb_[1] = y;
            }
            else
            {
                if (inBus != 0 && has[inBus])
                    phase += bus[inBus] * 2.0f; // a full-level modulator: about two cycles of phase
                y = std::sin(dsp::kTwoPi * phase) * gain;
            }
            bus[outBus] = add ? bus[outBus] + y : y;
            has[outBus] = true;
            const float freq = op.fixedHz > 0.0f ? op.fixedHz : hz * op.ratio * std::exp2(op.detuneCents / 1200.0f);
            fmPhase_[i] += std::min(freq / sr, 0.49f);
            fmPhase_[i] -= std::floor(fmPhase_[i]);
        }
        out = bus[0] / std::sqrt(static_cast<float>(fmCarrierCount(patch_.fmAlg)));
    }
    if (patch_.osc == Osc::Bowl)
    {
        constexpr float ratios[4] = { 1.0f, 2.76f, 5.2f, 8.4f }, levels[4] = { 1.0f, 0.55f, 0.3f, 0.15f };
        advance(phase_[4], 0.7f / sr);
        float e = 1.0f;
        for (int i = 0; i < 4; ++i)
        {
            if (hz * ratios[i] < sr * 0.45f)
            {
                advance(phase_[i], dt * ratios[i]);
                const float beat = 1.0f + 0.35f * std::sin(dsp::kTwoPi * phase_[4] * static_cast<float>(i + 1) + 1.3f * static_cast<float>(i));
                out += levels[i] * e * beat * std::sin(dsp::kTwoPi * phase_[i]);
            }
            e *= fmEnv_;
        }
        out *= 0.5f;
    }
    if (patch_.sub > 0.0f)
    {
        advance(subPhase_, dt * 0.5f);
        out += patch_.sub * std::sin(dsp::kTwoPi * subPhase_);
    }
    if (hammerEnv_ > 1e-4f)
    {
        out += patch_.hammer * hammerEnv_ * drift_.next();
        hammerEnv_ *= hammerCoef_;
    }
    if (patch_.noise > 0.0f)
        out += patch_.noise * drift_.next();
    if (tineEnv_ > 1e-4f && dt * 14.0f < 0.45f) // high notes: the tine would sit above Nyquist
    {
        advance(tinePhase_, dt * 14.0f);
        out += patch_.tine * tineEnv_ * std::sin(dsp::kTwoPi * tinePhase_);
        tineEnv_ *= tineCoef_;
    }
    return out;
}

void SynthVoice::updateFilter(const SynthParams& m, float hz)
{
    const float base = dsp::macroLog(dsp::clamp01(m.cutoff + m.cutoffMod), 40.0f, patch_.cutoffHz, 16000.0f);
    const float keyTrack = std::exp2(patch_.keyTrack * std::log2(hz / 261.63f));
    float envOct = patch_.envOctaves * m.envAmount * 2.0f;
    if (patch_.envOctaves < 0.5f)
        envOct += std::max(0.0f, m.envAmount - 0.5f) * 6.0f; // lets static patches get an envelope
    envOct *= 0.5f + 0.5f * velocity_;
    const float cutoff = base * keyTrack * std::exp2(envOct * filterEnv_.value()) * cutoffDrift_;

    const float r = dsp::clamp01(m.resonance);
    const float reso = r <= 0.5f ? patch_.resonance * r * 2.0f
                                 : patch_.resonance + (0.95f - patch_.resonance) * (r - 0.5f) * 2.0f;
    k_ = patch_.fourPole ? 4.0f * reso : 2.0f - 2.0f * reso;
    const float target = dsp::filterG(cutoff, sampleRate_);
    if (!filterPrimed_)
    {
        g_ = target;
        gStep_ = 0.0f;
        filterPrimed_ = true;
    }
    else
        gStep_ = (target - g_) / static_cast<float>(kControlRate);
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
        pitch += patch_.vibratoSemis * std::sin(dsp::kTwoPi * vibPhase_);
    }
    if (pitch != hzPitch_)
    {
        hzPitch_ = pitch;
        hz_ = dsp::midiToHz(pitch);
        fmKeyScale_ = hz_ > 700.0f ? std::sqrt(700.0f / hz_) : 1.0f; // gentler FM up high: less harsh, no aliasing
    }
    filterEnv_.tick();
    if (--controlCounter_ <= 0)
    {
        if (--wanderCounter_ <= 0) // a new direction two or three times a second
        {
            wanderTarget_[0] = drift_.next();
            wanderTarget_[1] = drift_.next();
            wanderCounter_ = static_cast<int>(sampleRate_ * (0.3f + 0.2f * drift_.next()) / static_cast<float>(kControlRate));
        }
        for (int w = 0; w < 2; ++w)
            wander_[w] += (wanderTarget_[w] - wander_[w]) * 0.004f;
        driftRatio_ = std::exp2(wander_[0] * alive_ * 0.09f / 12.0f); // up to +-9 cents
        cutoffDrift_ = std::exp2(wander_[1] * alive_ * 0.25f);
        if (patch_.osc == Osc::Voice) // the vowel breathes between its two shapes
        {
            vowelPhase_ += 0.09f * static_cast<float>(kControlRate) / sampleRate_;
            vowelPhase_ -= std::floor(vowelPhase_);
            const float v = dsp::clamp01(0.5f + 0.5f * std::sin(dsp::kTwoPi * vowelPhase_) + 0.3f * alive_ * wander_[1]);
            for (int i = 0; i < 3; ++i)
                formant_[i].set(patch_.formantA[i] + (patch_.formantB[i] - patch_.formantA[i]) * v, 0.92f, sampleRate_);
        }
        if (patch_.osc == Osc::Phase)
        {
            // The Cutoff knob and the filter envelope bend the phase; high notes bend less (less aliasing).
            const float knob = macros.cutoff + macros.cutoffMod - 0.5f;
            const float bend = patch_.pdDepth + knob + patch_.pdEnv * macros.envAmount * 2.0f * filterEnv_.value() * (0.5f + 0.5f * velocity_);
            pdBend_ = dsp::clamp01(bend) * 0.8545f * std::min(1.0f, 1800.0f / std::max(hz_, 1.0f));
        }
        if (patch_.osc == Osc::Fm6)
        {
            // Felucca's FM6 macros: Cutoff is the modulators' level (the brightness), Resonance adds
            // feedback, Env scales how long the modulators take to decay.
            fmBright_ = std::exp2((macros.cutoff + macros.cutoffMod - 0.5f) * 3.0f);
            fmFbAmount_ = dsp::clamp01(patch_.fmFeedback + (macros.resonance - 0.5f));
            const float time = dsp::macroLog(macros.envAmount, 0.3f, 1.0f, 3.0f);
            for (int i = 0; i < 6; ++i)
            {
                const FmOperator& op = patch_.fm[i];
                const float scale = (kFmAlg[patch_.fmAlg & 31][5 - i] & 3) == 0 ? 1.0f : time;
                fmDecay_[i] = dsp::decayCoef(op.d * scale, sampleRate_);
                fmRelease_[i] = dsp::decayCoef(op.r, sampleRate_);
                fmLevel_[i] = fmTarget_[i] * ((kFmAlg[patch_.fmAlg & 31][5 - i] & 3) == 0 ? 1.0f : fmBright_);
            }
        }
        if (patch_.osc == Osc::Trio) // the slightly unstable filter of the chips
            trioDrift_ += (0.5f * drift_.next() - trioDrift_) * 0.12f;
        if (patch_.osc == Osc::String)
            stringFb_ = std::pow(0.001f, 1.0f / (std::max(0.05f, patch_.stringDecay) * std::max(20.0f, hz_)));
    }
    const float hz = hz_ * driftRatio_;
    if (controlCounter_ <= 0)
    {
        updateFilter(macros, hz);
        controlCounter_ = kControlRate;
    }
    g_ += gStep_;
    fmEnv_ *= fmCoef_;

    const float x = oscillator(hz);
    float y = x;
    if (!patch_.noFilter)
    {
        if (patch_.fourPole)
            y = ladder_.tick(x, g_, k_);
        else
        {
            const auto f = filter1_.tick(patch_.osc == Osc::Trio ? dsp::fastTanh(x * 1.3f) / 1.3f : x, g_ * (1.0f + 0.05f * trioDrift_), k_);
            y = patch_.filterMode == 1 ? f.bp * 1.4f : patch_.filterMode == 2 ? f.hp : patch_.filterMode == 3 ? f.lp + f.hp : f.lp;
        }
    }
    y = dsp::drive(y, patch_.drive);
    return y * amp_.tick() * (0.35f + 0.65f * velocity_) * patch_.level;
}

// ---------------------------------------------------------------------------------

void Synth::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    std::uint32_t seed = 0x51ed27u;
    for (auto& v : voices_)
        v.prepare(sampleRate, seed += 0x9e3779b9u);
    smoothCoef_ = 1.0f - std::exp(-1.0f / (0.012f * sampleRate)); // ~12 ms
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
    const SynthParams previous = live_;
    params_ = params;
    live_ = params;
    if (started_)
    {
        live_.level = previous.level;
        live_.cutoff = previous.cutoff;
        live_.cutoffMod = previous.cutoffMod;
        live_.resonance = previous.resonance;
    }
    started_ = true;
    const float glideSeconds = 0.5f * params_.glide * params_.glide;
    glideCoef_ = glideSeconds < 0.001f ? 0.0f : std::exp(-1.0f / (glideSeconds * sampleRate_ / 3.0f));
    patch_ = &synthPatch(params_.sound);
    chorus_ = patch_->chorus;
}

void Synth::reset()
{
    for (auto& v : voices_)
        v.kill();
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
    if (patch_ == nullptr)
        patch_ = &synthPatch(params_.sound);
    if (!poly_ && patch_->mono && last_ != nullptr && last_->active())
        target = last_;
    for (auto& v : voices_)
        if (target == nullptr && !v.active())
            target = &v;
    if (target == nullptr) // all busy releasing: steal the quietest
    {
        target = &voices_[0];
        for (auto& v : voices_)
            if (v.level() < target->level())
                target = &v;
    }
    target->start(*patch_, live_, id, pitch, velocity);
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
    float out = 0.0f;
    for (auto& v : voices_)
        out += v.tick(live_, glideCoef_);
    return out * live_.level;
}

int Synth::activeVoices() const
{
    int n = 0;
    for (const auto& v : voices_)
        n += v.active() ? 1 : 0;
    return n;
}

} // namespace gristo::sound
