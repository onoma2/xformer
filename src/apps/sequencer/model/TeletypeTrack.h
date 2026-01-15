#pragma once

#include "Config.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Types.h"
#include "model/Scale.h"
#include "MidiConfig.h"

#include "core/utils/StringBuilder.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
#include "state.h"
}

class TeletypeTrack {
public:
    static constexpr int TriggerInputCount = 4;
    static constexpr int TriggerOutputCount = 4;
    static constexpr int CvOutputCount = 4;
    static constexpr int8_t QuantizeOff = -2;
    static constexpr int8_t QuantizeDefault = -1;
    static constexpr int ScriptSlotCount = 4;
    static constexpr int EditableScriptCount = EDITABLE_SCRIPT_COUNT;
    static constexpr int ScriptLineCount = SCRIPT_MAX_COMMANDS;
    static constexpr int ScriptLineLength = 96;
    static constexpr int PatternSlotCount = 2;
    static constexpr int SlotScriptIndex = 3;
    static_assert(SlotScriptIndex < EditableScriptCount, "SlotScriptIndex out of range");
    enum class TimeBase : uint8_t {
        Ms,
        Clock,
        Last
    };

    //----------------------------------------
    // I/O Mapping Enums
    //----------------------------------------

    // TI-TR1 to TI-TR4: Trigger input sources (what triggers each script)
    enum class TriggerInputSource : uint8_t {
        None,
        CvIn1,      // CV input 1 (gate threshold > 1V)
        CvIn2,      // CV input 2
        CvIn3,      // CV input 3
        CvIn4,      // CV input 4
        GateOut1,   // Gate output 1 (read back)
        GateOut2,   // Gate output 2
        GateOut3,   // Gate output 3
        GateOut4,   // Gate output 4
        GateOut5,   // Gate output 5
        GateOut6,   // Gate output 6
        GateOut7,   // Gate output 7
        GateOut8,   // Gate output 8
        LogicalGate1, // Track 1 gate (pre-layout)
        LogicalGate2,
        LogicalGate3,
        LogicalGate4,
        LogicalGate5,
        LogicalGate6,
        LogicalGate7,
        LogicalGate8,
        Last
    };

    // TI-IN, TI-PARAM: CV input sources
    enum class CvInputSource : uint8_t {
        CvIn1,      // Physical CV input 1
        CvIn2,      // Physical CV input 2
        CvIn3,      // Physical CV input 3
        CvIn4,      // Physical CV input 4
        CvOut1,     // Physical CV output 1 (read back)
        CvOut2,     // Physical CV output 2
        CvOut3,     // Physical CV output 3
        CvOut4,     // Physical CV output 4
        CvOut5,     // Physical CV output 5
        CvOut6,     // Physical CV output 6
        CvOut7,     // Physical CV output 7
        CvOut8,     // Physical CV output 8
        LogicalCv1, // Track 1 CV (pre-layout)
        LogicalCv2,
        LogicalCv3,
        LogicalCv4,
        LogicalCv5,
        LogicalCv6,
        LogicalCv7,
        LogicalCv8,
        None,
        Last
    };

    // TO-TRA to TO-TRD: Trigger output destinations
    enum class TriggerOutputDest : uint8_t {
        GateOut1,   // Gate output 1
        GateOut2,   // Gate output 2
        GateOut3,   // Gate output 3
        GateOut4,   // Gate output 4
        GateOut5,   // Gate output 5
        GateOut6,   // Gate output 6
        GateOut7,   // Gate output 7
        GateOut8,   // Gate output 8
        Last
    };

    // TO-CV1 to TO-CV4: CV output destinations
    enum class CvOutputDest : uint8_t {
        CvOut1,     // CV output 1
        CvOut2,     // CV output 2
        CvOut3,     // CV output 3
        CvOut4,     // CV output 4
        CvOut5,     // CV output 5
        CvOut6,     // CV output 6
        CvOut7,     // CV output 7
        CvOut8,     // CV output 8
        Last
    };

    struct PatternSlot {
        std::array<tele_command_t, ScriptLineCount> slotScript{};
        std::array<tele_command_t, ScriptLineCount> metro{};
        uint8_t slotScriptLength = 0;
        uint8_t metroLength = 0;
        std::array<scene_pattern_t, PATTERN_COUNT> patterns{};
        std::array<TriggerInputSource, TriggerInputCount> triggerInputSource{};
        CvInputSource cvInSource = CvInputSource::CvIn1;
        CvInputSource cvParamSource = CvInputSource::CvIn2;
        CvInputSource cvXSource = CvInputSource::None;
        CvInputSource cvYSource = CvInputSource::None;
        CvInputSource cvZSource = CvInputSource::None;
        std::array<TriggerOutputDest, TriggerOutputCount> triggerOutputDest{};
        std::array<CvOutputDest, CvOutputCount> cvOutputDest{};
        std::array<Types::VoltageRange, CvOutputCount> cvOutputRange{};
        std::array<int16_t, CvOutputCount> cvOutputOffset{};
        std::array<int8_t, CvOutputCount> cvOutputQuantizeScale{};
        std::array<int8_t, CvOutputCount> cvOutputRootNote{};
        MidiSourceConfig midiSource{};
        uint8_t bootScriptIndex = 0;
        TimeBase timeBase = TimeBase::Ms;
        int16_t clockDivisor = 12;
        int16_t clockMultiplier = 100;
        bool resetMetroOnLoad = true;
    };

    TeletypeTrack() { clear(); }

    void clear();

    void gateOutputName(int index, StringBuilder &str) const;
    void cvOutputName(int index, StringBuilder &str) const;
    void seedOutputDestsFromTrackIndex(int trackIndex);

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

    scene_state_t &state() { return _state; }
    const scene_state_t &state() const { return _state; }

    // Active slot helper
    PatternSlot &activeSlot() { return _patternSlots[_activePatternSlot]; }
    const PatternSlot &activeSlot() const { return _patternSlots[_activePatternSlot]; }

    // midiSource
    const MidiSourceConfig &midiSource() const { return activeSlot().midiSource; }
          MidiSourceConfig &midiSource()       { return activeSlot().midiSource; }

    //----------------------------------------
    // Timing (time base + divisor/multiplier)
    //----------------------------------------

    TimeBase timeBase() const { return activeSlot().timeBase; }
    void setTimeBase(TimeBase mode) {
        activeSlot().timeBase = ModelUtils::clampedEnum(mode);
    }
    void editTimeBase(int value, bool shift) {
        (void)shift;
        setTimeBase(ModelUtils::adjustedEnum(activeSlot().timeBase, value));
    }
    void printTimeBase(StringBuilder &str) const {
        str(timeBaseName(activeSlot().timeBase));
    }

    int clockDivisor() const { return activeSlot().clockDivisor; }
    void setClockDivisor(int divisor) {
        activeSlot().clockDivisor = ModelUtils::clampDivisor(divisor);
    }
    void editClockDivisor(int value, bool shift) {
        setClockDivisor(ModelUtils::adjustedByDivisor(activeSlot().clockDivisor, value, shift));
    }
    void printClockDivisor(StringBuilder &str) const {
        ModelUtils::printDivisor(str, activeSlot().clockDivisor);
    }

    int clockMultiplier() const { return activeSlot().clockMultiplier; }
    void setClockMultiplier(int multiplier) {
        activeSlot().clockMultiplier = clamp(multiplier, 50, 150);
    }
    void editClockMultiplier(int value, bool shift) {
        setClockMultiplier(activeSlot().clockMultiplier + value * (shift ? 10 : 1));
    }
    void printClockMultiplier(StringBuilder &str) const {
        str("%.2fx", activeSlot().clockMultiplier * 0.01f);
    }

    //----------------------------------------
    // I/O Mapping Accessors
    //----------------------------------------

    // TI-TR1 to TI-TR4 (4 trigger inputs)
    TriggerInputSource triggerInputSource(int index) const {
        if (index < 0 || index >= TriggerInputCount) return TriggerInputSource::None;
        return activeSlot().triggerInputSource[index];
    }
    void setTriggerInputSource(int index, TriggerInputSource source) {
        if (index >= 0 && index < TriggerInputCount) {
            activeSlot().triggerInputSource[index] = ModelUtils::clampedEnum(source);
        }
    }
    void editTriggerInputSource(int index, int value, bool shift) {
        if (index >= 0 && index < TriggerInputCount) {
            setTriggerInputSource(index, ModelUtils::adjustedEnum(activeSlot().triggerInputSource[index], value));
        }
    }
    void printTriggerInputSource(int index, StringBuilder &str) const {
        if (index >= 0 && index < TriggerInputCount) {
            str(triggerInputSourceName(activeSlot().triggerInputSource[index]));
        }
    }

    // TI-IN (Teletype IN variable)
    CvInputSource cvInSource() const { return activeSlot().cvInSource; }
    void setCvInSource(CvInputSource source) {
        activeSlot().cvInSource = ModelUtils::clampedEnum(source);
    }
    void editCvInSource(int value, bool shift) {
        setCvInSource(ModelUtils::adjustedEnum(activeSlot().cvInSource, value));
    }
    void printCvInSource(StringBuilder &str) const {
        str(cvInputSourceName(activeSlot().cvInSource));
    }

    // TI-PARAM (Teletype PARAM variable)
    CvInputSource cvParamSource() const { return activeSlot().cvParamSource; }
    void setCvParamSource(CvInputSource source) {
        activeSlot().cvParamSource = ModelUtils::clampedEnum(source);
    }
    void editCvParamSource(int value, bool shift) {
        setCvParamSource(ModelUtils::adjustedEnum(activeSlot().cvParamSource, value));
    }
    void printCvParamSource(StringBuilder &str) const {
        str(cvInputSourceName(activeSlot().cvParamSource));
    }

    // TI-X, TI-Y, TI-Z: CV input sources for variables
    CvInputSource cvXSource() const { return activeSlot().cvXSource; }
    void setCvXSource(CvInputSource source) {
        activeSlot().cvXSource = ModelUtils::clampedEnum(source);
    }
    void editCvXSource(int value, bool shift) {
        (void)shift;
        setCvXSource(ModelUtils::adjustedEnum(activeSlot().cvXSource, value));
    }
    void printCvXSource(StringBuilder &str) const {
        str(cvInputSourceName(activeSlot().cvXSource));
    }

    CvInputSource cvYSource() const { return activeSlot().cvYSource; }
    void setCvYSource(CvInputSource source) {
        activeSlot().cvYSource = ModelUtils::clampedEnum(source);
    }
    void editCvYSource(int value, bool shift) {
        (void)shift;
        setCvYSource(ModelUtils::adjustedEnum(activeSlot().cvYSource, value));
    }
    void printCvYSource(StringBuilder &str) const {
        str(cvInputSourceName(activeSlot().cvYSource));
    }

    CvInputSource cvZSource() const { return activeSlot().cvZSource; }
    void setCvZSource(CvInputSource source) {
        activeSlot().cvZSource = ModelUtils::clampedEnum(source);
    }
    void editCvZSource(int value, bool shift) {
        (void)shift;
        setCvZSource(ModelUtils::adjustedEnum(activeSlot().cvZSource, value));
    }
    void printCvZSource(StringBuilder &str) const {
        str(cvInputSourceName(activeSlot().cvZSource));
    }

    // TO-TRA to TO-TRD (4 trigger outputs)
    TriggerOutputDest triggerOutputDest(int index) const {
        if (index < 0 || index >= TriggerOutputCount) return TriggerOutputDest::GateOut1;
        return activeSlot().triggerOutputDest[index];
    }
    void setTriggerOutputDest(int index, TriggerOutputDest dest) {
        if (index >= 0 && index < TriggerOutputCount) {
            activeSlot().triggerOutputDest[index] = ModelUtils::clampedEnum(dest);
        }
    }
    void editTriggerOutputDest(int index, int value, bool shift) {
        if (index >= 0 && index < TriggerOutputCount) {
            setTriggerOutputDest(index, ModelUtils::adjustedEnum(activeSlot().triggerOutputDest[index], value));
        }
    }
    void printTriggerOutputDest(int index, StringBuilder &str) const {
        if (index >= 0 && index < TriggerOutputCount) {
            str(triggerOutputDestName(activeSlot().triggerOutputDest[index]));
        }
    }

    // TO-CV1 to TO-CV4 (4 CV outputs)
    CvOutputDest cvOutputDest(int index) const {
        if (index < 0 || index >= CvOutputCount) return CvOutputDest::CvOut1;
        return activeSlot().cvOutputDest[index];
    }
    void setCvOutputDest(int index, CvOutputDest dest) {
        if (index >= 0 && index < CvOutputCount) {
            activeSlot().cvOutputDest[index] = ModelUtils::clampedEnum(dest);
        }
    }
    void editCvOutputDest(int index, int value, bool shift) {
        if (index >= 0 && index < CvOutputCount) {
            setCvOutputDest(index, ModelUtils::adjustedEnum(activeSlot().cvOutputDest[index], value));
        }
    }
    void printCvOutputDest(int index, StringBuilder &str) const {
        if (index >= 0 && index < CvOutputCount) {
            str(cvOutputDestName(activeSlot().cvOutputDest[index]));
        }
    }

    // Per-output range/offset
    Types::VoltageRange cvOutputRange(int index) const {
        if (index < 0 || index >= CvOutputCount) return Types::VoltageRange::Bipolar5V;
        return activeSlot().cvOutputRange[index];
    }
    void setCvOutputRange(int index, Types::VoltageRange range) {
        if (index >= 0 && index < CvOutputCount) {
            activeSlot().cvOutputRange[index] = ModelUtils::clampedEnum(range);
        }
    }
    void editCvOutputRange(int index, int value, bool shift) {
        (void)shift;
        if (index >= 0 && index < CvOutputCount) {
            setCvOutputRange(index, ModelUtils::adjustedEnum(activeSlot().cvOutputRange[index], value));
        }
    }
    void printCvOutputRange(int index, StringBuilder &str) const {
        if (index >= 0 && index < CvOutputCount) {
            str(Types::voltageRangeName(activeSlot().cvOutputRange[index]));
        }
    }

    int cvOutputOffset(int index) const {
        if (index < 0 || index >= CvOutputCount) return 0;
        return activeSlot().cvOutputOffset[index];
    }
    float cvOutputOffsetVolts(int index) const {
        return cvOutputOffset(index) * 0.01f;
    }
    void setCvOutputOffset(int index, int offset) {
        if (index >= 0 && index < CvOutputCount) {
            activeSlot().cvOutputOffset[index] = clamp(offset, -500, 500);
        }
    }
    void editCvOutputOffset(int index, int value, bool shift) {
        if (index >= 0 && index < CvOutputCount) {
            setCvOutputOffset(index, activeSlot().cvOutputOffset[index] + value * (shift ? 100 : 1));
        }
    }
    void printCvOutputOffset(int index, StringBuilder &str) const {
        if (index >= 0 && index < CvOutputCount) {
            str("%+.2fV", cvOutputOffsetVolts(index));
        }
    }

    // Per-output quantize scale (Off/Default/Scale)
    int cvOutputQuantizeScale(int index) const {
        if (index < 0 || index >= CvOutputCount) return QuantizeOff;
        return activeSlot().cvOutputQuantizeScale[index];
    }
    void setCvOutputQuantizeScale(int index, int scale) {
        if (index >= 0 && index < CvOutputCount) {
            activeSlot().cvOutputQuantizeScale[index] = clamp<int8_t>(scale, QuantizeOff, Scale::Count - 1);
        }
    }
    void editCvOutputQuantizeScale(int index, int value, bool shift) {
        (void)shift;
        if (index >= 0 && index < CvOutputCount) {
            setCvOutputQuantizeScale(index, activeSlot().cvOutputQuantizeScale[index] + value);
        }
    }
    void printCvOutputQuantizeScale(int index, StringBuilder &str) const {
        if (index < 0 || index >= CvOutputCount) return;
        int scale = activeSlot().cvOutputQuantizeScale[index];
        if (scale == QuantizeOff) {
            str("Off");
        } else if (scale == QuantizeDefault) {
            str("Default");
        } else {
            str(Scale::name(scale));
        }
    }

    // Per-output root note (Default or 0-11)
    int cvOutputRootNote(int index) const {
        if (index < 0 || index >= CvOutputCount) return -1;
        return activeSlot().cvOutputRootNote[index];
    }
    void setCvOutputRootNote(int index, int note) {
        if (index >= 0 && index < CvOutputCount) {
            activeSlot().cvOutputRootNote[index] = clamp<int8_t>(note, -1, 11);
        }
    }
    void editCvOutputRootNote(int index, int value, bool shift) {
        (void)shift;
        if (index >= 0 && index < CvOutputCount) {
            setCvOutputRootNote(index, activeSlot().cvOutputRootNote[index] + value);
        }
    }
    void printCvOutputRootNote(int index, StringBuilder &str) const {
        if (index < 0 || index >= CvOutputCount) return;
        int note = activeSlot().cvOutputRootNote[index];
        if (note < 0) {
            str("Default");
        } else {
            Types::printNote(str, note);
        }
    }

    //----------------------------------------
    // Conflict Detection
    //----------------------------------------

    // Check if a CV output is already used as CV OUTPUT destination
    bool isCvOutputUsedAsOutput(int cvOutIndex) const {
        for (int i = 0; i < CvOutputCount; ++i) {
            if (int(activeSlot().cvOutputDest[i]) == cvOutIndex) {
                return true;
            }
        }
        return false;
    }

    // Check if a CV output is already used as CV INPUT source (TI-IN or TI-PARAM)
    bool isCvOutputUsedAsInput(int cvOutIndex) const {
        const auto &slot = activeSlot();
        // Check TI-IN
        if (slot.cvInSource >= CvInputSource::CvOut1 &&
            slot.cvInSource <= CvInputSource::CvOut8) {
            int srcCv = int(slot.cvInSource) - int(CvInputSource::CvOut1);
            if (srcCv == cvOutIndex) {
                return true;
            }
        }

        // Check TI-PARAM
        if (slot.cvParamSource >= CvInputSource::CvOut1 &&
            slot.cvParamSource <= CvInputSource::CvOut8) {
            int srcCv = int(slot.cvParamSource) - int(CvInputSource::CvOut1);
            if (srcCv == cvOutIndex) {
                return true;
            }
        }

        return false;
    }

    // Check if a gate output is already used as trigger OUTPUT
    bool isGateOutputUsedAsOutput(int gateIndex) const {
        for (int i = 0; i < TriggerOutputCount; ++i) {
            if (int(activeSlot().triggerOutputDest[i]) == gateIndex) {
                return true;
            }
        }
        return false;
    }

    // Check if a gate output is already used as trigger INPUT
    bool isGateOutputUsedAsInput(int gateIndex) const {
        for (int i = 0; i < TriggerInputCount; ++i) {
            auto src = activeSlot().triggerInputSource[i];
            if (src >= TriggerInputSource::GateOut1 &&
                src <= TriggerInputSource::GateOut8) {
                int srcGate = int(src) - int(TriggerInputSource::GateOut1);
                if (srcGate == gateIndex) {
                    return true;
                }
            }
        }
        return false;
    }

    //----------------------------------------
    // UI Helpers - Get Available Options (excluding conflicts)
    //----------------------------------------

    // Get valid trigger input sources (TI-TR1-4)
    std::vector<TriggerInputSource> getAvailableTriggerInputSources() const {
        std::vector<TriggerInputSource> available;

        for (int i = 0; i < int(TriggerInputSource::Last); ++i) {
            auto src = TriggerInputSource(i);

            // If it's a gate output, check if it's used as OUTPUT
            if (src >= TriggerInputSource::GateOut1 &&
                src <= TriggerInputSource::GateOut8) {
                int gateIdx = i - int(TriggerInputSource::GateOut1);
                if (isGateOutputUsedAsOutput(gateIdx)) {
                    continue;  // Skip - conflict!
                }
            }

            available.push_back(src);
        }

        return available;
    }

    // Get valid CV input sources (TI-IN, TI-PARAM)
    std::vector<CvInputSource> getAvailableCvInputSources() const {
        std::vector<CvInputSource> available;

        for (int i = 0; i < int(CvInputSource::Last); ++i) {
            auto src = CvInputSource(i);

            // If it's a CV output, check if it's used as OUTPUT
            if (src >= CvInputSource::CvOut1 &&
                src <= CvInputSource::CvOut8) {
                int cvIdx = i - int(CvInputSource::CvOut1);
                if (isCvOutputUsedAsOutput(cvIdx)) {
                    continue;  // Skip - conflict!
                }
            }

            available.push_back(src);
        }

        return available;
    }

    // Get valid trigger output destinations (TO-TRA-D)
    std::vector<TriggerOutputDest> getAvailableTriggerOutputDests() const {
        std::vector<TriggerOutputDest> available;

        for (int i = 0; i < int(TriggerOutputDest::Last); ++i) {
            auto dest = TriggerOutputDest(i);
            int gateIdx = i;  // GateOut1=0, GateOut2=1, etc

            // Check if this gate is used as INPUT
            if (isGateOutputUsedAsInput(gateIdx)) {
                continue;  // Skip - conflict!
            }

            available.push_back(dest);
        }

        return available;
    }

    // Get valid CV output destinations (TO-CV1-4)
    std::vector<CvOutputDest> getAvailableCvOutputDests() const {
        std::vector<CvOutputDest> available;

        for (int i = 0; i < int(CvOutputDest::Last); ++i) {
            auto dest = CvOutputDest(i);
            int cvIdx = i;  // CvOut1=0, CvOut2=1, etc

            // Check if this CV output is used as INPUT
            if (isCvOutputUsedAsInput(cvIdx)) {
                continue;  // Skip - conflict!
            }

            available.push_back(dest);
        }

        return available;
    }

    bool hasAnyScriptCommands() const {
        for (int script = 0; script < EditableScriptCount; ++script) {
            for (int line = 0; line < ScriptLineCount; ++line) {
                const tele_command_t *cmd = ss_get_script_command(const_cast<scene_state_t *>(&_state), script, line);
                if (cmd && cmd->length > 0) {
                    return true;
                }
            }
        }
        return false;
    }
    const scene_pattern_t &pattern(int index) const {
        int clamped = clamp(index, 0, PATTERN_COUNT - 1);
        return _state.patterns[clamped];
    }
    void setPattern(int index, const scene_pattern_t &pattern) {
        int clamped = clamp(index, 0, PATTERN_COUNT - 1);
        _state.patterns[clamped] = pattern;
        syncToActiveSlot();
    }

    int bootScriptIndex() const { return activeSlot().bootScriptIndex; }
    void setBootScriptIndex(int index) {
        activeSlot().bootScriptIndex = clamp<uint8_t>(index, 0, ScriptSlotCount - 1);
    }
    void requestBootScriptRun() { _bootScriptRequested = true; }
    bool consumeBootScriptRequest() {
        bool pending = _bootScriptRequested;
        _bootScriptRequested = false;
        return pending;
    }
    bool consumeMetroResetOnLoad() {
        bool pending = _resetMetroOnLoad;
        _resetMetroOnLoad = false;
        return pending;
    }

    int activePatternSlot() const { return _activePatternSlot; }
    int patternSlotForPattern(int patternIndex) const {
        return clamp(patternIndex, 0, CONFIG_PATTERN_COUNT - 1) % PatternSlotCount;
    }
    PatternSlot patternSlotSnapshot(int patternIndex) const;
    void setPatternSlotForPattern(int patternIndex, const PatternSlot &slot);
    void clearPatternSlot(int patternIndex);
    void copyPatternSlot(int srcPatternIndex, int dstPatternIndex);
    void onPatternChanged(int patternIndex);
    void applyPatternSlot(int slotIndex);
    void applyActivePatternSlot();
    void syncToActiveSlot();

    //----------------------------------------
    // Name printing helpers
    //----------------------------------------

    static const char *triggerInputSourceName(TriggerInputSource source);
    static const char *cvInputSourceName(CvInputSource source);
    static const char *triggerOutputDestName(TriggerOutputDest dest);
    static const char *cvOutputDestName(CvOutputDest dest);
    static const char *timeBaseName(TimeBase mode) {
        switch (mode) {
        case TimeBase::Ms:    return "MS";
        case TimeBase::Clock: return "Clock";
        case TimeBase::Last:  break;
        }
        return nullptr;
    }

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
    }

    int8_t _trackIndex = -1;
    scene_state_t _state;

    std::array<PatternSlot, PatternSlotCount> _patternSlots{};
    uint8_t _activePatternSlot = 0;
    bool _resetMetroOnLoad = true;
    bool _bootScriptRequested = false;

    friend class Track;
};
