#include "TeletypeTrack.h"

extern "C" {
#include "teletype.h"
}

void TeletypeTrack::clear() {
    ss_init(&_state);
    _resetMetroOnLoad = true;
    _bootScriptRequested = false;

    // Initialize both pattern slots with default values
    for (int slot = 0; slot < PatternSlotCount; ++slot) {
        auto &ps = _patternSlots[slot];

        ps.midiSource.clear();

        // Default trigger inputs
        ps.triggerInputSource[0] = TriggerInputSource::CvIn1;
        ps.triggerInputSource[1] = TriggerInputSource::CvIn2;
        ps.triggerInputSource[2] = TriggerInputSource::LogicalGate3;
        ps.triggerInputSource[3] = TriggerInputSource::LogicalGate4;

        // Default CV inputs
        ps.cvInSource = CvInputSource::CvIn3;
        ps.cvParamSource = CvInputSource::CvIn4;
        ps.cvXSource = CvInputSource::None;
        ps.cvYSource = CvInputSource::None;
        ps.cvZSource = CvInputSource::LogicalCv4;
        ps.cvTSource = CvInputSource::None;

        // Default trigger outputs
        ps.triggerOutputDest[0] = TriggerOutputDest::GateOut1;
        ps.triggerOutputDest[1] = TriggerOutputDest::GateOut2;
        ps.triggerOutputDest[2] = TriggerOutputDest::GateOut3;
        ps.triggerOutputDest[3] = TriggerOutputDest::GateOut4;

        // Default CV outputs
        ps.cvOutputDest[0] = CvOutputDest::CvOut1;
        ps.cvOutputDest[1] = CvOutputDest::CvOut2;
        ps.cvOutputDest[2] = CvOutputDest::CvOut3;
        ps.cvOutputDest[3] = CvOutputDest::CvOut4;

        ps.bootScriptIndex = 0;
        ps.timeBase = TimeBase::Ms;
        ps.clockDivisor = 12;
        ps.clockMultiplier = 100;
        ps.resetMetroOnLoad = true;

        // CV output config defaults
        for (int i = 0; i < CvOutputCount; ++i) {
            ps.cvOutputRange[i] = Types::VoltageRange::Bipolar5V;
            ps.cvOutputOffset[i] = 0;
            ps.cvOutputQuantizeScale[i] = (i == 0) ? QuantizeDefault : QuantizeOff;
            ps.cvOutputRootNote[i] = -1;
        }

        // Clear slot scripts and patterns
        ps.slotScriptLength = 0;
        ps.metroLength = 0;
        std::memset(ps.slotScript.data(), 0, sizeof(ps.slotScript));
        std::memset(ps.metro.data(), 0, sizeof(ps.metro));
        for (int i = 0; i < PATTERN_COUNT; ++i) {
            ps.patterns[i] = _state.patterns[i];  // Copy from initialized VM state
        }
    }
    _activePatternSlot = 0;
}

void TeletypeTrack::gateOutputName(int index, StringBuilder &str) const {
    str("TT G%d", (index % 4) + 1);
}

void TeletypeTrack::cvOutputName(int index, StringBuilder &str) const {
    str("TT CV%d", (index % 4) + 1);
}

void TeletypeTrack::seedOutputDestsFromTrackIndex(int trackIndex) {
    int start = clamp(trackIndex, 0, CONFIG_CHANNEL_COUNT - 1);
    auto &slot = activeSlot();
    for (int i = 0; i < TriggerOutputCount; ++i) {
        int outputIndex = (start + i) % CONFIG_CHANNEL_COUNT;
        slot.triggerOutputDest[i] = TriggerOutputDest(outputIndex);
    }
    for (int i = 0; i < CvOutputCount; ++i) {
        int outputIndex = (start + i) % CONFIG_CHANNEL_COUNT;
        slot.cvOutputDest[i] = CvOutputDest(outputIndex);
    }
}

const char *TeletypeTrack::triggerInputSourceName(TriggerInputSource source) {
    switch (source) {
    case TriggerInputSource::None:     return "None";
    case TriggerInputSource::CvIn1:    return "CV In 1";
    case TriggerInputSource::CvIn2:    return "CV In 2";
    case TriggerInputSource::CvIn3:    return "CV In 3";
    case TriggerInputSource::CvIn4:    return "CV In 4";
    case TriggerInputSource::GateOut1: return "Gate Out 1";
    case TriggerInputSource::GateOut2: return "Gate Out 2";
    case TriggerInputSource::GateOut3: return "Gate Out 3";
    case TriggerInputSource::GateOut4: return "Gate Out 4";
    case TriggerInputSource::GateOut5: return "Gate Out 5";
    case TriggerInputSource::GateOut6: return "Gate Out 6";
    case TriggerInputSource::GateOut7: return "Gate Out 7";
    case TriggerInputSource::GateOut8: return "Gate Out 8";
    case TriggerInputSource::LogicalGate1: return "L-G1";
    case TriggerInputSource::LogicalGate2: return "L-G2";
    case TriggerInputSource::LogicalGate3: return "L-G3";
    case TriggerInputSource::LogicalGate4: return "L-G4";
    case TriggerInputSource::LogicalGate5: return "L-G5";
    case TriggerInputSource::LogicalGate6: return "L-G6";
    case TriggerInputSource::LogicalGate7: return "L-G7";
    case TriggerInputSource::LogicalGate8: return "L-G8";
    case TriggerInputSource::Last:     break;
    }
    return nullptr;
}

const char *TeletypeTrack::cvInputSourceName(CvInputSource source) {
    switch (source) {
    case CvInputSource::CvIn1:  return "CV In 1";
    case CvInputSource::CvIn2:  return "CV In 2";
    case CvInputSource::CvIn3:  return "CV In 3";
    case CvInputSource::CvIn4:  return "CV In 4";
    case CvInputSource::CvOut1: return "CV Out 1";
    case CvInputSource::CvOut2: return "CV Out 2";
    case CvInputSource::CvOut3: return "CV Out 3";
    case CvInputSource::CvOut4: return "CV Out 4";
    case CvInputSource::CvOut5: return "CV Out 5";
    case CvInputSource::CvOut6: return "CV Out 6";
    case CvInputSource::CvOut7: return "CV Out 7";
    case CvInputSource::CvOut8: return "CV Out 8";
    case CvInputSource::CvRoute1: return "CVR 1";
    case CvInputSource::CvRoute2: return "CVR 2";
    case CvInputSource::CvRoute3: return "CVR 3";
    case CvInputSource::CvRoute4: return "CVR 4";
    case CvInputSource::LogicalCv1: return "L-CV1";
    case CvInputSource::LogicalCv2: return "L-CV2";
    case CvInputSource::LogicalCv3: return "L-CV3";
    case CvInputSource::LogicalCv4: return "L-CV4";
    case CvInputSource::LogicalCv5: return "L-CV5";
    case CvInputSource::LogicalCv6: return "L-CV6";
    case CvInputSource::LogicalCv7: return "L-CV7";
    case CvInputSource::LogicalCv8: return "L-CV8";
    case CvInputSource::None:   return "Off";
    case CvInputSource::Last:   break;
    }
    return nullptr;
}

const char *TeletypeTrack::triggerOutputDestName(TriggerOutputDest dest) {
    switch (dest) {
    case TriggerOutputDest::GateOut1: return "Gate Out 1";
    case TriggerOutputDest::GateOut2: return "Gate Out 2";
    case TriggerOutputDest::GateOut3: return "Gate Out 3";
    case TriggerOutputDest::GateOut4: return "Gate Out 4";
    case TriggerOutputDest::GateOut5: return "Gate Out 5";
    case TriggerOutputDest::GateOut6: return "Gate Out 6";
    case TriggerOutputDest::GateOut7: return "Gate Out 7";
    case TriggerOutputDest::GateOut8: return "Gate Out 8";
    case TriggerOutputDest::Last:     break;
    }
    return nullptr;
}

const char *TeletypeTrack::cvOutputDestName(CvOutputDest dest) {
    switch (dest) {
    case CvOutputDest::CvOut1: return "CV Out 1";
    case CvOutputDest::CvOut2: return "CV Out 2";
    case CvOutputDest::CvOut3: return "CV Out 3";
    case CvOutputDest::CvOut4: return "CV Out 4";
    case CvOutputDest::CvOut5: return "CV Out 5";
    case CvOutputDest::CvOut6: return "CV Out 6";
    case CvOutputDest::CvOut7: return "CV Out 7";
    case CvOutputDest::CvOut8: return "CV Out 8";
    case CvOutputDest::Last:   break;
    }
    return nullptr;
}

void TeletypeTrack::write(VersionedSerializedWriter &writer) const {
    auto *self = const_cast<TeletypeTrack *>(this);
    self->syncToActiveSlot();

    const auto &slot = activeSlot();

    // Write I/O mapping configuration from active slot
    for (int i = 0; i < 4; ++i) {
        writer.write(uint8_t(slot.triggerInputSource[i]));
    }
    writer.write(uint8_t(slot.cvInSource));
    writer.write(uint8_t(slot.cvParamSource));
    writer.write(uint8_t(slot.cvXSource));
    writer.write(uint8_t(slot.cvYSource));
    writer.write(uint8_t(slot.cvZSource));
    writer.write(uint8_t(slot.cvTSource));
    for (int i = 0; i < 4; ++i) {
        writer.write(uint8_t(slot.triggerOutputDest[i]));
    }
    for (int i = 0; i < 4; ++i) {
        writer.write(uint8_t(slot.cvOutputDest[i]));
    }
    slot.midiSource.write(writer);
    writer.write(uint8_t(slot.bootScriptIndex));
    writer.write(uint8_t(slot.timeBase));
    writer.write(slot.clockDivisor);
    writer.write(slot.clockMultiplier);
    for (int i = 0; i < 4; ++i) {
        writer.write(uint8_t(slot.cvOutputRange[i]));
        writer.write(slot.cvOutputOffset[i]);
    }
    for (int i = 0; i < 4; ++i) {
        writer.write(slot.cvOutputQuantizeScale[i]);
        writer.write(slot.cvOutputRootNote[i]);
    }
    scene_state_t &state = const_cast<scene_state_t &>(_state);
    for (int script = 0; script < EditableScriptCount; ++script) {
        writer.write(state.scripts[script].l);
        for (int line = 0; line < ScriptLineCount; ++line) {
            const tele_command_t *cmd = ss_get_script_command(&state, script, line);
            writer.write(cmd, sizeof(tele_command_t));
        }
    }
    for (int pattern = 0; pattern < PATTERN_COUNT; ++pattern) {
        writer.write(_state.patterns[pattern]);
    }

    writer.write(_activePatternSlot);
    for (int slotIdx = 0; slotIdx < PatternSlotCount; ++slotIdx) {
        const auto &patternSlot = _patternSlots[slotIdx];
        writer.write(patternSlot.slotScriptLength);
        writer.write(patternSlot.metroLength);
        writer.write(patternSlot.slotScript);
        writer.write(patternSlot.metro);
        for (int pattern = 0; pattern < PATTERN_COUNT; ++pattern) {
            writer.write(patternSlot.patterns[pattern]);
        }
        for (int i = 0; i < 4; ++i) {
            writer.write(uint8_t(patternSlot.triggerInputSource[i]));
        }
        writer.write(uint8_t(patternSlot.cvInSource));
        writer.write(uint8_t(patternSlot.cvParamSource));
        writer.write(uint8_t(patternSlot.cvXSource));
        writer.write(uint8_t(patternSlot.cvYSource));
        writer.write(uint8_t(patternSlot.cvZSource));
        writer.write(uint8_t(patternSlot.cvTSource));
        for (int i = 0; i < 4; ++i) {
            writer.write(uint8_t(patternSlot.triggerOutputDest[i]));
        }
        for (int i = 0; i < 4; ++i) {
            writer.write(uint8_t(patternSlot.cvOutputDest[i]));
        }
        patternSlot.midiSource.write(writer);
        writer.write(uint8_t(patternSlot.bootScriptIndex));
        writer.write(uint8_t(patternSlot.timeBase));
        writer.write(patternSlot.clockDivisor);
        writer.write(patternSlot.clockMultiplier);
        for (int i = 0; i < 4; ++i) {
            writer.write(uint8_t(patternSlot.cvOutputRange[i]));
            writer.write(patternSlot.cvOutputOffset[i]);
        }
        for (int i = 0; i < 4; ++i) {
            writer.write(patternSlot.cvOutputQuantizeScale[i]);
            writer.write(patternSlot.cvOutputRootNote[i]);
        }
        writer.write(uint8_t(patternSlot.resetMetroOnLoad));
    }
}

void TeletypeTrack::read(VersionedSerializedReader &reader) {
    clear();

    // Read legacy I/O mapping into slot 0 (for backwards compatibility)
    // If slot data is present in file, it will be overwritten below
    auto &legacySlot = _patternSlots[0];
    for (int i = 0; i < 4; ++i) {
        uint8_t val;
        reader.read(val);
        legacySlot.triggerInputSource[i] = ModelUtils::clampedEnum(TriggerInputSource(val));
    }
    uint8_t cvInVal, cvParamVal, cvXVal, cvYVal, cvZVal, cvTVal;
    reader.read(cvInVal);
    reader.read(cvParamVal);
    reader.read(cvXVal);
    reader.read(cvYVal);
    reader.read(cvZVal);
    reader.read(cvTVal);
    legacySlot.cvInSource = ModelUtils::clampedEnum(CvInputSource(cvInVal));
    legacySlot.cvParamSource = ModelUtils::clampedEnum(CvInputSource(cvParamVal));
    legacySlot.cvXSource = ModelUtils::clampedEnum(CvInputSource(cvXVal));
    legacySlot.cvYSource = ModelUtils::clampedEnum(CvInputSource(cvYVal));
    legacySlot.cvZSource = ModelUtils::clampedEnum(CvInputSource(cvZVal));
    legacySlot.cvTSource = ModelUtils::clampedEnum(CvInputSource(cvTVal));

    for (int i = 0; i < 4; ++i) {
        uint8_t val;
        reader.read(val);
        legacySlot.triggerOutputDest[i] = ModelUtils::clampedEnum(TriggerOutputDest(val));
    }
    for (int i = 0; i < 4; ++i) {
        uint8_t val;
        reader.read(val);
        legacySlot.cvOutputDest[i] = ModelUtils::clampedEnum(CvOutputDest(val));
    }
    legacySlot.midiSource.read(reader);
    uint8_t bootScriptVal = 0;
    reader.read(bootScriptVal);
    legacySlot.bootScriptIndex = clamp<uint8_t>(bootScriptVal, 0, ScriptSlotCount - 1);
    uint8_t timeBaseVal;
    reader.read(timeBaseVal);
    legacySlot.timeBase = ModelUtils::clampedEnum(TimeBase(timeBaseVal));
    reader.read(legacySlot.clockDivisor);
    legacySlot.clockDivisor = ModelUtils::clampDivisor(legacySlot.clockDivisor);
    reader.read(legacySlot.clockMultiplier);
    legacySlot.clockMultiplier = clamp<int16_t>(legacySlot.clockMultiplier, 50, 150);
    for (int i = 0; i < 4; ++i) {
        uint8_t rangeVal;
        reader.read(rangeVal);
        legacySlot.cvOutputRange[i] = ModelUtils::clampedEnum(Types::VoltageRange(rangeVal));
        reader.read(legacySlot.cvOutputOffset[i]);
        legacySlot.cvOutputOffset[i] = clamp<int16_t>(legacySlot.cvOutputOffset[i], -500, 500);
    }
    for (int i = 0; i < 4; ++i) {
        reader.read(legacySlot.cvOutputQuantizeScale[i]);
        legacySlot.cvOutputQuantizeScale[i] = clamp<int8_t>(legacySlot.cvOutputQuantizeScale[i], QuantizeOff, Scale::Count - 1);
        reader.read(legacySlot.cvOutputRootNote[i]);
        legacySlot.cvOutputRootNote[i] = clamp<int8_t>(legacySlot.cvOutputRootNote[i], -1, 11);
    }

    // Read VM scripts
    for (int script = 0; script < EditableScriptCount; ++script) {
        ss_clear_script(&_state, script);
        uint8_t scriptLen = 0;
        reader.read(scriptLen);
        _state.scripts[script].l = clamp<uint8_t>(scriptLen, 0, ScriptLineCount);
        for (int line = 0; line < ScriptLineCount; ++line) {
            reader.read(_state.scripts[script].c[line], 0);
        }
    }
    for (int pattern = 0; pattern < PATTERN_COUNT; ++pattern) {
        reader.read(_state.patterns[pattern], 0);
    }

    // Read pattern slots (overwrites legacy data if present)
    uint8_t activeSlotVal = 0;
    reader.read(activeSlotVal);
    _activePatternSlot = clamp<uint8_t>(activeSlotVal, 0, PatternSlotCount - 1);
    for (int slotIdx = 0; slotIdx < PatternSlotCount; ++slotIdx) {
        auto &patternSlot = _patternSlots[slotIdx];
        reader.read(patternSlot.slotScriptLength, 0);
        reader.read(patternSlot.metroLength, 0);
        reader.read(patternSlot.slotScript, 0);
        reader.read(patternSlot.metro, 0);
        for (int pattern = 0; pattern < PATTERN_COUNT; ++pattern) {
            reader.read(patternSlot.patterns[pattern], 0);
        }
        for (int i = 0; i < 4; ++i) {
            uint8_t val;
            reader.read(val, 0);
            patternSlot.triggerInputSource[i] = ModelUtils::clampedEnum(TriggerInputSource(val));
        }
        uint8_t cvInVal2, cvParamVal2, cvXVal2, cvYVal2, cvZVal2, cvTVal2;
        reader.read(cvInVal2, 0);
        reader.read(cvParamVal2, 0);
        reader.read(cvXVal2, 0);
        reader.read(cvYVal2, 0);
        reader.read(cvZVal2, 0);
        reader.read(cvTVal2, 0);
        patternSlot.cvInSource = ModelUtils::clampedEnum(CvInputSource(cvInVal2));
        patternSlot.cvParamSource = ModelUtils::clampedEnum(CvInputSource(cvParamVal2));
        patternSlot.cvXSource = ModelUtils::clampedEnum(CvInputSource(cvXVal2));
        patternSlot.cvYSource = ModelUtils::clampedEnum(CvInputSource(cvYVal2));
        patternSlot.cvZSource = ModelUtils::clampedEnum(CvInputSource(cvZVal2));
        patternSlot.cvTSource = ModelUtils::clampedEnum(CvInputSource(cvTVal2));
        for (int i = 0; i < 4; ++i) {
            uint8_t val;
            reader.read(val, 0);
            patternSlot.triggerOutputDest[i] = ModelUtils::clampedEnum(TriggerOutputDest(val));
        }
        for (int i = 0; i < 4; ++i) {
            uint8_t val;
            reader.read(val, 0);
            patternSlot.cvOutputDest[i] = ModelUtils::clampedEnum(CvOutputDest(val));
        }
        patternSlot.midiSource.read(reader);
        uint8_t bootVal = 0;
        reader.read(bootVal, 0);
        patternSlot.bootScriptIndex = clamp<uint8_t>(bootVal, 0, ScriptSlotCount - 1);
        uint8_t tbVal = 0;
        reader.read(tbVal, 0);
        patternSlot.timeBase = ModelUtils::clampedEnum(TimeBase(tbVal));
        reader.read(patternSlot.clockDivisor, 0);
        patternSlot.clockDivisor = ModelUtils::clampDivisor(patternSlot.clockDivisor);
        reader.read(patternSlot.clockMultiplier, 0);
        patternSlot.clockMultiplier = clamp<int16_t>(patternSlot.clockMultiplier, 50, 150);
        for (int i = 0; i < 4; ++i) {
            uint8_t rangeVal;
            reader.read(rangeVal, 0);
            patternSlot.cvOutputRange[i] = ModelUtils::clampedEnum(Types::VoltageRange(rangeVal));
            reader.read(patternSlot.cvOutputOffset[i], 0);
            patternSlot.cvOutputOffset[i] = clamp<int16_t>(patternSlot.cvOutputOffset[i], -500, 500);
        }
        for (int i = 0; i < 4; ++i) {
            reader.read(patternSlot.cvOutputQuantizeScale[i], 0);
            patternSlot.cvOutputQuantizeScale[i] = clamp<int8_t>(patternSlot.cvOutputQuantizeScale[i], QuantizeOff, Scale::Count - 1);
            reader.read(patternSlot.cvOutputRootNote[i], 0);
            patternSlot.cvOutputRootNote[i] = clamp<int8_t>(patternSlot.cvOutputRootNote[i], -1, 11);
        }
        uint8_t resetMetroVal = 0;
        reader.read(resetMetroVal, 0);
        patternSlot.resetMetroOnLoad = resetMetroVal != 0;
    }
    _resetMetroOnLoad = true;
    _bootScriptRequested = false;
    applyPatternSlot(_activePatternSlot);
}

TeletypeTrack::PatternSlot TeletypeTrack::patternSlotSnapshot(int patternIndex) const {
    auto *self = const_cast<TeletypeTrack *>(this);
    self->syncToActiveSlot();
    const int slot = patternSlotForPattern(patternIndex);
    return self->_patternSlots[slot];
}

void TeletypeTrack::setPatternSlotForPattern(int patternIndex, const PatternSlot &slot) {
    syncToActiveSlot();
    const int slotIndex = patternSlotForPattern(patternIndex);
    _patternSlots[slotIndex] = slot;
    if (slotIndex == _activePatternSlot) {
        applyPatternSlot(slotIndex);
    }
}

void TeletypeTrack::clearPatternSlot(int patternIndex) {
    syncToActiveSlot();
    const int slotIndex = patternSlotForPattern(patternIndex);
    auto &slot = _patternSlots[slotIndex];
    slot.slotScriptLength = 0;
    slot.metroLength = 0;
    std::memset(slot.slotScript.data(), 0, sizeof(slot.slotScript));
    std::memset(slot.metro.data(), 0, sizeof(slot.metro));
    scene_state_t defaults{};
    ss_init(&defaults);
    for (int i = 0; i < PATTERN_COUNT; ++i) {
        slot.patterns[i] = defaults.patterns[i];
    }
    if (slotIndex == _activePatternSlot) {
        applyPatternSlot(slotIndex);
    }
}

void TeletypeTrack::copyPatternSlot(int srcPatternIndex, int dstPatternIndex) {
    syncToActiveSlot();
    const int srcSlot = patternSlotForPattern(srcPatternIndex);
    const int dstSlot = patternSlotForPattern(dstPatternIndex);
    if (srcSlot == dstSlot) {
        return;
    }
    _patternSlots[dstSlot] = _patternSlots[srcSlot];
    if (dstSlot == _activePatternSlot) {
        applyPatternSlot(dstSlot);
    }
}

void TeletypeTrack::onPatternChanged(int patternIndex) {
    const int slot = patternSlotForPattern(patternIndex);
    if (slot == _activePatternSlot) {
        return;
    }
    syncToActiveSlot();

    applyPatternSlot(slot);
}

void TeletypeTrack::applyPatternSlot(int slotIndex) {
    const int slot = clamp(slotIndex, 0, PatternSlotCount - 1);
    _activePatternSlot = slot;

    // Copy slot-specific VM state (scripts and patterns) into the VM
    const auto &patternSlot = _patternSlots[_activePatternSlot];
    _state.scripts[SlotScriptIndex].l = clamp<uint8_t>(patternSlot.slotScriptLength, 0, ScriptLineCount);
    _state.scripts[METRO_SCRIPT].l = clamp<uint8_t>(patternSlot.metroLength, 0, ScriptLineCount);
    std::memcpy(_state.scripts[SlotScriptIndex].c, patternSlot.slotScript.data(), sizeof(patternSlot.slotScript));
    std::memcpy(_state.scripts[METRO_SCRIPT].c, patternSlot.metro.data(), sizeof(patternSlot.metro));
    for (int i = 0; i < PATTERN_COUNT; ++i) {
        _state.patterns[i] = patternSlot.patterns[i];
    }
    _resetMetroOnLoad = patternSlot.resetMetroOnLoad;
}

void TeletypeTrack::applyActivePatternSlot() {
    applyPatternSlot(_activePatternSlot);
}

void TeletypeTrack::syncToActiveSlot() {
    // Sync VM state (scripts and patterns) back to the active slot
    auto &patternSlot = _patternSlots[_activePatternSlot];
    patternSlot.slotScriptLength = _state.scripts[SlotScriptIndex].l;
    patternSlot.metroLength = _state.scripts[METRO_SCRIPT].l;
    std::memcpy(patternSlot.slotScript.data(), _state.scripts[SlotScriptIndex].c, sizeof(patternSlot.slotScript));
    std::memcpy(patternSlot.metro.data(), _state.scripts[METRO_SCRIPT].c, sizeof(patternSlot.metro));

    for (int i = 0; i < PATTERN_COUNT; ++i) {
        patternSlot.patterns[i] = _state.patterns[i];
    }
    patternSlot.resetMetroOnLoad = _resetMetroOnLoad;
}
