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

    static constexpr int StageCount = 8;

    //----------------------------------------
    // Stage
    //----------------------------------------

    class Stage {
    public:
        enum class TriggerDir : uint8_t {
            Rise,
            Fall,
            Off
        };

        // Threshold (-127 to +127)
        int8_t threshold() const { return _threshold; }
        void setThreshold(int threshold) {
            _threshold = clamp(threshold, -127, 127);
        }

        // Direction
        TriggerDir direction() const { return _direction; }
        void setDirection(TriggerDir dir) {
            _direction = dir;
        }

        // Note index (-63 to +64)
        int8_t noteIndex() const { return _noteIndex; }
        void setNoteIndex(int8_t index) {
            _noteIndex = clamp(index, int8_t(-63), int8_t(64));
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

    ClockSource clockSource() const { return _clockSource; }
    void setClockSource(ClockSource source) {
        _clockSource = source;
    }
    void toggleClockSource() {
        _clockSource = static_cast<ClockSource>((static_cast<int>(_clockSource) + 1) % 3);
    }

    // Divisor (ticks)
    int divisor() const { return _divisor; }
    void setDivisor(int div) {
        _divisor = clamp(div, 1, 768);
    }

    // Loop mode
    bool loop() const { return _loop; }
    void setLoop(bool loop) {
        _loop = loop;
    }
    void toggleLoop() {
        _loop = !_loop;
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

    enum class ScaleSource : uint8_t {
        Project,
        Track
    };

    ScaleSource scaleSource() const { return _scaleSource; }
    void setScaleSource(ScaleSource source) {
        _scaleSource = source;
    }

    // Track-scale selection (only used when ScaleSource::Track)
    int scale() const { return _scale; }
    void setScale(int scale) {
        _scale = clamp(scale, 0, Scale::Count - 1);
    }
    void editScale(int value, bool shift) {
        setScale(scale() + value);
    }
    void printScale(StringBuilder &str) const {
        str(Scale::name(scale()));
    }

    // Root note (0-11: C-B)
    int rootNote() const { return _rootNote; }
    void setRootNote(int root) {
        _rootNote = clamp(root, 0, 11);
    }

    // Slew
    bool slewEnabled() const { return _slewEnabled; }
    void setSlewEnabled(bool enabled) {
        _slewEnabled = enabled;
    }
    void toggleSlew() {
        _slewEnabled = !_slewEnabled;
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
    void writeRouted(Routing::Target target, int intValue, float floatValue);

    //----------------------------------------
    // Methods
    //----------------------------------------

    void clear();
    void clearStage(int index);

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

    void setTrackIndex(int trackIndex) { _trackIndex = trackIndex; }
    const Scale &selectedScale(const Scale &projectScale) const {
        return _scaleSource == ScaleSource::Project ? projectScale : Scale::get(_scale);
    }

    // Editing helpers for list UI
    void editDivisor(int value, bool shift) {
        setDivisor(ModelUtils::adjustedByDivisor(divisor(), value, shift));
    }
    void printDivisor(StringBuilder &str) const { ModelUtils::printDivisor(str, divisor()); }
    void printClockSource(StringBuilder &str) const {
        switch (_clockSource) {
        case ClockSource::Internal: str("Internal Saw"); break;
        case ClockSource::InternalTriangle: str("Internal Tri"); break;
        case ClockSource::External: str("External"); break;
        }
    }
    void printThresholdMode(StringBuilder &str) const { str(_thresholdMode == ThresholdMode::Position ? "Position" : "Length"); }
    void printScaleSource(StringBuilder &str) const { str(_scaleSource == ScaleSource::Project ? "Project" : "Track"); }
    void editRootNote(int value, bool shift) { setRootNote(rootNote() + value); }
    void printRootNote(StringBuilder &str) const { Types::printNote(str, rootNote()); }
    void printSlew(StringBuilder &str) const { str(slewEnabled() ? "On" : "Off"); }
    void printLoop(StringBuilder &str) const { str(loop() ? "Loop" : "Once"); }

private:
    ClockSource _clockSource = ClockSource::Internal;
    uint16_t _divisor = 192;
    bool _loop = true;

    ThresholdMode _thresholdMode = ThresholdMode::Position;

    ScaleSource _scaleSource = ScaleSource::Project;
    int8_t _scale = 0;
    int8_t _rootNote = 0;       // C
    bool _slewEnabled = false;

    int _trackIndex = -1;
    std::array<Stage, StageCount> _stages;
};
