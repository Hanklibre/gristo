// Gristo's drum engines. The Analog and Synth kicks and snares and the 808 / 909 hats are
// ported from Mutable Instruments Plaits (Copyright 2016 Emilie Gillet, MIT licence,
// plaits/dsp/drums). The Acoustic kick, snare, tom and the cymbal follow Punk Labs'
// OneTrick URCHIN (Copyright (c) 2023 Punk Labs LLC, GPL-3.0-or-later). The Crack snare
// (a Karplus-Strong string, as in Erica Synths' Perkons), clap, cowbell, shaker and bell
// are Gristo's own.
#include "Drums.h"

namespace gristo::sound {

namespace {

using E = DrumEngine;
constexpr float kPans[kNumDrumVoices] = { 0.0f, 0.05f, 0.25f, 0.3f, -0.3f, -0.2f };

DrumSpec spec(E engine, float hz, float tone, float decay, float punch, float snap, float drive = 1.0f, float gain = 1.0f)
{
    DrumSpec s;
    s.engine = engine;
    s.hz = hz;
    s.tone = tone;
    s.decay = decay;
    s.punch = punch;
    s.snap = snap;
    s.drive = drive;
    s.gain = gain;
    return s;
}

KitPatch kit(std::initializer_list<DrumSpec> voices, float lowpass = 20000.0f, int crush = 1, float busDrive = 1.0f, float gain = 1.0f)
{
    KitPatch k;
    int v = 0;
    for (const DrumSpec& s : voices)
    {
        k.voices[v] = s;
        k.voices[v].pan = kPans[v];
        ++v;
    }
    k.busLowpassHz = lowpass;
    k.crush = crush;
    k.busDrive = busDrive;
    k.gain = gain;
    return k;
}

const KitPatch* buildKits()
{
    static KitPatch kits[static_cast<int>(Kit::Count)];
    //                       engine          Hz      tone   decay  punch  snap   drive  gain
    kits[0] = kit({ // Classic: a 909-style kit
        spec(E::SynthKick,   55.0f,  0.55f, 0.45f, 0.65f, 0.25f),
        spec(E::SynthSnare,  195.0f, 0.55f, 0.4f,  0.5f,  0.6f),
        spec(E::Hat909,      420.0f, 0.7f,  0.18f, 0.5f,  0.4f),
        spec(E::Hat909,      420.0f, 0.7f,  0.5f,  0.5f,  0.4f),
        spec(E::Clap,        1150.0f, 0.5f, 0.45f, 0.5f,  0.5f),
        spec(E::AnalogKick,  140.0f, 0.5f,  0.45f, 0.3f,  0.1f) }, 20000.0f, 1, 1.15f);
    kits[1] = kit({ // 808
        spec(E::AnalogKick,  48.0f,  0.35f, 0.72f, 0.45f, 0.2f),
        spec(E::AnalogSnare, 180.0f, 0.4f,  0.4f,  0.5f,  0.55f),
        spec(E::Hat808,      400.0f, 0.65f, 0.22f, 0.5f,  0.15f),
        spec(E::Hat808,      400.0f, 0.65f, 0.55f, 0.5f,  0.15f),
        spec(E::Clap,        1100.0f, 0.45f, 0.5f, 0.55f, 0.45f),
        spec(E::Cowbell,     540.0f, 0.5f,  0.4f,  0.5f,  0.5f) });
    kits[2] = kit({ // Trap: long 808, a tight snappy snare, crisp hats
        spec(E::AnalogKick,  45.0f,  0.4f,  0.85f, 0.6f,  0.3f,  1.6f),
        spec(E::SynthSnare,  215.0f, 0.65f, 0.35f, 0.6f,  0.8f),
        spec(E::Hat808,      430.0f, 0.8f,  0.14f, 0.5f,  0.25f),
        spec(E::Hat808,      430.0f, 0.75f, 0.45f, 0.5f,  0.25f),
        spec(E::Clap,        1300.0f, 0.6f, 0.4f,  0.4f,  0.6f),
        spec(E::AnalogSnare, 460.0f, 0.15f, 0.1f,  0.5f,  0.05f) }, 20000.0f, 1, 1.25f);
    kits[3] = kit({ // Lo-fi: dusty acoustic hits through a crunchy bus
        spec(E::AcousticKick, 60.0f, 0.3f,  0.35f, 0.4f,  0.5f),
        spec(E::AcousticSnare, 190.0f, 0.35f, 0.3f, 0.45f, 0.55f),
        spec(E::Hat909,      380.0f, 0.4f,  0.16f, 0.4f,  0.7f),
        spec(E::Shaker,      6000.0f, 0.4f, 0.35f, 0.4f,  0.5f),
        spec(E::AnalogSnare, 520.0f, 0.1f,  0.12f, 0.5f,  0.1f),
        spec(E::Tom,         120.0f, 0.35f, 0.45f, 0.45f, 0.3f) }, 7500.0f, 2, 1.6f);
    kits[4] = kit({ // Minimal: short, dry, clicky
        spec(E::SynthKick,   52.0f,  0.45f, 0.3f,  0.55f, 0.15f),
        spec(E::AnalogSnare, 500.0f, 0.2f,  0.12f, 0.5f,  0.1f),
        spec(E::Hat909,      460.0f, 0.75f, 0.08f, 0.5f,  0.6f),
        spec(E::Hat808,      410.0f, 0.6f,  0.4f,  0.5f,  0.3f),
        spec(E::Bell,        900.0f, 0.4f,  0.15f, 0.6f,  0.3f),
        spec(E::Cowbell,     800.0f, 0.6f,  0.2f,  0.5f,  0.5f) });
    kits[5] = kit({ // Reggaeton
        spec(E::AnalogKick,  55.0f,  0.45f, 0.5f,  0.6f,  0.25f, 1.3f),
        spec(E::SynthSnare,  235.0f, 0.6f,  0.3f,  0.55f, 0.7f),
        spec(E::Hat808,      420.0f, 0.7f,  0.18f, 0.5f,  0.3f),
        spec(E::Shaker,      7000.0f, 0.55f, 0.3f, 0.5f,  0.5f),
        spec(E::AnalogSnare, 520.0f, 0.15f, 0.1f,  0.5f,  0.05f),
        spec(E::Clap,        1200.0f, 0.55f, 0.45f, 0.5f, 0.5f) });
    kits[6] = kit({ // House 909
        spec(E::SynthKick,   50.0f,  0.55f, 0.55f, 0.6f,  0.2f),
        spec(E::Clap,        1150.0f, 0.5f, 0.5f,  0.55f, 0.5f),
        spec(E::Hat909,      420.0f, 0.7f,  0.16f, 0.5f,  0.35f),
        spec(E::Hat909,      420.0f, 0.7f,  0.5f,  0.5f,  0.35f),
        spec(E::Cymbal,      0.0f,   0.6f,  0.7f,  0.5f,  0.3f),
        spec(E::Shaker,      7500.0f, 0.6f, 0.25f, 0.5f,  0.5f) });
    kits[7] = kit({ // Techno: driven, dirty
        spec(E::SynthKick,   48.0f,  0.5f,  0.5f,  0.7f,  0.55f, 2.0f),
        spec(E::AnalogSnare, 170.0f, 0.55f, 0.35f, 0.5f,  0.7f),
        spec(E::Hat909,      440.0f, 0.75f, 0.14f, 0.5f,  0.25f),
        spec(E::Hat909,      440.0f, 0.7f,  0.45f, 0.5f,  0.25f),
        spec(E::Clap,        1250.0f, 0.55f, 0.55f, 0.6f, 0.55f),
        spec(E::AnalogKick,  110.0f, 0.55f, 0.4f,  0.5f,  0.4f) }, 20000.0f, 1, 1.4f);
    kits[8] = kit({ // Breakbeat: a live kit, a little squashed
        spec(E::AcousticKick, 62.0f, 0.45f, 0.4f,  0.55f, 0.5f),
        spec(E::AcousticSnare, 200.0f, 0.55f, 0.35f, 0.55f, 0.6f),
        spec(E::Cymbal,      0.0f,   0.55f, 0.08f, 0.5f,  0.3f),
        spec(E::Cymbal,      0.0f,   0.5f,  0.55f, 0.5f,  0.35f),
        spec(E::Tom,         130.0f, 0.5f,  0.5f,  0.5f,  0.3f),
        spec(E::CrackSnare,  260.0f, 0.5f,  0.25f, 0.4f,  0.4f) }, 13000.0f, 1, 1.45f);
    kits[9] = kit({ // Jungle: a tuned-up snare, rides, rattling ghosts
        spec(E::AcousticKick, 58.0f, 0.45f, 0.45f, 0.55f, 0.5f),
        spec(E::AcousticSnare, 235.0f, 0.65f, 0.3f, 0.6f, 0.7f),
        spec(E::Cymbal,      0.0f,   0.6f,  0.07f, 0.5f,  0.25f),
        spec(E::Cymbal,      0.0f,   0.55f, 0.75f, 0.5f,  0.5f),
        spec(E::Tom,         175.0f, 0.55f, 0.4f,  0.55f, 0.3f),
        spec(E::CrackSnare,  310.0f, 0.6f,  0.2f,  0.5f,  0.5f) }, 15000.0f, 1, 1.35f);
    kits[10] = kit({ // Amapiano: a short kick, shakers, and the log drum
        spec(E::AnalogKick,  52.0f,  0.4f,  0.45f, 0.55f, 0.2f),
        spec(E::Clap,        1150.0f, 0.55f, 0.4f, 0.5f,  0.5f),
        spec(E::Shaker,      6500.0f, 0.5f, 0.3f,  0.6f,  0.5f),
        spec(E::Hat808,      420.0f, 0.7f,  0.45f, 0.5f,  0.3f),
        spec(E::AnalogKick,  70.0f,  0.6f,  0.72f, 0.95f, 0.65f, 1.4f),
        spec(E::Cowbell,     700.0f, 0.55f, 0.3f,  0.5f,  0.5f) });
    kits[11] = kit({ // Dancehall
        spec(E::SynthKick,   60.0f,  0.55f, 0.35f, 0.7f,  0.3f, 1.3f),
        spec(E::SynthSnare,  270.0f, 0.65f, 0.25f, 0.5f,  0.65f),
        spec(E::Hat909,      440.0f, 0.75f, 0.12f, 0.5f,  0.4f),
        spec(E::Shaker,      7000.0f, 0.6f, 0.25f, 0.5f,  0.5f),
        spec(E::AnalogSnare, 1600.0f, 0.1f, 0.08f, 0.5f,  0.05f),
        spec(E::Clap,        1250.0f, 0.6f, 0.45f, 0.5f,  0.55f) }, 20000.0f, 1, 1.3f);
    kits[12] = kit({ // Temple: frame drum, wood fish, seed rattle, hand bell, singing bowl, gong
        spec(E::Tom,         90.0f,  0.35f, 0.75f, 0.35f, 0.2f,  1.0f,  0.45f),
        spec(E::AnalogSnare, 620.0f, 0.1f,  0.15f, 0.5f,  0.0f),
        spec(E::Shaker,      5500.0f, 0.45f, 0.3f, 0.7f,  0.4f),
        spec(E::Bell,        2400.0f, 0.5f, 0.4f,  0.5f,  0.4f),
        spec(E::Bell,        520.0f, 0.4f,  0.85f, 0.3f,  0.6f),
        spec(E::Bell,        180.0f, 0.45f, 0.9f,  0.4f,  0.85f) });
    kits[13] = kit({ // Dusty Soul: a soft, dead live kit
        spec(E::AcousticKick, 66.0f, 0.3f,  0.3f,  0.4f,  0.4f),
        spec(E::AcousticSnare, 185.0f, 0.4f, 0.3f, 0.45f, 0.5f),
        spec(E::Hat909,      380.0f, 0.45f, 0.15f, 0.4f,  0.7f),
        spec(E::Shaker,      7000.0f, 0.5f, 0.35f, 0.3f,  0.5f),
        spec(E::AnalogSnare, 800.0f, 0.1f,  0.1f,  0.5f,  0.05f),
        spec(E::Tom,         110.0f, 0.35f, 0.45f, 0.4f,  0.3f) }, 9000.0f, 1, 1.5f);
    kits[14] = kit({ // Acoustic: the modelled kit
        spec(E::AcousticKick, 58.0f, 0.45f, 0.5f,  0.55f, 0.5f),
        spec(E::AcousticSnare, 185.0f, 0.5f, 0.4f, 0.5f,  0.6f),
        spec(E::Cymbal,      0.0f,   0.55f, 0.06f, 0.5f,  0.25f),
        spec(E::Cymbal,      0.0f,   0.5f,  0.6f,  0.5f,  0.3f),
        spec(E::Tom,         135.0f, 0.5f,  0.5f,  0.5f,  0.3f),
        spec(E::Tom,         92.0f,  0.5f,  0.6f,  0.5f,  0.3f) }, 20000.0f, 1, 1.15f);
    return kits;
}

// Output levels, so every engine sits about as loud at its defaults.
float engineGain(E e)
{
    switch (e)
    {
        case E::AnalogKick: return 1.6f;
        case E::SynthKick: return 0.8f;
        case E::AcousticKick: return 0.42f;
        case E::AnalogSnare: return 0.9f;
        case E::SynthSnare: return 1.1f;
        case E::AcousticSnare: return 0.5f;
        case E::CrackSnare: return 0.8f;
        case E::Clap: return 1.0f;
        case E::Hat808: return 1.3f;
        case E::Hat909: return 0.9f;
        case E::Cymbal: return 3.2f;
        case E::Tom: return 0.45f;
        case E::Cowbell: return 0.5f;
        case E::Shaker: return 1.4f;
        case E::Bell: return 0.5f;
        default: return 1.0f;
    }
}

float semis(float s) { return std::exp2(s / 12.0f); }
float diode(float x) { return x >= 0.0f ? x : 0.7f * (2.0f * x) / (1.0f + std::abs(2.0f * x)); }

} // namespace

const char* drumEngineName(DrumEngine engine)
{
    switch (engine)
    {
        case E::AnalogKick: return "Analog Kick";
        case E::SynthKick: return "Synth Kick";
        case E::AcousticKick: return "Acoustic Kick";
        case E::AnalogSnare: return "Analog Snare";
        case E::SynthSnare: return "Synth Snare";
        case E::AcousticSnare: return "Acoustic Snare";
        case E::CrackSnare: return "Crack Snare";
        case E::Clap: return "Clap";
        case E::Hat808: return "808 Hat";
        case E::Hat909: return "909 Hat";
        case E::Cymbal: return "Cymbal";
        case E::Tom: return "Tom";
        case E::Cowbell: return "Cowbell";
        case E::Shaker: return "Shaker";
        case E::Bell: return "Bell";
        default: return "";
    }
}

const char* kitName(Kit kit)
{
    switch (kit)
    {
        case Kit::Classic: return "Classic";
        case Kit::Eight08: return "808";
        case Kit::Trap: return "Trap";
        case Kit::LoFi: return "Lo-fi";
        case Kit::Minimal: return "Minimal";
        case Kit::Reggaeton: return "Reggaeton";
        case Kit::House: return "House 909";
        case Kit::Techno: return "Techno";
        case Kit::Breakbeat: return "Breakbeat";
        case Kit::Jungle: return "Jungle";
        case Kit::Amapiano: return "Amapiano";
        case Kit::Dancehall: return "Dancehall";
        case Kit::Temple: return "Temple";
        case Kit::DustySoul: return "Dusty Soul";
        case Kit::Acoustic: return "Acoustic";
        case Kit::Count: break;
    }
    return "";
}

const KitPatch& kitPatch(Kit kit)
{
    static const KitPatch* kits = buildKits();
    return kits[std::clamp(static_cast<int>(kit), 0, static_cast<int>(Kit::Count) - 1)];
}

DrumSpec engineDefault(DrumEngine engine, int voice)
{
    const bool open = voice == 3;
    switch (engine)
    {
        case E::AnalogKick: return spec(engine, voice == 0 ? 50.0f : 140.0f, 0.4f, voice == 0 ? 0.65f : 0.45f, 0.5f, 0.2f);
        case E::SynthKick: return spec(engine, 55.0f, 0.55f, 0.45f, 0.6f, 0.25f);
        case E::AcousticKick: return spec(engine, 58.0f, 0.45f, 0.5f, 0.55f, 0.5f);
        case E::AnalogSnare: return spec(engine, voice >= 4 ? 500.0f : 180.0f, voice >= 4 ? 0.15f : 0.4f, voice >= 4 ? 0.12f : 0.4f, 0.5f, voice >= 4 ? 0.05f : 0.55f);
        case E::SynthSnare: return spec(engine, 200.0f, 0.55f, 0.4f, 0.5f, 0.6f);
        case E::AcousticSnare: return spec(engine, 190.0f, 0.5f, 0.4f, 0.5f, 0.6f);
        case E::CrackSnare: return spec(engine, 260.0f, 0.5f, 0.3f, 0.5f, 0.5f);
        case E::Clap: return spec(engine, 1150.0f, 0.5f, 0.45f, 0.5f, 0.5f);
        case E::Hat808: return spec(engine, 400.0f, 0.65f, open ? 0.55f : 0.22f, 0.5f, 0.2f);
        case E::Hat909: return spec(engine, 420.0f, 0.7f, open ? 0.5f : 0.18f, 0.5f, 0.4f);
        case E::Cymbal: return spec(engine, 0.0f, 0.55f, open ? 0.6f : 0.07f, 0.5f, 0.3f);
        case E::Tom: return spec(engine, voice == 0 ? 75.0f : 130.0f, 0.5f, 0.5f, 0.5f, 0.3f);
        case E::Cowbell: return spec(engine, 540.0f, 0.5f, 0.4f, 0.5f, 0.5f);
        case E::Shaker: return spec(engine, 6500.0f, 0.5f, 0.3f, 0.5f, 0.5f);
        case E::Bell: return spec(engine, 700.0f, 0.5f, 0.6f, 0.5f, 0.4f);
        default: return spec(E::AnalogKick, 50.0f, 0.4f, 0.6f, 0.5f, 0.2f);
    }
}

void DrumVoice::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    comb_.assign(4 * kCombLength, 0.0f);
    string_.assign(static_cast<size_t>(sampleRate / 40.0f) + 8, 0.0f);
    dc_.setLowpass(15.0f, sampleRate);
}

bool DrumVoice::tonal() const
{
    switch (spec_.engine)
    {
        case E::Hat808:
        case E::Hat909:
        case E::Cymbal:
        case E::Shaker:
        case E::Clap: return false;
        default: return true;
    }
}

void DrumVoice::trigger(const DrumSpec& s, float velocity, float alive, int filter, float cutoff)
{
    if (comb_.empty())
        prepare(sampleRate_);
    const float sr = sampleRate_;
    const bool wasCymbal = active_ && spec_.engine == E::Cymbal;
    spec_ = s;
    // No two hits quite alike: pitch, length, level all move a little.
    alive = dsp::clamp01(alive);
    spec_.hz *= semis(noise_.next() * (0.04f + 0.17f * alive));
    spec_.decay = dsp::clamp01(spec_.decay + 0.04f * alive * noise_.next());
    velocity = dsp::clamp01(velocity * (1.0f + 0.1f * alive * noise_.next()));
    for (float* v : { &spec_.tone, &spec_.decay, &spec_.punch, &spec_.snap })
        *v = dsp::clamp01(*v);
    accent_ = velocity;
    const float acc = velocity, f0 = std::clamp(spec_.hz, 10.0f, sr * 0.2f) / sr;
    const float tone = spec_.tone, decay = spec_.decay, punch = spec_.punch, snap = spec_.snap;

    declick_ = active_ ? lastOut_ : 0.0f;
    declickCoef_ = dsp::decayCoef(0.003f, sr);
    chokeCoef_ = dsp::decayCoef(0.03f, sr);
    choke_ = false;
    chokeGain_ = 1.0f;
    active_ = true;
    silent_ = 0;
    t_ = 0.0f;

    // Soft hits are darker, not only quieter (on every engine).
    const float toneHz = 1800.0f * std::exp2(acc * 3.6f);
    velocityToneOn_ = toneHz < 16000.0f;
    velocityTone_.reset();
    velocityTone_.setLowpass(toneHz, sr);
    filterMode_ = std::clamp(filter, 0, 3);
    if (filterMode_ > 0)
    {
        filter_.reset();
        filter_.set(40.0f * std::pow(450.0f, dsp::clamp01(cutoff)) / sr, 0.9f);
    }

    switch (spec_.engine)
    {
        case E::AnalogKick: // Plaits AnalogBassDrum
            q_ = 1500.0f * semis(decay * 80.0f);
            toneF_ = std::min(4.0f * f0 * semis(tone * 108.0f), 1.0f);
            leak_ = 0.08f * (tone + 0.25f);
            scale_ = 0.001f / f0;
            pulseRemaining_ = static_cast<int>(1.0e-3f * sr);
            fmPulseRemaining_ = static_cast<int>(6.0e-3f * sr);
            pulseHeight_ = 3.0f + 7.0f * acc;
            lpOut_ = 0.0f;
            res_[0].reset();
            break;
        case E::SynthKick: // Plaits SyntheticBassDrum
        {
            const float d = decay * decay, fmd = 0.3f * 0.3f;
            coef_[0] = 1.0f - 1.0f / (0.008f * (1.0f + fmd * 4.0f) * sr);         // fm
            coef_[1] = 1.0f - 1.0f / (0.02f * sr) * semis(-d * 60.0f);              // body
            coef_[2] = 1.0f - 1.0f / (0.005f * sr);                                // transient
            toneF_ = std::min(4.0f * f0 * semis(tone * 108.0f), 1.0f);
            env_[0] = 1.0f;                                                       // fm
            env_[1] = env_[2] = 0.3f + 0.7f * acc;                                // body, transient
            hold_ = static_cast<int>(0.001f * sr);                                // body pulse
            fmPulseRemaining_ = static_cast<int>(0.0013f * sr);
            res_[0].set(5000.0f / sr, 2.0f);                                      // the click's filter
            break;
        }
        case E::AnalogSnare: // Plaits AnalogSnareDrum
        {
            static const float modes[5] = { 1.0f, 2.0f, 3.18f, 4.16f, 5.62f };
            const float xt = decay * (1.0f + decay * (decay - 1.0f));
            const float q = 2000.0f * semis(xt * 84.0f);
            coef_[0] = d48(1.0f - 0.0017f * semis(-decay * (50.0f + snap * 10.0f)));
            leak_ = snap * (2.0f - snap) * 0.1f;
            for (int i = 0; i < 5; ++i)
            {
                freqs_[i] = std::min(f0 * modes[i], 0.499f);
                res_[i].reset();
                res_[i].set(freqs_[i], 1.0f + freqs_[i] * (i == 0 ? q : q * 0.25f));
            }
            float t = tone;
            if (t < 0.666667f)
            {
                t *= 1.5f;
                gains_[0] = 1.5f + (1.0f - t) * (1.0f - t) * 4.5f;
                gains_[1] = 2.0f * t + 0.15f;
                gains_[2] = gains_[3] = gains_[4] = 0.0f;
            }
            else
            {
                t = (t - 0.666667f) * 3.0f;
                gains_[0] = 1.5f - t * 0.5f;
                gains_[1] = 2.15f - t * 0.7f;
                for (int i = 2; i < 5; ++i)
                {
                    gains_[i] = t;
                    t *= t;
                }
            }
            const float fn = std::min(f0 * 16.0f, 0.499f);
            noiseFilter_.reset();
            noiseFilter_.set(fn, 1.0f + fn * 1.5f);
            pulseRemaining_ = static_cast<int>(1.0e-3f * sr);
            pulseHeight_ = 3.0f + 7.0f * acc;
            env_[0] = 2.0f; // noise
            break;
        }
        case E::SynthSnare: // Plaits SyntheticSnareDrum
        {
            const float xt = decay * (1.0f + decay * (decay - 1.0f)), fm = punch * punch;
            coef_[0] = 1.0f - 1.0f / (0.015f * sr) * semis(-xt * 72.0f - fm * 12.0f + snap * 7.0f); // drum
            coef_[1] = 1.0f - 1.0f / (0.01f * sr) * semis(-decay * 60.0f - snap * 7.0f);           // snare
            coef_[2] = 1.0f - 1.0f / (0.007f * sr);                                                // fm
            env_[0] = env_[1] = 0.3f + 0.7f * acc;
            env_[2] = 1.0f;
            phase_[0] = phase_[1] = 0.0f;
            hold_ = static_cast<int>((0.04f + decay * 0.03f) * sr);
            const float bright = semis((tone - 0.5f) * 24.0f);
            onePole_[0].setLowpass(3.0f * spec_.hz, sr);                       // drum lp
            onePole_[1].setLowpass(std::min(10.0f * spec_.hz * bright, sr * 0.45f), sr); // snare hp
            noiseFilter_.reset();
            noiseFilter_.set(std::min(35.0f * f0 * bright, 0.49f), 0.5f + 2.0f * snap);
            break;
        }
        case E::Hat808:
        case E::Hat909: // Plaits HiHat: metallic noise, a band-pass, a VCA, a high-pass
        {
            static const float ratios[6] = { 1.0f, 1.304f, 1.466f, 1.787f, 1.932f, 2.536f };
            for (int i = 0; i < 6; ++i)
                freqs_[i] = std::min(2.0f * f0 * ratios[i], 0.499f);
            coef_[0] = d48(1.0f - 0.003f * semis(-decay * 84.0f));
            coef_[1] = d48(1.0f - 0.0025f * semis(-decay * 36.0f));
            env_[0] = (1.5f + 0.5f * (1.0f - decay)) * (0.3f + 0.7f * acc);
            const float c = std::min(150.0f * semis(tone * 72.0f), 16000.0f) / sr;
            const bool is808 = spec_.engine == E::Hat808;
            colour_.set(c, is808 ? 3.0f + 3.0f * tone : 1.0f);
            hp_.set(c, 0.5f);
            if (!is808) // ring-modulated pairs (909-ish)
            {
                const float ratio = f0 / (0.01f + f0);
                const float hz[6] = { 200.0f, 7530.0f, 510.0f, 8075.0f, 730.0f, 10500.0f };
                for (int i = 0; i < 6; ++i)
                    freqs_[i] = std::min(hz[i] / sr * ratio * (spec_.hz / 420.0f), 0.45f);
            }
            break;
        }
        case E::AcousticKick:
        case E::AcousticSnare:
        case E::Tom: // after OneTrick URCHIN: a batter head, a resonant head, a shell, wires on the snare
        {
            const bool kick = spec_.engine == E::AcousticKick, snare = spec_.engine == E::AcousticSnare;
            const float headDecay = dsp::macroLog(decay, kick ? 0.12f : 0.08f, kick ? 0.55f : snare ? 0.3f : 0.5f, 2.0f);
            const float tension = punch * (kick ? 12.0f : snare ? 5.0f : 9.0f); // semitones of pitch sag
            env_[3] = tension;
            for (int i = 0; i < 5; ++i)
            {
                const float ratio = ((spec_.hz - 20.0f) * static_cast<float>(i + 1) + 20.0f) / std::max(spec_.hz, 30.0f);
                headGain_[i] = ((i % 2) ? 1.0f : -1.0f) * std::pow(1.0f / ratio, 1.6f - 0.8f * tone);
                const float excite = i == 0 ? 1.0f : 0.35f + 0.65f * acc;
                headEnv_[0][i] = excite;
                headEnv_[1][i] = excite * (0.2f + 0.8f * acc);
                headCoef_[0][i] = dsp::decayCoef(headDecay / ratio, sr);
                headCoef_[1][i] = dsp::decayCoef(headDecay * 1.6f / ratio, sr);
            }
            gains_[0] = kick ? 1.6f : snare ? 1.1f : 1.3f;            // batter head
            gains_[1] = kick ? 0.7f : snare ? 0.6f : 0.7f;            // resonant head
            gains_[2] = snare ? 0.6f + 1.4f * snap : 0.0f;            // wires
            freqs_[0] = kick ? -2.0f : snare ? 3.0f : -1.0f;          // the resonant head, semitones
            energy_ = 0.3f + 0.7f * acc;
            energyCoef_ = dsp::decayCoef(headDecay * 0.5f, sr);
            wireAttack_ = 1.0f - std::exp(-1.0f / (0.002f * sr));
            wireRelease_ = 1.0f - std::exp(-1.0f / (0.03f * sr));
            colour_.set(std::min(2400.0f * spec_.hz / 185.0f, sr * 0.4f) / sr, 1.4f);
            onePole_[0].setLowpass(std::min(7500.0f * spec_.hz / 185.0f, sr * 0.45f), sr);
            shellPhase_ = 0.0f;
            shellEnv_ = acc * acc * acc * (kick ? 0.03f : snare ? 0.05f : 0.04f) * (0.5f + tone);
            shellCoef_ = dsp::decayCoef(headDecay * 0.5f, sr);
            env_[0] = (kick ? 0.25f : 0.3f) * punch * 2.0f * (0.5f + 0.8f * acc); // the stick / beater click
            coef_[0] = dsp::decayCoef(0.004f, sr);
            onePole_[1].setLowpass(3500.0f, sr);
            break;
        }
        case E::Cymbal: // after OneTrick URCHIN
        {
            if (!wasCymbal) // a new hit adds to what still rings; anything else starts clean
            {
                env_[0] = strike_ = 0.0f;
                std::fill(comb_.begin(), comb_.end(), 0.0f);
            }
            strike_ = std::max(strike_, acc);
            const float size = dsp::clamp01(0.3f + (0.5f - tone));
            strikeCoef_ = dsp::decayCoef(0.2f + 0.04f * size, sr);
            env_[0] = std::min(1.0f, std::max(env_[0] + 0.5f * acc, acc));
            const float seconds = dsp::macroLog(decay, 0.03f, 0.5f, 4.0f);
            coef_[0] = dsp::decayCoef(seconds, sr);
            static const float hz[4] = { 120.54f, 163.38f, 189.64f, 85.0f };
            for (int i = 0; i < 4; ++i)
            {
                combDelay_[i] = std::min(sr / hz[i], static_cast<float>(kCombLength - 2));
                combFb_[i] = 0.9999f * std::pow(0.001f, 1.0f / (seconds * hz[i]));
            }
            env_[3] = size;
            onePole_[0].setLowpass(400.0f, sr);
            control_ = 0;
            break;
        }
        case E::CrackSnare: // a noise burst into a short, damped string (Karplus-Strong), and a noise crack
        {
            stringDelay_ = std::clamp(sr / spec_.hz, 2.0f, static_cast<float>(string_.size()) - 3.0f);
            stringFb_ = std::pow(0.001f, 1.0f / (dsp::macroLog(decay, 0.04f, 0.25f, 1.2f) * spec_.hz));
            stringDamp_ = 0.65f - 0.6f * tone;
            hold_ = static_cast<int>((0.002f + 0.004f * punch) * sr); // the burst
            env_[0] = 0.3f + 0.7f * acc;                             // the crack
            coef_[0] = dsp::decayCoef(0.05f + 0.25f * snap * decay + 0.03f, sr);
            hp_.reset();
            hp_.set(std::min(2000.0f + 3000.0f * tone, sr * 0.4f) / sr, 0.7f);
            break;
        }
        case E::Clap: // several quick bursts of band-passed noise, then a tail
        {
            bursts_ = 2 + static_cast<int>(punch * 3.99f);
            nextBurst_ = 0.0f;
            env_[0] = 0.0f;
            coef_[0] = dsp::decayCoef(0.012f, sr);
            coef_[1] = dsp::decayCoef(dsp::macroLog(decay, 0.08f, 0.3f, 1.2f), sr);
            env_[1] = 0.3f + 0.7f * acc;
            noiseFilter_.reset();
            noiseFilter_.set(std::min(spec_.hz * semis((tone - 0.5f) * 24.0f), sr * 0.4f) / sr, 1.2f + 2.0f * (1.0f - snap));
            break;
        }
        case E::Cowbell: // two square waves a fifth-ish apart, a band-pass, a fast then a slow decay
            freqs_[0] = f0;
            freqs_[1] = f0 * 1.4815f;
            colour_.reset();
            colour_.set(std::min(f0 * 4.0f * semis((tone - 0.5f) * 12.0f), 0.45f), 2.0f);
            env_[0] = 0.3f + 0.7f * acc;
            coef_[0] = dsp::decayCoef(0.03f + 0.04f * punch, sr);
            coef_[1] = dsp::decayCoef(dsp::macroLog(decay, 0.05f, 0.35f, 1.5f), sr);
            break;
        case E::Shaker: // beads in a shell: a soft attack, band-passed noise
            env_[0] = 0.0f;
            env_[1] = 0.3f + 0.7f * acc;
            coef_[0] = 1.0f / std::max(1.0f, (0.002f + 0.03f * punch) * sr);
            coef_[1] = dsp::decayCoef(dsp::macroLog(decay, 0.03f, 0.15f, 0.8f), sr);
            colour_.reset();
            colour_.set(std::min(spec_.hz * semis((tone - 0.5f) * 12.0f), sr * 0.45f) / sr, 1.0f + 2.0f * snap);
            break;
        case E::Bell: // struck metal: four inharmonic partials, the high ones dying first
        {
            static const float ratios[4] = { 1.0f, 2.76f, 5.4f, 8.93f };
            const float seconds = dsp::macroLog(decay, 0.08f, 1.0f, 4.5f);
            for (int i = 0; i < 4; ++i)
            {
                freqs_[i] = std::min(f0 * ratios[i] * (i ? 1.0f + 0.01f * snap * static_cast<float>(i) : 1.0f), 0.45f);
                gains_[i] = i == 0 ? 1.0f : std::pow(0.3f + 0.6f * tone, static_cast<float>(i)) * (0.6f + 0.4f * acc);
                headEnv_[0][i] = 1.0f;
                headCoef_[0][i] = dsp::decayCoef(seconds / (1.0f + static_cast<float>(i) * 1.5f), sr);
            }
            env_[0] = punch * acc * 0.5f; // the striker
            coef_[0] = dsp::decayCoef(0.003f, sr);
            phase_[4] = 0.0f;
            break;
        }
        default: break;
    }
}

float DrumVoice::render()
{
    const float sr = sampleRate_;
    const float f0 = std::clamp(spec_.hz, 10.0f, sr * 0.2f) / sr;
    const float acc = accent_;
    switch (spec_.engine)
    {
        case E::AnalogKick:
        {
            float pulse = 0.0f;
            if (pulseRemaining_ > 0)
            {
                --pulseRemaining_;
                pulse = pulseRemaining_ ? pulseHeight_ : pulseHeight_ - 1.0f;
                pulse_ = pulse;
            }
            else
            {
                pulse_ *= 1.0f - 1.0f / (0.2e-3f * sr);
                pulse = pulse_;
            }
            pulseLp_ += (pulse - pulseLp_) * (1.0f / (0.1e-3f * sr));
            pulse = diode((pulse - pulseLp_) + pulse * 0.044f);
            float fmPulse = 0.0f;
            if (fmPulseRemaining_ > 0)
            {
                --fmPulseRemaining_;
                fmPulse = 1.0f;
                retrig_ = fmPulseRemaining_ ? 0.0f : -0.8f;
            }
            else
                retrig_ *= 1.0f - 1.0f / (0.05f * sr);
            fmPulseLp_ += (fmPulse - fmPulseLp_) * (1.0f / (0.1e-3f * sr));
            const float punch = 0.7f + diode(10.0f * lpOut_ - 1.0f);
            const float f = std::clamp(f0 * (1.0f + fmPulseLp_ * 1.7f * spec_.punch + punch * 0.08f * spec_.snap), 0.0f, 0.4f);
            res_[0].setDirty(f, 1.0f + q_ * f);
            float lp, bp, hp;
            res_[0].tick((pulse - retrig_ * 0.2f) * scale_, lp, bp, hp);
            lpOut_ = lp;
            toneLp_ += (pulse * leak_ + bp - toneLp_) * toneF_;
            return toneLp_;
        }
        case E::SynthKick:
        {
            phaseNoise_ += (random() - 0.5f - phaseNoise_) * k48(0.002f);
            const float dirt = spec_.snap * std::max(1.0f - 8.0f * f0, 0.0f);
            if (fmPulseRemaining_ > 0)
            {
                --fmPulseRemaining_;
                phase_[0] = 0.25f;
            }
            else
            {
                env_[0] *= coef_[0];
                const float fm = 1.0f + spec_.punch * 3.5f * envLp_[2];
                phase_[0] += std::min(f0 * fm, 0.5f);
                phase_[0] -= std::floor(phase_[0]);
            }
            if (hold_ > 0)
                --hold_;
            else
            {
                env_[1] *= coef_[1];
                env_[2] *= coef_[2];
            }
            const float c = k48(0.1f);
            envLp_[0] += (env_[1] - envLp_[0]) * c;
            envLp_[1] += (env_[2] - envLp_[1]) * c;
            envLp_[2] += (env_[0] - envLp_[2]) * c;
            // A sine that gets dirtier (folded towards a soft triangle, its phase wobbling).
            float ph = phase_[0] + phaseNoise_ * dirt;
            ph -= std::floor(ph);
            const float tri = (ph < 0.5f ? ph : 1.0f - ph) * 4.0f - 1.0f;
            const float soft = 2.0f * tri / (1.0f + std::abs(tri));
            const float clean = std::sin(dsp::kTwoPi * (ph + 0.75f));
            const float body = soft + (1.0f - dirt) * (clean - soft);
            // The click: a slope-limited step, high-passed and low-passed; plus a little noise.
            const float in = hold_ > 0 ? 0.0f : 1.0f;
            clickLp_ += (in - clickLp_) * (in > clickLp_ ? k48(0.5f) : k48(0.1f));
            clickHp_ += (clickLp_ - clickHp_) * k48(0.04f);
            const float click = res_[0].lowpass(clickLp_ - clickHp_);
            noiseLp_ += (random() - noiseLp_) * k48(0.05f);
            noiseHp_ += (noiseLp_ - noiseHp_) * k48(0.005f);
            const float transient = click + (noiseLp_ - noiseHp_);
            auto vca = [](float sample, float gain) {
                sample = (sample - 0.6f) * gain;
                return 3.0f * sample / (2.0f + std::abs(sample)) + gain * 0.3f;
            };
            const float mix = -vca(body, envLp_[0]) - transient * envLp_[1] * spec_.tone;
            toneLp_ += (mix - toneLp_) * std::min(4.0f * f0 * semis(spec_.tone * 108.0f), 1.0f);
            return toneLp_;
        }
        case E::AnalogSnare:
        {
            float pulse = 0.0f;
            if (pulseRemaining_ > 0)
            {
                --pulseRemaining_;
                pulse = pulseRemaining_ ? pulseHeight_ : pulseHeight_ - 1.0f;
                pulse_ = pulse;
            }
            else
            {
                pulse_ *= 1.0f - 1.0f / (0.1e-3f * sr);
                pulse = pulse_;
            }
            pulseLp_ += (pulse - pulseLp_) * k48(0.75f);
            float shell = 0.0f;
            for (int i = 0; i < 5; ++i)
            {
                if (gains_[i] <= 0.0f)
                    continue;
                const float ex = i == 0 ? (pulse - pulseLp_) + 0.006f * pulse : 0.026f * pulse;
                shell += gains_[i] * (res_[i].bandpass(ex) + ex * leak_);
            }
            shell = dsp::fastTanh(shell);
            const float snappy = std::clamp(spec_.snap * 1.1f - 0.05f, 0.0f, 1.0f);
            float n = 2.0f * random() - 1.0f;
            if (n < 0.0f)
                n = 0.0f;
            env_[0] *= coef_[0];
            n = noiseFilter_.bandpass(n * env_[0] * snappy * 2.0f);
            return n + shell * (1.0f - snappy);
        }
        case E::SynthSnare:
        {
            const float snappy = std::clamp(spec_.snap * 1.1f - 0.05f, 0.0f, 1.0f), fm = spec_.punch * spec_.punch;
            if (env_[0] > 0.03f || (static_cast<int>(t_ * sr) & 1))
                env_[0] *= coef_[0];
            if (hold_ > 0)
                --hold_;
            else
                env_[1] *= coef_[1];
            env_[2] *= coef_[2];
            const float f = f0 * (1.0f + fm * 4.0f * env_[2]);
            phase_[0] += f;
            phase_[1] += f * 1.47f;
            phase_[0] -= std::floor(phase_[0]);
            phase_[1] -= std::floor(phase_[1]);
            auto shape = [](float p) {
                const float tri = (p < 0.5f ? p : 1.0f - p) * 4.0f - 1.3f;
                return 2.0f * tri / (1.0f + std::abs(tri));
            };
            float drum = -0.1f + shape(phase_[0]) * 0.6f + shape(phase_[1]) * 0.25f;
            drum = onePole_[0].lowpass(drum * env_[0] * std::sqrt(1.0f - snappy));
            float snare = noiseFilter_.lowpass(random());
            snare = onePole_[1].highpass(snare);
            snare = (snare + 0.1f) * (env_[1] + env_[2]) * std::sqrt(snappy);
            return snare + drum;
        }
        case E::Hat808:
        case E::Hat909:
        {
            float metal = 0.0f;
            if (spec_.engine == E::Hat808)
            {
                for (int i = 0; i < 6; ++i)
                {
                    phase_[i] += freqs_[i];
                    phase_[i] -= std::floor(phase_[i]);
                    metal += phase_[i] < 0.5f ? 1.0f : 0.0f;
                }
                metal = 0.33f * metal - 1.0f;
            }
            else
                for (int pair = 0; pair < 3; ++pair)
                {
                    float& a = phase_[2 * pair];
                    float& b = phase_[2 * pair + 1];
                    const float sq = dsp::squareBlep(a, freqs_[2 * pair], 0.5f), saw = dsp::sawBlep(b, freqs_[2 * pair + 1]);
                    metal += sq * saw;
                    a += freqs_[2 * pair];
                    a -= std::floor(a);
                    b += freqs_[2 * pair + 1];
                    b -= std::floor(b);
                }
            float x = colour_.bandpass(metal);
            const float noisiness = spec_.snap * spec_.snap;
            noiseClock_ += std::min(f0 * (16.0f + 16.0f * (1.0f - noisiness)), 0.5f);
            if (noiseClock_ >= 1.0f)
            {
                noiseClock_ -= 1.0f;
                noiseSample_ = random() - 0.5f;
            }
            x += noisiness * (noiseSample_ - x);
            if (spec_.engine == E::Hat808)
            {
                env_[0] *= coef_[0];
                float s = x * (x > 0.0f ? 4.0f : 0.1f);
                s = s / (1.0f + std::abs(s));
                x = (s + 0.1f) * env_[0];
            }
            else
            {
                env_[0] *= env_[0] > 0.5f ? coef_[0] : coef_[1];
                x *= env_[0];
            }
            return hp_.highpass(x);
        }
        case E::AcousticKick:
        case E::AcousticSnare:
        case E::Tom:
        {
            float out = 0.0f;
            const float tension = env_[3];
            const float sag = semis(std::max(0.0f, (energy_ - 0.15f) / 0.85f) * tension);
            energy_ *= energyCoef_;
            const float interval = semis(freqs_[0]);
            float heads[2] = { 0.0f, 0.0f };
            for (int h = 0; h < 2; ++h)
            {
                const float f = spec_.hz * sag * (h == 1 ? interval : 1.0f);
                for (int i = 0; i < 5; ++i)
                {
                    float& env = headEnv_[h][i];
                    if (env <= 1e-5f)
                        continue;
                    const float hz = (f - 20.0f) * static_cast<float>(i + 1) + 20.0f;
                    if (hz < sr * 0.45f)
                    {
                        headPhase_[h][i] += hz / sr;
                        headPhase_[h][i] -= std::floor(headPhase_[h][i]);
                        heads[h] += std::sin(dsp::kTwoPi * headPhase_[h][i]) * env * headGain_[i];
                    }
                    env *= headCoef_[h][i];
                }
            }
            out += heads[0] * gains_[0] + heads[1] * gains_[1];
            if (gains_[2] > 0.0f) // wires rattle against the resonant head as long as it moves
            {
                const float level = std::abs(heads[1]);
                wireEnv_ += (level - wireEnv_) * (level > wireEnv_ ? wireAttack_ : wireRelease_);
                if (wireEnv_ < 1e-5f)
                    wireEnv_ = 0.0f;
                const float density = (5000.0f + 5000.0f * std::min(1.0f, wireEnv_ * 2.0f)) / sr;
                const float tick = random() < density ? wireEnv_ * (0.6f + 0.4f * noise_.next()) / std::sqrt(density) : 0.0f;
                out += onePole_[0].lowpass(tick * 0.5f + colour_.bandpass(tick) * 1.2f) * gains_[2];
            }
            if (shellEnv_ > 1e-5f)
            {
                shellFm_ += 117.0f / sr;
                shellFm_ -= std::floor(shellFm_);
                shellPhase_ += spec_.hz * (spec_.engine == E::AcousticKick ? 9.0f : 3.7f) * (1.0f + 0.25f * std::sin(dsp::kTwoPi * shellFm_)) / sr;
                shellPhase_ -= std::floor(shellPhase_);
                out += std::sin(dsp::kTwoPi * shellPhase_) * shellEnv_ * 8.0f;
                shellEnv_ *= shellCoef_;
            }
            if (env_[0] > 1e-5f) // the stick
            {
                out += onePole_[1].lowpass(noise_.next() + 0.8f) * env_[0];
                env_[0] *= coef_[0];
            }
            if (headEnv_[0][0] <= 1e-5f && headEnv_[1][0] <= 1e-5f && wireEnv_ <= 1e-5f && env_[0] <= 1e-5f)
                silent_ = 1 << 30; // done
            return out;
        }
        case E::Cymbal:
        {
            if (env_[0] <= 1e-5f)
            {
                silent_ = 1 << 30;
                return 0.0f;
            }
            if (--control_ <= 0)
            {
                control_ = 16;
                auto between = [](float t, float from, float to) { return from * std::pow(to / from, dsp::clamp01(t)); };
                const float size = env_[3];
                const float rest = between(size, 4800.0f, 1000.0f), excited = between(size, 4800.0f, 150.0f);
                hp_.set(std::min(between(env_[0], rest, excited), sr * 0.4f) / sr, 0.7f);
                colour_.set(std::min(between(size, 8000.0f, 12800.0f), sr * 0.45f) / sr, 0.7f);
            }
            lfo_[0] += 15.31f / sr;
            lfo_[0] -= std::floor(lfo_[0]);
            lfo_[1] += 9.31f / sr;
            lfo_[1] -= std::floor(lfo_[1]);
            const float wander = (1.0f + 0.006f * std::sin(dsp::kTwoPi * lfo_[0])) * (1.0f + 0.006f * std::sin(dsp::kTwoPi * lfo_[1]));
            static const float levels[4] = { 1.0f, 1.0f, 0.8f, 0.6f };
            float sum = 0.0f;
            for (int i = 0; i < 4; ++i)
            {
                float* line = comb_.data() + i * kCombLength;
                const float delay = std::clamp(combDelay_[i] / wander, 2.0f, static_cast<float>(kCombLength - 2));
                const float pos = static_cast<float>(combWrite_ + kCombLength) - delay;
                const int i0 = static_cast<int>(pos);
                const float frac = pos - static_cast<float>(i0);
                const float read = line[i0 % kCombLength] + (line[(i0 + 1) % kCombLength] - line[i0 % kCombLength]) * frac;
                const float strike = dsp::fastTanh(noise_.next() * strike_ * (1.0f + 2.0f * acc));
                const float y = strike + read * combFb_[i];
                line[combWrite_] = std::abs(y) < 1e-20f ? 0.0f : y;
                sum += y * levels[i];
            }
            combWrite_ = (combWrite_ + 1) % kCombLength;
            strike_ *= strikeCoef_;
            env_[0] *= coef_[0];
            float c = hp_.highpass(sum * 0.07f);
            c = colour_.lowpass(onePole_[0].highpass(c));
            return c;
        }
        case E::CrackSnare:
        {
            const auto size = string_.size();
            const float pos = static_cast<float>(static_cast<size_t>(stringWrite_) + size) - stringDelay_;
            const auto i0 = static_cast<size_t>(pos);
            const float frac = pos - static_cast<float>(i0);
            const float read = string_[i0 % size] + (string_[(i0 + 1) % size] - string_[i0 % size]) * frac;
            stringLp_ += (read - stringLp_) * (1.0f - stringDamp_);
            float y = stringLp_ * stringFb_;
            if (hold_ > 0)
            {
                --hold_;
                y += noise_.next() * (0.5f + 0.5f * acc);
            }
            string_[static_cast<size_t>(stringWrite_)] = dsp::fastTanh(y);
            stringWrite_ = static_cast<int>((static_cast<size_t>(stringWrite_) + 1) % size);
            env_[0] *= coef_[0];
            const float crack = hp_.highpass(noise_.next()) * env_[0] * spec_.snap * 1.5f;
            return y * 0.7f + crack;
        }
        case E::Clap:
        {
            if (bursts_ > 0 && t_ >= nextBurst_)
            {
                env_[0] = 1.0f;
                --bursts_;
                nextBurst_ += 0.008f + 0.006f * random();
            }
            float e = env_[0];
            env_[0] *= coef_[0];
            if (bursts_ == 0)
            {
                e = std::max(e, env_[1] * 0.6f);
                env_[1] *= coef_[1];
            }
            return noiseFilter_.bandpass(noise_.next()) * e * 1.8f * (0.3f + 0.7f * acc);
        }
        case E::Cowbell:
        {
            float x = 0.0f;
            for (int i = 0; i < 2; ++i)
            {
                x += dsp::squareBlep(phase_[i], freqs_[i], 0.5f);
                phase_[i] += freqs_[i];
                phase_[i] -= std::floor(phase_[i]);
            }
            env_[0] *= env_[0] > 0.25f ? coef_[0] : coef_[1];
            return colour_.bandpass(x) * env_[0];
        }
        case E::Shaker:
        {
            env_[0] = std::min(1.0f, env_[0] + coef_[0]);
            env_[1] *= coef_[1];
            return colour_.bandpass(noise_.next()) * env_[0] * env_[1] * 1.5f;
        }
        case E::Bell:
        {
            float x = 0.0f;
            for (int i = 0; i < 4; ++i)
            {
                phase_[i] += freqs_[i];
                phase_[i] -= std::floor(phase_[i]);
                x += std::sin(dsp::kTwoPi * phase_[i]) * gains_[i] * headEnv_[0][i];
                headEnv_[0][i] *= headCoef_[0][i];
            }
            if (env_[0] > 1e-5f)
            {
                x += noise_.next() * env_[0];
                env_[0] *= coef_[0];
            }
            if (headEnv_[0][0] <= 1e-5f)
                silent_ = 1 << 30;
            return x * acc;
        }
        default: return 0.0f;
    }
}

float DrumVoice::tick()
{
    if (!active_)
        return 0.0f;
    float out = render() * engineGain(spec_.engine) * spec_.gain;
    if (velocityToneOn_)
        out = velocityTone_.lowpass(out);
    if (filterMode_ > 0)
    {
        float lp, bp, hp;
        filter_.tick(out, lp, bp, hp);
        out = filterMode_ == 1 ? lp : filterMode_ == 2 ? bp * 1.5f : hp;
    }
    out = dsp::drive(out, spec_.drive);
    out = dc_.highpass(out);
    if (choke_)
    {
        chokeGain_ *= chokeCoef_;
        out *= chokeGain_;
    }
    lastOut_ = out;
    if (std::abs(declick_) > 1e-6f)
    {
        out += declick_;
        declick_ *= declickCoef_;
    }
    t_ += 1.0f / sampleRate_;
    // Done: silent for 50 ms after the first 30 ms (or the engine said so).
    silent_ = std::abs(out) < 2e-5f ? silent_ + 1 : (silent_ >= (1 << 30) ? silent_ : 0);
    if ((silent_ > static_cast<int>(0.05f * sampleRate_) && t_ > 0.03f && bursts_ == 0) || (choke_ && chokeGain_ < 1e-4f))
    {
        active_ = false;
        if (spec_.engine == E::Cymbal)
            for (float& v : comb_)
                v = 0.0f;
    }
    return out;
}

} // namespace gristo::sound
