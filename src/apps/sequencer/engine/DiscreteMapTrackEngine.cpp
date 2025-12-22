#include "DiscreteMapTrackEngine.h"

#include "Engine.h"

#include <algorithm>
#include <cmath>

void DiscreteMapTrackEngine::reset() {
    _sequence = &_discreteMapTrack.sequence(pattern());

    // Internal ramp always starts at -5V (full range)
    _rampPhase = 0.0f;
    _rampValue = kInternalRampMin;

    // Initialize prevInput below the full range to allow first crossing
    _prevInput = kPrevInputInit;
    _currentInput = kInternalRampMin;

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
    _lastScannerSegment = -1;
    _prevRangeHigh = _sequence ? _sequence->rangeHigh() : 0.0f;
    _prevRangeLow = _sequence ? _sequence->rangeLow() : 0.0f;
    _prevThresholdMode = _sequence ? _sequence->thresholdMode() : DiscreteMapSequence::ThresholdMode::Position;

    // Initialize sampled pitch params (for Gate mode)
    _sampledOctave = _discreteMapTrack.octave();
    _sampledTranspose = _discreteMapTrack.transpose();
    _sampledRootNote = _sequence ? _sequence->rootNote() : 0;
}

void DiscreteMapTrackEngine::changePattern() {
    _sequence = &_discreteMapTrack.sequence(pattern());
    _thresholdsDirty = true;
    _prevLoop = _sequence->loop();
    _prevRangeHigh = _sequence->rangeHigh();
    _prevRangeLow = _sequence->rangeLow();
    _prevThresholdMode = _sequence->thresholdMode();
    _extOnceArmed = false;
    _extOnceDone = false;
    _extMinSeen = 0.f;
    _extMaxSeen = 0.f;
    _lastScannerSegment = -1;
}

void DiscreteMapTrackEngine::restart() {
    _rampPhase = 0.0f;
    _running = true;
    _resetTickOffset = 0;
    _prevSync = _discreteMapTrack.routedSync();
    _prevLoop = _sequence ? _sequence->loop() : true;
    _prevRangeHigh = _sequence ? _sequence->rangeHigh() : 0.0f;
    _prevRangeLow = _sequence ? _sequence->rangeLow() : 0.0f;
    _prevThresholdMode = _sequence ? _sequence->thresholdMode() : DiscreteMapSequence::ThresholdMode::Position;
    _extOnceArmed = false;
    _extOnceDone = false;
    _extMinSeen = 0.f;
    _extMaxSeen = 0.f;
    _lastScannerSegment = -1;
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

    float currentRangeHigh = _sequence->rangeHigh();
    float currentRangeLow = _sequence->rangeLow();
    if (std::abs(currentRangeHigh - _prevRangeHigh) > kRangeEpsilon ||
        std::abs(currentRangeLow - _prevRangeLow) > kRangeEpsilon) {
        _thresholdsDirty = true;
        _prevRangeHigh = currentRangeHigh;
        _prevRangeLow = currentRangeLow;
    }
    auto thresholdMode = _sequence->thresholdMode();
    if (thresholdMode != _prevThresholdMode) {
        _thresholdsDirty = true;
        _prevThresholdMode = thresholdMode;
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
    bool extOnceFreeze = updateExternalOnce();

    // Scanner logic: map routed value (0-34) to 34 segments
    // Segment 0: Bottom dead zone, 1..32: Stages 0..31, 33: Top dead zone
    float scannerVal = _discreteMapTrack.routedScanner();
    int currentSegment = int(clamp(scannerVal, 0.f, 34.f));
    
    // Edge detection
    if (currentSegment != _lastScannerSegment) {
        // If we just entered a valid stage segment (1..32)
        if (currentSegment >= 1 && currentSegment <= 32) {
            int stageIndex = currentSegment - 1;
            if (stageIndex >= 0 && stageIndex < DiscreteMapSequence::StageCount) {
                _sequence->stage(stageIndex).cycleDirection();
            }
        }
        _lastScannerSegment = currentSegment;
    }

    // 2. Recalc thresholds if needed
    if (_thresholdsDirty) {
        if (thresholdMode == DiscreteMapSequence::ThresholdMode::Length) {
            recalculateLengthThresholds();
        } else {
            recalculatePositionThresholds();
        }
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
        // Sample pitch params for Gate mode (sample-and-hold behavior)
        if (_discreteMapTrack.cvUpdateMode() == DiscreteMapTrack::CvUpdateMode::Gate) {
            _sampledOctave = _discreteMapTrack.octave();
            _sampledTranspose = _discreteMapTrack.transpose();
            _sampledRootNote = _sequence->rootNote();
        }

        uint32_t stepTicks = _sequence->divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
        if (stepTicks == 0) stepTicks = 1;
        int gateLengthPercent = _sequence->gateLength();
        if (gateLengthPercent == 0) {
            _gateTimer = 3; // Explicit 1-tick pulse
        } else {
            _gateTimer = (stepTicks * gateLengthPercent) / 100;
        }
    }

    // 4. Update CV output based on CV Update Mode
    bool shouldOutputCv = false;

    // Condition 1: Track is not muted OR cvUpdateMode is Always
    bool muteCondition = !mute() || _discreteMapTrack.cvUpdateMode() == DiscreteMapTrack::CvUpdateMode::Always;

    // Condition 2: There's an active stage (Gate mode) OR cvUpdateMode is Always
    bool gateCondition = (_activeStage >= 0) || _discreteMapTrack.cvUpdateMode() == DiscreteMapTrack::CvUpdateMode::Always;

    // Update CV if both conditions are satisfied
    shouldOutputCv = muteCondition && gateCondition;

    if (shouldOutputCv) {
        if (_activeStage >= 0) {
            _targetCv = noteIndexToVoltage(_sequence->stage(_activeStage).noteIndex());
            _targetCv += _discreteMapTrack.offset() * 0.01f;
        } else {
            _targetCv = 0.0f;  // Default to 0V when no stage is active in Always mode
        }

        if (_sequence->slewEnabled()) {
            // Simple slew rate (can be made configurable later)
            // Use exponential slew for smooth transitions
            float slewRate = 0.1f;
            _cvOutput += (_targetCv - _cvOutput) * slewRate;
        } else {
            _cvOutput = _targetCv;
        }
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

bool DiscreteMapTrackEngine::updateExternalOnce() {
    bool extOnceMode = _sequence->clockSource() == DiscreteMapSequence::ClockSource::External && !_sequence->loop();
    if (!extOnceMode) {
        return false;
    }

    float winLo = std::min(rangeMin(), rangeMax());
    float winHi = std::max(rangeMin(), rangeMax());
    float spanAbs = std::max(kMinSpanAbs, std::abs(rangeSpan()));

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
            if (coveragePercent >= kCoveragePct) {
                _extOnceArmed = false;
                _extOnceDone = true;
            }
        } else {
            // Arm when entering the voltage window (with 5% tolerance)
            float armTolerance = spanAbs * kArmTolerancePct;
            if (_currentInput >= (winLo - armTolerance) && _currentInput <= (winHi + armTolerance)) {
                _extOnceArmed = true;
                _extMinSeen = _extMaxSeen = _currentInput;
            }
        }
    }

    return _extOnceDone || !_extOnceArmed;
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
    if (sequence.clockSource() == DiscreteMapSequence::ClockSource::InternalTriangle) {
        float triPhase = (_rampPhase < 0.5f) ? (_rampPhase * 2.0f) : (1.0f - (_rampPhase - 0.5f) * 2.0f);
        _rampValue = kInternalRampMin + triPhase * (kInternalRampMax - kInternalRampMin);
    } else {
        _rampValue = kInternalRampMin + _rampPhase * (kInternalRampMax - kInternalRampMin);
    }

    if (!_sequence->loop() && _running && posInPeriod + 1 >= periodTicks) {
        _running = false;
        _rampValue = kInternalRampMax;
        _rampPhase = 1.0f;
    }
}

float DiscreteMapTrackEngine::getRoutedInput() {
    return _discreteMapTrack.routedInput();
}

float DiscreteMapTrackEngine::getThresholdVoltage(int stageIndex) {
    if (_sequence->thresholdMode() == DiscreteMapSequence::ThresholdMode::Position) {
        return _positionThresholds[stageIndex];
    }
    return _lengthThresholds[stageIndex];
}

void DiscreteMapTrackEngine::recalculatePositionThresholds() {
    float minV = rangeMin();
    float spanV = rangeMax() - minV;

    for (int i = 0; i < DiscreteMapSequence::StageCount; i++) {
        const auto &stage = _sequence->stage(i);
        // Position mode: use threshold value directly as voltage
        // Map from -100..+100 to rangeMin..rangeMax
        float normalizedThreshold = (stage.threshold() + 100) / 200.0f;
        _positionThresholds[i] = minV + normalizedThreshold * spanV;
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

    // Use sampled values in Gate mode, current values in Always mode
    int octave, transpose, rootNote;
    if (_discreteMapTrack.cvUpdateMode() == DiscreteMapTrack::CvUpdateMode::Gate) {
        octave = _sampledOctave;
        transpose = _sampledTranspose;
        rootNote = _sampledRootNote;
    } else {
        octave = _discreteMapTrack.octave();
        transpose = _discreteMapTrack.transpose();
        rootNote = _sequence->rootNote();
    }

    int shift = octave * scale.notesPerOctave() + transpose;

    // Convert note index to volts using scale. For chromatic scales add rootNote in semitones.
    float volts = scale.noteToVolts(noteIndex + shift);
    if (scale.isChromatic()) {
        volts += rootNote * (1.f / 12.f);
    }
    return volts;
}
