#pragma once

#include "TeletypeProgram.h"
#include "TeletypeRuntime.h"
#include "Serialize.h"

#include <cstdint>

class TT2MiniTrack {
public:
    TT2MiniTrack() { clear(); }

    void clear() {
        for (int i = 0; i < TT2ConfigMini::SceneCount; ++i) {
            init(_programs[i]);
        }
        init(_runtime);
    }

    TeletypeProgramT<TT2ConfigMini> &program(int scene) { return _programs[scene % TT2ConfigMini::SceneCount]; }
    const TeletypeProgramT<TT2ConfigMini> &program(int scene) const { return _programs[scene % TT2ConfigMini::SceneCount]; }

    TT2RuntimeT<TT2ConfigMini> &runtime() { return _runtime; }
    const TT2RuntimeT<TT2ConfigMini> &runtime() const { return _runtime; }

    // Persist the per-scene programs only — runtime is volatile and re-inits on load.
    // Flat blob, no version gate (dev: files break freely).
    void write(VersionedSerializedWriter &writer) const {
        for (int i = 0; i < TT2ConfigMini::SceneCount; ++i) {
            writer.write(_programs[i]);
        }
    }
    void read(VersionedSerializedReader &reader) {
        for (int i = 0; i < TT2ConfigMini::SceneCount; ++i) {
            reader.read(_programs[i]);
        }
        init(_runtime);
    }

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
    }

    int8_t _trackIndex = -1;
    TeletypeProgramT<TT2ConfigMini> _programs[TT2ConfigMini::SceneCount];
    TT2RuntimeT<TT2ConfigMini> _runtime;

    friend class Track;
};

static_assert(sizeof(TT2MiniTrack) <= 9520, "");
