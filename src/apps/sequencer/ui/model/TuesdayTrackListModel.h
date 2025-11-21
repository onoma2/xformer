#pragma once

#include "Config.h"

#include "RoutableListModel.h"

#include "model/TuesdayTrack.h"

class TuesdayTrackListModel : public RoutableListModel {
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
        // Tuesday track parameters don't have routing targets yet
        // Future: Could add Flow, Power, etc. as routing targets
        return Routing::Target::None;
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
            _track->printAlgorithm(str);
            break;
        case Flow:
            _track->printFlow(str);
            break;
        case Ornament:
            _track->printOrnament(str);
            break;
        case Power:
            _track->printPower(str);
            break;
        case LoopLength:
            _track->printLoopLength(str);
            break;
        case Rotate:
            if (_track->loopLength() == 0) {
                str("N/A");
            } else {
                _track->printRotate(str);
            }
            break;
        case Glide:
            _track->printGlide(str);
            break;
        case Skew:
            _track->printSkew(str);
            break;
        case CvUpdateMode:
            _track->printCvUpdateMode(str);
            break;
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case Algorithm:
            _track->editAlgorithm(value, shift);
            break;
        case Flow:
            _track->editFlow(value, shift);
            break;
        case Ornament:
            _track->editOrnament(value, shift);
            break;
        case Power:
            _track->editPower(value, shift);
            break;
        case LoopLength:
            _track->editLoopLength(value, shift);
            break;
        case Rotate:
            if (_track->loopLength() != 0) {
                _track->editRotate(value, shift);
            }
            break;
        case Glide:
            _track->editGlide(value, shift);
            break;
        case Skew:
            _track->editSkew(value, shift);
            break;
        case CvUpdateMode:
            _track->editCvUpdateMode(value, shift);
            break;
        case Last:
            break;
        }
    }

    TuesdayTrack *_track;
};
