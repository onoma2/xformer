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

// Stub host: real TV bank backed by int16_t[16], inert elsewhere.
struct TvStubHost : TT2Host {
    int16_t tv[16] = {};
    Modulator mod;
    GeodeConfig geode;

    TvStubHost() { geode.clear(); mod.clear(); }

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
    int16_t hostTvGet(uint8_t slot) override { return slot < 16 ? tv[slot] : 0; }
    void hostTvSet(uint8_t slot, int16_t v) override { if (slot < 16) tv[slot] = v; }

    Modulator &hostModulator(uint8_t) override { return mod; }
    int16_t hostModulatorOutput(uint8_t) override { return 0; }
    void hostModulatorTrigger(uint8_t) override {}

    GeodeConfig &hostGeodeConfig() override { return geode; }
    int16_t hostGeodeMix() override { return 0; }
    void hostGeodeTriggerVoice(uint8_t, int16_t, int16_t) override {}
    void hostGeodeTriggerAll(int16_t, int16_t) override {}
    int16_t hostGeodeRun() override { return 0; }
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

UNIT_TEST("TtSharedTv") {

CASE("set_then_get_roundtrip") {
    TvStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    evalText("TV 0 42", rt, out);
    expectEqual(int(host.tv[0]), 42, "TV 0 42 stores 42");
    auto r = evalText("TV 0", rt, out);
    expectEqual(int(r.error), int(TT2EvalError::None), "TV 0 get ok");
    expectEqual(int(r.value), 42, "TV 0 reads 42 back");
    tt2SetActiveHost(nullptr);
}

CASE("shared_across_invocations") {
    TvStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    evalText("TV 3 17", rt, out);
    auto r = evalText("TV 3", rt, out);
    expectEqual(int(r.error), int(TT2EvalError::None), "TV 3 get ok");
    expectEqual(int(r.value), 17, "TV 3 shared across invocations");
    tt2SetActiveHost(nullptr);
}

CASE("last_writer_wins") {
    TvStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    evalText("TV 0 5", rt, out);
    evalText("TV 0 9", rt, out);
    auto r = evalText("TV 0", rt, out);
    expectEqual(int(r.value), 9, "last writer wins");
    tt2SetActiveHost(nullptr);
}

CASE("out_of_range_high_is_silent_zero") {
    TvStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    evalText("TV 99 7", rt, out);
    auto r = evalText("TV 99", rt, out);
    expectEqual(int(r.error), int(TT2EvalError::None), "TV 99 get ok");
    expectEqual(int(r.value), 0, "TV 99 reads 0 (out of range)");
    tt2SetActiveHost(nullptr);
}

CASE("out_of_range_negative_is_silent_zero") {
    TvStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    auto r = evalText("TV -1", rt, out);
    expectEqual(int(r.error), int(TT2EvalError::None), "TV -1 get ok");
    expectEqual(int(r.value), 0, "TV -1 reads 0 (out of range)");
    tt2SetActiveHost(nullptr);
}

CASE("null_host_reads_zero") {
    tt2SetActiveHost(nullptr);
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    auto r = evalText("TV 0", rt, out);
    expectEqual(int(r.error), int(TT2EvalError::None), "TV 0 get ok (null host)");
    expectEqual(int(r.value), 0, "TV 0 reads 0 (null host)");
}

}
