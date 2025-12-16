#include "DiscreteMapTrackEngine.h"

#include "Engine.h"

#include <cmath>

void DiscreteMapTrackEngine::reset() {
    _rampPhase = 0.0f;
    _rampValue = rangeMin();
    _prevInput = rangeMin() - 0.001f;  // Ensure first RISE can trigger
    _currentInput = rangeMin();
    _activeStage = -1;
    _cvOutput = 0.0f;
    _targetCv = 0.0f;
    _running = true;
    _thresholdsDirty = true;
    _activity = false;

    _sequence = &_discreteMapTrack.sequence(pattern());
}

void DiscreteMapTrackEngine::restart() {
    _rampPhase = 0.0f;
    _running = true;
}

void DiscreteMapTrackEngine::changePattern() {
    _sequence = &_discreteMapTrack.sequence(pattern());
    _thresholdsDirty = true;
}

TrackEngine::TickResult DiscreteMapTrackEngine::tick(uint32_t tick) {
    _sequence = &_discreteMapTrack.sequence(pattern());

    // 1. Update input source
    if (_sequence->clockSource() == DiscreteMapSequence::ClockSource::Internal) {
        if (_running || _sequence->loop()) {
            updateRamp(tick);
        }
        _currentInput = _rampValue;
    } else {
        _currentInput = getRoutedInput();
    }

    // 2. Recalc length thresholds if needed
    if (_thresholdsDirty && _sequence->thresholdMode() == DiscreteMapSequence::ThresholdMode::Length) {
        recalculateLengthThresholds();
        _thresholdsDirty = false;
    }

    // 3. Find active stage from threshold crossings
    int newStage = findActiveStage(_currentInput, _prevInput);

    // Activity detection: true if stage changed
    _activity = (newStage != _activeStage && newStage >= 0);

    float prevCv = _cvOutput;
    bool gateChanged = newStage != _activeStage;

    _activeStage = newStage;

    // 4. Update CV output
    if (_activeStage >= 0) {
        _targetCv = noteIndexToVoltage(_sequence->stage(_activeStage).noteIndex());

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

    if (gateChanged) {
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

    float min = rangeMin();
    float max = rangeMax();
    _rampValue = min + _rampPhase * (max - min);

    if (!_sequence->loop() && _running && posInPeriod + 1 >= periodTicks) {
        _running = false;
        _rampValue = max;
        _rampPhase = 1.0f;
    }
}

float DiscreteMapTrackEngine::getRoutedInput() {
    // TODO: Implement routing system integration
    // For now, return 0 (will be implemented when routing is set up)
    return 0.0f;
}

float DiscreteMapTrackEngine::getThresholdVoltage(int stageIndex) {
    const auto &stage = _sequence->stage(stageIndex);

    if (_sequence->thresholdMode() == DiscreteMapSequence::ThresholdMode::Position) {
        // Position mode: use threshold value directly as voltage
        // Map from -127..+128 to rangeMin..rangeMax
        float normalizedThreshold = (stage.threshold() + 127) / 255.0f;
        return rangeMin() + normalizedThreshold * (rangeMax() - rangeMin());
    } else {
        // Length mode: use pre-calculated thresholds
        return _lengthThresholds[stageIndex];
    }
}

void DiscreteMapTrackEngine::recalculateLengthThresholds() {
    // Length mode: distribute thresholds proportionally based on absolute threshold values
    float totalWeight = 0;
    for (int i = 0; i < DiscreteMapSequence::StageCount; i++) {
        totalWeight += std::abs(_sequence->stage(i).threshold());
    }

    if (totalWeight == 0) {
        totalWeight = 1;  // Avoid division by zero
    }

    float range = rangeMax() - rangeMin();
    float cumulative = rangeMin();

    for (int i = 0; i < DiscreteMapSequence::StageCount; i++) {
        float proportion = std::abs(_sequence->stage(i).threshold()) / totalWeight;
        cumulative += proportion * range;
        _lengthThresholds[i] = cumulative;
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

    // No crossing detected, keep current active stage
    return _activeStage;
}

float DiscreteMapTrackEngine::noteIndexToVoltage(int8_t noteIndex) {
    const Scale &scale = _sequence->selectedScale(_model.project().selectedScale());

    // Convert note index to volts using scale. For chromatic scales add rootNote in semitones.
    float volts = scale.noteToVolts(noteIndex);
    if (scale.isChromatic()) {
        volts += _sequence->rootNote() * (1.f / 12.f);
    }
    return volts;
}
