#pragma once

#include "Config.h"
#include "TrackEngine.h"
#include "SequenceState.h"
#include "SortedQueue.h"
#include "Groove.h"
#include "RecordHistory.h"
#include "StepRecorder.h"

class NoteTrackEngine : public TrackEngine {
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
#endif
    };

    struct GateCompare {
        bool operator()(const Gate &a, const Gate &b) {
            return a.tick < b.tick;
        }
    };

    NoteTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine) :
        TrackEngine(engine, model, track, linkedTrackEngine),
        _noteTrack(track.noteTrack())
    {
        reset();
    }

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Note; }

    virtual void reset() override;
    virtual void restart() override;
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual void changePattern() override;

    virtual void monitorMidi(uint32_t tick, const MidiMessage &message) override;
    virtual void clearMidiMonitoring() override;

    virtual const TrackLinkData *linkData() const override { return &_linkData; }

    virtual bool activity() const override { return _activity; }
    virtual bool gateOutput(int index) const override { return _gateOutput; }
    virtual float cvOutput(int index) const override { return _cvOutput; }
    virtual float sequenceProgress() const override {
        return _currentStep < 0 ? 0.f : float(_currentStep - _sequence->firstStep()) / (_sequence->lastStep() - _sequence->firstStep());
    }

    const NoteSequence &sequence() const { return *_sequence; }
    bool isActiveSequence(const NoteSequence &sequence) const { return &sequence == _sequence; }

    int currentStep() const { return _currentStep; }
    int currentRecordStep() const { return _stepRecorder.stepIndex(); }

    void setMonitorStep(int index);

private:
    void triggerStep(uint32_t tick, uint32_t divisor);
    void recordStep(uint32_t tick, uint32_t divisor);
    int noteFromMidiNote(uint8_t midiNote) const;
    float applyHarmony(float baseNote, const NoteSequence::Step &step, const NoteSequence &sequence, const Scale &scale, int octave, int transpose);

    bool fill() const {
        return (_noteTrack.fillMuted() || !TrackEngine::mute()) ? TrackEngine::fill() : false;
    }

    NoteTrack &_noteTrack;

    TrackLinkData _linkData;

    NoteSequence *_sequence = nullptr;
    const NoteSequence *_fillSequence = nullptr;

    uint32_t _freeRelativeTick;
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
