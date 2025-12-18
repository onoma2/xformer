#include "DiscreteMapTrackEngine.h"

#include "Engine.h"

#include <algorithm>
#include <cmath>

void DiscreteMapTrackEngine::reset() {
    _sequence = &_discreteMapTrack.sequence(pattern());

    // Internal ramp always starts at -5V (full range)
    _rampPhase = 0.0f;
    _rampValue = -5.0f;

    // Initialize prevInput below the full range to allow first crossing
    _prevInput = -5.001f;
    _currentInput = -5.0f;

    _prevSync = _discreteMapTrack.routedSync();
    _resetTickOffset = 0;
    _prevLoop = _sequence ? _sequence->loop() : true;
    _activeStage = -1;
    _cvOutput = 0.0f;
    _targetCv = 0.0f;
    _gateTimer = 0;
    _running = true;
    _thresholdsDirty = true;
    _activity = false;
    _extOnceArmed = false;
    _extOnceDone = false;
    _extMinSeen = 0.f;
    _extMaxSeen = 0.f;
}

void DiscreteMapTrackEngine::restart() {
    _rampPhase = 0.0f;
    _running = true;
    _resetTickOffset = 0;
    _prevSync = _discreteMapTrack.routedSync();
    _prevLoop = _sequence ? _sequence->loop() : true;
    _extOnceArmed = false;
    _extOnceDone = false;
    _extMinSeen = 0.f;
    _extMaxSeen = 0.f;
}

void DiscreteMapTrackEngine::changePattern() {
    _sequence = &_discreteMapTrack.sequence(pattern());
    _thresholdsDirty = true;
    _prevLoop = _sequence->loop();
    _extOnceArmed = false;
    _extOnceDone = false;
    _extMinSeen = 0.f;
    _extMaxSeen = 0.f;
}

TrackEngine::TickResult DiscreteMapTrackEngine::tick(uint32_t tick) {
    _sequence = &_discreteMapTrack.sequence(pattern());

    // Handle loop mode toggled from Once->Loop by restarting ramp
    if (!_prevLoop && _sequence->loop()) {
        reset();
        _resetTickOffset = tick;
    }
    if (_prevLoop && !_sequence->loop()) {
        _extOnceArmed = false;
        _extOnceDone = false;
        _extMinSeen = 0.f;
        _extMaxSeen = 0.f;
    }
    _prevLoop = _sequence->loop();

    // Invalidate threshold cache if voltage window is being modulated
    if (_sequence->isRouted(Routing::Target::DiscreteMapRangeHigh) ||
        _sequence->isRouted(Routing::Target::DiscreteMapRangeLow)) {
        _thresholdsDirty = true;
    }

    // Sync / reset handling
    uint32_t relativeTick = tick - _resetTickOffset;
    bool resetRequested = false;
    auto syncMode = _sequence->syncMode();
    switch (syncMode) {
    case DiscreteMapSequence::SyncMode::ResetMeasure: {
        uint32_t resetDivisor = _sequence->resetMeasure() * _engine.measureDivisor();
        if (resetDivisor > 0 && relativeTick % resetDivisor == 0) {
            resetRequested = true;
        }
        break;
    }
    case DiscreteMapSequence::SyncMode::External: {
        float syncVal = _discreteMapTrack.routedSync();
        if (_prevSync <= 0.f && syncVal > 0.f) {
            resetRequested = true;
        }
        _prevSync = syncVal;
        break;
    }
    case DiscreteMapSequence::SyncMode::Off:
    case DiscreteMapSequence::SyncMode::Last:
        break;
    }

    if (resetRequested) {
        reset();
        _resetTickOffset = tick;
        relativeTick = 0;
    }

    // 1. Update input source
    if (_sequence->clockSource() != DiscreteMapSequence::ClockSource::External) {
        if (_running || _sequence->loop()) {
            updateRamp(relativeTick);
        }
        _currentInput = _rampValue;
    } else {
        _currentInput = getRoutedInput();
    }

    // External ONCE: arm inside window and freeze after one sweep across the defined range
    bool extOnceFreeze = false;
    bool extOnceMode = _sequence->clockSource() == DiscreteMapSequence::ClockSource::External && !_sequence->loop();
    if (extOnceMode) {
        float winLo = std::min(rangeMin(), rangeMax());
        float winHi = std::max(rangeMin(), rangeMax());
        float spanAbs = std::max(0.01f, std::abs(rangeSpan()));

        if (!_extOnceDone) {
            if (_extOnceArmed) {
                // Track min/max to detect range coverage
                _extMinSeen = std::min(_extMinSeen, _currentInput);
                _extMaxSeen = std::max(_extMaxSeen, _currentInput);

                // Calculate what percentage of the range we've covered
                float coverageSpan = _extMaxSeen - _extMinSeen;
                float coveragePercent = coverageSpan / spanAbs;

                // Complete when we've covered 90% of the defined range (direction-agnostic)
                // This tolerates LFOs/envelopes that don't quite reach the exact endpoints
                if (coveragePercent >= 0.90f) {
                    _extOnceArmed = false;
                    _extOnceDone = true;
                }
            } else {
                // Arm when entering the voltage window (with 5% tolerance)
                float armTolerance = spanAbs * 0.05f;
                if (_currentInput >= (winLo - armTolerance) && _currentInput <= (winHi + armTolerance)) {
                    _extOnceArmed = true;
                    _extMinSeen = _extMaxSeen = _currentInput;
                }
            }
        }

        extOnceFreeze = _extOnceDone || !_extOnceArmed;
    }

    // 2. Recalc length thresholds if needed
    if (_thresholdsDirty && _sequence->thresholdMode() == DiscreteMapSequence::ThresholdMode::Length) {
        recalculateLengthThresholds();
        _thresholdsDirty = false;
    }

    // 3. Find active stage from threshold crossings
    int newStage = extOnceFreeze ? _activeStage : findActiveStage(_currentInput, _prevInput);

    // Activity detection: true if stage changed
    _activity = (newStage != _activeStage && newStage >= 0);

    float prevCv = _cvOutput;
    bool prevGate = _gateTimer > 0;
    bool gateChanged = newStage != _activeStage;

    if (_gateTimer > 0) {
        --_gateTimer;
    }

    _activeStage = newStage;

    // Trigger Gate
    if (gateChanged && _activeStage >= 0) {
        uint32_t stepTicks = _sequence->divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
        if (stepTicks == 0) stepTicks = 1;
        int gateLengthPercent = _sequence->gateLength();
        if (gateLengthPercent == 0) {
            _gateTimer = 3; // Explicit 1-tick pulse
        } else {
            _gateTimer = (stepTicks * gateLengthPercent) / 100;
        }
    }

    // 4. Update CV output
    if (_activeStage >= 0) {
        _targetCv = noteIndexToVoltage(_sequence->stage(_activeStage).noteIndex());
        _targetCv += _discreteMapTrack.offset() * 0.01f;

        if (_sequence->slewEnabled()) {
            // Simple slew rate (can be made configurable later)
            // Use exponential slew for smooth transitions
            float slewRate = 0.1f;
            _cvOutput += (_targetCv - _cvOutput) * slewRate;
        } else {
            _cvOutput = _targetCv;
        }
    } else {
        _cvOutput = 0.0f;
    }

    _prevInput = _currentInput;

    TickResult result = TickResult::NoUpdate;

    bool currentGate = _gateTimer > 0 && _activeStage >= 0;
    if (currentGate != prevGate) {
        result |= TickResult::GateUpdate;
    }
    if (gateChanged || std::abs(_cvOutput - prevCv) > 1e-6f) {
        result |= TickResult::CvUpdate;
    }

    return result;
}

void DiscreteMapTrackEngine::update(float dt) {
    // No per-frame updates needed
    (void)dt;
}

void DiscreteMapTrackEngine::updateRamp(uint32_t tick) {
    const auto &sequence = *_sequence;

    uint32_t periodTicks = sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
    if (periodTicks == 0) {
        periodTicks = 1;
    }

    uint32_t posInPeriod = _running ? (tick % periodTicks) : periodTicks;
    _rampPhase = float(posInPeriod) / float(periodTicks);

    // Internal ramp/triangle always uses full ±5V range (perfect synced modulation source)
    // ABOVE/BELOW parameters only affect threshold positions, not the ramp itself
    const float INTERNAL_RAMP_MIN = -5.0f;
    const float INTERNAL_RAMP_MAX = 5.0f;

    if (sequence.clockSource() == DiscreteMapSequence::ClockSource::InternalTriangle) {
        float triPhase = (_rampPhase < 0.5f) ? (_rampPhase * 2.0f) : (1.0f - (_rampPhase - 0.5f) * 2.0f);
        _rampValue = INTERNAL_RAMP_MIN + triPhase * (INTERNAL_RAMP_MAX - INTERNAL_RAMP_MIN);
    } else {
        _rampValue = INTERNAL_RAMP_MIN + _rampPhase * (INTERNAL_RAMP_MAX - INTERNAL_RAMP_MIN);
    }

    if (!_sequence->loop() && _running && posInPeriod + 1 >= periodTicks) {
        _running = false;
        _rampValue = INTERNAL_RAMP_MAX;
        _rampPhase = 1.0f;
    }
}

float DiscreteMapTrackEngine::getRoutedInput() {
    return _discreteMapTrack.routedInput();
}

float DiscreteMapTrackEngine::getThresholdVoltage(int stageIndex) {
    const auto &stage = _sequence->stage(stageIndex);

    if (_sequence->thresholdMode() == DiscreteMapSequence::ThresholdMode::Position) {
        // Position mode: use threshold value directly as voltage
        // Map from -100..+100 to rangeMin..rangeMax
        float normalizedThreshold = (stage.threshold() + 100) / 200.0f;
        return rangeMin() + normalizedThreshold * (rangeMax() - rangeMin());
    } else {
        // Length mode: use pre-calculated thresholds
        return _lengthThresholds[stageIndex];
    }
}

void DiscreteMapTrackEngine::recalculateLengthThresholds() {
    // Length mode: each threshold value determines the length of the interval
    // from the previous threshold to the current threshold.
    // Map bipolar threshold values [-100, +100] to positive range [0, 200]
    // for length calculation (time unit division)
    float totalWeight = 0;

    for (int i = 0; i < DiscreteMapSequence::StageCount; i++) {
        // Map from [-100, +100] to [0, 200]
        int mappedValue = _sequence->stage(i).threshold() + 100;  // -100 -> 0, 0 -> 100, +100 -> 200
        totalWeight += mappedValue;
    }

    // Handle the special case where all mapped values sum to 0 (all sliders down to -100)
    if (totalWeight == 0) {
        // Distribute evenly across the range
        for (int i = 0; i < DiscreteMapSequence::StageCount; i++) {
            _lengthThresholds[i] = rangeMin() + (float(i + 1) / float(DiscreteMapSequence::StageCount)) * (rangeMax() - rangeMin());
        }
        return;
    }

    // Calculate cumulative threshold positions
    float currentVoltage = rangeMin(); // Start from the bottom (-5V)

    for (int i = 0; i < DiscreteMapSequence::StageCount; i++) {
        // Map threshold value to determine proportional length
        int mappedValue = _sequence->stage(i).threshold() + 100;  // Convert to [0, 200] range
        float stageProportion = float(mappedValue) / totalWeight;
        float stageLength = stageProportion * (rangeMax() - rangeMin());

        // Move to the end of this stage's interval
        currentVoltage += stageLength;

        // Set this stage's threshold voltage (end of its interval)
        _lengthThresholds[i] = currentVoltage;
    }
}

int DiscreteMapTrackEngine::findActiveStage(float input, float prevInput) {
    // Scan stages in order, first crossing wins
    for (int i = 0; i < DiscreteMapSequence::StageCount; i++) {
        const auto &stage = _sequence->stage(i);

        if (stage.direction() == DiscreteMapSequence::Stage::TriggerDir::Off) {
            continue;
        }

        float thresh = getThresholdVoltage(i);
        bool crossed = false;

        if (stage.direction() == DiscreteMapSequence::Stage::TriggerDir::Rise) {
            // Rising edge: previous was below, current is at or above
            crossed = (prevInput < thresh && input >= thresh);
        } else {  // Fall
            // Falling edge: previous was above, current is at or below
            crossed = (prevInput > thresh && input <= thresh);
        }

        if (crossed) {
            return i;
        }
    }

    // No crossing detected

    // Check if current active stage is still valid
    if (_activeStage >= 0 && _sequence->stage(_activeStage).direction() == DiscreteMapSequence::Stage::TriggerDir::Off) {
        return -1;
    }

    return _activeStage;
}

float DiscreteMapTrackEngine::noteIndexToVoltage(int8_t noteIndex) {
    const Scale &scale = _sequence->selectedScale(_model.project().selectedScale());

    int octave = _discreteMapTrack.octave();
    int transpose = _discreteMapTrack.transpose();
    int shift = octave * scale.notesPerOctave() + transpose;

    // Convert note index to volts using scale. For chromatic scales add rootNote in semitones.
    float volts = scale.noteToVolts(noteIndex + shift);
    if (scale.isChromatic()) {
        volts += _sequence->rootNote() * (1.f / 12.f);
    }
    return volts;
}
