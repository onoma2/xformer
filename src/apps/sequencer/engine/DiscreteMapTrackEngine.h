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

    virtual bool activity() const override { return _activityTimer > 0; }
    virtual bool gateOutput(int index) const override {
        if (_monitorOverrideActive) {
            return _monitorGateOutput;
        }
        return !mute() && _gateTimer > 0 && _activeStage >= 0;
    }
    virtual float cvOutput(int index) const override {
        if (_monitorOverrideActive) {
            return _cvOutput;
        }
        // When muted and in Gate mode, output should be 0
        if (mute() && _discreteMapTrack.cvUpdateMode() == DiscreteMapTrack::CvUpdateMode::Gate) {
            return 0.0f;
        }
        return _cvOutput;
    }
    virtual float sequenceProgress() const override { return _rampPhase; }

    const DiscreteMapSequence &sequence() const { return *_sequence; }
    bool isActiveSequence(const DiscreteMapSequence &sequence) const { return &sequence == _sequence; }
    void invalidateThresholds() { _thresholdsDirty = true; }
    void setMonitorStage(int index);

    // For Testing/UI
    int activeStage() const { return _activeStage; }
    float currentInput() const { return _currentInput; }
    float rampPhase() const { return _rampPhase; }
    
    int findActiveStage(float input, float prevInput);
    void recalculateLengthThresholds();
    void recalculatePositionThresholds();
    float getThresholdVoltage(int stageIndex);

private:
    static constexpr float kInternalRampMin = -5.0f;
    static constexpr float kInternalRampMax = 5.0f;
    static constexpr float kPrevInputInit = -5.001f;
    static constexpr float kMinSpanAbs = 0.01f;
    static constexpr float kArmTolerancePct = 0.05f;
    static constexpr float kCoveragePct = 0.90f;
    static constexpr float kRangeEpsilon = 1e-6f;
    static constexpr uint32_t kActivityPulseTicks = 12;

    void updateRamp(double tickPos);
    uint32_t scaledDivisorTicks() const;
    float getRoutedInput();
    float noteIndexToVoltage(int8_t noteIndex, bool useSampled = true);
    bool updateExternalOnce();

    // Voltage range helpers (delegated to sequence parameters)
    // rangeMin = START of ramp (rangeLow)
    // rangeMax = END of ramp (rangeHigh)
    // For inverted ranges (rangeHigh < rangeLow), ramp goes backwards
    float rangeMin() const {
        return _sequence->rangeLow();
    }
    float rangeMax() const {
        return _sequence->rangeHigh();
    }
    float rangeSpan() const { return _sequence->rangeSpan(); }

    DiscreteMapTrack &_discreteMapTrack;

    DiscreteMapSequence *_sequence = nullptr;

    // === Ramp State (Internal clock source) ===
    float _rampPhase = 0.0f;        // 0.0 - 1.0
    float _rampValue = 0.0f;        // Current voltage
    bool _running = true;

    // === Input ===
    float _currentInput = 0.0f;
    float _prevInput = 0.0f;
    float _prevSync = 0.0f;
    bool _prevLoop = true;

    // === Threshold Cache ===
    float _lengthThresholds[DiscreteMapSequence::StageCount];
    float _positionThresholds[DiscreteMapSequence::StageCount];
    bool _thresholdsDirty = true;
    float _prevRangeHigh = 0.0f;
    float _prevRangeLow = 0.0f;
    DiscreteMapSequence::ThresholdMode _prevThresholdMode = DiscreteMapSequence::ThresholdMode::Position;

    // === Stage State ===
    int _activeStage = -1;          // -1 = none

    // === Output ===
    float _cvOutput = 0.0f;
    float _targetCv = 0.0f;
    uint32_t _gateTimer = 0;
    bool _monitorGateOutput = false;
    bool _monitorOverrideActive = false;
    int _monitorStageIndex = -1;

    // === Sampled pitch params (Gate mode) ===
    int _sampledOctave = 0;
    int _sampledTranspose = 0;
    int _sampledRootNote = 0;

    // === Activity ===
    bool _activity = false;
    uint32_t _activityTimer = 0;

    // === External ONCE sweep tracking ===
    bool _extOnceArmed = false;
    bool _extOnceDone = false;
    float _extMinSeen = 0.0f;
    float _extMaxSeen = 0.0f;
    int _lastScannerSegment = -1;

    // === Sync bookkeeping ===
    uint32_t _resetTickOffset = 0;
};
