#include "TeletypeTrack.h"

extern "C" {
#include "teletype.h"
}

void TeletypeTrack::clear() {
    ss_init(&_state);

    _midiSource.clear();  // Default to MIDI port, Omni channel

    // Default I/O mapping
    // TI-TR1-4 → None (avoid cross-triggering by default)
    _triggerInputSource[0] = TriggerInputSource::None;
    _triggerInputSource[1] = TriggerInputSource::None;
    _triggerInputSource[2] = TriggerInputSource::None;
    _triggerInputSource[3] = TriggerInputSource::None;

    // TI-IN → CV input 1, TI-PARAM → CV input 2
    _cvInSource = CvInputSource::CvIn1;
    _cvParamSource = CvInputSource::CvIn2;
    _cvXSource = CvInputSource::None;
    _cvYSource = CvInputSource::None;
    _cvZSource = CvInputSource::None;

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

    // Script presets (S0-S3) default to presets 0-3
    for (int i = 0; i < ScriptSlotCount; ++i) {
        _scriptPresetIndex[i] = static_cast<uint8_t>(i);
    }
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
        _cvOutputQuantizeScale[i] = QuantizeOff;
        _cvOutputRootNote[i] = -1;
    }
    for (int i = 0; i < PATTERN_COUNT; ++i) {
        _patterns[i] = _state.patterns[i];
    }
}

void TeletypeTrack::gateOutputName(int index, StringBuilder &str) const {
    str("TT G%d", (index % 4) + 1);
}

void TeletypeTrack::cvOutputName(int index, StringBuilder &str) const {
    str("TT CV%d", (index % 4) + 1);
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
    for (int script = 0; script < EditableScriptCount; ++script) {
        for (int line = 0; line < ScriptLineCount; ++line) {
            char lineBuffer[ScriptLineLength] = {};
            const tele_command_t *cmd = ss_get_script_command(const_cast<scene_state_t *>(&_state), script, line);
            if (cmd && cmd->length > 0) {
                print_command(cmd, lineBuffer);
            }
            writer.write(lineBuffer, ScriptLineLength);
        }
    }
    for (int pattern = 0; pattern < PATTERN_COUNT; ++pattern) {
        writer.write(_patterns[pattern]);
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
    char lineBuffer[ScriptLineLength] = {};
    for (int script = 0; script < EditableScriptCount; ++script) {
        ss_clear_script(&_state, script);
        for (int line = 0; line < ScriptLineCount; ++line) {
            reader.read(lineBuffer, ScriptLineLength, 0);
            lineBuffer[ScriptLineLength - 1] = '\0';
            if (lineBuffer[0] == '\0') {
                continue;
            }
            tele_command_t cmd = {};
            char errorMsg[TELE_ERROR_MSG_LENGTH] = {};
            tele_error_t error = parse(lineBuffer, &cmd, errorMsg);
            if (error != E_OK) {
                continue;
            }
            error = validate(&cmd, errorMsg);
            if (error != E_OK) {
                continue;
            }
            ss_overwrite_script_command(&_state, script, line, &cmd);
        }
    }
    for (int pattern = 0; pattern < PATTERN_COUNT; ++pattern) {
        reader.read(_patterns[pattern], 0);
    }
    _resetMetroOnLoad = true;
}
