#pragma once

#include "Config.h"

#include "RoutableListModel.h"

#include "model/DiscreteMapSequence.h"
#include "model/DiscreteMapTrack.h"

class DiscreteMapSequenceListModel : public RoutableListModel {
public:
    DiscreteMapSequenceListModel() = default;

    void setSequence(DiscreteMapSequence *sequence) { _sequence = sequence; }
    void setTrack(DiscreteMapTrack *track) { _track = track; }

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
        case RangeHigh:
            return Routing::Target::DiscreteMapRangeHigh;
        case RangeLow:
            return Routing::Target::DiscreteMapRangeLow;
        case Scale:
            return Routing::Target::Scale;
        case RootNote:
            return Routing::Target::RootNote;
        case Octave:
            return Routing::Target::Octave;
        case Transpose:
            return Routing::Target::Transpose;
        case CvUpdateMode:
            return Routing::Target::None;  // Not routable
        default:
            return Routing::Target::None;
        }
    }

private:
    enum Item {
        ClockSource,
        SyncMode,
        Divisor,
        GateLength,
        Loop,
        ResetMeasure,
        ThresholdMode,
        RangeHigh,
        RangeLow,
        Scale,
        RootNote,
        Slew,
        Octave,
        Transpose,
        Offset,
        CvUpdateMode,  // Add this before Last
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case ClockSource:   return "Clock";
        case SyncMode:      return "Sync";
        case Divisor:       return "Divisor";
        case GateLength:    return "Gate Len";
        case Loop:          return "Loop";
        case ResetMeasure:  return "Reset Measure";
        case ThresholdMode: return "Threshold";
        case RangeHigh:     return "Above";
        case RangeLow:      return "Below";
        case Scale:         return "Scale";
        case RootNote:      return "Root";
        case Slew:          return "Slew";
        case Octave:        return "Octave";
        case Transpose:     return "Transpose";
        case Offset:        return "Offset";
        case CvUpdateMode:  return "CV Update";
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
        case Slew:
            _sequence->printSlew(str);
            break;
        case Octave:
            if (_track) str("%+d", _track->octave());
            break;
        case Transpose:
            if (_track) str("%+d", _track->transpose());
            break;
        case Offset:
            if (_track) str("%+.2fV", _track->offset() * 0.01f);
            break;
        case CvUpdateMode:
            if (_track) _track->printCvUpdateMode(str);
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
        case Slew:
            _sequence->toggleSlew();
            break;
        case Octave:
            if (_track) _track->setOctave(ModelUtils::adjusted(_track->octave(), value, -10, 10));
            break;
        case Transpose:
            if (_track) _track->setTranspose(ModelUtils::adjusted(_track->transpose(), value, -60, 60));
            break;
        case Offset:
            if (_track) _track->setOffset(ModelUtils::adjusted(_track->offset(), value * (shift ? 1 : 10), -500, 500));
            break;
        case CvUpdateMode:
            if (_track) _track->editCvUpdateMode(value, shift);
            break;
        case Last:
            break;
        }
    }

    DiscreteMapSequence *_sequence = nullptr;
    DiscreteMapTrack *_track = nullptr;
};
