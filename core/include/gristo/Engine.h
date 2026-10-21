#pragma once

#include "gristo/Arp.h"
#include "gristo/Chords.h"
#include "gristo/Groove.h"
#include "gristo/Humanizer.h"
#include "gristo/Modulation.h"
#include "gristo/PatternGenerator.h"

#include <vector>

namespace gristo {

struct DynamicsParams
{
    // Per-voice output velocity range (0..1 = MIDI 0..127). Every note of the voice is
    // mapped into [velMin, velMax]: accents at velMax, ghosts towards velMin.
    std::array<float, kNumVoices> velMin { 1.0f / 127, 1.0f / 127, 1.0f / 127, 1.0f / 127, 1.0f / 127, 1.0f / 127 };
    std::array<float, kNumVoices> velMax { 1.0f, 1.0f, 108.0f / 127, 115.0f / 127, 115.0f / 127, 115.0f / 127 };
    float dynamics = 0.6f; // master: 0 = flat, 1 = map level strongly shapes velocity
    // Per-voice scale of the master Dynamics (0..2); the product is clamped to 1.
    std::array<float, kNumVoices> voiceDynamics { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    // Added to the normalized velocity before range mapping (set by modulation).
    std::array<float, kNumVoices> velocityOffset {};
};

// Position inside the velocity range of a non-accented note at full strength.
constexpr float kNormalVelocity = 0.65f;

struct EngineParams
{
    PatternParams pattern;
    GrooveParams groove;
    HumanizeParams humanize;
    DynamicsParams dynamics;
    ModulationParams modulation;
    std::array<ArpParams, kNumArps> arps {};
    ChordParams chords;
    float noteLengthBeats = 0.125f;
};

// Output "voices" 0..5 are the drums; kArpVoiceBase + a is arp a; then the chords.
constexpr int kArpVoiceBase = kNumVoices;
constexpr int kChordVoice = kArpVoiceBase + kNumArps;

// Fully resolved description of one voice at one step.
struct HitInfo
{
    bool on = false;
    bool accent = false;
    bool dropped = false;    // removed by Probability this time round
    float level = 0.0f;
    float velocity = 0.0f;   // 0..1, final (inside the voice's velocity range)
    float velocityNorm = 0.0f; // 0..1 position inside the velocity range
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
    int note = -1;         // arps: the MIDI note; drums: -1 (the plugin maps voice -> note)
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

    void setParams(const EngineParams& p);
    const EngineParams& params() const { return params_; }
    const DrumMap& map() const { return map_; }

    HitInfo describe(std::int64_t step, int voice, double bpm, bool withHumanize = true) const;
    StepHits evaluateStep(std::int64_t step) const
    {
        // Only build the modulated copy when lanes are routed (it's a big struct).
        return hasModulation_ ? generator_.evaluate(effectiveParams(step).pattern, step) : generator_.evaluate(params_.pattern, step);
    }

    // Parameters in effect at a step: the base parameters plus all modulation lanes.
    // Shuffle modulation is sampled at the start of the groove cell so positions stay
    // monotonic.
    EngineParams effectiveParams(std::int64_t step) const;

    // Sum of (amount x lane value) routed to a target at a step (unscaled, -4..4).
    float modulationAt(int target, std::int64_t step) const;

    // Maps a normalized velocity into a voice's velocity range.
    float mapVelocity(int voice, float normalized) const;

    // Appends this block's events to `out`, sorted by sample offset.
    void process(const BlockInfo& block, std::vector<OutputEvent>& out);

    // Forget transport state (call on prepare / reset). Pending note-offs are dropped.
    void reset();

    // Notes currently feeding arp `arp` (held or latched on MIDI in), in press order.
    void setArpInput(int arp, const int* notes, int count);
    // Advances since the arp's last reset (for the UI).
    std::int64_t arpCount(int arp) const { return arps_[static_cast<size_t>(arp)].count; }

    // Resolve the arp step counter / pattern without playing (used by tests and UI).
    static int resetSteps(const ArpParams& p);

    // Where the chord progression is at a step (slot -1: chords off or empty).
    ChordPosition chordPositionAt(std::int64_t step) const;
    // The chord sounding at a step (Mutate applied), empty if none.
    Chord chordAt(std::int64_t step) const;

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
        int note = -1;
    };

    struct ArpState
    {
        std::int64_t nextStep = 0;
        std::int64_t count = 0;
        bool needsResync = true;
        int lastNote = -1;
        bool lastSlide = false;
        std::array<bool, 128> sounding {};
        std::array<int, kMaxHeldNotes> held {};
        int heldCount = 0;
        std::int64_t patternCycle = -1;
        std::uint32_t patternSeed = 0;
        float patternMutate = -1.0f;
        ArpPattern pattern;
    };

    struct ChordState
    {
        std::int64_t nextStep = 0;
        bool needsResync = true;
        std::int64_t lastSerial = 0;
        bool haveChord = false;
        Chord held;                 // notes of the held chord (MIDI out notes)
        std::array<bool, 128> sounding {};
    };

    static float normalizedVelocity(const EngineParams& ep, const VoiceHit& hit, int voice);
    bool chordsActive() const;
    std::int64_t clockHitsBefore(std::int64_t step) const;
    ChordPosition computeChordPosition(std::int64_t step) const;
    void releaseHeldChord(double beats);
    void processChords(double ppqStart, double ppqEnd, double bpm);
    double gridAt(std::int64_t step) const;
    void schedule(const HitInfo& hit, int voice);
    void flushNoteOffs(std::vector<OutputEvent>& out);
    void resync(double ppqStart);

    bool arpActive(int arp) const;
    bool arpTrigger(int arp, std::int64_t step, double bpm, HitInfo& best, double& earliest) const;
    void resyncArp(int arp, std::int64_t firstStep, double bpm);
    void processArp(int arp, double ppqStart, double ppqEnd, double bpm);
    void playArp(int arp, std::int64_t step, std::int64_t k, std::int64_t cycle, double beats, float velocityNorm);
    const ArpPattern& arpPattern(int arp, std::int64_t cycle);

    DrumMap map_;
    PatternGenerator generator_;
    EngineParams params_;
    bool hasModulation_ = false;
    bool shuffleModulated_ = false;

    std::vector<Pending> pending_;
    std::array<std::int64_t, kNumVoices> nextStep_ {};
    std::array<bool, kNumVoices> sounding_ {};
    std::array<ArpState, kNumArps> arps_ {};
    ChordState chord_;
    // Drums-clocked chords: running count of clock hits (a cache, so lookups stay cheap).
    mutable std::int64_t hitsMemoCycle_ = -1, hitsMemoStep_ = 0, hitsMemoHits_ = 0;
    // Chord position per step for the current parameters: chords and both arps ask
    // about the same steps, so each is worked out once.
    static constexpr int kChordCacheSize = 512;
    struct ChordCacheEntry
    {
        std::int64_t step = 0;
        std::uint32_t generation = 0;
        ChordPosition pos;
    };
    mutable std::array<ChordCacheEntry, kChordCacheSize> chordCache_ {};
    std::uint32_t chordGeneration_ = 1;
    double expectedPpq_ = 0.0;
    bool needsResync_ = true;
};

} // namespace gristo
