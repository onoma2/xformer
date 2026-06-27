#pragma once

#include "Config.h"
#include "Bitfield.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Types.h"
#include "Scale.h"
#include "RotatedScale.h"

#include "core/math/Math.h"

#include "Routing.h"
#include "RouteParamKey.h"
#include "ProjectVersion.h"

class FractalSequence {
public:
    // Cell count a fresh sequence/buffer initializes to (a musical default, not a
    // RAM bound — the buffer's max lives in Config as CONFIG_FRACTAL_MAX_CELLS).
    static constexpr int DefaultCells = 16;

    //----------------------------------------
    // Types
    //----------------------------------------

    // OrderMode — per-segment read order (KD-12 path traversal)
    enum class OrderMode : uint8_t {
        Forward, Reverse, Pendulum, Random, Converge, Diverge, Page, Last
    };

    static const char *orderModeName(OrderMode mode) {
        switch (mode) {
        case OrderMode::Forward:  return "Fwd";
        case OrderMode::Reverse:  return "Rev";
        case OrderMode::Pendulum: return "Pend";
        case OrderMode::Random:   return "Rand";
        case OrderMode::Converge: return "Conv";
        case OrderMode::Diverge:  return "Div";
        case OrderMode::Page:     return "Page";
        case OrderMode::Last:     break;
        }
        return nullptr;
    }

    // LoopMode — replay the captured loop forever or once
    enum class LoopMode : uint8_t {
        Loop, Once, Last
    };

    static const char *loopModeName(LoopMode mode) {
        switch (mode) {
        case LoopMode::Loop: return "Loop";
        case LoopMode::Once: return "Once";
        case LoopMode::Last: break;
        }
        return nullptr;
    }

    // RecordMode — overwrite the existing cell or latch onto it
    enum class RecordMode : uint8_t {
        Replace, Latch, Last
    };

    static const char *recordModeName(RecordMode mode) {
        switch (mode) {
        case RecordMode::Replace: return "Replace";
        case RecordMode::Latch:   return "Latch";
        case RecordMode::Last:    break;
        }
        return nullptr;
    }

    // CaptureCadence — capture per loop section or per parent event
    enum class CaptureCadence : uint8_t {
        Section, Event, Last
    };

    static const char *captureCadenceName(CaptureCadence mode) {
        switch (mode) {
        case CaptureCadence::Section: return "Section";
        case CaptureCadence::Event:   return "Event";
        case CaptureCadence::Last:    break;
        }
        return nullptr;
    }

    // CaptureFidelity — quantize the captured timing or keep the raw feel
    enum class CaptureFidelity : uint8_t {
        Quantized, Feel, Last
    };

    static const char *captureFidelityName(CaptureFidelity mode) {
        switch (mode) {
        case CaptureFidelity::Quantized: return "Quantized";
        case CaptureFidelity::Feel:      return "Feel";
        case CaptureFidelity::Last:      break;
        }
        return nullptr;
    }

    //----------------------------------------
    // Properties
    //----------------------------------------

    // trackIndex
    int trackIndex() const { return _trackIndex; }
    void setTrackIndex(int trackIndex) { _trackIndex = trackIndex; }

    // scale
    int rawScale() const { return int(_scaleGroup.scale) - 1; }
    int scale() const { return Routing::routedValueInt(ParamKey::Scale, _trackIndex, rawScale(), 0, Scale::Count - 1); }
    void setScale(int scale) { _scaleGroup.scale = clamp(scale, -1, Scale::Count - 1) + 1; }
    RotatedScaleView selectedScale(int projectScale, int projectRotate) const {
        int idx    = scale()       < 0 ? projectScale  : scale();
        int rotate = scaleRotate() < 0 ? projectRotate : scaleRotate();
        return RotatedScaleView(Scale::get(idx), rotate);
    }

    // rootNote
    int rawRootNote() const { return int(_scaleGroup.rootNote) - 1; }
    int rootNote() const { return Routing::routedValueInt(ParamKey::RootNote, _trackIndex, rawRootNote(), 0, 11); }
    void setRootNote(int rootNote) { _scaleGroup.rootNote = clamp(rootNote, -1, 11) + 1; }

    // scaleRotate
    int scaleRotate() const { return int(_scaleGroup.scaleRotate) - 1; }
    void setScaleRotate(int scaleRotate) { _scaleGroup.scaleRotate = clamp(scaleRotate, -1, 31) + 1; }

    // divisor
    int divisor() const { return _divisor; }
    void setDivisor(int divisor) { _divisor = ModelUtils::clampDivisor(divisor); }
    void printDivisor(StringBuilder &str) const { ModelUtils::printDivisor(str, _divisor); }
    void editDivisor(int value, bool shift) { setDivisor(ModelUtils::adjustedByDivisor(divisor(), value, shift)); }

    // clockMultiplier
    int clockMultiplier() const { return Routing::routedValueInt(ParamKey::ClockMultiplier, _trackIndex, _clockMultiplier, 50, 150); }
    void setClockMultiplier(int clockMultiplier) { _clockMultiplier = clamp(clockMultiplier, 50, 150); }
    void editClockMultiplier(int value, bool shift) { setClockMultiplier(clockMultiplier() + value * (shift ? 10 : 1)); }
    void printClockMultiplier(StringBuilder &str) const { str("%.2fx", clockMultiplier() * 0.01f); }

    // resetMeasure
    int resetMeasure() const { return _resetMeasure; }
    void setResetMeasure(int resetMeasure) { _resetMeasure = clamp(resetMeasure, 0, 128); }
    void editResetMeasure(int value, bool shift) { setResetMeasure(ModelUtils::adjustedByPowerOfTwo(resetMeasure(), value, shift)); }
    void printResetMeasure(StringBuilder &str) const {
        if (resetMeasure() == 0) {
            str("off");
        } else {
            str("%d %s", resetMeasure(), resetMeasure() > 1 ? "bars" : "bar");
        }
    }

    // loopFirst / loopLast
    int loopFirst() const { return _loopFirst; }
    void setLoopFirst(int v) { _loopFirst = clamp(v, 0, CONFIG_FRACTAL_MAX_CELLS - 1); }
    void editLoopFirst(int value, bool shift) { setLoopFirst(loopFirst() + value); }
    void printLoopFirst(StringBuilder &str) const { str("%d", loopFirst() + 1); }

    int loopLast() const { return _loopLast; }
    void setLoopLast(int v) { _loopLast = clamp(v, 0, CONFIG_FRACTAL_MAX_CELLS - 1); }
    void editLoopLast(int value, bool shift) { setLoopLast(loopLast() + value); }
    void printLoopLast(StringBuilder &str) const { str("%d", loopLast() + 1); }

    // rotate
    int rotate() const { return Routing::routedValueInt(ParamKey::Rotate, _trackIndex, _rotate, -64, 64); }
    void setRotate(int rotate) { _rotate = clamp(rotate, -64, 64); }

    // orderMode (per-segment read order)
    OrderMode orderMode() const { return _orderMode; }
    void setOrderMode(OrderMode v) { _orderMode = ModelUtils::clampedEnum(v); }
    void editOrderMode(int value, bool shift) { setOrderMode(ModelUtils::adjustedEnum(orderMode(), value)); }
    void printOrderMode(StringBuilder &str) const { str(orderModeName(orderMode())); }

    // loopMode
    LoopMode loopMode() const { return _loopMode; }
    void setLoopMode(LoopMode v) { _loopMode = ModelUtils::clampedEnum(v); }
    void editLoopMode(int value, bool shift) { setLoopMode(ModelUtils::adjustedEnum(loopMode(), value)); }
    void printLoopMode(StringBuilder &str) const { str(loopModeName(loopMode())); }

    // recordSkip (capture stride / Pack, 0..15). Plain setup field, not routable.
    int recordSkip() const { return _recordSkip; }
    void setRecordSkip(int v) { _recordSkip = clamp(v, 0, 15); }
    void editRecordSkip(int value, bool shift) { setRecordSkip(recordSkip() + value); }
    void printRecordSkip(StringBuilder &str) const { str("%d", recordSkip()); }

    // recordFirst / recordLast
    int recordFirst() const { return _recordFirst; }
    void setRecordFirst(int v) { _recordFirst = clamp(v, 0, CONFIG_FRACTAL_MAX_CELLS - 1); }
    void editRecordFirst(int value, bool shift) { setRecordFirst(recordFirst() + value); }
    void printRecordFirst(StringBuilder &str) const { str("%d", recordFirst() + 1); }

    int recordLast() const { return _recordLast; }
    void setRecordLast(int v) { _recordLast = clamp(v, 0, CONFIG_FRACTAL_MAX_CELLS - 1); }
    void editRecordLast(int value, bool shift) { setRecordLast(recordLast() + value); }
    void printRecordLast(StringBuilder &str) const { str("%d", recordLast() + 1); }

    // ornFirst / ornLast (ornament eligibility zone)
    int ornFirst() const { return _ornFirst; }
    void setOrnFirst(int v) { _ornFirst = clamp(v, 0, CONFIG_FRACTAL_MAX_CELLS - 1); }

    int ornLast() const { return _ornLast; }
    void setOrnLast(int v) { _ornLast = clamp(v, 0, CONFIG_FRACTAL_MAX_CELLS - 1); }

    // recordMode
    RecordMode recordMode() const { return _recordMode; }
    void setRecordMode(RecordMode v) { _recordMode = ModelUtils::clampedEnum(v); }
    void editRecordMode(int value, bool shift) { setRecordMode(ModelUtils::adjustedEnum(recordMode(), value)); }
    void printRecordMode(StringBuilder &str) const { str(recordModeName(recordMode())); }

    // captureCadence
    CaptureCadence captureCadence() const { return _captureCadence; }
    void setCaptureCadence(CaptureCadence v) { _captureCadence = ModelUtils::clampedEnum(v); }
    void editCaptureCadence(int value, bool shift) { setCaptureCadence(ModelUtils::adjustedEnum(captureCadence(), value)); }
    void printCaptureCadence(StringBuilder &str) const { str(captureCadenceName(captureCadence())); }

    // captureFidelity
    CaptureFidelity captureFidelity() const { return _captureFidelity; }
    void setCaptureFidelity(CaptureFidelity v) { _captureFidelity = ModelUtils::clampedEnum(v); }
    void editCaptureFidelity(int value, bool shift) { setCaptureFidelity(ModelUtils::adjustedEnum(captureFidelity(), value)); }
    void printCaptureFidelity(StringBuilder &str) const { str(captureFidelityName(captureFidelity())); }

    // recordTrigger (arm). Routable per-sequence.
    int recordTrigger() const { return Routing::routedValueInt(ParamKey::FractalRecordTrigger, _trackIndex, _recordTrigger, 0, 1); }
    void setRecordTrigger(int v) { _recordTrigger = clamp(v, 0, 1); }
    void editRecordTrigger(int value, bool shift) { setRecordTrigger(value > 0 ? 1 : 0); }
    void printRecordTrigger(StringBuilder &str) const { str(recordTrigger() ? "On" : "Off"); }

    // branchCount (0..7). Routable per-sequence.
    int branchCount() const { return Routing::routedValueInt(ParamKey::FractalBranchCount, _trackIndex, _branchCount, 0, 7); }
    void setBranchCount(int v) { _branchCount = clamp(v, 0, 7); }

    // path (bit-word, 0..255). Routable per-sequence.
    int path() const { return Routing::routedValueInt(ParamKey::FractalPath, _trackIndex, _path, 0, 255); }
    void setPath(int v) { _path = clamp(v, 0, 255); }

    // branchSeed
    int branchSeed() const { return _branchSeed; }
    void setBranchSeed(int v) { _branchSeed = clamp(v, 0, 65535); }

    // branchPool (enabled transform bitmask)
    int branchPool() const { return _branchPool; }
    void setBranchPool(int v) { _branchPool = clamp(v, 0, 255); }

    // ornamentRate (0..100). Routable per-sequence.
    int ornamentRate() const { return Routing::routedValueInt(ParamKey::FractalOrnamentRate, _trackIndex, _ornamentRate, 0, 100); }
    void setOrnamentRate(int v) { _ornamentRate = clamp(v, 0, 100); }

    // ornamentIntensity (0..100). Routable per-sequence.
    int ornamentIntensity() const { return Routing::routedValueInt(ParamKey::FractalOrnamentIntensity, _trackIndex, _ornamentIntensity, 0, 100); }
    void setOrnamentIntensity(int v) { _ornamentIntensity = clamp(v, 0, 100); }

    //----------------------------------------
    // Routing
    //----------------------------------------

    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }

    //----------------------------------------
    // Methods
    //----------------------------------------

    void clear() {
        setDefaults();
    }

    void write(VersionedSerializedWriter &writer) const {
        int8_t scaleField = rawScale();
        int8_t rootNoteField = rawRootNote();
        int8_t scaleRotateField = int8_t(scaleRotate());
        writer.write(scaleField);
        writer.write(rootNoteField);
        writer.write(scaleRotateField);
        writer.write(_divisor);
        writer.write(_resetMeasure);
        writer.write(_clockMultiplier);
        writer.write(_loopFirst);
        writer.write(_loopLast);
        writer.write(_rotate);
        writer.write(static_cast<uint8_t>(_orderMode));
        writer.write(static_cast<uint8_t>(_loopMode));
        writer.write(_recordSkip);
        writer.write(_recordFirst);
        writer.write(_recordLast);
        writer.write(_ornFirst);
        writer.write(_ornLast);
        writer.write(static_cast<uint8_t>(_recordMode));
        writer.write(static_cast<uint8_t>(_captureCadence));
        writer.write(static_cast<uint8_t>(_captureFidelity));
        writer.write(_recordTrigger);
        writer.write(_branchCount);
        writer.write(_path);
        writer.write(_branchSeed);
        writer.write(_branchPool);
        writer.write(_ornamentRate);
        writer.write(_ornamentIntensity);
    }

    void read(VersionedSerializedReader &reader) {
        int8_t scaleField = 0;
        int8_t rootNoteField = 0;
        int8_t scaleRotateField = 0;
        reader.read(scaleField);
        reader.read(rootNoteField);
        reader.read(scaleRotateField);
        setScale(scaleField);
        setRootNote(rootNoteField);
        setScaleRotate(scaleRotateField);
        reader.read(_divisor);
        _divisor = ModelUtils::clampDivisor(_divisor);
        reader.read(_resetMeasure);
        _resetMeasure = clamp(int(_resetMeasure), 0, 128);
        reader.read(_clockMultiplier);
        reader.read(_loopFirst);
        reader.read(_loopLast);
        reader.read(_rotate);
        uint8_t orderMode;
        reader.read(orderMode);
        _orderMode = ModelUtils::clampedEnum(static_cast<OrderMode>(orderMode));
        uint8_t loopMode;
        reader.read(loopMode);
        _loopMode = ModelUtils::clampedEnum(static_cast<LoopMode>(loopMode));
        reader.read(_recordSkip);
        reader.read(_recordFirst);
        reader.read(_recordLast);
        reader.read(_ornFirst);
        reader.read(_ornLast);
        uint8_t recordMode;
        reader.read(recordMode);
        _recordMode = ModelUtils::clampedEnum(static_cast<RecordMode>(recordMode));
        uint8_t captureCadence;
        reader.read(captureCadence);
        _captureCadence = ModelUtils::clampedEnum(static_cast<CaptureCadence>(captureCadence));
        uint8_t captureFidelity;
        reader.read(captureFidelity);
        _captureFidelity = ModelUtils::clampedEnum(static_cast<CaptureFidelity>(captureFidelity));
        reader.read(_recordTrigger);
        reader.read(_branchCount);
        reader.read(_path);
        reader.read(_branchSeed);
        reader.read(_branchPool);
        reader.read(_ornamentRate);
        reader.read(_ornamentIntensity);
    }

private:
    void setDefaults() {
        _scaleGroup.raw = 0;
        setScale(-1);
        setRootNote(-1);
        setScaleRotate(-1);
        _divisor = 12;
        _resetMeasure = 0;
        _clockMultiplier = 100;
        _loopFirst = 0;
        _loopLast = DefaultCells - 1;
        _rotate = 0;
        _orderMode = OrderMode::Forward;
        _loopMode = LoopMode::Loop;
        _recordSkip = 0;
        _recordFirst = 0;
        _recordLast = DefaultCells - 1;
        _ornFirst = 0;
        _ornLast = DefaultCells - 1;
        _recordMode = RecordMode::Replace;
        _captureCadence = CaptureCadence::Event;
        _captureFidelity = CaptureFidelity::Feel;
        _recordTrigger = 0;
        _branchCount = 0;
        _path = 0;
        _branchSeed = 0;
        _branchPool = 0xff;
        _ornamentRate = 0;
        _ornamentIntensity = 0;
    }

    int8_t _trackIndex = -1;
    union {
        uint16_t raw;
        BitField<uint16_t, 0, 5> scale;       // scale + 1
        BitField<uint16_t, 5, 4> rootNote;    // rootNote + 1
        BitField<uint16_t, 9, 6> scaleRotate; // scaleRotate + 1
    } _scaleGroup = { 0 };
    uint8_t _divisor = 12;
    uint8_t _resetMeasure = 0;
    uint8_t _clockMultiplier = 100;

    uint8_t _loopFirst = 0;
    uint8_t _loopLast = 0;
    int8_t _rotate = 0;
    OrderMode _orderMode = OrderMode::Forward;
    LoopMode _loopMode = LoopMode::Loop;
    uint8_t _recordSkip = 0;
    uint8_t _recordFirst = 0;
    uint8_t _recordLast = 0;
    uint8_t _ornFirst = 0;
    uint8_t _ornLast = 0;
    RecordMode _recordMode = RecordMode::Replace;
    CaptureCadence _captureCadence = CaptureCadence::Section;
    CaptureFidelity _captureFidelity = CaptureFidelity::Quantized;
    uint8_t _recordTrigger = 0;
    uint8_t _branchCount = 0;
    uint8_t _path = 0;
    uint16_t _branchSeed = 0;
    uint8_t _branchPool = 0xff;
    uint8_t _ornamentRate = 0;
    uint8_t _ornamentIntensity = 0;

    friend class FractalTrack;
};

static_assert(sizeof(FractalSequence) <= 560, "FractalSequence too large");
