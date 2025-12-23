#pragma once

#include "Config.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Types.h"
#include "Scale.h"
#include "ProjectVersion.h"
#include "Routing.h"

#include "core/math/Math.h"
#include "core/utils/StringBuilder.h"

#include <array>
#include <algorithm>
#include <cstdint>

class IndexedSequence {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    static constexpr int MaxSteps = 32;
    static constexpr int PatternCount = CONFIG_PATTERN_COUNT;  // 8 patterns
    static constexpr uint8_t TargetGroupsAll = 0;
    static constexpr uint8_t TargetGroupsUngrouped = 0x10;
    static constexpr uint8_t TargetGroupsSelected = 0x20;
    static constexpr uint16_t GateLengthTrigger = 101; // Special: fixed short trigger pulse

    //----------------------------------------
    // Step
    //----------------------------------------

    class Step {
    public:
        // Bit-packed step data (32 bits):
        // bits 0-6:   note_index (7 bits, signed -63..64 encoded in 7 bits)
        // bits 7-22:  duration (16 bits = direct tick count, 0-65535)
        // bits 23-31: gate_length (9 bits = 0-511, 0-100% or GateLengthTrigger)

        int8_t noteIndex() const {
            uint8_t raw = static_cast<uint8_t>(_packed & 0x7F);
            if (raw == 64) {
                return 64;
            }
            if (raw > 64) {
                return static_cast<int8_t>(int(raw) - 128);
            }
            return static_cast<int8_t>(raw);
        }

        void setNoteIndex(int8_t index) {
            index = clamp(index, int8_t(-63), int8_t(64));
            uint8_t raw = index < 0 ? (static_cast<uint8_t>(index) & 0x7F) : static_cast<uint8_t>(index);
            _packed = (_packed & ~0x7F) | raw;
        }

        uint16_t duration() const {
            return static_cast<uint16_t>((_packed >> 7) & 0xFFFF);
        }

        void setDuration(uint16_t ticks) {
            _packed = (_packed & ~(0xFFFF << 7)) | ((ticks & 0xFFFF) << 7);
        }

        uint16_t gateLength() const {
            return static_cast<uint16_t>((_packed >> 23) & 0x1FF);
        }

        void setGateLength(uint16_t percentage) {
            percentage = clamp(percentage, uint16_t(0), uint16_t(GateLengthTrigger));
            _packed = (_packed & ~(0x1FF << 23)) | ((percentage & 0x1FF) << 23);
        }

        // Group mask (4 bits for groups A-D)
        uint8_t groupMask() const {
            return _groupMask;
        }

        void setGroupMask(uint8_t mask) {
            _groupMask = mask & 0x0F;  // Only 4 bits
        }

        void toggleGroup(int groupIndex) {
            _groupMask ^= (1 << (groupIndex & 0x3));
        }

        void clear() {
            _packed = 0;
            _groupMask = 0;
        }

        void write(VersionedSerializedWriter &writer) const {
            writer.write(_packed);
            writer.write(_groupMask);
        }

        void read(VersionedSerializedReader &reader) {
            reader.read(_packed);
            reader.read(_groupMask);
        }

    private:
        uint32_t _packed = 0;      // Bit-packed: note(7) + duration(16) + gate(9)
        uint8_t _groupMask = 0;    // Groups A-D (bits 0-3)
    };

    //----------------------------------------
    // Route Configuration
    //----------------------------------------

    enum class ModTarget : uint8_t {
        Duration,      // Modulate step duration (additive)
        GateLength,    // Modulate gate % (additive)
        NoteIndex,     // Transpose note index (additive)
        Last
    };

    enum class RouteCombineMode : uint8_t {
        AtoB,
        Mux,
        Min,
        Max,
        Last
    };

    enum class SyncMode : uint8_t {
        Off,
        ResetMeasure,
        External,
        Last
    };

    struct RouteConfig {
        uint8_t targetGroups = TargetGroupsAll; // Bitmask: 0b1010 = groups A+C (0 = ALL, 0x10 = UNGR)
        ModTarget targetParam = ModTarget::Duration;
        float amount = 100.0f;          // Scale factor (-200% to +200%)
        bool enabled = false;

        void clear() {
            targetGroups = TargetGroupsAll;
            targetParam = ModTarget::Duration;
            amount = 100.0f;
            enabled = false;
        }

        void write(VersionedSerializedWriter &writer) const {
            writer.write(targetGroups);
            writer.write(static_cast<uint8_t>(targetParam));
            writer.write(amount);
            writer.write(enabled);
        }

        void read(VersionedSerializedReader &reader) {
            reader.read(targetGroups);
            uint8_t param;
            reader.read(param);
            targetParam = static_cast<ModTarget>(param);
            reader.read(amount);
            reader.read(enabled);
        }
    };

    //----------------------------------------
    // Routing Helper
    //----------------------------------------

    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }
    void writeRouted(Routing::Target target, int intValue, float floatValue);

    //----------------------------------------
    // Sequence Properties
    //----------------------------------------

    // divisor (clock ticks)
    int divisor() const { return _divisor; }
    void setDivisor(int div) {
        _divisor = clamp(div, 1, 768);
    }

    // loop mode
    bool loop() const { return _loop; }
    void setLoop(bool loop) { _loop = loop; }
    void toggleLoop() { _loop = !_loop; }

    // run mode
    Types::RunMode runMode() const { return _runMode.get(isRouted(Routing::Target::RunMode)); }
    void setRunMode(Types::RunMode runMode, bool routed = false) {
        _runMode.set(ModelUtils::clampedEnum(runMode), routed);
    }
    void editRunMode(int value, bool /*shift*/) {
        if (!isRouted(Routing::Target::RunMode)) {
            setRunMode(ModelUtils::adjustedEnum(runMode(), value));
        }
    }
    void printRunMode(StringBuilder &str) const {
        printRouted(str, Routing::Target::RunMode);
        str(Types::runModeName(runMode()));
    }

    // active length (dynamic step count)
    int activeLength() const { return _activeLength; }
    void setActiveLength(int length) {
        _activeLength = clamp(length, 1, MaxSteps);
        // Clamp first step to new length
        if (firstStep() >= _activeLength) {
            setFirstStep(_activeLength - 1);
        }
    }

    // scale selection (-1 = Project scale, 0..N = Track scale)
    int scale() const { return _scale; }
    void setScale(int scale) {
        _scale = clamp(scale, -1, Scale::Count - 1);
    }

    // syncMode
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

    // root note (0-11: C-B)
    int rootNote() const { return _rootNote.get(isRouted(Routing::Target::RootNote)); }
    void setRootNote(int rootNote, bool routed = false) {
        _rootNote.set(clamp(rootNote, -1, 11), routed);
    }

    void editRootNote(int value, bool shift) {
        if (!isRouted(Routing::Target::RootNote)) {
            setRootNote(rootNote() + value);
        }
    }

    void printRootNote(StringBuilder &str) const {
        printRouted(str, Routing::Target::RootNote);
        if (rootNote() < 0) {
            str("Default");
        } else {
            Types::printNote(str, rootNote());
        }
    }

    // firstStep (Rotation Offset)

    int firstStep() const {
        return _firstStep.get(isRouted(Routing::Target::FirstStep));
    }

    void setFirstStep(int firstStep, bool routed = false) {
        _firstStep.set(clamp(firstStep, 0, _activeLength - 1), routed);
    }

    void editFirstStep(int value, bool shift) {
        if (!isRouted(Routing::Target::FirstStep)) {
            setFirstStep(firstStep() + value);
        }
    }

    void printFirstStep(StringBuilder &str) const {
        printRouted(str, Routing::Target::FirstStep);
        str("%d", firstStep() + 1);
    }

    // resetMeasure (0 = off, >0 bars)
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

    // Steps
    Step& step(int index) {
        return _steps[clamp(index, 0, MaxSteps - 1)];
    }

    const Step& step(int index) const {
        return _steps[clamp(index, 0, MaxSteps - 1)];
    }

    // Route configuration
    const RouteConfig& routeA() const { return _routeA; }
    RouteConfig& routeA() { return _routeA; }
    void setRouteA(const RouteConfig& cfg) { _routeA = cfg; }
    float routedIndexedA() const { return _routedIndexedA; }

    const RouteConfig& routeB() const { return _routeB; }
    RouteConfig& routeB() { return _routeB; }
    void setRouteB(const RouteConfig& cfg) { _routeB = cfg; }
    float routedIndexedB() const { return _routedIndexedB; }

    RouteCombineMode routeCombineMode() const { return _routeCombineMode; }
    void setRouteCombineMode(RouteCombineMode mode) { _routeCombineMode = ModelUtils::clampedEnum(mode); }

    //----------------------------------------
    // Methods
    //----------------------------------------

    // Split step at specified index into two steps
    // Current step becomes first half (ceil duration), new step at index+1 becomes second half (floor duration)
    void splitStep(int index) {
        if (_activeLength >= MaxSteps) return; // Can't exceed max steps

        index = clamp(index, 0, int(_activeLength) - 1);
        auto &currentStep = _steps[index];
        uint16_t totalDuration = currentStep.duration();

        // Calculate durations
        // First step gets ceil(total / 2) -> (total + 1) / 2
        // Second step gets floor(total / 2) -> total / 2
        uint16_t duration1 = (totalDuration + 1) / 2;
        uint16_t duration2 = totalDuration / 2;

        // Shift steps to the right from insertion point (index + 1)
        for (int i = _activeLength; i > index + 1; i--) {
            _steps[i] = _steps[i - 1];
        }

        // Update current step (first half)
        currentStep.setDuration(duration1);

        // Initialize new step (second half) at index + 1
        // It inherits all properties from the original step except duration
        _steps[index + 1] = currentStep;
        _steps[index + 1].setDuration(duration2);

        _activeLength++;
    }

    // Insert a new step at the specified index
    // Shifts steps to the right, clones previous step's data
    // Automatically increments activeLength
    void insertStep(int index) {
        if (_activeLength >= MaxSteps) return;  // Can't exceed 32 steps

        index = clamp(index, 0, int(_activeLength));

        // Shift steps to the right from insertion point
        for (int i = _activeLength; i > index; i--) {
            _steps[i] = _steps[i - 1];
        }

        // Initialize new step
        if (index == _activeLength) {
            if (index > 0) {
                // Appending: Clone previous step
                _steps[index] = _steps[index - 1];
            } else {
                // First step ever: Default initialization
                _steps[index].clear();
                _steps[index].setDuration(192);      // Quarter note
                _steps[index].setGateLength(50);     // 50%
                _steps[index].setNoteIndex(0);       // Root
            }
        }
        // Else: Inserting in middle. The shift loop above (_steps[i] = _steps[i-1])
        // effectively duplicated the step at 'index' into 'index+1', leaving 'index'
        // as the "clone" of the original step. No further action needed.

        _activeLength++;
    }

    // Delete step at the specified index
    // Shifts steps to the left
    // Automatically decrements activeLength
    void deleteStep(int index) {
        if (_activeLength <= 1) return;  // Must have at least 1 step
        if (index >= _activeLength) return;  // Can't delete beyond active range

        index = clamp(index, 0, int(_activeLength) - 1);

        // Shift steps to the left from deletion point
        for (int i = index; i < _activeLength - 1; i++) {
            _steps[i] = _steps[i + 1];
        }

        _activeLength--;

        // Clear the now-unused last step (cleanup)
        _steps[_activeLength].clear();
    }

    // Check if insert is possible at current length
    bool canInsert() const { return _activeLength < MaxSteps; }

    // Check if delete is possible
    bool canDelete() const { return _activeLength > 1; }

    void clear() {
        _divisor = 12;  // 1/16 note at 192 PPQN
        _loop = true;
        _activeLength = 3;
        _scale = -1;  // Use project scale
        _runMode.clear();
        _rootNote.clear();
        _firstStep.clear();
        setRunMode(Types::RunMode::Forward);
        _syncMode = SyncMode::Off;
        _resetMeasure = 0;
        _routeA.clear();
        _routeB.clear();
        _routeCombineMode = RouteCombineMode::AtoB;

        // Initialize steps with sensible defaults
        for (int i = 0; i < MaxSteps; ++i) {
            auto &s = _steps[i];
            s.clear();
            if (i < 3) {
                s.setDuration(256);      // 3 steps over 4 bars (768 ticks total)
                s.setGateLength(10);     // 10%
                s.setNoteIndex(0);       // Root note
            } else {
                s.setDuration(0);        // Silent/skip by default
                s.setGateLength(0);
                s.setNoteIndex(0);
            }
        }
    }

    void write(VersionedSerializedWriter &writer) const {
        writer.write(_divisor);
        writer.write(_loop);
        _runMode.write(writer);
        writer.write(_activeLength);
        writer.write(_scale);
        writer.write(_rootNote); // Now writes Routable base
        writer.write(static_cast<uint8_t>(_syncMode));
        writer.write(_resetMeasure);
        _firstStep.write(writer);

        _routeA.write(writer);
        _routeB.write(writer);
        writer.write(static_cast<uint8_t>(_routeCombineMode));

        for (const auto &s : _steps) {
            s.write(writer);
        }
    }

    void read(VersionedSerializedReader &reader) {
        reader.read(_divisor);
        reader.read(_loop);
        _runMode.read(reader);
        reader.read(_activeLength);
        reader.read(_scale);
        reader.read(_rootNote);

        uint8_t sync;
        reader.read(sync);
        _syncMode = ModelUtils::clampedEnum(static_cast<SyncMode>(sync));

        reader.read(_resetMeasure);

        _firstStep.read(reader);

        _routeA.read(reader);
        _routeB.read(reader);

        uint8_t mode;
        reader.read(mode);
        _routeCombineMode = ModelUtils::clampedEnum(static_cast<RouteCombineMode>(mode));

        for (auto &s : _steps) {
            s.read(reader);
        }
    }
    void setTrackIndex(int trackIndex) { _trackIndex = trackIndex; }

    const Scale &selectedScale(const Scale &projectScale) const {
        return scale() < 0 ? projectScale : Scale::get(scale());
    }

    // UI helper methods
    void editDivisor(int value, bool shift) {
        setDivisor(ModelUtils::adjustedByDivisor(divisor(), value, shift));
    }

    void printDivisor(StringBuilder &str) const {
        ModelUtils::printDivisor(str, divisor());
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

    // editRootNote/printRootNote removed from here to avoid duplication

    void printLoop(StringBuilder &str) const {
        str(loop() ? "Loop" : "Once");
    }

private:
    uint16_t _divisor = 192;      // Clock divisor in ticks
    bool _loop = true;            // Loop mode
    Routable<Types::RunMode> _runMode;
    uint8_t _activeLength = 16;   // Dynamic step count (1-32)
    int8_t _scale = -1;           // Scale selection
    Routable<int8_t> _rootNote;   // Root note (C), now Routable
    Routable<uint8_t> _firstStep; // Rotation offset (0-31)
    SyncMode _syncMode = SyncMode::Off;
    uint8_t _resetMeasure = 0;    // Bars (0 = off)
    int _trackIndex = -1;

    RouteConfig _routeA;
    RouteConfig _routeB;
    RouteCombineMode _routeCombineMode = RouteCombineMode::AtoB;
    float _routedIndexedA = 0.f;  // Routed CV value for route A
    float _routedIndexedB = 0.f;  // Routed CV value for route B

    std::array<Step, MaxSteps> _steps;
};
