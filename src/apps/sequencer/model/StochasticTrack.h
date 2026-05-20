#pragma once

#include "Config.h"
#include "Types.h"
#include "StochasticSequence.h"
#include "Serialize.h"
#include "Routing.h"

class StochasticTrack {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    using StochasticSequenceArray = std::array<StochasticSequence, CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT>;

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

    // Internal layout padding / legacy
    StochasticModeInternal modeInternal() const { return _modeInternal; }
    void setModeInternal(StochasticModeInternal mode) { _modeInternal = ModelUtils::clampedEnum(mode); }

    // lock
    bool lock() const { return _lock; }
    void setLock(bool lock) { _lock = lock; }

    // fillMuted
    bool fillMuted() const { return _fillMuted; }
    void setFillMuted(bool fillMuted) { _fillMuted = fillMuted; }

    // loopFirst
    int loopFirst() const { return _loopFirst; }
    void setLoopFirst(int first) { _loopFirst = clamp(first, 0, CONFIG_STEP_COUNT - 1); }

    // loopLast
    int loopLast() const { return _loopLast; }
    void setLoopLast(int last) { _loopLast = clamp(last, 0, CONFIG_STEP_COUNT - 1); }

    // slideTime
    int slideTime() const { return _slideTime.get(isRouted(Routing::Target::SlideTime)); }
    void setSlideTime(int slideTime, bool routed = false) { _slideTime.set(clamp(slideTime, 0, 100), routed); }

    // octave
    int octave() const { return _octave.get(isRouted(Routing::Target::Octave)); }
    void setOctave(int octave, bool routed = false) { _octave.set(clamp(octave, -10, 10), routed); }

    // transpose
    int transpose() const { return _transpose.get(isRouted(Routing::Target::Transpose)); }
    void setTranspose(int transpose, bool routed = false) { _transpose.set(clamp(transpose, -100, 100), routed); }

    // fillMode
    FillMode fillMode() const { return _fillMode; }
    void setFillMode(FillMode mode) { _fillMode = ModelUtils::clampedEnum(mode); }

    // cvUpdateMode
    CvUpdateMode cvUpdateMode() const { return _cvUpdateMode; }
    void setCvUpdateMode(CvUpdateMode mode) { _cvUpdateMode = ModelUtils::clampedEnum(mode); }

    // playMode
    Types::PlayMode playMode() const { return _playMode; }
    void setPlayMode(Types::PlayMode playMode) { _playMode = ModelUtils::clampedEnum(playMode); }
    void editPlayMode(int value, bool shift) { setPlayMode(ModelUtils::adjustedEnum(playMode(), value)); }
    void printPlayMode(StringBuilder &str) const { str(Types::playModeName(playMode())); }

    // gateBias
    int gateBias() const { return _gateBias.get(isRouted(Routing::Target::GateProbabilityBias)); }
    void setGateBias(int bias, bool routed = false) { _gateBias.set(clamp(bias, -8, 8), routed); }

    // retriggerBias
    int retriggerBias() const { return _retriggerBias.get(isRouted(Routing::Target::RetriggerProbabilityBias)); }
    void setRetriggerBias(int bias, bool routed = false) { _retriggerBias.set(clamp(bias, -8, 8), routed); }

    // lengthBias
    int lengthBias() const { return _lengthBias.get(isRouted(Routing::Target::LengthBias)); }
    void setLengthBias(int bias, bool routed = false) { _lengthBias.set(clamp(bias, -8, 8), routed); }

    // noteBias
    int noteBias() const { return _noteBias.get(isRouted(Routing::Target::NoteProbabilityBias)); }
    void setNoteBias(int bias, bool routed = false) { _noteBias.set(clamp(bias, -8, 8), routed); }

    // UI Helpers
    void printLock(StringBuilder &str) const { ModelUtils::printYesNo(str, lock()); }
    void editLock(int value, bool shift) { setLock(value > 0); }

    void printOctave(StringBuilder &str) const { printRouted(str, Routing::Target::Octave); str("%+d", octave()); }
    void editOctave(int value, bool shift) { if (!isRouted(Routing::Target::Octave)) setOctave(octave() + value); }

    void printTranspose(StringBuilder &str) const { printRouted(str, Routing::Target::Transpose); str("%+d", transpose()); }
    void editTranspose(int value, bool shift) { if (!isRouted(Routing::Target::Transpose)) setTranspose(transpose() + value); }

    void printCvUpdateMode(StringBuilder &str) const { str(cvUpdateModeName(cvUpdateMode())); }
    void editCvUpdateMode(int value, bool shift) { setCvUpdateMode(ModelUtils::adjustedEnum(cvUpdateMode(), value)); }

    void printSlideTime(StringBuilder &str) const { printRouted(str, Routing::Target::SlideTime); str("%d%%", slideTime()); }
    void editSlideTime(int value, bool shift) { if (!isRouted(Routing::Target::SlideTime)) setSlideTime(ModelUtils::adjustedByStep(slideTime(), value, 5, !shift)); }

    int8_t activePatternIndex() const { return -1; }
    void setActivePatternIndex(int index) {}

    uint32_t activeSeed() const { return 0; }
    void setActiveSeed(uint32_t seed) {}

    // sequences
    const StochasticSequenceArray &sequences() const { return _sequences; }
          StochasticSequenceArray &sequences()       { return _sequences; }

    const StochasticSequence &sequence(int index) const { return _sequences[index]; }
          StochasticSequence &sequence(int index)       { return _sequences[index]; }

    //----------------------------------------
    // Routing
    //----------------------------------------

    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }
    void writeRouted(Routing::Target target, int intValue, float floatValue);

    //----------------------------------------
    // Methods
    //----------------------------------------

    StochasticTrack() { clear(); }

    void clear() {
        _lock = false;
        _fillMuted = true;
        _loopFirst = 0;
        _loopLast = CONFIG_STEP_COUNT - 1;
        _slideTime.setBase(0);
        _octave.setBase(0);
        _transpose.setBase(0);
        _fillMode = FillMode::None;
        _cvUpdateMode = CvUpdateMode::Gate;
        _playMode = Types::PlayMode::Aligned;
        _gateBias.setBase(0);
        _retriggerBias.setBase(0);
        _lengthBias.setBase(0);
        _noteBias.setBase(0);

        _modeInternal = StochasticModeInternal::Loop;

        for (auto &sequence : _sequences) {
            sequence.clear();
        }
    }

    void write(VersionedSerializedWriter &writer) const {
        writer.write(_fillMuted);
        writer.write(_loopFirst);
        writer.write(_loopLast);
        _slideTime.write(writer);
        _octave.write(writer);
        _transpose.write(writer);
        writer.write(static_cast<uint8_t>(_fillMode));
        writer.write(static_cast<uint8_t>(_cvUpdateMode));
        writer.write(static_cast<uint8_t>(_playMode));
        _gateBias.write(writer);
        _retriggerBias.write(writer);
        _lengthBias.write(writer);
        _noteBias.write(writer);

        // Phase 7b additions
        writer.write(static_cast<uint8_t>(_modeInternal));

        for (const auto &sequence : _sequences) {
            sequence.write(writer);
        }
    }

    void read(VersionedSerializedReader &reader) {
        reader.read(_fillMuted);
        reader.read(_loopFirst);
        _loopFirst = clamp(int(_loopFirst), 0, CONFIG_STEP_COUNT - 1);
        reader.read(_loopLast);
        _loopLast = clamp(int(_loopLast), 0, CONFIG_STEP_COUNT - 1);
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
        _playMode = playMode < uint8_t(Types::PlayMode::Last) ? static_cast<Types::PlayMode>(playMode) : Types::PlayMode::Free;
        _gateBias.read(reader);
        _retriggerBias.read(reader);
        _lengthBias.read(reader);
        _noteBias.read(reader);

        // Phase 7b additions
        uint8_t modeInternal;
        reader.read(modeInternal);
        _modeInternal = modeInternal < uint8_t(StochasticModeInternal::Last) ? static_cast<StochasticModeInternal>(modeInternal) : StochasticModeInternal::Loop;

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
    bool _fillMuted;
    uint8_t _loopFirst;
    uint8_t _loopLast;
    Routable<uint8_t> _slideTime;
    Routable<int8_t> _octave;
    Routable<int8_t> _transpose;
    FillMode _fillMode;
    CvUpdateMode _cvUpdateMode;
    Types::PlayMode _playMode;
    Routable<int8_t> _gateBias;
    Routable<int8_t> _retriggerBias;
    Routable<int8_t> _lengthBias;
    Routable<int8_t> _noteBias;

    // Phase 7b Generator controls
    StochasticModeInternal _modeInternal;

    StochasticSequenceArray _sequences;

    friend class Track;
};

static_assert(sizeof(StochasticTrack) <= 9544, "StochasticTrack too large");
