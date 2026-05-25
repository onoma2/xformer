#pragma once

#include "TrackEngine.h"

#include "model/Track.h"
#include "model/PhaseFluxTrack.h"
#include "model/PhaseFluxSequence.h"
#include "model/PhaseFluxMath.h"

#include <cstdint>

class PhaseFluxTrackEngine : public TrackEngine {
public:
    static constexpr int kStageCount = PhaseFluxMath::kStageCount;
    static constexpr int kMaxPulses = 8;

    PhaseFluxTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine) :
        TrackEngine(engine, model, track, linkedTrackEngine),
        _phaseFluxTrack(track.phaseFluxTrack())
    {
        reset();
    }

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::PhaseFlux; }

    virtual void reset() override;
    virtual void restart() override;
    virtual void stop() override;
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual void changePattern() override;

    virtual bool activity() const override { return _activityTimer > 0; }
    virtual bool gateOutput(int index) const override {
        (void)index;
        return !mute() && _gateState;
    }
    virtual float cvOutput(int index) const override {
        (void)index;
        if (mute() && _phaseFluxTrack.cvUpdateMode() == PhaseFluxTrack::CvUpdateMode::Gate) {
            return 0.0f;
        }
        return _cvOutput;
    }
    virtual float sequenceProgress() const override {
        if (_cycleTicks <= 0) return -1.f;
        return _stagePhase;
    }

    const PhaseFluxSequence &sequence() const { return *_sequence; }
    bool isActiveSequence(const PhaseFluxSequence &sequence) const { return &sequence == _sequence; }

    int activeCell() const { return _activeCell; }
    int slotIndex() const { return _slotIdx; }

private:
    static constexpr uint32_t kActivityPulseTicks = 12;
    static constexpr uint32_t kMinPulseGateTicks = 6;
    static constexpr uint32_t kMinPulseGapTicks = 1;

    struct ScheduledPulse {
        uint16_t triggerOffset;  // ticks from slot start
        uint16_t gateTicks;
        float cv;
    };

    bool detectLayoutChange();
    void rebuildCumulativeTable();
    void rebuildSchedule(int slotDurationTicks);

    PhaseFluxTrack &_phaseFluxTrack;
    PhaseFluxSequence *_sequence = nullptr;

    // Timing — cumulative table in CONFIG_PPQN ticks (snake-walk order).
    uint32_t _resetTickOffset = 0;
    int _cumulativeTicks[kStageCount + 1];
    int _cycleTicks = 0;
    bool _firstTickAfterReset = true;
    bool _layoutDirty = true;

    // Cached layout inputs for change detection.
    uint8_t _cachedStageDivisor[kStageCount];
    uint8_t _cachedStageLen[kStageCount];
    bool _cachedSkip[kStageCount];
    uint16_t _cachedDivisor = 0;
    uint8_t _cachedClockMult = 0;

    // Per-cell accumulator (scale degrees).
    int _accumulatorCounter[kStageCount];

    // Per-stage-visit pulse-fire memory (bit i = pulse i fired).
    uint8_t _pulseFired = 0;

    // Derived position.
    int _slotIdx = 0;
    int _prevSlotIdx = -1;
    int _activeCell = 0;
    float _stagePhase = 0.f;

    // Per-stage-visit schedule built at slot entry by rebuildSchedule().
    ScheduledPulse _schedule[kMaxPulses];
    int _scheduleCount = 0;

    // Output.
    bool _gateState = false;
    uint32_t _gateTimer = 0;
    float _cvOutput = 0.f;

    // Activity LED.
    bool _activity = false;
    uint32_t _activityTimer = 0;
};

static_assert(sizeof(PhaseFluxTrackEngine) <= 1024, "PhaseFluxTrackEngine too large");
