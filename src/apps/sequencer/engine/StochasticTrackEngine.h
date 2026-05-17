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

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Stochastic; }

    virtual void reset() override;
    virtual void restart() override;
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual void changePattern() override;

    virtual void monitorMidi(uint32_t tick, const MidiMessage &message) override;
    virtual void clearMidiMonitoring() override;

    virtual const TrackLinkData *linkData() const override { return &_linkData; }

    virtual bool activity() const override { return _activity; }
    virtual bool gateOutput(int index) const override {
        return index == 0 ? _gateOutput : (index == 1 ? _accentOutput : false);
    }
    virtual float cvOutput(int index) const override {
        return index == 0 ? _cvOutput : 0.f;
    }
    virtual float sequenceProgress() const override;

    const StochasticSequence &sequence() const { return *_sequence; }
    bool isActiveSequence(const StochasticSequence &sequence) const { return &sequence == _sequence; }

    int currentStep() const { return _currentStep; }
    int currentRecordStep() const { return _stepRecorder.stepIndex(); }

    void setMonitorStep(int index);

    struct LockedStep {
        float noteValue;
        uint32_t stepLength;
        int8_t gateOffset;
        uint8_t retrigger;
        bool gate;
        bool slide;
        bool accent;
        bool legato;
        bool valid;
    };

    const LockedStep* lockedSteps() const { return _lockedSteps; }
    int lockedStepCount() const { return _lockedStepCount; }

private:
    void triggerStep(uint32_t tick, uint32_t divisor, bool forNextStep);
    void triggerStep(uint32_t tick, uint32_t divisor);
    void recordStep(uint32_t tick, uint32_t divisor);
    int noteFromMidiNote(uint8_t midiNote) const;

    bool fill() const {
        return (_stochasticTrack.fillMuted() || !TrackEngine::mute()) ? TrackEngine::fill() : false;
    }

    void initLockedSteps();
    void freeLockedSteps();

    StochasticTrack &_stochasticTrack;

    TrackLinkData _linkData;

    StochasticSequence *_sequence;
    const StochasticSequence *_fillSequence;

    uint32_t _freeRelativeTick;
    SequenceState _sequenceState;
    int _currentStep;
    int _index;
    bool _prevCondition;

    int _monitorStepIndex = -1;

    RecordHistory _recordHistory;
    bool _monitorOverrideActive = false;
    StepRecorder _stepRecorder;

    bool _activity;
    bool _gateOutput;
    bool _accentOutput;
    float _cvOutput;
    float _cvOutputTarget;
    bool _slideActive;
    unsigned int _currentStageRepeat;

    int _skips;
    int _lastDegree = -1;
    Random _rng;

    // Heap-allocated cache for locked steps
    LockedStep *_lockedSteps = nullptr;
    int _lockedStepCount = 0;

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
};
