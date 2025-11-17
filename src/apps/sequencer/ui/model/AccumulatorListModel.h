#pragma once

#include "ListModel.h"

#include "model/NoteSequence.h"

class AccumulatorListModel : public ListModel {
public:
    enum Item {
        Enabled,
        Direction,
        Order,
        MinValue,
        MaxValue,
        StepValue,
        CurrentValue,
        Last
    };

    AccumulatorListModel() : _sequence(nullptr) {}

    void setSequence(NoteSequence *sequence) {
        _sequence = sequence;
    }

    virtual int rows() const override {
        return _sequence ? Last : 0;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (!_sequence) return;
        
        if (column == 0) {
            formatName(Item(row), str);
        } else if (column == 1) {
            formatValue(Item(row), str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (!_sequence || column != 1) return;

        // Check if this row has indexed values (Direction or Order)
        int count = indexedCount(row);
        if (count > 0) {
            // For indexed values, cycle through the options
            int current = indexed(row);
            if (current >= 0) {
                int next = (current + value) % count;
                // Handle negative values properly
                if (next < 0) next += count;
                setIndexed(row, next);
            }
        } else {
            // For non-indexed values, use the original editValue method
            editValue(Item(row), value, shift);
        }
    }

    virtual int indexedCount(int row) const override {
        if (!_sequence) return 0;
        
        switch (Item(row)) {
        case Direction:
            return 3; // Up, Down, Freeze
        case Order:
            return 4; // Wrap, Pendulum, Random, Hold
        default:
            return 0;
        }
    }

    virtual int indexed(int row) const override {
        if (!_sequence) return -1;
        
        switch (Item(row)) {
        case Direction:
            return static_cast<int>(_sequence->accumulator().direction());
        case Order:
            return static_cast<int>(_sequence->accumulator().order());
        default:
            return -1;
        }
    }

    virtual void setIndexed(int row, int index) override {
        if (!_sequence || index < 0) return;
        
        if (index < indexedCount(row)) {
            switch (Item(row)) {
            case Direction:
                const_cast<Accumulator&>(_sequence->accumulator()).setDirection(static_cast<Accumulator::Direction>(index));
                break;
            case Order:
                const_cast<Accumulator&>(_sequence->accumulator()).setOrder(static_cast<Accumulator::Order>(index));
                break;
            default:
                break;
            }
        }
    }

private:
    static const char *itemName(Item item) {
        switch (item) {
        case Enabled:         return "ENABLED";
        case Direction:       return "DIRECTN";
        case Order:           return "ORDER";
        case MinValue:        return "MIN";
        case MaxValue:        return "MAX";
        case StepValue:       return "STEP";
        case CurrentValue:    return "CURRENT";
        case Last:            break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        if (!_sequence) return;
        
        switch (item) {
        case Enabled:
            str(_sequence->accumulator().enabled() ? "ON" : "OFF");
            break;
        case Direction: {
            auto dir = _sequence->accumulator().direction();
            switch (dir) {
            case Accumulator::Direction::Up:     str("UP"); break;
            case Accumulator::Direction::Down:   str("DOWN"); break;
            case Accumulator::Direction::Freeze: str("FREEZE"); break;
            }
            break;
        }
        case Order: {
            auto order = _sequence->accumulator().order();
            switch (order) {
            case Accumulator::Order::Wrap:      str("WRAP"); break;
            case Accumulator::Order::Pendulum:  str("PEND"); break;
            case Accumulator::Order::Random:    str("RAND"); break;
            case Accumulator::Order::Hold:      str("HOLD"); break;
            }
            break;
        }
        case MinValue:
            str("%d", _sequence->accumulator().minValue());
            break;
        case MaxValue:
            str("%d", _sequence->accumulator().maxValue());
            break;
        case StepValue:
            str("%d", _sequence->accumulator().stepValue());
            break;
        case CurrentValue:
            str("%d", _sequence->accumulator().currentValue());
            break;
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        if (!_sequence) return;
        
        const int step = shift ? 10 : 1;
        
        switch (item) {
        case Enabled:
            const_cast<Accumulator&>(_sequence->accumulator()).setEnabled(!_sequence->accumulator().enabled());
            break;
        case MinValue:
            const_cast<Accumulator&>(_sequence->accumulator()).setMinValue(
                clamp(_sequence->accumulator().minValue() + value * step, -100, 100));
            break;
        case MaxValue:
            const_cast<Accumulator&>(_sequence->accumulator()).setMaxValue(
                clamp(_sequence->accumulator().maxValue() + value * step, -100, 100));
            break;
        case StepValue:
            const_cast<Accumulator&>(_sequence->accumulator()).setStepValue(
                clamp(_sequence->accumulator().stepValue() + value * step, 1, 100));
            break;
        default:
            break;
        }
    }

    int clamp(int value, int min, int max) const {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }

    NoteSequence *_sequence;
};