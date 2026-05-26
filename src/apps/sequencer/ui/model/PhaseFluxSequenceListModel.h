#pragma once

#include "Config.h"
#include "RoutableListModel.h"
#include "model/PhaseFluxSequence.h"

class PhaseFluxSequenceListModel : public RoutableListModel {
public:
    void setSequence(PhaseFluxSequence *sequence) { _sequence = sequence; }

    virtual int rows() const override { return _sequence ? Last : 0; }
    virtual int columns() const override { return 2; }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (!_sequence) return;
        if (column == 0) formatName(Item(row), str);
        else if (column == 1) formatValue(Item(row), str);
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (!_sequence || column != 1) return;
        editValue(Item(row), value, shift);
    }

    virtual Routing::Target routingTarget(int row) const override {
        switch (Item(row)) {
        case Divisor:    return Routing::Target::Divisor;
        case ClockMult:  return Routing::Target::ClockMult;
        case Scale:      return Routing::Target::Scale;
        case RootNote:   return Routing::Target::RootNote;
        default:         return Routing::Target::None;
        }
    }

private:
    enum Item {
        Divisor,
        ClockMult,
        GlobalPhase,
        PitchMode,
        PitchRate,
        Scale,
        RootNote,
        ResetMeasure,
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case Divisor:      return "Divisor";
        case ClockMult:    return "Clock Mult";
        case GlobalPhase:  return "Global Phase";
        case PitchMode:    return "Pitch Mode";
        case PitchRate:    return "Pitch Rate";
        case Scale:        return "Scale";
        case RootNote:     return "Root";
        case ResetMeasure: return "Reset Measure";
        case Last:         break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const { str(itemName(item)); }

    void formatValue(Item item, StringBuilder &str) const {
        switch (item) {
        case Divisor:      _sequence->printDivisor(str); break;
        case ClockMult:    _sequence->printClockMultiplier(str); break;
        case GlobalPhase:  _sequence->printGlobalPhase(str); break;
        case PitchMode:    _sequence->printPitchMode(str); break;
        case PitchRate:    _sequence->printPitchRate(str); break;
        case Scale:        _sequence->printScale(str); break;
        case RootNote:     _sequence->printRootNote(str); break;
        case ResetMeasure: _sequence->printResetMeasure(str); break;
        case Last:         break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case Divisor:      _sequence->editDivisor(value, shift); break;
        case ClockMult:    _sequence->editClockMultiplier(value, shift); break;
        case GlobalPhase:  _sequence->editGlobalPhase(value, shift); break;
        case PitchMode:    _sequence->editPitchMode(value, shift); break;
        case PitchRate:    _sequence->editPitchRate(value, shift); break;
        case Scale:        _sequence->editScale(value, shift); break;
        case RootNote:     _sequence->editRootNote(value, shift); break;
        case ResetMeasure: _sequence->editResetMeasure(value, shift); break;
        case Last:         break;
        }
    }

    PhaseFluxSequence *_sequence = nullptr;
};
