#pragma once

#include "model/Modulator.h"
#include "model/GeodeConfig.h"

#include "TT2Evaluator.h"

#include <cstdint>

// Host accessor for native TT2 ops that need live engine / cross-track state
// (transport, tempo, CV-router bus, other tracks' note steps, routing source).
// TT2TrackEngine implements this and registers itself active around script
// execution, so the op layer (TeletypeNativeOps.cpp) stays decoupled from
// Engine.h. All values cross the boundary as int16 in Teletype units.
struct TT2Host {
    virtual ~TT2Host() = default;

    // transport / tempo
    virtual int16_t hostTempo() = 0;                       // BPM
    virtual void hostSetTempo(int16_t bpm) = 0;
    virtual int16_t hostTransportRunning() = 0;            // 1 / 0
    virtual void hostSetTransportRunning(int16_t state) = 0;
    virtual int16_t hostBarFraction(uint8_t bars) = 0;     // 0..16383
    virtual int16_t hostWms(uint8_t mult) = 0;             // ms
    virtual int16_t hostWtu(uint8_t div, uint8_t mult) = 0;

    // per-track pattern
    virtual int16_t hostTrackPattern(uint8_t track) = 0;
    virtual void hostSetTrackPattern(uint8_t track, uint8_t pattern) = 0;
    virtual int16_t hostTrackPatternVal(uint8_t track, int16_t bank, int16_t idx) = 0;
    virtual void hostSetTrackPatternVal(uint8_t track, int16_t bank, int16_t idx, int16_t v) = 0;
    virtual TT2EvalError hostTriggerTrackScript(uint8_t track, int16_t script) = 0;

    // cross-track note/gate (Note tracks)
    virtual int16_t hostNoteGateGet(uint8_t track, uint8_t step) = 0;
    virtual void hostNoteGateSet(uint8_t track, uint8_t step, int16_t v) = 0;
    virtual int16_t hostNoteNoteGet(uint8_t track, uint8_t step) = 0;
    virtual void hostNoteNoteSet(uint8_t track, uint8_t step, int16_t v) = 0;
    virtual int16_t hostNoteGateHere(uint8_t track) = 0;
    virtual int16_t hostNoteNoteHere(uint8_t track) = 0;

    // routing / bus
    virtual int16_t hostRoutingSource(uint8_t index) = 0;  // 0..16383
    virtual int16_t hostBusCv(uint8_t index) = 0;          // raw 0..16383
    virtual void hostSetBusCv(uint8_t index, int16_t raw) = 0;
    virtual int16_t hostTvGet(uint8_t slot) = 0;          // shared TV bank, 0-based
    virtual void hostTvSet(uint8_t slot, int16_t v) = 0;

    // modulator slots (MO.*) — direct field access via the shared param dictionary
    virtual Modulator &hostModulator(uint8_t idx) = 0;     // 0-based slot
    virtual int16_t hostModulatorOutput(uint8_t idx) = 0;  // current output 0..127
    virtual void hostModulatorTrigger(uint8_t idx) = 0;

    // Geode engine (G.*) — global config + voice triggers + M2 run macro
    virtual GeodeConfig &hostGeodeConfig() = 0;
    virtual int16_t hostGeodeMix() = 0;                    // mix level, raw 0..16383
    virtual void hostGeodeTriggerVoice(uint8_t v, int16_t divs, int16_t repeats) = 0;
    virtual void hostGeodeTriggerAll(int16_t divs, int16_t repeats) = 0;
    virtual int16_t hostGeodeRun() = 0;                    // run macro 0..16383, 8192=noon
    virtual void hostGeodeSetRun(int16_t macro) = 0;
};

// Active host during script execution (set/cleared by TT2TrackEngine). Null
// when scripts run without an engine (editor / unit tests) — ops guard it.
TT2Host *tt2ActiveHost();
void tt2SetActiveHost(TT2Host *host);
