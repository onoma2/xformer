#pragma once

#include "Config.h"
#include "Types.h"
#include "Serialize.h"
#include "DiscreteMapSequence.h"
#include "RouteParamKey.h"

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

    // playMode

    Types::PlayMode playMode() const { return _playMode; }
    void setPlayMode(Types::PlayMode playMode) {
        _playMode = ModelUtils::clampedEnum(playMode);
    }

    void editPlayMode(int value, bool shift) {
        setPlayMode(ModelUtils::adjustedEnum(playMode(), value));
    }

    void printPlayMode(StringBuilder &str) const {
        str(Types::playModeName(playMode()));
    }

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

    // Inlets read the override path; _routedInput/_routedScanner storage + setters
    // are superseded (dead).
    float routedInput() const { return Routing::routedValue(ParamKey::DiscreteMapInput, _trackIndex, 0.f, -5.f, 5.f); }
    void setRoutedInput(float v) { _routedInput = v; }
    float routedScanner() const { return Routing::routedValue(ParamKey::DiscreteMapScanner, _trackIndex, 0.f, 0.f, 34.f); }
    void setRoutedScanner(float v) { _routedScanner = v; }

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
    float _routedScanner = 0.f;
    float _routedSync = 0.f;
    CvUpdateMode _cvUpdateMode = CvUpdateMode::Gate;  // Default to Gate to maintain current behavior
    Types::PlayMode _playMode = Types::PlayMode::Aligned;

    friend class Track;
};
