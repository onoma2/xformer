#pragma once

#include "Config.h"

#include "RoutableListModel.h"

#include "model/IndexedSequence.h"

class IndexedSequenceListModel : public RoutableListModel {
public:
    enum Item {
        Divisor,
        Length,
        Active,
        Loop,
        RunMode,
        Scale,
        RootNote,
        FirstStep,
        SyncMode,
        ResetMeasure,
        Last
    };

    IndexedSequenceListModel()
    {}

    void setSequence(IndexedSequence *sequence) {
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

    virtual int indexedCount(int row) const override {
        return indexedCountValue(Item(row));
    }

    virtual int indexed(int row) const override {
        return indexedValue(Item(row));
    }

    virtual void setIndexed(int row, int index) override {
        if (index >= 0 && index < indexedCount(row)) {
            setIndexedValue(Item(row), index);
        }
    }

    virtual Routing::Target routingTarget(int row) const override {
        switch (Item(row)) {
        case Divisor:
            return Routing::Target::Divisor;
        case Scale:
            return Routing::Target::Scale;
        case RootNote:
            return Routing::Target::RootNote;
        case FirstStep:
            return Routing::Target::FirstStep;
        case RunMode:
            return Routing::Target::RunMode;
        case Length:
        case Active:
        case Loop:
        case SyncMode:
        case ResetMeasure:
        default:
            return Routing::Target::None;
        }
    }

private:
    static const char *itemName(Item item) {
        switch (item) {
        case Divisor:       return "Divisor";
        case Length:        return "Length";
        case Active:        return "Active";
        case Loop:          return "Loop";
        case RunMode:       return "Run Mode";
        case Scale:         return "Scale";
        case RootNote:      return "Root Note";
        case FirstStep:     return "First Step";
        case SyncMode:      return "Sync";
        case ResetMeasure:  return "Reset Measure";
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
            _sequence->printDivisor(str);
            break;
        case Length:
            str("%d", _sequence->activeLength());
            break;
        case Active:
            str("%d", _sequence->activeLength());
            break;
        case Loop:
            _sequence->printLoop(str);
            break;
        case RunMode:
            _sequence->printRunMode(str);
            break;
        case Scale:
            _sequence->printScale(str);
            break;
        case RootNote:
            _sequence->printRootNote(str);
            break;
        case FirstStep:
            _sequence->printFirstStep(str);
            break;
        case SyncMode:
            _sequence->printSyncMode(str);
            break;
        case ResetMeasure:
            _sequence->printResetMeasure(str);
            break;
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case Divisor:
            _sequence->editDivisor(value, shift);
            break;
        case Length:
            if (value > 0) {
                _sequence->appendSteps(value);
            } else if (value < 0) {
                _sequence->trimSteps(-value);
            }
            break;
        case Active:
            _sequence->setActiveLength(_sequence->activeLength() + value);
            break;
        case Loop:
            _sequence->toggleLoop();
            break;
        case RunMode:
            _sequence->editRunMode(value, shift);
            break;
        case Scale:
            _sequence->editScale(value, shift);
            break;
        case RootNote:
            _sequence->editRootNote(value, shift);
            break;
        case FirstStep:
            _sequence->editFirstStep(value, shift);
            break;
        case SyncMode:
            _sequence->editSyncMode(value, shift);
            break;
        case ResetMeasure:
            _sequence->editResetMeasure(value, shift);
            break;
        case Last:
            break;
        }
    }

    int indexedCountValue(Item item) const {
        switch (item) {
        case RunMode:
            return int(Types::RunMode::Last);
        default:
            break;
        }
        return 0;
    }

    int indexedValue(Item item) const {
        switch (item) {
        case RunMode:
            return int(_sequence->runMode());
        default:
            break;
        }
        return -1;
    }

    void setIndexedValue(Item item, int index) {
        switch (item) {
        case RunMode:
            _sequence->setRunMode(Types::RunMode(index));
            break;
        default:
            break;
        }
    }

    IndexedSequence *_sequence;
};
