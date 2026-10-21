#include "TestFramework.h"

#include "gristo/sound/Sound.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using namespace gristo::sound;

namespace {

constexpr double kRate = 48000.0;

struct Rendered
{
    std::vector<float> left, right;
    bool finite = true;
    float peak = 0.0f;
    float rms(size_t from, size_t to) const
    {
        double sum = 0.0;
        to = std::min(to, left.size());
        for (size_t i = from; i < to; ++i)
            sum += static_cast<double>(left[i]) * left[i] + static_cast<double>(right[i]) * right[i];
        return to > from ? static_cast<float>(std::sqrt(sum / (2.0 * static_cast<double>(to - from)))) : 0.0f;
    }
};

void renderInto(SoundEngine& engine, Rendered& out, double seconds)
{
    const auto n = static_cast<size_t>(seconds * kRate);
    std::vector<float> l(n, 0.0f), r(n, 0.0f);
    for (size_t pos = 0; pos < n; pos += 256)
    {
        const int block = static_cast<int>(std::min<size_t>(256, n - pos));
        engine.render(l.data() + pos, r.data() + pos, block, 1.0f);
    }
    for (size_t i = 0; i < n; ++i)
    {
        out.finite = out.finite && std::isfinite(l[i]) && std::isfinite(r[i]);
        out.peak = std::max({ out.peak, std::abs(l[i]), std::abs(r[i]) });
    }
    out.left.insert(out.left.end(), l.begin(), l.end());
    out.right.insert(out.right.end(), r.begin(), r.end());
}

SoundEngine& dryEngine(SoundEngine& e)
{
    e.prepare(kRate);
    FxParams fx;
    e.setFx(fx);
    return e;
}

int zeroCrossings(const std::vector<float>& x, size_t from, size_t to)
{
    int n = 0;
    for (size_t i = from + 1; i < std::min(to, x.size()); ++i)
        n += (x[i - 1] < 0.0f) != (x[i] < 0.0f) ? 1 : 0;
    return n;
}

} // namespace

TEST("sound: every drum of every kit sounds, stays finite and dies away")
{
    for (int k = 0; k < static_cast<int>(Kit::Count); ++k)
        for (int v = 0; v < kNumDrumVoices; ++v)
        {
            SoundEngine e;
            dryEngine(e);
            KitParams kit;
            kit.kit = static_cast<Kit>(k);
            kit.reverbSend = 0.0f;
            e.setKit(kit);
            e.triggerDrum(v, 1.0f);
            Rendered out;
            renderInto(e, out, 6.0);
            CHECK_MSG(out.finite, kitName(kit.kit) << " voice " << v << " not finite");
            CHECK_MSG(out.peak > 0.02f, kitName(kit.kit) << " voice " << v << " too quiet: " << out.peak);
            CHECK_MSG(out.rms(static_cast<size_t>(5.5 * kRate), out.left.size()) < 1e-4f,
                      kitName(kit.kit) << " voice " << v << " still ringing");
            CHECK(!e.drumActive(v));
        }
}

TEST("sound: kit Tune raises the kick's pitch, Decay lengthens it")
{
    auto kickCrossings = [](float tune, float decay, float& tail) {
        SoundEngine e;
        dryEngine(e);
        KitParams kit;
        kit.kit = Kit::Eight08;
        kit.tune = tune;
        kit.decay = decay;
        kit.reverbSend = 0.0f;
        e.setKit(kit);
        e.triggerDrum(0, 1.0f);
        Rendered out;
        renderInto(e, out, 1.0);
        tail = out.rms(static_cast<size_t>(0.4 * kRate), static_cast<size_t>(0.6 * kRate));
        return zeroCrossings(out.left, static_cast<size_t>(0.1 * kRate), static_cast<size_t>(0.3 * kRate));
    };
    float tailShort = 0.0f, tailLong = 0.0f, tailMid = 0.0f;
    const int low = kickCrossings(0.0f, 0.5f, tailMid);
    const int high = kickCrossings(12.0f, 0.5f, tailMid);
    CHECK_MSG(high > low * 3 / 2, "tune +12: " << high << " vs " << low);
    kickCrossings(0.0f, 0.1f, tailShort);
    kickCrossings(0.0f, 0.9f, tailLong);
    CHECK(tailLong > tailShort * 2.0f);
}

TEST("sound: the closed hat chokes the open hat")
{
    auto openHatTail = [](bool choke) {
        SoundEngine e;
        dryEngine(e);
        KitParams kit;
        kit.kit = Kit::Eight08;
        kit.reverbSend = 0.0f;
        kit.level[2] = 0.0f; // mute the closed hat itself, only its choke matters
        e.setKit(kit);
        e.triggerDrum(3, 1.0f);
        Rendered out;
        renderInto(e, out, 0.05);
        if (choke)
            e.triggerDrum(2, 1.0f);
        renderInto(e, out, 0.25);
        return out.rms(static_cast<size_t>(0.15 * kRate), static_cast<size_t>(0.3 * kRate));
    };
    CHECK(openHatTail(true) < openHatTail(false) * 0.05f);
}

TEST("sound: every synth sound plays, releases and goes quiet")
{
    for (int s = 0; s < static_cast<int>(SynthSound::Count); ++s)
    {
        SoundEngine e;
        dryEngine(e);
        SynthParams p;
        p.sound = static_cast<SynthSound>(s);
        p.reverbSend = 0.0f;
        e.setSynth(0, p);
        e.noteOn(0, 48, 48.0f, 0.9f);
        Rendered out;
        renderInto(e, out, 1.5);
        e.noteOff(0, 48);
        renderInto(e, out, 8.0);
        const char* name = synthSoundName(p.sound);
        CHECK_MSG(out.finite, name << " not finite");
        CHECK_MSG(out.peak > 0.02f, name << " too quiet: " << out.peak);
        CHECK_MSG(out.rms(static_cast<size_t>(9.0 * kRate), out.left.size()) < 1e-4f, name << " still sounding");
        CHECK_MSG(e.activeSynthVoices(0) == 0, name << " voices left active");
    }
}

TEST("sound: overlapping notes glide in one voice; separate notes let the tail ring")
{
    SoundEngine e;
    dryEngine(e);
    SynthParams p;
    p.sound = SynthSound::WarmPad;
    e.setSynth(0, p);

    // Slide: the second note starts before the first ends -> legato, one voice.
    e.noteOn(0, 48, 48.0f, 0.8f);
    Rendered out;
    renderInto(e, out, 0.3);
    e.noteOn(0, 55, 55.0f, 0.8f);
    e.noteOff(0, 48);
    renderInto(e, out, 0.1);
    CHECK(e.activeSynthVoices(0) == 1);

    // Separate notes: the first is released, its tail rings under the next.
    e.noteOff(0, 55);
    e.noteOn(0, 60, 60.0f, 0.8f);
    renderInto(e, out, 0.1);
    CHECK(e.activeSynthVoices(0) == 2);
    e.allNotesOff();
    renderInto(e, out, 12.0);
    CHECK(e.activeSynthVoices(0) == 0);
}

TEST("sound: the chord synth is polyphonic")
{
    SoundEngine e;
    dryEngine(e);
    SynthParams p;
    p.sound = SynthSound::EPiano;
    e.setSynth(kChordSynth, p);
    for (int n : { 48, 52, 55, 59 })
        e.noteOn(kChordSynth, n, static_cast<float>(n), 0.7f);
    Rendered out;
    renderInto(e, out, 0.2);
    CHECK(e.activeSynthVoices(kChordSynth) == 4);
    for (int n : { 48, 52, 55, 59 })
        e.noteOff(kChordSynth, n);
    renderInto(e, out, 6.0);
    CHECK(e.activeSynthVoices(kChordSynth) == 0);
    CHECK(out.finite);
}

TEST("sound: Cutoff and its modulation brighten the synth")
{
    auto brightness = [](float cutoff, float mod) {
        SoundEngine e;
        dryEngine(e);
        SynthParams p;
        p.sound = SynthSound::ReeseBass;
        p.cutoff = cutoff;
        p.cutoffMod = mod;
        p.reverbSend = 0.0f;
        e.setSynth(0, p);
        e.noteOn(0, 40, 40.0f, 0.8f);
        Rendered out;
        renderInto(e, out, 0.5);
        // Brightness proxy: how much the signal changes per sample.
        double diff = 0.0, level = 0.0;
        for (size_t i = 1; i < out.left.size(); ++i)
        {
            diff += std::abs(out.left[i] - out.left[i - 1]);
            level += std::abs(out.left[i]);
        }
        return diff / std::max(level, 1e-9);
    };
    CHECK(brightness(0.8f, 0.0f) > brightness(0.3f, 0.0f) * 1.5);
    CHECK(brightness(0.3f, 0.5f) > brightness(0.3f, 0.0f) * 1.5);
}

TEST("sound: reverb and delay at maximum stay stable and decay")
{
    SoundEngine e;
    e.prepare(kRate);
    FxParams fx;
    fx.reverbSize = 1.0f;
    fx.delayFeedback = 0.9f;
    fx.delayBeats = 0.25f;
    fx.bpm = 140.0;
    e.setFx(fx);
    KitParams kit;
    kit.kit = Kit::Trap;
    kit.reverbSend = 1.0f;
    kit.delaySend = 1.0f;
    e.setKit(kit);
    for (int v = 0; v < kNumDrumVoices; ++v)
        e.triggerDrum(v, 1.0f);
    Rendered out;
    renderInto(e, out, 40.0);
    CHECK(out.finite);
    CHECK(out.peak <= 0.81f); // soft-clipped output
    CHECK(out.rms(static_cast<size_t>(1.0 * kRate), static_cast<size_t>(2.0 * kRate)) > 1e-3f); // tails audible
    CHECK(out.rms(static_cast<size_t>(38.0 * kRate), out.left.size()) < 1e-4f);
}

TEST("sound: names cover every kit and synth sound")
{
    for (int k = 0; k < static_cast<int>(Kit::Count); ++k)
        CHECK(std::string(kitName(static_cast<Kit>(k))).size() > 1);
    for (int s = 0; s < static_cast<int>(SynthSound::Count); ++s)
    {
        CHECK(std::string(synthSoundName(static_cast<SynthSound>(s))).size() > 1);
        CHECK(std::string(synthCategoryName(static_cast<SynthSound>(s))).size() > 1);
    }
}

TEST("sound: ensemble sounds are stereo, basses stay mono")
{
    auto width = [](SynthSound sound) {
        SoundEngine e;
        dryEngine(e);
        SynthParams p;
        p.sound = sound;
        p.reverbSend = 0.0f;
        e.setSynth(kChordSynth, p); // the centre-panned synth
        e.noteOn(kChordSynth, 60, 60.0f, 0.8f);
        Rendered out;
        renderInto(e, out, 1.0);
        double diff = 0.0, sum = 0.0;
        for (size_t i = 0; i < out.left.size(); ++i)
        {
            diff += std::abs(out.left[i] - out.right[i]);
            sum += std::abs(out.left[i]) + std::abs(out.right[i]);
        }
        return diff / std::max(sum, 1e-9);
    };
    CHECK(width(SynthSound::PolyStrings) > 0.05);
    CHECK(width(SynthSound::SubBass) < 1e-6);
}

TEST("sound: a level jump glides instead of clicking")
{
    SoundEngine e;
    dryEngine(e);
    SynthParams p;
    p.sound = SynthSound::SubBass;
    p.reverbSend = 0.0f;
    p.level = 0.1f;
    e.setSynth(kChordSynth, p);
    e.noteOn(kChordSynth, 45, 45.0f, 0.9f);
    Rendered out;
    renderInto(e, out, 0.5);
    p.level = 1.0f;
    e.setSynth(kChordSynth, p);
    Rendered after;
    renderInto(e, after, 0.002); // 2 ms: still on the way up
    renderInto(e, after, 0.3);
    const size_t twoMs = static_cast<size_t>(0.002 * kRate);
    CHECK(after.rms(0, twoMs) < after.rms(static_cast<size_t>(0.2 * kRate), after.left.size()) * 0.6f);
}

TEST("sound: harder drum hits are brighter, not only louder")
{
    auto brightness = [](float velocity) {
        SoundEngine e;
        dryEngine(e);
        KitParams kit;
        kit.kit = Kit::Eight08;
        kit.reverbSend = 0.0f;
        e.setKit(kit);
        e.triggerDrum(1, velocity); // snare
        Rendered out;
        renderInto(e, out, 0.2);
        double diff = 0.0, level = 0.0;
        for (size_t i = 1; i < out.left.size(); ++i)
        {
            diff += std::abs(out.left[i] - out.left[i - 1]);
            level += std::abs(out.left[i]);
        }
        return diff / std::max(level, 1e-9);
    };
    CHECK(brightness(1.0f) > brightness(0.3f) * 1.1);
}

TEST("sound: the ladder stays stable and bounded at full resonance")
{
    for (float cutoff : { 0.0f, 0.3f, 0.7f, 1.0f })
    {
        SoundEngine e;
        dryEngine(e);
        SynthParams p;
        p.sound = SynthSound::AcidBass;
        p.resonance = 1.0f;
        p.cutoff = cutoff;
        p.envAmount = 1.0f;
        p.reverbSend = 0.0f;
        p.level = 1.0f;
        e.setSynth(0, p);
        for (int n : { 24, 36, 48, 72 })
            e.noteOn(0, n, static_cast<float>(n), 1.0f);
        Rendered out;
        renderInto(e, out, 2.0);
        CHECK(out.finite);
        CHECK(out.peak <= 0.81f);
    }
}

TEST("sound: monophonic sounds reuse their voice; poly ones let tails ring")
{
    auto voicesAfterTwoNotes = [](SynthSound sound) {
        SoundEngine e;
        dryEngine(e);
        SynthParams p;
        p.sound = sound;
        e.setSynth(0, p);
        e.noteOn(0, 48, 48.0f, 0.8f);
        Rendered out;
        renderInto(e, out, 0.2);
        e.noteOff(0, 48);
        renderInto(e, out, 0.005); // the first note is still ringing out
        e.noteOn(0, 50, 50.0f, 0.8f);
        renderInto(e, out, 0.01);
        return e.activeSynthVoices(0);
    };
    CHECK(voicesAfterTwoNotes(SynthSound::AcidBass) == 1);
    CHECK(voicesAfterTwoNotes(SynthSound::Pluck) == 2);
}

TEST("sound: hard sync is band-limited (little energy between the harmonics)")
{
    // Energy away from the harmonics of a high sync-lead note: aliasing.
    SoundEngine e;
    dryEngine(e);
    SynthParams p;
    p.sound = SynthSound::SyncLead;
    p.cutoff = 1.0f;
    p.envAmount = 0.0f;
    p.reverbSend = 0.0f;
    e.setSynth(kChordSynth, p);
    const int note = 84;
    e.noteOn(kChordSynth, note, static_cast<float>(note), 1.0f);
    Rendered out;
    renderInto(e, out, 0.6);
    const size_t n = 8192, start = static_cast<size_t>(0.3 * kRate);
    const double f0 = 440.0 * std::pow(2.0, (note - 69) / 12.0);
    double harmonic = 0.0, other = 0.0;
    for (size_t k = 8; k < n / 2; k += 3) // a DFT at every third bin is plenty
    {
        double re = 0.0, im = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * static_cast<double>(i) / static_cast<double>(n));
            const double a = 2.0 * M_PI * static_cast<double>(k * i) / static_cast<double>(n);
            re += out.left[start + i] * w * std::cos(a);
            im -= out.left[start + i] * w * std::sin(a);
        }
        const double f = static_cast<double>(k) * kRate / static_cast<double>(n);
        const double h = f / f0;
        const bool nearHarmonic = std::abs(h - std::round(h)) * f0 < std::max(3.0 * kRate / static_cast<double>(n), 0.015 * f0);
        (nearHarmonic ? harmonic : other) += re * re + im * im;
    }
    const double inharmonicDb = 10.0 * std::log10(other / (harmonic + other));
    CHECK_MSG(inharmonicDb < -25.0, "inharmonic energy " << inharmonicDb << " dB");
}

TEST("sound: non-finite settings never reach the output")
{
    SoundEngine e;
    e.prepare(kRate);
    FxParams fx;
    fx.reverbSize = std::numeric_limits<float>::quiet_NaN();
    fx.delayFeedback = std::numeric_limits<float>::infinity();
    e.setFx(fx);
    KitParams kit;
    kit.tone = std::numeric_limits<float>::quiet_NaN();
    kit.reverbSend = 1.0f;
    e.setKit(kit);
    SynthParams p;
    p.cutoff = std::numeric_limits<float>::quiet_NaN();
    p.level = std::numeric_limits<float>::infinity();
    p.reverbSend = 1.0f;
    e.setSynth(0, p);
    e.triggerDrum(0, 1.0f);
    e.noteOn(0, 48, 48.0f, 1.0f);
    Rendered out;
    renderInto(e, out, 1.0);
    CHECK(out.finite);
    CHECK(out.peak > 0.01f); // and it still plays
}
