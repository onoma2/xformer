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
// never trigger-driven.
void TT2TrackEngine::updateInputTriggers() {
    bool now[TT2_TRIGGER_INPUT_COUNT];
    for (int i = 0; i < TT2_TRIGGER_INPUT_COUNT; ++i) {
        now[i] = inputState(uint8_t(i));
    }
    uint8_t fired = tt2RisingEdges(now, _prevInputState, TT2_TRIGGER_INPUT_COUNT);
    for (int i = 0; i < TT2_TRIGGER_INPUT_COUNT; ++i) {
        if (fired & (1 << i)) {
            runScript(uint8_t(i));
        }
    }
}
