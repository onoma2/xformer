#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"
#include "engine/TT2Host.h"

extern "C" {
#include "command.h"
#include "tt_parser.h"
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

UNIT_TEST("TeletypeV2LfoOps") {

CASE("LFO.C is a recognized parser token") {
    tele_command_t src = {};
    char err[TELE_ERROR_MSG_LENGTH] = {};
    expectTrue(parse("LFO.C 1 16", &src, err) == E_OK, "LFO.C parses");
}

CASE("LFO.R sets Free-domain centi-Hz rate, locks Sine/Run") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("LFO.R 1 100", rt, out);
    expectTrue(host.mods[0].rateDomain() == Modulator::RateDomain::Free, "domain Free");
    expectEqual(host.mods[0].rate(), 100, "rate = 100 centi-Hz (1Hz)");
    expectTrue(host.mods[0].shape() == Modulator::Shape::Sine, "shape Sine");
    expectTrue(host.mods[0].mode() == Modulator::Mode::Run, "mode Run");
    tt2SetActiveHost(nullptr);
}

CASE("LFO.C sets Tempo-domain rate from note divisor (384/d)") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("LFO.C 1 16", rt, out);
    expectTrue(host.mods[0].rateDomain() == Modulator::RateDomain::Tempo, "domain Tempo");
    expectEqual(host.mods[0].rate(), 24, "1/16 -> rate 24");
    evalText("LFO.C 1 4", rt, out);
    expectEqual(host.mods[0].rate(), 96, "1/4 -> rate 96");
    evalText("LFO.C 1 1", rt, out);
    expectEqual(host.mods[0].rate(), 384, "whole note -> rate 384");
    tt2SetActiveHost(nullptr);
}

CASE("LFO.A sets depth, LFO.O sets offset") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("LFO.A 1 100", rt, out);
    expectEqual(host.mods[0].depth(), 100, "LFO.A -> depth");
    evalText("LFO.O 1 -10", rt, out);
    expectEqual(host.mods[0].offset(), -10, "LFO.O -> offset");
    tt2SetActiveHost(nullptr);
}

CASE("any LFO.* write forces Shape::Sine and Mode::Run") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    host.mods[1].setShape(Modulator::Shape::ADSR);
    host.mods[1].setMode(Modulator::Mode::Trig);
    evalText("LFO.A 2 50", rt, out);
    expectTrue(host.mods[1].shape() == Modulator::Shape::Sine, "shape forced Sine");
    expectTrue(host.mods[1].mode() == Modulator::Mode::Run, "mode forced Run");
    expectEqual(host.mods[1].depth(), 50, "depth still set");
    tt2SetActiveHost(nullptr);
}

CASE("rate ops switch the slot's domain (later op wins)") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("LFO.R 3 200", rt, out);
    evalText("LFO.C 3 8", rt, out);
    expectTrue(host.mods[2].rateDomain() == Modulator::RateDomain::Tempo, "ends Tempo");
    expectEqual(host.mods[2].rate(), 48, "1/8 -> rate 48");
    tt2SetActiveHost(nullptr);
}

CASE("values clamp to each field's range") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("LFO.R 1 9999", rt, out);
    expectEqual(host.mods[0].rate(), 1600, "Free rate clamps to 1600 (16Hz)");
    evalText("LFO.R 1 0", rt, out);
    expectEqual(host.mods[0].rate(), 1, "Free rate clamps to 1");
    evalText("LFO.C 1 0", rt, out);
    expectEqual(host.mods[0].rate(), 384, "divisor floored to 1 -> 384");
    evalText("LFO.C 1 100", rt, out);
    expectEqual(host.mods[0].rate(), 6, "384/100=3 clamps to Tempo min 6");
    evalText("LFO.A 1 999", rt, out);
    expectEqual(host.mods[0].depth(), 127, "depth clamps to 127");
    evalText("LFO.O 1 999", rt, out);
    expectEqual(host.mods[0].offset(), 63, "offset clamps to 63");
    tt2SetActiveHost(nullptr);
}

CASE("read path returns the field, no lock applied") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    evalText("LFO.A 1 70", rt, out);
    expectEqual(int(evalText("LFO.A 1", rt, out).value), 70, "LFO.A get returns depth");
    tt2SetActiveHost(nullptr);
}

CASE("slot index out of range is OutOfRange, no write") {
    ModStubHost host; tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt); TT2OutputState out = {}; init(out);
    int before = host.mods[0].rate();
    expectTrue(evalText("LFO.R 0 100", rt, out).error == TT2EvalError::OutOfRange,
               "slot 0 rejected");
    expectTrue(evalText("LFO.R 9 100", rt, out).error == TT2EvalError::OutOfRange,
               "slot 9 rejected");
    expectEqual(host.mods[0].rate(), before, "no write on bad slot");
    tt2SetActiveHost(nullptr);
}

}
