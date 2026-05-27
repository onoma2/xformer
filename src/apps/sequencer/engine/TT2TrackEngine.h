#pragma once

#include "TeletypeInterpreter.h"
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

    // Execute one script by index against the bound track's program and runtime.
    // Output state is written into this engine's TT2OutputState.
    TT2EvalResult runScript(uint8_t scriptIndex) {
        if (!_track) {
            return {TT2EvalError::NoTrack, 0, 0};
        }
        return ::runScript(_track->program(), _track->runtime(), _output, scriptIndex);
    }

    // Convert a raw 0..16383 CV value to Performer float volts (-5V .. +5V).
    static float rawToVolts(int16_t raw) {
        if (raw < 0) raw = 0;
        if (raw > 16383) raw = 16383;
        float norm = raw / 16383.f;
        return norm * 10.f - 5.f;
    }

    // Performer CV output interface.
    // Returns 0V for outputs that have never been written (dirty bit clear).
    // Once written, the value persists until changed or the engine is re-init'd.
    float cvOutput(int index) const {
        if (index < 0 || index >= TT2_OUTPUT_CV_COUNT) {
            return 0.f;
        }
        if (!(_output.cvDirty & (1 << index))) {
            return 0.f;
        }
        return rawToVolts(_output.cv[index].targetRaw);
    }

    // Performer gate output interface.
    bool gateOutput(int index) const {
        if (index < 0 || index >= TT2_OUTPUT_TR_COUNT) {
            return false;
        }
        return _output.tr[index].level != 0;
    }

private:
    TT2Track *_track = nullptr;
    TT2OutputState _output;
};

static_assert(sizeof(TT2TrackEngine) <= 344, "TT2TrackEngine size drift");
