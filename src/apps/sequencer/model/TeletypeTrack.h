#pragma once

#include "Config.h"
#include "Serialize.h"
#include "ModelUtils.h"

#include "core/utils/StringBuilder.h"

#include <array>
#include <vector>

extern "C" {
#include "state.h"
}

class TeletypeTrack {
public:
    static constexpr int TriggerInputCount = 4;
    static constexpr int TriggerOutputCount = 4;
    static constexpr int CvOutputCount = 4;
    static constexpr int ScriptSlotCount = 4;

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

    TeletypeTrack() { clear(); }

    void clear();

    void gateOutputName(int index, StringBuilder &str) const;
    void cvOutputName(int index, StringBuilder &str) const;

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

    scene_state_t &state() { return _state; }
    const scene_state_t &state() const { return _state; }

    //----------------------------------------
    // I/O Mapping Accessors
    //----------------------------------------

    // TI-TR1 to TI-TR4 (4 trigger inputs)
    TriggerInputSource triggerInputSource(int index) const {
        if (index < 0 || index >= TriggerInputCount) return TriggerInputSource::None;
        return _triggerInputSource[index];
    }
    void setTriggerInputSource(int index, TriggerInputSource source) {
        if (index >= 0 && index < TriggerInputCount) {
            _triggerInputSource[index] = ModelUtils::clampedEnum(source);
        }
    }
    void editTriggerInputSource(int index, int value, bool shift) {
        if (index >= 0 && index < TriggerInputCount) {
            setTriggerInputSource(index, ModelUtils::adjustedEnum(_triggerInputSource[index], value));
        }
    }
    void printTriggerInputSource(int index, StringBuilder &str) const {
        if (index >= 0 && index < TriggerInputCount) {
            str(triggerInputSourceName(_triggerInputSource[index]));
        }
    }

    // TI-IN (Teletype IN variable)
    CvInputSource cvInSource() const { return _cvInSource; }
    void setCvInSource(CvInputSource source) {
        _cvInSource = ModelUtils::clampedEnum(source);
    }
    void editCvInSource(int value, bool shift) {
        setCvInSource(ModelUtils::adjustedEnum(_cvInSource, value));
    }
    void printCvInSource(StringBuilder &str) const {
        str(cvInputSourceName(_cvInSource));
    }

    // TI-PARAM (Teletype PARAM variable)
    CvInputSource cvParamSource() const { return _cvParamSource; }
    void setCvParamSource(CvInputSource source) {
        _cvParamSource = ModelUtils::clampedEnum(source);
    }
    void editCvParamSource(int value, bool shift) {
        setCvParamSource(ModelUtils::adjustedEnum(_cvParamSource, value));
    }
    void printCvParamSource(StringBuilder &str) const {
        str(cvInputSourceName(_cvParamSource));
    }

    // TO-TRA to TO-TRD (4 trigger outputs)
    TriggerOutputDest triggerOutputDest(int index) const {
        if (index < 0 || index >= TriggerOutputCount) return TriggerOutputDest::GateOut1;
        return _triggerOutputDest[index];
    }
    void setTriggerOutputDest(int index, TriggerOutputDest dest) {
        if (index >= 0 && index < TriggerOutputCount) {
            _triggerOutputDest[index] = ModelUtils::clampedEnum(dest);
        }
    }
    void editTriggerOutputDest(int index, int value, bool shift) {
        if (index >= 0 && index < TriggerOutputCount) {
            setTriggerOutputDest(index, ModelUtils::adjustedEnum(_triggerOutputDest[index], value));
        }
    }
    void printTriggerOutputDest(int index, StringBuilder &str) const {
        if (index >= 0 && index < TriggerOutputCount) {
            str(triggerOutputDestName(_triggerOutputDest[index]));
        }
    }

    // TO-CV1 to TO-CV4 (4 CV outputs)
    CvOutputDest cvOutputDest(int index) const {
        if (index < 0 || index >= CvOutputCount) return CvOutputDest::CvOut1;
        return _cvOutputDest[index];
    }
    void setCvOutputDest(int index, CvOutputDest dest) {
        if (index >= 0 && index < CvOutputCount) {
            _cvOutputDest[index] = ModelUtils::clampedEnum(dest);
        }
    }
    void editCvOutputDest(int index, int value, bool shift) {
        if (index >= 0 && index < CvOutputCount) {
            setCvOutputDest(index, ModelUtils::adjustedEnum(_cvOutputDest[index], value));
        }
    }
    void printCvOutputDest(int index, StringBuilder &str) const {
        if (index >= 0 && index < CvOutputCount) {
            str(cvOutputDestName(_cvOutputDest[index]));
        }
    }

    //----------------------------------------
    // Conflict Detection
    //----------------------------------------

    // Check if a CV output is already used as CV OUTPUT destination
    bool isCvOutputUsedAsOutput(int cvOutIndex) const {
        for (int i = 0; i < CvOutputCount; ++i) {
            if (int(_cvOutputDest[i]) == cvOutIndex) {
                return true;
            }
        }
        return false;
    }

    // Check if a CV output is already used as CV INPUT source (TI-IN or TI-PARAM)
    bool isCvOutputUsedAsInput(int cvOutIndex) const {
        // Check TI-IN
        if (_cvInSource >= CvInputSource::CvOut1 &&
            _cvInSource <= CvInputSource::CvOut8) {
            int srcCv = int(_cvInSource) - int(CvInputSource::CvOut1);
            if (srcCv == cvOutIndex) {
                return true;
            }
        }

        // Check TI-PARAM
        if (_cvParamSource >= CvInputSource::CvOut1 &&
            _cvParamSource <= CvInputSource::CvOut8) {
            int srcCv = int(_cvParamSource) - int(CvInputSource::CvOut1);
            if (srcCv == cvOutIndex) {
                return true;
            }
        }

        return false;
    }

    // Check if a gate output is already used as trigger OUTPUT
    bool isGateOutputUsedAsOutput(int gateIndex) const {
        for (int i = 0; i < TriggerOutputCount; ++i) {
            if (int(_triggerOutputDest[i]) == gateIndex) {
                return true;
            }
        }
        return false;
    }

    // Check if a gate output is already used as trigger INPUT
    bool isGateOutputUsedAsInput(int gateIndex) const {
        for (int i = 0; i < TriggerInputCount; ++i) {
            auto src = _triggerInputSource[i];
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

    //----------------------------------------
    // Script preset selection (session-only)
    //----------------------------------------
    int scriptPresetIndex(int slot) const {
        if (slot < 0 || slot >= ScriptSlotCount) {
            return 0;
        }
        return _scriptPresetIndex[slot];
    }
    void setScriptPresetIndex(int slot, int index) {
        if (slot >= 0 && slot < ScriptSlotCount) {
            _scriptPresetIndex[slot] = static_cast<uint8_t>(index);
        }
    }

    //----------------------------------------
    // Name printing helpers
    //----------------------------------------

    static const char *triggerInputSourceName(TriggerInputSource source);
    static const char *cvInputSourceName(CvInputSource source);
    static const char *triggerOutputDestName(TriggerOutputDest dest);
    static const char *cvOutputDestName(CvOutputDest dest);

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
    }

    int8_t _trackIndex = -1;
    scene_state_t _state;

    // I/O Mapping configuration
    std::array<TriggerInputSource, TriggerInputCount> _triggerInputSource;  // TI-TR1 to TI-TR4
    CvInputSource _cvInSource;                              // TI-IN
    CvInputSource _cvParamSource;                           // TI-PARAM
    std::array<TriggerOutputDest, TriggerOutputCount> _triggerOutputDest;  // TO-TRA to TO-TRD
    std::array<CvOutputDest, CvOutputCount> _cvOutputDest;                 // TO-CV1 to TO-CV4
    std::array<uint8_t, ScriptSlotCount> _scriptPresetIndex{};             // S0-S3 (session-only)

    friend class Track;
};
