#include "TestFramework.h"

#include "gristo/sound/Sound.h"
#include "../sound/src/Dsp.h"
#include "../sound/src/Synth.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using namespace gristo::sound;

namespace {

constexpr double kRate = 48000.0;
constexpr double kPi = 3.14159265358979323846; // M_PI isn't standard (MSVC lacks it)

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
            const double w = 0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(i) / static_cast<double>(n));
            const double a = 2.0 * kPi * static_cast<double>(k * i) / static_cast<double>(n);
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

TEST("sound: stems take their part out of the mix")
{
    auto render = [](bool separate, std::vector<float>& mix, std::vector<float>& kit, std::vector<float>& synth) {
        SoundEngine e;
        dryEngine(e);
        SynthParams p;
        p.sound = SynthSound::SubBass;
        p.reverbSend = 0.0f;
        e.setSynth(0, p);
        KitParams k;
        k.reverbSend = 0.0f;
        e.setKit(k);
        e.triggerDrum(0, 1.0f);
        e.noteOn(0, 40, 40.0f, 0.9f);
        const int n = 4800;
        std::vector<float> r(n), kr(n), sr(n);
        mix.assign(n, 0.0f);
        kit.assign(n, 0.0f);
        synth.assign(n, 0.0f);
        Stems stems;
        stems.kit[0] = kit.data();
        stems.kit[1] = kr.data();
        stems.synth[0][0] = synth.data();
        stems.synth[0][1] = sr.data();
        e.render(mix.data(), r.data(), n, 1.0f, separate ? &stems : nullptr);
    };
    auto peak = [](const std::vector<float>& x) {
        float m = 0.0f;
        for (float v : x)
            m = std::max(m, std::abs(v));
        return m;
    };
    std::vector<float> mix, kit, synth;
    render(false, mix, kit, synth);
    CHECK(peak(mix) > 0.1f && peak(kit) == 0.0f && peak(synth) == 0.0f);
    render(true, mix, kit, synth);
    CHECK(peak(mix) < 1.0e-4f); // nothing else is playing
    CHECK(peak(kit) > 0.1f && peak(synth) > 0.1f);
}

TEST("sound: Duck makes the kick push a synth down, and it comes back")
{
    auto levels = [](float duck) {
        SoundEngine e;
        dryEngine(e);
        SynthParams p;
        p.sound = SynthSound::Organ;
        p.reverbSend = 0.0f;
        p.duck = duck;
        e.setSynth(kChordSynth, p);
        KitParams kit;
        kit.level[0] = 0.0f; // the kick itself is silent here: only its ducking counts
        kit.reverbSend = 0.0f;
        e.setKit(kit);
        e.noteOn(kChordSynth, 60, 60.0f, 0.8f);
        Rendered out;
        renderInto(e, out, 0.5);
        e.triggerDrum(0, 1.0f);
        renderInto(e, out, 1.5);
        const auto at = [&](double from, double to) { return out.rms(static_cast<size_t>(from * kRate), static_cast<size_t>(to * kRate)); };
        return std::array<float, 3> { at(0.3, 0.5), at(0.51, 0.56), at(1.7, 2.0) };
    };
    const auto off = levels(0.0f), on = levels(1.0f);
    CHECK(off[1] > off[0] * 0.8f);  // no ducking: steady
    CHECK(on[1] < on[0] * 0.35f);   // pushed down right after the kick
    CHECK(on[2] > on[0] * 0.85f);   // and back up
}

TEST("sound: a drum voice can play a user sample instead of its synth")
{
    SampleData sample;
    sample.sampleRate = 24000.0; // half the engine's rate: played at the right speed
    const int n = 2400;          // 0.1 s
    for (int i = 0; i < n; ++i)
    {
        const float s = 0.6f * std::sin(6.2831853f * 200.0f * static_cast<float>(i) / 24000.0f);
        sample.left.push_back(s);
        sample.right.push_back(s);
    }
    SoundEngine e;
    dryEngine(e);
    KitParams kit;
    kit.reverbSend = 0.0f;
    e.setKit(kit);
    e.setDrumSample(1, &sample);
    e.triggerDrum(1, 1.0f);
    Rendered out;
    renderInto(e, out, 0.3);
    CHECK(out.finite && out.peak > 0.1f);
    // 200 Hz for 0.1 s: about 40 zero crossings, then silence.
    const int crossings = zeroCrossings(out.left, 0, static_cast<size_t>(0.1 * kRate));
    CHECK_MSG(crossings >= 36 && crossings <= 44, crossings << " crossings");
    CHECK(out.rms(static_cast<size_t>(0.15 * kRate), out.left.size()) < 2.0e-3f);
    CHECK(!e.drumActive(1));
    // Other voices keep their synth; clearing brings this one's back.
    e.setDrumSample(1, nullptr);
    e.triggerDrum(1, 1.0f);
    Rendered synth;
    renderInto(e, synth, 0.05);
    CHECK(synth.peak > 0.05f && e.drumActive(1));
}

namespace {

// A bar of kick, snare, hats and a held chord, rendered with one character setting.
Rendered characterRun(float dirt, Grit grit, float alive, double tail = 0.0)
{
    SoundEngine e;
    dryEngine(e);
    CharacterParams c;
    c.dirt = dirt;
    c.grit = grit;
    c.alive = alive;
    e.setCharacter(c);
    SynthParams pad;
    pad.sound = SynthSound::PolySynth;
    e.setSynth(kChordSynth, pad);
    Rendered out;
    for (int note : { 48, 55, 60, 64 })
        e.noteOn(kChordSynth, note, static_cast<float>(note), 0.8f);
    for (int step = 0; step < 16; ++step)
    {
        if (step % 4 == 0)
            e.triggerDrum(0, 1.0f);
        if (step % 8 == 4)
            e.triggerDrum(1, 0.9f);
        e.triggerDrum(2, step % 2 == 0 ? 0.8f : 0.4f);
        renderInto(e, out, 0.125);
    }
    for (int note : { 48, 55, 60, 64 })
        e.noteOff(kChordSynth, note);
    if (tail > 0.0)
        renderInto(e, out, tail);
    return out;
}

} // namespace

TEST("sound: every kind of dirt stays finite, bounded and about as loud as clean")
{
    const Rendered clean = characterRun(0.0f, Grit::Tape, 0.5f);
    const float reference = clean.rms(0, clean.left.size());
    CHECK(reference > 0.02f);
    for (int g = 0; g < static_cast<int>(Grit::Count); ++g)
        for (float dirt : { 0.25f, 0.6f, 1.0f })
        {
            const Rendered out = characterRun(dirt, static_cast<Grit>(g), 0.5f);
            const float rms = out.rms(0, out.left.size());
            CHECK(out.finite);
            CHECK(out.peak <= 1.0f);
            CHECK_MSG(rms > reference * 0.5f && rms < reference * 2.2f,
                      gritName(static_cast<Grit>(g)) << " at " << dirt << ": rms " << rms << " against clean " << reference);
            // It is audibly a different sound, not a level change.
            double diff = 0.0;
            for (size_t i = 0; i < out.left.size(); ++i)
                diff += std::abs(out.left[i] - clean.left[i]);
            CHECK(diff / static_cast<double>(out.left.size()) > 0.002);
        }
}

TEST("sound: tape hiss fades out with the music, silence stays silent")
{
    const Rendered out = characterRun(1.0f, Grit::Tape, 0.5f, 12.0);
    const size_t n = out.left.size();
    CHECK(out.rms(n - 24000, n) < 2e-5f);
    // With nothing played there is no hiss at all.
    SoundEngine e;
    dryEngine(e);
    CharacterParams c;
    c.dirt = 1.0f;
    e.setCharacter(c);
    Rendered silent;
    renderInto(e, silent, 1.0);
    CHECK(silent.peak == 0.0f);
}

TEST("sound: Crunch folds highs back as grain; Alive makes two hits differ")
{
    // A closed hat alone: with Crunch, energy appears below the hat's own band.
    auto lowShare = [](float dirt) {
        SoundEngine e;
        dryEngine(e);
        CharacterParams c;
        c.dirt = dirt;
        c.grit = Grit::Crunch;
        e.setCharacter(c);
        KitParams kit;
        kit.kit = Kit::Classic; // the 909-style hat: all its energy is high
        kit.reverbSend = 0.0f;
        e.setKit(kit);
        e.triggerDrum(2, 1.0f);
        Rendered out;
        renderInto(e, out, 0.2);
        double low = 0.0, all = 0.0, z = 0.0;
        for (float x : out.left) // one-pole lowpass at about 1.5 kHz
        {
            z += 0.18 * (x - z);
            low += z * z;
            all += static_cast<double>(x) * x;
        }
        return low / std::max(all, 1e-12);
    };
    const double crushed = lowShare(1.0f), clean = lowShare(0.0f);
    CHECK_MSG(crushed > clean * 1.5, crushed << " against " << clean);

    auto difference = [](float alive) {
        SoundEngine e;
        dryEngine(e);
        CharacterParams c;
        c.dirt = 0.0f;
        c.alive = alive;
        e.setCharacter(c);
        KitParams kit;
        kit.reverbSend = 0.0f;
        e.setKit(kit);
        double total = 0.0;
        Rendered first;
        e.triggerDrum(0, 0.9f);
        renderInto(e, first, 1.0);
        for (int hit = 0; hit < 6; ++hit)
        {
            Rendered next;
            e.triggerDrum(0, 0.9f);
            renderInto(e, next, 1.0);
            for (size_t i = 0; i < 9600; ++i)
                total += std::abs(next.left[i] - first.left[i]);
        }
        return total;
    };
    CHECK(difference(1.0f) > difference(0.0f) * 1.5);
}

TEST("sound: the plucked strings are in tune; shimmer adds an octave to the tail and dies away")
{
    for (SynthSound sound : { SynthSound::NylonGuitar, SynthSound::Koto })
        for (float note : { 45.0f, 57.0f, 69.0f, 81.0f })
        {
            SoundEngine e;
            dryEngine(e);
            CharacterParams c;
            c.dirt = 0.0f;
            c.alive = 0.0f;
            e.setCharacter(c);
            SynthParams p;
            p.sound = sound;
            p.reverbSend = 0.0f;
            e.setSynth(kChordSynth, p);
            e.noteOn(kChordSynth, 60, note, 0.9f);
            Rendered out;
            renderInto(e, out, 0.5);
            CHECK(out.finite && out.peak > 0.02f);
            // Autocorrelation peak near the expected period.
            const double period = kRate / (440.0 * std::pow(2.0, (note - 69.0) / 12.0));
            double best = -1.0;
            int bestLag = 0;
            for (int lag = static_cast<int>(period * 0.9); lag <= static_cast<int>(period * 1.1) + 1; ++lag)
            {
                double sum = 0.0;
                for (size_t i = 4800; i < 14400; ++i)
                    sum += static_cast<double>(out.left[i]) * out.left[i + static_cast<size_t>(lag)];
                if (sum > best) { best = sum; bestLag = lag; }
            }
            CHECK_MSG(std::abs(bestLag - period) <= std::max(1.0, period * 0.012), "note " << note << ": period " << bestLag << " for " << period);
        }

    auto tail = [](float shimmer) {
        SoundEngine e;
        e.prepare(kRate);
        FxParams fx;
        fx.reverbSize = 0.8f;
        fx.shimmer = shimmer;
        e.setFx(fx);
        SynthParams p;
        p.sound = SynthSound::SingingBowl;
        p.reverbSend = 0.8f;
        e.setSynth(kChordSynth, p);
        e.noteOn(kChordSynth, 60, 60.0f, 0.9f);
        Rendered out;
        renderInto(e, out, 0.3);
        e.noteOff(kChordSynth, 60);
        renderInto(e, out, 40.0);
        return out;
    };
    const Rendered plain = tail(0.0f), lit = tail(1.0f);
    CHECK(lit.finite && lit.peak <= 1.0f);
    const size_t a = static_cast<size_t>(3.0 * kRate), b = static_cast<size_t>(6.0 * kRate);
    CHECK_MSG(lit.rms(a, b) > plain.rms(a, b) * 1.15f, "tail " << lit.rms(a, b) << " against " << plain.rms(a, b)); // the tail is fed again
    CHECK(lit.rms(lit.left.size() - 48000, lit.left.size()) < 1e-3f);             // and it ends
}

TEST("sound: a drum's own settings reshape it; Body rings at its tuned pitch and dies away")
{
    auto hit = [](const DrumTweak& tweak, int voice, double seconds = 1.0) {
        SoundEngine e;
        dryEngine(e);
        CharacterParams c;
        c.dirt = 0.0f;
        c.alive = 0.0f;
        e.setCharacter(c);
        KitParams kit;
        kit.reverbSend = 0.0f;
        kit.voice[static_cast<size_t>(voice)] = tweak;
        e.setKit(kit);
        e.triggerDrum(voice, 0.9f);
        Rendered out;
        renderInto(e, out, seconds);
        return out;
    };
    const Rendered plain = hit({}, 1);
    DrumTweak t;
    t.decay = 1.0f;
    const Rendered longer = hit(t, 1);
    CHECK(longer.rms(9600, 24000) > plain.rms(9600, 24000) * 2.0f);
    t = {};
    t.tune = 12.0f;
    CHECK(zeroCrossings(hit(t, 0).left, 2400, 9600) > zeroCrossings(hit({}, 0).left, 2400, 9600) * 1.5);
    t = {};
    t.pan = 1.0f;
    const Rendered right = hit(t, 0);
    double l = 0.0, r = 0.0;
    for (size_t i = 0; i < right.left.size(); ++i)
    {
        l += std::abs(right.left[i]);
        r += std::abs(right.right[i]);
    }
    CHECK(r > l * 5.0);

    // Body on the closed hat (a noise drum): the tail now has a pitch, 620 Hz by default;
    // Tube (a harmonic series) rings right at it.
    t = {};
    t.body = 1.0f;
    t.bodyDecay = 0.8f;
    t.bodyType = 3;
    const Rendered body = hit(t, 2, 6.0);
    CHECK(body.finite && body.peak <= 1.0f);
    CHECK(body.rms(14400, 24000) > hit({}, 2).rms(14400, 24000) * 5.0f + 1e-5f);
    const double period = kRate / 620.0;
    double best = -1.0;
    int bestLag = 0;
    for (int lag = 60; lag <= 100; ++lag)
    {
        double sum = 0.0;
        for (size_t i = 9600; i < 19200; ++i)
            sum += static_cast<double>(body.left[i]) * body.left[i + static_cast<size_t>(lag)];
        if (sum > best) { best = sum; bestLag = lag; }
    }
    CHECK_MSG(std::abs(bestLag - period) <= 2.0, "period " << bestLag << " for " << period);
    CHECK(body.rms(body.left.size() - 24000, body.left.size()) < 1e-4f);
    // The four types are four different bodies, and all of them die away.
    std::vector<Rendered> types;
    for (int type = 0; type < 4; ++type)
    {
        t.bodyType = type;
        types.push_back(hit(t, 1, 6.0));
        const Rendered& b = types.back();
        CHECK(b.finite && b.peak <= 1.0f);
        CHECK(b.rms(b.left.size() - 24000, b.left.size()) < 1e-4f);
    }
    for (int a = 0; a < 4; ++a)
        for (int b = a + 1; b < 4; ++b)
        {
            double diff = 0.0, size = 0.0;
            for (size_t i = 2400; i < 24000; ++i)
            {
                diff += std::abs(types[static_cast<size_t>(a)].left[i] - types[static_cast<size_t>(b)].left[i]);
                size += std::abs(types[static_cast<size_t>(a)].left[i]);
            }
            CHECK_MSG(diff > 0.2 * size, "body types " << a << " and " << b << " sound alike");
        }

    // Every extreme of every drum of every kit stays finite and bounded.
    for (int k = 0; k < static_cast<int>(Kit::Count); ++k)
        for (int extreme = 0; extreme < 2; ++extreme)
        {
            SoundEngine e;
            dryEngine(e);
            KitParams kit;
            kit.kit = static_cast<Kit>(k);
            for (auto& v : kit.voice)
            {
                v.tune = extreme ? 24.0f : -24.0f;
                v.decay = v.tone = v.punch = v.noise = v.drive = v.body = v.bodyDecay = extreme ? 1.0f : 0.0f;
                v.body = 1.0f;
                v.bodyTune = extreme ? 24.0f : -24.0f;
            }
            e.setKit(kit);
            Rendered out;
            for (int round = 0; round < 4; ++round)
            {
                for (int v = 0; v < kNumDrumVoices; ++v)
                    e.triggerDrum(v, 1.0f);
                renderInto(e, out, 0.25);
            }
            CHECK(out.finite && out.peak <= 1.0f);
        }

    // Synth body: adds something, stays finite.
    SoundEngine e;
    dryEngine(e);
    SynthParams p;
    p.sound = SynthSound::NylonGuitar;
    p.body = 1.0f;
    e.setSynth(kChordSynth, p);
    e.noteOn(kChordSynth, 60, 60.0f, 0.9f);
    Rendered out;
    renderInto(e, out, 1.0);
    CHECK(out.finite && out.peak > 0.02f && out.peak <= 1.0f);
}

TEST("sound: the modelled drums: pitch sags after the hit, wires ring only while the head moves")
{
    auto hit = [](int voice, float velocity) {
        SoundEngine e;
        dryEngine(e);
        CharacterParams c;
        c.dirt = 0.0f;
        c.alive = 0.0f;
        e.setCharacter(c);
        KitParams kit;
        kit.kit = Kit::Acoustic;
        kit.reverbSend = 0.0f;
        e.setKit(kit);
        e.triggerDrum(voice, velocity);
        Rendered out;
        renderInto(e, out, 3.0);
        return out;
    };
    // Floor tom: more cycles per second right after the hit than later on.
    const Rendered tom = hit(5, 1.0f);
    CHECK(tom.finite && tom.peak > 0.05f && tom.peak <= 1.0f);
    const int early = zeroCrossings(tom.left, 480, 5280), late = zeroCrossings(tom.left, 14400, 19200);
    CHECK_MSG(early > late + 2, "crossings " << early << " then " << late);
    // Snare: bright wire noise well after the stick, gone when the head has stopped.
    const Rendered snare = hit(1, 1.0f);
    double high = 0.0, z = 0.0;
    for (size_t i = 2400; i < 7200; ++i) // 50-150 ms: one-pole highpass at about 3 kHz
    {
        z += 0.33 * (snare.left[i] - z);
        high += (snare.left[i] - z) * (snare.left[i] - z);
    }
    CHECK(high / 4800.0 > 1e-6);
    CHECK(snare.rms(snare.left.size() - 24000, snare.left.size()) < 1e-4f);
    // A soft hit is duller, not just quieter: fewer crossings in the first 20 ms.
    CHECK(zeroCrossings(hit(1, 0.25f).left, 0, 960) < zeroCrossings(snare.left, 0, 960));
}

TEST("sound: modelled cymbals ring and choke; the phase sounds get brighter as Cutoff bends them")
{
    auto hat = [](int voice, bool choke) {
        SoundEngine e;
        dryEngine(e);
        KitParams kit;
        kit.kit = Kit::Acoustic;
        kit.reverbSend = 0.0f;
        e.setKit(kit);
        e.triggerDrum(voice, 1.0f);
        Rendered out;
        renderInto(e, out, 0.1);
        if (choke)
            e.triggerDrum(2, 0.5f);
        renderInto(e, out, 5.9);
        return out;
    };
    const Rendered closed = hat(2, false), open = hat(3, false), choked = hat(3, true);
    for (const Rendered* r : { &closed, &open, &choked })
        CHECK(r->finite && r->peak > 0.03f && r->peak <= 1.0f);
    CHECK(open.rms(14400, 28800) > closed.rms(14400, 28800) * 4.0f);   // the open hat rings on
    CHECK(choked.rms(14400, 28800) < open.rms(14400, 28800) * 0.5f);   // unless the closed one stops it
    CHECK(open.rms(open.left.size() - 24000, open.left.size()) < 1e-4f);

    for (SynthSound sound : { SynthSound::PhaseBass, SynthSound::ResoLead, SynthSound::PhasePad, SynthSound::PhaseKeys, SynthSound::FoldDrone })
    {
        auto crossings = [sound](float cutoff) {
            SoundEngine e;
            dryEngine(e);
            CharacterParams c;
            c.dirt = 0.0f;
            c.alive = 0.0f;
            e.setCharacter(c);
            SynthParams p;
            p.sound = sound;
            p.cutoff = cutoff;
            p.envAmount = 0.0f;
            p.reverbSend = 0.0f;
            e.setSynth(0, p);
            e.noteOn(0, 48, 48.0f, 0.9f);
            Rendered out;
            renderInto(e, out, 3.0);
            CHECK(out.finite && out.peak > 0.01f && out.peak <= 1.0f);
            // Share of the energy above about 500 Hz (two one-pole high-passes; the note is 130 Hz).
            double high = 0.0, all = 1e-12, z1 = 0.0, z2 = 0.0, h1 = 0.0;
            const double a = 1.0 - std::exp(-2.0 * kPi * 500.0 / kRate);
            for (size_t i = 96000; i < 144000; ++i)
            {
                const double x = out.left[i];
                z1 += a * (x - z1);
                h1 = x - z1;
                z2 += a * (h1 - z2);
                const double h2 = h1 - z2;
                high += h2 * h2;
                all += x * x;
            }
            return high / all;
        };
        const double bright = crossings(1.0f), dark = crossings(0.0f);
        CHECK_MSG(bright > dark, synthSoundName(sound) << ": " << bright << " against " << dark);
    }
}

TEST("sound: every TRIO, LOFI and FM6 sound plays in tune, finite and bounded; FM Cutoff opens the modulators")
{
    for (SynthSound sound : { SynthSound::TrioBass, SynthSound::TrioSync, SynthSound::RingBell, SynthSound::ChipChoir, SynthSound::TrioArp,
                              SynthSound::ChipLead, SynthSound::ChipBass, SynthSound::ChipArp, SynthSound::WaveVox, SynthSound::FmRhodes,
                              SynthSound::DxBass, SynthSound::FmBells, SynthSound::FmMarimba, SynthSound::FmBrass, SynthSound::FmGlass })
        for (float note : { 36.0f, 60.0f, 84.0f })
        {
            SoundEngine e;
            dryEngine(e);
            SynthParams p;
            p.sound = sound;
            p.reverbSend = 0.0f;
            e.setSynth(0, p);
            e.noteOn(0, 60, note, 1.0f);
            Rendered out;
            renderInto(e, out, 1.0);
            e.noteOff(0, 60);
            renderInto(e, out, 6.0);
            CHECK_MSG(out.finite && out.peak > 0.01f && out.peak <= 1.0f, synthSoundName(sound) << " at " << note << ": peak " << out.peak);
            CHECK_MSG(out.rms(out.left.size() - 24000, out.left.size()) < 1e-3f, synthSoundName(sound) << " does not stop");
        }
    auto highs = [](float cutoff) {
        SoundEngine e;
        dryEngine(e);
        SynthParams p;
        p.sound = SynthSound::FmRhodes;
        p.cutoff = cutoff;
        p.reverbSend = 0.0f;
        e.setSynth(0, p);
        e.noteOn(0, 60, 48.0f, 0.9f);
        Rendered out;
        renderInto(e, out, 0.3);
        double high = 0.0, all = 1e-12, z = 0.0;
        for (size_t i = 2400; i < out.left.size(); ++i)
        {
            z += 0.06 * (out.left[i] - z);
            high += (out.left[i] - z) * (out.left[i] - z);
            all += static_cast<double>(out.left[i]) * out.left[i];
        }
        return high / all;
    };
    CHECK(highs(1.0f) > highs(0.0f) * 1.5);
}

TEST("sound: Beads grains the synths, gated by the drums it listens to, divided or by chance")
{
    auto run = [](BeadsParams b, int hits, double gap) {
        SoundEngine e;
        dryEngine(e);
        SynthParams p;
        p.sound = SynthSound::WarmPad;
        p.reverbSend = 0.0f;
        e.setSynth(kChordSynth, p);
        b.level = 1.0f;
        b.reverb = 0.0f;
        b.send[kChordSynth] = 1.0f;
        e.setBeads(b);
        for (int note : { 48, 55, 60 })
            e.noteOn(kChordSynth, note, static_cast<float>(note), 0.8f);
        Rendered out;
        renderInto(e, out, 1.0); // fill the buffer
        int grains = 0;
        for (int h = 0; h < hits; ++h)
        {
            const int before = e.beadsGrains();
            e.triggerDrum(1, 0.8f);
            renderInto(e, out, 0.003);
            grains += e.beadsGrains() > before ? 1 : 0;
            renderInto(e, out, gap - 0.003);
        }
        return std::make_pair(grains, out);
    };
    BeadsParams b;
    b.size = 0.55f;
    b.triggers[1] = true; // the snare gates
    b.density = 0.0f;     // every hit
    auto [every, sound] = run(b, 16, 0.12);
    CHECK(every == 16);
    CHECK(sound.finite && sound.peak > 0.01f && sound.peak <= 1.0f);
    b.density = 0.23f; // 0.23 * 2.15 * 8 = 3.9: every 4th
    CHECK(run(b, 16, 0.12).first == 4);
    b.density = 0.5f; // noon: none
    CHECK(run(b, 16, 0.12).first == 0);
    b.density = 0.75f; // by chance, about a quarter
    const int some = run(b, 64, 0.03).first;
    CHECK(some > 4 && some < 32);
    // Not listening to the snare: its hits do nothing; the free clock left of noon makes grains anyway.
    b.triggers[1] = false;
    b.triggers[0] = true;
    b.density = 0.0f;
    CHECK(run(b, 8, 0.12).first == 0);
    BeadsParams free;
    free.density = 0.3f; // a steady clock, ~3.4 Hz
    for (int q = 0; q < 4; ++q)
    {
        free.quality = q;
        free.feedback = 0.8f;
        free.pitchRandom = 1.0f;
        free.timeRandom = 0.5f;
        free.size = q == 1 ? 0.2f : 0.6f; // backwards grains too
        const auto r = run(free, 1, 3.0).second;
        CHECK(r.finite && r.peak <= 1.0f);
        CHECK(r.rms(48000, r.left.size()) > 1e-3f);
    }
}

TEST("sound: fuzz: random sounds, settings and Beads, changed while playing, stay finite and in bounds")
{
    dsp::Noise rng(0xf022u);
    auto u = [&rng] { return 0.5f + 0.5f * rng.next(); };
    for (int round = 0; round < 6; ++round)
    {
        SoundEngine e;
        e.prepare(round % 2 ? 44100.0 : 96000.0);
        std::vector<float> l(256), r(256);
        for (int block = 0; block < 400; ++block)
        {
            if (block % 7 == 0)
            {
                for (int s = 0; s < kNumSynths; ++s)
                {
                    SynthParams p;
                    p.sound = static_cast<SynthSound>(static_cast<int>(u() * static_cast<float>(SynthSound::Count)) % static_cast<int>(SynthSound::Count));
                    p.cutoff = u(); p.resonance = u(); p.envAmount = u(); p.attack = u(); p.release = u(); p.glide = u();
                    p.level = u(); p.reverbSend = u(); p.delaySend = u(); p.duck = u(); p.body = u(); p.cutoffMod = 2.0f * u() - 1.0f;
                    e.setSynth(s, p);
                }
                KitParams k;
                k.kit = static_cast<Kit>(static_cast<int>(u() * static_cast<float>(Kit::Count)) % static_cast<int>(Kit::Count));
                for (auto& t : k.voice)
                {
                    t.tune = 48.0f * u() - 24.0f; t.decay = u(); t.tone = u(); t.punch = u(); t.noise = u(); t.drive = u();
                    t.pan = 2.0f * u() - 1.0f; t.body = u(); t.bodyTune = 48.0f * u() - 24.0f; t.bodyDecay = u();
                }
                e.setKit(k);
                BeadsParams b;
                b.density = u(); b.time = u(); b.size = u(); b.pitch = 48.0f * u() - 24.0f; b.shape = u();
                b.timeRandom = u(); b.sizeRandom = u(); b.pitchRandom = u(); b.shapeRandom = u(); b.feedback = u();
                b.level = u(); b.reverb = u(); b.quality = static_cast<int>(u() * 4.0f) % 4; b.freeze = u() > 0.7f; b.musicalPitch = u() > 0.5f;
                for (auto& s : b.send) s = u();
                for (auto& t : b.triggers) t = u() > 0.5f;
                e.setBeads(b);
                FxParams fx;
                fx.reverbSize = u(); fx.delayFeedback = 0.9f * u(); fx.shimmer = u(); fx.bpm = 40.0 + 200.0 * u();
                e.setFx(fx);
                CharacterParams c;
                c.dirt = u(); c.alive = u(); c.grit = static_cast<Grit>(static_cast<int>(u() * 3.0f) % 3);
                e.setCharacter(c);
            }
            for (int n = 0; n < 3; ++n)
            {
                const int synth = static_cast<int>(u() * 4.0f) % 4, note = 24 + static_cast<int>(u() * 80.0f);
                if (u() > 0.5f) e.noteOn(synth, note, static_cast<float>(note), u()); else e.noteOff(synth, note);
            }
            if (u() > 0.5f)
                e.triggerDrum(static_cast<int>(u() * 6.0f) % 6, u());
            std::fill(l.begin(), l.end(), 0.0f);
            std::fill(r.begin(), r.end(), 0.0f);
            e.render(l.data(), r.data(), 256, 1.0f);
            for (int i = 0; i < 256; ++i)
                CHECK(std::isfinite(l[static_cast<size_t>(i)]) && std::abs(l[static_cast<size_t>(i)]) <= 1.0f);
        }
    }
}

TEST("sound: the Beads reverb gives a long, vast tail that still ends, as the main reverb and on Beads, on every medium")
{
    auto tail = [](int quality) {
        SoundEngine e;
        e.prepare(kRate);
        FxParams fx;
        fx.reverbSize = 1.0f;
        e.setFx(fx);
        BeadsParams media; // the main reverb follows Beads' Quality, even with Beads silent
        media.quality = quality;
        e.setBeads(media);
        CharacterParams c;
        c.dirt = 0.0f;
        e.setCharacter(c);
        KitParams kit;
        kit.reverbSend = 1.0f;
        e.setKit(kit);
        e.triggerDrum(1, 1.0f);
        Rendered out;
        renderInto(e, out, 40.0);
        return out;
    };
    const Rendered vast = tail(0);
    CHECK(vast.finite && vast.peak <= 1.0f);
    const size_t at3 = static_cast<size_t>(3.0 * kRate), at4 = static_cast<size_t>(4.0 * kRate);
    CHECK(vast.rms(at3, at4) > 1e-4f);
    CHECK(vast.rms(vast.left.size() - 48000, vast.left.size()) < 1e-4f);
    // It is a stereo reverb: the two sides differ.
    double diff = 0.0;
    for (size_t i = at3; i < at4; ++i)
        diff += std::abs(vast.left[i] - vast.right[i]);
    CHECK(diff > 1.0);
    // The other media: darker and longer, and Clean tape (the module at 32 kHz) longer still.
    std::array<double, 4> late {};
    for (int q = 0; q < 4; ++q)
    {
        const Rendered r = q == 0 ? vast : tail(q);
        CHECK(r.finite && r.peak <= 1.0f);
        CHECK(r.rms(r.left.size() - 48000, r.left.size()) < 1e-4f);
        late[static_cast<size_t>(q)] = r.rms(static_cast<size_t>(6.0 * kRate), static_cast<size_t>(7.0 * kRate));
    }
    CHECK(late[1] > late[0]);
    CHECK(late[2] > late[1]);

    // On Beads: full Reverb keeps the grains ringing after they stop.
    SoundEngine e;
    dryEngine(e);
    SynthParams p;
    p.sound = SynthSound::WarmPad;
    p.reverbSend = 0.0f;
    e.setSynth(kChordSynth, p);
    BeadsParams b;
    b.level = 1.0f;
    b.reverb = 1.0f;
    b.density = 0.3f;
    b.time = 0.05f; // grains of what just played
    b.send[kChordSynth] = 1.0f;
    e.setBeads(b);
    e.noteOn(kChordSynth, 60, 60.0f, 0.8f);
    Rendered out;
    renderInto(e, out, 2.0);
    e.noteOff(kChordSynth, 60);
    b.density = 0.5f; // no new grains
    e.setBeads(b);
    renderInto(e, out, 8.0);
    CHECK(out.finite && out.peak <= 1.0f);
    CHECK(out.rms(static_cast<size_t>(5.0 * kRate), static_cast<size_t>(6.0 * kRate)) > 1e-4f);
}

namespace {
// Fundamental by autocorrelation between 60 and 1000 Hz.
float pitchOf(const std::vector<float>& x, double rate)
{
    const int lo = static_cast<int>(rate / 1000.0), hi = static_cast<int>(rate / 60.0);
    double best = -1.0;
    int lag = lo;
    const size_t n = std::min<size_t>(x.size() - static_cast<size_t>(hi), static_cast<size_t>(rate * 0.2));
    std::vector<double> r(static_cast<size_t>(hi + 1), 0.0);
    for (int k = lo; k <= hi; ++k)
        for (size_t i = 0; i < n; ++i)
            r[static_cast<size_t>(k)] += static_cast<double>(x[i]) * x[i + static_cast<size_t>(k)];
    double top = 0.0;
    for (int k = lo; k <= hi; ++k)
        top = std::max(top, r[static_cast<size_t>(k)]);
    for (int k = lo; k <= hi; ++k) // the first lag close to the best one (not a multiple of the period)
        if (r[static_cast<size_t>(k)] > 0.9 * top && r[static_cast<size_t>(k)] > best)
        {
            best = r[static_cast<size_t>(k)];
            lag = k;
            while (k + 1 <= hi && r[static_cast<size_t>(k + 1)] > best)
                best = r[static_cast<size_t>(++k)], lag = k;
            break;
        }
    return static_cast<float>(rate / lag);
}
} // namespace

TEST("sound: Rings rings in tune at any host rate, on every model, strummed by its part's notes")
{
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
        for (int model = 0; model < 6; ++model)
        {
            SoundEngine e;
            e.prepare(rate);
            CharacterParams c;
            c.dirt = 0.0f;
            c.alive = 0.0f;
            e.setCharacter(c);
            SynthParams quiet;
            quiet.level = 0.0f;
            quiet.reverbSend = quiet.delaySend = 0.0f;
            e.setSynth(1, quiet);
            RingsParams r;
            r.on = true;
            r.source = 1;
            r.model = model;
            r.polyphony = 1;
            r.level = 1.0f;
            r.reverb = 0.0f;
            r.structure = model == 0 ? 0.25f : 0.25f; // harmonic partials
            r.brightness = 0.5f;
            r.damping = 0.7f;
            r.position = 0.3f;
            e.setRings(r);
            e.noteOn(1, 57, 57.0f, 0.8f); // A3: 220 Hz
            const auto n = static_cast<size_t>(rate * 1.0);
            std::vector<float> l(n, 0.0f), rr(n, 0.0f);
            for (size_t pos = 0; pos < n; pos += 128)
                e.render(l.data() + pos, rr.data() + pos, static_cast<int>(std::min<size_t>(128, n - pos)), 1.0f);
            float peak = 0.0f;
            bool finite = true;
            for (size_t i = 0; i < n; ++i)
            {
                finite = finite && std::isfinite(l[i]) && std::isfinite(rr[i]);
                peak = std::max(peak, std::max(std::abs(l[i]), std::abs(rr[i])));
            }
            CHECK_MSG(finite && peak > 0.01f && peak <= 1.0f, "rate " << rate << " model " << model << " peak " << peak);
            if (model == 2 || model == 0)
            {
                std::vector<float> mono(n);
                for (size_t i = 0; i < n; ++i)
                    mono[i] = l[i] + rr[i];
                const std::vector<float> part(mono.begin() + static_cast<long>(rate * 0.05), mono.end());
                const float hz = pitchOf(part, rate);
                CHECK_MSG(std::abs(12.0f * std::log2(hz / 220.0f)) < 0.3f, "rate " << rate << " model " << model << " pitch " << hz);
            }
        }
    // Off: silent and cheap.
    SoundEngine e;
    e.prepare(kRate);
    dryEngine(e);
    RingsParams r;
    r.level = 1.0f;
    e.setRings(r);
    e.noteOn(0, 57, 57.0f, 0.8f);
    e.noteOff(0, 57);
    Rendered out;
    SynthParams quiet;
    quiet.level = 0.0f;
    e.setSynth(0, quiet);
    renderInto(e, out, 0.5);
    CHECK(out.peak < 1e-6f);
}

TEST("sound: a muted part is silent, but a pre-fader Beads send keeps feeding Beads")
{
    auto run = [](bool mute, bool pre, bool beads) {
        SoundEngine e;
        dryEngine(e);
        CharacterParams c;
        c.dirt = 0.0f;
        e.setCharacter(c);
        SynthParams p;
        p.sound = SynthSound::WarmPad;
        p.reverbSend = p.delaySend = 0.0f;
        e.setSynth(kChordSynth, p);
        BeadsParams b;
        b.level = beads ? 1.0f : 0.0f;
        b.reverb = 0.0f;
        b.density = 0.35f;
        b.time = 0.05f;
        b.send[kChordSynth] = 1.0f;
        e.setBeads(b);
        MixParams m;
        m.synthMute[kChordSynth] = mute;
        m.beadsPre[kChordSynth] = pre;
        e.setMix(m);
        e.noteOn(kChordSynth, 60, 60.0f, 0.8f);
        Rendered out;
        renderInto(e, out, 2.0);
        return out.rms(48000, 96000);
    };
    const float open = run(false, false, false);
    CHECK(open > 1e-3f);
    CHECK(run(true, false, false) < open * 0.01f);  // muted: gone
    CHECK(run(true, false, true) < open * 0.01f);   // post-fader: Beads hears nothing either
    CHECK(run(true, true, true) > open * 0.05f);    // pre-fader: the grains still come
}

TEST("sound: the delay's Tape, BBD and Clean are three echoes; all die away; returns can be muted and routed")
{
    auto echo = [](int type, float age, bool mute = false, float toReverb = 0.0f) {
        SoundEngine e;
        dryEngine(e);
        FxParams fx;
        fx.delayType = type;
        fx.delayAge = age;
        fx.delayFeedback = 0.8f;
        fx.delayBeats = 0.5;
        e.setFx(fx);
        CharacterParams c;
        c.dirt = 0.0f;
        e.setCharacter(c);
        KitParams kit;
        kit.reverbSend = 0.0f;
        kit.delaySend = 1.0f;
        e.setKit(kit);
        MixParams m;
        m.delayMute = mute;
        m.delayToReverb = toReverb;
        e.setMix(m);
        e.triggerDrum(1, 1.0f);
        Rendered out;
        renderInto(e, out, 12.0);
        return out;
    };
    std::vector<Rendered> types { echo(0, 0.5f), echo(1, 0.5f), echo(2, 0.5f) };
    for (const auto& t : types)
    {
        CHECK(t.finite && t.peak <= 1.0f);
        CHECK(t.rms(static_cast<size_t>(0.5 * kRate), static_cast<size_t>(1.5 * kRate)) > 1e-3f); // echoes
        CHECK(t.rms(t.left.size() - 24000, t.left.size()) < 1e-4f);                               // that end
    }
    // BBD and Tape are darker than Clean: less of the echoes' energy in their highs.
    auto brightness = [](const Rendered& r) {
        double hi = 0.0, all = 0.0;
        for (size_t i = static_cast<size_t>(0.6 * kRate); i < static_cast<size_t>(2.0 * kRate); ++i)
        {
            const double d = static_cast<double>(r.left[i]) - r.left[i - 1];
            hi += d * d;
            all += static_cast<double>(r.left[i]) * r.left[i];
        }
        return hi / std::max(all, 1e-12);
    };
    CHECK(brightness(types[1]) < brightness(types[2]) * 0.7);
    CHECK(brightness(types[0]) < brightness(types[2]) * 0.7);
    // Muted: no echoes. Into the reverb: a tail after the echoes would have ended.
    const Rendered muted = echo(0, 0.5f, true);
    CHECK(muted.rms(static_cast<size_t>(0.5 * kRate), static_cast<size_t>(1.5 * kRate)) < 1e-4f);
    const Rendered reverb = echo(2, 0.0f, false, 1.0f);
    CHECK(reverb.rms(static_cast<size_t>(0.5 * kRate), static_cast<size_t>(1.5 * kRate))
          > types[2].rms(static_cast<size_t>(0.5 * kRate), static_cast<size_t>(1.5 * kRate)));
}

// ---------------------------------------------------------------------------------
// The synth engine.

namespace {
// One voice at twice the host rate, decimated, as the synth runs it; no drift.
std::vector<float> runVoice(const SynthPatch& patch, float note, double seconds, const SynthParams& macros = {})
{
    SynthVoice v;
    v.prepare(static_cast<float>(2.0 * kRate), 1234u);
    v.setAlive(0.0f);
    dsp::Decimator2 decimator;
    v.start(patch, macros, 1, note, 1.0f);
    std::vector<float> out(static_cast<size_t>(seconds * kRate));
    for (auto& y : out)
    {
        const float a = v.tick(macros, 0.0f);
        y = decimator.process(a, v.tick(macros, 0.0f));
    }
    return out;
}
double rmsOf(const std::vector<float>& x, size_t from, size_t to)
{
    double sum = 0.0;
    for (size_t i = from; i < to; ++i)
        sum += static_cast<double>(x[i]) * x[i];
    return std::sqrt(sum / static_cast<double>(std::max<size_t>(1, to - from)));
}
double frequencyOf(const std::vector<float>& x, size_t from, size_t to)
{
    double first = -1.0, last = -1.0;
    int crossings = 0;
    for (size_t i = from + 1; i < to; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            const double t = static_cast<double>(i - 1) + x[i - 1] / (x[i - 1] - x[i]);
            if (first < 0.0)
                first = t;
            last = t;
            ++crossings;
        }
    return crossings > 1 ? (crossings - 1) * kRate / (last - first) : 0.0;
}
SynthPatch plainPatch()
{
    SynthPatch p;
    p.driftCents = 0.0f;
    p.filter = FilterModel::None;
    p.a = 0.0001f;
    p.s = 1.0f;
    return p;
}
} // namespace

TEST("synth: the ladder and the OTA filter ring in tune at full resonance; the SEM peak is held in")
{
    for (FilterModel model : { FilterModel::Ladder, FilterModel::Ota })
        for (float cutoff : { 220.0f, 1000.0f, 4000.0f })
        {
            SynthPatch p = plainPatch();
            p.osc1 = 0.0f;
            p.hammer = 1.0f; // only a burst at the start
            p.filter = model;
            p.cutoffHz = cutoff;
            p.keyTrack = 0.0f;
            p.resonance = 1.0f;
            p.velocityToFilter = 0.0f;
            SynthParams m;
            m.resonance = 1.0f;
            m.envAmount = 0.0f;
            const auto x = runVoice(p, 60.0f, 2.0, m);
            const double f = frequencyOf(x, 48000, 96000);
            CHECK_MSG(std::abs(f / cutoff - 1.0) < 0.06, "rings at " << f << " Hz for " << cutoff);
            const double early = rmsOf(x, 48000, 52800), late = rmsOf(x, 91200, 96000);
            CHECK(early > 0.05 && late > early * 0.8 && late < early * 1.2); // steady, neither dying nor growing
        }
    // A loud saw driven into a resonant SEM: rounded off, not blown up.
    for (int mode = 0; mode < 4; ++mode)
    {
        SynthPatch p = plainPatch();
        p.filter = FilterModel::Sem;
        p.filterMode = mode;
        p.cutoffHz = 800.0f;
        p.resonance = 1.0f;
        p.drive = 6.0f;
        p.keyTrack = 0.0f;
        const auto x = runVoice(p, 36.0f, 0.5);
        float peak = 0.0f;
        for (float v : x)
            peak = std::max(peak, std::abs(v));
        CHECK_MSG(peak < 1.9f, "SEM mode " << mode << " peak " << peak); // (the decimator's overshoot on top)
    }
}

TEST("synth: the oscillators and FM are band-limited (little energy between the harmonics)")
{
    auto inharmonicDb = [](const std::vector<float>& x, double f0) {
        const size_t n = 4096, start = 9600;
        double harmonic = 0.0, other = 0.0;
        for (size_t k = 4; k < n / 2; k += 2)
        {
            double re = 0.0, im = 0.0;
            for (size_t i = 0; i < n; ++i)
            {
                const double w = 0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(i) / static_cast<double>(n));
                const double a = 2.0 * kPi * static_cast<double>(k * i) / static_cast<double>(n);
                re += x[start + i] * w * std::cos(a);
                im -= x[start + i] * w * std::sin(a);
            }
            const double h = static_cast<double>(k) * kRate / static_cast<double>(n) / f0;
            const bool near = std::abs(h - std::round(h)) * f0 < 3.0 * kRate / static_cast<double>(n);
            (near ? harmonic : other) += re * re + im * im;
        }
        return 10.0 * std::log10(other / (harmonic + other));
    };
    const float note = 96.0f; // 2093 Hz: plenty of harmonics above Nyquist
    const double f0 = 440.0 * std::pow(2.0, (note - 69.0) / 12.0);
    for (float shape : { 0.0f, 0.5f, 1.0f })
    {
        SynthPatch p = plainPatch();
        p.shape1 = shape;
        CHECK_MSG(inharmonicDb(runVoice(p, note, 0.35), f0) < -28.0, "shape " << shape);
    }
    SynthPatch fm = plainPatch();
    fm.osc = Osc::Fm4;
    fm.fmAlg = 2;
    fm.levelA = 0.5f;
    fm.levelB = 0.4f;
    fm.ratioB1 = 14.0f; // the tine, far above at this note: faded out, not folded back
    fm.envA = { 0.0001f, 100.0f, 1.0f };
    fm.envB = fm.envA;
    CHECK(inharmonicDb(runVoice(fm, note, 0.35), f0) < -28.0);
}

TEST("synth: each FM algorithm is its own sound")
{
    SynthPatch p = plainPatch();
    p.osc = Osc::Fm4;
    p.ratioA = 2.0f;
    p.ratioB1 = 3.0f;
    p.ratioB2 = 1.0f;
    p.levelA = 0.6f;
    p.levelB = 0.5f;
    p.fmMix = 0.5f;
    p.fmFeedback = 0.3f;
    p.envA = { 0.0001f, 100.0f, 1.0f };
    p.envB = p.envA;
    std::vector<std::vector<float>> spectra;
    for (int alg = 0; alg < 8; ++alg)
    {
        p.fmAlg = alg;
        const auto x = runVoice(p, 48.0f, 0.3);
        std::vector<float> s;
        for (int h = 1; h <= 12; ++h) // the first harmonics' levels
        {
            double re = 0.0, im = 0.0;
            const double f = 130.81 * h;
            for (size_t i = 4800; i < 14400; ++i)
            {
                re += x[i] * std::cos(2.0 * kPi * f * static_cast<double>(i) / kRate);
                im += x[i] * std::sin(2.0 * kPi * f * static_cast<double>(i) / kRate);
            }
            s.push_back(static_cast<float>(10.0 * std::log10(re * re + im * im + 1e-9)));
        }
        spectra.push_back(s);
    }
    for (int a = 0; a < 8; ++a)
        for (int b = a + 1; b < 8; ++b)
        {
            double diff = 0.0;
            for (size_t h = 0; h < 12; ++h)
                diff += std::abs(spectra[static_cast<size_t>(a)][h] - spectra[static_cast<size_t>(b)][h]);
            CHECK_MSG(diff / 12.0 > 1.0, "algorithms " << a + 1 << " and " << b + 1 << " sound alike");
        }
}

TEST("synth: the Tone knobs at 50% are the design; turned, they change the sound and stay safe")
{
    for (int s = 0; s < static_cast<int>(SynthSound::Count); ++s)
    {
        const auto sound = static_cast<SynthSound>(s);
        const SynthPatch& design = synthPatch(sound);
        SynthParams params;
        params.sound = sound;
        const SynthPatch same = applyTone(design, params);
        CHECK(same.shape1 == design.shape1 && same.osc2 == design.osc2 && same.levelA == design.levelA && same.ratioA == design.ratioA
              && same.pdWave == design.pdWave && same.trioSet == design.trioSet && same.drive == design.drive && same.filter == design.filter);
        for (int k = 0; k < kSynthToneKnobs; ++k)
            CHECK(std::string(synthToneName(sound, k)).size() > 1);
    }
    // Every knob of a few sounds of each engine moves the sound; at its ends it stays finite.
    for (SynthSound sound : { SynthSound::WarmPad, SynthSound::EPiano, SynthSound::Organ, SynthSound::MonkChoir, SynthSound::Koto,
                              SynthSound::SingingBowl, SynthSound::PhasePad, SynthSound::TrioArp, SynthSound::ChipLead })
    {
        auto render = [sound](int knob, float value, int filterChoice) {
            SoundEngine e;
            dryEngine(e);
            SynthParams p;
            p.sound = sound;
            p.reverbSend = 0.0f;
            p.level = 0.3f;
            if (knob >= 0)
                p.tone[static_cast<size_t>(knob)] = value;
            p.filterChoice = filterChoice;
            e.setSynth(kChordSynth, p);
            for (int n : { 48, 55, 64 })
                e.noteOn(kChordSynth, n, static_cast<float>(n), 0.9f);
            Rendered out;
            renderInto(e, out, 1.2);
            return out;
        };
        const Rendered design = render(-1, 0.5f, 0);
        for (int k = 0; k < kSynthToneKnobs; ++k)
        {
            double change = 0.0;
            for (float value : { 0.0f, 1.0f })
            {
                const Rendered moved = render(k, value, 0);
                CHECK_MSG(moved.finite, synthSoundName(sound) << " tone " << k);
                double d = 0.0;
                for (size_t i = 0; i < moved.left.size(); ++i)
                    d += std::abs(static_cast<double>(moved.left[i]) - design.left[i]);
                change = std::max(change, d / static_cast<double>(moved.left.size()));
            }
            CHECK_MSG(change > 1e-4, synthSoundName(sound) << " tone " << k << " (" << synthToneName(sound, k) << ") does nothing");
        }
        for (int f = 1; f <= 5; ++f)
            CHECK(render(-1, 0.5f, f).finite);
    }
}

TEST("synth: the sounds are loudness-matched (within 2 dB, K-weighted)")
{
    struct Biquad
    {
        double b0, b1, b2, a1, a2, z1 = 0.0, z2 = 0.0;
        double tick(double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };
    std::vector<double> loudness;
    for (int s = 0; s < static_cast<int>(SynthSound::Count); ++s)
    {
        const auto sound = static_cast<SynthSound>(s);
        const bool bass = std::string(synthCategoryName(sound)) == "Bass";
        SoundEngine e;
        dryEngine(e);
        CharacterParams c;
        c.dirt = 0.0f;
        e.setCharacter(c);
        SynthParams p;
        p.sound = sound;
        p.reverbSend = 0.0f;
        p.level = 0.15f;
        const int synth = bass ? 0 : kChordSynth;
        e.setSynth(synth, p);
        Rendered out;
        if (bass)
            for (int k = 0; k < 8; ++k)
            {
                const int line[8] = { 36, 36, 43, 36, 41, 36, 39, 43 };
                e.noteOn(synth, 10 + k, static_cast<float>(line[k]), k % 2 ? 0.7f : 0.95f);
                renderInto(e, out, 0.18);
                e.noteOff(synth, 10 + k);
                renderInto(e, out, 0.07);
            }
        else
        {
            for (int n : { 60, 64, 67 })
                e.noteOn(synth, n, static_cast<float>(n), 0.8f);
            renderInto(e, out, 2.0);
        }
        // The loudest 400 ms, K-weighted (ITU BS.1770 at 48 kHz).
        Biquad shelf[2] { { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 },
                          { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 } };
        Biquad hp[2] { { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 }, { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 } };
        std::vector<double> energy(out.left.size());
        for (size_t i = 0; i < energy.size(); ++i)
        {
            const double l = hp[0].tick(shelf[0].tick(out.left[i])), r = hp[1].tick(shelf[1].tick(out.right[i]));
            energy[i] = l * l + r * r;
        }
        const size_t window = static_cast<size_t>(0.4 * kRate), hop = static_cast<size_t>(0.1 * kRate);
        double best = 1e-12;
        for (size_t from = 0; from + window <= energy.size(); from += hop)
        {
            double sum = 0.0;
            for (size_t i = from; i < from + window; ++i)
                sum += energy[i];
            best = std::max(best, sum / static_cast<double>(window));
        }
        loudness.push_back(-0.691 + 10.0 * std::log10(best));
    }
    std::vector<double> sorted = loudness;
    std::sort(sorted.begin(), sorted.end());
    const double median = sorted[sorted.size() / 2];
    for (int s = 0; s < static_cast<int>(SynthSound::Count); ++s)
        CHECK_MSG(std::abs(loudness[static_cast<size_t>(s)] - median) < 2.0,
                  synthSoundName(static_cast<SynthSound>(s)) << " at " << loudness[static_cast<size_t>(s)] << " LUFS, the median " << median);
}

TEST("synth: chords clicked in quick succession keep all their notes, however long the tails")
{
    for (SynthSound sound : { SynthSound::WarmPad, SynthSound::FmBells, SynthSound::FeltPiano })
    {
        Synth synth;
        synth.prepare(static_cast<float>(kRate));
        synth.setPoly(true);
        SynthParams p;
        p.sound = sound;
        p.release = 1.0f; // the longest tails
        synth.setParams(p);
        int base = 0;
        for (int chord = 0; chord < 10; ++chord)
        {
            for (int n = 0; n < 6; ++n)
                synth.noteOff(base + n);
            base = 100 * (chord + 1);
            for (int n = 0; n < 6; ++n)
                synth.noteOn(base + n, static_cast<float>(48 + 3 * n + chord % 3), 0.8f);
            for (int i = 0; i < 2400; ++i)
                synth.tick();
            CHECK_MSG(synth.heldVoices() == 6, synthSoundName(sound) << " chord " << chord << ": " << synth.heldVoices() << " notes");
        }
    }
}
