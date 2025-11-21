#pragma once

#include "Config.h"

#include "RoutableListModel.h"

#include "model/TuesdayTrack.h"

class TuesdaySequenceListModel : public RoutableListModel {
public:
    void setTrack(TuesdayTrack &track) {
        _track = &track;
    }

    virtual int rows() const override {
        return Last;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (column == 0) {
            formatName(Item(row), str);
        } else if (column == 1) {
            formatValue(Item(row), str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            editValue(Item(row), value, shift);
        }
    }

    virtual Routing::Target routingTarget(int row) const override {
        // Sequence parameters don't have routing targets yet
        return Routing::Target::None;
    }

private:
    enum Item {
        Divisor,
        ResetMeasure,
        Scale,
        RootNote,
        Octave,
        Transpose,
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case Divisor:       return "Divisor";
        case ResetMeasure:  return "Reset Measure";
        case Scale:         return "Scale";
        case RootNote:      return "Root Note";
        case Octave:        return "Octave";
        case Transpose:     return "Transpose";
        case Last:          break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        switch (item) {
        case Divisor:
            _track->printDivisor(str);
            break;
        case ResetMeasure:
            _track->printResetMeasure(str);
            break;
        case Scale:
            _track->printScale(str);
            break;
        case RootNote:
            _track->printRootNote(str);
            break;
        case Octave:
            _track->printOctave(str);
            break;
        case Transpose:
            _track->printTranspose(str);
            break;
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case Divisor:
            _track->editDivisor(value, shift);
            break;
        case ResetMeasure:
            _track->editResetMeasure(value, shift);
            break;
        case Scale:
            _track->editScale(value, shift);
            break;
        case RootNote:
            _track->editRootNote(value, shift);
            break;
        case Octave:
            _track->editOctave(value, shift);
            break;
        case Transpose:
            _track->editTranspose(value, shift);
            break;
        case Last:
            break;
        }
    }

    TuesdayTrack *_track;
};
