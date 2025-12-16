#pragma once

#include "Config.h"

#include "RoutableListModel.h"

#include "model/DiscreteMapSequence.h"

class DiscreteMapSequenceListModel : public RoutableListModel {
public:
    DiscreteMapSequenceListModel() = default;

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
        case Scale:
            return Routing::Target::Scale;
        case RootNote:
            return Routing::Target::RootNote;
        default:
            return Routing::Target::None;
        }
    }

private:
    enum Item {
        ClockSource,
        Divisor,
        Loop,
        ThresholdMode,
        ScaleSource,
        Scale,
        RootNote,
        Slew,
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case ClockSource:   return "Clock";
        case Divisor:       return "Divisor";
        case Loop:          return "Loop";
        case ThresholdMode: return "Threshold";
        case ScaleSource:   return "Scale Src";
        case Scale:         return "Scale";
        case RootNote:      return "Root";
        case Slew:          return "Slew";
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
        case Divisor:
            _sequence->printDivisor(str);
            break;
        case Loop:
            _sequence->printLoop(str);
            break;
        case ThresholdMode:
            _sequence->printThresholdMode(str);
            break;
        case ScaleSource:
            _sequence->printScaleSource(str);
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
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case ClockSource:
            _sequence->toggleClockSource();
            break;
        case Divisor:
            _sequence->editDivisor(value, shift);
            break;
        case Loop:
            _sequence->toggleLoop();
            break;
        case ThresholdMode:
            _sequence->toggleThresholdMode();
            break;
        case ScaleSource:
            _sequence->setScaleSource(
                _sequence->scaleSource() == DiscreteMapSequence::ScaleSource::Project
                    ? DiscreteMapSequence::ScaleSource::Track
                    : DiscreteMapSequence::ScaleSource::Project);
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
        case Last:
            break;
        }
    }

    DiscreteMapSequence *_sequence = nullptr;
};
