#pragma once

#include "ListModel.h"

#include "model/Routing.h"
#include "model/RouteBrowse.h"

// Source picker list for the tab editor: the CV-domain sources RouteBrowse::sourceList
// offers for a target (MIDI and the self-route bus excluded), rendered by name.
class RouteSourceSelectListModel : public ListModel {
public:
    void setTarget(Routing::Target target) {
        _count = RouteBrowse::sourceList(target, _sources, MaxSources);
    }

    Routing::Source sourceAt(int index) const {
        return (index >= 0 && index < _count) ? _sources[index] : Routing::Source::None;
    }

    int indexOf(Routing::Source source) const {
        for (int i = 0; i < _count; ++i) {
            if (_sources[i] == source) {
                return i;
            }
        }
        return 0;
    }

    virtual int rows() const override {
        return _count;
    }

    virtual int columns() const override {
        return 1;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (column == 0 && row >= 0 && row < _count) {
            Routing::printSource(_sources[row], str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
    }

private:
    static constexpr int MaxSources = int(Routing::Source::Last);
    Routing::Source _sources[MaxSources];
    int _count = 0;
};
