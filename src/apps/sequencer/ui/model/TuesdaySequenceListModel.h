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
        case GateLength:
            return Routing::Target::GateLength;
        case Rotate:
            return Routing::Target::Rotate;
        case Octave:
            return Routing::Target::Octave;
        case Transpose:
            return Routing::Target::Transpose;
        case Divisor:
            return Routing::Target::Divisor;
        case ClockMult:
            return Routing::Target::ClockMult;
        case Scale:
            return Routing::Target::Scale;
        case RootNote:
            return Routing::Target::RootNote;
        case Start:
            return Routing::Target::None;
        case MaskParameter:
            return Routing::Target::None; // Not routable
        case TimeMode:
            return Routing::Target::None; // Not routable
        case MaskProgression:
            return Routing::Target::None; // Not routable
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
        Rotate,
        Glide,
        Skew,
        GateLength, // Added
        CvUpdateMode,
        Trill,
        Start,
        Octave,
        Transpose,
        Divisor,
        ClockMult,
        ResetMeasure,
        Scale,
        RootNote,
        MaskParameter,
        TimeMode,
        MaskProgression,
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case Algorithm:     return "Algorithm";
        case Flow:          return "Flow";
        case Ornament:      return "Ornament";
        case Power:         return "Power";
        case LoopLength:    return "Loop Length";
        case Rotate:        return "Rotate";
        case Glide:         return "Glide";
        case Skew:          return "Skew";
        case GateLength:    return "Gate Length";
        case CvUpdateMode:  return "CV Mode";
        case Trill:         return "Old Trill";
        case Start:         return "Start";
        case Octave:        return "Octave";
        case Transpose:     return "Transpose";
        case Divisor:       return "Divisor";
        case ClockMult:     return "Clock Mult";
        case ResetMeasure:  return "Reset Measure";
        case Scale:         return "Scale";
        case RootNote:      return "Root Note";
        case MaskParameter: return "Mask Param";
        case TimeMode: return "Time Mode";
        case MaskProgression: return "Mask Prog";
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
        case Rotate:
            _sequence->printRotate(str);
            break;
        case Glide:
            _sequence->printGlide(str);
            break;
        case Skew:
            _sequence->printSkew(str);
            break;
        case GateLength:
            _sequence->printGateLength(str);
            break;
        case CvUpdateMode:
            _sequence->printCvUpdateMode(str);
            break;
        case Trill:
            _sequence->printTrill(str);
            break;
        case Start:
            _sequence->printStart(str);
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
        case ClockMult:
            _sequence->printClockMultiplier(str);
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
        case MaskParameter:
            _sequence->printMaskParameter(str);
            break;
        case TimeMode:
            _sequence->printTimeMode(str);
            break;
        case MaskProgression:
            _sequence->printMaskProgression(str);
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
        case Rotate:
            _sequence->editRotate(value, shift);
            break;
        case Glide:
            _sequence->editGlide(value, shift);
            break;
        case Skew:
            _sequence->editSkew(value, shift);
            break;
        case GateLength:
            _sequence->editGateLength(value, shift);
            break;
        case CvUpdateMode:
            _sequence->editCvUpdateMode(value, shift);
            break;
        case Trill:
            _sequence->editTrill(value, shift);
            break;
        case Start:
            _sequence->editStart(value, shift);
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
        case ClockMult:
            _sequence->editClockMultiplier(value, shift);
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
        case MaskParameter:
            _sequence->editMaskParameter(value, shift);
            break;
        case TimeMode:
            _sequence->editTimeMode(value, shift);
            break;
        case MaskProgression:
            _sequence->editMaskProgression(value, shift);
            break;
        case Last:
            break;
        }
    }

    TuesdaySequence *_sequence;
};
