#include "PhaseFluxTrackEngine.h"

#include "Engine.h"

#include "model/Curve.h"
#include "model/Scale.h"

#include <algorithm>
#include <cmath>

namespace {

// §6.3 mask table. LSB = pulse 0. A set bit MUTES that pulse.
//   Off, 1in2, 1in3, 1in4, 2in3, 2in4, 3in4, 1in8
constexpr uint8_t kMaskTable[8] = {
    0x00, 0xAA, 0x49, 0x11, 0xB6, 0x55, 0x77, 0x01,
};

inline float pitchRangeToOctaves(PhaseFluxSequence::PitchRangeType v) {
    switch (v) {
    case PhaseFluxSequence::PitchRangeType::Half:  return 0.5f;
    case PhaseFluxSequence::PitchRangeType::One:   return 1.0f;
    case PhaseFluxSequence::PitchRangeType::Two:   return 2.0f;
    case PhaseFluxSequence::PitchRangeType::Three: return 3.0f;
    }
    return 1.0f;
}

inline Curve::Type temporalCurveLut(PhaseFluxSequence::TemporalCurveType v) {
    // Bounce = ExpDown3x per spec §16 implementer punch list — Phase C may
    // swap if hardware audition prefers ExpDown2x or a bespoke LUT.
    switch (v) {
    case PhaseFluxSequence::TemporalCurveType::Linear: return Curve::RampUp;
    case PhaseFluxSequence::TemporalCurveType::Bell:   return Curve::Bell;
    case PhaseFluxSequence::TemporalCurveType::Bounce: return Curve::ExpDown3x;
    }
    return Curve::RampUp;
}

inline Curve::Type pitchCurveLut(PhaseFluxSequence::PitchCurveType v) {
    switch (v) {
    case PhaseFluxSequence::PitchCurveType::Ramp:     return Curve::RampUp;
    case PhaseFluxSequence::PitchCurveType::Bell:     return Curve::Bell;
    case PhaseFluxSequence::PitchCurveType::Triangle: return Curve::Triangle;
    }
    return Curve::RampUp;
}

inline float clamp01(float v) {
    return std::max(0.f, std::min(1.f, v));
}

inline float applyPowerBend(float x, int knob) {
    if (knob == 0) return x;
    return PhaseFluxMath::powerBend(clamp01(x), PhaseFluxMath::powerBendKnobToParam(knob));
}

} // namespace

void PhaseFluxTrackEngine::reset() {
    _resetTickOffset = 0;
    _firstTickAfterReset = true;
    _layoutDirty = true;
    _pulseFired = 0;
    _slotIdx = 0;
    _prevSlotIdx = -1;
    _activeCell = 0;
    _stagePhase = 0.f;
    _gateState = false;
    _gateTimer = 0;
    _cvOutput = 0.f;
    _activity = false;
    _activityTimer = 0;
    _scheduleCount = 0;
    for (int i = 0; i < kStageCount; ++i) {
        _accumulatorCounter[i] = 0;
    }
    for (int i = 0; i <= kStageCount; ++i) {
        _cumulativeTicks[i] = 0;
    }
    _cycleTicks = 0;
    _sequence = &_phaseFluxTrack.sequence(pattern());
}

void PhaseFluxTrackEngine::restart() {
    // §11.1 light reset — _resetTickOffset re-anchored on next tick,
    // pulse-fire + gate cleared, accumulator counters preserved per §7.1.
    _firstTickAfterReset = true;
    _pulseFired = 0;
    _gateState = false;
    _gateTimer = 0;
    _prevSlotIdx = -1;
    _scheduleCount = 0;
}

void PhaseFluxTrackEngine::stop() {
    _gateState = false;
    _gateTimer = 0;
}

void PhaseFluxTrackEngine::changePattern() {
    // §7.1 — pattern switch is the one event that clears accumulator counters.
    reset();
}

bool PhaseFluxTrackEngine::detectLayoutChange() {
    if (!_sequence) return false;
    uint16_t div = uint16_t(_sequence->divisor());
    uint8_t mult = uint8_t(_sequence->clockMultiplier());
    if (div != _cachedDivisor || mult != _cachedClockMult) return true;
    for (int i = 0; i < kStageCount; ++i) {
        const auto &s = _sequence->stage(i);
        if (uint8_t(s.stageDivisor()) != _cachedStageDivisor[i]) return true;
        if (s.skip() != _cachedSkip[i]) return true;
    }
    return false;
}

void PhaseFluxTrackEngine::rebuildCumulativeTable() {
    int stageDivisorTicksArr[kStageCount];
    bool skipArr[kStageCount];
    for (int i = 0; i < kStageCount; ++i) {
        const auto &s = _sequence->stage(i);
        stageDivisorTicksArr[i] = PhaseFluxMath::stageDivisorTicks(s.stageDivisor());
        skipArr[i] = s.skip();
        _cachedStageDivisor[i] = uint8_t(s.stageDivisor());
        _cachedSkip[i] = s.skip();
    }
    _cachedDivisor = uint16_t(_sequence->divisor());
    _cachedClockMult = uint8_t(_sequence->clockMultiplier());

    _cycleTicks = PhaseFluxMath::computeCumulativeTicks(
        stageDivisorTicksArr, skipArr,
        int(_engine.measureDivisor()),
        _sequence->clockMultiplier(),
        _cumulativeTicks);

    // §3.4 — held gate from old layout ends; pulse-fire memory cleared.
    // _prevSlotIdx reset forces rebuildSchedule on next tick even if same slot.
    _gateTimer = 0;
    _gateState = false;
    _pulseFired = 0;
    _scheduleCount = 0;
    _prevSlotIdx = -1;
    _layoutDirty = false;
}

void PhaseFluxTrackEngine::rebuildSchedule(int slotDurationTicks) {
    _scheduleCount = 0;
    if (slotDurationTicks <= 0) return;

    const auto &stage = _sequence->stage(_activeCell);
    const int pulseCount = stage.pulseCount();

    const Scale &scale = _sequence->selectedScale(_model.project().scale());
    const int rootNote = _sequence->selectedRootNote(_model.project().rootNote());
    const int octave = _phaseFluxTrack.octave();
    const int transpose = _phaseFluxTrack.transpose();

    const Curve::Type tempCurveType = temporalCurveLut(stage.temporalCurve());
    const Curve::Type pitchCurveType = pitchCurveLut(stage.pitchCurve());
    const bool tFlipV = stage.temporalFlipV();
    const bool tFlipH = stage.temporalFlipH();
    const bool pFlipV = stage.pitchFlipV();
    const bool pFlipH = stage.pitchFlipH();
    const int phaseShift = stage.phaseShift();
    const int maskByte = kMaskTable[int(stage.mask())];
    const int maskShift = stage.maskShift();

    const float rangeOctaves = pitchRangeToOctaves(stage.pitchRange());
    const int rangeDegrees = std::max(1, int(std::round(rangeOctaves * scale.notesPerOctave())));
    const int accOffset = _accumulatorCounter[_activeCell] * stage.accumulatorStep();
    const int baseDegree = stage.basePitch() + accOffset
        + octave * scale.notesPerOctave() + transpose;

    // §6.1 + §6.2 — build per-pulse (triggerOffset, cv) pairs, then apply
    // §6.3 mask and §6.4 collision clamp.
    struct PulseCandidate {
        int origIdx;
        float triggerTime;
        float cv;
    };
    PulseCandidate cands[kMaxPulses];
    int candCount = 0;

    for (int i = 0; i < pulseCount; ++i) {
        // §6.3 mask gates on the ORIGINAL pulse index — survives §6.4 reorder.
        bool muted = (maskByte >> ((i + maskShift) & 7)) & 1;
        if (muted) continue;

        // §6.1 temporal pipeline
        float t_raw = (pulseCount == 1) ? 0.5f : (float(i) / float(pulseCount - 1));
        float t_warped = applyPowerBend(t_raw, stage.temporalWarp());
        float t_input = tFlipH ? (1.f - t_warped) : t_warped;
        float t_curved = Curve::eval(tempCurveType, t_input);
        float t_flipped = tFlipV ? (1.f - t_curved) : t_curved;
        float t_final = applyPowerBend(t_flipped, stage.temporalResponse());
        float t_shifted = t_final + (float(phaseShift) * 0.125f);
        t_shifted = std::fmod(t_shifted, 1.f);
        if (t_shifted < 0.f) t_shifted += 1.f;
        float triggerTime = t_shifted * float(slotDurationTicks);

        // §6.2 pitch pipeline using triggerTime/slotDuration as φ
        float phi = (pulseCount == 1) ? 0.0f : t_shifted;
        float phi_warped = applyPowerBend(phi, stage.pitchWarp());
        float phi_input = pFlipH ? (1.f - phi_warped) : phi_warped;
        float p_curved = Curve::eval(pitchCurveType, phi_input);
        float p_flipped = pFlipV ? (1.f - p_curved) : p_curved;
        float p_final = applyPowerBend(p_flipped, stage.pitchResponse());
        // §6.2.1 maskMelody/tiltMelody centrality filter deferred to Phase C.

        float offsetDegrees = 0.f;
        switch (stage.pitchDirection()) {
        case PhaseFluxSequence::PitchDirectionType::Up:
            offsetDegrees = p_final * float(rangeDegrees);
            break;
        case PhaseFluxSequence::PitchDirectionType::Down:
            offsetDegrees = -p_final * float(rangeDegrees);
            break;
        case PhaseFluxSequence::PitchDirectionType::Bipolar:
            offsetDegrees = (p_final - 0.5f) * float(rangeDegrees);
            break;
        }

        int degree = baseDegree + int(std::round(offsetDegrees));
        float cv = scale.noteToVolts(degree);
        if (scale.isChromatic()) cv += float(rootNote) * (1.f / 12.f);

        cands[candCount].origIdx = i;
        cands[candCount].triggerTime = triggerTime;
        cands[candCount].cv = cv;
        ++candCount;
    }

    // Insertion sort by trigger time — avoids std::sort inlining past array bounds.
    for (int i = 1; i < candCount; ++i) {
        PulseCandidate key = cands[i];
        int j = i - 1;
        while (j >= 0 && cands[j].triggerTime > key.triggerTime) {
            cands[j + 1] = cands[j];
            --j;
        }
        cands[j + 1] = key;
    }

    // §6.4 collision clamp — verbatim port of StochasticGenerator::evaluateBurst.
    const int kMinStageParentTicks = int(kMinPulseGateTicks + kMinPulseGapTicks);
    if (slotDurationTicks < kMinStageParentTicks) return;

    int prevEnd = 0;
    for (int k = 0; k < candCount; ++k) {
        int t = int(cands[k].triggerTime);
        if (t < prevEnd + int(kMinPulseGapTicks)) continue;

        int nextT = (k + 1 < candCount) ? int(cands[k + 1].triggerTime) : slotDurationTicks;
        int pulsePeriod = nextT - t;
        if (pulsePeriod <= 0) continue;

        int nominalGate = (int(stage.gateLength()) * pulsePeriod) / 100;
        int gateTicks = std::min(nominalGate, pulsePeriod - int(kMinPulseGapTicks));
        if (gateTicks < int(kMinPulseGateTicks)) continue;

        if (_scheduleCount >= kMaxPulses) break;
        _schedule[_scheduleCount].triggerOffset = uint16_t(std::max(0, t));
        _schedule[_scheduleCount].gateTicks = uint16_t(gateTicks);
        _schedule[_scheduleCount].cv = cands[k].cv;
        ++_scheduleCount;
        prevEnd = t + gateTicks;
    }
}

TrackEngine::TickResult PhaseFluxTrackEngine::tick(uint32_t tick) {
    // Resolve sequence lazily — pattern() can change without changePattern().
    PhaseFluxSequence *seq = &_phaseFluxTrack.sequence(pattern());
    if (seq != _sequence) {
        _sequence = seq;
        _layoutDirty = true;
        _prevSlotIdx = -1;
    }

    if (_firstTickAfterReset) {
        _resetTickOffset = tick;
        _firstTickAfterReset = false;
    }

    if (_layoutDirty || detectLayoutChange()) {
        rebuildCumulativeTable();
    }

    uint32_t relativeTick = tick - _resetTickOffset;

    // §3.3 Reset Measure — light reset, preserves counters per §7.1.
    int resetMeasure = _sequence->resetMeasure();
    if (resetMeasure > 0) {
        uint32_t resetDivisor = uint32_t(resetMeasure) * _engine.measureDivisor();
        if (relativeTick % resetDivisor == 0 && relativeTick != 0) {
            _resetTickOffset = tick;
            relativeTick = 0;
            _pulseFired = 0;
            _gateTimer = 0;
            _gateState = false;
            _prevSlotIdx = -1;
        }
    }

    int slotIdx = 0, activeCell = 0;
    float stagePhase = 0.f;
    bool valid = PhaseFluxMath::deriveTickPosition(
        relativeTick, _cumulativeTicks, _cycleTicks,
        slotIdx, activeCell, stagePhase);

    if (!valid) {
        // §3.5 idle — hold gate low, freeze counters, preserve CV.
        if (_gateState) {
            _gateState = false;
            _gateTimer = 0;
            return GateUpdate;
        }
        return NoUpdate;
    }

    _slotIdx = slotIdx;
    _activeCell = activeCell;
    _stagePhase = stagePhase;

    TickResult result = NoUpdate;

    // Slot change: advance previous cell's accumulator, rebuild schedule.
    if (_slotIdx != _prevSlotIdx) {
        if (_prevSlotIdx >= 0) {
            int completedCell = int(PhaseFluxMath::snakeOrder()[_prevSlotIdx]);
            const auto &completed = _sequence->stage(completedCell);
            int len = completed.accumulatorLength();
            if (len > 0) {
                _accumulatorCounter[completedCell] =
                    (_accumulatorCounter[completedCell] + 1) % len;
            }
        }
        _pulseFired = 0;
        _prevSlotIdx = _slotIdx;

        int slotDuration = _cumulativeTicks[_slotIdx + 1] - _cumulativeTicks[_slotIdx];
        rebuildSchedule(slotDuration);
    }

    uint32_t cycleTick = relativeTick % uint32_t(_cycleTicks);
    int posInSlot = int(cycleTick) - _cumulativeTicks[_slotIdx];

    // Fire any scheduled pulses whose trigger has been reached this tick.
    for (int k = 0; k < _scheduleCount; ++k) {
        if (_pulseFired & (1 << k)) continue;
        if (posInSlot < int(_schedule[k].triggerOffset)) continue;

        _pulseFired |= uint8_t(1 << k);

        _cvOutput = _schedule[k].cv;
        _gateState = true;
        _gateTimer = _schedule[k].gateTicks;
        _activity = true;
        _activityTimer = kActivityPulseTicks;
        result = result | CvUpdate | GateUpdate;
    }

    // Drop gate when timer expires.
    if (_gateTimer > 0) {
        --_gateTimer;
        if (_gateTimer == 0 && _gateState) {
            _gateState = false;
            result = result | GateUpdate;
        }
    }

    if (_activityTimer > 0) {
        --_activityTimer;
    }

    return result;
}

void PhaseFluxTrackEngine::update(float dt) {
    (void)dt;
    // Slide / smoothing deferred to Phase C.
}
