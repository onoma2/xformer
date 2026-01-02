#include "TeletypeTrackEngine.h"

#include "TeletypeBridge.h"

#include "Engine.h"
#include "Slide.h"

#include "core/Debug.h"
#include "core/math/Math.h"
#include "model/Types.h"

#include <algorithm>
#include <cmath>

extern "C" {
#include "teletype.h"
}

namespace {
constexpr float kActivityHoldMs = 200.f;
constexpr uint8_t kManualScriptCount = 4;
constexpr uint8_t kPresetScriptCount = TeletypeTrackEngine::PresetScriptCount;

struct PresetScript {
    const char *name;
    const char *lines[3];
    size_t lineCount;
};

constexpr PresetScript kPresetScripts[kPresetScriptCount] = {
    { "Test 1",
      { "TIME.ACT 1",
        "X LIM SUB MOD TIME 16384 8000 -1500 1500",
        "CV 1 ADD 8000 X ; TR.PULSE 1" },
      3 },
    { "Test 2",
      { "X LIM SUB MOD LAST 2 16384 8000 -1500 1500",
        "CV 2 ADD 8000 X ; TR.PULSE 2",
        nullptr },
      2 },
    { "Test 3",
      { "X LIM SUB MOD LAST 1 16384 8000 -1500 1500",
        "CV 3 ADD 8000 X ; TR.PULSE 3",
        nullptr },
      2 },
    { "Test 4",
      { "X LIM SUB MOD TIME 16384 8000 -1500 1500",
        "CV 4 ADD 8000 X ; TR.PULSE 4",
        nullptr },
      2 },
    { "Avg Clamp",
      { "X LIM SUB AVG IN PARAM 8000 -1500 1500",
        "CV 1 ADD 8000 X ; TR.PULSE 1",
        nullptr },
      2 },
    { "Param QT",
      { "X LIM SUB QT PARAM 1024 8000 -1500 1500",
        "CV 2 ADD 8000 X ; TR.PULSE 2",
        nullptr },
      2 },
    { "Shift Wrap",
      { "X LIM SUB WRAP LSH IN 1 0 16383 8000 -1500 1500",
        "CV 3 ADD 8000 X ; TR.PULSE 3",
        nullptr },
      2 },
    { "Rsh Mix",
      { "X LIM SUB DIV ADD RSH IN 1 PARAM 2 8000 -1500 1500",
        "CV 4 ADD 8000 X ; TR.PULSE 4",
        nullptr },
      2 },
    { "Time Mod",
      { "TIME.ACT 1",
        "X LIM SUB MOD TIME 16384 8000 -1500 1500",
        "CV 1 ADD 8000 X ; TR.PULSE 1" },
      3 },
    { "Rand/Drunk",
      { "X RAND 16383 ; Y RRAND 0 16383",
        "DRUNK 64",
        "Z LIM SUB LIM MUL SUB X PARAM 2 0 16383 8000 -1500 1500 ; CV 2 ADD 8000 Z ; TR.PULSE ADD 2 TOSS" },
      3 },
};
} // namespace

TeletypeTrackEngine::TeletypeTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine) :
    TrackEngine(engine, model, track, linkedTrackEngine),
    _teletypeTrack(track.teletypeTrack())
{
    reset();
}

void TeletypeTrackEngine::reset() {
    ss_init(&_teletypeTrack.state());
    _bootScriptPending = true;
    _activity = false;
    _activityCountdownMs = 0.f;
    _tickRemainderMs = 0.f;
    _clockRemainder = 0.0;
    _lastClockPos = 0.0;
    _clockPosValid = false;
    _clockTickCounter = 0;
    _metroRemainingMs = 0.f;
    _metroPeriodMs = 0;
    _metroActive = false;
    _performerGateOutput.fill(false);
    _performerCvOutput.fill(0.f);
    _teletypeCvRaw.fill(0);
    _teletypeCvOffset.fill(0);
    _teletypePulseRemainingMs.fill(0.f);
    _teletypeInputState.fill(false);
    _teletypePrevInputState.fill(false);
    _cvOutputTarget.fill(0.f);
    _cvSlewTime.fill(1);  // Match Teletype default: 1ms
    _cvSlewActive.fill(false);
    _manualScriptIndex = 0;
    installBootScript();
    syncMetroFromState();
}

void TeletypeTrackEngine::restart() {
    _bootScriptPending = true;
}

TrackEngine::TickResult TeletypeTrackEngine::tick(uint32_t tick) {
    (void)tick;
    if (_bootScriptPending) {
        runBootScript();
        _bootScriptPending = false;
    }
    return TrackEngine::TickResult::NoUpdate;
}

void TeletypeTrackEngine::update(float dt) {
    if (dt <= 0.f) {
        return;
    }

    if (_teletypeTrack.consumeScriptsDirty()) {
        loadScriptsFromModel();
    }

    // Apply CV slew using Performer's Slide mechanism
    for (uint8_t i = 0; i < CvOutputCount; ++i) {
        if (!_cvSlewActive[i] || _cvSlewTime[i] <= 0) {
            continue;
        }

        auto dest = _teletypeTrack.cvOutputDest(i);
        int destIndex = int(dest);

        if (destIndex < 0 || destIndex >= PerformerCvCount) {
            continue;
        }

        // Convert Teletype slew time (1-32767 ms) to Performer slide time (1-100)
        // Teletype uses LINEAR interpolation over time T milliseconds
        // Performer uses EXPONENTIAL smoothing: tau = (slideTime/100)^2 * 2
        // For exponential to reach ~99% in time T: slideTime ≈ sqrt(T)
        // Examples: 100ms→10, 1000ms→31.6, 10000ms→100
        int slideTime = clamp(static_cast<int>(std::sqrt(_cvSlewTime[i])), 1, 100);

        // Apply slew using Performer's existing mechanism
        float current = _performerCvOutput[destIndex];
        float slewed = Slide::applySlide(current, _cvOutputTarget[i], slideTime, dt);
        _performerCvOutput[destIndex] = slewed;

        // Check if we've reached target (within small epsilon)
        if (std::abs(slewed - _cvOutputTarget[i]) < 0.001f) {
            _performerCvOutput[destIndex] = _cvOutputTarget[i];
            _cvSlewActive[i] = false;
        }
    }

    TeletypeBridge::ScopedEngine scope(*this);
    float timeDelta = advanceTime(dt);
    updateInputTriggers();
    runMetro(timeDelta);
    updatePulses(timeDelta);
    refreshActivity(dt);
}

void TeletypeTrackEngine::handleTr(uint8_t index, int16_t value) {
    if (index >= TriggerOutputCount) {
        return;
    }

    // Map TO-TRA-D to actual gate output
    auto dest = _teletypeTrack.triggerOutputDest(index);
    int destIndex = int(dest);  // GateOut1=0, GateOut2=1, etc

    if (destIndex < 0 || destIndex >= PerformerGateCount) {
        return;
    }

    bool next = value != 0;
    if (_performerGateOutput[destIndex] != next) {
        _performerGateOutput[destIndex] = next;
        _activity = true;
        _activityCountdownMs = kActivityHoldMs;
    }
}

void TeletypeTrackEngine::beginPulse(uint8_t index, int16_t timeMs) {
    if (index >= TriggerOutputCount || timeMs <= 0) {
        return;
    }
    _teletypePulseRemainingMs[index] = timeMs;
}

void TeletypeTrackEngine::clearPulse(uint8_t index) {
    if (index >= TriggerOutputCount) {
        return;
    }
    _teletypePulseRemainingMs[index] = 0.f;
}

void TeletypeTrackEngine::setPulseTime(uint8_t index, int16_t timeMs) {
    if (index >= TriggerOutputCount || timeMs <= 0) {
        return;
    }
    if (_teletypePulseRemainingMs[index] > 0.f) {
        _teletypePulseRemainingMs[index] = timeMs;
    }
}

void TeletypeTrackEngine::handleCv(uint8_t index, int16_t value, bool slew) {
    if (index >= CvOutputCount) {
        return;
    }

    // Calculate target voltage
    int32_t rawValue = value + _teletypeCvOffset[index];
    int16_t raw = static_cast<int16_t>(clamp<int32_t>(rawValue, 0, 16383));
    _teletypeCvRaw[index] = raw;
    float targetVoltage = rawToVolts(raw);
    const auto &baseRange = Types::voltageRangeInfo(Types::VoltageRange::Bipolar5V);
    const auto &outRange = Types::voltageRangeInfo(_teletypeTrack.cvOutputRange(index));
    float normalized = baseRange.normalize(targetVoltage);
    targetVoltage = outRange.denormalize(normalized);
    targetVoltage += _teletypeTrack.cvOutputOffsetVolts(index);
    targetVoltage = clamp(targetVoltage, -5.f, 5.f);

    // Map TO-CV1-4 to actual CV output
    auto dest = _teletypeTrack.cvOutputDest(index);
    int destIndex = int(dest);  // CvOut1=0, CvOut2=1, etc

    if (destIndex < 0 || destIndex >= PerformerCvCount) {
        return;
    }

    if (slew && _cvSlewTime[index] > 0) {
        // Slew to new value
        _cvOutputTarget[index] = targetVoltage;
        _cvSlewActive[index] = true;
    } else {
        // Snap immediately
        _cvOutputTarget[index] = targetVoltage;
        _performerCvOutput[destIndex] = targetVoltage;
        _cvSlewActive[index] = false;
    }

    _activity = true;
    _activityCountdownMs = kActivityHoldMs;
}

void TeletypeTrackEngine::setCvSlew(uint8_t index, int16_t value) {
    if (index >= CvOutputCount) {
        return;
    }
    // Teletype CV.SLEW range is 1-32767 ms
    _cvSlewTime[index] = clamp<int16_t>(value, 1, 32767);
}

void TeletypeTrackEngine::setCvOffset(uint8_t index, int16_t value) {
    if (index >= CvOutputCount) {
        return;
    }
    _teletypeCvOffset[index] = value;
}

uint16_t TeletypeTrackEngine::cvRaw(uint8_t index) const {
    if (index >= CvOutputCount) {
        return 0;
    }
    return _teletypeCvRaw[index];
}

void TeletypeTrackEngine::updateAdc(bool force) {
    (void)force;
    scene_state_t &state = _teletypeTrack.state();

    auto inSource = _teletypeTrack.cvInSource();
    auto paramSource = _teletypeTrack.cvParamSource();

    float inVoltage = 0.f;
    float paramVoltage = 0.f;

    // Read TI-IN from mapped source
    if (inSource >= TeletypeTrack::CvInputSource::CvIn1 &&
        inSource <= TeletypeTrack::CvInputSource::CvIn4) {
        // Physical CV input
        int channel = int(inSource) - int(TeletypeTrack::CvInputSource::CvIn1);
        if (channel < CvInput::Channels) {
            inVoltage = _engine.cvInput().channel(channel);
        }
    }
    // TODO: Add CV output readback support later
    // else if (inSource >= TeletypeTrack::CvInputSource::CvOut1 &&
    //          inSource <= TeletypeTrack::CvInputSource::CvOut8) {
    //     // Physical CV output (read back) - needs implementation
    // }

    // Read TI-PARAM from mapped source
    if (paramSource >= TeletypeTrack::CvInputSource::CvIn1 &&
        paramSource <= TeletypeTrack::CvInputSource::CvIn4) {
        // Physical CV input
        int channel = int(paramSource) - int(TeletypeTrack::CvInputSource::CvIn1);
        if (channel < CvInput::Channels) {
            paramVoltage = _engine.cvInput().channel(channel);
        }
    }
    // TODO: Add CV output readback support later
    // else if (paramSource >= TeletypeTrack::CvInputSource::CvOut1 &&
    //          paramSource <= TeletypeTrack::CvInputSource::CvOut8) {
    //     // Physical CV output (read back) - needs implementation
    // }

    ss_set_in(&state, voltsToRaw(inVoltage));
    ss_set_param(&state, voltsToRaw(paramVoltage));
}

bool TeletypeTrackEngine::inputState(uint8_t index) const {
    if (index >= TriggerInputCount) {  // Only 4 trigger inputs (TI-TR1-4)
        return false;
    }

    // Map TI-TR1-4 to physical source
    auto source = _teletypeTrack.triggerInputSource(index);

    // Check CV inputs (with threshold)
    if (source >= TeletypeTrack::TriggerInputSource::CvIn1 &&
        source <= TeletypeTrack::TriggerInputSource::CvIn4) {
        int channel = int(source) - int(TeletypeTrack::TriggerInputSource::CvIn1);
        if (channel < CvInput::Channels) {
            float voltage = _engine.cvInput().channel(channel);
            return voltage > 1.0f;  // Gate threshold at 1V
        }
        return false;
    }

    // Check gate outputs (read back)
    if (source >= TeletypeTrack::TriggerInputSource::GateOut1 &&
        source <= TeletypeTrack::TriggerInputSource::GateOut8) {
        int gateIndex = int(source) - int(TeletypeTrack::TriggerInputSource::GateOut1);
        uint8_t gates = _engine.gateOutput();
        return (gates >> gateIndex) & 0x1;
    }

    return false;
}

void TeletypeTrackEngine::triggerManualScript() {
    runScript(_manualScriptIndex);
}

void TeletypeTrackEngine::selectNextManualScript() {
    _manualScriptIndex = (_manualScriptIndex + 1) % kManualScriptCount;
}

void TeletypeTrackEngine::syncMetroFromState() {
    scene_state_t &state = _teletypeTrack.state();
    int16_t period = state.variables.m;
    int16_t minPeriod = _teletypeTrack.timeBase() == TeletypeTrack::TimeBase::Clock
                            ? 1
                            : METRO_MIN_UNSUPPORTED_MS;
    if (period < minPeriod) {
        period = minPeriod;
    }
    _metroPeriodMs = period;
    _metroActive = state.variables.m_act;
    if (_metroActive && _metroRemainingMs <= 0.f) {
        _metroRemainingMs = _metroPeriodMs;
    }
}

void TeletypeTrackEngine::resetMetroTimer() {
    if (_metroActive && _metroPeriodMs > 0) {
        _metroRemainingMs = _metroPeriodMs;
    }
}

uint32_t TeletypeTrackEngine::timeTicks() const {
    if (_teletypeTrack.timeBase() == TeletypeTrack::TimeBase::Clock) {
        return _clockTickCounter;
    }
    return TeletypeBridge::ticksMs();
}

void TeletypeTrackEngine::installBootScript() {
    scene_state_t &state = _teletypeTrack.state();
    if (_teletypeTrack.hasAnyScriptText()) {
        loadScriptsFromModel();
    } else {
        seedScriptsFromPresets();
    }
    state.variables.m_act = 0;
}

void TeletypeTrackEngine::runBootScript() {
    TeletypeBridge::ScopedEngine scope(*this);
    run_script(&_teletypeTrack.state(), 0);
    _activity = true;
    _activityCountdownMs = kActivityHoldMs;
}

float TeletypeTrackEngine::advanceTime(float dt) {
    if (_teletypeTrack.timeBase() == TeletypeTrack::TimeBase::Clock) {
        return advanceClockTime();
    }

    float deltaMs = dt * 1000.f;
    _tickRemainderMs += deltaMs;
    while (_tickRemainderMs >= 1.f) {
        float step = std::min(_tickRemainderMs, 255.f);
        tele_tick(&_teletypeTrack.state(), static_cast<uint8_t>(step));
        _tickRemainderMs -= step;
    }
    return deltaMs;
}

float TeletypeTrackEngine::advanceClockTime() {
    double tickPos = _engine.clock().tickPosition();
    double clockMult = _teletypeTrack.clockMultiplier() * 0.01;
    double divisor = _teletypeTrack.clockDivisor();
    double effectiveDivisor = std::max(1.0, divisor / clockMult);
    double telePos = tickPos / effectiveDivisor;

    if (!_clockPosValid) {
        _lastClockPos = telePos;
        _clockPosValid = true;
        return 0.f;
    }

    double delta = telePos - _lastClockPos;
    if (delta < 0.0) {
        delta = 0.0;
    }
    _lastClockPos = telePos;
    _clockRemainder += delta;

    while (_clockRemainder >= 1.0) {
        double step = std::min(_clockRemainder, 255.0);
        tele_tick(&_teletypeTrack.state(), static_cast<uint8_t>(step));
        _clockRemainder -= step;
        _clockTickCounter += static_cast<uint32_t>(step);
    }

    return static_cast<float>(delta);
}

void TeletypeTrackEngine::updateInputTriggers() {
    for (uint8_t i = 0; i < TriggerInputCount; ++i) {
        _teletypeInputState[i] = inputState(i);
        if (_teletypeInputState[i] && !_teletypePrevInputState[i]) {
            runScript(i);
        }
        _teletypePrevInputState[i] = _teletypeInputState[i];
    }
}

void TeletypeTrackEngine::runMetro(float timeDelta) {
    scene_state_t &state = _teletypeTrack.state();
    bool active = state.variables.m_act;
    if (!active) {
        _metroActive = false;
        _metroRemainingMs = 0.f;
        return;
    }

    int16_t period = state.variables.m;
    int16_t minPeriod = _teletypeTrack.timeBase() == TeletypeTrack::TimeBase::Clock
                            ? 1
                            : METRO_MIN_UNSUPPORTED_MS;
    if (period < minPeriod) {
        period = minPeriod;
    }

    if (_metroPeriodMs != period || !_metroActive) {
        _metroPeriodMs = period;
        _metroRemainingMs = _metroPeriodMs;
        _metroActive = true;
    }

    if (ss_get_script_len(&state, METRO_SCRIPT) == 0) {
        return;
    }

    _metroRemainingMs -= timeDelta;
    while (_metroRemainingMs <= 0.f) {
        run_script(&state, METRO_SCRIPT);
        _metroRemainingMs += _metroPeriodMs;
        _activity = true;
        _activityCountdownMs = kActivityHoldMs;
    }
}

void TeletypeTrackEngine::updatePulses(float timeDelta) {
    for (size_t i = 0; i < _teletypePulseRemainingMs.size(); ++i) {
        if (_teletypePulseRemainingMs[i] <= 0.f) {
            continue;
        }
        _teletypePulseRemainingMs[i] -= timeDelta;
        if (_teletypePulseRemainingMs[i] <= 0.f) {
            _teletypePulseRemainingMs[i] = 0.f;
            tele_tr_pulse_end(&_teletypeTrack.state(), static_cast<uint8_t>(i));
        }
    }
}

void TeletypeTrackEngine::refreshActivity(float dt) {
    if (_activityCountdownMs <= 0.f) {
        _activity = false;
        return;
    }
    _activityCountdownMs -= dt * 1000.f;
    if (_activityCountdownMs <= 0.f) {
        _activity = false;
    }
}

float TeletypeTrackEngine::rawToVolts(int16_t value) const {
    int16_t clamped = clamp<int16_t>(value, 0, 16383);
    float norm = clamped / 16383.f;
    return norm * 10.f - 5.f;
}

int16_t TeletypeTrackEngine::voltsToRaw(float volts) const {
    float clamped = clamp(volts, -5.f, 5.f);
    float norm = (clamped + 5.f) / 10.f;
    int32_t raw = static_cast<int32_t>(std::lround(norm * 16383.f));
    return static_cast<int16_t>(clamp<int32_t>(raw, 0, 16383));
}

float TeletypeTrackEngine::midiNoteToVolts(int note) {
    // Simple 1V/octave chromatic conversion
    // MIDI note 60 = middle C = 5V
    // Each semitone = 1/12V (83.33mV)
    // Range: approximately -10.58V (note -127) to +10.58V (note 127)
    return note / 12.0f;
}

void TeletypeTrackEngine::installPresetScripts() {
    for (int slot = 0; slot < kManualScriptCount; ++slot) {
        int presetIndex = _teletypeTrack.scriptPresetIndex(slot);
        applyPresetToScript(slot, presetIndex);
    }
}

void TeletypeTrackEngine::runScript(int scriptIndex) {
    if (scriptIndex < 0 || scriptIndex >= kManualScriptCount) {
        return;
    }
    scene_state_t &state = _teletypeTrack.state();
    if (ss_get_script_len(&state, scriptIndex) == 0) {
        return;
    }
    TeletypeBridge::ScopedEngine scope(*this);
    run_script(&state, static_cast<uint8_t>(scriptIndex));
    _activity = true;
    _activityCountdownMs = kActivityHoldMs;
}

const char *TeletypeTrackEngine::presetName(int presetIndex) {
    if (presetIndex < 0 || presetIndex >= kPresetScriptCount) {
        return "Unknown";
    }
    return kPresetScripts[presetIndex].name;
}

bool TeletypeTrackEngine::applyPresetLine(scene_state_t &state, int scriptIndex, size_t lineIndex, const char *cmd) {
    if (!cmd) {
        return false;
    }
    tele_command_t parsed = {};
    char errorMsg[TELE_ERROR_MSG_LENGTH] = {};
    tele_error_t error = parse(cmd, &parsed, errorMsg);
    if (error != E_OK) {
        DBG("TT parse error: %s (%s)", tele_error(error), errorMsg);
        return false;
    }
    error = validate(&parsed, errorMsg);
    if (error != E_OK) {
        DBG("TT validate error: %s (%s)", tele_error(error), errorMsg);
        return false;
    }
    ss_overwrite_script_command(&state, scriptIndex, lineIndex, &parsed);
    return true;
}

bool TeletypeTrackEngine::applyScriptLine(scene_state_t &state, int scriptIndex, size_t lineIndex, const char *cmd) {
    if (!cmd || cmd[0] == '\0') {
        return false;
    }
    tele_command_t parsed = {};
    char errorMsg[TELE_ERROR_MSG_LENGTH] = {};
    tele_error_t error = parse(cmd, &parsed, errorMsg);
    if (error != E_OK) {
        DBG("TT parse error: %s (%s)", tele_error(error), errorMsg);
        return false;
    }
    error = validate(&parsed, errorMsg);
    if (error != E_OK) {
        DBG("TT validate error: %s (%s)", tele_error(error), errorMsg);
        return false;
    }
    ss_overwrite_script_command(&state, scriptIndex, lineIndex, &parsed);
    return true;
}

void TeletypeTrackEngine::applyPresetToScript(int scriptIndex, int presetIndex) {
    if (scriptIndex < 0 || scriptIndex >= kManualScriptCount) {
        return;
    }
    if (presetIndex < 0 || presetIndex >= kPresetScriptCount) {
        return;
    }

    scene_state_t &state = _teletypeTrack.state();
    ss_clear_script(&state, scriptIndex);
    _teletypeTrack.clearScript(scriptIndex);

    const auto &preset = kPresetScripts[presetIndex];
    for (size_t line = 0; line < preset.lineCount; ++line) {
        const char *cmd = preset.lines[line];
        if (!cmd) {
            continue;
        }
        applyPresetLine(state, scriptIndex, line, cmd);
        _teletypeTrack.setScriptLine(scriptIndex, static_cast<int>(line), cmd);
    }
}

void TeletypeTrackEngine::loadScriptsFromModel() {
    scene_state_t &state = _teletypeTrack.state();
    for (int script = 0; script < TeletypeTrack::EditableScriptCount; ++script) {
        ss_clear_script(&state, script);
        for (int line = 0; line < TeletypeTrack::ScriptLineCount; ++line) {
            const char *text = _teletypeTrack.scriptLine(script, line);
            if (text[0] == '\0') {
                continue;
            }
            applyScriptLine(state, script, line, text);
        }
    }
    for (int pattern = 0; pattern < PATTERN_COUNT; ++pattern) {
        state.patterns[pattern] = _teletypeTrack.pattern(pattern);
    }
}

void TeletypeTrackEngine::seedScriptsFromPresets() {
    scene_state_t &state = _teletypeTrack.state();
    _teletypeTrack.clearScripts();
    installPresetScripts();
    ss_clear_script(&state, METRO_SCRIPT);
    _teletypeTrack.clearScript(METRO_SCRIPT);
}
