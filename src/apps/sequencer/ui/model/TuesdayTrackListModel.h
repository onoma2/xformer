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
        case LoopLength:
            return Routing::Target::LoopLength;
        case Rotate:
            return Routing::Target::Rotate;
        case Glide:
            return Routing::Target::Glide;
        case Skew:
            return Routing::Target::Skew;
        case CvUpdateMode:
            return Routing::Target::CvUpdateMode;
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
        CvUpdateMode,
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
        case CvUpdateMode:  return "CV Mode";
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
            if (_sequence->loopLength() == 0) {
                str("N/A");
            } else {
                _sequence->printRotate(str);
            }
            break;
        case Glide:
            _sequence->printGlide(str);
            break;
        case Skew:
            _sequence->printSkew(str);
            break;
        case CvUpdateMode:
            _sequence->printCvUpdateMode(str);
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
            if (_sequence->loopLength() != 0) {
                _sequence->editRotate(value, shift);
            }
            break;
        case Glide:
            _sequence->editGlide(value, shift);
            break;
        case Skew:
            _sequence->editSkew(value, shift);
            break;
        case CvUpdateMode:
            _sequence->editCvUpdateMode(value, shift);
            break;
        case Last:
            break;
        }
    }

    TuesdaySequence *_sequence;
};
