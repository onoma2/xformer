#pragma once

#include "Config.h"

#include "RoutableListModel.h"

#include "model/TuesdayTrack.h"

class TuesdayTrackListModel : public RoutableListModel {
public:
    void setTrack(TuesdayTrack *track) {
        _track = track;
    }

    virtual int rows() const override {
        return _track ? Last : 0;
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
        default:
            return Routing::Target::None;
        }
    }

private:
    enum Item {
        PlayMode,
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case PlayMode:     return "Play Mode";
        case Last:         break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        switch (item) {
        case PlayMode:
            _track->printPlayMode(str);
            break;
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case PlayMode:
            _track->editPlayMode(value, shift);
            break;
        case Last:
            break;
        }
    }

    TuesdayTrack *_track = nullptr;
};
