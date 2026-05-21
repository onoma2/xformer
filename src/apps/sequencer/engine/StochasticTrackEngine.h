#pragma once

#include "TrackEngine.h"
#include "SequenceState.h"
#include "SortedQueue.h"
#include "RecordHistory.h"
#include "StepRecorder.h"
#include "model/StochasticSequence.h"
#include "model/StochasticTrack.h"

class StochasticTrackEngine : public TrackEngine {
public:
    StochasticTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine);
    ~StochasticTrackEngine();

    virtual void reset() override;
    virtual void restart() override;
    virtual void stop() override;
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Stochastic; }

    virtual bool gateOutput(int index) const override {
        return index == 0 ? _gateOutput : _accentOutput;
    }

    virtual float cvOutput(int index) const override {
        return _cvOutput;
    }

    virtual void changePattern() override {
        _sequence = &(_stochasticTrack.sequence(pattern()));
    }

    int currentStep() const { return _patternIndex; }
    bool activity() const { return _activity; }
    int lastDegree() const { return _lastDegree; }
    int lastDurationIndex() const { return _lastDurationIndex; }
    uint16_t loopCycleCount() const { return _loopCycleCount; }
    int jumpRegister() const { return _jumpRegister; }
    uint8_t sleepRemaining() const { return _sleepRemaining; }
    void renewLoopSources() { refreshLoopSources(); }
    void renewRhythm();
    void renewMelody();

    // Max children per parent burst — matches burstCount LUT {2, 3, 4, 5}.
    static constexpr int kMaxChildren = 5;

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
    void initLockedSteps();
    void freeLockedSteps();

    void triggerStep(uint32_t tick, uint32_t divisor);
    void resetMeasure();
    void refreshLoopSources();

    const StochasticTrack &_stochasticTrack;
    const StochasticSequence *_sequence = nullptr;
    Random _rng;

    // Engine state
    uint8_t _patternIndex = 0;
    uint32_t _relativeTick = 0;
    uint8_t _sleepRemaining = 0;
    uint16_t _loopCycleCount = 0;
    int _lastDegree = -1;
    int _lastDurationIndex = 0;
    int _jumpRegister = 0;
    int _lastFreeStepIndex = -1;
    bool _patternCycleEnded = false;
    uint32_t _eventElapsed = 0;
    uint32_t _eventDuration = 0;

    bool _activity = false;
    bool _gateOutput = false;
    bool _accentOutput = false;
    bool _slideActive = false;
    float _cvOutput = 0.f;
    float _cvOutputTarget = 0.f;

    struct Gate {
        uint32_t tick;
        bool gate;
        bool accent;
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

    // Evaluated Event Lock data
    struct LockedParentEvent {
        float cv;
        uint32_t durationTicks;
        bool rest;
        bool legato;
        bool slide;
        bool accent;
        bool valid;

        struct LockedChild {
            float cv;
            uint32_t tickOffset;
            uint32_t gateTicks;
            bool slide;
            bool accent;
            bool valid;
        } children[kMaxChildren];
    };

    LockedParentEvent *_lockedParents = nullptr;
};

static_assert(sizeof(StochasticTrackEngine) <= 512, "StochasticTrackEngine too large");
