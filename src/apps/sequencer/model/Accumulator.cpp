#include "Accumulator.h"
#include <algorithm> // For std::min and std::max

Accumulator::Accumulator() :
    _mode(Track),
    _polarity(Unipolar),
    _direction(Up),
    _order(Wrap),
    _enabled(false),
    _ratchetTriggerMode(First),
    _currentValue(0),
    _minValue(-7),
    _maxValue(7),
    _stepValue(1),
    _ratchetTriggerParam(0)
{
}

void Accumulator::tick() const {
    if (_enabled) {
        if (_direction == Up) {
            const_cast<Accumulator*>(this)->_currentValue += _stepValue;
            if (_order == Wrap && _currentValue > _maxValue) {
                const_cast<Accumulator*>(this)->_currentValue = _minValue + (_currentValue - _maxValue - 1);
            }
        } else if (_direction == Down) {
            const_cast<Accumulator*>(this)->_currentValue -= _stepValue;
            if (_order == Wrap && _currentValue < _minValue) {
                const_cast<Accumulator*>(this)->_currentValue = _maxValue - (_minValue - _currentValue - 1);
            }
        }
        // Apply clamping if not wrapping
        if (_order != Wrap) {
            const_cast<Accumulator*>(this)->_currentValue = std::max(_minValue, std::min(_currentValue, _maxValue));
        }
    }
}