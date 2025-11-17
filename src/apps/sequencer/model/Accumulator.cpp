#include "Accumulator.h"
#include "core/utils/Random.h"
#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"
#include <algorithm> // For std::min and std::max

Accumulator::Accumulator() :
    _mode(Track),
    _polarity(Unipolar),
    _direction(Up),
    _order(Wrap),
    _enabled(false),
    _ratchetTriggerMode(First),
    _currentValue(0),
    _minValue(0),
    _maxValue(7),
    _stepValue(1),
    _ratchetTriggerParam(0),
    _pendulumDirection(1)
{
}

void Accumulator::tick() const {
    if (_enabled) {
        switch (_order) {
        case Wrap:
            tickWithWrap();
            break;
        case Pendulum:
            tickWithPendulum();
            break;
        case Random:
            tickWithRandom();
            break;
        case Hold:
            tickWithHold();
            break;
        }
    }
}

void Accumulator::tickWithWrap() const {
    if (_direction == Up) {
        const_cast<Accumulator*>(this)->_currentValue += _stepValue;
        if (_currentValue > _maxValue) {
            const_cast<Accumulator*>(this)->_currentValue = _minValue + (_currentValue - _maxValue - 1);
        }
    } else if (_direction == Down) {
        const_cast<Accumulator*>(this)->_currentValue -= _stepValue;
        if (_currentValue < _minValue) {
            const_cast<Accumulator*>(this)->_currentValue = _maxValue - (_minValue - _currentValue - 1);
        }
    }
}

void Accumulator::tickWithPendulum() const {
    if (_direction == Freeze) return;

    int step = _stepValue * _pendulumDirection;
    const_cast<Accumulator*>(this)->_currentValue += step;

    // Check for boundary conditions and reverse direction
    if (_currentValue >= _maxValue) {
        const_cast<Accumulator*>(this)->_currentValue = _maxValue;
        const_cast<Accumulator*>(this)->_pendulumDirection = -1; // Change direction to down
    } else if (_currentValue <= _minValue) {
        const_cast<Accumulator*>(this)->_currentValue = _minValue;
        const_cast<Accumulator*>(this)->_pendulumDirection = 1; // Change direction to up
    }
}

void Accumulator::tickWithRandom() const {
    if (_direction == Freeze) return;

    static ::Random rng;  // Use global namespace to avoid naming conflict with enum

    if (_minValue == _maxValue) {
        // If min equals max, just set to that value
        const_cast<Accumulator*>(this)->_currentValue = _minValue;
    } else {
        // Generate a random value between min and max
        int range = _maxValue - _minValue + 1; // +1 to make max inclusive
        int randomValue = _minValue + (int)(rng.nextRange(range));
        const_cast<Accumulator*>(this)->_currentValue = randomValue;
    }
}

void Accumulator::tickWithHold() const {
    if (_direction == Freeze) return;

    if (_direction == Up) {
        const_cast<Accumulator*>(this)->_currentValue += _stepValue;
        if (_currentValue >= _maxValue) {
            const_cast<Accumulator*>(this)->_currentValue = _maxValue;
        }
    } else if (_direction == Down) {
        const_cast<Accumulator*>(this)->_currentValue -= _stepValue;
        if (_currentValue <= _minValue) {
            const_cast<Accumulator*>(this)->_currentValue = _minValue;
        }
    }
}

void Accumulator::write(VersionedSerializedWriter &writer) const {
    // Write bitfield parameters as single byte
    uint8_t flags = (_mode << 0) | (_polarity << 2) |
                    (_direction << 3) | (_order << 5) |
                    (_enabled << 7);
    writer.write(flags);

    // Write value parameters
    writer.write(_minValue);
    writer.write(_maxValue);
    writer.write(_stepValue);
    writer.write(_currentValue);
    writer.write(_pendulumDirection);
}

void Accumulator::read(VersionedSerializedReader &reader) {
    // Read bitfield flags
    uint8_t flags;
    reader.read(flags);
    _mode = (flags >> 0) & 0x03;
    _polarity = (flags >> 2) & 0x01;
    _direction = (flags >> 3) & 0x03;
    _order = (flags >> 5) & 0x03;
    _enabled = (flags >> 7) & 0x01;

    // Read value parameters
    reader.read(_minValue);
    reader.read(_maxValue);
    reader.read(_stepValue);
    reader.read(_currentValue);
    reader.read(_pendulumDirection);
}