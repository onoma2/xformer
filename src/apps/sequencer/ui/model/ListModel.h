#pragma once

#include "core/utils/StringBuilder.h"

#include "model/Routing.h"

class ListModel {
public:
    virtual int rows() const = 0;
    virtual int columns() const = 0;

    virtual void cell(int row, int column, StringBuilder &str) const = 0;

    virtual void edit(int row, int column, int value, bool shift) = 0;

    virtual Routing::Target routingTarget(int) const { return Routing::Target::None; }

    virtual void setSelectedScale(int, bool) {}

    virtual int indexedCount(int row) const { return 0; }
    virtual int indexed(int row) const { return -1; }
    virtual void setIndexed(int row, int index) {}
};
