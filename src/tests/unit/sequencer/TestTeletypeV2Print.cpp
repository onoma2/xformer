#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"

extern "C" {
#include "command.h"
#include "tt_parser.h"
#include "ops/op_enum.h"
}

// PRINT / PRT: a 16-slot ephemeral dashboard value store in TT2Runtime.
// PRINT n x writes slot n (1-based), PRINT n reads it. PRT is an alias.
// Pure runtime state — no host, not serialized (mirrors the TT1-track store).

namespace {

TT2EvalResult evalText(const char *text, TT2Runtime &rt, TT2OutputState &out) {
    tele_command_t src = {};
    char err[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, err);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    return evaluateCommand(cmd, rt, out, nullptr);
}

} // namespace

UNIT_TEST("TeletypeV2Print") {

CASE("PRINT set then get round-trips a slot") {
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    evalText("PRINT 1 500", rt, out);
    expectEqual(int(evalText("PRINT 1", rt, out).value), 500, "slot 1 = 500");
    evalText("PRINT 16 -3", rt, out);
    expectEqual(int(evalText("PRINT 16", rt, out).value), -3, "slot 16 = -3");
}

CASE("PRT is an alias of PRINT (same store)") {
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    evalText("PRT 2 42", rt, out);
    expectEqual(int(evalText("PRINT 2", rt, out).value), 42, "PRT writes the PRINT store");
    expectEqual(int(evalText("PRT 2", rt, out).value), 42, "PRT reads it back");
}

CASE("slots are independent") {
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    evalText("PRINT 1 11", rt, out);
    evalText("PRINT 2 22", rt, out);
    expectEqual(int(evalText("PRINT 1", rt, out).value), 11, "slot 1 unchanged");
    expectEqual(int(evalText("PRINT 2", rt, out).value), 22, "slot 2 unchanged");
}

CASE("init clears the dashboard") {
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    expectEqual(int(evalText("PRINT 1", rt, out).value), 0, "fresh slot reads 0");
}

CASE("int16 values round-trip without clamping") {
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    evalText("PRINT 3 -12345", rt, out);
    expectEqual(int(evalText("PRINT 3", rt, out).value), -12345, "raw int16 stored");
}

CASE("slot index out of range is OutOfRange, no write") {
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    expectTrue(evalText("PRINT 0 5", rt, out).error == TT2EvalError::OutOfRange,
               "slot 0 rejected");
    expectTrue(evalText("PRINT 17 5", rt, out).error == TT2EvalError::OutOfRange,
               "slot 17 rejected");
}

}
