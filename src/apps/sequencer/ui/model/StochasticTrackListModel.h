#pragma once

#include "Config.h"

#include "RoutableListModel.h"

#include "model/StochasticTrack.h"

class StochasticTrackListModel : public RoutableListModel {
public:
    StochasticTrackListModel() {}

    void setTrack(StochasticTrack &track) {
        _track = &track;
    }

    virtual int rows() const override {
        return 0;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
    }

    virtual void edit(int row, int column, int value, bool shift) override {
    }

    virtual Routing::Target routingTarget(int row) const override {
        return Routing::Target::None;
    }

private:
    StochasticTrack *_track = nullptr;
};
