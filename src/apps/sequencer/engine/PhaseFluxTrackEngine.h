#pragma once

#include "TrackEngine.h"

#include "model/Track.h"
#include "model/PhaseFluxTrack.h"
#include "model/PhaseFluxSequence.h"
#include "model/PhaseFluxMath.h"
#include "model/AccumulatorConfig.h"

#include <cstdint>

class PhaseFluxTrackEngine final : public TrackEngine {
public:
    static constexpr int kStageCount = PhaseFluxMath::kStageCount;
    static constexpr int kMaxPulses = 16;

    PhaseFluxTrackEngine(Engine &engine, const Model &model, Track &track) :
        TrackEngine(engine, model, track),
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
        // §9 mute behavior: DAC holds previous voltage when track is muted.
        // The engine stops pushing CvUpdate notifications when muted; the
        // accessor just returns the last computed value. cvUpdateMode is
        // decoupled — it controls evaluation timing only (§6.2.2).
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

    // Test accessors — counter state observability (spec §13.10 Task 5).
    int noteAccumCounter(int idx) const { return _noteAccumCounter[idx]; }
    int pulseAccumCounter(int idx) const { return _pulseAccumCounter[idx]; }
    int8_t noteAccumDir(int idx) const { return _noteAccumDir[idx]; }
    int8_t pulseAccumDir(int idx) const { return _pulseAccumDir[idx]; }
    uint32_t cycleCount() const { return _cycleCount; }

    // Pure helper — advance one counter per accumulator config and per-cell step.
    // Static so unit tests can exercise the order/polarity/bounds logic without
    // constructing a full Engine. Caller owns the counter + pendulumDir state.
    static void advanceCounter(int &counter, int8_t &pendulumDir,
                               const AccumulatorConfig &cfg, int step);

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
    uint8_t _cachedLength[kStageCount];
    bool _cachedSkip[kStageCount];
    uint16_t _cachedDivisor = 0;
    uint8_t _cachedClockMult = 0;
    int8_t _cachedLenNudge = 0;

    // Dual-accumulator state (spec §13.3, Task 5).
    // Local scope uses [cell] index; Track scope shares [0].
    int _noteAccumCounter[kStageCount];
    int8_t _noteAccumDir[kStageCount];
    int _pulseAccumCounter[kStageCount];
    int8_t _pulseAccumDir[kStageCount];
    uint32_t _cycleCount = 0;
    uint32_t _prevCycleTick = 0;

    // Per-stage-visit pulse-fire memory (bit i = pulse i fired).
    uint16_t _pulseFired = 0;

    // Derived position.
    int _slotIdx = 0;
    int _prevSlotIdx = -1;
    int _activeCell = 0;
    float _stagePhase = 0.f;

    // Continuous pitch phase for Global pitch mode. Free-running accumulator,
    // never resets at cell entry. Advances every tick at rate × tempo.
    float _pitchPhase = 0.f;

    // Per-stage-visit schedule built at slot entry by rebuildSchedule().
    ScheduledPulse _schedule[kMaxPulses];
    int _scheduleCount = 0;

    // Output.
    bool _gateState = false;
    uint32_t _gateTimer = 0;
    float _cvOutput = 0.f;
    float _cvOutputTarget = 0.f;

    // Activity LED.
    bool _activity = false;
    uint32_t _activityTimer = 0;
};

static_assert(sizeof(PhaseFluxTrackEngine) <= 900, "PhaseFluxTrackEngine too large (early-warn ceiling — TeletypeTrackEngine at 944 is the real container limit)");
