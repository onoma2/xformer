#pragma once

#include "Config.h"
#include "Serialize.h"

#include "core/utils/StringBuilder.h"

extern "C" {
#include "state.h"
}

class TeletypeTrack {
public:
    TeletypeTrack() { clear(); }

    void clear();

    void gateOutputName(int index, StringBuilder &str) const;
    void cvOutputName(int index, StringBuilder &str) const;

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

    scene_state_t &state() { return _state; }
    const scene_state_t &state() const { return _state; }

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
    }

    int8_t _trackIndex = -1;
    scene_state_t _state;

    friend class Track;
};
