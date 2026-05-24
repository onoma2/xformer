#pragma once

#include "TrackEngine.h"
#include "SequenceState.h"
#include "SortedQueue.h"
#include "RecordHistory.h"
#include "StepRecorder.h"
#include "StochasticCache.h"
#include "StochasticGenerator.h"
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
        _sequence = &(_stochasticTrack.sequence(pattern()));

        // In-flight queue entries targeted the previous sequence's timing/CV;
        // discard them on pattern switch.
        _gateQueue.clear();
        _cvQueue.clear();
        _patternCycleEnded = false;
        _lastStepContentValid = false;
        clearDirectHistory();
        // Cache is per-track and was built for the previous pattern's events;
        // rebuild for the new pattern.
        refreshStepCache();
    }

    int currentStep() const { return _currentStep; }
    bool activity() const { return _activity; }

    // Immediate transport sync after a model-level window edit (setFirst /
    // setSize). Snaps _currentStep, flushes gate/CV queues, rebuilds the
    // cache. Without this, the old window keeps driving playback until the
    // next event-boundary trigger.
    void syncWindowEdit();

    // Rebuild the engine cache from current sequence content. Lighter than
    // syncWindowEdit — no _currentStep snap, no queue flush. Call from
    // edit paths that change cache-baked shaping fields so non-Repeat
    // playback picks up the new shape on the next trigger.
    void refreshStepCacheNow() { refreshStepCache(); }
    int lastDegree() const { return _lastDegree; }
    int lastDurationIndex() const { return _lastDurationIndex; }
    uint16_t loopCycleCount() const { return _loopCycleCount; }
    uint16_t loopCycleCountMelody() const { return _loopCycleCountMelody; }
    int jumpRegister() const { return _jumpRegister; }
    uint8_t sleepRemaining() const { return _sleepRemaining; }
    void renewLoopSources() { refreshLoopSources(); }
    void renewRhythm();
    void renewMelody();

    // Max cluster cells — matches burstCount LUT {2..8}. Sized 8 so Fit-mode
    // clusters can pack up to 8 trap-style hits into one prev_dur.
    static constexpr int kMaxBurst = 8;

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

    // Duration LUT lives in StochasticTypes.h (kStochasticDurationLut + the
    // stochasticDurationFraction() accessor). Existing call sites keep the
    // StochasticTrackEngine::DurationFraction / getDurationFraction API via
    // thin aliases below.
    using DurationFraction = StochasticDurationFraction;
    static DurationFraction getDurationFraction(int index) {
        return stochasticDurationFraction(index);
    }

    // Ticks at PPQN=192 assuming divisor = 1/16 (legacy reference scale).
    static uint32_t getDurationMultiplier(int index) {
        auto f = stochasticDurationFraction(index);
        return (48u * f.num) / f.den;
    }

private:
    // Mutable / const accessors for the owned sequence and track. Mutable
    // overloads are used by engine writes (Loop source regen, mutation,
    // validity flags); const overloads for read paths.
    StochasticSequence &sequence()             { return *_sequence; }
    const StochasticSequence &sequence() const { return *_sequence; }
    StochasticTrack &stochasticTrack()             { return _stochasticTrack; }
    const StochasticTrack &stochasticTrack() const { return _stochasticTrack; }

    // Sequence divisor scaled by clockMultiplier + PPQN conversion. Same
    // value flows to tick() and refreshStepCache().
    uint32_t effectiveDivisor() const;

    void triggerStep(uint32_t tick, uint32_t divisor);
    void resetMeasure();
    void refreshLoopSources();
    // Per-domain Poisson patience roll. Fired from pattern-cycle wrap AND
    // from preempted reset-measure boundary so both count toward the
    // patience counters. Each domain rolls independently.
    void rollPatience();

    // Cycle-end hook bundle: jump octave walk, sleep skip count, patience
    // rolls, mutate. Fired from both natural pattern-cycle wrap and from
    // preempted resetMeasure() so all four advance on the same boundary.
    void rollCycleEndHooks();

    // If a Loop domain is marked invalid, regenerate its source events.
    // No-op for Live domains. Called from triggerStep.
    void ensureLoopSources(const Scale &scale, int rootNote);
    // Live writeback into the events array so the events display reflects
    // the just-played event. Flags a coalesced cache refresh.
    void writeLiveRhythmShadow(int stepIndex, const StochasticStepContent &rhythm);
    void writeLiveMelodyShadow(int stepIndex, const StochasticStepContent &melody);
    // Rebuild the engine cache from current sequence content.
    void refreshStepCache();
    void clearDirectHistory();
    void recordDirectHistory(float cv, bool rest, bool gate, uint8_t children);

    // Ownership:
    //   StochasticSequence    — pattern material (events, ticket weights,
    //                           rotations, validity flags).
    //   StochasticTrack       — track-global controls (divisor, clock
    //                           multiplier, scale, octave, transpose,
    //                           slide time, fill mode).
    //   StochasticTrackEngine — runtime playback state (queues, repeat
    //                           cache, patience counters, jump register,
    //                           sleep counter).
    StochasticTrack &_stochasticTrack;
    StochasticSequence *_sequence = nullptr;
    Random _rng;

    // Engine state
    uint8_t _currentStep = 0;
    uint32_t _relativeTick = 0;
    uint8_t _sleepRemaining = 0;
    uint16_t _loopCycleCount = 0;          // legacy alias kept for indicator path (== rhythm count)
    uint16_t _loopCycleCountMelody = 0;
    // Window-edit detection — Size changes the cache walk extent and forces
    // a refresh; First does not (cells are step-keyed).
    uint8_t _lastAppliedSize = 0;
    uint8_t _lastAppliedFirst = 0;
    int _lastDegree = -1;
    StochasticGenerator::PitchGenState _pitchState{};
    int _lastDurationIndex = 0;
    int _jumpRegister = 0;
    int _lastFreeStepIndex = -1;
    bool _patternCycleEnded = false;
    uint32_t _eventElapsed = 0;
    uint32_t _eventDuration = 0;

    // Coalesce cache refreshes from Live writes. Both writeLiveRhythmShadow
    // and writeLiveMelodyShadow can fire in the same trigger; instead of
    // rebuilding twice, they set this flag and triggerStep rebuilds once
    // at the top of the next call before any cache read.
    bool _stepCacheRefreshPending = false;

    // Phase 16 P10 (2026-05-23): Codex audit follow-up. Knobs consumed inside
    // rebuildStepCache are cache-baked state — when a routing target
    // writes them mid-cycle (no UI involvement, no shaping-edit notify), the
    // cache must rebuild before the next event read. triggerStep snapshots
    // the current shaping-knob set at the top and flags refresh on any drift.
    // Routed-CV invalidation snapshot. Index via ShapingKnob enum so
    // adding a knob is one enum entry + one line in readShapingKnobs().
    // sign of `contour` is folded into uint8 storage — `!=` only cares
    // about identity, not numeric order.
    enum class ShapingKnob : uint8_t {
        NoteDuration, Variation, Burst, GateLength,
        Complexity, Contour, MarblesBias, MarblesSpread,
        BurstCount, BurstRate, BurstHold, Range,
        Count
    };
    static constexpr size_t kShapingKnobCount = size_t(ShapingKnob::Count);
    void readShapingKnobs(uint8_t out[kShapingKnobCount]) const;

    uint8_t _shapingSnapshot[kShapingKnobCount] = {};
    bool    _shapingSnapshotValid = false;

    bool _activity = false;
    bool _gateOutput = false;
    bool _slideActive = false;
    float _cvOutput = 0.f;
    float _cvOutputTarget = 0.f;

    // Repeat path: holds the last-emitted event for replay on a Repeat hit.
    StochasticStepContent _lastStepContent;
    bool _lastStepContentValid = false;

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

    // Playback-ready cells materialized from the events array. Engine reads
    // runtimeSteps[K] for each played step; rebuilt on edit / mutation / renew.
    stochastic_cache::StepCache _stepCache{};
};

// Ceiling sized to absorb the engine cache (RuntimeStep[64] + CellAux[64] +
// housekeeping ≈ 390 B) while staying within the track-engine container
// step. See PROJECT.md "Engine gate".
static_assert(sizeof(StochasticTrackEngine) <= 1024, "StochasticTrackEngine too large");
