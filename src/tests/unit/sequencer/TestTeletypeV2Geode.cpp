#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"
#include "engine/TT2Host.h"

#include "model/Types.h"

extern "C" {
#include "command.h"
#include "tt_parser.h"
#include "ops/op_enum.h"
}

namespace {

// Stub host: real behavior for the Geode/modulator surface, inert elsewhere.
struct GeodeStubHost : TT2Host {
    GeodeConfig geode;
    Modulator mod;
    int runOffset = 0;   // mirrors the real host: run lives in modulator(1).offset (-64..63)
    int16_t mix = 0;
    int triggerVoiceCount = 0, lastVoice = -1, lastDivs = 0, lastReps = 0;
    int triggerAllCount = 0, lastAllDivs = 0, lastAllReps = 0;

    GeodeStubHost() { geode.clear(); mod.clear(); }

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

    Modulator &hostModulator(uint8_t) override { return mod; }
    int16_t hostModulatorOutput(uint8_t) override { return 0; }
    void hostModulatorTrigger(uint8_t) override {}

    GeodeConfig &hostGeodeConfig() override { return geode; }
    int16_t hostGeodeMix() override { return mix; }
    void hostGeodeTriggerVoice(uint8_t v, int16_t divs, int16_t reps) override {
        ++triggerVoiceCount; lastVoice = v; lastDivs = divs; lastReps = reps;
    }
    void hostGeodeTriggerAll(int16_t divs, int16_t reps) override {
        ++triggerAllCount; lastAllDivs = divs; lastAllReps = reps;
    }
    // Mirror the real host: store M2 offset (integer), so readback is quantized
    // to the 128-step offset grid (NOT a lossless macro store).
    int16_t hostGeodeRun() override {
        int m = (runOffset + 64) * 128;
        return int16_t(m > 16383 ? 16383 : m);
    }
    void hostGeodeSetRun(int16_t macro) override {
        if (macro < 0) macro = 0; if (macro > 16383) macro = 16383;
        runOffset = macro / 128 - 64;
    }
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

UNIT_TEST("TeletypeV2Geode") {

CASE("globals set/get roundtrip via host") {
    GeodeStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    evalText("G.TIME 4000", rt, out);
    expectEqual(host.geode.time(), 4000, "G.TIME set");
    expectEqual(int(evalText("G.TIME", rt, out).value), 4000, "G.TIME get");
    evalText("G.TONE 2000", rt, out); expectEqual(host.geode.intone(), 2000, "G.TONE");
    evalText("G.RAMP 3000", rt, out); expectEqual(host.geode.ramp(), 3000, "G.RAMP");
    evalText("G.CURV 4000", rt, out); expectEqual(host.geode.curve(), 4000, "G.CURV");
    evalText("G.MODE 2", rt, out);    expectEqual(host.geode.mode(), 2, "G.MODE");
    tt2SetActiveHost(nullptr);
}

CASE("short aliases hit the same handler") {
    GeodeStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("G.T 1234", rt, out); expectEqual(host.geode.time(), 1234, "G.T = G.TIME");
    evalText("G.I 5678", rt, out); expectEqual(host.geode.intone(), 5678, "G.I = G.TONE");
    evalText("G.C 999", rt, out);  expectEqual(host.geode.curve(), 999, "G.C = G.CURV");
    tt2SetActiveHost(nullptr);
}

CASE("G.RUN run macro is quantized to the M2 offset grid, alias G.N") {
    GeodeStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    // run lives in M2's integer offset, so readback snaps to the 128-step grid:
    evalText("G.RUN 12000", rt, out);
    expectEqual(int(evalText("G.RUN", rt, out).value), 11904, "12000 -> 11904 (quantized)");
    // a grid-aligned value (multiple of 128) round-trips exactly:
    evalText("G.N 4096", rt, out);
    expectEqual(int(evalText("G.N", rt, out).value), 4096, "G.N alias; 4096 grid-aligned, exact");
    tt2SetActiveHost(nullptr);
}

CASE("G.VAL reads mix, alias G.L") {
    GeodeStubHost host; host.mix = 5000; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    expectEqual(int(evalText("G.VAL", rt, out).value), 5000, "G.VAL = mix");
    expectEqual(int(evalText("G.L", rt, out).value), 5000, "G.L = G.VAL");
    tt2SetActiveHost(nullptr);
}

CASE("G.TUNE per-voice and voice 0 = all") {
    GeodeStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("G.TUNE 1 3 2", rt, out);
    expectEqual(host.geode.tuneNumerator(0), 3, "voice1 num");
    expectEqual(host.geode.tuneDenominator(0), 2, "voice1 den");
    evalText("G.TUNE 0 5 4", rt, out);
    expectEqual(host.geode.tuneNumerator(0), 5, "all -> v0 num");
    expectEqual(host.geode.tuneNumerator(5), 5, "all -> v5 num");
    tt2SetActiveHost(nullptr);
}

CASE("G.V triggers voice (0 = all)") {
    GeodeStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("G.V 1 4 2", rt, out);
    expectEqual(host.triggerVoiceCount, 1, "one voice trigger");
    expectEqual(host.lastVoice, 0, "voice 1 -> index 0");
    expectEqual(host.lastDivs, 4, "divs passed");
    expectEqual(host.lastReps, 2, "reps passed");
    evalText("G.V 0 8 1", rt, out);
    expectEqual(host.triggerAllCount, 1, "voice 0 -> trigger all");
    expectEqual(host.lastAllDivs, 8, "all divs passed");
    evalText("G.V 9 1 1", rt, out);
    expectEqual(host.triggerVoiceCount, 1, "voice > 6 is a no-op");
    tt2SetActiveHost(nullptr);
}

CASE("G.S sets time/tone/run in one line") {
    GeodeStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("G.S 1000 2000 3072", rt, out);   // 3072 = 24*128, grid-aligned
    expectEqual(host.geode.time(), 1000, "G.S time");
    expectEqual(host.geode.intone(), 2000, "G.S tone");
    expectEqual(int(evalText("G.RUN", rt, out).value), 3072, "G.S run (read back via host)");
    tt2SetActiveHost(nullptr);
}

CASE("G.O / G.BAR / G.R are intentionally unsupported") {
    GeodeStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    expectTrue(evalText("G.O 100", rt, out).error == TT2EvalError::UnsupportedOp,
               "G.O unsupported");
    expectTrue(evalText("G.BAR 4", rt, out).error == TT2EvalError::UnsupportedOp,
               "G.BAR unsupported");
    expectTrue(evalText("G.R 1 2", rt, out).error == TT2EvalError::UnsupportedOp,
               "G.R unsupported");
    tt2SetActiveHost(nullptr);
}

}
