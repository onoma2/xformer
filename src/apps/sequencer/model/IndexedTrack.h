#pragma once

#include "Config.h"
#include "Types.h"
#include "Serialize.h"
#include "IndexedSequence.h"
#include "RouteParamKey.h"

#include <array>

class IndexedTrack {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    using IndexedSequenceArray = std::array<IndexedSequence, CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT>;

    //----------------------------------------
    // Properties
    //----------------------------------------

    // CvUpdateMode

    enum class CvUpdateMode : uint8_t {
        Gate,
        Always,
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

    // slideTime

    int slideTime() const { return Routing::routedValueInt(ParamKey::SlideTime, _trackIndex, _slideTime, 0, 100); }
    void setSlideTime(int slideTime) {
        _slideTime = clamp(slideTime, 0, 100);
    }

    void editSlideTime(int value, bool shift) {
        setSlideTime(ModelUtils::adjustedByStep(_slideTime, value, 5, !shift));
    }

    void printSlideTime(StringBuilder &str) const {
        printRouted(str, Routing::Target::SlideTime);
        str("%d%%", slideTime());
    }

    // sequences

    const IndexedSequenceArray &sequences() const { return _sequences; }
          IndexedSequenceArray &sequences()       { return _sequences; }

    const IndexedSequence &sequence(int index) const { return _sequences[index]; }
          IndexedSequence &sequence(int index)       { return _sequences[index]; }

    //----------------------------------------
    // Methods
    //----------------------------------------

    IndexedTrack() { clear(); }

    void clear();

    void gateOutputName(int index, StringBuilder &str) const;
    void cvOutputName(int index, StringBuilder &str) const;

    // octave

    int octave() const { return Routing::routedValueInt(ParamKey::Octave, _trackIndex, _octave, -10, 10); }
    void setOctave(int octave) {
        _octave = clamp(octave, -10, 10);
    }

    void editOctave(int value, bool shift) {
        setOctave(_octave + value);
    }

    void printOctave(StringBuilder &str) const {
        printRouted(str, Routing::Target::Octave);
        str("%+d", octave());
    }

    // transpose

    int transpose() const { return Routing::routedValueInt(ParamKey::Transpose, _trackIndex, _transpose, -60, 60); }
    void setTranspose(int transpose) {
        _transpose = clamp(transpose, -60, 60);
    }

    void editTranspose(int value, bool shift) {
        setTranspose(_transpose + value);
    }

    void printTranspose(StringBuilder &str) const {
        printRouted(str, Routing::Target::Transpose);
        str("%+d", transpose());
    }

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

    float routedSync() const { return _routedSync; }
    void setRoutedSync(float v) { _routedSync = v; }

    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
        for (auto &sequence : _sequences) {
            sequence.setTrackIndex(trackIndex);
        }
    }

    int8_t _trackIndex = -1;
    CvUpdateMode _cvUpdateMode = CvUpdateMode::Gate;
    Types::PlayMode _playMode = Types::PlayMode::Aligned;
    IndexedSequenceArray _sequences;
    float _routedSync = 0.f;
    int8_t _octave;
    int8_t _transpose;
    uint8_t _slideTime;

    friend class Track;
};
