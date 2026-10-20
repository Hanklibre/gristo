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
        case SynthSound::Count: break;
    }
    return "";
}

const char* synthCategoryName(SynthSound sound)
{
    const int i = static_cast<int>(sound);
    if (i <= static_cast<int>(SynthSound::FmBass)) return "Bass";
    if (i <= static_cast<int>(SynthSound::Organ)) return "Keys";
    if (i <= static_cast<int>(SynthSound::DarkPad)) return "Pads";
    return "Leads";
}

// ---------------------------------------------------------------------------------

void SynthVoice::start(const SynthPatch& patch, const SynthParams& macros, int id, float pitch, float velocity)
{
    const bool wasActive = active();
    patch_ = patch;
    id_ = id;
    pitch_ = targetPitch_ = pitch;
    velocity_ = dsp::clamp01(velocity);
    amp_.set(attackTime(patch_, macros), patch_.d, patch_.s, releaseTime(patch_, macros), sampleRate_);
    filterEnv_.set(patch_.fa, patch_.fd, patch_.fs, patch_.fr, sampleRate_);
    amp_.gateOn();
    filterEnv_.gateOn();
    fmEnv_ = 1.0f;
    fmCoef_ = dsp::decayCoef(std::max(patch_.fmIndexTime, 0.01f), sampleRate_);
    tineEnv_ = patch_.tine > 0.0f ? 1.0f : 0.0f;
    tineCoef_ = dsp::decayCoef(0.25f, sampleRate_);
    if (!wasActive)
    {
        for (int i = 0; i < 5; ++i)
            phase_[i] = 0.2f * static_cast<float>(i); // spread supersaw phases
        filter1_.reset();
        filter2_.reset();
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
            constexpr float spread[5] = { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f };
            for (int i = 0; i < 5; ++i)
            {
                const float d = dt * std::exp2(spread[i] * patch_.detuneCents / 1200.0f);
                advance(phase_[i], d);
                out += dsp::sawBlep(phase_[i], d);
            }
            out *= 0.45f;
            break;
        }
        case Osc::Reese:
            for (int i = 0; i < 2; ++i)
            {
                const float d = dt * std::exp2((i == 0 ? -1.0f : 1.0f) * patch_.detuneCents / 1200.0f);
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
            const float index = patch_.fmIndexEnd + (patch_.fmIndex - patch_.fmIndexEnd) * fmEnv_;
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
            const float before = phase_[0];
            advance(phase_[0], dt);
            const float ratio = 1.0f + patch_.syncAmount * filterEnv_.value();
            syncPhase_ += dt * ratio;
            if (phase_[0] < before) // master wrapped: hard sync
                syncPhase_ = phase_[0] * ratio;
            syncPhase_ -= std::floor(syncPhase_);
            out = 2.0f * syncPhase_ - 1.0f;
            break;
        }
    }
    if (patch_.sub > 0.0f)
    {
        advance(subPhase_, dt * 0.5f);
        out += patch_.sub * std::sin(dsp::kTwoPi * subPhase_);
    }
    if (tineEnv_ > 1e-4f)
    {
        advance(tinePhase_, std::min(dt * 14.0f, 0.45f));
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
    const float cutoff = base * keyTrack * std::exp2(envOct * filterEnv_.value());

    const float r = dsp::clamp01(m.resonance);
    const float reso = r <= 0.5f ? patch_.resonance * r * 2.0f
                                 : patch_.resonance + (0.95f - patch_.resonance) * (r - 0.5f) * 2.0f;
    if (patch_.fourPole)
    {
        filter1_.set(cutoff, 0.1f, sampleRate_);
        filter2_.set(cutoff, reso, sampleRate_);
    }
    else
    {
        filter1_.set(cutoff, reso, sampleRate_);
    }
}

float SynthVoice::tick(const SynthParams& macros, float glideCoef)
{
    if (!amp_.active())
        return 0.0f;
    pitch_ = targetPitch_ + (pitch_ - targetPitch_) * glideCoef;
    float pitch = pitch_;
    if (patch_.vibratoHz > 0.0f)
    {
        vibPhase_ += patch_.vibratoHz / sampleRate_;
        vibPhase_ -= std::floor(vibPhase_);
        pitch += patch_.vibratoSemis * std::sin(dsp::kTwoPi * vibPhase_);
    }
    const float hz = dsp::midiToHz(pitch);

    filterEnv_.tick();
    if (--controlCounter_ <= 0)
    {
        updateFilter(macros, hz);
        controlCounter_ = 16;
    }
    fmEnv_ *= fmCoef_;

    float y = filter1_.tick(oscillator(hz)).lp;
    if (patch_.fourPole)
        y = filter2_.tick(y).lp;
    y = dsp::drive(y, patch_.drive);
    return y * amp_.tick() * (0.35f + 0.65f * velocity_) * patch_.level;
}

// ---------------------------------------------------------------------------------

void Synth::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    for (auto& v : voices_)
        v.prepare(sampleRate);
    reset();
}

void Synth::reset()
{
    for (auto& v : voices_)
        v.kill();
}

void Synth::noteOn(int id, float pitch, float velocity)
{
    // A note that starts while another is still held is a slide: glide, no retrigger.
    for (auto& v : voices_)
        if (v.gated())
        {
            v.legato(id, pitch, velocity);
            return;
        }
    SynthVoice* target = nullptr;
    for (auto& v : voices_)
        if (!v.active())
        {
            target = &v;
            break;
        }
    if (target == nullptr) // all busy releasing: steal the quietest
    {
        target = &voices_[0];
        for (auto& v : voices_)
            if (v.level() < target->level())
                target = &v;
    }
    target->start(synthPatch(params_.sound), params_, id, pitch, velocity);
}

void Synth::noteOff(int id)
{
    for (auto& v : voices_)
        if (v.gated() && v.id() == id)
            v.release(params_);
}

void Synth::allNotesOff()
{
    for (auto& v : voices_)
        if (v.gated())
            v.release(params_);
}

float Synth::tick()
{
    const float glideSeconds = 0.5f * params_.glide * params_.glide;
    const float glideCoef = glideSeconds < 0.001f ? 0.0f : std::exp(-1.0f / (glideSeconds * sampleRate_ / 3.0f));
    float out = 0.0f;
    for (auto& v : voices_)
        out += v.tick(params_, glideCoef);
    return out * params_.level;
}

int Synth::activeVoices() const
{
    int n = 0;
    for (const auto& v : voices_)
        n += v.active() ? 1 : 0;
    return n;
}

} // namespace gristo::sound
