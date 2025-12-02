#pragma once

#include "Config.h"

#include "RoutableListModel.h"

#include "model/TuesdaySequence.h"

class TuesdaySequenceListModel : public RoutableListModel {
public:
    TuesdaySequenceListModel()
    {}

    void setSequence(TuesdaySequence *sequence) {
        _sequence = sequence;
    }

    virtual int rows() const override {
        return _sequence ? Last : 0;
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
        switch (Item(row)) {
        case Algorithm:
            return Routing::Target::Algorithm;
        case Flow:
            return Routing::Target::Flow;
        case Ornament:
            return Routing::Target::Ornament;
        case Power:
            return Routing::Target::Power;
        case Glide:
            return Routing::Target::Glide;
        case Trill:
            return Routing::Target::Trill;
        case GateOffset:
            return Routing::Target::GateOffset;
        case Scan:
            return Routing::Target::Scan;
        case Rotate:
            return Routing::Target::Rotate;
        case Octave:
            return Routing::Target::Octave;
        case Transpose:
            return Routing::Target::Transpose;
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
        Algorithm,
        Flow,
        Ornament,
        Power,
        LoopLength,
        Scan,
        Rotate,
        Glide,
        Skew,
        GateOffset,
        CvUpdateMode,
        Trill,
        UseScale,
        Octave,
        Transpose,
        Divisor,
        ResetMeasure,
        Scale,
        RootNote,
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case Algorithm:     return "Algorithm";
        case Flow:          return "Flow";
        case Ornament:      return "Ornament";
        case Power:         return "Power";
        case LoopLength:    return "Loop Length";
        case Scan:          return "Scan";
        case Rotate:        return "Rotate";
        case Glide:         return "Glide";
        case Skew:          return "Skew";
        case GateOffset:    return "Gate Offset";
        case CvUpdateMode:  return "CV Mode";
        case Trill:         return "Trill";
        case UseScale:      return "Use Scale";
        case Octave:        return "Octave";
        case Transpose:     return "Transpose";
        case Divisor:       return "Divisor";
        case ResetMeasure:  return "Reset Measure";
        case Scale:         return "Scale";
        case RootNote:      return "Root Note";
        case Last:          break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        switch (item) {
        case Algorithm:
            _sequence->printAlgorithm(str);
            break;
        case Flow:
            _sequence->printFlow(str);
            break;
        case Ornament:
            _sequence->printOrnament(str);
            break;
        case Power:
            _sequence->printPower(str);
            break;
        case LoopLength:
            _sequence->printLoopLength(str);
            break;
        case Scan:
            _sequence->printScan(str);
            break;
        case Rotate:
            _sequence->printRotate(str);
            break;
        case Glide:
            _sequence->printGlide(str);
            break;
        case Skew:
            _sequence->printSkew(str);
            break;
        case GateOffset:
            _sequence->printGateOffset(str);
            break;
        case CvUpdateMode:
            _sequence->printCvUpdateMode(str);
            break;
        case Trill:
            _sequence->printTrill(str);
            break;
        case UseScale:
            _sequence->printUseScale(str);
            break;
        case Octave:
            _sequence->printOctave(str);
            break;
        case Transpose:
            _sequence->printTranspose(str);
            break;
        case Divisor:
            _sequence->printDivisor(str);
            break;
        case ResetMeasure:
            _sequence->printResetMeasure(str);
            break;
        case Scale:
            _sequence->printScale(str);
            break;
        case RootNote:
            _sequence->printRootNote(str);
            break;
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case Algorithm:
            _sequence->editAlgorithm(value, shift);
            break;
        case Flow:
            _sequence->editFlow(value, shift);
            break;
        case Ornament:
            _sequence->editOrnament(value, shift);
            break;
        case Power:
            _sequence->editPower(value, shift);
            break;
        case LoopLength:
            _sequence->editLoopLength(value, shift);
            break;
        case Scan:
            _sequence->editScan(value, shift);
            break;
        case Rotate:
            _sequence->editRotate(value, shift);
            break;
        case Glide:
            _sequence->editGlide(value, shift);
            break;
        case Skew:
            _sequence->editSkew(value, shift);
            break;
        case GateOffset:
            _sequence->editGateOffset(value, shift);
            break;
        case CvUpdateMode:
            _sequence->editCvUpdateMode(value, shift);
            break;
        case Trill:
            _sequence->editTrill(value, shift);
            break;
        case UseScale:
            _sequence->editUseScale(value, shift);
            break;
        case Octave:
            _sequence->editOctave(value, shift);
            break;
        case Transpose:
            _sequence->editTranspose(value, shift);
            break;
        case Divisor:
            _sequence->editDivisor(value, shift);
            break;
        case ResetMeasure:
            _sequence->editResetMeasure(value, shift);
            break;
        case Scale:
            _sequence->editScale(value, shift);
            break;
        case RootNote:
            _sequence->editRootNote(value, shift);
            break;
        case Last:
            break;
        }
    }

    TuesdaySequence *_sequence;
};
