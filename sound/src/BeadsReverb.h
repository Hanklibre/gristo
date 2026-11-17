#pragma once

// The reverb of Mutable Instruments Beads (Copyright 2017 Emilie Gillet, MIT licence;
// beads/dsp/fx/reverb.h and fx_engine.h): the Griesinger topology from Dattorro's
// paper, four allpass diffusers then a loop of two allpass + delay branches, with
// slow modulation smearing the first diffuser and the two long delays. Ported to
// floating point; the delay lengths (set for 48 kHz) follow the host's rate.

#include "Dsp.h"

#include <vector>

namespace gristo::sound {

class BeadsReverb
{
public:
    void prepare(float sampleRate);
    void reset();
    void setMix(float mix) { mix_ = mix; }          // 0..1: dry .. all reverb
    void setTime(float time) { time_ = time; }      // loop gain, ~0.3 .. 0.95
    void setLowpass(float lp) { lp_ = lp; }         // loop damping coefficient, ~0.4 .. 0.75
    void process(float& l, float& r);

private:
    static constexpr int kLines = 10;
    float read(int line, float offset) const;       // offset -1: the line's tail
    float readTail(int line) const { return read(line, -1.0f); }
    float interpolate(int line, float offset) const;
    void write(int line, float value, int offset = 0);

    std::vector<float> buffer_;
    int mask_ = 0, writePtr_ = 0;
    int base_[kLines] {}, length_[kLines] {};
    float scale_ = 1.0f, sampleRate_ = 48000.0f;
    float lfoPhase_[2] {}, lfoInc_[2] {}, lfo_[2] {};
    int lfoCount_ = 0;
    float mix_ = 0.0f, time_ = 0.5f, lp_ = 0.7f, lp1_ = 0.0f, lp2_ = 0.0f;
    float wetGain_ = 0.0f, dryGain_ = 1.0f, send_ = 0.0f;
};

} // namespace gristo::sound
