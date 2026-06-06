#pragma once

#include "Config.h"
#include "Types.h"
#include "PhaseFluxSequence.h"
#include "Serialize.h"
#include "Routing.h"

class PhaseFluxTrack {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    using PhaseFluxSequenceArray = std::array<PhaseFluxSequence, CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT>;

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

    // lock — intentionally not persisted (matches Stochastic)
    bool lock() const { return _lock; }
    void setLock(bool v) { _lock = v; }

    // slideTime — Routable 0..100. editX edits the base (anchor), so it works under a route.
    int slideTime() const { return Routing::routedValueInt(ParamKey::SlideTime, _trackIndex, _slideTime.base, 0, 100); }
    void setSlideTime(int v, bool routed = false) { _slideTime.set(clamp(v, 0, 100), routed); }
    void editSlideTime(int value, bool) { setSlideTime(_slideTime.base + value); }

    // octave — Routable ±10
    int octave() const { return Routing::routedValueInt(ParamKey::Octave, _trackIndex, _octave.base, -10, 10); }
    void setOctave(int v, bool routed = false) { _octave.set(clamp(v, -10, 10), routed); }
    void editOctave(int value, bool) { setOctave(_octave.base + value); }

    // transpose — Routable ±100 (scale degrees, see spec §12.1)
    int transpose() const { return Routing::routedValueInt(ParamKey::Transpose, _trackIndex, _transpose.base, -60, 60); }
    void setTranspose(int v, bool routed = false) { _transpose.set(clamp(v, -100, 100), routed); }
    void editTranspose(int value, bool) { setTranspose(_transpose.base + value); }

    // fillMode
    FillMode fillMode() const { return _fillMode; }
    void setFillMode(FillMode mode) { _fillMode = ModelUtils::clampedEnum(mode); }

    // cvUpdateMode
    CvUpdateMode cvUpdateMode() const { return _cvUpdateMode; }
    void setCvUpdateMode(CvUpdateMode mode) { _cvUpdateMode = ModelUtils::clampedEnum(mode); }
    void editCvUpdateMode(int value, bool) { setCvUpdateMode(ModelUtils::adjustedEnum(cvUpdateMode(), value)); }
    void printCvUpdateMode(StringBuilder &str) const { str(cvUpdateModeName(cvUpdateMode())); }

    // playMode
    Types::PlayMode playMode() const { return _playMode; }
    void setPlayMode(Types::PlayMode playMode) { _playMode = ModelUtils::clampedEnum(playMode); }
    void editPlayMode(int value, bool) { setPlayMode(ModelUtils::adjustedEnum(playMode(), value)); }
    void printPlayMode(StringBuilder &str) const { str(Types::playModeName(playMode())); }

    // sequences
    const PhaseFluxSequenceArray &sequences() const { return _sequences; }
          PhaseFluxSequenceArray &sequences()       { return _sequences; }
    const PhaseFluxSequence &sequence(int index) const { return _sequences[index]; }
          PhaseFluxSequence &sequence(int index)       { return _sequences[index]; }

    //----------------------------------------
    // Routing
    //----------------------------------------

    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }

    //----------------------------------------
    // Methods
    //----------------------------------------

    PhaseFluxTrack() { clear(); }

    void clear() {
        _lock = false;
        _slideTime.setBase(0);
        _octave.setBase(0);
        _transpose.setBase(0);
        _fillMode = FillMode::None;
        _cvUpdateMode = CvUpdateMode::Gate;
        _playMode = Types::PlayMode::Aligned;
        for (auto &sequence : _sequences) {
            sequence.clear();
        }
    }

    void write(VersionedSerializedWriter &writer) const {
        _slideTime.write(writer);
        _octave.write(writer);
        _transpose.write(writer);
        writer.write(static_cast<uint8_t>(_fillMode));
        writer.write(static_cast<uint8_t>(_cvUpdateMode));
        writer.write(static_cast<uint8_t>(_playMode));
        for (const auto &sequence : _sequences) {
            sequence.write(writer);
        }
    }

    void read(VersionedSerializedReader &reader) {
        _slideTime.read(reader);
        _octave.read(reader);
        _transpose.read(reader);
        uint8_t fillMode;
        reader.read(fillMode);
        _fillMode = fillMode < uint8_t(FillMode::Last) ? static_cast<FillMode>(fillMode) : FillMode::None;
        uint8_t cvUpdateMode;
        reader.read(cvUpdateMode);
        _cvUpdateMode = cvUpdateMode < uint8_t(CvUpdateMode::Last) ? static_cast<CvUpdateMode>(cvUpdateMode) : CvUpdateMode::Gate;
        uint8_t playMode;
        reader.read(playMode);
        _playMode = playMode < uint8_t(Types::PlayMode::Last) ? static_cast<Types::PlayMode>(playMode) : Types::PlayMode::Aligned;
        for (auto &sequence : _sequences) {
            sequence.read(reader);
        }
    }

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
        for (auto &sequence : _sequences) {
            sequence.setTrackIndex(trackIndex);
        }
    }

    int8_t _trackIndex = -1;

    bool _lock;
    Routable<uint8_t> _slideTime;
    Routable<int8_t> _octave;
    Routable<int8_t> _transpose;
    FillMode _fillMode;
    CvUpdateMode _cvUpdateMode;
    Types::PlayMode _playMode;

    PhaseFluxSequenceArray _sequences;

    friend class Track;
};

static_assert(sizeof(PhaseFluxTrack) <= 9544, "PhaseFluxTrack too large");
