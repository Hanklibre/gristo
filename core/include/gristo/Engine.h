#pragma once

#include "gristo/Groove.h"
#include "gristo/Humanizer.h"
#include "gristo/PatternGenerator.h"

#include <vector>

namespace gristo {

struct DynamicsParams
{
    // Stolperbeats-style velocity boundaries: accents play at velHigh, regular hits
    // at velLow, and ghost-level hits (just above the Fill threshold) below that.
    std::array<float, kNumVoices> velLow { 0.62f, 0.6f, 0.5f, 0.55f, 0.55f, 0.55f };
    std::array<float, kNumVoices> velHigh { 1.0f, 1.0f, 0.85f, 0.9f, 0.9f, 0.9f };
    float dynamics = 0.6f; // 0 = flat, 1 = map level strongly shapes velocity
};

struct EngineParams
{
    PatternParams pattern;
    GrooveParams groove;
    HumanizeParams humanize;
    DynamicsParams dynamics;
    float noteLengthBeats = 0.125f;
};

// Fully resolved description of one voice at one step.
struct HitInfo
{
    bool on = false;
    bool accent = false;
    bool dropped = false;    // removed by Probability this time round
    float level = 0.0f;
    float velocity = 0.0f;   // 0..1
    double gridBeats = 0.0;  // groove position (no feel / humanize)
    double timeBeats = 0.0;  // final onset
    int rollCount = 1;
    double rollSpacingBeats = 0.0;
};

struct BlockInfo
{
    bool playing = false;
    double ppqStart = 0.0; // beats at the first sample of the block
    double bpm = 120.0;
    double sampleRate = 48000.0;
    int numSamples = 0;
};

struct OutputEvent
{
    int sampleOffset = 0;
    int voice = 0;
    float velocity = 0.0f; // 0 for note-off
    bool noteOn = false;
};

// Pattern generator + groove + humanizer + a sample-accurate scheduler.
//
// Everything is a pure function of (params, absolute step), so the output does not
// depend on block size. The scheduler walks each voice's steps in order and emits a
// step once its (possibly early) onset falls before the end of the current block;
// onsets that are already in the past (rushed notes right after a jump) are emitted
// at the start of the block.
class Engine
{
public:
    explicit Engine(const DrumMapData& data = defaultMapData());

    void setParams(const EngineParams& p) { params_ = p; }
    const EngineParams& params() const { return params_; }
    const DrumMap& map() const { return map_; }

    HitInfo describe(std::int64_t step, int voice, double bpm, bool withHumanize = true) const;
    StepHits evaluateStep(std::int64_t step) const { return generator_.evaluate(params_.pattern, step); }

    // Appends this block's events to `out`, sorted by sample offset.
    void process(const BlockInfo& block, std::vector<OutputEvent>& out);

    // Forget transport state (call on prepare / reset). Pending note-offs are dropped.
    void reset();

    // Largest distance a note can be moved earlier than its straight position.
    static constexpr double kMaxEarlyBeats = 0.5;
    // A block start further than this from the previous block's end is a jump.
    static constexpr double kJumpToleranceBeats = 0.02;

private:
    struct Pending
    {
        double beats;
        int voice;
        float velocity;
        bool noteOn;
    };

    HitInfo describe(const StepHits& hits, std::int64_t step, int voice, double bpm, bool withHumanize) const;
    float velocityFor(const VoiceHit& hit, int voice) const;
    void schedule(const HitInfo& hit, int voice);
    void flushNoteOffs(std::vector<OutputEvent>& out);
    void resync(double ppqStart);

    DrumMap map_;
    PatternGenerator generator_;
    EngineParams params_;

    std::vector<Pending> pending_;
    std::array<std::int64_t, kNumVoices> nextStep_ {};
    std::array<bool, kNumVoices> sounding_ {};
    double expectedPpq_ = 0.0;
    bool needsResync_ = true;
};

} // namespace gristo
