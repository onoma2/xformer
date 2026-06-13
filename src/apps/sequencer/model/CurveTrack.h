#pragma once

#include "Config.h"
#include "Types.h"
#include "CurveSequence.h"
#include "Serialize.h"
#include "Routing.h"
#include "RouteParamKey.h"

#include <cmath>

class CurveTrack {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    using CurveSequenceArray = std::array<CurveSequence, CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT>;

    // FillMode

    enum class FillMode : uint8_t {
        None,
        Variation,
        NextPattern,
        Invert,
        Last
    };

    static const char *fillModeName(FillMode fillMode) {
        switch (fillMode) {
        case FillMode::None:        return "None";
        case FillMode::Variation:   return "Variation";
        case FillMode::NextPattern: return "Next Pattern";
        case FillMode::Invert:      return "Invert";
        case FillMode::Last:        break;
        }
        return nullptr;
    }

    enum class MuteMode : uint8_t {
        LastValue,
        Zero,
        Min,
        Max,
        Last
    };

    static const char *muteModeName(MuteMode muteMode) {
        switch (muteMode) {
        case MuteMode::LastValue:   return "Last Value";
        case MuteMode::Zero:        return "0V";
        case MuteMode::Min:         return "Min";
        case MuteMode::Max:         return "Max";
        case MuteMode::Last:        break;
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

    // muteMode

    MuteMode muteMode() const { return _muteMode; }
    void setMuteMode(MuteMode muteMode) {
        _muteMode = ModelUtils::clampedEnum(muteMode);
    }

    void editMuteMode(int value, bool shift) {
        setMuteMode(ModelUtils::adjustedEnum(muteMode(), value));
    }

    void printMuteMode(StringBuilder &str) const {
        str(muteModeName(muteMode()));
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

    // offset

    int offset() const { return Routing::routedValueInt(ParamKey::Offset, _trackIndex, _offset, -500, 500); }
    float offsetVolts() const { return offset() * 0.01f; }
    void setOffset(int offset) {
        _offset = clamp(offset, -500, 500);
    }

    void editOffset(int value, bool shift) {
        setOffset(_offset + value * (shift ? 100 : 1));
    }

    void printOffset(StringBuilder &str) const {
        printRouted(str, Routing::Target::Offset);
        str("%+.2fV", offsetVolts());
    }

    // rotate

    int rotate() const { return Routing::routedValueInt(ParamKey::Rotate, _trackIndex, _rotate, -64, 64); }
    void setRotate(int rotate) {
        _rotate = clamp(rotate, -64, 64);
    }

    void editRotate(int value, bool shift) {
        setRotate(_rotate + value);
    }

    void printRotate(StringBuilder &str) const {
        printRouted(str, Routing::Target::Rotate);
        str("%+d", rotate());
    }

    // shapeProbabilityBias

    int shapeProbabilityBias() const { return Routing::routedValueInt(ParamKey::ShapeProbabilityBias, _trackIndex, _shapeProbabilityBias, -8, 8); }
    void setShapeProbabilityBias(int shapeProbabilityBias) {
        _shapeProbabilityBias = clamp(shapeProbabilityBias, -8, 8);
    }

    void editShapeProbabilityBias(int value, bool shift) {
        setShapeProbabilityBias(_shapeProbabilityBias + value);
    }

    void printShapeProbabilityBias(StringBuilder &str) const {
        printRouted(str, Routing::Target::ShapeProbabilityBias);
        str("%+.1f%%", shapeProbabilityBias() * 12.5f);
    }

    // gateProbabilityBias

    int gateProbabilityBias() const { return Routing::routedValueInt(ParamKey::GateProbabilityBias, _trackIndex, _gateProbabilityBias, -CurveSequence::GateProbability::Range, CurveSequence::GateProbability::Range); }
    void setGateProbabilityBias(int gateProbabilityBias) {
        _gateProbabilityBias = clamp(gateProbabilityBias, -CurveSequence::GateProbability::Range, CurveSequence::GateProbability::Range);
    }

    void editGateProbabilityBias(int value, bool shift) {
        setGateProbabilityBias(_gateProbabilityBias + value);
    }

    void printGateProbabilityBias(StringBuilder &str) const {
        printRouted(str, Routing::Target::GateProbabilityBias);
        str("%+.1f%%", gateProbabilityBias() * 12.5f);
    }

    // globalPhase

    float globalPhase() const { return Routing::routedValue(ParamKey::Phase, _trackIndex, _globalPhase, 0.f, 1.f); }
    void setGlobalPhase(float globalPhase) {
        _globalPhase = clamp(globalPhase, 0.f, 1.f);
    }

    void editGlobalPhase(float value, bool shift) {
        setGlobalPhase(_globalPhase + value * (shift ? 0.1f : 0.01f));
    }

    void printGlobalPhase(StringBuilder &str) const {
        printRouted(str, Routing::Target::Phase);
        str("%.2f", globalPhase());
    }

    // curveRate (speed multiplier: 0.0 = stop, 1.0 = normal, 4.0 = 4x)

    float curveRate() const { return Routing::routedValueInt(ParamKey::CurveRate, _trackIndex, int(std::lround(_curveRate * 100.f)), 0, 400) / 100.f; }
    void setCurveRate(float curveRate) {
        _curveRate = clamp(curveRate, 0.f, 4.f);
    }

    void editCurveRate(int value, bool shift) {
        setCurveRate(_curveRate + value * (shift ? 0.1f : 0.01f));
    }

    void printCurveRate(StringBuilder &str) const {
        printRouted(str, Routing::Target::CurveRate);
        str("%.2fx", curveRate());
    }

    // sequences

    const CurveSequenceArray &sequences() const { return _sequences; }
          CurveSequenceArray &sequences()       { return _sequences; }

    const CurveSequence &sequence(int index) const { return _sequences[index]; }
          CurveSequence &sequence(int index)       { return _sequences[index]; }

    // LFO-shape population functions at track level
    void populateWithLfoShape(int sequenceIndex, Curve::Type shape, int firstStep, int lastStep);
    void populateWithLfoPattern(int sequenceIndex, Curve::Type shape, int firstStep, int lastStep);
    void populateWithLfoWaveform(int sequenceIndex, Curve::Type upShape, Curve::Type downShape, int firstStep, int lastStep);

    // Advanced LFO waveform functions at track level
    void populateWithSineWaveLfo(int sequenceIndex, int firstStep, int lastStep);
    void populateWithTriangleWaveLfo(int sequenceIndex, int firstStep, int lastStep);
    void populateWithSawtoothWaveLfo(int sequenceIndex, int firstStep, int lastStep);
    void populateWithSquareWaveLfo(int sequenceIndex, int firstStep, int lastStep);

    //----------------------------------------
    // Routing
    //----------------------------------------

    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }

    //----------------------------------------
    // Methods
    //----------------------------------------

    CurveTrack() { clear(); }

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
    MuteMode _muteMode;
    uint8_t _slideTime;
    int16_t _offset;
    int8_t _rotate;
    int8_t _shapeProbabilityBias;
    int8_t _gateProbabilityBias;
    float _curveRate;  // Speed multiplier 0.0-4.0
    float _globalPhase;

    CurveSequenceArray _sequences;

    friend class Track;
};
