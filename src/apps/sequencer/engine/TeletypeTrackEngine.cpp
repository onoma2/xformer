#include "TeletypeTrackEngine.h"

#include "TeletypeBridge.h"

#include "Engine.h"
#include "NoteTrackEngine.h"
#include "Slide.h"

#include "core/Debug.h"
#include "core/math/Math.h"
#include "MidiUtils.h"
#include "model/Scale.h"
#include "model/Track.h"
#include "model/Types.h"

#include <algorithm>
#include <cmath>

extern "C" {
#include "teletype.h"
}

namespace {
constexpr float kActivityHoldMs = 200.f;
constexpr uint8_t kManualScriptCount = 4;
constexpr uint8_t kEnvIdle = 0;
constexpr uint8_t kEnvAttack = 1;
constexpr uint8_t kEnvDecay = 2;

const NoteSequence *noteSequenceForTrack(const Model &model, int trackIndex) {
    if (trackIndex < 0 || trackIndex >= CONFIG_TRACK_COUNT) {
        return nullptr;
    }
    const Track &track = model.project().track(trackIndex);
    if (track.trackMode() != Track::TrackMode::Note) {
        return nullptr;
    }
    int patternIndex = model.project().playState().trackState(trackIndex).pattern();
    if (patternIndex < 0 || patternIndex >= CONFIG_PATTERN_COUNT) {
        return nullptr;
    }
    return &track.noteTrack().sequence(patternIndex);
}

NoteSequence *noteSequenceForTrack(Model &model, int trackIndex) {
    if (trackIndex < 0 || trackIndex >= CONFIG_TRACK_COUNT) {
        return nullptr;
    }
    Track &track = model.project().track(trackIndex);
    if (track.trackMode() != Track::TrackMode::Note) {
        return nullptr;
    }
    int patternIndex = model.project().playState().trackState(trackIndex).pattern();
    if (patternIndex < 0 || patternIndex >= CONFIG_PATTERN_COUNT) {
        return nullptr;
    }
    return &track.noteTrack().sequence(patternIndex);
}
} // namespace

TeletypeTrackEngine::TeletypeTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine) :
    TrackEngine(engine, model, track, linkedTrackEngine),
    _teletypeTrack(track.teletypeTrack())
{
    reset();
}

void TeletypeTrackEngine::reset() {
    if (_initialized) {
        return;
    }
    _initialized = true;
    scene_state_t &state = _teletypeTrack.state();
    tele_command_t savedScripts[3][TeletypeTrack::ScriptLineCount] = {};
    uint8_t savedLengths[3] = {};
    for (int script = 0; script < 3; ++script) {
        savedLengths[script] = state.scripts[script].l;
        for (int line = 0; line < TeletypeTrack::ScriptLineCount; ++line) {
            savedScripts[script][line] = state.scripts[script].c[line];
        }
    }

    uint8_t prevMetroActive = state.variables.m_act;
    ss_init(&state);
    state.variables.m_act = prevMetroActive;
    for (int script = 0; script < 3; ++script) {
        state.scripts[script].l = savedLengths[script];
        for (int line = 0; line < TeletypeTrack::ScriptLineCount; ++line) {
            state.scripts[script].c[line] = savedScripts[script][line];
        }
    }
    _teletypeTrack.applyPatternSlot(_teletypeTrack.patternSlotForPattern(_trackState.pattern()));
    _bootScriptPending = false;
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
    _pulseClockMs = 0.f;
    _trDiv.fill(1);
    _trDivCounter.fill(0);
    _trWidthPct.fill(0);
    _trWidthEnabled.fill(false);
    _trLastPulseMs.fill(0.f);
    _trLastIntervalMs.fill(0.f);
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
    _envTargetRaw.fill(16383);
    _envOffsetRaw.fill(0);
    _envAttackMs.fill(50);
    _envDecayMs.fill(300);
    _envLoopSetting.fill(1);
    _envLoopsRemaining.fill(0);
    _envSlewMs.fill(0);
    _envStageRemainingMs.fill(0.f);
    _envEorTr.fill(-1);
    _envEocTr.fill(-1);
    _envState.fill(kEnvIdle);
    _lfoActive.fill(false);
    _lfoCycleMs.fill(1000);
    _lfoWave.fill(0);
    _lfoAmp.fill(100);
    _lfoFold.fill(0);
    _lfoOffsetRaw.fill(8192);
    _lfoPhase.fill(0.f);
    _manualScriptIndex = 0;
    _cachedPattern = -1;
    _geodeOutRouting.fill(-1);
    _geodeParams.clear();
    _geodeEngine.reset();
    _geodeActive = false;
    _geodeFreePhase = 0.f;
    _geodePhaseOffset = 0.f;
    _geodeBarSeconds = 2.f;
    _geodeWasClockRunning = false;
    installBootScript();
    syncMetroFromState();
}

void TeletypeTrackEngine::restart() {
    _bootScriptPending = false;
}

void TeletypeTrackEngine::runBootScriptNow() {
    if (_teletypeTrack.consumeMetroResetOnLoad()) {
        _teletypeTrack.state().variables.m_act = 0;
        syncMetroFromState();
    }
    _bootScriptPending = false;
    runBootScript();
}

void TeletypeTrackEngine::panic() {
    TeletypeBridge::ScopedEngine scope(*this);
    clear_delays(&_teletypeTrack.state());
    _teletypeTrack.state().variables.m_act = 0;
    syncMetroFromState();
    _lfoActive.fill(false);
    _envState.fill(kEnvIdle);
    _geodeActive = false;
    _geodeOutRouting.fill(-1);
    _geodeFreePhase = 0.f;
    _geodePhaseOffset = 0.f;
    for (uint8_t i = 0; i < TriggerOutputCount; ++i) {
        clearPulse(i);
        handleTr(i, 0);
    }
    for (uint8_t i = 0; i < CvOutputCount; ++i) {
        handleCv(i, 8192, false);
    }
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
    if (_teletypeTrack.consumeBootScriptRequest()) {
        runBootScriptNow();
    }
    _pulseClockMs += dt * 1000.f;

    const int currentPattern = pattern();
    if (currentPattern != _cachedPattern) {
        _teletypeTrack.onPatternChanged(currentPattern);
        _cachedPattern = currentPattern;
    }

    // Update tick duration from clock (for interpolation)
    if (_teletypeTrack.timeBase() == TeletypeTrack::TimeBase::Clock) {
        _tickDurationMs = static_cast<float>(_engine.clock().tickDuration() * 1000.0);
    } else {
        _tickDurationMs = 1.0f;  // 1ms timebase
    }

    // Advance CV interpolation phase and apply interpolation
    float dtMs = dt * 1000.0f;
    for (uint8_t i = 0; i < CvOutputCount; ++i) {
        if (!_cvInterpolationEnabled[i]) {
            continue;
        }

        // Advance phase (0.0 to 1.0 over one tick)
        _cvTickPhase[i] = std::min(1.0f, _cvTickPhase[i] + dtMs / _tickDurationMs);

        // Linear interpolation from previous to real target
        float interpolated = _prevCvOutputTarget[i] +
                             (_cvRealTarget[i] - _prevCvOutputTarget[i]) * _cvTickPhase[i];

        // Update _cvOutputTarget with interpolated value (used by slew/output logic)
        _cvOutputTarget[i] = interpolated;
    }

    updateAdc(false);

    // Apply CV slew using Performer's Slide mechanism
    for (uint8_t i = 0; i < CvOutputCount; ++i) {
        const bool envActive = _envState[i] != kEnvIdle;
        const int16_t slewMs = envActive ? _envSlewMs[i] : _cvSlewTime[i];
        if (!_cvSlewActive[i] || slewMs <= 0) {
            continue;
        }

        auto dest = _teletypeTrack.cvOutputDest(i);
        int destIndex = int(dest);
        int trackSlot = -1;
        {
            int slot = 0;
            const auto &cvOutputTracks = _model.project().cvOutputTracks();
            int trackIndex = _track.trackIndex();
            for (int channel = 0; channel < CONFIG_CHANNEL_COUNT; ++channel) {
                if (cvOutputTracks[channel] == trackIndex) {
                    if (channel == destIndex) {
                        trackSlot = slot;
                        break;
                    }
                    ++slot;
                }
            }
        }

        if (trackSlot < 0 || trackSlot >= PerformerCvCount) {
            continue;
        }

        // Convert Teletype slew time (1-32767 ms) to Performer slide time (1-100)
        // Teletype uses LINEAR interpolation over time T milliseconds
        // Performer uses EXPONENTIAL smoothing: tau = (slideTime/100)^2 * 2
        // For exponential to reach ~99% in time T: slideTime ≈ sqrt(T)
        // Examples: 100ms→10, 1000ms→31.6, 10000ms→100
        int slideTime = clamp(static_cast<int>(std::sqrt(slewMs)), 1, 100);

        // Apply slew using Performer's existing mechanism
        float current = _performerCvOutput[trackSlot];
        float slewed = Slide::applySlide(current, _cvOutputTarget[i], slideTime, dt);
        _performerCvOutput[trackSlot] = slewed;

        // Check if we've reached target (within small epsilon)
        if (std::abs(slewed - _cvOutputTarget[i]) < 0.001f) {
            _performerCvOutput[trackSlot] = _cvOutputTarget[i];
            _cvSlewActive[i] = false;
        }
    }

    TeletypeBridge::ScopedEngine scope(*this);
    float timeDelta = advanceTime(dt);
    updateInputTriggers();
    runMetro(timeDelta);
    updatePulses(timeDelta);
    if (timeDelta > 0.f) {
        for (uint8_t i = 0; i < CvOutputCount; ++i) {
            if (_envState[i] == kEnvIdle) {
                continue;
            }
            if (_envStageRemainingMs[i] <= 0.f) {
                continue;
            }
            _envStageRemainingMs[i] -= timeDelta;
            if (_envStageRemainingMs[i] > 0.f) {
                continue;
            }
            _envStageRemainingMs[i] = 0.f;
            _cvSlewActive[i] = false;

            auto dest = _teletypeTrack.cvOutputDest(i);
            int destIndex = int(dest);
            int trackSlot = -1;
            {
                int slot = 0;
                const auto &cvOutputTracks = _model.project().cvOutputTracks();
                int trackIndex = _track.trackIndex();
                for (int channel = 0; channel < CONFIG_CHANNEL_COUNT; ++channel) {
                    if (cvOutputTracks[channel] == trackIndex) {
                        if (channel == destIndex) {
                            trackSlot = slot;
                            break;
                        }
                        ++slot;
                    }
                }
            }

            if (trackSlot >= 0 && trackSlot < PerformerCvCount) {
                _performerCvOutput[trackSlot] = _cvOutputTarget[i];
            }
        }
    }
    updateEnvelopes();
    if (timeDelta > 0.f) {
        updateLfos(timeDelta);
    }
    updateGeode(dt);
    refreshActivity(dt);
}

void TeletypeTrackEngine::handleTr(uint8_t index, int16_t value) {
    if (index >= TriggerOutputCount) {
        return;
    }

    // Map TO-TRA-D to actual gate output
    auto dest = _teletypeTrack.triggerOutputDest(index);
    int destIndex = int(dest);  // GateOut1=0, GateOut2=1, etc
    int trackSlot = -1;
    {
        int slot = 0;
        const auto &gateOutputTracks = _model.project().gateOutputTracks();
        int trackIndex = _track.trackIndex();
        for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
            if (gateOutputTracks[i] == trackIndex) {
                if (i == destIndex) {
                    trackSlot = slot;
                    break;
                }
                ++slot;
            }
        }
    }

    if (trackSlot < 0 || trackSlot >= PerformerGateCount) {
        return;
    }

    bool next = value != 0;
    if (_performerGateOutput[trackSlot] != next) {
        _performerGateOutput[trackSlot] = next;
        _activity = true;
        _activityCountdownMs = kActivityHoldMs;
    }
}

void TeletypeTrackEngine::beginPulse(uint8_t index, int16_t timeMs) {
    if (index >= TriggerOutputCount || timeMs <= 0) {
        return;
    }
    float interval = _trLastIntervalMs[index];
    if (interval <= 0.f && _metroActive && _metroPeriodMs > 0) {
        interval = _metroPeriodMs * std::max<uint8_t>(1, _trDiv[index]);
    }
    if (_trLastPulseMs[index] > 0.f) {
        float elapsed = _pulseClockMs - _trLastPulseMs[index];
        if (elapsed > 0.f) {
            interval = elapsed;
        }
    }

    if (_trWidthEnabled[index]) {
        int16_t width = static_cast<int16_t>(std::lround(interval * (_trWidthPct[index] / 100.f)));
        if (width <= 0) {
            width = 1;
        }
        timeMs = width;
    }

    _teletypePulseRemainingMs[index] = timeMs;
    _trLastPulseMs[index] = _pulseClockMs;
    _trLastIntervalMs[index] = interval > 0.f ? interval : static_cast<float>(timeMs);
}

bool TeletypeTrackEngine::allowPulse(uint8_t index) {
    if (index >= TriggerOutputCount) {
        return false;
    }
    uint8_t div = _trDiv[index];
    if (div <= 1) {
        return true;
    }
    uint8_t counter = static_cast<uint8_t>(_trDivCounter[index] + 1);
    if (counter < div) {
        _trDivCounter[index] = counter;
        return false;
    }
    _trDivCounter[index] = 0;
    return true;
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

void TeletypeTrackEngine::setTrDiv(uint8_t index, int16_t div) {
    if (index >= TriggerOutputCount) {
        return;
    }
    if (div < 1) {
        div = 1;
    }
    _trDiv[index] = static_cast<uint8_t>(div);
    _trDivCounter[index] = 0;
}

void TeletypeTrackEngine::setTrWidth(uint8_t index, int16_t pct) {
    if (index >= TriggerOutputCount) {
        return;
    }
    if (pct <= 0) {
        _trWidthEnabled[index] = false;
        _trWidthPct[index] = 0;
        return;
    }
    if (pct > 100) {
        pct = 100;
    }
    _trWidthEnabled[index] = true;
    _trWidthPct[index] = static_cast<uint8_t>(pct);
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
    targetVoltage = applyCvQuantize(index, targetVoltage);

    // Map TO-CV1-4 to actual CV output
    auto dest = _teletypeTrack.cvOutputDest(index);
    int destIndex = int(dest);  // CvOut1=0, CvOut2=1, etc
    int trackSlot = -1;
    {
        int slot = 0;
        const auto &cvOutputTracks = _model.project().cvOutputTracks();
        int trackIndex = _track.trackIndex();
        for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
            if (cvOutputTracks[i] == trackIndex) {
                if (i == destIndex) {
                    trackSlot = slot;
                    break;
                }
                ++slot;
            }
        }
    }

    if (trackSlot < 0 || trackSlot >= PerformerCvCount) {
        return;
    }

    // Handle interpolation (if enabled)
    if (_cvInterpolationEnabled[index]) {
        _prevCvOutputTarget[index] = _cvOutputTarget[index];
        _cvRealTarget[index] = targetVoltage;
        _cvTickPhase[index] = 0.0f;  // Start new interpolation
        // _cvOutputTarget will be updated by interpolation logic in update()
    } else {
        _cvRealTarget[index] = targetVoltage;
    }

    if (slew && _cvSlewTime[index] > 0) {
        // Slew to new value
        if (!_cvInterpolationEnabled[index]) {
            _cvOutputTarget[index] = targetVoltage;
        }
        _cvSlewActive[index] = true;
    } else {
        // Snap immediately
        if (!_cvInterpolationEnabled[index]) {
            _cvOutputTarget[index] = targetVoltage;
            _performerCvOutput[trackSlot] = targetVoltage;
        }
        _cvSlewActive[index] = false;
    }

    _activity = true;
    _activityCountdownMs = kActivityHoldMs;
}

void TeletypeTrackEngine::setEnvTarget(uint8_t index, int16_t value) {
    if (index >= CvOutputCount) {
        return;
    }
    _envTargetRaw[index] = clamp<int16_t>(value, 0, 16383);
}

void TeletypeTrackEngine::setEnvAttack(uint8_t index, int16_t ms) {
    if (index >= CvOutputCount) {
        return;
    }
    _envAttackMs[index] = clamp<int16_t>(ms, 1, 32767);
}

void TeletypeTrackEngine::setEnvDecay(uint8_t index, int16_t ms) {
    if (index >= CvOutputCount) {
        return;
    }
    _envDecayMs[index] = clamp<int16_t>(ms, 1, 32767);
}

void TeletypeTrackEngine::setEnvOffset(uint8_t index, int16_t value) {
    if (index >= CvOutputCount) {
        return;
    }
    _envOffsetRaw[index] = clamp<int16_t>(value, 0, 16383);
}

void TeletypeTrackEngine::setEnvLoop(uint8_t index, int16_t count) {
    if (index >= CvOutputCount) {
        return;
    }
    _envLoopSetting[index] = clamp<int16_t>(count, 0, 127);
}

void TeletypeTrackEngine::setEnvEor(uint8_t index, int16_t tr) {
    if (index >= CvOutputCount) {
        return;
    }
    if (tr <= 0 || tr > TriggerOutputCount) {
        _envEorTr[index] = -1;
        return;
    }
    _envEorTr[index] = static_cast<int8_t>(tr - 1);
}

void TeletypeTrackEngine::setEnvEoc(uint8_t index, int16_t tr) {
    if (index >= CvOutputCount) {
        return;
    }
    if (tr <= 0 || tr > TriggerOutputCount) {
        _envEocTr[index] = -1;
        return;
    }
    _envEocTr[index] = static_cast<int8_t>(tr - 1);
}

void TeletypeTrackEngine::setLfoRate(uint8_t index, int16_t ms) {
    if (index >= CvOutputCount) {
        return;
    }
    if (ms <= 0) {
        _lfoCycleMs[index] = 0;
        return;
    }
    _lfoCycleMs[index] = clamp<int16_t>(ms, 20, 32767);
}

void TeletypeTrackEngine::setLfoWave(uint8_t index, int16_t value) {
    if (index >= CvOutputCount) {
        return;
    }
    _lfoWave[index] = clamp<int16_t>(value, 0, 16383);
}

void TeletypeTrackEngine::setLfoAmp(uint8_t index, int16_t value) {
    if (index >= CvOutputCount) {
        return;
    }
    if (value < 0) {
        value = 0;
    } else if (value > 100) {
        value = 100;
    }
    _lfoAmp[index] = static_cast<uint8_t>(value);
}

void TeletypeTrackEngine::setLfoFold(uint8_t index, int16_t value) {
    if (index >= CvOutputCount) {
        return;
    }
    if (value < 0) {
        value = 0;
    } else if (value > 100) {
        value = 100;
    }
    _lfoFold[index] = static_cast<uint8_t>(value);
}

void TeletypeTrackEngine::setLfoOffset(uint8_t index, int16_t value) {
    if (index >= CvOutputCount) {
        return;
    }
    _lfoOffsetRaw[index] = clamp<int16_t>(value, 0, 16383);
}

void TeletypeTrackEngine::setLfoStart(uint8_t index, int16_t state) {
    if (index >= CvOutputCount) {
        return;
    }
    if (state <= 0) {
        _lfoActive[index] = false;
        return;
    }
    if (!_lfoActive[index]) {
        // Only reset phase on fresh start, not repeated calls
        _lfoPhase[index] = 0.f;
    }
    _lfoActive[index] = true;
}

void TeletypeTrackEngine::setCvInterpolation(int index, bool enabled) {
    if (index < 0 || index >= CvOutputCount) {
        return;
    }
    _cvInterpolationEnabled[index] = enabled;
    if (enabled) {
        // Initialize interpolation state
        _cvRealTarget[index] = _cvOutputTarget[index];
        _prevCvOutputTarget[index] = _cvOutputTarget[index];
        _cvTickPhase[index] = 1.0f;  // Start at end of phase (stable)
    }
}

int16_t TeletypeTrackEngine::noteGateGet(int trackIndex, int stepIndex) const {
    const NoteSequence *sequence = noteSequenceForTrack(_model, trackIndex);
    if (!sequence) {
        return 0;
    }
    if (stepIndex < 0 || stepIndex >= CONFIG_STEP_COUNT) {
        return 0;
    }
    return sequence->step(stepIndex).gate() ? 1 : 0;
}

void TeletypeTrackEngine::noteGateSet(int trackIndex, int stepIndex, int16_t value) {
    NoteSequence *sequence = noteSequenceForTrack(_engine.model(), trackIndex);
    if (!sequence) {
        return;
    }
    if (stepIndex < 0 || stepIndex >= CONFIG_STEP_COUNT) {
        return;
    }
    sequence->step(stepIndex).setGate(value != 0);
}

int16_t TeletypeTrackEngine::noteNoteGet(int trackIndex, int stepIndex) const {
    const NoteSequence *sequence = noteSequenceForTrack(_model, trackIndex);
    if (!sequence) {
        return 0;
    }
    if (stepIndex < 0 || stepIndex >= CONFIG_STEP_COUNT) {
        return 0;
    }
    return sequence->step(stepIndex).note() - NoteSequence::Note::Min;
}

void TeletypeTrackEngine::noteNoteSet(int trackIndex, int stepIndex, int16_t value) {
    NoteSequence *sequence = noteSequenceForTrack(_engine.model(), trackIndex);
    if (!sequence) {
        return;
    }
    if (stepIndex < 0 || stepIndex >= CONFIG_STEP_COUNT) {
        return;
    }
    int16_t clamped = clamp<int16_t>(value, 0, NoteSequence::Note::Range - 1);
    sequence->step(stepIndex).setNote(clamped + NoteSequence::Note::Min);
}

int16_t TeletypeTrackEngine::noteGateHere(int trackIndex) const {
    if (trackIndex < 0 || trackIndex >= CONFIG_TRACK_COUNT) {
        return 0;
    }
    const TrackEngine &trackEngine = _engine.trackEngine(trackIndex);
    if (trackEngine.trackMode() != Track::TrackMode::Note) {
        return 0;
    }
    const auto &noteEngine = trackEngine.as<NoteTrackEngine>();
    int stepIndex = noteEngine.currentStep();
    if (stepIndex < 0 || stepIndex >= CONFIG_STEP_COUNT) {
        return 0;
    }
    const NoteSequence *sequence = noteSequenceForTrack(_model, trackIndex);
    if (!sequence) {
        return 0;
    }
    return sequence->step(stepIndex).gate() ? 1 : 0;
}

int16_t TeletypeTrackEngine::noteNoteHere(int trackIndex) const {
    if (trackIndex < 0 || trackIndex >= CONFIG_TRACK_COUNT) {
        return 0;
    }
    const TrackEngine &trackEngine = _engine.trackEngine(trackIndex);
    if (trackEngine.trackMode() != Track::TrackMode::Note) {
        return 0;
    }
    const auto &noteEngine = trackEngine.as<NoteTrackEngine>();
    int stepIndex = noteEngine.currentNoteStep();
    if (stepIndex < 0 || stepIndex >= CONFIG_STEP_COUNT) {
        return 0;
    }
    const NoteSequence *sequence = noteSequenceForTrack(_model, trackIndex);
    if (!sequence) {
        return 0;
    }
    return sequence->step(stepIndex).note() - NoteSequence::Note::Min;
}

void TeletypeTrackEngine::triggerEnv(uint8_t index) {
    if (index >= CvOutputCount) {
        return;
    }
    _envState[index] = kEnvAttack;
    _envSlewMs[index] = _envAttackMs[index];
    _envStageRemainingMs[index] = static_cast<float>(_envAttackMs[index]);
    int16_t loops = _envLoopSetting[index];
    _envLoopsRemaining[index] = loops == 0 ? -1 : loops;

    handleCv(index, _envOffsetRaw[index], false);
    handleCv(index, _envTargetRaw[index], true);
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

bool TeletypeTrackEngine::anyCvSlewActive() const {
    for (bool active : _cvSlewActive) {
        if (active) {
            return true;
        }
    }
    return false;
}

bool TeletypeTrackEngine::isTransportRunning() const {
    return _engine.clock().isRunning();
}

float TeletypeTrackEngine::routingSource(int index) const {
    return _engine.routingEngine().routeSource(index);
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
    auto xSource = _teletypeTrack.cvXSource();
    auto ySource = _teletypeTrack.cvYSource();
    auto zSource = _teletypeTrack.cvZSource();

    auto readCvSource = [this](TeletypeTrack::CvInputSource source, float &volts) -> bool {
        if (source == TeletypeTrack::CvInputSource::None) {
            return false;
        }
        if (source >= TeletypeTrack::CvInputSource::CvIn1 &&
            source <= TeletypeTrack::CvInputSource::CvIn4) {
            int channel = int(source) - int(TeletypeTrack::CvInputSource::CvIn1);
            if (channel < CvInput::Channels) {
                volts = _engine.cvInput().channel(channel);
                return true;
            }
            return false;
        }
        if (source >= TeletypeTrack::CvInputSource::CvOut1 &&
            source <= TeletypeTrack::CvInputSource::CvOut8) {
            int channel = int(source) - int(TeletypeTrack::CvInputSource::CvOut1);
            volts = _engine.cvOutput().channel(channel);
            return true;
        }
        if (source >= TeletypeTrack::CvInputSource::LogicalCv1 &&
            source <= TeletypeTrack::CvInputSource::LogicalCv8) {
            int trackIndex = int(source) - int(TeletypeTrack::CvInputSource::LogicalCv1);
            if (trackIndex >= 0 && trackIndex < CONFIG_TRACK_COUNT) {
                volts = _engine.trackEngine(trackIndex).cvOutput(0);
                return true;
            }
        }
        return false;
    };

    float inVoltage = 0.f;
    float paramVoltage = 0.f;

    // Read TI-IN from mapped source
    readCvSource(inSource, inVoltage);

    // Read TI-PARAM from mapped source
    readCvSource(paramSource, paramVoltage);

    ss_set_in(&state, voltsToRaw(inVoltage));
    ss_set_param(&state, voltsToRaw(paramVoltage));

    float xVoltage = 0.f;
    if (readCvSource(xSource, xVoltage)) {
        state.variables.x = voltsToRaw(xVoltage);
    }
    float yVoltage = 0.f;
    if (readCvSource(ySource, yVoltage)) {
        state.variables.y = voltsToRaw(yVoltage);
    }
    float zVoltage = 0.f;
    if (readCvSource(zSource, zVoltage)) {
        state.variables.z = voltsToRaw(zVoltage);
    }
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

    if (source >= TeletypeTrack::TriggerInputSource::LogicalGate1 &&
        source <= TeletypeTrack::TriggerInputSource::LogicalGate8) {
        int trackIndex = int(source) - int(TeletypeTrack::TriggerInputSource::LogicalGate1);
        if (trackIndex >= 0 && trackIndex < CONFIG_TRACK_COUNT) {
            return _engine.trackEngine(trackIndex).gateOutput(0);
        }
    }

    return false;
}

void TeletypeTrackEngine::triggerManualScript() {
    runScript(_manualScriptIndex);
}

void TeletypeTrackEngine::selectNextManualScript() {
    _manualScriptIndex = (_manualScriptIndex + 1) % kManualScriptCount;
}

void TeletypeTrackEngine::triggerScript(int scriptIndex) {
    runScript(scriptIndex);
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

void TeletypeTrackEngine::setMetroPeriod(int16_t periodMs) {
    _teletypeTrack.state().variables.m = periodMs;
    syncMetroFromState();
}

void TeletypeTrackEngine::setMetroActive(bool active) {
    _teletypeTrack.state().variables.m_act = active;
    syncMetroFromState();
}

uint32_t TeletypeTrackEngine::timeTicks() const {
    if (_teletypeTrack.timeBase() == TeletypeTrack::TimeBase::Clock) {
        return _clockTickCounter;
    }
    return TeletypeBridge::ticksMs();
}

float TeletypeTrackEngine::busCv(int index) const {
    return _engine.busCv(index);
}

void TeletypeTrackEngine::setBusCv(int index, float volts) {
    _engine.setBusCv(index, volts);
}

float TeletypeTrackEngine::measureFraction() const {
    return _engine.measureFraction();
}

float TeletypeTrackEngine::measureFractionBars(uint8_t bars) const {
    if (bars <= 1) {
        return measureFraction();
    }
    uint32_t divisor = _engine.measureDivisor();
    uint32_t span = divisor * static_cast<uint32_t>(bars);
    if (span == 0) {
        return 0.f;
    }
    uint32_t tick = _engine.tick();
    return float(tick % span) / span;
}

int TeletypeTrackEngine::trackPattern(int trackIndex) const {
    if (trackIndex < 0 || trackIndex >= CONFIG_TRACK_COUNT) {
        return 0;
    }
    return _engine.trackEngine(trackIndex).pattern();
}

float TeletypeTrackEngine::tempo() const {
    return _engine.tempo();
}

void TeletypeTrackEngine::showMessage(const char *text) {
    _engine.showMessage(text);
}

void TeletypeTrackEngine::setTempo(float bpm) {
    _engine.setTempo(bpm);
}

void TeletypeTrackEngine::setTrackPattern(int trackIndex, int patternIndex) {
    if (trackIndex < 0 || trackIndex >= CONFIG_TRACK_COUNT) {
        return;
    }
    if (patternIndex < 0 || patternIndex >= CONFIG_PATTERN_COUNT) {
        return;
    }
    _engine.selectTrackPattern(trackIndex, patternIndex);
}

void TeletypeTrackEngine::setTransportRunning(int16_t state) {
    if (state == 1) {
        if (!_engine.clockRunning()) {
            _engine.clockStart();
        }
    } else if (state == 0) {
        if (_engine.clockRunning()) {
            // Use reset to mirror start/stop behavior; switch to clockStop() for pause semantics.
            _engine.clockReset();
        }
    } else {
        if (_engine.clockRunning()) {
            _engine.clockStop();
        }
    }
}

void TeletypeTrackEngine::setMetroAllPeriod(int16_t periodMs) {
    _engine.setTeletypeMetroAll(periodMs);
}

void TeletypeTrackEngine::setMetroAllActive(bool active) {
    _engine.setTeletypeMetroActiveAll(active);
}

void TeletypeTrackEngine::resetMetroAll() {
    _engine.resetTeletypeMetroAll();
}

void TeletypeTrackEngine::installBootScript() {
    scene_state_t &state = _teletypeTrack.state();
    loadScriptsFromModel();
    if (!_teletypeTrack.hasAnyScriptCommands()) {
        seedScriptsFromPresets();
    }
    (void)state;
}

void TeletypeTrackEngine::runBootScript() {
    TeletypeBridge::ScopedEngine scope(*this);
    run_script(&_teletypeTrack.state(), static_cast<uint8_t>(_teletypeTrack.bootScriptIndex()));
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
    double tickMs = _engine.clock().tickDuration() * 1000.0;

    if (!_clockPosValid) {
        _lastClockPos = tickPos;
        _clockPosValid = true;
        return 0.f;
    }

    double delta = tickPos - _lastClockPos;
    if (delta < 0.0) {
        delta = 0.0;
    }
    _lastClockPos = tickPos;
    double deltaMs = delta * tickMs;
    _clockRemainder += deltaMs;

    while (_clockRemainder >= 1.0) {
        double step = std::min(_clockRemainder, 255.0);
        tele_tick(&_teletypeTrack.state(), static_cast<uint8_t>(step));
        _clockRemainder -= step;
        _clockTickCounter += static_cast<uint32_t>(step);
    }

    return static_cast<float>(deltaMs);
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
    int16_t minPeriod = METRO_MIN_UNSUPPORTED_MS;
    if (_teletypeTrack.timeBase() == TeletypeTrack::TimeBase::Clock) {
        double bpm = _engine.clock().bpm();
        double clockMult = _teletypeTrack.clockMultiplier() * 0.01;
        double divisor = _teletypeTrack.clockDivisor();
        double beatMs = bpm > 0.0 ? 60000.0 / bpm : 0.0;
        double periodMs = beatMs * (divisor / double(CONFIG_SEQUENCE_PPQN)) / std::max(0.01, clockMult);
        int16_t derived = clamp<int16_t>(static_cast<int16_t>(std::lround(periodMs)), 1, 32767);
        state.variables.m = derived;
        period = derived;
        minPeriod = 1;
    }
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

void TeletypeTrackEngine::updateEnvelopes() {
    scene_state_t &state = _teletypeTrack.state();
    for (uint8_t i = 0; i < CvOutputCount; ++i) {
        if (_envState[i] == kEnvIdle) {
            continue;
        }
        if (_envStageRemainingMs[i] > 0.f) {
            continue;
        }

        if (_envState[i] == kEnvAttack) {
            const int8_t eor = _envEorTr[i];
            if (eor >= 0) {
                int16_t timeMs = state.variables.tr_time[eor];
                if (timeMs > 0) {
                    state.variables.tr[eor] = state.variables.tr_pol[eor];
                    handleTr(static_cast<uint8_t>(eor), state.variables.tr[eor]);
                    beginPulse(static_cast<uint8_t>(eor), timeMs);
                }
            }
            _envState[i] = kEnvDecay;
            _envSlewMs[i] = _envDecayMs[i];
            _envStageRemainingMs[i] = static_cast<float>(_envDecayMs[i]);
            handleCv(i, _envOffsetRaw[i], true);
            continue;
        }

        if (_envState[i] == kEnvDecay) {
            const int8_t eoc = _envEocTr[i];
            if (eoc >= 0) {
                int16_t timeMs = state.variables.tr_time[eoc];
                if (timeMs > 0) {
                    state.variables.tr[eoc] = state.variables.tr_pol[eoc];
                    handleTr(static_cast<uint8_t>(eoc), state.variables.tr[eoc]);
                    beginPulse(static_cast<uint8_t>(eoc), timeMs);
                }
            }

            if (_envLoopsRemaining[i] < 0) {
                _envState[i] = kEnvAttack;
                _envSlewMs[i] = _envAttackMs[i];
                _envStageRemainingMs[i] = static_cast<float>(_envAttackMs[i]);
                handleCv(i, _envTargetRaw[i], true);
                continue;
            }

            if (_envLoopsRemaining[i] > 0) {
                _envLoopsRemaining[i] -= 1;
            }
            if (_envLoopsRemaining[i] > 0) {
                _envState[i] = kEnvAttack;
                _envSlewMs[i] = _envAttackMs[i];
                _envStageRemainingMs[i] = static_cast<float>(_envAttackMs[i]);
                handleCv(i, _envTargetRaw[i], true);
            } else {
                _envState[i] = kEnvIdle;
                _envStageRemainingMs[i] = 0.f;
            }
        }
    }
}

void TeletypeTrackEngine::updateLfos(float timeDeltaMs) {
    if (timeDeltaMs <= 0.f) {
        return;
    }

    for (uint8_t i = 0; i < CvOutputCount; ++i) {
        if (!_lfoActive[i]) {
            continue;
        }
        const int16_t cycleMs = _lfoCycleMs[i];
        if (cycleMs <= 0) {
            continue;
        }

        float phase = _lfoPhase[i] + (timeDeltaMs / static_cast<float>(cycleMs));
        if (phase >= 1.f) {
            phase -= std::floor(phase);
        }
        _lfoPhase[i] = phase;

        // Waveform generation
        float wave = 0.f;
        const int16_t waveParam = _lfoWave[i];

        if (waveParam <= 12287) {
            // Sections 1-3: Skewed triangle (saw → triangle → ramp)
            // Peak position: 100% → 50% → 0%
            float peakPos = 1.f - (waveParam / 12287.f);

            if (phase < peakPos) {
                // Rising portion: -1 to +1
                if (peakPos > 0.f) {
                    wave = (phase / peakPos) * 2.f - 1.f;
                } else {
                    wave = 1.f;  // Already at peak
                }
            } else {
                // Falling portion: +1 to -1
                float fallRange = 1.f - peakPos;
                if (fallRange > 0.f) {
                    wave = 1.f - ((phase - peakPos) / fallRange) * 2.f;
                } else {
                    wave = 1.f;  // Stay at peak
                }
            }
        } else {
            // Section 4: Pulse (50% → 5% duty cycle)
            float t = (waveParam - 12288) / (16383.f - 12288.f);
            float duty = 0.5f - (0.5f - 0.05f) * t;  // 0.5 → 0.05
            wave = phase < duty ? 1.f : -1.f;
        }

        // Apply amplitude
        float ampVolts = (_lfoAmp[i] / 100.f) * 5.f;
        float output = wave * ampVolts;

        // Apply fold
        float foldThreshold = -5.f + (_lfoFold[i] / 100.f) * 10.f;
        if (output < foldThreshold) {
            output = 2.f * foldThreshold - output;
        }

        // Apply offset and clamp once
        float offsetVolts = rawToVolts(_lfoOffsetRaw[i]);
        output += offsetVolts;
        output = clamp(output, -5.f, 5.f);

        int16_t raw = voltsToRaw(output);
        handleCv(i, raw, false);
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

float TeletypeTrackEngine::applyCvQuantize(int index, float volts) const {
    int scaleIndex = _teletypeTrack.cvOutputQuantizeScale(index);
    if (scaleIndex == TeletypeTrack::QuantizeOff) {
        return volts;
    }

    const Scale &scale = (scaleIndex < 0) ? _model.project().selectedScale()
                                          : Scale::get(scaleIndex);
    int rootNote = _teletypeTrack.cvOutputRootNote(index);
    if (rootNote < 0) {
        rootNote = _model.project().rootNote();
    }

    if (scale.isChromatic()) {
        volts -= rootNote * (1.f / 12.f);
    }

    int note = scale.noteFromVolts(volts);
    volts = scale.noteToVolts(note);

    if (scale.isChromatic()) {
        volts += rootNote * (1.f / 12.f);
    }

    return clamp(volts, -5.f, 5.f);
}

float TeletypeTrackEngine::midiNoteToVolts(int note) {
    // Simple 1V/octave chromatic conversion
    // MIDI note 60 = middle C = 5V
    // Each semitone = 1/12V (83.33mV)
    // Range: approximately -10.58V (note -127) to +10.58V (note 127)
    return note / 12.0f;
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

void TeletypeTrackEngine::loadScriptsFromModel() {
    _teletypeTrack.applyActivePatternSlot();
}

void TeletypeTrackEngine::seedScriptsFromPresets() {
    scene_state_t &state = _teletypeTrack.state();
    ss_clear_script(&state, 0);
    applyScriptLine(state, 0, 0, "M.ACT 1");
    ss_clear_script(&state, METRO_SCRIPT);
    applyScriptLine(state, METRO_SCRIPT, 0, "CV 1 N 48 ; TR.P 1");
}

// ====================================================================================
// Geode Engine Integration
// ====================================================================================

void TeletypeTrackEngine::setGeodeTime(int16_t value) {
    _geodeParams.time = clamp<int16_t>(value, 0, 16383);
}

void TeletypeTrackEngine::setGeodeIntone(int16_t value) {
    _geodeParams.intone = clamp<int16_t>(value, 0, 16383);
}

void TeletypeTrackEngine::setGeodeRamp(int16_t value) {
    _geodeParams.ramp = clamp<int16_t>(value, 0, 16383);
}

void TeletypeTrackEngine::setGeodeCurve(int16_t value) {
    _geodeParams.curve = clamp<int16_t>(value, 0, 16383);
}

void TeletypeTrackEngine::setGeodeRun(int16_t value) {
    _geodeParams.run = clamp<int16_t>(value, 0, 16383);
}

void TeletypeTrackEngine::setGeodeMode(int16_t value) {
    _geodeParams.mode = static_cast<uint8_t>(clamp<int16_t>(value, 0, 2));
}

void TeletypeTrackEngine::setGeodeOffset(int16_t value) {
    _geodeParams.offset = clamp<int16_t>(value, 0, 16383);
}

void TeletypeTrackEngine::setGeodeTune(uint8_t voiceIndex, int16_t numerator, int16_t denominator) {
    if (voiceIndex == 0) {
        for (int i = 0; i < GeodeEngine::VoiceCount; ++i) {
            _geodeParams.tuneNum[i] = numerator;
            _geodeParams.tuneDen[i] = denominator;
            _geodeEngine.setVoiceTune(i, numerator, denominator);
        }
        return;
    }
    if (voiceIndex > GeodeEngine::VoiceCount) {
        return;
    }
    int index = voiceIndex - 1;
    _geodeParams.tuneNum[index] = numerator;
    _geodeParams.tuneDen[index] = denominator;
    _geodeEngine.setVoiceTune(index, numerator, denominator);
}

void TeletypeTrackEngine::setGeodeBars(int16_t bars) {
    _geodeParams.bars = clamp<int16_t>(bars, 1, 128);
}

void TeletypeTrackEngine::setGeodeOut(uint8_t cvIndex, int16_t voiceIndex) {
    if (cvIndex == 0 || cvIndex > CvOutputCount) {
        return;
    }
    int8_t mapping = -1;
    if (voiceIndex >= 0 && voiceIndex <= GeodeEngine::VoiceCount) {
        mapping = static_cast<int8_t>(voiceIndex);
    }
    _geodeOutRouting[cvIndex - 1] = mapping;
}

void TeletypeTrackEngine::triggerGeodeVoice(uint8_t voiceIndex, int16_t divs, int16_t repeats) {
    float time = normalizeUnipolar(_geodeParams.time);
    float intone = normalizeBipolar(_geodeParams.intone);
    float run = normalizeBipolar(_geodeParams.run);
    uint8_t mode = _geodeParams.mode;
    float mf = _geodeFreePhase;
    const bool clockRunning = _engine.clockRunning();
    const float bars = static_cast<float>(clamp<int16_t>(_geodeParams.bars, 1, 128));
    if (clockRunning) {
        float transportPhase = measureFractionBars(static_cast<uint8_t>(bars));
        if (!_geodeWasClockRunning) {
            float offset = _geodeFreePhase - transportPhase;
            _geodePhaseOffset = offset - std::floor(offset);
        }
        mf = transportPhase + _geodePhaseOffset;
        mf -= std::floor(mf);
        _geodeFreePhase = mf;
        double tickSec = _engine.clock().tickDuration();
        if (tickSec > 0.0) {
            _geodeBarSeconds = static_cast<float>(tickSec * _engine.measureDivisor());
        }
    }
    _geodeWasClockRunning = clockRunning;
    _geodeEngine.syncMeasureFraction(mf);

    if (voiceIndex == 0) {
        // Voice 0 = all voices
        _geodeEngine.triggerAllVoices(divs, repeats);
        for (int i = 0; i < GeodeEngine::VoiceCount; ++i) {
            _geodeEngine.setVoicePhase(i, std::fmod(mf * divs, 1.0f));
        }
        _geodeEngine.triggerImmediateAll(time, intone, run, mode);
    } else if (voiceIndex <= GeodeEngine::VoiceCount) {
        // Voice 1-6 → internal index 0-5
        int index = voiceIndex - 1;
        _geodeEngine.triggerVoice(index, divs, repeats);
        _geodeEngine.setVoicePhase(index, std::fmod(mf * divs, 1.0f));
        _geodeEngine.triggerImmediate(voiceIndex - 1, time, intone, run, mode);
    }
    _geodeActive = true;
}

int16_t TeletypeTrackEngine::getGeodeTime() const {
    return _geodeParams.time;
}

int16_t TeletypeTrackEngine::getGeodeIntone() const {
    return _geodeParams.intone;
}

int16_t TeletypeTrackEngine::getGeodeRamp() const {
    return _geodeParams.ramp;
}

int16_t TeletypeTrackEngine::getGeodeCurve() const {
    return _geodeParams.curve;
}

int16_t TeletypeTrackEngine::getGeodeRun() const {
    return _geodeParams.run;
}

int16_t TeletypeTrackEngine::getGeodeMode() const {
    return static_cast<int16_t>(_geodeParams.mode);
}

int16_t TeletypeTrackEngine::getGeodeOffset() const {
    return _geodeParams.offset;
}

int16_t TeletypeTrackEngine::getGeodeBars() const {
    return _geodeParams.bars;
}

int16_t TeletypeTrackEngine::getGeodeVal() const {
    return _geodeEngine.outputRaw(_geodeParams.offset);
}

int16_t TeletypeTrackEngine::getGeodeVoice(uint8_t voiceIndex) const {
    if (voiceIndex == 0 || voiceIndex > GeodeEngine::VoiceCount) {
        return 0;
    }
    // Voice 1-6 → internal index 0-5
    return _geodeEngine.voiceOutputRaw(voiceIndex - 1, _geodeParams.offset);
}

int16_t TeletypeTrackEngine::getGeodeTuneNumerator(uint8_t voiceIndex) const {
    if (voiceIndex == 0 || voiceIndex > GeodeEngine::VoiceCount) {
        return 1;
    }
    return _geodeParams.tuneNum[voiceIndex - 1];
}

int16_t TeletypeTrackEngine::getGeodeTuneDenominator(uint8_t voiceIndex) const {
    if (voiceIndex == 0 || voiceIndex > GeodeEngine::VoiceCount) {
        return 1;
    }
    return _geodeParams.tuneDen[voiceIndex - 1];
}

float TeletypeTrackEngine::normalizeUnipolar(int16_t value) const {
    // 0-16383 → 0.0-1.0
    return clamp<int16_t>(value, 0, 16383) / 16383.f;
}

float TeletypeTrackEngine::normalizeBipolar(int16_t value) const {
    // 0-16383 → -1.0 to +1.0 (8192 = center/zero)
    return (clamp<int16_t>(value, 0, 16383) - 8192) / 8191.f;
}

void TeletypeTrackEngine::updateGeode(float dt) {
    bool hasRouting = false;
    for (int i = 0; i < CvOutputCount; ++i) {
        if (_geodeOutRouting[i] >= 0) {
            hasRouting = true;
            break;
        }
    }
    if (!_geodeActive && !_geodeEngine.anyVoiceActive() && !hasRouting) {
        return;
    }

    // Normalize parameters for GeodeEngine
    float time = normalizeUnipolar(_geodeParams.time);
    float intone = normalizeBipolar(_geodeParams.intone);
    float ramp = normalizeUnipolar(_geodeParams.ramp);
    float curve = normalizeBipolar(_geodeParams.curve);
    float run = normalizeBipolar(_geodeParams.run);
    uint8_t mode = _geodeParams.mode;

    // Get measure fraction (transport-locked or freerun with cached bar length)
    float mf = 0.f;
    const bool clockRunning = _engine.clockRunning();
    const float bars = static_cast<float>(clamp<int16_t>(_geodeParams.bars, 1, 128));
    if (clockRunning) {
        float transportPhase = measureFractionBars(static_cast<uint8_t>(bars));
        if (!_geodeWasClockRunning) {
            float offset = _geodeFreePhase - transportPhase;
            _geodePhaseOffset = offset - std::floor(offset);
        }
        mf = transportPhase + _geodePhaseOffset;
        mf -= std::floor(mf);
        _geodeFreePhase = mf;
        double tickSec = _engine.clock().tickDuration();
        if (tickSec > 0.0) {
            _geodeBarSeconds = static_cast<float>(tickSec * _engine.measureDivisor());
        }
    } else {
        float barSeconds = _geodeBarSeconds > 0.f ? _geodeBarSeconds : 2.f;
        float periodSec = barSeconds * bars;
        if (periodSec <= 0.f) {
            periodSec = 2.f * bars;
        }
        _geodeFreePhase += dt / periodSec;
        _geodeFreePhase -= std::floor(_geodeFreePhase);
        mf = _geodeFreePhase;
    }
    _geodeWasClockRunning = clockRunning;

    // Update Geode engine
    _geodeEngine.update(dt, mf, time, intone, ramp, curve, run, mode);

    // Route Geode output to CVs (mix or per-voice)
    for (int i = 0; i < CvOutputCount; ++i) {
        int8_t mapping = _geodeOutRouting[i];
        if (mapping < 0) {
            continue;
        }
        int16_t raw = (mapping == 0)
            ? _geodeEngine.outputRaw(_geodeParams.offset)
            : _geodeEngine.voiceOutputRaw(mapping - 1, _geodeParams.offset);
        handleCv(static_cast<uint8_t>(i), raw, false);
    }

    // Deactivate if no voices remain active
    if (!_geodeEngine.anyVoiceActive()) {
        _geodeActive = false;
    }

    // Set activity indicator when Geode is running
    if (_geodeActive) {
        _activity = true;
        _activityCountdownMs = kActivityHoldMs;
    }
}

// ====================================================================================
// MIDI Integration
// ====================================================================================

void TeletypeTrackEngine::clearMidiMonitoring() {
    scene_state_t &state = _teletypeTrack.state();
    scene_midi_t &midi = state.midi;

    // Clear event counts
    midi.on_count = 0;
    midi.off_count = 0;
    midi.cc_count = 0;

    // Reset last event tracking
    midi.last_event_type = 0;
    midi.last_channel = 0;
    midi.last_note = 0;
    midi.last_velocity = 0;
    midi.last_controller = 0;
    midi.last_cc = 0;
}

bool TeletypeTrackEngine::receiveMidi(MidiPort port, const MidiMessage &message) {
    // Filter by MIDI source (port + channel)
    if (!MidiUtils::matchSource(port, message, _teletypeTrack.midiSource())) {
        return false;  // Not our source - allow other tracks to process
    }

    // Process the message (populate data + trigger script)
    processMidiMessage(message);

    // CRITICAL: Return true to consume message
    // - Prevents other tracks from seeing this message
    // - User controls which track gets which MIDI via source config
    // - Example: Track 1 set to "USB Ch 2" won't consume "MIDI Ch 1"
    return true;
}

void TeletypeTrackEngine::processMidiMessage(const MidiMessage &message) {
    // Clear buffers for fresh state
    clearMidiMonitoring();

    scene_state_t &state = _teletypeTrack.state();
    scene_midi_t &midi = state.midi;

    int8_t scriptToRun = -1;

    // Handle Note On
    if (message.isNoteOn() && message.velocity() > 0) {
        if (midi.on_count < MAX_MIDI_EVENTS) {
            midi.note_on[midi.on_count] = message.note();
            midi.note_vel[midi.on_count] = message.velocity();
            midi.on_channel[midi.on_count] = message.channel();
            midi.on_count++;
        }
        midi.last_event_type = 1;
        midi.last_channel = message.channel();
        midi.last_note = message.note();
        midi.last_velocity = message.velocity();

        // Check if script assigned to Note On
        scriptToRun = midi.on_script;
    }
    // Handle Note Off (including velocity-0 note-ons)
    else if (message.isNoteOff() || (message.isNoteOn() && message.velocity() == 0)) {
        if (midi.off_count < MAX_MIDI_EVENTS) {
            midi.note_off[midi.off_count] = message.note();
            midi.off_channel[midi.off_count] = message.channel();
            midi.off_count++;
        }
        midi.last_event_type = 0;
        midi.last_channel = message.channel();
        midi.last_note = message.note();
        midi.last_velocity = 0;

        // Check if script assigned to Note Off
        scriptToRun = midi.off_script;
    }
    // Handle Control Change
    else if (message.isControlChange()) {
        if (midi.cc_count < MAX_MIDI_EVENTS) {
            midi.cn[midi.cc_count] = message.controlNumber();
            midi.cc[midi.cc_count] = message.controlValue();
            midi.cc_channel[midi.cc_count] = message.channel();
            midi.cc_count++;
        }
        midi.last_event_type = 2;
        midi.last_channel = message.channel();
        midi.last_controller = message.controlNumber();
        midi.last_cc = message.controlValue();

        // Check if script assigned to CC
        scriptToRun = midi.cc_script;
    }

    // Trigger assigned script if valid (0-3 are S0-S3)
    if (scriptToRun >= 0 && scriptToRun < EDITABLE_SCRIPT_COUNT) {
        runMidiTriggeredScript(scriptToRun);
    }
}

void TeletypeTrackEngine::runMidiTriggeredScript(int scriptIndex) {
    // Set active engine so teletype callbacks (tele_tr, tele_cv, etc.) work correctly
    TeletypeBridge::ScopedEngine scopedEngine(*this);

    scene_state_t &state = _teletypeTrack.state();

    // Use teletype VM's run_script function
    // Declared in teletype/src/teletype.h
    run_script(&state, scriptIndex);

    // Note: run_script() executes all lines of the script synchronously
    // CV/TR/DELAY ops are handled by the VM and engine callbacks
}
