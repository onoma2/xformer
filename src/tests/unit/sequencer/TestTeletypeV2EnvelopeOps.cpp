#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"
#include "engine/TT2Host.h"

extern "C" {
#include "command.h"
#include "tt_parser.h"
#include "ops/op_enum.h"
}

// E.* envelope ops: ADSR-shape-locked aliases over the modulator engine.
// Every E.* write claims slot n as a triggered ADSR (forces Shape::ADSR +
// Mode::Trig), then sets one field. Mirrors the MO.* op contract + fixture.

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
    int16_t hostTvGet(uint8_t) override { return 0; }
    void hostTvSet(uint8_t, int16_t) override {}
    int16_t hostTrackPatternVal(uint8_t, int16_t, int16_t) override { return 0; }
    void hostSetTrackPatternVal(uint8_t, int16_t, int16_t, int16_t) override {}
    TT2EvalError hostTriggerTrackScript(uint8_t, int16_t) override { return TT2EvalError::None; }

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

UNIT_TEST("TeletypeV2EnvelopeOps") {

CASE("field writes set the right modulator field") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    evalText("E.A 1 500", rt, out);
    expectEqual(host.mods[0].attack(), 500, "E.A -> attack");
    evalText("E.D 1 300", rt, out);
    expectEqual(host.mods[0].decay(), 300, "E.D -> decay");
    evalText("E.O 1 -10", rt, out);
    expectEqual(host.mods[0].offset(), -10, "E.O -> offset");
    evalText("E 1 100", rt, out);
    expectEqual(host.mods[0].amplitude(), 100, "bare E -> amplitude");
    tt2SetActiveHost(nullptr);
}

CASE("any E.* write forces Shape::ADSR and Mode::Trig") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    host.mods[1].setShape(Modulator::Shape::Sine);
    host.mods[1].setMode(Modulator::Mode::Gate);
    evalText("E.A 2 400", rt, out);
    expectTrue(host.mods[1].shape() == Modulator::Shape::ADSR, "shape forced to ADSR");
    expectTrue(host.mods[1].mode() == Modulator::Mode::Trig, "mode forced to Trig");
    expectEqual(host.mods[1].attack(), 400, "attack still set");
    tt2SetActiveHost(nullptr);
}

CASE("E.T forces Trig and triggers the slot") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("E.T 1", rt, out);
    expectTrue(host.mods[0].mode() == Modulator::Mode::Trig, "E.T -> Mode::Trig");
    expectEqual(host.triggerCount, 1, "one trigger");
    expectEqual(host.lastTrigIdx, 0, "slot 1 -> index 0");
    tt2SetActiveHost(nullptr);
}

CASE("bare E reads slot output, not amplitude") {
    ModStubHost host; host.outputs[0] = 77; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("E 1 90", rt, out);
    expectEqual(host.mods[0].amplitude(), 90, "E 1 90 sets amplitude");
    expectEqual(int(evalText("E 1", rt, out).value), 77, "bare E 1 reads output[0]");
    tt2SetActiveHost(nullptr);
}

CASE("values clamp to each field's own range") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("E.A 1 9999", rt, out);
    expectEqual(host.mods[0].attack(), 2000, "attack clamps to 2000");
    evalText("E.O 1 -999", rt, out);
    expectEqual(host.mods[0].offset(), -64, "offset clamps to -64");
    evalText("E.O 1 999", rt, out);
    expectEqual(host.mods[0].offset(), 63, "offset clamps to +63");
    evalText("E 1 999", rt, out);
    expectEqual(host.mods[0].amplitude(), 127, "amplitude clamps to 127");
    tt2SetActiveHost(nullptr);
}

CASE("slot index out of range is OutOfRange, no write") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    int before = host.mods[0].attack();
    expectTrue(evalText("E.A 0 500", rt, out).error == TT2EvalError::OutOfRange,
               "slot 0 rejected");
    expectTrue(evalText("E.A 9 500", rt, out).error == TT2EvalError::OutOfRange,
               "slot 9 rejected");
    expectEqual(host.mods[0].attack(), before, "no write on bad slot");
    tt2SetActiveHost(nullptr);
}

}
