#include "TestFramework.h"

#include "gristo/sound/Sound.h"

#include <algorithm>
#include <cmath>
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
