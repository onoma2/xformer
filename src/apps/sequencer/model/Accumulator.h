#pragma once

#include <cstdint>
#include <algorithm> // For std::min and std::max
#include "core/utils/Random.h"

class VersionedSerializedWriter;
class VersionedSerializedReader;

class Accumulator {
public:
    enum Mode { Stage, Track };
    enum Polarity { Unipolar, Bipolar };
    enum Direction { Up, Down, Freeze };
    enum Order { Wrap, Pendulum, Random, Hold };

    enum RatchetTriggerMode {
        First,
        All,
        Last,
        EveryN,
        RandomTrigger
    };

    Accumulator();

    void setEnabled(bool enabled) { _enabled = enabled; }
    bool enabled() const { return _enabled; } // Added
    void setStepValue(uint8_t stepValue) { _stepValue = stepValue; }
    void setMinValue(int16_t minValue) { _minValue = minValue; }
    void setMaxValue(int16_t maxValue) { _maxValue = maxValue; }
    void setOrder(Order order) { _order = order; }
    void setDirection(Direction direction) { _direction = direction; }
    void tick() const;
    void reset();
    int16_t currentValue() const { return _currentValue; }
    uint8_t stepValue() const { return _stepValue; }
    int16_t minValue() const { return _minValue; }
    int16_t maxValue() const { return _maxValue; }
    Mode mode() const { return static_cast<Mode>(_mode); }
    Polarity polarity() const { return static_cast<Polarity>(_polarity); }
    Direction direction() const { return static_cast<Direction>(_direction); }
    Order order() const { return static_cast<Order>(_order); }
    void setMode(Mode mode) { _mode = mode; }
    void setPolarity(Polarity polarity) { _polarity = polarity; }

    // Serialization
    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    void tickWithWrap() const;
    void tickWithPendulum() const;
    void tickWithRandom() const;
    void tickWithHold() const;

    uint8_t _mode : 2;
    uint8_t _polarity : 1;
    uint8_t _direction : 2;
    uint8_t _order : 2;
    uint8_t _enabled : 1;
    uint8_t _ratchetTriggerMode : 3;

    mutable int16_t _currentValue;  // Mark as mutable to allow modification through const references
    mutable int8_t _pendulumDirection; // For Pendulum mode: 1 for up, -1 for down
    int16_t _minValue;
    int16_t _maxValue;
    uint8_t _stepValue;
    uint8_t _ratchetTriggerParam;
};