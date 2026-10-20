#pragma once

#include <array>
#include <cstdint>
#include <memory>

// Gristo's built-in sounds: synthesized drum kits, a small poly synth per arp, and a
// shared reverb + delay. Host-independent (no JUCE) so the plugin, the standalone /
// Android app and the tests all use the same code.
namespace gristo::sound {

constexpr int kNumDrumVoices = 6; // kick, snare, hihat 1, hihat 2, perc 1, perc 2
constexpr int kNumSynths = 2;     // one per arp

enum class Kit : int { Classic = 0, Eight08, Trap, LoFi, Minimal, Reggaeton, Count };
const char* kitName(Kit kit);

enum class SynthSound : int
{
    SubBass = 0, AcidBass, ReeseBass, FmBass,     // basses
    EPiano, Bell, Pluck, Organ,                   // keys
    WarmPad, GlassPad, DarkPad,                   // pads
    SquareLead, SyncLead, SoftLead,               // leads
    Count
};
const char* synthSoundName(SynthSound sound);
const char* synthCategoryName(SynthSound sound); // "Bass", "Keys", "Pads", "Leads"

struct KitParams
{
    Kit kit = Kit::Eight08;
    float tune = 0.0f;  // semitones, -12..12
    float decay = 0.5f; // 0..1, 0.5 = as designed
    float tone = 0.5f;  // 0..1, dark .. bright, 0.5 = neutral
    float drive = 0.0f; // 0..1
    std::array<float, kNumDrumVoices> level { 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f };
    float reverbSend = 0.08f;
    float delaySend = 0.0f;
};

// Macro knobs are 0..1 with 0.5 = the sound as designed (glide and level excepted).
struct SynthParams
{
    SynthSound sound = SynthSound::AcidBass;
    float cutoff = 0.5f;
    float resonance = 0.5f;
    float envAmount = 0.5f;
    float attack = 0.5f;
    float release = 0.5f;
    float glide = 0.3f; // 0..1 -> 0..500 ms, used when notes overlap (slides)
    float level = 0.7f;
    float reverbSend = 0.15f;
    float delaySend = 0.0f;
    float cutoffMod = 0.0f; // modulation lane offset, added to `cutoff`
};

struct FxParams
{
    float reverbSize = 0.5f;     // 0..1
    float delayBeats = 0.75f;    // delay time in beats (0.75 = dotted 8th)
    float delayFeedback = 0.35f; // 0..0.9
    double bpm = 120.0;
};

class SoundEngine
{
public:
    SoundEngine();
    ~SoundEngine();
    SoundEngine(const SoundEngine&) = delete;
    SoundEngine& operator=(const SoundEngine&) = delete;

    void prepare(double sampleRate); // allocates; call before rendering
    void reset();                    // silences everything

    void setKit(const KitParams& params);
    void setSynth(int synth, const SynthParams& params);
    void setFx(const FxParams& params);

    void triggerDrum(int voice, float velocity); // velocity 0..1
    // `id` identifies the note for its note-off (the MIDI out note); `pitch` is
    // what it sounds like (MIDI note number, fractional allowed).
    void noteOn(int synth, int id, float pitch, float velocity);
    void noteOff(int synth, int id);
    void allNotesOff();

    // Adds `numSamples` of stereo output to left/right (soft-clipped, times gain).
    void render(float* left, float* right, int numSamples, float gain);

    // For tests and meters.
    int activeSynthVoices(int synth) const;
    bool drumActive(int voice) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gristo::sound
