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

class IndexedSequence {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    static constexpr int MaxSteps = 32;
    static constexpr int PatternCount = CONFIG_PATTERN_COUNT;  // 8 patterns

    //----------------------------------------
    // Step
    //----------------------------------------

    class Step {
    public:
        // Bit-packed step data (32 bits):
        // bits 0-6:   note_index (7 bits = 0-127, using 0-63 for Scale lookup)
        // bits 7-22:  duration (16 bits = direct tick count, 0-65535)
        // bits 23-31: gate_length (9 bits = 0-511, as % of duration)

        int8_t noteIndex() const {
            return static_cast<int8_t>(_packed & 0x7F);
        }

        void setNoteIndex(int8_t index) {
            index = clamp(index, int8_t(-63), int8_t(64));
            _packed = (_packed & ~0x7F) | (index & 0x7F);
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
            percentage = clamp(percentage, uint16_t(0), uint16_t(100));
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

    struct RouteConfig {
        uint8_t targetGroups = 0;       // Bitmask: 0b1010 = groups A+C
        ModTarget targetParam = ModTarget::Duration;
        float amount = 100.0f;          // Scale factor (-200% to +200%)
        bool enabled = false;

        void clear() {
            targetGroups = 0;
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

    // active length (dynamic step count)
    int activeLength() const { return _activeLength; }
    void setActiveLength(int length) {
        _activeLength = clamp(length, 1, MaxSteps);
    }

    // scale selection (-1 = Project scale, 0..N = Track scale)
    int scale() const { return _scale; }
    void setScale(int scale) {
        _scale = clamp(scale, -1, Scale::Count - 1);
    }

    // root note (0-11: C-B)
    int rootNote() const { return _rootNote; }
    void setRootNote(int root) {
        _rootNote = clamp(root, 0, 11);
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
    void setRouteA(const RouteConfig& cfg) { _routeA = cfg; }

    const RouteConfig& routeB() const { return _routeB; }
    void setRouteB(const RouteConfig& cfg) { _routeB = cfg; }

    //----------------------------------------
    // Methods
    //----------------------------------------

    void clear() {
        _divisor = 192;  // Quarter note at 192 PPQN
        _loop = true;
        _activeLength = 16;
        _scale = -1;  // Use project scale
        _rootNote = 0;
        _routeA.clear();
        _routeB.clear();

        // Initialize steps with sensible defaults
        for (auto &s : _steps) {
            s.clear();
            s.setDuration(192);      // Quarter note at 192 PPQN
            s.setGateLength(50);     // 50% gate length
            s.setNoteIndex(0);       // Root note
        }
    }

    void write(VersionedSerializedWriter &writer) const {
        writer.write(_divisor);
        writer.write(_loop);
        writer.write(_activeLength);
        writer.write(_scale);
        writer.write(_rootNote);

        _routeA.write(writer);
        _routeB.write(writer);

        for (const auto &s : _steps) {
            s.write(writer);
        }
    }

    void read(VersionedSerializedReader &reader) {
        reader.read(_divisor);
        reader.read(_loop);
        reader.read(_activeLength);
        reader.read(_scale);
        reader.read(_rootNote);

        _routeA.read(reader);
        _routeB.read(reader);

        for (auto &s : _steps) {
            s.read(reader);
        }
    }

    void writeRouted(Routing::Target target, int intValue, float floatValue);

    inline bool isRouted(Routing::Target target) const {
        return Routing::isRouted(target, _trackIndex);
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

    void editRootNote(int value, bool shift) {
        setRootNote(rootNote() + value);
    }

    void printRootNote(StringBuilder &str) const {
        Types::printNote(str, rootNote());
    }

    void printLoop(StringBuilder &str) const {
        str(loop() ? "Loop" : "Once");
    }

private:
    uint16_t _divisor = 192;      // Clock divisor in ticks
    bool _loop = true;            // Loop mode
    uint8_t _activeLength = 16;   // Dynamic step count (1-32)
    int8_t _scale = -1;           // Scale selection
    int8_t _rootNote = 0;         // Root note (C)
    int _trackIndex = -1;

    RouteConfig _routeA;
    RouteConfig _routeB;

    std::array<Step, MaxSteps> _steps;
};
