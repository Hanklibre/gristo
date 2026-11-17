// Mutable Instruments Rings, played by Gristo. The DSP is the module's own code
// (third_party/eurorack, Copyright 2015 Emilie Gillet, MIT licence).
#include "Rings.h"

#include "rings/dsp/part.h"
#include "rings/dsp/patch.h"
#include "rings/dsp/performance_state.h"

#include <algorithm>
#include <cmath>

namespace gristo::sound {

struct Rings::Engine
{
    rings::Part part;
    std::array<std::uint16_t, 32768> reverbBuffer {};
};

Rings::Rings() : engine_(std::make_unique<Engine>()) { reset(); }
Rings::~Rings() = default;

void Rings::prepare(float sampleRate)
{
    hostRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    ratio_ = 48000.0 / static_cast<double>(hostRate_);
    reset();
}

void Rings::reset()
{
    engine_->reverbBuffer.fill(0);
    engine_->part.Init(engine_->reverbBuffer.data());
    model_ = polyphony_ = -1;
    nextIn_ = 1.0;
    lastIn_ = 0.0f;
    inCount_ = 0;
    outL_.fill(0.0f);
    outR_.fill(0.0f);
    produced_ = 0;
    readPos_ = -static_cast<double>(kBlock + 8); // behind the newest block, so the cubic always has its points
    pending_.clear();
    pending_.reserve(64);
    quietBlocks_ = kQuietBlocks;
    note_ = 60.0f;
    chord_ = 0;
}

void Rings::strum(float midiNote)
{
    if (std::isfinite(midiNote) && pending_.size() < 64)
        pending_.push_back(std::clamp(midiNote, 12.0f, 120.0f));
}

void Rings::renderBlock()
{
    auto& part = engine_->part;
    const RingsSettings& s = settings_;
    const int model = std::clamp(s.model, 0, static_cast<int>(rings::RESONATOR_MODEL_LAST) - 1);
    const int polyphony = s.polyphony >= 4 ? 4 : s.polyphony >= 2 ? 2 : 1;
    if (polyphony != polyphony_) // the module reconfigures its resonators on these: only on a change
    {
        polyphony_ = polyphony;
        part.set_polyphony(polyphony);
    }
    if (model != model_)
    {
        model_ = model;
        part.set_model(static_cast<rings::ResonatorModel>(model));
    }

    rings::PerformanceState ps {};
    ps.internal_exciter = s.pluck;
    ps.internal_strum = false;
    ps.internal_note = false;
    ps.strum = false;
    if (!pending_.empty())
    {
        note_ = pending_.front();
        pending_.erase(pending_.begin());
        ps.strum = true;
    }
    ps.tonic = 12.0f + static_cast<float>(((static_cast<int>(std::lround(s.tonic)) % 12) + 12) % 12);
    ps.note = note_ - ps.tonic;
    ps.fm = 0.0f;
    // As the module reads its chord from Structure, with a little hysteresis.
    const float chord = std::clamp(s.structure, 0.0f, 1.0f) * static_cast<float>(rings::kNumChords - 1);
    const float hysteresis = chord - static_cast<float>(chord_) > 0.0f ? -0.1f : 0.1f;
    chord_ = std::clamp(static_cast<int>(chord + hysteresis + 0.5f), 0, rings::kNumChords - 1);
    ps.chord = chord_;

    rings::Patch patch {};
    patch.structure = std::clamp(s.structure, 0.0f, 0.9995f);
    patch.brightness = std::clamp(s.brightness, 0.0f, 1.0f);
    patch.damping = std::clamp(s.damping, 0.0f, 0.9995f);
    patch.position = std::clamp(s.position, 0.0f, 0.9995f);

    float out[kBlock], aux[kBlock];
    part.Process(ps, patch, inBlock_.data(), out, aux, kBlock);

    float peak = 0.0f;
    for (int i = 0; i < kBlock; ++i)
    {
        const float l = std::isfinite(out[i]) ? out[i] : 0.0f;
        const float r = std::isfinite(aux[i]) ? aux[i] : 0.0f;
        const auto k = static_cast<size_t>((produced_ + i) & (kRing - 1));
        outL_[k] = l;
        outR_[k] = r;
        peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
    }
    produced_ += kBlock;
    quietBlocks_ = ps.strum || peak > 1e-4f ? 0 : std::min(quietBlocks_ + 1, kQuietBlocks);
}

void Rings::process(float in, float& outL, float& outR)
{
    if (!std::isfinite(in))
        in = 0.0f;
    // The input at the module's sample times.
    const double step = 1.0 / ratio_; // host samples per module sample
    while (nextIn_ <= 1.0)
    {
        inBlock_[static_cast<size_t>(inCount_++)] = lastIn_ + (in - lastIn_) * static_cast<float>(nextIn_);
        nextIn_ += step;
        if (inCount_ == kBlock)
        {
            inCount_ = 0;
            renderBlock();
        }
    }
    nextIn_ -= 1.0;
    lastIn_ = in;

    // The output, read a little behind what has been rendered.
    const double pos = readPos_;
    readPos_ += ratio_;
    const auto i = static_cast<std::int64_t>(std::floor(pos));
    if (i < 1 || i + 2 >= produced_)
    {
        outL = outR = 0.0f;
        return;
    }
    const float t = static_cast<float>(pos - static_cast<double>(i));
    auto cubic = [&](const std::array<float, kRing>& b) {
        const float xm1 = b[static_cast<size_t>((i - 1) & (kRing - 1))], x0 = b[static_cast<size_t>(i & (kRing - 1))];
        const float x1 = b[static_cast<size_t>((i + 1) & (kRing - 1))], x2 = b[static_cast<size_t>((i + 2) & (kRing - 1))];
        const float c = (x1 - xm1) * 0.5f, v = x0 - x1, w = c + v, a = w + v + (x2 - x0) * 0.5f, bb = w + a;
        return (((a * t) - bb) * t + c) * t + x0;
    };
    outL = cubic(outL_);
    outR = cubic(outR_);
}

} // namespace gristo::sound
