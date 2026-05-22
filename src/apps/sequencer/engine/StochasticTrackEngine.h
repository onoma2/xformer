#pragma once

#include "TrackEngine.h"
#include "SequenceState.h"
#include "SortedQueue.h"
#include "RecordHistory.h"
#include "StepRecorder.h"
#include "StochasticCache.h"
#include "model/StochasticSequence.h"
#include "model/StochasticTrack.h"

class StochasticTrackEngine : public TrackEngine {
public:
    StochasticTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine);

    virtual void reset() override;
    virtual void restart() override;
    virtual void stop() override;
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Stochastic; }

    virtual bool gateOutput(int index) const override {
        // No second gate jack on this device; index ignored.
        return _gateOutput;
    }

    virtual float cvOutput(int index) const override {
        return _cvOutput;
    }

    virtual void changePattern() override {
        // Engine owns the right to write to the sequence (Loop tape regeneration,
        // mutation/permutation, validity flags). See ownership contract in
        // .tasks/stochastic-track-port/PHASE13-FEATURES-PLAN.md.
        _sequence = &(_stochasticTrack.sequence(pattern()));

        // In-flight queue entries targeted the previous sequence's timing/CV;
        // discard them on pattern switch.
        _gateQueue.clear();
        _cvQueue.clear();
        _patternCycleEnded = false;
        _lastEventValid = false;
        clearDirectHistory();
        // Phase 14B Patch B: cache is per-track, so a pattern switch leaves
        // stale rank data behind. Rebuild from the new pattern's events.
        refreshCache();
    }

    int currentStep() const { return _patternIndex; }
    bool activity() const { return _activity; }

    // Phase 14B follow-on (2026-05-22): immediate transport sync after a
    // model-level window edit (setFirst / setLast / setSize). Without this,
    // engine state — _patternIndex, queued gate/CV events, cached ranks —
    // can still reflect the old window until the next event-boundary trigger,
    // so a Last shrink "feels like it still honors slot 31." Callers from
    // edit paths must invoke this when the active stochastic track's window
    // changes. See engine/StochasticTrackEngine.cpp:syncWindowEdit body.
    void syncWindowEdit();
    int lastDegree() const { return _lastDegree; }
    int lastDurationIndex() const { return _lastDurationIndex; }
    uint16_t loopCycleCount() const { return _loopCycleCount; }
    uint16_t loopCycleCountMelody() const { return _loopCycleCountMelody; }
    int jumpRegister() const { return _jumpRegister; }
    uint8_t sleepRemaining() const { return _sleepRemaining; }
    void renewLoopSources() { refreshLoopSources(); }
    void renewRhythm();
    void renewMelody();

    // Max children per parent burst — matches burstCount LUT {2, 3, 4, 5}.
    static constexpr int kMaxChildren = 5;

    // Recent evaluated parent events for Direct hero drawing. This is UI truth,
    // not source material: only what just reached the scheduler is recorded.
    static constexpr int kDirectHistoryMax = 12;
    struct DirectHistoryEvent {
        float cv = 0.f;
        uint8_t children = 0;
        bool rest = false;
        bool gate = false;
    };
    uint8_t directHistoryCount() const;
    DirectHistoryEvent directHistoryEvent(int age) const;

    // Patience boredom indicator: Poisson CDF probability of regen at the
    // given loop count and patience knob (0..100). Returns 0.0 when knob = 100
    // (off sentinel — patience disabled, never regenerates).
    static float patienceProbability(uint32_t loops, int patience);

    // Linear "tension building" meter scaled to expected loops (λ). Reaches
    // 1.0 at the typical regen point so the on-screen bar visibly fills before
    // the Poisson roll fires. Returns 0.0 when patience knob = 100 (off).
    static float patienceMeter(uint32_t loops, int patience);

    // V5 duration LUT as multipliers of the sequence divisor, sorted descending.
    // ticks = (divisor * num) / den. Labels assume divisor = 1/16:
    //   slot 0: ×8     → 1/2
    //   slot 1: ×4     → 1/4
    //   slot 2: ×3     → 3/16
    //   slot 3: ×2     → 1/8
    //   slot 4: ×4/3   → 1/8T
    //   slot 5: ×1     → 1/16 (= divisor)
    //   slot 6: ×2/3   → 1/16T
    //   slot 7: ×1/2   → 1/32
    // Whole table scales with the sequence's clock divisor.
    struct DurationFraction { uint16_t num; uint16_t den; };
    static DurationFraction getDurationFraction(int index) {
        static const DurationFraction lut[] = {
            { 8, 1 }, { 4, 1 }, { 3, 1 }, { 2, 1 },
            { 4, 3 }, { 1, 1 }, { 2, 3 }, { 1, 2 },
        };
        if (index < 0) index = 0;
        if (index > 7) index = 7;
        return lut[index];
    }

    // Backward-compatibility helper for any call site that still wants ticks
    // at PPQN=192 assuming divisor = 1/16 (legacy reference).
    static uint32_t getDurationMultiplier(int index) {
        auto f = getDurationFraction(index);
        return (48u * f.num) / f.den;
    }

private:
    // ---- Patch 2 helpers (Phase 13 Batch 0) -----------------------------------
    // Local mutable / const accessors for the owned sequence and track. These
    // make the ownership contract visible at call sites and give one place to
    // assert pointer validity. Mutable overloads are used by engine writes
    // (Loop tape regen, mutation, validity flag writes); const overloads for
    // read paths.
    StochasticSequence &sequence()             { return *_sequence; }
    const StochasticSequence &sequence() const { return *_sequence; }
    StochasticTrack &stochasticTrack()             { return _stochasticTrack; }
    const StochasticTrack &stochasticTrack() const { return _stochasticTrack; }

    void triggerStep(uint32_t tick, uint32_t divisor);
    void resetMeasure();
    void refreshLoopSources();
    // Phase 12: per-domain Poisson patience roll. Fired from pattern-cycle wrap
    // AND from forced reset-measure boundary so both count toward the patience
    // counters. Each domain rolls independently against its own counter.
    void rollPatience();

    // ---- Patch 2 intent-named helpers ----------------------------------------
    // ensureLoopSources: if Loop domain is marked invalid, regenerate the
    // source loop tape. No-op for Live domains. Called from triggerStep.
    void ensureLoopSources(const Scale &scale, int rootNote);
    // writeLiveRhythmShadow / writeLiveMelodyShadow: Live mode writes the
    // just-generated event back into source tape so the loop-tape UI
    // visualizes recent activity. This is the mid-audio writeback flagged in
    // docs/stoch-review.md finding #1 — kept for behavior parity in Patch 2;
    // may relocate to engine-owned shadow in Patch 3.
    void writeLiveRhythmShadow(int readIndex, const StochasticSourceEvent &rhythm);
    void writeLiveMelodyShadow(int readIndex, const StochasticSourceEvent &melody);
    // Phase 14B Patch B: rebuild the engine-side cache (gStochasticCaches[idx])
    // from the current event tape. Called after every event-write path so the
    // cache rank is always in sync with the audible content. Pure shadow until
    // Patch C — only the mask filter reads from it.
    void refreshCache();
    // Lock cache removed 2026-05-22. The heap-allocated LockedParentEvent[]
    // colliding with the stack caused crashes at ≥2 stochastic tracks. The
    // engine no longer captures evaluated output; the `_lock` flag on
    // StochasticTrack remains as a UI placeholder and is ignored here. See
    // `.tasks/stochastic-track-port/LOCK-DESIGN-DEFERRED.md` for the deferred
    // redesign — possibly absorbed by the fractal-track trunk substrate.
    void clearDirectHistory();
    void recordDirectHistory(float cv, bool rest, bool gate, uint8_t children);

    // Ownership contract (Patch 1 / Phase 13 Batch 0):
    //   StochasticSequence    — owns pattern material (source loop tape, ticket
    //                           weights, masks, rotations, validity flags).
    //   StochasticTrack       — owns track-global controls (Lock toggle, divisor,
    //                           clock multiplier, scale, octave, transpose, slide
    //                           time, fill mode).
    //   StochasticTrackEngine — owns runtime playback state (queues, locked
    //                           evaluated history, repeat cache, patience counters,
    //                           jump register, sleep counter).
    //
    // The engine has the right to mutate sequence material (Loop tape regeneration,
    // mutation, permutation, validity flags) and track state where applicable.
    // Storing these as non-const references makes that ownership explicit and
    // removes the const_cast trick that previously hid the write boundary.
    // See `.tasks/stochastic-track-port/PHASE13-FEATURES-PLAN.md`.
    StochasticTrack &_stochasticTrack;
    StochasticSequence *_sequence = nullptr;
    Random _rng;

    // Engine state
    uint8_t _patternIndex = 0;
    uint32_t _relativeTick = 0;
    uint8_t _sleepRemaining = 0;
    uint16_t _loopCycleCount = 0;          // legacy alias kept for indicator path (== rhythm count)
    uint16_t _loopCycleCountMelody = 0;
    // Phase 12 Mask/Tilt instant re-rank cache. Engine re-runs generateMaskRanks
    // when tilt, size, or the playback window (first/last) changes — or on
    // Mutate fire. Ranks are window-local so first/last shifts reshape ranks.
    int8_t _lastAppliedTilt = 0;
    uint8_t _lastAppliedSize = 0;
    uint8_t _lastAppliedFirst = 0;
    uint8_t _lastAppliedLast = 0;
    int _lastDegree = -1;
    int _lastDurationIndex = 0;
    int _jumpRegister = 0;
    int _lastFreeStepIndex = -1;
    bool _patternCycleEnded = false;
    uint32_t _eventElapsed = 0;
    uint32_t _eventDuration = 0;

    bool _activity = false;
    bool _gateOutput = false;
    bool _slideActive = false;
    float _cvOutput = 0.f;
    float _cvOutputTarget = 0.f;

    // Repeat cache — Phase 12 reuse of last-emitted event on Bernoulli hit.
    StochasticSourceEvent _lastEvent;
    bool _lastEventValid = false;

    struct Gate {
        uint32_t tick;
        bool gate;
    };

    struct GateCompare {
        bool operator()(const Gate &a, const Gate &b) {
            return a.tick < b.tick;
        }
    };

    SortedQueue<Gate, 16, GateCompare> _gateQueue;

    struct Cv {
        uint32_t tick;
        float cv;
        bool slide;
    };

    struct CvCompare {
        bool operator()(const Cv &a, const Cv &b) {
            return a.tick < b.tick;
        }
    };

    SortedQueue<Cv, 16, CvCompare> _cvQueue;

    // Phase 14B engine cache — playback-ready CV/gate cells materialized from
    // the event tape on every event write. Mask filter reads deterministic
    // rank from here instead of event.densityRank() (which had random noise).
    //
    // Lives inside the engine (not as a file-scope CCMRAM global like the old
    // gStochasticCaches[]) because the Container<...> for track engines is
    // already sized to its largest variant — paying the cache's ~448 B as part
    // of StochasticTrackEngine costs ~56 B per slot (912 → 968) instead of an
    // extra 3.6 KB CCMRAM global. See PROJECT.md "Engine gate" section.
    stochastic_cache::Cache _cache{};
};

// Bumped from 512 in Phase 14B to absorb the engine cache (CachedCell[64] +
// CellAux[64] + parentCacheIdx[64] + housekeeping ≈ 456 B). The new ceiling
// keeps Stochastic at or below the previous container-sizing engine
// (TeletypeTrackEngine = 912 B per PROJECT.md), so the container slot growth
// is at most ~56 B × 8 tracks ≈ 448 B in CCMRAM.
static_assert(sizeof(StochasticTrackEngine) <= 1024, "StochasticTrackEngine too large");
