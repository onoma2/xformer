#pragma once

#include "TrackEngine.h"

#include "model/Track.h"
#include "model/DiscreteMapSequence.h"
#include "model/Scale.h"

class DiscreteMapTrackEngine : public TrackEngine {
public:
    DiscreteMapTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine) :
        TrackEngine(engine, model, track, linkedTrackEngine),
        _discreteMapTrack(track.discreteMapTrack())
    {
        reset();
    }

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::DiscreteMap; }

    virtual void reset() override;
    virtual void restart() override;
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual void changePattern() override;

    virtual bool activity() const override { return _activity; }
    virtual bool gateOutput(int index) const override { return _activeStage >= 0; }
    virtual float cvOutput(int index) const override { return _cvOutput; }
    virtual float sequenceProgress() const override { return _rampPhase; }

    const DiscreteMapSequence &sequence() const { return *_sequence; }
    bool isActiveSequence(const DiscreteMapSequence &sequence) const { return &sequence == _sequence; }
    void invalidateThresholds() { _thresholdsDirty = true; }

    // For UI
    int activeStage() const { return _activeStage; }
    float currentInput() const { return _currentInput; }
    float rampPhase() const { return _rampPhase; }

private:
    void updateRamp(uint32_t tick);
    float getRoutedInput();
    float getThresholdVoltage(int stageIndex);
    void recalculateLengthThresholds();
    int findActiveStage(float input, float prevInput);
    float noteIndexToVoltage(int8_t noteIndex);

    // Voltage range helpers (always ±5V for internal ramp)
    float rangeMin() const { return -5.0f; }
    float rangeMax() const { return 5.0f; }

    DiscreteMapTrack &_discreteMapTrack;

    DiscreteMapSequence *_sequence = nullptr;

    // === Ramp State (Internal clock source) ===
    float _rampPhase = 0.0f;        // 0.0 - 1.0
    float _rampValue = 0.0f;        // Current voltage
    bool _running = true;

    // === Input ===
    float _currentInput = 0.0f;
    float _prevInput = 0.0f;

    // === Threshold Cache (Length mode) ===
    float _lengthThresholds[DiscreteMapSequence::StageCount];
    bool _thresholdsDirty = true;

    // === Stage State ===
    int _activeStage = -1;          // -1 = none

    // === Output ===
    float _cvOutput = 0.0f;
    float _targetCv = 0.0f;

    // === Activity ===
    bool _activity = false;
};
