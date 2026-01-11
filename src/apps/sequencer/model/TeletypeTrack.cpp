#include "TeletypeTrack.h"

extern "C" {
#include "teletype.h"
}

void TeletypeTrack::clear() {
    ss_init(&_state);

    _midiSource.clear();  // Default to MIDI port, Omni channel

    // Default I/O mapping
    // TI-TR1-4 → None (avoid cross-triggering by default)
    _triggerInputSource[0] = TriggerInputSource::CvIn1;
    _triggerInputSource[1] = TriggerInputSource::CvIn2;
    _triggerInputSource[2] = TriggerInputSource::LogicalGate3;
    _triggerInputSource[3] = TriggerInputSource::LogicalGate4;

    // TI-IN → CV input 3, TI-PARAM → CV input 4
    _cvInSource = CvInputSource::CvIn3;
    _cvParamSource = CvInputSource::CvIn4;
    _cvXSource = CvInputSource::None;
    _cvYSource = CvInputSource::None;
    _cvZSource = CvInputSource::LogicalCv4;

    // TO-TRA-D → Gate outputs 1-4
    _triggerOutputDest[0] = TriggerOutputDest::GateOut1;
    _triggerOutputDest[1] = TriggerOutputDest::GateOut2;
    _triggerOutputDest[2] = TriggerOutputDest::GateOut3;
    _triggerOutputDest[3] = TriggerOutputDest::GateOut4;

    // TO-CV1-4 → CV outputs 1-4
    _cvOutputDest[0] = CvOutputDest::CvOut1;
    _cvOutputDest[1] = CvOutputDest::CvOut2;
    _cvOutputDest[2] = CvOutputDest::CvOut3;
    _cvOutputDest[3] = CvOutputDest::CvOut4;

    _bootScriptIndex = 0;
    // Scripts are stored in scene_state; nothing else to clear.
    _resetMetroOnLoad = true;

    // Timing defaults
    _timeBase = TimeBase::Ms;
    _clockDivisor = 12;
    _clockMultiplier = 100;

    // CV range/offset defaults
    for (int i = 0; i < CvOutputCount; ++i) {
        _cvOutputRange[i] = Types::VoltageRange::Bipolar5V;
        _cvOutputOffset[i] = 0;
        _cvOutputQuantizeScale[i] = (i == 0) ? QuantizeDefault : QuantizeOff;
        _cvOutputRootNote[i] = -1;
    }
    for (int i = 0; i < PATTERN_COUNT; ++i) {
        _patterns[i] = _state.patterns[i];
    }

    _activePatternSlot = 0;
    for (int slot = 0; slot < PatternSlotCount; ++slot) {
        _activePatternSlot = slot;
        syncActiveSlotScripts();
        syncActiveSlotPatterns();
        syncActiveSlotMappings();
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
    for (int i = 0; i < TriggerOutputCount; ++i) {
        int outputIndex = (start + i) % CONFIG_CHANNEL_COUNT;
        _triggerOutputDest[i] = TriggerOutputDest(outputIndex);
    }
    for (int i = 0; i < CvOutputCount; ++i) {
        int outputIndex = (start + i) % CONFIG_CHANNEL_COUNT;
        _cvOutputDest[i] = CvOutputDest(outputIndex);
    }
    syncActiveSlotMappings();
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
    self->syncActiveSlotScripts();
    self->syncActiveSlotPatterns();
    self->syncActiveSlotMappings();

    // Write I/O mapping configuration
    for (int i = 0; i < 4; ++i) {
        writer.write(uint8_t(_triggerInputSource[i]));
    }
    writer.write(uint8_t(_cvInSource));
    writer.write(uint8_t(_cvParamSource));
    writer.write(uint8_t(_cvXSource));
    writer.write(uint8_t(_cvYSource));
    writer.write(uint8_t(_cvZSource));
    for (int i = 0; i < 4; ++i) {
        writer.write(uint8_t(_triggerOutputDest[i]));
    }
    for (int i = 0; i < 4; ++i) {
        writer.write(uint8_t(_cvOutputDest[i]));
    }
    _midiSource.write(writer);
    writer.write(uint8_t(_bootScriptIndex));
    writer.write(uint8_t(_timeBase));
    writer.write(_clockDivisor);
    writer.write(_clockMultiplier);
    for (int i = 0; i < 4; ++i) {
        writer.write(uint8_t(_cvOutputRange[i]));
        writer.write(_cvOutputOffset[i]);
    }
    for (int i = 0; i < 4; ++i) {
        writer.write(_cvOutputQuantizeScale[i]);
        writer.write(_cvOutputRootNote[i]);
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
        writer.write(_patterns[pattern]);
    }

    writer.write(_activePatternSlot);
    for (int slot = 0; slot < PatternSlotCount; ++slot) {
        const auto &patternSlot = _patternSlots[slot];
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

    // Read I/O mapping configuration
    for (int i = 0; i < 4; ++i) {
        uint8_t val;
        reader.read(val);
        _triggerInputSource[i] = ModelUtils::clampedEnum(TriggerInputSource(val));
    }
    uint8_t cvInVal, cvParamVal, cvXVal, cvYVal, cvZVal;
    reader.read(cvInVal);
    reader.read(cvParamVal);
    reader.read(cvXVal);
    reader.read(cvYVal);
    reader.read(cvZVal);
    _cvInSource = ModelUtils::clampedEnum(CvInputSource(cvInVal));
    _cvParamSource = ModelUtils::clampedEnum(CvInputSource(cvParamVal));
    _cvXSource = ModelUtils::clampedEnum(CvInputSource(cvXVal));
    _cvYSource = ModelUtils::clampedEnum(CvInputSource(cvYVal));
    _cvZSource = ModelUtils::clampedEnum(CvInputSource(cvZVal));

    for (int i = 0; i < 4; ++i) {
        uint8_t val;
        reader.read(val);
        _triggerOutputDest[i] = ModelUtils::clampedEnum(TriggerOutputDest(val));
    }
    for (int i = 0; i < 4; ++i) {
        uint8_t val;
        reader.read(val);
        _cvOutputDest[i] = ModelUtils::clampedEnum(CvOutputDest(val));
    }
    _midiSource.read(reader);
    uint8_t bootScriptVal = 0;
    reader.read(bootScriptVal);
    _bootScriptIndex = clamp<int8_t>(bootScriptVal, 0, ScriptSlotCount - 1);
    uint8_t timeBaseVal;
    reader.read(timeBaseVal);
    _timeBase = ModelUtils::clampedEnum(TimeBase(timeBaseVal));
    reader.read(_clockDivisor);
    _clockDivisor = ModelUtils::clampDivisor(_clockDivisor);
    reader.read(_clockMultiplier);
    _clockMultiplier = clamp<int16_t>(_clockMultiplier, 50, 150);
    for (int i = 0; i < 4; ++i) {
        uint8_t rangeVal;
        reader.read(rangeVal);
        _cvOutputRange[i] = ModelUtils::clampedEnum(Types::VoltageRange(rangeVal));
        reader.read(_cvOutputOffset[i]);
        _cvOutputOffset[i] = clamp<int16_t>(_cvOutputOffset[i], -500, 500);
    }
    for (int i = 0; i < 4; ++i) {
        reader.read(_cvOutputQuantizeScale[i]);
        _cvOutputQuantizeScale[i] = clamp<int8_t>(_cvOutputQuantizeScale[i], QuantizeOff, Scale::Count - 1);
        reader.read(_cvOutputRootNote[i]);
        _cvOutputRootNote[i] = clamp<int8_t>(_cvOutputRootNote[i], -1, 11);
    }
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
        reader.read(_patterns[pattern], 0);
    }
    uint8_t activeSlot = 0;
    reader.read(activeSlot);
    _activePatternSlot = clamp<uint8_t>(activeSlot, 0, PatternSlotCount - 1);
    for (int slot = 0; slot < PatternSlotCount; ++slot) {
        auto &patternSlot = _patternSlots[slot];
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
        uint8_t cvInVal, cvParamVal, cvXVal, cvYVal, cvZVal;
        reader.read(cvInVal, 0);
        reader.read(cvParamVal, 0);
        reader.read(cvXVal, 0);
        reader.read(cvYVal, 0);
        reader.read(cvZVal, 0);
        patternSlot.cvInSource = ModelUtils::clampedEnum(CvInputSource(cvInVal));
        patternSlot.cvParamSource = ModelUtils::clampedEnum(CvInputSource(cvParamVal));
        patternSlot.cvXSource = ModelUtils::clampedEnum(CvInputSource(cvXVal));
        patternSlot.cvYSource = ModelUtils::clampedEnum(CvInputSource(cvYVal));
        patternSlot.cvZSource = ModelUtils::clampedEnum(CvInputSource(cvZVal));
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
        uint8_t bootScriptVal = 0;
        reader.read(bootScriptVal, 0);
        patternSlot.bootScriptIndex = clamp<uint8_t>(bootScriptVal, 0, ScriptSlotCount - 1);
        uint8_t timeBaseVal = 0;
        reader.read(timeBaseVal, 0);
        patternSlot.timeBase = ModelUtils::clampedEnum(TimeBase(timeBaseVal));
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
}

TeletypeTrack::PatternSlot TeletypeTrack::patternSlotSnapshot(int patternIndex) const {
    auto *self = const_cast<TeletypeTrack *>(this);
    self->syncActiveSlotScripts();
    self->syncActiveSlotPatterns();
    self->syncActiveSlotMappings();
    const int slot = patternSlotForPattern(patternIndex);
    return self->_patternSlots[slot];
}

void TeletypeTrack::setPatternSlotForPattern(int patternIndex, const PatternSlot &slot) {
    syncActiveSlotScripts();
    syncActiveSlotPatterns();
    syncActiveSlotMappings();
    const int slotIndex = patternSlotForPattern(patternIndex);
    _patternSlots[slotIndex] = slot;
    if (slotIndex == _activePatternSlot) {
        applyPatternSlot(slotIndex);
    }
}

void TeletypeTrack::clearPatternSlot(int patternIndex) {
    syncActiveSlotScripts();
    syncActiveSlotPatterns();
    syncActiveSlotMappings();
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
    syncActiveSlotScripts();
    syncActiveSlotPatterns();
    syncActiveSlotMappings();
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
    syncActiveSlotScripts();
    syncActiveSlotPatterns();
    syncActiveSlotMappings();

    applyPatternSlot(slot);
}

void TeletypeTrack::applyPatternSlot(int slotIndex) {
    const int slot = clamp(slotIndex, 0, PatternSlotCount - 1);
    _activePatternSlot = slot;
    const auto &patternSlot = _patternSlots[_activePatternSlot];
    _state.scripts[SlotScriptIndex].l = clamp<uint8_t>(patternSlot.slotScriptLength, 0, ScriptLineCount);
    _state.scripts[METRO_SCRIPT].l = clamp<uint8_t>(patternSlot.metroLength, 0, ScriptLineCount);
    std::memcpy(_state.scripts[SlotScriptIndex].c, patternSlot.slotScript.data(), sizeof(patternSlot.slotScript));
    std::memcpy(_state.scripts[METRO_SCRIPT].c, patternSlot.metro.data(), sizeof(patternSlot.metro));
    for (int i = 0; i < PATTERN_COUNT; ++i) {
        _patterns[i] = patternSlot.patterns[i];
        _state.patterns[i] = patternSlot.patterns[i];
    }
    _triggerInputSource = patternSlot.triggerInputSource;
    _cvInSource = patternSlot.cvInSource;
    _cvParamSource = patternSlot.cvParamSource;
    _cvXSource = patternSlot.cvXSource;
    _cvYSource = patternSlot.cvYSource;
    _cvZSource = patternSlot.cvZSource;
    _triggerOutputDest = patternSlot.triggerOutputDest;
    _cvOutputDest = patternSlot.cvOutputDest;
    _cvOutputRange = patternSlot.cvOutputRange;
    _cvOutputOffset = patternSlot.cvOutputOffset;
    _cvOutputQuantizeScale = patternSlot.cvOutputQuantizeScale;
    _cvOutputRootNote = patternSlot.cvOutputRootNote;
    _midiSource = patternSlot.midiSource;
    _bootScriptIndex = patternSlot.bootScriptIndex;
    _timeBase = patternSlot.timeBase;
    _clockDivisor = patternSlot.clockDivisor;
    _clockMultiplier = patternSlot.clockMultiplier;
    _resetMetroOnLoad = patternSlot.resetMetroOnLoad;
}

void TeletypeTrack::applyActivePatternSlot() {
    applyPatternSlot(_activePatternSlot);
}

void TeletypeTrack::syncActiveSlotScripts() {
    auto &patternSlot = _patternSlots[_activePatternSlot];
    patternSlot.slotScriptLength = _state.scripts[SlotScriptIndex].l;
    patternSlot.metroLength = _state.scripts[METRO_SCRIPT].l;
    std::memcpy(patternSlot.slotScript.data(), _state.scripts[SlotScriptIndex].c, sizeof(patternSlot.slotScript));
    std::memcpy(patternSlot.metro.data(), _state.scripts[METRO_SCRIPT].c, sizeof(patternSlot.metro));
}

void TeletypeTrack::syncActiveSlotPatterns() {
    auto &patternSlot = _patternSlots[_activePatternSlot];
    for (int i = 0; i < PATTERN_COUNT; ++i) {
        patternSlot.patterns[i] = _patterns[i];
    }
}

void TeletypeTrack::syncActiveSlotMappings() {
    auto &patternSlot = _patternSlots[_activePatternSlot];
    patternSlot.triggerInputSource = _triggerInputSource;
    patternSlot.cvInSource = _cvInSource;
    patternSlot.cvParamSource = _cvParamSource;
    patternSlot.cvXSource = _cvXSource;
    patternSlot.cvYSource = _cvYSource;
    patternSlot.cvZSource = _cvZSource;
    patternSlot.triggerOutputDest = _triggerOutputDest;
    patternSlot.cvOutputDest = _cvOutputDest;
    patternSlot.cvOutputRange = _cvOutputRange;
    patternSlot.cvOutputOffset = _cvOutputOffset;
    patternSlot.cvOutputQuantizeScale = _cvOutputQuantizeScale;
    patternSlot.cvOutputRootNote = _cvOutputRootNote;
    patternSlot.midiSource = _midiSource;
    patternSlot.bootScriptIndex = _bootScriptIndex;
    patternSlot.timeBase = _timeBase;
    patternSlot.clockDivisor = _clockDivisor;
    patternSlot.clockMultiplier = _clockMultiplier;
    patternSlot.resetMetroOnLoad = _resetMetroOnLoad;
}
