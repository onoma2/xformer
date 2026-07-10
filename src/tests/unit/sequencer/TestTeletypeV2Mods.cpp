#include "UnitTest.h"

#include "engine/TT2Runner.h"

#include "model/Types.h"

extern "C" {
#include "command.h"
#include "tt_parser.h"
#include "ops/op_enum.h"
}

// Coverage for the four core Teletype pre-command mods that were unported in
// the native TT2 evaluator: W (while), OTHER, P.MAP, PN.MAP.
namespace {

struct ParseResult {
    tele_error_t error;
    char errorMsg[TELE_ERROR_MSG_LENGTH];
    tele_command_t cmd;
};

ParseResult tryParse(const char *text) {
    ParseResult result;
    result.cmd = {};
    result.errorMsg[0] = '\0';
    result.error = parse(text, &result.cmd, result.errorMsg);
    return result;
}

TT2Command lower(const tele_command_t &src) {
    TT2Command dst = {};
    lowerCommand(src, dst);
    return dst;
}

void writeLine(TT2Script &script, uint8_t lineIndex, const char *text) {
    auto r = tryParse(text);
    expectEqual(int(r.error), int(E_OK), "parse line");
    script.commands[lineIndex] = lower(r.cmd);
}

} // namespace

UNIT_TEST("TeletypeV2Mods") {

    // ---- W (while) ----

    CASE("w_reevaluates_condition_until_false") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "A 2");
        writeLine(program.scripts[0], 1, "W LT A 100: A * A A");
        program.scripts[0].length = 2;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(256), "A squared up to 256");
    }

    CASE("w_infinite_loop_stopped_by_op_budget") {
        // W's own 10000 cap is shadowed by the tighter shared op budget (§4.1):
        // an always-true condition stops with BudgetExceeded, not a silent cap.
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "A 0");
        writeLine(program.scripts[0], 1, "W 1: A + A 1");   // condition always true
        program.scripts[0].length = 2;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::BudgetExceeded),
                    "infinite W stopped by the op budget");
    }

    // ---- OTHER ----

    CASE("other_runs_when_every_did_not_fire") {
        // EVERY 2 fires on the 2nd tick; OTHER runs on the ticks EVERY skipped.
        // X marks EVERY firing, Y marks OTHER firing.
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "EVERY 2: X 1");
        writeLine(program.scripts[0], 1, "OTHER: Y 1");
        program.scripts[0].length = 2;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        // 1st run: EVERY counter -> 1, does NOT fire -> OTHER runs (Y=1, X untouched).
        runScript(program, runtime, output, 0);
        expectEqual(runtime.variables.x, int16_t(0), "EVERY did not fire on tick 1");
        expectEqual(runtime.variables.y, int16_t(1), "OTHER ran when EVERY skipped");

        // 2nd run: EVERY fires (X=1) -> OTHER does not run.
        runtime.variables.y = 0;
        runScript(program, runtime, output, 0);
        expectEqual(runtime.variables.x, int16_t(1), "EVERY fired on tick 2");
        expectEqual(runtime.variables.y, int16_t(0), "OTHER skipped when EVERY fired");
    }

    // ---- P.MAP ----

    CASE("pmap_maps_body_over_working_pattern_window") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "P.MAP: * 2 I");
        program.scripts[0].length = 1;

        // Working pattern 0, window [0,2] = {1,2,3}; cell 3 outside window.
        program.patterns[0].start = 0;
        program.patterns[0].end = 2;
        program.patterns[0].val[0] = 1;
        program.patterns[0].val[1] = 2;
        program.patterns[0].val[2] = 3;
        program.patterns[0].val[3] = 9;

        TT2Runtime runtime = {};
        init(runtime);
        runtime.variables.p_n = 0;
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(program.patterns[0].val[0], int16_t(2), "cell0 doubled");
        expectEqual(program.patterns[0].val[1], int16_t(4), "cell1 doubled");
        expectEqual(program.patterns[0].val[2], int16_t(6), "cell2 doubled");
        expectEqual(program.patterns[0].val[3], int16_t(9), "cell3 outside window untouched");
    }

    // ---- PN.MAP ----

    CASE("pnmap_maps_body_over_explicit_pattern") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "PN.MAP 1: + I 10");
        program.scripts[0].length = 1;

        program.patterns[1].start = 0;
        program.patterns[1].end = 1;
        program.patterns[1].val[0] = 5;
        program.patterns[1].val[1] = 7;

        TT2Runtime runtime = {};
        init(runtime);
        runtime.variables.p_n = 0;   // working pattern is 0, but PN.MAP targets 1
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(program.patterns[1].val[0], int16_t(15), "cell0 + 10");
        expectEqual(program.patterns[1].val[1], int16_t(17), "cell1 + 10");
    }

}
