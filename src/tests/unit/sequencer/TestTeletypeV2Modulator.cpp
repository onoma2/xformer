#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"
#include "engine/TT2Host.h"

extern "C" {
#include "command.h"
#include "tt_parser.h"
#include "ops/op_enum.h"
}

namespace {

using Param = Modulator::Param;

// Stub host: real per-slot modulators + output + trigger record, inert elsewhere.
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

UNIT_TEST("TeletypeV2Modulator") {

CASE("named verb set/get roundtrip per slot") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    evalText("MO.DEPTH 3 100", rt, out);
    expectEqual(host.mods[2].depth(), 100, "slot 3 depth set");
    expectEqual(int(evalText("MO.DEPTH 3", rt, out).value), 100, "slot 3 depth get");
    evalText("MO.SHAPE 1 5", rt, out);
    expectEqual(int(host.mods[0].shape()), 5, "slot 1 shape set");
    evalText("MO.MODE 2 2", rt, out);
    expectEqual(int(host.mods[1].mode()), 2, "slot 2 mode set");
    evalText("MO.OFF 4 -20", rt, out);
    expectEqual(host.mods[3].offset(), -20, "slot 4 offset set");
    tt2SetActiveHost(nullptr);
}

CASE("short alias matches canonical") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("MO.D 3 77", rt, out);
    expectEqual(host.mods[2].depth(), 77, "MO.D == MO.DEPTH");
    evalText("MO.S 3 4", rt, out);
    expectEqual(int(host.mods[2].shape()), 4, "MO.S == MO.SHAPE");
    tt2SetActiveHost(nullptr);
}

CASE("MO.P addresses any field; equals named verb") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("MO.P 3 2 100", rt, out);          // addr 2 = Depth
    expectEqual(host.mods[2].depth(), 100, "MO.P depth set");
    expectEqual(int(evalText("MO.P 3 2", rt, out).value), 100, "MO.P depth get");
    evalText("MO.P 5 5 500", rt, out);          // addr 5 = Attack (long tail)
    expectEqual(host.mods[4].attack(), 500, "MO.P reaches long-tail Attack");
    tt2SetActiveHost(nullptr);
}

CASE("bare MO reads slot output") {
    ModStubHost host; host.outputs[2] = 77; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    expectEqual(int(evalText("MO 3", rt, out).value), 77, "MO 3 = output[2]");
    tt2SetActiveHost(nullptr);
}

CASE("MO.TRIG triggers the slot, alias MO.T") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("MO.TRIG 2", rt, out);
    expectEqual(host.triggerCount, 1, "one trigger");
    expectEqual(host.lastTrigIdx, 1, "slot 2 -> index 1");
    evalText("MO.T 5", rt, out);
    expectEqual(host.lastTrigIdx, 4, "MO.T alias, slot 5 -> index 4");
    tt2SetActiveHost(nullptr);
}

CASE("value clamps to the field's own range") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("MO.DEPTH 3 200", rt, out);
    expectEqual(host.mods[2].depth(), 127, "depth clamps to 127");
    tt2SetActiveHost(nullptr);
}

CASE("slot index out of range is OutOfRange, no write") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    int before = host.mods[0].depth();
    expectTrue(evalText("MO.DEPTH 0 50", rt, out).error == TT2EvalError::OutOfRange,
               "slot 0 rejected");
    expectTrue(evalText("MO.DEPTH 9 50", rt, out).error == TT2EvalError::OutOfRange,
               "slot 9 rejected");
    expectEqual(host.mods[0].depth(), before, "no write on bad slot");
    tt2SetActiveHost(nullptr);
}

CASE("WPN forwards signed bank/idx to host set/get") {
    struct RecHost : ModStubHost {
        int16_t lastTrack = -1, lastBank = -1, lastIdx = -1, lastVal = -1;
        int16_t cell = 0;
        int16_t hostTrackPatternVal(uint8_t tr, int16_t b, int16_t i) override {
            lastTrack = tr; lastBank = b; lastIdx = i; return cell;
        }
        void hostSetTrackPatternVal(uint8_t tr, int16_t b, int16_t i, int16_t v) override {
            lastTrack = tr; lastBank = b; lastIdx = i; lastVal = v; cell = v;
        }
    } host;
    tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    evalText("WPN 2 0 5 42", rt, out);
    expectEqual(int(host.lastTrack), 1, "track 1-based→0-based");
    expectEqual(int(host.lastBank), 0, "bank passed signed");
    expectEqual(int(host.lastIdx), 5, "idx passed signed");
    expectEqual(int(host.lastVal), 42, "value written");
    evalText("WPN 2 0 5", rt, out);
    expectEqual(int(host.cell), 42, "read back same cell");
    tt2SetActiveHost(nullptr);
}

CASE("WS propagates host error; track stays 1-based at boundary") {
    struct ScriptHost : ModStubHost {
        int calls = 0; int16_t lastTrack = -1, lastScript = -1;
        TT2EvalError ret = TT2EvalError::None;
        TT2EvalError hostTriggerTrackScript(uint8_t tr, int16_t n) override {
            ++calls; lastTrack = tr; lastScript = n; return ret;
        }
    } host;
    tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    auto res = evalText("WS 2 1", rt, out);
    expectEqual(int(host.lastTrack), 1, "track 2 → host track 1");
    expectEqual(int(host.lastScript), 1, "script stays 1-based at boundary");
    expectEqual(int(res.error), int(TT2EvalError::None), "no error");
    host.calls = 0; host.lastTrack = -1;
    evalText("WS 0 1", rt, out);
    expectEqual(int(host.lastTrack), 255, "0 → 255 reaches host guard");
    host.ret = TT2EvalError::OutOfRange;
    auto r2 = evalText("WS 2 11", rt, out);
    expectEqual(int(r2.error), int(TT2EvalError::OutOfRange), "OutOfRange propagated");
    host.ret = TT2EvalError::ExecDepthOverflow;
    auto r3 = evalText("WS 2 1", rt, out);
    expectEqual(int(r3.error), int(TT2EvalError::ExecDepthOverflow), "ExecDepthOverflow propagated");
    tt2SetActiveHost(nullptr);
}

}
