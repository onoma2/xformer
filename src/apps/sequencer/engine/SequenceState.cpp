#include "SequenceState.h"

#include "core/Debug.h"
#include "core/math/Math.h"

static int randomStep(int firstStep, int lastStep, Random &rng) {
    return rng.nextRange(lastStep - firstStep + 1) + firstStep;
}

void SequenceState::reset() {
    _step = -1;
    _prevStep = -1;
    _direction = 1;
    _iteration = 0;
}

void SequenceState::advanceFree(Types::RunMode runMode, int firstStep, int lastStep, Random &rng) {
     ASSERT(firstStep <= lastStep, "invalid first/last step");

   _prevStep = _step;

    if (_step == -1) {
        // first step
        switch (runMode) {
        case Types::RunMode::Forward:
        case Types::RunMode::Pendulum:
        case Types::RunMode::PingPong:
            _step = firstStep;
            _direction = 1;
            break;
        case Types::RunMode::Backward:
            _step = lastStep;
            _direction = -1;
            break;
        case Types::RunMode::Random:
        case Types::RunMode::RandomWalk:
            _step = randomStep(firstStep, lastStep, rng);
            _direction = 1;
            break;
        case Types::RunMode::Last:
            break;
        }
    } else {
        // advance step
        _step = clamp(int(_step), firstStep, lastStep);

        switch (runMode) {
        case Types::RunMode::Forward:
            _direction = 1;
            if (_step >= lastStep) {
                _step = firstStep;
                ++_iteration;
            } else {
                ++_step;
            }
            break;
        case Types::RunMode::Backward:
            _direction = -1;
            if (_step <= firstStep) {
                _step = lastStep;
                ++_iteration;
            } else {
                --_step;
            }
            break;
        case Types::RunMode::Pendulum:
        case Types::RunMode::PingPong:
            if (_direction > 0 && _step >= lastStep) {
                _direction = -1;
            } else if (_direction < 0 && _step <= firstStep) {
                _direction = 1;
                ++_iteration;
            }
            
            if (runMode == Types::RunMode::Pendulum) {
                if (_step + _direction >= firstStep && _step + _direction <= lastStep) {
                    _step += _direction;
                }
            } else { // PingPong
                _step += _direction;
            }
            break;
        case Types::RunMode::Random:
            _direction = 1;
            _step = randomStep(firstStep, lastStep, rng);
            break;
        case Types::RunMode::RandomWalk:
            advanceRandomWalk(firstStep, lastStep, rng);
            break;
        case Types::RunMode::Last:
            break;
        }
    }
}

void SequenceState::advanceAligned(int absoluteStep, Types::RunMode runMode, int firstStep, int lastStep, Random &rng) {
     ASSERT(firstStep <= lastStep, "invalid first/last step");

    _prevStep = _step;

    int stepCount = lastStep - firstStep + 1;

    switch (runMode) {
    case Types::RunMode::Forward:
        _direction = 1;
        _step = firstStep + absoluteStep % stepCount;
        _iteration = absoluteStep / stepCount;
        break;
    case Types::RunMode::Backward:
        _direction = -1;
        _step = lastStep - absoluteStep % stepCount;
        _iteration = absoluteStep / stepCount;
        break;
    case Types::RunMode::Pendulum:
        _iteration = absoluteStep / (2 * stepCount);
        absoluteStep %= (2 * stepCount);
        _direction = (absoluteStep < stepCount) ? 1 : -1;
        _step = (absoluteStep < stepCount) ? (firstStep + absoluteStep) : (lastStep - (absoluteStep - stepCount));
        break;
    case Types::RunMode::PingPong:
        _iteration = absoluteStep / (2 * stepCount - 2);
        absoluteStep %= (2 * stepCount - 2);
        _direction = (absoluteStep < stepCount) ? 1 : -1;
        _step = (absoluteStep < stepCount) ? (firstStep + absoluteStep) : (lastStep - (absoluteStep - stepCount) - 1);
        break;
    case Types::RunMode::Random:
        _direction = 1;
        _step = firstStep + rng.nextRange(stepCount);
        break;
    case Types::RunMode::RandomWalk:
        // direction is handled inside advanceRandomWalk
        advanceRandomWalk(firstStep, lastStep, rng);
        break;
    case Types::RunMode::Last:
        break;
    }
}

void SequenceState::setStep(int step, int firstStep, int lastStep) {
    ASSERT(firstStep <= lastStep, "invalid first/last step");

    _prevStep = _step;
    _step = clamp(step, firstStep, lastStep);
    _direction = (_prevStep <= _step) ? 1 : -1;
    if (_prevStep > _step) {
        ++_iteration;
    }
}

void SequenceState::advanceRandomWalk(int firstStep, int lastStep, Random &rng) {
    if (_step == -1) {
        _step = randomStep(firstStep, lastStep, rng);
        _direction = 1;
    } else {
        int dir = rng.nextRange(2);
        if (dir == 0) {
            _step = _step == firstStep ? lastStep : _step - 1;
            _direction = -1;
        } else {
            _step = _step == lastStep ? firstStep : _step + 1;
            _direction = 1;
        }
    }
}
