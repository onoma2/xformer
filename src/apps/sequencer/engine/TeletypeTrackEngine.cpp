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
    _performerGateOutput.fill(false);
    _performerCvOutput.fill(0.f);
    _teletypeCvRaw.fill(0);
    _teletypeCvOffset.fill(0);
    _teletypePulseRemainingMs.fill(0.f);
    _teletypeInputState.fill(false);
    _teletypePrevInputState.fill(false);
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
    (void)slew;
    if (index >= CvOutputCount) {
        return;
    }

    // Map TO-CV1-4 to actual CV output
    auto dest = _teletypeTrack.cvOutputDest(index);
    int destIndex = int(dest);  // CvOut1=0, CvOut2=1, etc

    if (destIndex < 0 || destIndex >= PerformerCvCount) {
        return;
    }

    int32_t rawValue = value + _teletypeCvOffset[index];
    int16_t raw = static_cast<int16_t>(clamp<int32_t>(rawValue, 0, 16383));
    _teletypeCvRaw[index] = raw;
    _performerCvOutput[destIndex] = rawToVolts(raw);
    _activity = true;
    _activityCountdownMs = kActivityHoldMs;
}

void TeletypeTrackEngine::setCvSlew(uint8_t index, int16_t value) {
    (void)index;
    (void)value;
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
    for (uint8_t i = 0; i < TriggerInputCount; ++i) {
        _teletypeInputState[i] = inputState(i);
        if (_teletypeInputState[i] && !_teletypePrevInputState[i]) {
            runScript(i);
        }
        _teletypePrevInputState[i] = _teletypeInputState[i];
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
    for (size_t i = 0; i < _teletypePulseRemainingMs.size(); ++i) {
        if (_teletypePulseRemainingMs[i] <= 0.f) {
            continue;
        }
        _teletypePulseRemainingMs[i] -= dtMs;
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

void TeletypeTrackEngine::installTestScripts() {
    struct ScriptDef {
        uint8_t index;
        const char *lines[3];
        size_t lineCount;
    };
    constexpr ScriptDef scripts[kManualScriptCount] = {
        { 0,
          { "EVERY 2: TR.PULSE 1 ; CV 1 N 24",
            nullptr,
            nullptr },
          1 },
        { 1,
          { "EVERY 3: TR.PULSE 2 ; CV 2 N 36",
            nullptr,
            nullptr },
          1 },
        { 2,
          { "EVERY 4: TR.PULSE 3 ; CV 3 N 48",
            nullptr,
            nullptr },
          1 },
        { 3,
          { "EVERY 5: TR.PULSE 4 ; CV 4 N 60",
            nullptr,
            nullptr },
          1 },
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
