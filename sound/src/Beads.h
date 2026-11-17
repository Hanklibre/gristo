#pragma once

// A granular processor after Mutable Instruments Beads (Copyright 2017 Emilie Gillet,
// MIT licence; https://github.com/pichenettes/eurorack, beads/dsp): its trigger
// generator (a clock that free-runs or follows gates, dividing them or letting them
// through by chance), its grain scheduler and envelope, its four "media" (digital,
// vintage digital, clean tape, dirty tape) and its feedback path. Ported to floating
// point. Gristo's changes: the gates are drum hits; random pitch can snap to musical
// intervals; the wet signal goes to Gristo's own reverb instead of a second one.

#include "Dsp.h"

#include <array>
#include <cstdint>
#include <vector>

namespace gristo::sound {

struct BeadsSettings
{
    float density = 0.25f; // see Beads::Trigger
    float time = 0.5f;     // 0 = right behind the recording head .. 1 = the oldest audio
    float size = 0.5f;     // grain length; below 1/3 the grains play backwards
    float pitch = 0.0f;    // semitones
    float shape = 0.5f;    // the grain envelope: square, decay, bell, reverse ramp
    float timeRandom = 0.0f, sizeRandom = 0.0f, pitchRandom = 0.0f, shapeRandom = 0.0f; // 0..1
    float feedback = 0.0f;
    int quality = 0;       // 0 digital, 1 vintage digital, 2 clean tape, 3 dirty tape
    bool freeze = false;
    bool gated = false;    // grains follow gates (drum hits) instead of the internal clock
    bool musicalPitch = true; // random pitch snaps to octaves, fifths and fourths
};

class Beads
{
public:
    static constexpr int kGrains = 32;

    void prepare(float sampleRate); // allocates (4 seconds of stereo audio)
    void reset();
    void set(const BeadsSettings& settings) { settings_ = settings; }
    void gate() { pendingGate_ = true; } // a drum hit, picked up at the next sample
    // Writes the input to the buffer and returns the grains (wet only).
    void process(float inL, float inR, float& outL, float& outR);
    int activeGrains() const;

private:
    struct Grain
    {
        bool active = false;
        double delay = 0.0; // samples behind the recording head
        float speed = 1.0f;
        int remaining = 0;
        // The grain envelope: attack, sustain, release, with a smoothness between linear and S-shaped.
        int stage = 3;
        float phase = 0.0f, inc[4] {}, smoothness = 0.0f;
        float steal = 1.0f, stealStep = 0.0f;
        float gain[2][2] {};
    };

    bool trigger();
    void startGrain(float fraction);
    float envelope(Grain& g);
    void read(double delay, float& l, float& r) const;
    float random() { return 0.5f + 0.5f * noise_.next(); }

    BeadsSettings settings_;
    float sampleRate_ = 48000.0f;
    std::vector<float> buffer_[2];
    int size_ = 0, write_ = 0;
    std::array<Grain, kGrains> grains_ {};
    dsp::Noise noise_ { 0xbead5u };

    // Trigger generator
    bool pendingGate_ = false;
    float clockPhase_ = 0.0f, ratio_ = 1.0f;
    int sinceGate_ = 0, gateCount_ = 0;
    float regularity_ = 1.0f;
    int sinceGrain_ = 0;

    // Media and feedback
    float lastWet_[2] {}, held_[2] {}, holdPhase_ = 0.0f;
    float wowPhase_ = 0.0f, flutterPhase_ = 0.0f;
    dsp::OnePole tone_[2], dcIn_[2], dcOut_[2];
};

} // namespace gristo::sound
