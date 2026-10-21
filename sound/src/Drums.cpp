#include "Drums.h"

namespace gristo::sound {

namespace {

// Small builder so the kit tables below read like a sound designer's notes.
struct P
{
    DrumPatch d;
    P& body(float level, float f0, float f1, float pitchTime, float decay)
    {
        d.body = level; d.f0 = f0; d.f1 = f1; d.pitchTime = pitchTime; d.bodyDecay = decay;
        return *this;
    }
    P& body2(float level, float ratio) { d.body2 = level; d.body2Ratio = ratio; return *this; }
    P& noise(float level, float decay, float hz, float q, NoiseFilter filter, float attack = 0.0f)
    {
        d.noise = level; d.noiseDecay = decay; d.noiseHz = hz; d.noiseQ = q; d.noiseFilter = filter; d.noiseAttack = attack;
        return *this;
    }
    P& claps(int count, float spacing) { d.claps = count; d.clapSpacing = spacing; return *this; }
    P& metal(float level, float decay, float hz) { d.metal = level; d.metalDecay = decay; d.metalHz = hz; return *this; }
    P& click(float level) { d.click = level; return *this; }
    P& drive(float amount) { d.drive = amount; return *this; }
    P& gain(float g) { d.gain = g; return *this; }
};

constexpr auto HP = NoiseFilter::Highpass;
constexpr auto BP = NoiseFilter::Bandpass;
constexpr float kPans[kNumDrumVoices] = { 0.0f, 0.05f, 0.25f, 0.3f, -0.3f, -0.2f };

KitPatch makeKit(std::initializer_list<P> voices, float lowpass = 20000.0f, int crush = 1, float busDrive = 1.0f)
{
    KitPatch k;
    int v = 0;
    for (const P& p : voices)
    {
        k.voices[v] = p.d;
        k.voices[v].pan = kPans[v];
        ++v;
    }
    k.busLowpassHz = lowpass;
    k.crush = crush;
    k.busDrive = busDrive;
    return k;
}

const KitPatch* buildKits()
{
    static KitPatch kits[static_cast<int>(Kit::Count)];
    // Classic: the original preview kit.
    kits[0] = makeKit({
        P().body(1.0f, 160.0f, 46.0f, 0.0087f, 0.45f).click(0.15f),
        P().body(0.55f, 210.0f, 185.0f, 0.0045f, 0.12f).noise(0.7f, 0.2f, 1200.0f, 0.2f, HP),
        P().noise(0.45f, 0.045f, 7000.0f, 0.2f, HP),
        P().noise(0.35f, 0.32f, 7000.0f, 0.2f, HP),
        P().noise(0.6f, 0.16f, 1200.0f, 0.4f, BP).claps(3, 0.011f),
        P().body(0.8f, 240.0f, 150.0f, 0.012f, 0.28f).click(0.05f),
    });
    // 808: round boom, two-tone snare, metallic hats, clap, conga.
    kits[1] = makeKit({
        P().body(1.0f, 105.0f, 50.0f, 0.025f, 0.9f).click(0.06f).drive(1.4f),
        P().body(0.45f, 185.0f, 180.0f, 0.01f, 0.16f).body2(0.32f, 1.78f).noise(0.55f, 0.2f, 1800.0f, 0.15f, HP),
        P().metal(0.5f, 0.06f, 8500.0f).noise(0.08f, 0.03f, 9000.0f, 0.2f, HP).gain(1.4f),
        P().metal(0.45f, 0.5f, 8500.0f).noise(0.06f, 0.35f, 9000.0f, 0.2f, HP).gain(1.4f),
        P().noise(0.7f, 0.25f, 1100.0f, 0.5f, BP).claps(4, 0.009f),
        P().body(0.75f, 340.0f, 310.0f, 0.008f, 0.22f).click(0.08f),
    });
    // Trap: long saturated 808 boom, crisp snare+clap layer, tight hats, rim snap.
    kits[2] = makeKit({
        P().body(1.0f, 150.0f, 45.0f, 0.045f, 2.0f).click(0.18f).drive(2.4f),
        P().body(0.3f, 230.0f, 200.0f, 0.006f, 0.1f).noise(0.85f, 0.17f, 2600.0f, 0.15f, HP).claps(2, 0.007f),
        P().metal(0.4f, 0.035f, 9500.0f).noise(0.25f, 0.028f, 10000.0f, 0.2f, HP).gain(1.4f),
        P().metal(0.38f, 0.32f, 9500.0f).noise(0.18f, 0.26f, 10000.0f, 0.2f, HP).gain(1.4f),
        P().noise(0.8f, 0.3f, 1400.0f, 0.45f, BP).claps(3, 0.012f),
        P().body(0.45f, 1750.0f, 1650.0f, 0.004f, 0.035f).noise(0.45f, 0.05f, 2600.0f, 0.35f, BP).click(0.2f),
    });
    // Lo-fi boom bap: thick and dusty, dark hats, rim and shaker, filtered + crushed bus.
    kits[3] = makeKit({
        P().body(1.0f, 115.0f, 52.0f, 0.022f, 0.38f).click(0.25f).drive(1.8f),
        P().body(0.5f, 195.0f, 175.0f, 0.008f, 0.13f).noise(0.6f, 0.2f, 2800.0f, 0.12f, BP).drive(1.5f),
        P().noise(0.42f, 0.045f, 5500.0f, 0.2f, HP).gain(1.4f),
        P().noise(0.3f, 0.28f, 5000.0f, 0.2f, HP).metal(0.12f, 0.25f, 7000.0f).gain(1.4f),
        P().body(0.6f, 820.0f, 780.0f, 0.004f, 0.045f).noise(0.3f, 0.035f, 2500.0f, 0.3f, BP).click(0.3f),
        P().noise(0.45f, 0.08f, 6500.0f, 0.25f, BP, 0.012f).gain(2.0f),
    }, 5200.0f, 3, 1.6f);
    // Minimal / electro: short and clicky, rim, ticks, blip and clave.
    kits[4] = makeKit({
        P().body(1.0f, 210.0f, 58.0f, 0.008f, 0.22f).click(0.35f),
        P().body(0.7f, 1100.0f, 1020.0f, 0.003f, 0.04f).noise(0.35f, 0.045f, 3500.0f, 0.3f, BP).click(0.4f),
        P().noise(0.35f, 0.022f, 9500.0f, 0.2f, HP).metal(0.2f, 0.02f, 9000.0f).gain(1.4f),
        P().metal(0.35f, 0.13f, 9000.0f).noise(0.15f, 0.1f, 9500.0f, 0.2f, HP).gain(1.4f),
        P().body(0.6f, 1600.0f, 1200.0f, 0.006f, 0.07f),
        P().body(0.7f, 2500.0f, 2500.0f, 0.01f, 0.055f).body2(0.3f, 0.62f),
    });
    // Reggaeton: punchy kick, snappy dembow snare, crisp hats, shaker, clap, timbale.
    kits[5] = makeKit({
        P().body(1.0f, 150.0f, 52.0f, 0.025f, 0.45f).click(0.2f).drive(1.8f),
        P().body(0.45f, 260.0f, 240.0f, 0.006f, 0.08f).noise(0.8f, 0.13f, 3000.0f, 0.15f, HP).claps(2, 0.006f),
        P().noise(0.38f, 0.04f, 8000.0f, 0.2f, HP).metal(0.18f, 0.035f, 9000.0f).gain(1.4f),
        P().noise(0.4f, 0.12f, 7000.0f, 0.3f, BP, 0.01f).gain(1.4f),
        P().noise(0.75f, 0.2f, 1300.0f, 0.45f, BP).claps(3, 0.010f),
        P().body(0.65f, 480.0f, 440.0f, 0.008f, 0.32f).body2(0.25f, 1.52f).noise(0.2f, 0.03f, 4000.0f, 0.3f, BP),
    });
    // House (909-style): clicky punchy kick, bright snare, sizzling hats, wide clap, ride.
    kits[6] = makeKit({
        P().body(1.0f, 220.0f, 50.0f, 0.011f, 0.5f).click(0.32f).drive(1.6f),
        P().body(0.5f, 225.0f, 190.0f, 0.006f, 0.12f).body2(0.22f, 1.48f).noise(0.75f, 0.22f, 4200.0f, 0.15f, HP),
        P().metal(0.32f, 0.05f, 10500.0f).noise(0.32f, 0.045f, 10500.0f, 0.2f, HP).gain(1.4f),
        P().metal(0.3f, 0.42f, 10500.0f).noise(0.26f, 0.4f, 10000.0f, 0.2f, HP).gain(1.4f),
        P().noise(0.8f, 0.24f, 1250.0f, 0.45f, BP).claps(4, 0.008f),
        P().metal(0.26f, 1.1f, 6200.0f).noise(0.1f, 0.9f, 8000.0f, 0.3f, HP).gain(1.3f),
    }, 20000.0f, 1, 1.1f);
    // Techno: deep driven kick, dark clap-snare, tight metallic hats, tom, zap.
    kits[7] = makeKit({
        P().body(1.0f, 135.0f, 44.0f, 0.02f, 0.75f).click(0.25f).drive(2.8f),
        P().body(0.3f, 185.0f, 170.0f, 0.008f, 0.1f).noise(0.75f, 0.18f, 1700.0f, 0.4f, BP).claps(2, 0.008f),
        P().metal(0.45f, 0.03f, 11000.0f).noise(0.15f, 0.02f, 11000.0f, 0.2f, HP).gain(1.4f),
        P().metal(0.4f, 0.26f, 11000.0f).noise(0.12f, 0.22f, 10000.0f, 0.2f, HP).gain(1.4f),
        P().body(0.75f, 190.0f, 120.0f, 0.03f, 0.28f).click(0.1f).drive(1.5f),
        P().body(0.6f, 2400.0f, 380.0f, 0.009f, 0.07f).click(0.2f),
    }, 14000.0f, 1, 1.4f);
    // Breakbeat: sampled-break feel; punchy kick, crunchy snare, dusty hats, ghost rim, ride.
    kits[8] = makeKit({
        P().body(1.0f, 145.0f, 55.0f, 0.018f, 0.35f).click(0.3f).drive(2.0f),
        P().body(0.55f, 205.0f, 185.0f, 0.007f, 0.13f).body2(0.3f, 1.7f).noise(0.85f, 0.2f, 3500.0f, 0.25f, BP).drive(2.0f),
        P().noise(0.4f, 0.05f, 7500.0f, 0.2f, HP).metal(0.15f, 0.04f, 8000.0f).gain(1.4f),
        P().noise(0.35f, 0.3f, 7000.0f, 0.2f, HP).metal(0.15f, 0.3f, 8000.0f).gain(1.4f),
        P().body(0.5f, 900.0f, 850.0f, 0.004f, 0.05f).noise(0.4f, 0.06f, 3000.0f, 0.3f, BP).click(0.3f),
        P().metal(0.28f, 0.8f, 6500.0f).noise(0.1f, 0.6f, 8000.0f, 0.3f, HP).gain(1.3f),
    }, 12000.0f, 2, 1.8f);
    // Jungle: tight kick, high-tuned ringing snare, crisp hats, sub boom, ride.
    kits[9] = makeKit({
        P().body(1.0f, 175.0f, 60.0f, 0.01f, 0.25f).click(0.35f).drive(1.5f),
        P().body(0.5f, 310.0f, 275.0f, 0.006f, 0.1f).body2(0.25f, 1.6f).noise(0.9f, 0.16f, 4500.0f, 0.15f, HP).drive(1.6f),
        P().noise(0.4f, 0.035f, 9000.0f, 0.2f, HP).gain(1.4f),
        P().noise(0.35f, 0.25f, 8000.0f, 0.2f, HP).metal(0.1f, 0.25f, 8500.0f).gain(1.4f),
        P().body(1.0f, 62.0f, 44.0f, 0.06f, 1.4f).click(0.05f).drive(1.8f),
        P().metal(0.3f, 0.7f, 7000.0f).noise(0.1f, 0.5f, 8500.0f, 0.3f, HP).gain(1.3f),
    }, 14000.0f, 2, 1.4f);
    // Amapiano: soft round kick, rim-clap snare, shaker hats, open hat, log drum, wood.
    kits[10] = makeKit({
        P().body(1.0f, 120.0f, 50.0f, 0.02f, 0.4f).click(0.1f).drive(1.3f),
        P().body(0.3f, 400.0f, 380.0f, 0.004f, 0.05f).noise(0.6f, 0.12f, 2000.0f, 0.4f, BP).claps(2, 0.007f).click(0.15f),
        P().noise(0.45f, 0.07f, 6500.0f, 0.3f, BP, 0.012f).gain(1.8f),
        P().noise(0.3f, 0.2f, 8000.0f, 0.2f, HP).gain(1.4f),
        P().body(1.0f, 115.0f, 74.0f, 0.06f, 0.6f).body2(0.35f, 2.0f).click(0.08f).drive(2.2f),
        P().body(0.6f, 430.0f, 385.0f, 0.008f, 0.15f).body2(0.2f, 1.5f).click(0.15f),
    }, 20000.0f, 1, 1.2f);
    // Dancehall: punchy short kick, tight high snare, crisp hats, shaker, rim, clap.
    kits[11] = makeKit({
        P().body(1.0f, 165.0f, 55.0f, 0.015f, 0.35f).click(0.2f).drive(1.6f),
        P().body(0.5f, 285.0f, 262.0f, 0.005f, 0.08f).noise(0.8f, 0.12f, 3500.0f, 0.15f, HP),
        P().noise(0.38f, 0.035f, 8500.0f, 0.2f, HP).metal(0.12f, 0.03f, 9000.0f).gain(1.4f),
        P().noise(0.42f, 0.09f, 7000.0f, 0.3f, BP, 0.01f).gain(1.6f),
        P().body(0.5f, 1700.0f, 1650.0f, 0.003f, 0.03f).noise(0.4f, 0.04f, 2800.0f, 0.35f, BP).click(0.3f),
        P().noise(0.75f, 0.2f, 1300.0f, 0.45f, BP).claps(3, 0.01f),
    }, 20000.0f, 1, 1.3f);
    return kits;
}

// The six 808 hat oscillator frequencies, scaled so the bandpass sits at `metalHz`.
constexpr float kMetalHz[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };

} // namespace

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
        case Kit::Count: break;
    }
    return "";
}

const KitPatch& kitPatch(Kit kit)
{
    static const KitPatch* kits = buildKits();
    const int i = std::clamp(static_cast<int>(kit), 0, static_cast<int>(Kit::Count) - 1);
    return kits[i];
}

void DrumVoice::trigger(const DrumPatch& patch, float velocity, float tuneSemis, float decayScale)
{
    patch_ = patch;
    // No two hits quite alike: a few cents and a touch of brightness, as a real drum.
    const float wobble = noiseGen_.next(), shimmer = noiseGen_.next();
    const float tune = std::exp2(tuneSemis / 12.0f + wobble * 0.008f);
    patch_.f0 *= tune;
    patch_.f1 *= tune;
    patch_.noiseHz = std::min(patch_.noiseHz * std::sqrt(tune), sampleRate_ * 0.45f);
    patch_.metalHz = std::min(patch_.metalHz * tune, sampleRate_ * 0.45f);

    const float sr = sampleRate_;
    velocity_ = 0.1f + 0.9f * velocity * velocity;
    // Harder hits are brighter and punchier, soft ones darker: not only louder.
    const float v = std::clamp(velocity, 0.0f, 1.0f);
    patch_.noiseHz = std::min(patch_.noiseHz * (0.85f + 0.3f * v) * (1.0f + 0.04f * shimmer), sr * 0.45f);
    const float toneHz = 1800.0f * std::exp2(v * 3.6f); // ~4 kHz at 0.3, open at full
    velocityToneOn_ = toneHz < 16000.0f;
    velocityTone_.reset();
    velocityTone_.setLowpass(toneHz, sr);
    patch_.click *= 0.5f + 0.8f * v;
    if (patch_.f0 > patch_.f1)
        patch_.f0 = patch_.f1 + (patch_.f0 - patch_.f1) * (0.6f + 0.6f * v);
    declick_ = active_ ? lastOut_ : 0.0f;
    declickCoef_ = dsp::decayCoef(0.003f, sr);
    t_ = 0.0f;
    active_ = true;
    choke_ = false;
    chokeGain_ = 1.0f;

    phase_ = phase2_ = 0.0f;
    freq_ = patch_.f0;
    pitchCoef_ = std::exp(-1.0f / (std::max(patch_.pitchTime, 0.0005f) * sr));
    bodyEnv_ = patch_.body > 0.0f || patch_.body2 > 0.0f ? 1.0f : 0.0f;
    bodyCoef_ = dsp::decayCoef(patch_.bodyDecay * decayScale, sr);

    noiseEnv_ = patch_.noise > 0.0f && patch_.claps == 0 ? 1.0f : 0.0f;
    noiseCoef_ = patch_.claps > 0 ? dsp::decayCoef(0.012f, sr) : dsp::decayCoef(patch_.noiseDecay * decayScale, sr);
    clapIndex_ = 0;
    nextClap_ = 0.0f;
    noiseFilter_.reset();
    noiseFilter_.set(patch_.noiseHz, patch_.noiseQ, sr);

    metalEnv_ = patch_.metal > 0.0f ? 1.0f : 0.0f;
    metalCoef_ = dsp::decayCoef(patch_.metalDecay * decayScale, sr);
    metalFilter_.reset();
    metalFilter_.set(patch_.metalHz, 0.35f, sr);
    metalHp_.reset();
    metalHp_.setLowpass(patch_.metalHz * 0.6f, sr);

    clickEnv_ = patch_.click > 0.0f ? 1.0f : 0.0f;
    clickCoef_ = dsp::decayCoef(0.004f, sr);
    clickLp_.reset();
    clickLp_.setLowpass(3500.0f, sr);
}

float DrumVoice::tick()
{
    if (!active_)
        return 0.0f;
    const float sr = sampleRate_;
    float out = 0.0f;

    if (bodyEnv_ > 1e-5f)
    {
        freq_ = patch_.f1 + (freq_ - patch_.f1) * pitchCoef_;
        phase_ += freq_ / sr;
        phase_ -= std::floor(phase_);
        float s = std::sin(dsp::kTwoPi * phase_) * patch_.body;
        if (patch_.body2 > 0.0f)
        {
            phase2_ += freq_ * patch_.body2Ratio / sr;
            phase2_ -= std::floor(phase2_);
            s += std::sin(dsp::kTwoPi * phase2_) * patch_.body2;
        }
        out += s * bodyEnv_;
        bodyEnv_ *= bodyCoef_;
    }

    if (patch_.noise > 0.0f)
    {
        if (clapIndex_ < patch_.claps && t_ >= nextClap_)
        {
            noiseEnv_ = 1.0f;
            ++clapIndex_;
            nextClap_ += patch_.clapSpacing;
            if (clapIndex_ == patch_.claps) // last burst: let the tail ring
                noiseCoef_ = dsp::decayCoef(patch_.noiseDecay, sr);
        }
        if (noiseEnv_ > 1e-5f)
        {
            const auto f = noiseFilter_.tick(noiseGen_.next());
            const float n = patch_.noiseFilter == NoiseFilter::Highpass ? f.hp
                          : patch_.noiseFilter == NoiseFilter::Bandpass ? f.bp * 1.8f
                                                                        : f.lp;
            const float attack = patch_.noiseAttack > 0.0f ? std::min(1.0f, t_ / patch_.noiseAttack) : 1.0f;
            out += n * patch_.noise * noiseEnv_ * attack;
            noiseEnv_ *= noiseCoef_;
        }
    }

    if (metalEnv_ > 1e-5f)
    {
        const float scale = patch_.metalHz / 8000.0f;
        float sum = 0.0f;
        for (int i = 0; i < 6; ++i)
        {
            metalPhase_[i] += kMetalHz[i] * scale * 4.0f / sr;
            metalPhase_[i] -= std::floor(metalPhase_[i]);
            sum += metalPhase_[i] < 0.5f ? 1.0f : -1.0f;
        }
        const float m = metalHp_.highpass(metalFilter_.tick(sum * 0.3f).bp);
        out += m * patch_.metal * metalEnv_ * 2.0f;
        metalEnv_ *= metalCoef_;
    }

    if (clickEnv_ > 1e-5f)
    {
        out += clickLp_.lowpass(noiseGen_.next() + 0.8f) * patch_.click * clickEnv_;
        clickEnv_ *= clickCoef_;
    }

    if (velocityToneOn_)
        out = velocityTone_.lowpass(out);
    out = dsp::drive(out, patch_.drive) * velocity_ * patch_.gain;
    if (choke_)
    {
        chokeGain_ *= 0.995f;
        out *= chokeGain_;
    }
    lastOut_ = out;
    if (std::abs(declick_) > 1e-6f)
    {
        out += declick_;
        declick_ *= declickCoef_;
    }
    t_ += 1.0f / sr;

    const bool clapsPending = clapIndex_ < patch_.claps;
    if (!clapsPending && bodyEnv_ <= 1e-5f && noiseEnv_ <= 1e-5f && metalEnv_ <= 1e-5f && clickEnv_ <= 1e-5f)
        active_ = false;
    if (choke_ && chokeGain_ < 1e-4f)
        active_ = false;
    return out;
}

} // namespace gristo::sound
