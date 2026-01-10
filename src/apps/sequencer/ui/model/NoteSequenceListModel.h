#pragma once

#include "Config.h"

#include "RoutableListModel.h"

#include "model/NoteSequence.h"
#include "model/Scale.h"

class NoteSequenceListModel : public RoutableListModel {
public:
    enum Item {
        Mode,
        FirstStep,
        LastStep,
        NoteFirstStep,
        NoteLastStep,
        RunMode,
        DivisorX,
        DivisorY,
        ClockMult,
        ResetMeasure,
        Scale,
        RootNote,
        Last
    };

    NoteSequenceListModel()
    {}

    void setSequence(NoteSequence *sequence) {
        _sequence = sequence;
    }

    virtual int rows() const override {
        return _sequence ? itemCount() : 0;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (column == 0) {
            formatName(itemForRow(row), str);
        } else if (column == 1) {
            formatValue(itemForRow(row), str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            editValue(itemForRow(row), value, shift);
        }
    }

    virtual int indexedCount(int row) const override {
        return indexedCountValue(itemForRow(row));
    }

    virtual int indexed(int row) const override {
        return indexedValue(itemForRow(row));
    }

    virtual void setIndexed(int row, int index) override {
        if (index >= 0 && index < indexedCount(row)) {
            setIndexedValue(itemForRow(row), index);
        }
    }

    virtual Routing::Target routingTarget(int row) const override {
        switch (itemForRow(row)) {
        case DivisorX:
            return Routing::Target::Divisor;
        case ClockMult:
            return Routing::Target::ClockMult;
        case FirstStep:
            return Routing::Target::FirstStep;
        case LastStep:
            return Routing::Target::LastStep;
        case RunMode:
            return Routing::Target::RunMode;
        case Scale:
            return Routing::Target::Scale;
        case RootNote:
            return Routing::Target::RootNote;
        default:
            return Routing::Target::None;
        }
    }

private:
    const char *itemName(Item item) const {
        switch (item) {
        case Mode:              return "Mode";
        case FirstStep:         return "First Step";
        case LastStep:          return "Last Step";
        case NoteFirstStep:     return "Note First";
        case NoteLastStep:      return "Note Last";
        case RunMode:           return "Run Mode";
        case DivisorX:          return "Div X";
        case DivisorY:          return _sequence->mode() == NoteSequence::Mode::Ikra ? "Div N" : "Div Y";
        case ClockMult:         return "Clock Mult";
        case ResetMeasure:      return "Reset Measure";
        case Scale:             return "Scale";
        case RootNote:          return "Root Note";
        case Last:              break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        switch (item) {
        case Mode:
            _sequence->printMode(str);
            break;
        case FirstStep:
            _sequence->printFirstStep(str);
            break;
        case LastStep:
            _sequence->printLastStep(str);
            break;
        case NoteFirstStep:
            _sequence->printNoteFirstStep(str);
            break;
        case NoteLastStep:
            _sequence->printNoteLastStep(str);
            break;
        case RunMode:
            _sequence->printRunMode(str);
            break;
        case DivisorX:
            _sequence->printDivisor(str);
            break;
        case DivisorY:
            _sequence->printDivisorY(str);
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
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case Mode:
            _sequence->editMode(value, shift);
            break;
        case FirstStep:
            _sequence->editFirstStep(value, shift);
            break;
        case LastStep:
            _sequence->editLastStep(value, shift);
            break;
        case NoteFirstStep:
            _sequence->editNoteFirstStep(value, shift);
            break;
        case NoteLastStep:
            _sequence->editNoteLastStep(value, shift);
            break;
        case RunMode:
            _sequence->editRunMode(value, shift);
            break;
        case DivisorX:
            _sequence->editDivisor(value, shift);
            break;
        case DivisorY:
            _sequence->editDivisorY(value, shift);
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
        case Last:
            break;
        }
    }

    int indexedCountValue(Item item) const {
        switch (item) {
        case Mode:
            return int(NoteSequence::Mode::Last);
        case FirstStep:
        case LastStep:
        case NoteFirstStep:
        case NoteLastStep:
            return 16;
        case RunMode:
            return int(Types::RunMode::Last);
        case DivisorX:
        case DivisorY:
        case ResetMeasure:
            return 16;
        case ClockMult:
            return 101;
        case Scale:
            return Scale::Count + 1;
        case RootNote:
            return 12 + 1;
        case Last:
            break;
        }
        return -1;
    }

    int indexedValue(Item item) const {
        switch (item) {
        case Mode:
            return int(_sequence->mode());
        case FirstStep:
            return _sequence->firstStep();
        case LastStep:
            return _sequence->lastStep();
        case NoteFirstStep:
            return _sequence->noteFirstStep();
        case NoteLastStep:
            return _sequence->noteLastStep();
        case RunMode:
            return int(_sequence->runMode());
        case DivisorX:
            return _sequence->indexedDivisor();
        case DivisorY:
            return _sequence->indexedDivisorY();
        case ClockMult:
            return _sequence->clockMultiplier() - 50;
        case ResetMeasure:
            return _sequence->resetMeasure();
        case Scale:
            return _sequence->indexedScale();
        case RootNote:
            return _sequence->indexedRootNote();
        case Last:
            break;
        }
        return -1;
    }

    void setIndexedValue(Item item, int index) {
        switch (item) {
        case Mode:
            return _sequence->setMode(NoteSequence::Mode(index));
        case FirstStep:
            return _sequence->setFirstStep(index);
        case LastStep:
            return _sequence->setLastStep(index);
        case NoteFirstStep:
            return _sequence->setNoteFirstStep(index);
        case NoteLastStep:
            return _sequence->setNoteLastStep(index);
        case RunMode:
            return _sequence->setRunMode(Types::RunMode(index));
        case DivisorX:
            return _sequence->setIndexedDivisor(index);
        case DivisorY:
            return _sequence->setIndexedDivisorY(index);
        case ClockMult:
            return _sequence->setClockMultiplier(index + 50);
        case ResetMeasure:
            return _sequence->setResetMeasure(index);
        case Scale:
            return _sequence->setIndexedScale(index);
        case RootNote:
            return _sequence->setIndexedRootNote(index);
        case Last:
            break;
        }
    }

    NoteSequence *_sequence;

    static const Item linearItems[];

    static const Item reneItems[];

    static const Item ikraItems[];

    int itemCount() const {
        int count = 0;
        const Item *items = linearItems;
        if (_sequence->mode() == NoteSequence::Mode::Ikra) {
            items = ikraItems;
        } else if (_sequence->mode() == NoteSequence::Mode::ReRene) {
            items = reneItems;
        }
        while (items[count] != Last) {
            ++count;
        }
        return count;
    }

    Item itemForRow(int row) const {
        const Item *items = linearItems;
        if (_sequence->mode() == NoteSequence::Mode::Ikra) {
            items = ikraItems;
        } else if (_sequence->mode() == NoteSequence::Mode::ReRene) {
            items = reneItems;
        }
        int count = itemCount();
        if (row < 0 || row >= count) {
            return items[0];
        }
        return items[row];
    }
};
