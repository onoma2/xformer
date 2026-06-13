#include "TT2TrackEngine.h"

#include "Engine.h"
#include "CvInput.h"

// Resolve one trigger input's configured source to a boolean gate level.
// Mirrors TeletypeTrackEngine::inputState: CvIn (threshold > 1V), GateOut
// (read back the engine's gate output), LogicalGate (another track's gate).
bool TT2TrackEngine::inputState(uint8_t index) const {
    if (index >= TT2_TRIGGER_INPUT_COUNT) {
        return false;
    }
    TT2TriggerSource source = _tt2Track.program().triggerSource[index];

    if (source >= TT2TriggerSource::CvIn1 && source <= TT2TriggerSource::CvIn4) {
        int ch = int(source) - int(TT2TriggerSource::CvIn1);
        if (ch < CvInput::Channels) {
            return _engine.cvInput().channel(ch) > 1.0f;  // gate threshold at 1V
        }
        return false;
    }

    if (source >= TT2TriggerSource::GateOut1 && source <= TT2TriggerSource::GateOut8) {
        int g = int(source) - int(TT2TriggerSource::GateOut1);
        return (_engine.gateOutput() >> g) & 0x1;
    }

    if (source >= TT2TriggerSource::LogicalGate1 && source <= TT2TriggerSource::LogicalGate8) {
        int t = int(source) - int(TT2TriggerSource::LogicalGate1);
        if (t >= 0 && t < CONFIG_TRACK_COUNT) {
            return _engine.trackEngine(t).gateOutput(0);
        }
        return false;
    }

    return false;  // None
}

// Sample the trigger inputs and fire the matching script on each rising edge.
// TI i -> script i (0-3); the metro (4) and init (5) scripts are reserved and
// never trigger-driven. A muted input (variables.mutes bit) still tracks its
// edge (no backlog on unmute) but does not fire. Latches the live level for STATE.
void TT2TrackEngine::updateInputTriggers() {
    auto &runtime = _tt2Track.runtime();
    bool now[TT2_TRIGGER_INPUT_COUNT];
    for (int i = 0; i < TT2_TRIGGER_INPUT_COUNT; ++i) {
        now[i] = inputState(uint8_t(i));
        runtime.inputLevel[i] = now[i] ? 1 : 0;
    }
    uint8_t fired = tt2RisingEdges(now, _prevInputState, TT2_TRIGGER_INPUT_COUNT);
    uint8_t mutes = uint8_t(runtime.variables.mutes);
    for (int i = 0; i < TT2_TRIGGER_INPUT_COUNT; ++i) {
        if ((fired & (1 << i)) && !((mutes >> i) & 1)) {
            runScript(uint8_t(i));
        }
    }
}

// Resolve a CV input source to Performer volts. Mirrors the legacy
// TeletypeTrackEngine readCvSource: physical CV in, CV out read-back, CV router
// lane, or another track's CV. None -> 0.
float TT2TrackEngine::cvSourceVolts(TT2CvInputSource source) const {
    if (source >= TT2CvInputSource::CvIn1 && source <= TT2CvInputSource::CvIn4) {
        int ch = int(source) - int(TT2CvInputSource::CvIn1);
        return ch < CvInput::Channels ? _engine.cvInput().channel(ch) : 0.f;
    }
    if (source >= TT2CvInputSource::CvOut1 && source <= TT2CvInputSource::CvOut8) {
        int ch = int(source) - int(TT2CvInputSource::CvOut1);
        return _engine.cvOutput().channel(ch);
    }
    if (source >= TT2CvInputSource::CvRoute1 && source <= TT2CvInputSource::CvRoute4) {
        int lane = int(source) - int(TT2CvInputSource::CvRoute1);
        return _engine.cvRouteOutput(lane);
    }
    if (source >= TT2CvInputSource::LogicalCv1 && source <= TT2CvInputSource::LogicalCv8) {
        int t = int(source) - int(TT2CvInputSource::LogicalCv1);
        if (t >= 0 && t < CONFIG_TRACK_COUNT) {
            return _engine.trackEngine(t).cvOutput(0);
        }
    }
    return 0.f;  // None / out of range
}

// Sample the six CV-mapped engine inputs each refresh. IN/PARAM are always
// written (0 if unmapped); X/Y/Z/T only when mapped, so a script can still set
// them by hand when their source is None.
void TT2TrackEngine::sampleInputs() {
    auto &program = _tt2Track.program();
    auto &v = _tt2Track.runtime().variables;
    v.in    = voltsToRaw(cvSourceVolts(program.cvInputSource[int(TT2CvInput::In)]));
    v.param = voltsToRaw(cvSourceVolts(program.cvInputSource[int(TT2CvInput::Param)]));
    int16_t *target[4] = { &v.x, &v.y, &v.z, &v.t };
    for (int k = 0; k < 4; ++k) {
        TT2CvInputSource src = program.cvInputSource[int(TT2CvInput::X) + k];
        if (src != TT2CvInputSource::None) {
            *target[k] = voltsToRaw(cvSourceVolts(src));
        }
    }
}
