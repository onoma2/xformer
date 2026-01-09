#pragma once

#include "Config.h"

#include "ListModel.h"

#include "model/Project.h"
#include "model/TeletypeTrack.h"

#include <array>

class GateOutputListModel : public ListModel {
public:
    GateOutputListModel(Project &project) :
        _project(project)
    {}

    virtual int rows() const override {
        return CONFIG_TRACK_COUNT;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (column == 0) {
            str("Gate%d", row + 1);
        } else if (column == 1) {
            int trackIndex = _project.gateOutputTrack(row);
            int outputIndex = 0;
            for (int i = 0; i < row; ++i) {
                outputIndex += _project.gateOutputTrack(i) == trackIndex ? 1 : 0;
            }
            str("Track%d:", trackIndex + 1);
            const auto &track = _project.track(trackIndex);
            if (track.trackMode() == Track::TrackMode::Teletype &&
                outputIndex < TeletypeTrack::TriggerOutputCount) {
                int dest = int(track.teletypeTrack().triggerOutputDest(outputIndex)) + 1;
                str(" TT G%d", dest);
            } else {
                track.gateOutputName(outputIndex, str);
            }
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            _project.editGateOutputTrack(row, value, shift);
        }
    }

private:
    Project &_project;
};
