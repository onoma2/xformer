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

    // Source slot classification. A slot value is None (-1), a parent Track
    // (0..CONFIG_TRACK_COUNT-1), or a routing Channel (above the track range,
    // indexing into the ordered kSourceChannels list).
    enum class SourceKind { None, Track, Channel };

    // Ordered single-channel list a source slot can select after the tracks:
    // CvIn1-4, CvOut1-8, BusCv1-4, GateOut1-8, Mod1-8 (the full exposed set).
    static constexpr int sourceChannelCount() { return 4 + 8 + 4 + 8 + 8; }
    static Routing::Source sourceChannel(int listIndex) {
        static const Routing::Source kSourceChannels[] = {
            Routing::Source::CvIn1, Routing::Source::CvIn2, Routing::Source::CvIn3, Routing::Source::CvIn4,
            Routing::Source::CvOut1, Routing::Source::CvOut2, Routing::Source::CvOut3, Routing::Source::CvOut4,
            Routing::Source::CvOut5, Routing::Source::CvOut6, Routing::Source::CvOut7, Routing::Source::CvOut8,
            Routing::Source::BusCv1, Routing::Source::BusCv2, Routing::Source::BusCv3, Routing::Source::BusCv4,
            Routing::Source::GateOut1, Routing::Source::GateOut2, Routing::Source::GateOut3, Routing::Source::GateOut4,
            Routing::Source::GateOut5, Routing::Source::GateOut6, Routing::Source::GateOut7, Routing::Source::GateOut8,
            Routing::Source::Mod1, Routing::Source::Mod2, Routing::Source::Mod3, Routing::Source::Mod4,
            Routing::Source::Mod5, Routing::Source::Mod6, Routing::Source::Mod7, Routing::Source::Mod8,
        };
        return kSourceChannels[clamp(listIndex, 0, sourceChannelCount() - 1)];
    }
    static SourceKind sourceKind(int v) {
        if (v < 0) return SourceKind::None;
        if (v < CONFIG_TRACK_COUNT) return SourceKind::Track;
        return SourceKind::Channel;
    }
    // Channel slot → Routing::Source (caller checks sourceKind == Channel first).
    static Routing::Source sourceChannelOf(int v) { return sourceChannel(v - CONFIG_TRACK_COUNT); }
    static int sourceMax() { return CONFIG_TRACK_COUNT - 1 + sourceChannelCount(); }

    //----------------------------------------
    // Properties
    //----------------------------------------

    // sourceA / sourceB: -1 = None, 0..CONFIG_TRACK_COUNT-1 = parent track,
    // above that = a single routing channel (sourceChannelOf).
    int sourceA() const { return _sourceA; }
    void setSourceA(int v) { _sourceA = sanitizeSource(clamp(v, -1, sourceMax())); }
    void editSourceA(int value, bool shift) { _sourceA = stepSource(_sourceA, value); }
    void printSourceA(StringBuilder &str) const { printSource(str, sourceA()); }

    int sourceB() const { return _sourceB; }
    void setSourceB(int v) { _sourceB = sanitizeSource(clamp(v, -1, sourceMax())); }
    void editSourceB(int value, bool shift) { _sourceB = stepSource(_sourceB, value); }
    void printSourceB(StringBuilder &str) const { printSource(str, sourceB()); }

    // A Track-kind source equal to this track's own index is a self-reference
    // (output feedback) — never allowed. Direct set snaps it to None; the encoder
    // walk skips over it in the travel direction.
    int sanitizeSource(int v) const {
        return (v == _trackIndex && sourceKind(v) == SourceKind::Track) ? -1 : v;
    }
    int stepSource(int cur, int delta) const {
        int v = clamp(cur + delta, -1, sourceMax());
        if (v == _trackIndex && sourceKind(v) == SourceKind::Track)
            v = clamp(v + (delta >= 0 ? 1 : -1), -1, sourceMax());
        return v;
    }

    // Any slot selects a source (gates capture). With channels, slot B alone may
    // hold a source, so this can't reduce to sourceA() >= 0.
    bool hasSource() const { return _sourceA >= 0 || _sourceB >= 0; }

    // gateLogic / cvLogic
    GateLogic gateLogic() const { return _gateLogic; }
    void setGateLogic(GateLogic v) { _gateLogic = ModelUtils::clampedEnum(v); }

    CvLogic cvLogic() const { return _cvLogic; }
    void setCvLogic(CvLogic v) { _cvLogic = ModelUtils::clampedEnum(v); }

    // bufferLength
    int bufferLength() const { return _bufferLength; }
    void setBufferLength(int v) {
        _bufferLength = clamp(v, 1, CONFIG_FRACTAL_MAX_CELLS);
        // Pull every pattern's loop/record/orn edges in to fit a shrunk buffer.
        int maxCell = _bufferLength - 1;
        for (auto &seq : _sequences) {
            seq.setLoopFirst(clamp(seq.loopFirst(), 0, maxCell));
            seq.setLoopLast(clamp(seq.loopLast(), 0, maxCell));
            seq.setRecordFirst(clamp(seq.recordFirst(), 0, maxCell));
            seq.setRecordLast(clamp(seq.recordLast(), 0, maxCell));
            seq.setOrnFirst(clamp(seq.ornFirst(), 0, maxCell));
            seq.setOrnLast(clamp(seq.ornLast(), 0, maxCell));
        }
    }

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

    // quantize: false = raw (no quantize), true = snap the played main note to
    // the sequence scale. Trunk storage stays raw; quantize only shapes playback.
    bool quantize() const { return _quantize; }
    void setQuantize(bool v) { _quantize = v; }
    void editQuantize(int value, bool shift) { setQuantize(value > 0); }
    void printQuantize(StringBuilder &str) const { str(_quantize ? "On" : "Raw"); }

    // cvUpdateMode
    CvUpdateMode cvUpdateMode() const { return _cvUpdateMode; }
    void setCvUpdateMode(CvUpdateMode mode) { _cvUpdateMode = ModelUtils::clampedEnum(mode); }

    // trackDelay (0..16 sections)
    int trackDelay() const { return _trackDelay; }
    void setTrackDelay(int v) { _trackDelay = clamp(v, 0, 16); }

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
        _bufferLength = FractalSequence::DefaultCells;
        _lock = false;
        _octave = 0;
        _transpose = 0;
        _slideTime = 0;
        _cvUpdateMode = CvUpdateMode::Gate;
        _quantize = false;
        _trackDelay = 0;
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
        writer.write(_quantize);
        writer.write(_trackDelay);
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
        reader.read(_quantize);
        reader.read(_trackDelay);
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
        switch (sourceKind(source)) {
        case SourceKind::None:    str("None"); break;
        case SourceKind::Track:   str("Track %d", source + 1); break;
        case SourceKind::Channel: Routing::printSource(sourceChannelOf(source), str); break;
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
    bool _quantize;
    uint8_t _trackDelay;
    Types::PlayMode _playMode;
    bool _recordMuted;

    FractalSequenceArray _sequences;

    friend class Track;
};

static_assert(sizeof(FractalTrack) <= 9544, "FractalTrack too large");
