#include "Accumulator.h"
#include "engine/AccumulatorOps.h"
#include "core/utils/Random.h"
#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"
#include <algorithm> // For std::min and std::max

Accumulator::Accumulator() :
    _mode(Track),
    _polarity(Unipolar),
    _direction(Up),
    _order(Wrap),
    _enabled(true),  // Default: ON for easier testing
    _ratchetTriggerMode(First),
    _triggerMode(Step),
    _currentValue(0),
    _pendulumDirection(1),
    _hasStarted(false),
    _minValue(0),
    _maxValue(7),
    _stepValue(1),
    _ratchetTriggerParam(0)
{
    // Initialize currentValue to minValue after both are initialized
    _currentValue = _minValue;
}

void Accumulator::tick() const {
    if (!_enabled) return;

    // Delay first tick - skip the first call to tick()
    if (!_hasStarted) {
        const_cast<Accumulator*>(this)->_hasStarted = true;
        return; // Skip first tick
    }

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

void Accumulator::reset() {
    _currentValue = _minValue;
    _pendulumDirection = 1; // Reset pendulum direction to up
    _hasStarted = false; // Reset delayed start flag
}

// All four delegate to the shared AccumulatorOps. _currentValue / _pendulumDirection
// are `mutable`, so no const_cast is needed; bridge through a local int because the
// ops take int& and the members are int16_t/int8_t. Direction: Up->+1, Down->-1,
// Freeze->0 (the ops treat direction 0 as a no-op).

void Accumulator::tickWithWrap() const {
    int dir = (_direction == Up) ? 1 : (_direction == Down) ? -1 : 0;
    int v = _currentValue;
    AccumulatorOps::tickWrap(v, dir, _stepValue, _minValue, _maxValue);
    _currentValue = int16_t(v);
}

void Accumulator::tickWithPendulum() const {
    int dir = (_direction == Up) ? 1 : (_direction == Down) ? -1 : 0;
    int v = _currentValue;
    int pdir = _pendulumDirection;
    AccumulatorOps::tickPendulum(v, pdir, dir, _stepValue, _minValue, _maxValue);
    _currentValue = int16_t(v);
    _pendulumDirection = int8_t(pdir);
}

void Accumulator::tickWithRandom() const {
    int dir = (_direction == Up) ? 1 : (_direction == Down) ? -1 : 0;
    int v = _currentValue;
    AccumulatorOps::tickRandom(v, dir, _minValue, _maxValue);
    _currentValue = int16_t(v);
}

void Accumulator::tickWithHold() const {
    int dir = (_direction == Up) ? 1 : (_direction == Down) ? -1 : 0;
    int v = _currentValue;
    AccumulatorOps::tickHold(v, dir, _stepValue, _minValue, _maxValue);
    _currentValue = int16_t(v);
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

    // Pack hasStarted and triggerMode into single byte
    uint8_t flags2 = (_hasStarted ? 1 : 0) | (_triggerMode << 1);
    writer.write(flags2);
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

    // Unpack hasStarted and triggerMode from single byte
    uint8_t flags2;
    reader.read(flags2);
    _hasStarted = (flags2 & 0x01) != 0;
    _triggerMode = (flags2 >> 1) & 0x03;
}