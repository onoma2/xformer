#pragma once

#include "Config.h"
#include "Types.h"
#include "Serialize.h"
#include "DiscreteMapSequence.h"

#include <array>

class DiscreteMapTrack {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    using DiscreteMapSequenceArray = std::array<DiscreteMapSequence, CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT>;

    //----------------------------------------
    // Properties
    //----------------------------------------

    // sequences

    const DiscreteMapSequenceArray &sequences() const { return _sequences; }
          DiscreteMapSequenceArray &sequences()       { return _sequences; }

    const DiscreteMapSequence &sequence(int index) const { return _sequences[index]; }
          DiscreteMapSequence &sequence(int index)       { return _sequences[index]; }

    //----------------------------------------
    // Methods
    //----------------------------------------

    DiscreteMapTrack() { clear(); }

    void clear();

    void gateOutputName(int index, StringBuilder &str) const;
    void cvOutputName(int index, StringBuilder &str) const;

    // CvUpdateMode

    enum class CvUpdateMode : uint8_t {
        Gate,    // Update CV only when a stage triggers (current behavior)
        Always,  // Update CV continuously regardless of stages (new)
        Last
    };

    static const char *cvUpdateModeName(CvUpdateMode mode) {
        switch (mode) {
        case CvUpdateMode::Gate:    return "Gate";
        case CvUpdateMode::Always:  return "Always";
        case CvUpdateMode::Last:    break;
        }
        return nullptr;
    }

    // cvUpdateMode

    CvUpdateMode cvUpdateMode() const { return _cvUpdateMode; }
    void setCvUpdateMode(CvUpdateMode cvUpdateMode) {
        _cvUpdateMode = ModelUtils::clampedEnum(cvUpdateMode);
    }

    void editCvUpdateMode(int value, bool shift) {
        setCvUpdateMode(ModelUtils::adjustedEnum(cvUpdateMode(), value));
    }

    void printCvUpdateMode(StringBuilder &str) const {
        str(cvUpdateModeName(cvUpdateMode()));
    }

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);
    void writeRouted(Routing::Target target, int intValue, float floatValue);

    float routedInput() const { return _routedInput; }
    float routedThresholdBias() const { return _routedThresholdBias; }
    float routedSync() const { return _routedSync; }

    int octave() const { return _octave; }
    void setOctave(int octave) { _octave = clamp(octave, -10, 10); }

    int transpose() const { return _transpose; }
    void setTranspose(int transpose) { _transpose = clamp(transpose, -60, 60); }

    int offset() const { return _offset; }
    void setOffset(int offset) { _offset = clamp(offset, -500, 500); }

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
        for (auto &sequence : _sequences) {
            sequence.setTrackIndex(trackIndex);
        }
    }

    int8_t _trackIndex = -1;
    DiscreteMapSequenceArray _sequences;

    // Routed state
    float _routedInput = 0.f;
    float _routedThresholdBias = 0.f;
    float _routedSync = 0.f;
    int8_t _octave = 0;
    int8_t _transpose = 0;
    int16_t _offset = 0;
    CvUpdateMode _cvUpdateMode = CvUpdateMode::Gate;  // Default to Gate to maintain current behavior

    friend class Track;
};
