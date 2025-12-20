#pragma once

#include "Config.h"

#include "RoutableListModel.h"

#include "model/IndexedSequence.h"

class IndexedSequenceListModel : public RoutableListModel {
public:
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

    virtual Routing::Target routingTarget(int row) const override {
        switch (Item(row)) {
        case Divisor:
            return Routing::Target::Divisor;
        case Scale:
            return Routing::Target::Scale;
        case RootNote:
            return Routing::Target::RootNote;
        case Loop:
        case ActiveLength:
        default:
            return Routing::Target::None;
        }
    }

private:
    enum Item {
        Divisor,
        Loop,
        ActiveLength,
        Scale,
        RootNote,
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case Divisor:       return "Divisor";
        case Loop:          return "Loop";
        case ActiveLength:  return "Length";
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
        case Divisor:
            _sequence->printDivisor(str);
            break;
        case Loop:
            _sequence->printLoop(str);
            break;
        case ActiveLength:
            str("%d", _sequence->activeLength());
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
        case Divisor:
            _sequence->editDivisor(value, shift);
            break;
        case Loop:
            _sequence->toggleLoop();
            break;
        case ActiveLength:
            _sequence->setActiveLength(_sequence->activeLength() + value);
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

    IndexedSequence *_sequence;
};
