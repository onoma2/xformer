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

private:
    void initLockedSteps();
    void freeLockedSteps();

    void triggerStep(uint32_t tick, uint32_t divisor);
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
    int _jumpRegister = 0;
    bool _patternCycleEnded = false;

    SequenceState _sequenceState;
    SequenceState _linkData;

    bool _prevCondition = false;
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
        } children[4];
    };

    LockedParentEvent *_lockedParents = nullptr;
};

static_assert(sizeof(StochasticTrackEngine) <= 512, "StochasticTrackEngine too large");
