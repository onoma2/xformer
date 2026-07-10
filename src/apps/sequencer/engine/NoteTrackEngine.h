#pragma once

#include "Config.h"
#include "TrackEngine.h"
#include "SequenceState.h"
#include "SortedQueue.h"
#include "Groove.h"
#include "RecordHistory.h"
#include "StepRecorder.h"

class NoteTrackEngine final : public TrackEngine {
public:
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // Sequence ID constants for experimental spread-ticks feature
    static constexpr uint8_t MainSequenceId = 0;
    static constexpr uint8_t FillSequenceId = 1;
#endif

    // Gate struct - must be public for testing
    struct Gate {
        uint32_t tick;
        bool gate;
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
        bool shouldTickAccumulator;  // Should this gate tick accumulator when fired?
        uint8_t sequenceId;           // Which sequence's accumulator (0=main, 1=fill)
        float cvTarget;               // Pre-calculated CV for this gate (for accumulator-driven CV)
#endif
        // Length-0 "trig" sentinel: gate-off is wall-clock, not from this queue.
        bool trig;
    };

    struct GateCompare {
        bool operator()(const Gate &a, const Gate &b) {
            return a.tick < b.tick;
        }
    };

    NoteTrackEngine(Engine &engine, const Model &model, Track &track) :
        TrackEngine(engine, model, track),
        _noteTrack(track.noteTrack())
    {
        reset();
    }

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Note; }

    virtual void reset() override;
    virtual void stop() override;
    virtual void restart() override;
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual void changePattern() override;

    virtual void monitorMidi(uint32_t tick, const MidiMessage &message) override;
    virtual void clearMidiMonitoring() override;

    virtual bool activity() const override { return _activity; }
    virtual bool gateOutput(int index) const override { return _gateOutput; }
    virtual float cvOutput(int index) const override { return _cvOutput; }
    virtual float sequenceProgress() const override {
        return _currentStep < 0 ? 0.f : float(_currentStep - _sequence->firstStep()) / (_sequence->lastStep() - _sequence->firstStep());
    }

    const NoteSequence &sequence() const { return *_sequence; }
    bool isActiveSequence(const NoteSequence &sequence) const { return &sequence == _sequence; }

    int currentStep() const { return _currentStep; }
    int currentNoteStep() const { return _noteSequenceState.step(); }
    int currentRecordStep() const { return _stepRecorder.stepIndex(); }

    void setMonitorStep(int index);

    // Pulse count of the audible (rotated) step for a given sequence-state index.
    // The pulse-hold must read this, not the raw unrotated state index. Pure +
    // static so it can be unit-tested directly.
    static int rotatedStepPulseCount(const NoteSequence &sequence, int stateStep,
                                     int firstStep, int lastStep, int rotate);

private:
    void triggerStep(uint32_t tick, uint32_t divisor);
    void recordStep(uint32_t tick, uint32_t divisor);
    int noteFromMidiNote(uint8_t midiNote) const;
    float applyHarmony(float baseNote, const NoteSequence::Step &step, const NoteSequence &sequence, const Scale &scale, int octave, int transpose);

    bool fill() const {
        return (_noteTrack.fillMuted() || !TrackEngine::mute()) ? TrackEngine::fill() : false;
    }

    NoteTrack &_noteTrack;

    NoteSequence *_sequence = nullptr;
    const NoteSequence *_fillSequence = nullptr;

    int _lastFreeStepIndex = -1;
    int _lastNoteFreeStepIndex = -1;
    SequenceState _noteSequenceState;
    int _reReneX = 0;
    int _reReneY = 0;
    int _reReneLastXTick = -1;
    int _reReneLastYTick = -1;
    uint32_t _reReneDivisorX = 0;
    uint32_t _reReneDivisorY = 0;
    NoteSequence::DivYSource _reReneDivYSource = NoteSequence::DivYSource::Divisor;
    int _reReneDivYTrack = 0;
    bool _reReneYGatePrev = false;
    SequenceState _sequenceState;
    int _currentStep;
    bool _prevCondition;
    int _pulseCounter;  // Tracks current pulse within step for pulse count feature

    int _monitorStepIndex = -1;

    RecordHistory _recordHistory;
    bool _monitorOverrideActive = false;
    StepRecorder _stepRecorder;

    bool _activity;
    bool _gateOutput;
    bool _trigGateActive = false;
    uint32_t _trigGateOffTicks = 0;
    float _cvOutput;
    float _cvOutputTarget;
    bool _slideActive;

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
