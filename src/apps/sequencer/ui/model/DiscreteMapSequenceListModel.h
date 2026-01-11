#pragma once

#include "Config.h"

#include "RoutableListModel.h"

#include "model/DiscreteMapSequence.h"

class DiscreteMapSequenceListModel : public RoutableListModel {
public:
    DiscreteMapSequenceListModel() = default;

    enum Item {
        ClockSource,
        SyncMode,
        Divisor,
        ClockMult,
        GateLength,
        Loop,
        ResetMeasure,
        ThresholdMode,
        RangeHigh,
        RangeLow,
        Scale,
        RootNote,
        SlewTime,
        Pluck,
        Octave,
        Transpose,
        Offset,
        Last
    };

    void setSequence(DiscreteMapSequence *sequence) { _sequence = sequence; }

    virtual int rows() const override { return _sequence ? Last : 0; }
    virtual int columns() const override { return 2; }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (!_sequence) {
            return;
        }
        if (column == 0) {
            formatName(Item(row), str);
        } else if (column == 1) {
            formatValue(Item(row), str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (!_sequence || column != 1) {
            return;
        }
        editValue(Item(row), value, shift);
    }

    virtual Routing::Target routingTarget(int row) const override {
        switch (Item(row)) {
        case Divisor:
            return Routing::Target::Divisor;
        case ClockMult:
            return Routing::Target::ClockMult;
        case RangeHigh:
            return Routing::Target::DiscreteMapRangeHigh;
        case RangeLow:
            return Routing::Target::DiscreteMapRangeLow;
        case Scale:
            return Routing::Target::Scale;
        case RootNote:
            return Routing::Target::RootNote;
        case SlewTime:
            return Routing::Target::SlideTime;
        case Pluck:
            return Routing::Target::None;
        case Octave:
            return Routing::Target::Octave;
        case Transpose:
            return Routing::Target::Transpose;
        case Offset:
            return Routing::Target::Offset;
        default:
            return Routing::Target::None;
        }
    }

private:
    static const char *itemName(Item item) {
        switch (item) {
        case ClockSource:   return "Clock";
        case SyncMode:      return "Sync";
        case Divisor:       return "Divisor";
        case ClockMult:     return "Clock Mult";
        case GateLength:    return "Gate Len";
        case Loop:          return "Loop";
        case ResetMeasure:  return "Reset Measure";
        case ThresholdMode: return "Threshold";
        case RangeHigh:     return "Above";
        case RangeLow:      return "Below";
        case Scale:         return "Scale";
        case RootNote:      return "Root";
        case SlewTime:      return "Slew";
        case Pluck:         return "Pluck";
        case Octave:        return "Octave";
        case Transpose:     return "Transpose";
        case Offset:        return "Offset";
        case Last:          break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        switch (item) {
        case ClockSource:
            _sequence->printClockSource(str);
            break;
        case SyncMode:
            _sequence->printSyncMode(str);
            break;
        case Divisor:
            _sequence->printDivisor(str);
            break;
        case ClockMult:
            _sequence->printClockMultiplier(str);
            break;
        case GateLength:
            _sequence->printGateLength(str);
            break;
        case Loop:
            _sequence->printLoop(str);
            break;
        case ResetMeasure:
            _sequence->printResetMeasure(str);
            break;
        case ThresholdMode:
            _sequence->printThresholdMode(str);
            break;
        case RangeHigh:
            _sequence->printRangeHigh(str);
            break;
        case RangeLow:
            _sequence->printRangeLow(str);
            break;
        case Scale:
            _sequence->printScale(str);
            break;
        case RootNote:
            _sequence->printRootNote(str);
            break;
        case SlewTime:
            _sequence->printSlewTime(str);
            break;
        case Pluck:
            _sequence->printPluck(str);
            break;
        case Octave:
            str("%+d", _sequence->octave());
            break;
        case Transpose:
            str("%+d", _sequence->transpose());
            break;
        case Offset:
            str("%+.2fV", _sequence->offset() * 0.01f);
            break;
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case ClockSource:
            _sequence->toggleClockSource();
            break;
        case SyncMode:
            _sequence->cycleSyncMode();
            break;
        case Divisor:
            _sequence->editDivisor(value, shift);
            break;
        case ClockMult:
            _sequence->editClockMultiplier(value, shift);
            break;
        case GateLength:
            _sequence->editGateLength(value, shift);
            break;
        case Loop:
            _sequence->toggleLoop();
            break;
        case ResetMeasure:
            _sequence->editResetMeasure(value, shift);
            break;
        case ThresholdMode:
            _sequence->toggleThresholdMode();
            break;
        case RangeHigh:
            _sequence->editRangeHigh(value, shift);
            break;
        case RangeLow:
            _sequence->editRangeLow(value, shift);
            break;
        case Scale:
            _sequence->editScale(value, shift);
            break;
        case RootNote:
            _sequence->editRootNote(value, shift);
            break;
        case SlewTime:
            _sequence->editSlewTime(value, shift);
            break;
        case Pluck:
            _sequence->editPluck(value, shift);
            break;
        case Octave:
            _sequence->setOctave(ModelUtils::adjusted(_sequence->octave(), value, -10, 10));
            break;
        case Transpose:
            _sequence->setTranspose(ModelUtils::adjusted(_sequence->transpose(), value, -60, 60));
            break;
        case Offset:
            _sequence->setOffset(ModelUtils::adjusted(_sequence->offset(), value * (shift ? 1 : 10), -500, 500));
            break;
        case Last:
            break;
        }
    }

    DiscreteMapSequence *_sequence = nullptr;
};
