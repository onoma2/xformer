#include "TeletypeTrackEngine.h"

#include "TeletypeBridge.h"

#include "Engine.h"

#include "core/Debug.h"
#include "core/math/Math.h"

#include <algorithm>
#include <cmath>

extern "C" {
#include "teletype.h"
}

namespace {
constexpr float kActivityHoldMs = 200.f;
constexpr uint8_t kManualScriptCount = 4;
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
    _metroRemainingMs = 0.f;
    _metroPeriodMs = 0;
    _metroActive = false;
    _gateOutput.fill(false);
    _cvOutput.fill(0.f);
    _cvRaw.fill(0);
    _cvOffset.fill(0);
    _pulseRemainingMs.fill(0.f);
    _inputState.fill(false);
    _prevInputState.fill(false);
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
    TeletypeBridge::ScopedEngine scope(*this);
    advanceTime(dt);
    updateInputTriggers();
    runMetro(dt);
    updatePulses(dt);
    refreshActivity(dt);
}

void TeletypeTrackEngine::handleTr(uint8_t index, int16_t value) {
    if (index >= GateCount) {
        return;
    }
    bool next = value != 0;
    if (_gateOutput[index] != next) {
        _gateOutput[index] = next;
        _activity = true;
        _activityCountdownMs = kActivityHoldMs;
    }
}

void TeletypeTrackEngine::beginPulse(uint8_t index, int16_t timeMs) {
    if (index >= GateCount || timeMs <= 0) {
        return;
    }
    _pulseRemainingMs[index] = timeMs;
}

void TeletypeTrackEngine::clearPulse(uint8_t index) {
    if (index >= GateCount) {
        return;
    }
    _pulseRemainingMs[index] = 0.f;
}

void TeletypeTrackEngine::setPulseTime(uint8_t index, int16_t timeMs) {
    if (index >= GateCount || timeMs <= 0) {
        return;
    }
    if (_pulseRemainingMs[index] > 0.f) {
        _pulseRemainingMs[index] = timeMs;
    }
}

void TeletypeTrackEngine::handleCv(uint8_t index, int16_t value, bool slew) {
    (void)slew;
    if (index >= CvCount) {
        return;
    }
    int32_t rawValue = value + _cvOffset[index];
    int16_t raw = static_cast<int16_t>(clamp<int32_t>(rawValue, 0, 16383));
    _cvRaw[index] = raw;
    _cvOutput[index] = rawToVolts(raw);
    _activity = true;
    _activityCountdownMs = kActivityHoldMs;
}

void TeletypeTrackEngine::setCvSlew(uint8_t index, int16_t value) {
    (void)index;
    (void)value;
}

void TeletypeTrackEngine::setCvOffset(uint8_t index, int16_t value) {
    if (index >= CvCount) {
        return;
    }
    _cvOffset[index] = value;
}

uint16_t TeletypeTrackEngine::cvRaw(uint8_t index) const {
    if (index >= CvCount) {
        return 0;
    }
    return _cvRaw[index];
}

void TeletypeTrackEngine::updateAdc(bool force) {
    (void)force;
    scene_state_t &state = _teletypeTrack.state();
    float inVoltage = 0.f;
    float paramVoltage = 0.f;
    if (CvInput::Channels > 0) {
        inVoltage = _engine.cvInput().channel(0);
    }
    if (CvInput::Channels > 1) {
        paramVoltage = _engine.cvInput().channel(1);
    }
    ss_set_in(&state, voltsToRaw(inVoltage));
    ss_set_param(&state, voltsToRaw(paramVoltage));
}

bool TeletypeTrackEngine::inputState(uint8_t index) const {
    if (index >= GateCount) {
        return false;
    }
    uint8_t gates = _engine.gateOutput();
    return (gates >> (index + 4)) & 0x1;
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
    if (period < METRO_MIN_UNSUPPORTED_MS) {
        period = METRO_MIN_UNSUPPORTED_MS;
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

void TeletypeTrackEngine::installBootScript() {
    scene_state_t &state = _teletypeTrack.state();
    installTestScripts();
    ss_clear_script(&state, METRO_SCRIPT);
    state.variables.m_act = 0;
}

void TeletypeTrackEngine::runBootScript() {
    TeletypeBridge::ScopedEngine scope(*this);
    run_script(&_teletypeTrack.state(), 0);
    _activity = true;
    _activityCountdownMs = kActivityHoldMs;
}

void TeletypeTrackEngine::advanceTime(float dt) {
    _tickRemainderMs += dt * 1000.f;
    while (_tickRemainderMs >= 1.f) {
        float step = std::min(_tickRemainderMs, 255.f);
        tele_tick(&_teletypeTrack.state(), static_cast<uint8_t>(step));
        _tickRemainderMs -= step;
    }
}

void TeletypeTrackEngine::updateInputTriggers() {
    for (uint8_t i = 0; i < GateCount; ++i) {
        _inputState[i] = inputState(i);
        if (_inputState[i] && !_prevInputState[i]) {
            runScript(i);
        }
        _prevInputState[i] = _inputState[i];
    }
}

void TeletypeTrackEngine::runMetro(float dt) {
    scene_state_t &state = _teletypeTrack.state();
    bool active = state.variables.m_act;
    if (!active) {
        _metroActive = false;
        _metroRemainingMs = 0.f;
        return;
    }

    int16_t period = state.variables.m;
    if (period < METRO_MIN_UNSUPPORTED_MS) {
        period = METRO_MIN_UNSUPPORTED_MS;
    }

    if (_metroPeriodMs != period || !_metroActive) {
        _metroPeriodMs = period;
        _metroRemainingMs = _metroPeriodMs;
        _metroActive = true;
    }

    if (ss_get_script_len(&state, METRO_SCRIPT) == 0) {
        return;
    }

    _metroRemainingMs -= dt * 1000.f;
    while (_metroRemainingMs <= 0.f) {
        run_script(&state, METRO_SCRIPT);
        _metroRemainingMs += _metroPeriodMs;
        _activity = true;
        _activityCountdownMs = kActivityHoldMs;
    }
}

void TeletypeTrackEngine::updatePulses(float dt) {
    float dtMs = dt * 1000.f;
    for (size_t i = 0; i < _pulseRemainingMs.size(); ++i) {
        if (_pulseRemainingMs[i] <= 0.f) {
            continue;
        }
        _pulseRemainingMs[i] -= dtMs;
        if (_pulseRemainingMs[i] <= 0.f) {
            _pulseRemainingMs[i] = 0.f;
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

void TeletypeTrackEngine::installTestScripts() {
    struct ScriptDef {
        uint8_t index;
        const char *lines[3];
        size_t lineCount;
    };
    constexpr ScriptDef scripts[kManualScriptCount] = {
        { 0,
          { "EVERY 2: A FLIP",
            "CV + 1 A N 24 ; TR.PULSE + 1 A",
            nullptr },
          2 },
        { 1,
          { "EVERY 3: B FLIP",
            "CV + 3 B N 36 ; TR.PULSE + 3 B",
            nullptr },
          2 },
        { 2,
          { "EVERY 4: C FLIP",
            "CV + 1 C N 48 ; TR.PULSE + 1 C",
            "DEL 100: CV + 1 C N 60 ; TR.PULSE + 1 C" },
          3 },
        { 3,
          { "EVERY 5: D FLIP",
            "CV + 3 D N 60 ; TR.PULSE + 3 D",
            "DEL 200: CV + 3 D N 72 ; TR.PULSE + 3 D" },
          3 },
    };

    scene_state_t &state = _teletypeTrack.state();
    for (const auto &script : scripts) {
        ss_clear_script(&state, script.index);
        for (size_t line = 0; line < script.lineCount; ++line) {
            const char *cmd = script.lines[line];
            if (!cmd) {
                continue;
            }
            tele_command_t parsed = {};
            char errorMsg[TELE_ERROR_MSG_LENGTH] = {};
            tele_error_t error = parse(cmd, &parsed, errorMsg);
            if (error != E_OK) {
                DBG("TT parse error: %s (%s)", tele_error(error), errorMsg);
                continue;
            }
            error = validate(&parsed, errorMsg);
            if (error != E_OK) {
                DBG("TT validate error: %s (%s)", tele_error(error), errorMsg);
                continue;
            }
            ss_overwrite_script_command(&state, script.index, line, &parsed);
        }
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
