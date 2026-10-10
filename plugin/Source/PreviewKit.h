#pragma once

#include <gristo/Arp.h>

namespace gristo {

// A tiny synthesized drum kit so the plugin makes sound out of the box, in hosts
// with awkward MIDI routing and in the standalone app. Not meant to replace a real
// drum instrument: route the MIDI output to one for production.
class PreviewKit
{
public:
    void prepare(double sampleRate);
    void reset();
    void trigger(int voice, float velocity);
    // Simple mono synth per arp, with glide when a note starts before the last ended.
    // `note` identifies the note (for note-off); `pitch` is what it sounds like.
    void arpNoteOn(int arp, int note, int pitch, float velocity);
    void arpNoteOff(int arp, int note);
    // Adds the kit's output to the two channel buffers.
    void render(float* left, float* right, int numSamples, float gain);

private:
    struct VoiceState
    {
        float amp = 0.0f;      // current amplitude envelope
        float ampDecay = 0.0f; // per-sample multiplier
        float phase = 0.0f;
        float freq = 0.0f;     // current oscillator frequency (Hz)
        float freqEnd = 0.0f;
        float freqDecay = 0.0f;
        float noiseAmp = 0.0f;
        float noiseDecay = 0.0f;
        float hpState = 0.0f;
        float hpPrev = 0.0f;
        int burstCounter = 0;  // clap re-triggers
        float velocity = 0.0f;
    };

    float decayFor(float seconds) const;
    float noise();

    struct SynthState
    {
        int note = -1;
        bool gate = false;
        float freq = 110.0f;
        float target = 110.0f;
        float phase = 0.0f;
        float env = 0.0f;
        float filterEnv = 0.0f;
        float lowpass = 0.0f;
        float velocity = 0.0f;
    };

    double sampleRate_ = 48000.0;
    std::uint32_t rng_ = 0x12345678u;
    VoiceState voices_[kNumVoices];
    SynthState synths_[kNumArps];
};

} // namespace gristo
