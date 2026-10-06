#include "gristo/Engine.h"

#include <algorithm>
#include <cmath>

namespace gristo {

namespace {
constexpr float kRollVelocity[3] = { 0.55f, 0.72f, 1.0f };
constexpr float kMinVelocity = 1.0f / 127.0f;
constexpr int kMaxStepsPerBlock = 4096;
} // namespace

Engine::Engine(const DrumMapData& data) : map_(data), generator_(map_)
{
    pending_.reserve(256);
}

void Engine::reset()
{
    pending_.clear();
    sounding_.fill(false);
    needsResync_ = true;
}

float Engine::velocityFor(const VoiceHit& hit, int voice) const
{
    const auto& d = params_.dynamics;
    const float lo = d.velLow[static_cast<size_t>(voice)];
    const float hi = d.velHigh[static_cast<size_t>(voice)];
    if (hit.accent)
        return hi;
    // How far above the Fill threshold the level is, relative to the accent level:
    // barely-there notes become ghosts, solid notes reach velLow.
    const float span = std::max(1e-3f, kAccentLevel - hit.threshold);
    const float t = std::clamp((hit.level - hit.threshold) / span, 0.0f, 1.0f);
    return lo * (1.0f - std::clamp(d.dynamics, 0.0f, 1.0f) * 0.6f * (1.0f - t));
}

HitInfo Engine::describe(std::int64_t step, int voice, double bpm, bool withHumanize) const
{
    return describe(generator_.evaluate(params_.pattern, step), step, voice, bpm, withHumanize);
}

HitInfo Engine::describe(const StepHits& hits, std::int64_t step, int voice, double bpm, bool withHumanize) const
{
    const auto& g = params_.groove;
    const VoiceHit& vh = hits[static_cast<size_t>(voice)];

    HitInfo h;
    h.on = vh.on;
    h.accent = vh.accent;
    h.dropped = vh.dropped;
    h.level = vh.level;
    h.gridBeats = Groove::gridPosition(g, step);
    h.timeBeats = h.gridBeats + Groove::feelOffset(g, voice, step, bpm);
    if (!h.on)
        return h;

    h.velocity = velocityFor(vh, voice);
    const double nextGrid = Groove::gridPosition(g, step + 1);
    h.rollCount = Groove::rollCount(g, voice);
    h.rollSpacingBeats = (nextGrid - h.gridBeats) / h.rollCount;

    if (withHumanize && params_.humanize.amount > 0.0f)
    {
        HumanizeInput in;
        in.voice = voice;
        in.step = step;
        in.level = vh.level;
        in.accent = vh.accent;
        in.gridBeats = h.gridBeats;
        in.nominalBeats = static_cast<double>(step) / kStepsPerBeat;
        in.prevGapBeats = h.gridBeats - Groove::gridPosition(g, step - 1);
        in.nextGapBeats = nextGrid - h.gridBeats;
        in.bpm = bpm;
        const HumanizeResult r = Humanizer::apply(params_.humanize, in);
        h.timeBeats += r.offsetBeats;
        h.velocity *= r.velocityScale;
    }
    h.velocity = std::clamp(h.velocity, kMinVelocity, 1.0f);
    return h;
}

void Engine::schedule(const HitInfo& hit, int voice)
{
    const double length = std::max(1e-4, static_cast<double>(params_.noteLengthBeats));
    for (int k = 0; k < hit.rollCount; ++k)
    {
        const double on = hit.timeBeats + k * hit.rollSpacingBeats;
        double off = on + length;
        float velocity = hit.velocity;
        if (hit.rollCount > 1)
        {
            off = on + std::min(length, 0.9 * hit.rollSpacingBeats);
            velocity = std::max(kMinVelocity, hit.velocity * kRollVelocity[std::min(k, 2)]);
        }

        // A new note-on cuts the previous note of the same voice.
        for (Pending& p : pending_)
            if (!p.noteOn && p.voice == voice && p.beats > on)
                p.beats = on;

        pending_.push_back({ on, voice, velocity, true });
        pending_.push_back({ off, voice, 0.0f, false });
    }
}

void Engine::flushNoteOffs(std::vector<OutputEvent>& out)
{
    for (int v = 0; v < kNumVoices; ++v)
    {
        if (sounding_[static_cast<size_t>(v)])
            out.push_back({ 0, v, 0.0f, false });
    }
    sounding_.fill(false);
    pending_.clear();
}

void Engine::resync(double ppqStart)
{
    const auto first = static_cast<std::int64_t>(std::ceil(ppqStart * kStepsPerBeat - 1e-9));
    nextStep_.fill(first);
    needsResync_ = false;
}

void Engine::process(const BlockInfo& block, std::vector<OutputEvent>& out)
{
    if (!block.playing || block.numSamples <= 0 || block.bpm <= 0.0 || block.sampleRate <= 0.0)
    {
        if (!block.playing)
        {
            flushNoteOffs(out);
            needsResync_ = true;
        }
        return;
    }

    const double beatsPerSample = block.bpm / (60.0 * block.sampleRate);
    const double ppqStart = block.ppqStart;
    const double ppqEnd = ppqStart + block.numSamples * beatsPerSample;

    if (needsResync_ || std::abs(ppqStart - expectedPpq_) > kJumpToleranceBeats)
    {
        flushNoteOffs(out);
        resync(ppqStart);
    }
    expectedPpq_ = ppqEnd;

    // Walk each voice forward until its next step starts after this block.
    for (int v = 0; v < kNumVoices; ++v)
    {
        auto& next = nextStep_[static_cast<size_t>(v)];
        for (int guard = 0; guard < kMaxStepsPerBlock; ++guard)
        {
            if (static_cast<double>(next) / kStepsPerBeat - kMaxEarlyBeats >= ppqEnd)
                break;
            const HitInfo hit = describe(generator_.evaluate(params_.pattern, next), next, v, block.bpm, true);
            if (hit.timeBeats >= ppqEnd)
                break;
            if (hit.on)
                schedule(hit, v);
            ++next;
        }
    }

    // Emit everything due before the end of the block, note-offs first on ties.
    std::stable_sort(pending_.begin(), pending_.end(), [](const Pending& a, const Pending& b) {
        if (a.beats != b.beats) return a.beats < b.beats;
        return !a.noteOn && b.noteOn;
    });

    size_t emitted = 0;
    for (; emitted < pending_.size() && pending_[emitted].beats < ppqEnd; ++emitted)
    {
        const Pending& p = pending_[emitted];
        // The small epsilon keeps onsets that land exactly on a sample boundary from
        // rounding down to the previous sample.
        const double offset = std::floor((p.beats - ppqStart) / beatsPerSample + 1e-6);
        const int sample = static_cast<int>(std::clamp(offset, 0.0, static_cast<double>(block.numSamples - 1)));
        out.push_back({ sample, p.voice, p.velocity, p.noteOn });
        sounding_[static_cast<size_t>(p.voice)] = p.noteOn;
    }
    pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(emitted));
}

} // namespace gristo
