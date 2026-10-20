#include "Drums.h"
#include "Fx.h"
#include "Synth.h"

namespace gristo::sound {

namespace {
constexpr float kSynthPan[kNumSynths] = { -0.15f, 0.2f, 0.0f };
constexpr int kOpenHat = 3, kClosedHat = 2;
} // namespace

struct SoundEngine::Impl
{
    float sampleRate = 48000.0f;
    KitParams kit;
    FxParams fx;
    SynthParams synthParams[kNumSynths];
    DrumVoice drums[kNumDrumVoices];
    Synth synths[kNumSynths];
    Reverb reverb;
    Delay delay;
    dsp::OnePole busToneL, busToneR, busAirL, busAirR, busColourL, busColourR;
    float crushL = 0.0f, crushR = 0.0f;
    int crushCount = 0;
    float configuredTone = -1.0f, configuredLowpass = -1.0f;

    void configureBus()
    {
        const KitPatch& patch = kitPatch(kit.kit);
        // Tone below 0.5 closes a lowpass; above 0.5 adds air (see render).
        const float lowpassHz = kit.tone < 0.5f ? 20000.0f * std::exp2((kit.tone - 0.5f) * 8.0f) : 20000.0f;
        if (std::abs(kit.tone - configuredTone) > 1e-4f)
        {
            busToneL.setLowpass(lowpassHz, sampleRate);
            busToneR.setLowpass(lowpassHz, sampleRate);
            busAirL.setLowpass(3000.0f, sampleRate);
            busAirR.setLowpass(3000.0f, sampleRate);
            configuredTone = kit.tone;
        }
        if (std::abs(patch.busLowpassHz - configuredLowpass) > 1e-3f)
        {
            busColourL.setLowpass(patch.busLowpassHz, sampleRate);
            busColourR.setLowpass(patch.busLowpassHz, sampleRate);
            configuredLowpass = patch.busLowpassHz;
        }
    }
};

SoundEngine::SoundEngine() : impl_(std::make_unique<Impl>()) {}
SoundEngine::~SoundEngine() = default;

void SoundEngine::prepare(double sampleRate)
{
    auto& m = *impl_;
    m.sampleRate = static_cast<float>(sampleRate);
    for (auto& d : m.drums)
        d.prepare(m.sampleRate);
    for (auto& s : m.synths)
        s.prepare(m.sampleRate);
    m.synths[kChordSynth].setPoly(true);
    m.reverb.prepare(m.sampleRate);
    m.delay.prepare(m.sampleRate);
    m.configuredTone = m.configuredLowpass = -1.0f;
    reset();
}

void SoundEngine::reset()
{
    auto& m = *impl_;
    for (auto& d : m.drums)
    {
        d = DrumVoice {};
        d.prepare(m.sampleRate);
    }
    for (auto& s : m.synths)
        s.reset();
    m.reverb.reset();
    m.delay.reset();
    for (auto* f : { &m.busToneL, &m.busToneR, &m.busAirL, &m.busAirR, &m.busColourL, &m.busColourR })
        f->reset();
    m.configureBus();
}

void SoundEngine::setKit(const KitParams& params)
{
    impl_->kit = params;
    impl_->configureBus();
}

void SoundEngine::setSynth(int synth, const SynthParams& params)
{
    if (synth < 0 || synth >= kNumSynths)
        return;
    impl_->synthParams[synth] = params;
    impl_->synths[synth].setParams(params);
}

void SoundEngine::setFx(const FxParams& params)
{
    auto& m = *impl_;
    m.fx = params;
    m.reverb.setSize(params.reverbSize);
    const double bpm = std::clamp(params.bpm, 20.0, 400.0);
    m.delay.set(static_cast<float>(params.delayBeats * 60.0 / bpm), params.delayFeedback);
}

void SoundEngine::triggerDrum(int voice, float velocity)
{
    auto& m = *impl_;
    if (voice < 0 || voice >= kNumDrumVoices)
        return;
    if (voice == kClosedHat && m.drums[kOpenHat].active())
        m.drums[kOpenHat].choke(); // closed hat chokes the open one
    const KitPatch& patch = kitPatch(m.kit.kit);
    const float decayScale = dsp::macroLog(m.kit.decay, 0.3f, 1.0f, 2.5f);
    m.drums[voice].trigger(patch.voices[voice], dsp::clamp01(velocity), std::clamp(m.kit.tune, -12.0f, 12.0f), decayScale);
}

void SoundEngine::noteOn(int synth, int id, float pitch, float velocity)
{
    if (synth >= 0 && synth < kNumSynths)
        impl_->synths[synth].noteOn(id, pitch, velocity);
}

void SoundEngine::noteOff(int synth, int id)
{
    if (synth >= 0 && synth < kNumSynths)
        impl_->synths[synth].noteOff(id);
}

void SoundEngine::allNotesOff()
{
    for (auto& s : impl_->synths)
        s.allNotesOff();
}

int SoundEngine::activeSynthVoices(int synth) const
{
    return synth >= 0 && synth < kNumSynths ? impl_->synths[synth].activeVoices() : 0;
}

bool SoundEngine::drumActive(int voice) const
{
    return voice >= 0 && voice < kNumDrumVoices && impl_->drums[voice].active();
}

void SoundEngine::render(float* left, float* right, int numSamples, float gain)
{
    auto& m = *impl_;
    const KitPatch& patch = kitPatch(m.kit.kit);
    const float busDrive = patch.busDrive * (1.0f + 5.0f * dsp::clamp01(m.kit.drive));
    const float air = m.kit.tone > 0.5f ? (m.kit.tone - 0.5f) * 1.6f : 0.0f;
    const bool colour = patch.busLowpassHz < 19000.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // Drums
        float kl = 0.0f, kr = 0.0f;
        for (int v = 0; v < kNumDrumVoices; ++v)
        {
            if (!m.drums[v].active())
                continue;
            const float s = m.drums[v].tick() * m.kit.level[static_cast<size_t>(v)];
            const float pan = m.drums[v].pan();
            kl += s * (1.0f - pan);
            kr += s * (1.0f + pan);
        }
        // Kit bus: colour (lo-fi), tone, drive.
        if (colour)
        {
            kl = m.busColourL.lowpass(kl);
            kr = m.busColourR.lowpass(kr);
        }
        if (patch.crush > 1)
        {
            if (m.crushCount++ % patch.crush == 0)
            {
                m.crushL = std::round(kl * 2048.0f) / 2048.0f;
                m.crushR = std::round(kr * 2048.0f) / 2048.0f;
            }
            kl = m.crushL;
            kr = m.crushR;
        }
        kl = m.busToneL.lowpass(kl);
        kr = m.busToneR.lowpass(kr);
        if (air > 0.0f)
        {
            kl += m.busAirL.highpass(kl) * air;
            kr += m.busAirR.highpass(kr) * air;
        }
        kl = dsp::drive(kl, busDrive) * patch.gain;
        kr = dsp::drive(kr, busDrive) * patch.gain;

        float revIn = (kl + kr) * 0.5f * m.kit.reverbSend;
        float dlyIn = (kl + kr) * 0.5f * m.kit.delaySend;
        float l = kl, r = kr;

        // Synths
        for (int s = 0; s < kNumSynths; ++s)
        {
            const float y = m.synths[s].tick();
            if (y == 0.0f)
                continue;
            l += y * (1.0f - kSynthPan[s]);
            r += y * (1.0f + kSynthPan[s]);
            revIn += y * m.synthParams[s].reverbSend;
            dlyIn += y * m.synthParams[s].delaySend;
        }

        float dl = 0.0f, dr = 0.0f, rl = 0.0f, rr = 0.0f;
        m.delay.process(dlyIn, dl, dr);
        m.reverb.process(revIn + (dl + dr) * 0.15f, rl, rr);
        l += dl + rl;
        r += dr + rr;

        left[i] += std::tanh(l * gain) * 0.8f;
        right[i] += std::tanh(r * gain) * 0.8f;
    }
}

} // namespace gristo::sound
