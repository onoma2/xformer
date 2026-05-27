#pragma once

#include "TeletypeProgram.h"
#include "TeletypeRuntime.h"

#include <cstdint>

class TT2Track {
public:
    TT2Track() { clear(); }

    void clear() {
        init(_program);
        init(_runtime);
    }

    TeletypeProgram &program() { return _program; }
    const TeletypeProgram &program() const { return _program; }

    TT2Runtime &runtime() { return _runtime; }
    const TT2Runtime &runtime() const { return _runtime; }

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
    }

    int8_t _trackIndex = -1;
    TeletypeProgram _program;
    TT2Runtime _runtime;

    friend class Track;
};

static_assert(sizeof(TT2Track) == 4504, "TT2Track size drift");
