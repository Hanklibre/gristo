#include "gristo/Engine.h"

#include <algorithm>
#include <cmath>

namespace gristo {

namespace {
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

void Engine::setParams(const EngineParams& p)
{
    params_ = p;
    hasModulation_ = false;
    shuffleModulated_ = false;
    for (const ModLane& lane : params_.modulation.lanes)
        for (const ModSlot& slot : lane.slots)
            if (slot.target > mod::kNone && slot.target < mod::kNumTargets && slot.amount != 0.0f)
            {
                hasModulation_ = true;
                shuffleModulated_ |= slot.target == mod::kShuffle;
            }
}

float Engine::modulationAt(int target, std::int64_t step) const
{
    float sum = 0.0f;
    for (const ModLane& lane : params_.modulation.lanes)
        for (const ModSlot& slot : lane.slots)
            if (slot.target == target && slot.amount != 0.0f)
                sum += slot.amount * lane.valueAt(step);
    return sum;
}

EngineParams Engine::effectiveParams(std::int64_t step) const
{
    EngineParams e = params_;
    if (!hasModulation_)
        return e;

    std::array<float, mod::kNumTargets> acc {};
    const int cell = Groove::cellSteps(params_.groove.timing);
    const std::int64_t cellStart = floorDiv(step, cell) * cell;
    for (const ModLane& lane : params_.modulation.lanes)
        for (const ModSlot& slot : lane.slots)
        {
            if (slot.target <= mod::kNone || slot.target >= mod::kNumTargets || slot.amount == 0.0f)
                continue;
            const std::int64_t at = slot.target == mod::kShuffle ? cellStart : step;
            acc[static_cast<size_t>(slot.target)] += std::clamp(slot.amount, -1.0f, 1.0f) * lane.valueAt(at);
        }

    auto add = [&](float base, int target, float lo, float hi) {
        return std::clamp(base + acc[static_cast<size_t>(target)] * mod::targetRange(target), lo, hi);
    };
    for (int v = 0; v < kNumVoices; ++v)
    {
        const auto i = static_cast<size_t>(v);
        e.pattern.fill[i] = add(e.pattern.fill[i], mod::kFill + v, 0.0f, 1.0f);
        e.pattern.probability[i] = add(e.pattern.probability[i], mod::kProbability + v, 0.0f, 1.0f);
        e.dynamics.velocityOffset[i] = acc[static_cast<size_t>(mod::kVelocity + v)];
    }
    e.pattern.mapX = add(e.pattern.mapX, mod::kMapX, 0.0f, 1.0f);
    e.pattern.mapY = add(e.pattern.mapY, mod::kMapY, 0.0f, 1.0f);
    e.pattern.chaos = add(e.pattern.chaos, mod::kChaos, 0.0f, 1.0f);
    e.groove.shuffle = add(e.groove.shuffle, mod::kShuffle, 0.0f, static_cast<float>(kNumShuffleRows - 1));
    e.groove.feelMs = add(e.groove.feelMs, mod::kFlamMs, 0.0f, 30.0f);
    e.humanize.amount = add(e.humanize.amount, mod::kHumanize, 0.0f, 1.0f);
    return e;
}

double Engine::gridAt(std::int64_t step) const
{
    return Groove::gridPosition(shuffleModulated_ ? effectiveParams(step).groove : params_.groove, step);
}

float Engine::normalizedVelocity(const EngineParams& ep, const VoiceHit& hit, int voice)
{
    if (hit.accent)
        return 1.0f;
    // How far above the Fill threshold the level is, relative to the accent level:
    // barely-there notes become ghosts, solid notes reach kNormalVelocity.
    const auto& d = ep.dynamics;
    const float span = std::max(1e-3f, kAccentLevel - hit.threshold);
    const float t = std::clamp((hit.level - hit.threshold) / span, 0.0f, 1.0f);
    const float dynamics = std::clamp(d.dynamics * d.voiceDynamics[static_cast<size_t>(voice)], 0.0f, 1.0f);
    return kNormalVelocity * (1.0f - dynamics * 0.6f * (1.0f - t));
}

float Engine::mapVelocity(int voice, float normalized) const
{
    const auto i = static_cast<size_t>(voice);
    const float lo = std::clamp(params_.dynamics.velMin[i], 0.0f, 1.0f);
    const float hi = std::clamp(params_.dynamics.velMax[i], lo, 1.0f);
    return std::clamp(lo + (hi - lo) * std::clamp(normalized, 0.0f, 1.0f), kMinVelocity, 1.0f);
}

HitInfo Engine::describe(std::int64_t step, int voice, double bpm, bool withHumanize) const
{
    const EngineParams ep = effectiveParams(step);
    const StepHits hits = generator_.evaluate(ep.pattern, step);
    const auto& g = ep.groove;
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

    float norm = normalizedVelocity(ep, vh, voice);
    const double nextGrid = gridAt(step + 1);
    h.rollCount = Groove::rollCount(g, voice, step);
    h.rollSpacingBeats = (nextGrid - h.gridBeats) / h.rollCount;

    if (withHumanize && ep.humanize.amount > 0.0f)
    {
        HumanizeInput in;
        in.voice = voice;
        in.step = step;
        in.level = vh.level;
        in.accent = vh.accent;
        in.gridBeats = h.gridBeats;
        in.nominalBeats = static_cast<double>(step) / kStepsPerBeat;
        in.prevGapBeats = h.gridBeats - gridAt(step - 1);
        in.nextGapBeats = nextGrid - h.gridBeats;
        in.bpm = bpm;
        const HumanizeResult r = Humanizer::apply(ep.humanize, in);
        h.timeBeats += r.offsetBeats;
        norm *= r.velocityScale;
    }
    norm += ep.dynamics.velocityOffset[static_cast<size_t>(voice)];
    h.velocityNorm = std::clamp(norm, 0.0f, 1.0f);
    h.velocity = mapVelocity(voice, h.velocityNorm);
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
            // Crescendo into the last hit of the roll, inside the voice's range.
            const float ramp = 0.55f + 0.45f * static_cast<float>(k) / static_cast<float>(hit.rollCount - 1);
            velocity = mapVelocity(voice, hit.velocityNorm * ramp);
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
            const HitInfo hit = describe(next, v, block.bpm, true);
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
