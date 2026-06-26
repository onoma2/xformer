#pragma once

#include "Config.h"
#include "Types.h"
#include "FractalSequence.h"
#include "Serialize.h"
#include "Routing.h"
#include "RouteParamKey.h"

class FractalTrack {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    using FractalSequenceArray = std::array<FractalSequence, CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT>;

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

    // GateLogic — combine the two source gates (KD-4)
    enum class GateLogic : uint8_t {
        A, B, And, Or, Xor, Nand, Last
    };

    static const char *gateLogicName(GateLogic logic) {
        switch (logic) {
        case GateLogic::A:    return "A";
        case GateLogic::B:    return "B";
        case GateLogic::And:  return "And";
        case GateLogic::Or:   return "Or";
        case GateLogic::Xor:  return "Xor";
        case GateLogic::Nand: return "Nand";
        case GateLogic::Last: break;
        }
        return nullptr;
    }

    // CvLogic — combine the two source CVs (KD-4)
    enum class CvLogic : uint8_t {
        A, B, Min, Max, Sum, Avg, Gated, Last
    };

    static const char *cvLogicName(CvLogic logic) {
        switch (logic) {
        case CvLogic::A:     return "A";
        case CvLogic::B:     return "B";
        case CvLogic::Min:   return "Min";
        case CvLogic::Max:   return "Max";
        case CvLogic::Sum:   return "Sum";
        case CvLogic::Avg:   return "Avg";
        case CvLogic::Gated: return "Gat";
        case CvLogic::Last:  break;
        }
        return nullptr;
    }

    //----------------------------------------
    // Properties
    //----------------------------------------

    // sourceA / sourceB (parent track indices; -1 = none)
    int sourceA() const { return _sourceA; }
    void setSourceA(int v) { _sourceA = clamp(v, -1, CONFIG_TRACK_COUNT - 1); }
    void editSourceA(int value, bool shift) { setSourceA(sourceA() + value); }
    void printSourceA(StringBuilder &str) const { printSource(str, sourceA()); }

    int sourceB() const { return _sourceB; }
    void setSourceB(int v) { _sourceB = clamp(v, -1, CONFIG_TRACK_COUNT - 1); }
    void editSourceB(int value, bool shift) { setSourceB(sourceB() + value); }
    void printSourceB(StringBuilder &str) const { printSource(str, sourceB()); }

    // gateLogic / cvLogic
    GateLogic gateLogic() const { return _gateLogic; }
    void setGateLogic(GateLogic v) { _gateLogic = ModelUtils::clampedEnum(v); }

    CvLogic cvLogic() const { return _cvLogic; }
    void setCvLogic(CvLogic v) { _cvLogic = ModelUtils::clampedEnum(v); }

    // bufferLength
    int bufferLength() const { return _bufferLength; }
    void setBufferLength(int v) { _bufferLength = clamp(v, 1, CONFIG_FRACTAL_MAX_CELLS); }

    // lock
    bool lock() const { return _lock; }
    void setLock(bool lock) { _lock = lock; }

    // recordMuted — ghost source: capture parent even while it's muted (KD-21)
    bool recordMuted() const { return _recordMuted; }
    void setRecordMuted(bool recordMuted) { _recordMuted = recordMuted; }
    void printRecordMuted(StringBuilder &str) const { ModelUtils::printYesNo(str, _recordMuted); }
    void editRecordMuted(int value, bool shift) { setRecordMuted(value > 0); }

    // octave
    int octave() const { return Routing::routedValueInt(ParamKey::Octave, _trackIndex, _octave, -10, 10); }
    void setOctave(int octave) { _octave = clamp(octave, -10, 10); }

    // transpose
    int transpose() const { return Routing::routedValueInt(ParamKey::Transpose, _trackIndex, _transpose, -100, 100); }
    void setTranspose(int transpose) { _transpose = clamp(transpose, -100, 100); }

    // slideTime
    int slideTime() const { return Routing::routedValueInt(ParamKey::SlideTime, _trackIndex, _slideTime, 0, 100); }
    void setSlideTime(int slideTime) { _slideTime = clamp(slideTime, 0, 100); }

    // cvUpdateMode
    CvUpdateMode cvUpdateMode() const { return _cvUpdateMode; }
    void setCvUpdateMode(CvUpdateMode mode) { _cvUpdateMode = ModelUtils::clampedEnum(mode); }

    // trackDelay (0..16 sections)
    int trackDelay() const { return _trackDelay; }
    void setTrackDelay(int v) { _trackDelay = clamp(v, 0, 16); }

    // scale group (track-wide)
    int rawScale() const { return int(_scaleGroup.scale) - 1; }
    int scale() const { return rawScale(); }
    void setScale(int scale) { _scaleGroup.scale = clamp(scale, -1, Scale::Count - 1) + 1; }

    int rawRootNote() const { return int(_scaleGroup.rootNote) - 1; }
    int rootNote() const { return rawRootNote(); }
    void setRootNote(int rootNote) { _scaleGroup.rootNote = clamp(rootNote, -1, 11) + 1; }

    int scaleRotate() const { return int(_scaleGroup.scaleRotate) - 1; }
    void setScaleRotate(int v) { _scaleGroup.scaleRotate = clamp(v, -1, 31) + 1; }

    // playMode
    Types::PlayMode playMode() const { return _playMode; }
    void setPlayMode(Types::PlayMode playMode) { _playMode = ModelUtils::clampedEnum(playMode); }

    // sequences
    const FractalSequenceArray &sequences() const { return _sequences; }
          FractalSequenceArray &sequences()       { return _sequences; }

    const FractalSequence &sequence(int index) const { return _sequences[index]; }
          FractalSequence &sequence(int index)       { return _sequences[index]; }

    //----------------------------------------
    // Routing
    //----------------------------------------

    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }

    //----------------------------------------
    // Methods
    //----------------------------------------

    FractalTrack() { clear(); }

    void clear() {
        _sourceA = -1;
        _sourceB = -1;
        _gateLogic = GateLogic::A;
        _cvLogic = CvLogic::A;
        _bufferLength = CONFIG_FRACTAL_DEFAULT_CELLS;
        _lock = false;
        _octave = 0;
        _transpose = 0;
        _slideTime = 0;
        _cvUpdateMode = CvUpdateMode::Gate;
        _trackDelay = 0;
        _scaleGroup.raw = 0;
        setScale(-1);
        setRootNote(-1);
        setScaleRotate(-1);
        _playMode = Types::PlayMode::Aligned;
        _recordMuted = false;

        for (auto &sequence : _sequences) {
            sequence.clear();
        }
    }

    void write(VersionedSerializedWriter &writer) const {
        writer.write(_sourceA);
        writer.write(_sourceB);
        writer.write(static_cast<uint8_t>(_gateLogic));
        writer.write(static_cast<uint8_t>(_cvLogic));
        writer.write(_bufferLength);
        writer.write(_lock);
        writer.write(_octave);
        writer.write(_transpose);
        writer.write(_slideTime);
        writer.write(static_cast<uint8_t>(_cvUpdateMode));
        writer.write(_trackDelay);
        writer.write(_scaleGroup.raw);
        writer.write(static_cast<uint8_t>(_playMode));
        writer.write(_recordMuted);

        for (const auto &sequence : _sequences) {
            sequence.write(writer);
        }
    }

    void read(VersionedSerializedReader &reader) {
        reader.read(_sourceA);
        reader.read(_sourceB);
        uint8_t gateLogic;
        reader.read(gateLogic);
        _gateLogic = gateLogic < uint8_t(GateLogic::Last) ? static_cast<GateLogic>(gateLogic) : GateLogic::A;
        uint8_t cvLogic;
        reader.read(cvLogic);
        _cvLogic = cvLogic < uint8_t(CvLogic::Last) ? static_cast<CvLogic>(cvLogic) : CvLogic::A;
        reader.read(_bufferLength);
        reader.read(_lock);
        reader.read(_octave);
        reader.read(_transpose);
        reader.read(_slideTime);
        uint8_t cvUpdateMode;
        reader.read(cvUpdateMode);
        _cvUpdateMode = cvUpdateMode < uint8_t(CvUpdateMode::Last) ? static_cast<CvUpdateMode>(cvUpdateMode) : CvUpdateMode::Gate;
        reader.read(_trackDelay);
        reader.read(_scaleGroup.raw);
        uint8_t playMode;
        reader.read(playMode);
        _playMode = playMode < uint8_t(Types::PlayMode::Last) ? static_cast<Types::PlayMode>(playMode) : Types::PlayMode::Aligned;
        reader.read(_recordMuted);

        for (auto &sequence : _sequences) {
            sequence.read(reader);
        }
    }

private:
    static void printSource(StringBuilder &str, int source) {
        if (source < 0) {
            str("None");
        } else {
            str("Track %d", source + 1);
        }
    }

    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
        for (auto &sequence : _sequences) {
            sequence.setTrackIndex(trackIndex);
        }
    }

    int8_t _trackIndex = -1;

    int8_t _sourceA;
    int8_t _sourceB;
    GateLogic _gateLogic;
    CvLogic _cvLogic;
    uint8_t _bufferLength;
    bool _lock;
    int8_t _octave;
    int8_t _transpose;
    uint8_t _slideTime;
    CvUpdateMode _cvUpdateMode;
    uint8_t _trackDelay;
    union {
        uint16_t raw;
        BitField<uint16_t, 0, 5> scale;
        BitField<uint16_t, 5, 4> rootNote;
        BitField<uint16_t, 9, 6> scaleRotate;
    } _scaleGroup = { 0 };
    Types::PlayMode _playMode;
    bool _recordMuted;

    FractalSequenceArray _sequences;

    friend class Track;
};

static_assert(sizeof(FractalTrack) <= 9544, "FractalTrack too large");
