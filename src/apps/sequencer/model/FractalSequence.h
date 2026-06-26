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

    // resetMeasure
    int resetMeasure() const { return _resetMeasure; }
    void setResetMeasure(int resetMeasure) { _resetMeasure = clamp(resetMeasure, 0, 128); }

    // runMode
    Types::RunMode runMode() const { return _runMode; }
    void setRunMode(Types::RunMode runMode) { _runMode = ModelUtils::clampedEnum(runMode); }

    // loopFirst / loopLast
    int loopFirst() const { return _loopFirst; }
    void setLoopFirst(int v) { _loopFirst = clamp(v, 0, CONFIG_FRACTAL_MAX_CELLS - 1); }

    int loopLast() const { return _loopLast; }
    void setLoopLast(int v) { _loopLast = clamp(v, 0, CONFIG_FRACTAL_MAX_CELLS - 1); }

    // rotate
    int rotate() const { return Routing::routedValueInt(ParamKey::Rotate, _trackIndex, _rotate, -64, 64); }
    void setRotate(int rotate) { _rotate = clamp(rotate, -64, 64); }

    // orderMode (per-segment read order, 0..6)
    int orderMode() const { return _orderMode; }
    void setOrderMode(int v) { _orderMode = clamp(v, 0, 6); }

    // loopMode (0 = Loop, 1 = Once)
    int loopMode() const { return _loopMode; }
    void setLoopMode(int v) { _loopMode = clamp(v, 0, 1); }

    // sleep
    int sleep() const { return Routing::routedValueInt(ParamKey::Sleep, _trackIndex, _sleep, 0, 100); }
    void setSleep(int sleep) { _sleep = clamp(sleep, 0, 100); }

    // recordFirst / recordLast
    int recordFirst() const { return _recordFirst; }
    void setRecordFirst(int v) { _recordFirst = clamp(v, 0, CONFIG_FRACTAL_MAX_CELLS - 1); }

    int recordLast() const { return _recordLast; }
    void setRecordLast(int v) { _recordLast = clamp(v, 0, CONFIG_FRACTAL_MAX_CELLS - 1); }

    // ornFirst / ornLast (ornament eligibility zone)
    int ornFirst() const { return _ornFirst; }
    void setOrnFirst(int v) { _ornFirst = clamp(v, 0, CONFIG_FRACTAL_MAX_CELLS - 1); }

    int ornLast() const { return _ornLast; }
    void setOrnLast(int v) { _ornLast = clamp(v, 0, CONFIG_FRACTAL_MAX_CELLS - 1); }

    // recordMode (0 = Replace, 1 = Latch)
    int recordMode() const { return _recordMode; }
    void setRecordMode(int v) { _recordMode = clamp(v, 0, 1); }

    // captureCadence (0 = Section, 1 = Event)
    int captureCadence() const { return _captureCadence; }
    void setCaptureCadence(int v) { _captureCadence = clamp(v, 0, 1); }

    // captureFidelity (0 = Quantized, 1 = Feel)
    int captureFidelity() const { return _captureFidelity; }
    void setCaptureFidelity(int v) { _captureFidelity = clamp(v, 0, 1); }

    // recordTrigger (arm). Routing destination deferred until the engine
    // capture path lands; raw field only for now.
    int recordTrigger() const { return _recordTrigger; }
    void setRecordTrigger(int v) { _recordTrigger = clamp(v, 0, 1); }

    // branchCount (0..7). Routing deferred.
    int branchCount() const { return _branchCount; }
    void setBranchCount(int v) { _branchCount = clamp(v, 0, 7); }

    // path (bit-word, 0..255). Routing deferred.
    int path() const { return _path; }
    void setPath(int v) { _path = clamp(v, 0, 255); }

    // branchSeed
    int branchSeed() const { return _branchSeed; }
    void setBranchSeed(int v) { _branchSeed = clamp(v, 0, 65535); }

    // branchPool (enabled transform bitmask)
    int branchPool() const { return _branchPool; }
    void setBranchPool(int v) { _branchPool = clamp(v, 0, 255); }

    // ornamentRate (0..100). Routing deferred.
    int ornamentRate() const { return _ornamentRate; }
    void setOrnamentRate(int v) { _ornamentRate = clamp(v, 0, 100); }

    // ornamentIntensity (0..100). Routing deferred.
    int ornamentIntensity() const { return _ornamentIntensity; }
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
        writer.write(static_cast<uint8_t>(_runMode));
        writer.write(_loopFirst);
        writer.write(_loopLast);
        writer.write(_rotate);
        writer.write(_orderMode);
        writer.write(_loopMode);
        writer.write(_sleep);
        writer.write(_recordFirst);
        writer.write(_recordLast);
        writer.write(_ornFirst);
        writer.write(_ornLast);
        writer.write(_recordMode);
        writer.write(_captureCadence);
        writer.write(_captureFidelity);
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
        uint8_t runMode;
        reader.read(runMode);
        _runMode = runMode < uint8_t(Types::RunMode::Last) ? static_cast<Types::RunMode>(runMode) : Types::RunMode::Forward;
        reader.read(_loopFirst);
        reader.read(_loopLast);
        reader.read(_rotate);
        reader.read(_orderMode);
        reader.read(_loopMode);
        reader.read(_sleep);
        reader.read(_recordFirst);
        reader.read(_recordLast);
        reader.read(_ornFirst);
        reader.read(_ornLast);
        reader.read(_recordMode);
        reader.read(_captureCadence);
        reader.read(_captureFidelity);
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
        _runMode = Types::RunMode::Forward;
        _loopFirst = 0;
        _loopLast = CONFIG_FRACTAL_DEFAULT_CELLS - 1;
        _rotate = 0;
        _orderMode = 0;
        _loopMode = 0;
        _sleep = 0;
        _recordFirst = 0;
        _recordLast = CONFIG_FRACTAL_DEFAULT_CELLS - 1;
        _ornFirst = 0;
        _ornLast = CONFIG_FRACTAL_DEFAULT_CELLS - 1;
        _recordMode = 0;
        _captureCadence = 0;
        _captureFidelity = 0;
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
    Types::RunMode _runMode = Types::RunMode::Forward;

    uint8_t _loopFirst = 0;
    uint8_t _loopLast = 0;
    int8_t _rotate = 0;
    uint8_t _orderMode = 0;
    uint8_t _loopMode = 0;
    uint8_t _sleep = 0;
    uint8_t _recordFirst = 0;
    uint8_t _recordLast = 0;
    uint8_t _ornFirst = 0;
    uint8_t _ornLast = 0;
    uint8_t _recordMode = 0;
    uint8_t _captureCadence = 0;
    uint8_t _captureFidelity = 0;
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
