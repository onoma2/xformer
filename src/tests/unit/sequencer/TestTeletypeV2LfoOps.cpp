#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"
#include "engine/TT2Host.h"

extern "C" {
#include "command.h"
#include "teletype.h"
#include "ops/op_enum.h"
}

// LFO.* ops: Sine-shape-locked, free-running aliases over the modulator engine.
// Every LFO.* write claims slot n as a free-running sine (forces Shape::Sine +
// Mode::Run), then sets one field. Two rate ops cover both rate domains:
// LFO.R (Free, centi-Hz) and LFO.C (Tempo, note divisor -> rate = 384/d).

namespace {

struct ModStubHost : TT2Host {
    Modulator mods[CONFIG_MODULATOR_COUNT];
    int16_t outputs[CONFIG_MODULATOR_COUNT] = {};
    GeodeConfig geode;
    int triggerCount = 0, lastTrigIdx = -1;

    ModStubHost() { for (auto &m : mods) m.clear(); geode.clear(); }

    int16_t hostTempo() override { return 0; }
    void hostSetTempo(int16_t) override {}
    int16_t hostTransportRunning() override { return 0; }
    void hostSetTransportRunning(int16_t) override {}
    int16_t hostBarFraction(uint8_t) override { return 0; }
    int16_t hostWms(uint8_t) override { return 0; }
    int16_t hostWtu(uint8_t, uint8_t) override { return 0; }
    int16_t hostTrackPattern(uint8_t) override { return 0; }
    void hostSetTrackPattern(uint8_t, uint8_t) override {}
    int16_t hostNoteGateGet(uint8_t, uint8_t) override { return 0; }
    void hostNoteGateSet(uint8_t, uint8_t, int16_t) override {}
    int16_t hostNoteNoteGet(uint8_t, uint8_t) override { return 0; }
    void hostNoteNoteSet(uint8_t, uint8_t, int16_t) override {}
    int16_t hostNoteGateHere(uint8_t) override { return 0; }
    int16_t hostNoteNoteHere(uint8_t) override { return 0; }
    int16_t hostRoutingSource(uint8_t) override { return 0; }
    int16_t hostBusCv(uint8_t) override { return 0; }
    void hostSetBusCv(uint8_t, int16_t) override {}

    Modulator &hostModulator(uint8_t idx) override {
        if (idx >= CONFIG_MODULATOR_COUNT) idx = 0;
        return mods[idx];
    }
    int16_t hostModulatorOutput(uint8_t idx) override {
        return idx < CONFIG_MODULATOR_COUNT ? outputs[idx] : 0;
    }
    void hostModulatorTrigger(uint8_t idx) override { ++triggerCount; lastTrigIdx = idx; }

    GeodeConfig &hostGeodeConfig() override { return geode; }
    int16_t hostGeodeMix() override { return 0; }
    void hostGeodeTriggerVoice(uint8_t, int16_t, int16_t) override {}
    void hostGeodeTriggerAll(int16_t, int16_t) override {}
    int16_t hostGeodeRun() override { return 8192; }
    void hostGeodeSetRun(int16_t) override {}
};

TT2EvalResult evalText(const char *text, TT2Runtime &rt, TT2OutputState &out) {
    tele_command_t src = {};
    char err[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, err);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    return evaluateCommand(cmd, rt, out, nullptr);
}

} // namespace

UNIT_TEST("TeletypeV2LfoOps") {

CASE("LFO.C is a recognized parser token") {
    tele_command_t src = {};
    char err[TELE_ERROR_MSG_LENGTH] = {};
    expectTrue(parse("LFO.C 1 16", &src, err) == E_OK, "LFO.C parses");
}

}
