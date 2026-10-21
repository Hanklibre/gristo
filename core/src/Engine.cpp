#include "gristo/Engine.h"

#include "gristo/Random.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace gristo {

namespace {
constexpr float kMinVelocity = 1.0f / 127.0f;
constexpr int kMaxStepsPerBlock = 4096;
constexpr double kSlideOverlapBeats = 0.02; // legato overlap for slides (~10 ms at 120 bpm)
constexpr double kSlideHoldBeats = 4.0;     // a slide note waits up to a bar for the next note
} // namespace

Engine::Engine(const DrumMapData& data) : map_(data), generator_(map_)
{
    pending_.reserve(256);
}

void Engine::reset()
{
    pending_.clear();
    sounding_.fill(false);
    for (ArpState& st : arps_)
    {
        st.sounding.fill(false);
        st.needsResync = true;
        st.lastNote = -1;
        st.lastSlide = false;
    }
    chord_ = ChordState {};
    hitsMemoCycle_ = -1;
    needsResync_ = true;
}

void Engine::setArpInput(int arp, const int* notes, int count)
{
    ArpState& st = arps_[static_cast<size_t>(arp)];
    st.heldCount = std::clamp(count, 0, kMaxHeldNotes);
    std::copy(notes, notes + st.heldCount, st.held.begin());
}

int Engine::resetSteps(const ArpParams& p)
{
    const int bars = p.resetBars >= 16 ? 16 : p.resetBars >= 8 ? 8 : p.resetBars >= 4 ? 4 : p.resetBars >= 2 ? 2 : 1;
    return bars * kStepsPerBar;
}

bool Engine::arpActive(int arp) const
{
    const ArpParams& ap = params_.arps[static_cast<size_t>(arp)];
    return ap.enabled && std::any_of(ap.triggers.begin(), ap.triggers.end(), [](bool t) { return t; });
}

bool Engine::arpTrigger(int arp, std::int64_t step, double bpm, HitInfo& best, double& earliest) const
{
    // Simultaneous trigger hits advance the arp once; the loudest hit wins.
    const ArpParams& ap = params_.arps[static_cast<size_t>(arp)];
    bool found = false;
    earliest = std::numeric_limits<double>::infinity();
    for (int v = 0; v < kNumVoices; ++v)
    {
        if (!ap.triggers[static_cast<size_t>(v)])
            continue;
        HitInfo h = describe(step, v, bpm, true);
        earliest = std::min(earliest, h.timeBeats);
        // The arp follows the drum pattern as written: hits the drums' Probability
        // skipped still step it (the arp's own Probability decides its notes).
        if (h.dropped)
        {
            h.on = true;
            h.velocityNorm = h.accent ? 1.0f : kNormalVelocity;
        }
        if (h.on && (!found || h.velocityNorm > best.velocityNorm))
        {
            best = h;
            found = true;
        }
    }
    return found;
}

void Engine::resyncArp(int arp, std::int64_t firstStep, double bpm)
{
    // Recount the trigger hits since the last reset bar, exactly as continuous
    // playback would have counted them.
    ArpState& st = arps_[static_cast<size_t>(arp)];
    const int resetLen = resetSteps(params_.arps[static_cast<size_t>(arp)]);
    const std::int64_t cycleStart = floorDiv(firstStep, resetLen) * resetLen;
    st.count = 0;
    for (std::int64_t s = cycleStart; s < firstStep; ++s)
    {
        HitInfo h;
        double earliest = 0.0;
        if (arpTrigger(arp, s, bpm, h, earliest))
            st.count += h.rollCount;
    }
    st.nextStep = firstStep;
    st.lastNote = -1;
    st.lastSlide = false;
    st.needsResync = false;
}

const ArpPattern& Engine::arpPattern(int arp, std::int64_t cycle)
{
    ArpState& st = arps_[static_cast<size_t>(arp)];
    const ArpParams& ap = params_.arps[static_cast<size_t>(arp)];
    if (st.patternCycle != cycle || st.patternSeed != ap.seed || st.patternMutate != ap.mutate)
    {
        st.pattern = Arp::pattern(ap.seed, ap.mutate, std::max<std::int64_t>(0, cycle));
        st.patternCycle = cycle;
        st.patternSeed = ap.seed;
        st.patternMutate = ap.mutate;
    }
    return st.pattern;
}

void Engine::playArp(int arp, std::int64_t step, std::int64_t k, std::int64_t cycle, double beats, float velocityNorm)
{
    ArpState& st = arps_[static_cast<size_t>(arp)];
    // Modulation lanes are read at the drum step that triggered the arp.
    const ArpParams ap = hasModulation_ ? effectiveParams(step).arps[static_cast<size_t>(arp)] : params_.arps[static_cast<size_t>(arp)];
    const int voice = kArpVoiceBase + arp;
    ArpPool pool;
    if (ap.followChords && chordsActive())
    {
        // The chord sequencer's current chord is this arp's "held" chord.
        const Chord chord = chordAt(step);
        pool = Arp::pool(ap, chord.notes.data(), chord.count);
    }
    else
    {
        pool = Arp::pool(ap, st.held.data(), st.heldCount);
    }
    static const ArpPattern kNoPattern {};
    const ArpPattern& pattern = ap.mode == ArpMode::Generate ? arpPattern(arp, cycle) : kNoPattern;
    ArpNote n = Arp::noteFor(ap, pool, pattern, k, cycle);
    if (n.play && (ap.probability < 1.0f || ap.probabilityRandom > 0.0f))
    {
        // Same idea as the drums' Probability: hashed per step, so re-renders match.
        const auto a = static_cast<std::uint32_t>(arp);
        float chance = ap.probability;
        if (ap.probabilityRandom > 0.0f)
            chance += rnd::bipolar(rnd::hash(params_.pattern.seed, 0xA9B0u + a, floorDiv(step, kStepsPerBar))) * 0.5f * ap.probabilityRandom;
        if (rnd::unit(rnd::hash(params_.pattern.seed, 0xA9C0u + a, step, k)) >= std::clamp(chance, 0.0f, 1.0f))
            n.play = false;
    }
    const int note = Arp::outputNote(n.note + ap.transpose, ap.shift);

    // End the previous note: slides overlap into a new pitch (legato), everything
    // else is cut at the new onset so the line stays monophonic.
    auto endPrevious = [&](bool slideInto) {
        if (st.lastNote < 0)
            return;
        for (Pending& p : pending_)
            if (!p.noteOn && p.voice == voice && p.note == st.lastNote)
                p.beats = slideInto ? beats + kSlideOverlapBeats : std::min(p.beats, beats);
    };

    if (!n.play)
    {
        if (st.lastSlide)
            endPrevious(false); // a rest after a slide: don't leave the note hanging
        st.lastSlide = false;
        return;
    }
    endPrevious(st.lastSlide && st.lastNote != note);

    const float lo = std::clamp(ap.velMin, 0.0f, 1.0f);
    const float hi = std::clamp(ap.velMax, lo, 1.0f);
    const float norm = n.accent ? 1.0f : std::clamp(velocityNorm + ap.velocityOffset, 0.0f, 1.0f);
    const float velocity = std::clamp(lo + (hi - lo) * norm, kMinVelocity, 1.0f);
    const double gate = std::max(0.02, static_cast<double>(ap.gateSteps) / kStepsPerBeat);
    const double off = beats + (n.slide ? std::max(gate, kSlideHoldBeats) : gate);

    pending_.push_back({ beats, voice, velocity, true, note });
    pending_.push_back({ off, voice, 0.0f, false, note });
    st.lastNote = note;
    st.lastSlide = n.slide;
}

// ---------------------------------------------------------------------------------
// Chord sequencer

bool Engine::chordsActive() const
{
    return params_.chords.enabled && Chords::usedCount(params_.chords) > 0;
}

std::int64_t Engine::clockHitsBefore(std::int64_t step) const
{
    // Hits of the clock voice from the reset bar up to (not including) `step`.
    const ChordParams& cp = params_.chords;
    const int resetLen = std::clamp(cp.resetBars, 1, 16) * kStepsPerBar;
    const std::int64_t cycle = floorDiv(step, resetLen);
    const std::int64_t cycleStart = cycle * resetLen;
    std::int64_t from = cycleStart, hits = 0;
    if (hitsMemoCycle_ == cycle && hitsMemoStep_ <= step)
    {
        from = hitsMemoStep_;
        hits = hitsMemoHits_;
    }
    const int voice = std::clamp(cp.clockVoice, 0, kNumVoices - 1);
    for (std::int64_t s = from; s < step; ++s)
        if (evaluateStep(s)[static_cast<size_t>(voice)].on)
            ++hits;
    hitsMemoCycle_ = cycle;
    hitsMemoStep_ = step;
    hitsMemoHits_ = hits;
    return hits;
}

ChordPosition Engine::chordPositionAt(std::int64_t step) const
{
    if (!chordsActive())
        return {};
    auto& entry = chordCache_[static_cast<size_t>(floorMod(step, kChordCacheSize))];
    if (entry.generation == chordGeneration_ && entry.step == step)
        return entry.pos;
    entry.step = step;
    entry.generation = chordGeneration_;
    entry.pos = computeChordPosition(step);
    return entry.pos;
}

ChordPosition Engine::computeChordPosition(std::int64_t step) const
{
    const ChordParams& cp = params_.chords;
    if (cp.clock == ChordClock::Drums)
    {
        const int voice = std::clamp(cp.clockVoice, 0, kNumVoices - 1);
        const bool hitNow = evaluateStep(step)[static_cast<size_t>(voice)].on;
        return Chords::drumsPosition(cp, step, clockHitsBefore(step) + (hitNow ? 1 : 0));
    }
    return Chords::barsPosition(cp, step);
}

Chord Engine::chordAt(std::int64_t step) const
{
    const ChordPosition pos = chordPositionAt(step);
    if (pos.slot < 0)
        return {};
    return Chords::build(params_.chords, Chords::played(params_.chords, pos));
}

void Engine::releaseHeldChord(double beats)
{
    if (!chord_.haveChord)
        return;
    for (int i = 0; i < chord_.held.count; ++i)
        pending_.push_back({ beats, kChordVoice, 0.0f, false, chord_.held.notes[static_cast<size_t>(i)] });
    chord_.haveChord = false;
}

void Engine::processChords(double ppqStart, double ppqEnd, double bpm)
{
    const ChordParams& cp = params_.chords;
    ChordState& st = chord_;
    if (!chordsActive() || cp.play == ChordPlay::Off)
    {
        releaseHeldChord(ppqStart);
        st.needsResync = true;
        return;
    }
    if (st.needsResync)
    {
        st.nextStep = static_cast<std::int64_t>(std::ceil(ppqStart * kStepsPerBeat - 1e-9));
        st.haveChord = false;
        st.needsResync = false;
    }
    const float lo = std::clamp(cp.velMin, 0.0f, 1.0f);
    const float hi = std::clamp(cp.velMax, lo, 1.0f);
    auto outNotes = [&cp](const Chord& c) {
        Chord out = c;
        for (int i = 0; i < out.count; ++i)
            out.notes[static_cast<size_t>(i)] = Arp::outputNote(c.notes[static_cast<size_t>(i)], cp.shift);
        return out;
    };

    for (int guard = 0; guard < kMaxStepsPerBlock; ++guard)
    {
        const std::int64_t s = st.nextStep;
        if (static_cast<double>(s) / kStepsPerBeat - kMaxEarlyBeats >= ppqEnd)
            break;

        if (cp.play == ChordPlay::Held)
        {
            const double beats = gridAt(s);
            if (beats >= ppqEnd)
                break;
            const ChordPosition pos = chordPositionAt(s);
            const Chord chord = pos.slot >= 0 ? outNotes(Chords::build(cp, Chords::played(cp, pos))) : Chord {};
            const bool edited = st.haveChord && (chord.count != st.held.count
                                                 || !std::equal(chord.notes.begin(), chord.notes.begin() + chord.count, st.held.notes.begin()));
            if (!st.haveChord || pos.serial != st.lastSerial || edited)
            {
                releaseHeldChord(beats);
                const float velocity = std::clamp(lo + (hi - lo) * 0.7f, kMinVelocity, 1.0f);
                for (int i = 0; i < chord.count; ++i)
                    pending_.push_back({ beats, kChordVoice, velocity, true, chord.notes[static_cast<size_t>(i)] });
                st.held = chord;
                st.haveChord = chord.count > 0;
                st.lastSerial = pos.serial;
            }
        }
        else // Stabs: the chord plays on hits of the stab voices
        {
            releaseHeldChord(ppqStart);
            HitInfo best;
            bool found = false;
            double earliest = std::numeric_limits<double>::infinity();
            for (int v = 0; v < kNumVoices; ++v)
            {
                if (!cp.stabTriggers[static_cast<size_t>(v)])
                    continue;
                const HitInfo h = describe(s, v, bpm, true);
                earliest = std::min(earliest, h.timeBeats);
                if (h.on && (!found || h.velocityNorm > best.velocityNorm))
                {
                    best = h;
                    found = true;
                }
            }
            if (earliest >= ppqEnd)
                break;
            if (found)
            {
                const Chord chord = outNotes(chordAt(s));
                // A new stab cuts the previous one.
                for (Pending& p : pending_)
                    if (!p.noteOn && p.voice == kChordVoice)
                        p.beats = std::min(p.beats, best.timeBeats);
                const float velocity = std::clamp(lo + (hi - lo) * best.velocityNorm, kMinVelocity, 1.0f);
                const double off = best.timeBeats + std::max(0.02, static_cast<double>(cp.stabGateSteps) / kStepsPerBeat);
                for (int i = 0; i < chord.count; ++i)
                {
                    pending_.push_back({ best.timeBeats, kChordVoice, velocity, true, chord.notes[static_cast<size_t>(i)] });
                    pending_.push_back({ off, kChordVoice, 0.0f, false, chord.notes[static_cast<size_t>(i)] });
                }
            }
        }
        ++st.nextStep;
    }
}

void Engine::processArp(int arp, double ppqStart, double ppqEnd, double bpm)
{
    ArpState& st = arps_[static_cast<size_t>(arp)];
    if (!arpActive(arp))
    {
        st.needsResync = true;
        return;
    }
    if (st.needsResync)
        resyncArp(arp, static_cast<std::int64_t>(std::ceil(ppqStart * kStepsPerBeat - 1e-9)), bpm);

    const int resetLen = resetSteps(params_.arps[static_cast<size_t>(arp)]);
    for (int guard = 0; guard < kMaxStepsPerBlock; ++guard)
    {
        const std::int64_t s = st.nextStep;
        if (static_cast<double>(s) / kStepsPerBeat - kMaxEarlyBeats >= ppqEnd)
            break;
        HitInfo hit;
        double earliest = 0.0;
        const bool found = arpTrigger(arp, s, bpm, hit, earliest);
        if (earliest >= ppqEnd)
            break;
        if (floorMod(s, resetLen) == 0)
            st.count = 0; // reset bar: the phrase starts over
        if (found)
        {
            const std::int64_t cycle = floorDiv(s, resetLen);
            for (int r = 0; r < hit.rollCount; ++r)
            {
                const float ramp = hit.rollCount > 1 ? 0.55f + 0.45f * static_cast<float>(r) / static_cast<float>(hit.rollCount - 1) : 1.0f;
                playArp(arp, s, st.count, cycle, hit.timeBeats + r * hit.rollSpacingBeats, hit.velocityNorm * ramp);
                ++st.count;
            }
        }
        ++st.nextStep;
    }
}

void Engine::setParams(const EngineParams& p)
{
    params_ = p;
    hasModulation_ = false;
    shuffleModulated_ = false;
    ++chordGeneration_; // positions depend on the pattern and the chord settings
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
    for (int a = 0; a < kNumArps; ++a)
    {
        ArpParams& ap = e.arps[static_cast<size_t>(a)];
        auto t = [a](int which) { return mod::arpTarget(a, which); };
        auto addInt = [&](int base, int target, int lo, int hi) {
            return static_cast<int>(std::lround(add(static_cast<float>(base), target, static_cast<float>(lo), static_cast<float>(hi))));
        };
        ap.density = add(ap.density, t(mod::kArpDensity), 0.0f, 1.0f);
        ap.accent = add(ap.accent, t(mod::kArpAccent), 0.0f, 1.0f);
        ap.slide = add(ap.slide, t(mod::kArpSlide), 0.0f, 1.0f);
        ap.gateSteps = add(ap.gateSteps, t(mod::kArpGate), 0.25f, 8.0f);
        ap.octaves = addInt(ap.octaves, t(mod::kArpOctaves), 1, 4);
        ap.length = addInt(ap.length, t(mod::kArpLength), 1, kMaxArpSteps);
        ap.transpose = addInt(ap.transpose, t(mod::kArpTranspose), -12, 12);
        ap.velocityOffset = acc[static_cast<size_t>(t(mod::kArpVelocity))];
        ap.cutoffOffset = acc[static_cast<size_t>(mod::arpCutoffTarget(a))];
        ap.probability = add(ap.probability, mod::arpProbTarget(a), 0.0f, 1.0f);
    }
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
    for (int a = 0; a < kNumArps; ++a)
    {
        ArpState& st = arps_[static_cast<size_t>(a)];
        for (int n = 0; n < 128; ++n)
            if (st.sounding[static_cast<size_t>(n)])
                out.push_back({ 0, kArpVoiceBase + a, 0.0f, false, n });
        st.sounding.fill(false);
        st.lastNote = -1;
        st.lastSlide = false;
        st.needsResync = true;
    }
    for (int n = 0; n < 128; ++n)
        if (chord_.sounding[static_cast<size_t>(n)])
            out.push_back({ 0, kChordVoice, 0.0f, false, n });
    chord_.sounding.fill(false);
    chord_.haveChord = false;
    chord_.needsResync = true;
    pending_.clear();
}

void Engine::resync(double ppqStart)
{
    hitsMemoCycle_ = -1;
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

    processChords(ppqStart, ppqEnd, block.bpm);
    for (int a = 0; a < kNumArps; ++a)
        processArp(a, ppqStart, ppqEnd, block.bpm);

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
        out.push_back({ sample, p.voice, p.velocity, p.noteOn, p.note });
        if (p.voice < kNumVoices)
            sounding_[static_cast<size_t>(p.voice)] = p.noteOn;
        else if (p.note >= 0 && p.note < 128 && p.voice < kChordVoice)
            arps_[static_cast<size_t>(p.voice - kArpVoiceBase)].sounding[static_cast<size_t>(p.note)] = p.noteOn;
        else if (p.note >= 0 && p.note < 128)
            chord_.sounding[static_cast<size_t>(p.note)] = p.noteOn;
    }
    pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(emitted));
}

} // namespace gristo
