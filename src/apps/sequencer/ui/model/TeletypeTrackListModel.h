#pragma once

#include "RoutableListModel.h"

class TeletypeTrackListModel : public RoutableListModel {
public:
    virtual int rows() const override {
        return 1;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (row != 0) {
            return;
        }
        if (column == 0) {
            str("Teletype");
        } else {
            str("N/A");
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        (void)row;
        (void)column;
        (void)value;
        (void)shift;
    }

    virtual Routing::Target routingTarget(int row) const override {
        (void)row;
        return Routing::Target::None;
    }
};
