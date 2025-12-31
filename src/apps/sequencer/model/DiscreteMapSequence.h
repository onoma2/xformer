#pragma once

#include "Config.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Types.h"
#include "Scale.h"
#include "Routing.h"

#include "core/math/Math.h"
#include "core/utils/StringBuilder.h"

#include <array>
#include <cstdint>

class DiscreteMapSequence {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    static constexpr int StageCount = 32;

    //----------------------------------------
    // Stage
    //----------------------------------------

    class Stage {
    public:
        enum class TriggerDir : uint8_t {
            Rise,
            Fall,
            Off,
            Both
        };

        // Threshold (-100 to +100)
        int8_t threshold() const { return _threshold; }
        void setThreshold(int threshold) {
            _threshold = clamp(threshold, -100, 100);
        }

        // Direction
        TriggerDir direction() const { return _direction; }
        void setDirection(TriggerDir dir) {
            _direction = dir;
        }
        void cycleDirection() {
            _direction = advanceDirection(_direction, 1);
        }
        static TriggerDir advanceDirection(TriggerDir dir, int delta) {
            static constexpr TriggerDir order[] = {
                TriggerDir::Rise,
                TriggerDir::Fall,
                TriggerDir::Off,
                TriggerDir::Both,
            };
            int idx = 0;
            for (int i = 0; i < int(sizeof(order) / sizeof(order[0])); ++i) {
                if (order[i] == dir) {
                    idx = i;
                    break;
                }
            }
            int count = int(sizeof(order) / sizeof(order[0]));
            int next = (idx + delta) % count;
            if (next < 0) {
                next += count;
            }
            return order[next];
        }

        // Note index (-63 to +63)
        int8_t noteIndex() const { return _noteIndex; }
        void setNoteIndex(int8_t index) {
            _noteIndex = clamp(index, int8_t(-63), int8_t(63));
        }

        void clear();
        void write(VersionedSerializedWriter &writer) const;
        void read(VersionedSerializedReader &reader);

    private:
        int8_t _threshold = 0;
        TriggerDir _direction = TriggerDir::Off;
        int8_t _noteIndex = 0;
    };

    //----------------------------------------
    // Clock
    //----------------------------------------

    enum class ClockSource : uint8_t {
        Internal,           // Sawtooth ramp
        InternalTriangle,   // Triangle ramp
        External            // Routed CV input
    };

    enum class SyncMode : uint8_t {
        Off,
        ResetMeasure,
        External,
        Last
    };

    ClockSource clockSource() const { return _clockSource; }
    void setClockSource(ClockSource source) {
        _clockSource = source;
    }
    void toggleClockSource() {
        _clockSource = static_cast<ClockSource>((static_cast<int>(_clockSource) + 1) % 3);
    }

    SyncMode syncMode() const { return _syncMode; }
    void setSyncMode(SyncMode mode) {
        _syncMode = ModelUtils::clampedEnum(mode);
    }
    void cycleSyncMode() {
        auto next = static_cast<int>(_syncMode) + 1;
        if (next >= static_cast<int>(SyncMode::Last)) next = 0;
        setSyncMode(static_cast<SyncMode>(next));
    }
    void editSyncMode(int value, bool /*shift*/) {
        setSyncMode(static_cast<SyncMode>(clamp(int(_syncMode) + value, 0, int(SyncMode::Last) - 1)));
    }
    void printSyncMode(StringBuilder &str) const {
        switch (_syncMode) {
        case SyncMode::Off:          str("Off"); break;
        case SyncMode::ResetMeasure: str("Reset"); break;
        case SyncMode::External:     str("Ext"); break;
        case SyncMode::Last:         break;
        }
    }

    // Divisor (ticks)
    int divisor() const { return _divisor; }
    void setDivisor(int div) {
        _divisor = clamp(div, 1, 768);
    }

    // clockMultiplier
    int clockMultiplier() const { return _clockMultiplier.get(isRouted(Routing::Target::ClockMult)); }
    void setClockMultiplier(int clockMultiplier, bool routed = false) {
        _clockMultiplier.set(clamp(clockMultiplier, 50, 150), routed);
    }

    // Loop mode
    bool loop() const { return _loop; }
    void setLoop(bool loop) {
        _loop = loop;
    }
    void toggleLoop() {
        _loop = !_loop;
    }

    // Gate Length (0-100%)
    int gateLength() const { return _gateLength; }
    void setGateLength(int length) {
        _gateLength = clamp(length, 0, 100);
    }
    void editGateLength(int value, bool shift) {
        setGateLength(gateLength() + value);
    }
    void printGateLength(StringBuilder &str) const {
        if (_gateLength == 0) {
            str("T");
        } else {
            str("%d%%", gateLength());
        }
    }

    //----------------------------------------
    // Threshold
    //----------------------------------------

    enum class ThresholdMode : uint8_t {
        Position,   // Absolute voltage positions
        Length      // Proportional distribution
    };

    ThresholdMode thresholdMode() const { return _thresholdMode; }
    void setThresholdMode(ThresholdMode mode) {
        _thresholdMode = mode;
    }
    void toggleThresholdMode() {
        _thresholdMode = (_thresholdMode == ThresholdMode::Position)
            ? ThresholdMode::Length : ThresholdMode::Position;
    }

    //----------------------------------------
    // Scale
    //----------------------------------------

    // Track-scale selection (-1 = Default/Project, 0..N = Track Scale)
    int scale() const { return _scale; }
    void setScale(int scale) {
        _scale = clamp(scale, -1, Scale::Count - 1);
    }
    void editScale(int value, bool shift) {
        setScale(scale() + value);
    }
    void printScale(StringBuilder &str) const {
        if (scale() < 0) {
            str("Project");
        } else {
            str(Scale::name(scale()));
        }
    }

    // Root note (0-11: C-B)
    int rootNote() const { return _rootNote; }
    void setRootNote(int root) {
        _rootNote = clamp(root, 0, 11);
    }

    // Slew time (0-100%, 0 = off)
    int slewTime() const { return _slewTime.get(isRouted(Routing::Target::SlideTime)); }
    void setSlewTime(int time, bool routed = false) {
        _slewTime.set(clamp(time, 0, 100), routed);
    }
    void editSlewTime(int value, bool shift) {
        if (!isRouted(Routing::Target::SlideTime)) {
            setSlewTime(ModelUtils::adjustedByStep(slewTime(), value, 5, !shift));
        }
    }
    void printSlewTime(StringBuilder &str) const {
        int time = slewTime();
        if (time == 0) {
            str("Off");
        } else {
            str("%d%%", time);
        }
    }
    bool slewEnabled() const { return slewTime() > 0; }

    // Octave
    int octave() const { return _octave; }
    void setOctave(int octave) {
        _octave = clamp(octave, -10, 10);
    }

    // Transpose
    int transpose() const { return _transpose; }
    void setTranspose(int transpose) {
        _transpose = clamp(transpose, -60, 60);
    }

    // Offset (in centiv olts: -500 to +500 = -5.00V to +5.00V)
    int offset() const { return _offset; }
    void setOffset(int offset) {
        _offset = clamp(offset, -500, 500);
    }

    // Voltage Range (ABOVE/BELOW)
    float rangeHigh() const { return _rangeHigh; }
    void setRangeHigh(float v) {
        _rangeHigh = clamp(v, -5.0f, 5.0f);
    }

    float rangeLow() const { return _rangeLow; }
    void setRangeLow(float v) {
        _rangeLow = clamp(v, -5.0f, 5.0f);
    }

    // Range span with near-zero protection (Option C)
    float rangeSpan() const {
        float span = _rangeHigh - _rangeLow;
        // Allow inversion but protect against collapsed range
        if (std::abs(span) < 0.01f) {  // Minimum 10mV
            return (span >= 0) ? 0.01f : -0.01f;
        }
        return span;
    }

    // UI editing helpers (0.1V increments, shift = 1V)
    void editRangeHigh(int value, bool shift) {
        float delta = shift ? float(value) : (float(value) * 0.1f);
        setRangeHigh(_rangeHigh + delta);
    }

    void editRangeLow(int value, bool shift) {
        float delta = shift ? float(value) : (float(value) * 0.1f);
        setRangeLow(_rangeLow + delta);
    }

    void printRangeHigh(StringBuilder &str) const {
        str("%+.1fV", _rangeHigh);
    }

    void printRangeLow(StringBuilder &str) const {
        str("%+.1fV", _rangeLow);
    }

    //----------------------------------------
    // Stages
    //----------------------------------------

    Stage& stage(int index) {
        return _stages[clamp(index, 0, StageCount - 1)];
    }
    const Stage& stage(int index) const {
        return _stages[clamp(index, 0, StageCount - 1)];
    }

    //----------------------------------------
    // Routing
    //----------------------------------------

    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }
    void writeRouted(Routing::Target target, int intValue, float floatValue);

    //----------------------------------------
    // Methods
    //----------------------------------------

    void clear();
    void clearStage(int index);
    void clearThresholds();
    void clearNotes();
    void randomize();
    void randomizeThresholds();
    void randomizeNotes();
    void randomizeDirections();

    // resetMeasure (0-128 bars)
    int resetMeasure() const { return _resetMeasure; }
    void setResetMeasure(int resetMeasure) {
        _resetMeasure = clamp(resetMeasure, 0, 128);
    }
    void editResetMeasure(int value, bool shift) {
        setResetMeasure(ModelUtils::adjustedByPowerOfTwo(resetMeasure(), value, shift));
    }
    void printResetMeasure(StringBuilder &str) const {
        if (resetMeasure() == 0) {
            str("off");
        } else {
            str("%d %s", resetMeasure(), resetMeasure() > 1 ? "bars" : "bar");
        }
    }

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

    void setTrackIndex(int trackIndex) { _trackIndex = trackIndex; }
    const Scale &selectedScale(const Scale &projectScale) const {
        return scale() < 0 ? projectScale : Scale::get(scale());
    }

    // Editing helpers for list UI
    void editDivisor(int value, bool shift) {
        setDivisor(ModelUtils::adjustedByDivisor(divisor(), value, shift));
    }
    void printDivisor(StringBuilder &str) const { ModelUtils::printDivisor(str, divisor()); }
    void editClockMultiplier(int value, bool shift) {
        if (!isRouted(Routing::Target::ClockMult)) {
            setClockMultiplier(clockMultiplier() + value * (shift ? 10 : 1));
        }
    }
    void printClockMultiplier(StringBuilder &str) const {
        printRouted(str, Routing::Target::ClockMult);
        str("%.2fx", clockMultiplier() * 0.01f);
    }
    void printClockSource(StringBuilder &str) const {
        switch (_clockSource) {
        case ClockSource::Internal: str("Internal Saw"); break;
        case ClockSource::InternalTriangle: str("Internal Tri"); break;
        case ClockSource::External: str("External"); break;
        }
    }
    void printSyncModeShort(StringBuilder &str) const {
        switch (_syncMode) {
        case SyncMode::Off:          str("OFF"); break;
        case SyncMode::ResetMeasure: str("RM"); break;
        case SyncMode::External:     str("EXT"); break;
        case SyncMode::Last:         break;
        }
    }
    void printThresholdMode(StringBuilder &str) const { str(_thresholdMode == ThresholdMode::Position ? "Position" : "Length"); }
    void editRootNote(int value, bool shift) { setRootNote(rootNote() + value); }
    void printRootNote(StringBuilder &str) const { Types::printNote(str, rootNote()); }
    void printSlew(StringBuilder &str) const { printSlewTime(str); }
    void printLoop(StringBuilder &str) const { str(loop() ? "Loop" : "Once"); }

private:
    ClockSource _clockSource = ClockSource::Internal;
    SyncMode _syncMode = SyncMode::Off;
    uint16_t _divisor = 192;
    Routable<uint8_t> _clockMultiplier;
    uint8_t _gateLength = 0;     // 0 = 1T pulse
    bool _loop = true;
    uint8_t _resetMeasure = 8;   // default 8 bars

    ThresholdMode _thresholdMode = ThresholdMode::Position;

    int8_t _scale = -1;
    int8_t _rootNote = 0;       // C
    Routable<uint8_t> _slewTime;
    int8_t _octave = 0;
    int8_t _transpose = 0;
    int16_t _offset = 0;

    float _rangeHigh = 5.0f;    // Default +5V (Eurorack standard)
    float _rangeLow = -5.0f;    // Default -5V

    int _trackIndex = -1;
    std::array<Stage, StageCount> _stages;
};
