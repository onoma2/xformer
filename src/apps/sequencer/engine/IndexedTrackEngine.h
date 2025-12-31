#pragma once

#include "TrackEngine.h"
#include "SequenceState.h"

#include "model/Track.h"
#include "model/IndexedSequence.h"
#include "model/Scale.h"

#include "core/utils/Random.h"

class IndexedTrackEngine : public TrackEngine {
public:
    IndexedTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine) :
        TrackEngine(engine, model, track, linkedTrackEngine),
        _indexedTrack(track.indexedTrack())
    {
        reset();
    }

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Indexed; }

    virtual void reset() override;
    virtual void restart() override;
    virtual void stop() override { _gateTimer = 0; _activity = false; }
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual void changePattern() override;

    virtual bool activity() const override { return gateOutput(0); }
    virtual bool gateOutput(int index) const override;
    virtual float cvOutput(int index) const override { return _cvOutput; }
    virtual float sequenceProgress() const override;

    const IndexedSequence &sequence() const { return *_sequence; }
    bool isActiveSequence(const IndexedSequence &sequence) const { return &sequence == _sequence; }

    // For Testing/UI
    int currentStep() const { return _currentStepIndex; }
    uint16_t effectiveStepDuration() const { return _effectiveStepDuration; }
    uint32_t effectiveGateTicks() const { return _gateTimer; }

private:
    // Constants for step parameters
    static constexpr uint16_t MAX_DURATION = IndexedSequence::MaxDuration;
    static constexpr uint16_t MAX_GATE_PERCENT = 100;
    static constexpr int8_t MIN_NOTE_INDEX = -63;
    static constexpr int8_t MAX_NOTE_INDEX = 64;

private:
    void primeNextStep() { _pendingTrigger = true; }
    void resetSequenceState();
    void advanceStep();
    void triggerStep();
    float noteIndexToVoltage(int8_t noteIndex) const;
    float routedSync() const;

    IndexedTrack &_indexedTrack;
    IndexedSequence *_sequence = nullptr;
    int _cachedPattern = -1;        // Cached pattern index to avoid redundant lookups

    // === Timing state ===
    double _stepTimer = 0.0;        // Counts up to step.duration (ticks)
    uint32_t _gateTimer = 0;        // Counts down from step.gateLength
    uint32_t _effectiveStepDuration = 0; // Sampled duration for current step
    int _currentStepIndex = 0;      // Current step (0 to activeLength-1)
    bool _running = true;
    bool _pendingTrigger = false;   // Fire step at start of next tick
    float _prevSync = 0.f;          // For external sync edge detection
    int _stepsRemaining = 0;        // Used for Once mode
    double _lastFreeTickPos = 0.0;

    // === Output ===
    float _cvOutput = 0.0f;
    float _cvOutputTarget = 0.0f;
    bool _activity = false;
    bool _slideActive = false;

    SequenceState _sequenceState;
    Random _rng;
};
