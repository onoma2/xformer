#pragma once

#include "TeletypeOutputState.h"

#include "model/TT2Track.h"

#include <cstdint>

class TT2TrackEngine {
public:
    TT2TrackEngine() {
        init(_output);
    }

    TT2OutputState &output() { return _output; }
    const TT2OutputState &output() const { return _output; }

    void setTrack(TT2Track *track) { _track = track; }
    TT2Track *track() { return _track; }
    const TT2Track *track() const { return _track; }

private:
    TT2Track *_track = nullptr;
    TT2OutputState _output;
};

static_assert(sizeof(TT2TrackEngine) <= 344, "TT2TrackEngine size drift");
