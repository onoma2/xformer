#pragma once

#include "Config.h"
#include "Types.h"
#include "NoteSequence.h"
#include "Serialize.h"
#include "Routing.h"

class NoteTrack {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    using NoteSequenceArray = std::array<NoteSequence, CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT>;

    // FillMode

    enum class FillMode : uint8_t {
        None,
        Gates,
        NextPattern,
        Condition,
        Last
    };

    static const char *fillModeName(FillMode fillMode) {
        switch (fillMode) {
        case FillMode::None:        return "None";
        case FillMode::Gates:       return "Gates";
        case FillMode::NextPattern: return "Next Pattern";
        case FillMode::Condition:   return "Condition";
        case FillMode::Last:        break;
        }
        return nullptr;
    }

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

    //----------------------------------------
    // Properties
    //----------------------------------------

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

    // fillMode

    FillMode fillMode() const { return _fillMode; }
    void setFillMode(FillMode fillMode) {
        _fillMode = ModelUtils::clampedEnum(fillMode);
    }

    void editFillMode(int value, bool shift) {
        setFillMode(ModelUtils::adjustedEnum(fillMode(), value));
    }

    void printFillMode(StringBuilder &str) const {
        str(fillModeName(fillMode()));
    }

    // fillMuted

    bool fillMuted() const { return _fillMuted; }
    void setFillMuted(bool fillMuted) {
        _fillMuted = fillMuted;
    }

    void editFillMuted(int value, bool shift) {
        setFillMuted(value > 0);
    }

    void printFillMuted(StringBuilder &str) const {
        ModelUtils::printYesNo(str, fillMuted());
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

    // slideTime

    int slideTime() const { return Routing::routedValueInt(ParamKey::SlideTime, _trackIndex, _slideTime.base, 0, 100); }
    void setSlideTime(int slideTime, bool routed = false) {
        _slideTime.set(clamp(slideTime, 0, 100), routed);
    }

    void editSlideTime(int value, bool shift) {
        if (!Routing::routeOverridden(ParamKey::SlideTime, _trackIndex)) {
            setSlideTime(ModelUtils::adjustedByStep(slideTime(), value, 5, !shift));
        }
    }

    void printSlideTime(StringBuilder &str) const {
        printRouted(str, Routing::Target::SlideTime);
        str("%d%%", slideTime());
    }

    // octave

    int octave() const { return Routing::routedValueInt(ParamKey::Octave, _trackIndex, _octave.base, -10, 10); }
    void setOctave(int octave, bool routed = false) {
        _octave.set(clamp(octave, -10, 10), routed);
    }

    void editOctave(int value, bool shift) {
        if (!Routing::routeOverridden(ParamKey::Octave, _trackIndex)) {
            setOctave(octave() + value);
        }
    }

    void printOctave(StringBuilder &str) const {
        printRouted(str, Routing::Target::Octave);
        str("%+d", octave());
    }

    // transpose

    int transpose() const { return Routing::routedValueInt(ParamKey::Transpose, _trackIndex, _transpose.base, -60, 60); }
    void setTranspose(int transpose, bool routed = false) {
        _transpose.set(clamp(transpose, -100, 100), routed);
    }

    void editTranspose(int value, bool shift) {
        if (!Routing::routeOverridden(ParamKey::Transpose, _trackIndex)) {
            setTranspose(transpose() + value);
        }
    }

    void printTranspose(StringBuilder &str) const {
        printRouted(str, Routing::Target::Transpose);
        str("%+d", transpose());
    }

    // rotate

    int rotate() const { return Routing::routedValueInt(ParamKey::Rotate, _trackIndex, _rotate.base, -64, 64); }
    void setRotate(int rotate, bool routed = false) {
        _rotate.set(clamp(rotate, -64, 64), routed);
    }

    void editRotate(int value, bool shift) {
        if (!Routing::routeOverridden(ParamKey::Rotate, _trackIndex)) {
            setRotate(rotate() + value);
        }
    }

    void printRotate(StringBuilder &str) const {
        printRouted(str, Routing::Target::Rotate);
        str("%+d", rotate());
    }

    // gateProbabilityBias

    int gateProbabilityBias() const { return Routing::routedValueInt(ParamKey::GateProbabilityBias, _trackIndex, _gateProbabilityBias.base, -NoteSequence::GateProbability::Range, NoteSequence::GateProbability::Range); }
    void setGateProbabilityBias(int gateProbabilityBias, bool routed = false) {
        _gateProbabilityBias.set(clamp(gateProbabilityBias, -NoteSequence::GateProbability::Range, NoteSequence::GateProbability::Range), routed);
    }

    void editGateProbabilityBias(int value, bool shift) {
        if (!Routing::routeOverridden(ParamKey::GateProbabilityBias, _trackIndex)) {
            setGateProbabilityBias(gateProbabilityBias() + value);
        }
    }

    void printGateProbabilityBias(StringBuilder &str) const {
        printRouted(str, Routing::Target::GateProbabilityBias);
        str("%+.1f%%", gateProbabilityBias() * 12.5f);
    }

    // retriggerProbabilityBias

    int retriggerProbabilityBias() const { return Routing::routedValueInt(ParamKey::RetriggerProbabilityBias, _trackIndex, _retriggerProbabilityBias.base, -NoteSequence::RetriggerProbability::Range, NoteSequence::RetriggerProbability::Range); }
    void setRetriggerProbabilityBias(int retriggerProbabilityBias, bool routed = false) {
        _retriggerProbabilityBias.set(clamp(retriggerProbabilityBias, -NoteSequence::RetriggerProbability::Range, NoteSequence::RetriggerProbability::Range), routed);
    }

    void editRetriggerProbabilityBias(int value, bool shift) {
        if (!Routing::routeOverridden(ParamKey::RetriggerProbabilityBias, _trackIndex)) {
            setRetriggerProbabilityBias(retriggerProbabilityBias() + value);
        }
    }

    void printRetriggerProbabilityBias(StringBuilder &str) const {
        printRouted(str, Routing::Target::RetriggerProbabilityBias);
        str("%+.1f%%", retriggerProbabilityBias() * 12.5f);
    }

    // lengthBias

    int lengthBias() const { return Routing::routedValueInt(ParamKey::LengthBias, _trackIndex, _lengthBias.base, -NoteSequence::Length::Range, NoteSequence::Length::Range); }
    void setLengthBias(int lengthBias, bool routed = false) {
        _lengthBias.set(clamp(lengthBias, -NoteSequence::Length::Range, NoteSequence::Length::Range), routed);
    }

    void editLengthBias(int value, bool shift) {
        if (!Routing::routeOverridden(ParamKey::LengthBias, _trackIndex)) {
            setLengthBias(lengthBias() + value);
        }
    }

    void printLengthBias(StringBuilder &str) const {
        printRouted(str, Routing::Target::LengthBias);
        str("%+.1f%%", lengthBias() * 12.5f);
    }

    // noteProbabilityBias

    int noteProbabilityBias() const { return Routing::routedValueInt(ParamKey::NoteProbabilityBias, _trackIndex, _noteProbabilityBias.base, -NoteSequence::NoteVariationProbability::Range, NoteSequence::NoteVariationProbability::Range); }
    void setNoteProbabilityBias(int noteProbabilityBias, bool routed = false) {
        _noteProbabilityBias.set(clamp(noteProbabilityBias, -NoteSequence::NoteVariationProbability::Range, NoteSequence::NoteVariationProbability::Range), routed);
    }

    void editNoteProbabilityBias(int value, bool shift) {
        if (!Routing::routeOverridden(ParamKey::NoteProbabilityBias, _trackIndex)) {
            setNoteProbabilityBias(noteProbabilityBias() + value);
        }
    }

    void printNoteProbabilityBias(StringBuilder &str) const {
        printRouted(str, Routing::Target::NoteProbabilityBias);
        str("%+.1f%%", noteProbabilityBias() * 12.5f);
    }

    // sequences

    const NoteSequenceArray &sequences() const { return _sequences; }
          NoteSequenceArray &sequences()       { return _sequences; }

    const NoteSequence &sequence(int index) const { return _sequences[index]; }
          NoteSequence &sequence(int index)       { return _sequences[index]; }

    //----------------------------------------
    // Routing
    //----------------------------------------

    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }
    void writeRouted(Routing::Target target, int intValue, float floatValue);

    //----------------------------------------
    // Methods
    //----------------------------------------

    NoteTrack() { clear(); }

    void clear();

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
        for (auto &sequence : _sequences) {
            sequence.setTrackIndex(trackIndex);
        }
    }

    int8_t _trackIndex = -1;
    Types::PlayMode _playMode;
    FillMode _fillMode;
    bool _fillMuted;
    CvUpdateMode _cvUpdateMode;
    Routable<uint8_t> _slideTime;
    Routable<int8_t> _octave;
    Routable<int8_t> _transpose;
    Routable<int8_t> _rotate;
    Routable<int8_t> _gateProbabilityBias;
    Routable<int8_t> _retriggerProbabilityBias;
    Routable<int8_t> _lengthBias;
    Routable<int8_t> _noteProbabilityBias;

    NoteSequenceArray _sequences;

    friend class Track;
};
