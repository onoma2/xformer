#pragma once

#include "Config.h"

#include "ListModel.h"

#include "engine/generators/Generator.h"

class GeneratorSelectListModel : public ListModel {
public:
    // Restrict the picker to a per-track subset; nullptr = the full enum (Note/Curve).
    void setModes(const Generator::Mode *modes, int count) {
        _modes = modes;
        _count = count;
    }

    Generator::Mode modeForRow(int row) const {
        return _modes ? _modes[row] : Generator::Mode(row);
    }

    virtual int rows() const override {
        return _modes ? _count : int(Generator::Mode::Last);
    }

    virtual int columns() const override {
        return 1;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (column == 0) {
            str(Generator::modeName(modeForRow(row)));
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
    }

private:
    const Generator::Mode *_modes = nullptr;
    int _count = 0;
};
