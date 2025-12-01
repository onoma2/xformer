#pragma once

#include "Config.h"
#include "Types.h"
#include "CurveSequence.h"
#include "Serialize.h"
#include "Routing.h"

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

    int slideTime() const { return _slideTime.get(isRouted(Routing::Target::SlideTime)); }
    void setSlideTime(int slideTime, bool routed = false) {
        _slideTime.set(clamp(slideTime, 0, 100), routed);
    }

    void editSlideTime(int value, bool shift) {
        if (!isRouted(Routing::Target::SlideTime)) {
            setSlideTime(ModelUtils::adjustedByStep(slideTime(), value, 5, !shift));
        }
    }

    void printSlideTime(StringBuilder &str) const {
        printRouted(str, Routing::Target::SlideTime);
        str("%d%%", slideTime());
    }

    // offset

    int offset() const { return _offset.get(isRouted(Routing::Target::Offset)); }
    float offsetVolts() const { return offset() * 0.01f; }
    void setOffset(int offset, bool routed = false) {
        _offset.set(clamp(offset, -500, 500), routed);
    }

    void editOffset(int value, bool shift) {
        if (!isRouted(Routing::Target::Offset)) {
            setOffset(offset() + value * (shift ? 100 : 1));
        }
    }

    void printOffset(StringBuilder &str) const {
        printRouted(str, Routing::Target::Offset);
        str("%+.2fV", offsetVolts());
    }

    // rotate

    int rotate() const { return _rotate.get(isRouted(Routing::Target::Rotate)); }
    void setRotate(int rotate, bool routed = false) {
        _rotate.set(clamp(rotate, -64, 64), routed);
    }

    void editRotate(int value, bool shift) {
        if (!isRouted(Routing::Target::Rotate)) {
            setRotate(rotate() + value);
        }
    }

    void printRotate(StringBuilder &str) const {
        printRouted(str, Routing::Target::Rotate);
        str("%+d", rotate());
    }

    // shapeProbabilityBias

    int shapeProbabilityBias() const { return _shapeProbabilityBias.get(isRouted(Routing::Target::ShapeProbabilityBias)); }
    void setShapeProbabilityBias(int shapeProbabilityBias, bool routed = false) {
        _shapeProbabilityBias.set(clamp(shapeProbabilityBias, -8, 8), routed);
    }

    void editShapeProbabilityBias(int value, bool shift) {
        if (!isRouted(Routing::Target::ShapeProbabilityBias)) {
            setShapeProbabilityBias(shapeProbabilityBias() + value);
        }
    }

    void printShapeProbabilityBias(StringBuilder &str) const {
        printRouted(str, Routing::Target::ShapeProbabilityBias);
        str("%+.1f%%", shapeProbabilityBias() * 12.5f);
    }

    // gateProbabilityBias

    int gateProbabilityBias() const { return _gateProbabilityBias.get(isRouted(Routing::Target::GateProbabilityBias)); }
    void setGateProbabilityBias(int gateProbabilityBias, bool routed = false) {
        _gateProbabilityBias.set(clamp(gateProbabilityBias, -CurveSequence::GateProbability::Range, CurveSequence::GateProbability::Range), routed);
    }

    void editGateProbabilityBias(int value, bool shift) {
        if (!isRouted(Routing::Target::GateProbabilityBias)) {
            setGateProbabilityBias(gateProbabilityBias() + value);
        }
    }

    void printGateProbabilityBias(StringBuilder &str) const {
        printRouted(str, Routing::Target::GateProbabilityBias);
        str("%+.1f%%", gateProbabilityBias() * 12.5f);
    }

    // globalPhase

    float globalPhase() const { return _globalPhase; }
    void setGlobalPhase(float globalPhase) {
        _globalPhase = clamp(globalPhase, 0.f, 1.f);
    }

    void editGlobalPhase(float value, bool shift) {
        setGlobalPhase(globalPhase() + value * (shift ? 0.1f : 0.01f));
    }

    void printGlobalPhase(StringBuilder &str) const {
        str("%.2f", globalPhase());
    }

    // wavefolderFold

    float wavefolderFold() const { return _wavefolderFold; }
    void setWavefolderFold(float value) { _wavefolderFold = clamp(value, 0.f, 1.f); }
    void editWavefolderFold(int value, bool shift) { setWavefolderFold(wavefolderFold() + value * (shift ? 0.1f : 0.01f)); }
    void printWavefolderFold(StringBuilder &str) const { str("%.2f", wavefolderFold()); }

    // wavefolderGain

    float wavefolderGain() const { return _wavefolderGain; }
    void setWavefolderGain(float value) { _wavefolderGain = clamp(value, 0.f, 2.f); }
    void editWavefolderGain(int value, bool shift) { setWavefolderGain(wavefolderGain() + value * (shift ? 0.1f : 0.01f)); }
    void printWavefolderGain(StringBuilder &str) const { str("%.2f", wavefolderGain()); }

    // djFilter

    float djFilter() const { return _djFilter; }
    void setDjFilter(float value) { _djFilter = clamp(value, -1.f, 1.f); }
    void editDjFilter(int value, bool shift) { setDjFilter(djFilter() + value * (shift ? 0.1f : 0.01f)); }
    void printDjFilter(StringBuilder &str) const { str("%+.2f", djFilter()); }

    // xFade
    float xFade() const { return _xFade; }
    void setXFade(float value) { _xFade = clamp(value, 0.f, 1.f); }
    void editXFade(int value, bool shift) { setXFade(xFade() + value * (shift ? 0.1f : 0.01f)); }
    void printXFade(StringBuilder &str) const { str("%.2f", xFade()); }

    // chaos

    int chaosAmount() const { return _chaosAmount; }
    void setChaosAmount(int value) { _chaosAmount = clamp(value, 0, 100); }
    void editChaosAmount(int value, bool shift) { setChaosAmount(chaosAmount() + value * (shift ? 5 : 1)); }
    void printChaosAmount(StringBuilder &str) const { str("%d%%", chaosAmount()); }

    int chaosRate() const { return _chaosRate; }
    void setChaosRate(int value) { _chaosRate = clamp(value, 0, 127); }
    void editChaosRate(int value, bool shift) { setChaosRate(chaosRate() + value * (shift ? 5 : 1)); }
    float chaosHz() const { return 0.1f + std::pow(_chaosRate / 127.f, 2.f) * 100.f; }
    void printChaosRate(StringBuilder &str) const {
        float rate = chaosHz();
        if (rate < 10.f) str("%.1fHz", rate);
        else str("%.0fHz", rate);
    }

    int chaosParam1() const { return _chaosParam1; }
    void setChaosParam1(int value) { _chaosParam1 = clamp(value, 0, 100); }
    void editChaosParam1(int value, bool shift) { setChaosParam1(chaosParam1() + value * (shift ? 5 : 1)); }
    void printChaosParam1(StringBuilder &str) const { str("%d", chaosParam1()); }

    int chaosParam2() const { return _chaosParam2; }
    void setChaosParam2(int value) { _chaosParam2 = clamp(value, 0, 100); }
    void editChaosParam2(int value, bool shift) { setChaosParam2(chaosParam2() + value * (shift ? 5 : 1)); }
    void printChaosParam2(StringBuilder &str) const { str("%d", chaosParam2()); }

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
    void writeRouted(Routing::Target target, int intValue, float floatValue);

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
    Routable<uint8_t> _slideTime;
    Routable<int16_t> _offset;
    Routable<int8_t> _rotate;
    Routable<int8_t> _shapeProbabilityBias;
    Routable<int8_t> _gateProbabilityBias;
    float _globalPhase;
    float _wavefolderFold;
    float _wavefolderGain;
    float _djFilter;
    float _xFade;

    int _chaosAmount;
    int _chaosRate;
    int _chaosParam1;
    int _chaosParam2;

    CurveSequenceArray _sequences;

    friend class Track;
};
