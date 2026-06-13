#pragma once

#include "TeletypeProgram.h"
#include "TeletypeRuntime.h"
#include "Serialize.h"

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

    // Persist the program only — runtime is volatile and re-inits on load.
    // Flat blob, no version gate (dev: files break freely).
    void write(VersionedSerializedWriter &writer) const {
        writer.write(_program);
    }
    void read(VersionedSerializedReader &reader) {
        reader.read(_program);
        init(_runtime);
    }

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
    }

    int8_t _trackIndex = -1;
    TeletypeProgram _program;
    TT2Runtime _runtime;

    friend class Track;
};

static_assert(sizeof(TT2Track) == 4520, "TT2Track size drift");
