#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"
#include "engine/TT2Runner.h"

#include "model/Types.h"

extern "C" {
#include "command.h"
#include "teletype.h"
#include "ops/op_enum.h"
}

namespace {

void writeLine(TT2Script &script, uint8_t line, const char *text) {
    tele_command_t src = {};
    char errMsg[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, errMsg);
    lowerCommand(src, script.commands[line]);
}

int16_t evalProg(const char *text, TT2Runtime &runtime, TT2OutputState &output,
                 const TeletypeProgram &program) {
    tele_command_t src = {};
    char errMsg[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, errMsg);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    return evaluateCommand(cmd, runtime, output, &program).value;
}

} // namespace

UNIT_TEST("TeletypeV2Function") {

    CASE("f_call_returns_fresult") {
        TeletypeProgram program; init(program);
        writeLine(program.scripts[1], 0, "FR 42");
        program.scripts[1].length = 1;
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        expectEqual(int(evalProg("$F 2", runtime, output, program)), 42, "$ returns FR");
    }

    CASE("f1_passes_one_param") {
        TeletypeProgram program; init(program);
        writeLine(program.scripts[1], 0, "FR ADD I1 10");
        program.scripts[1].length = 1;
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        // $F1 i x : param i = 5, script x = 2 -> I1 + 10 = 15
        expectEqual(int(evalProg("$F1 5 2", runtime, output, program)), 15, "I1 received");
    }

    CASE("f2_param_order") {
        TeletypeProgram program; init(program);
        writeLine(program.scripts[1], 0, "FR SUB I1 I2");
        program.scripts[1].length = 1;
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        // $F2 i j x (script 2): I1=4, I2=3 -> SUB = 1 (parity with upstream).
        expectEqual(int(evalProg("$F2 3 4 2", runtime, output, program)), 1, "I1-I2 order");
    }

    CASE("l_call_runs_one_line") {
        TeletypeProgram program; init(program);
        writeLine(program.scripts[1], 0, "FR 7");
        program.scripts[1].length = 1;
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        // $L i x : line i=1, script x=2 -> line index 0 of script 1
        expectEqual(int(evalProg("$L 1 2", runtime, output, program)), 7, "line executed");
    }

    CASE("fr_default_zero_when_unset") {
        TeletypeProgram program; init(program);
        writeLine(program.scripts[1], 0, "A 5");  // no FR set
        program.scripts[1].length = 1;
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        expectEqual(int(evalProg("$F 2", runtime, output, program)), 0, "no FR -> 0");
    }

    CASE("hw_parity_SCRIPT_reaches_metro_and_init") {
        // Full teletype: SCRIPT 1..8 = numbered, SCRIPT 9 = Metro, SCRIPT 10 = Init.
        TeletypeProgram program; init(program);
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        writeLine(program.scripts[7], 0, "A 8");                 // numbered 8 (idx 7)
        program.scripts[7].length = 1;
        writeLine(program.scripts[TT2_METRO_SCRIPT], 0, "B 9");  // Metro (idx 8) <- SCRIPT 9
        program.scripts[TT2_METRO_SCRIPT].length = 1;
        writeLine(program.scripts[TT2_INIT_SCRIPT], 0, "X 10");  // Init  (idx 9) <- SCRIPT 10
        program.scripts[TT2_INIT_SCRIPT].length = 1;
        auto runCmd = [&](const char *text) {
            tele_command_t src = {}; char err[TELE_ERROR_MSG_LENGTH] = {};
            parse(text, &src, err);
            TT2Command cmd = {}; lowerCommand(src, cmd);
            return evaluateCommand(cmd, runtime, output, &program);
        };
        runCmd("SCRIPT 8");   expectEqual(int(runtime.variables.a), 8,  "SCRIPT 8 -> numbered 8");
        runCmd("SCRIPT 9");   expectEqual(int(runtime.variables.b), 9,  "SCRIPT 9 -> Metro");
        runCmd("SCRIPT 10");  expectEqual(int(runtime.variables.x), 10, "SCRIPT 10 -> Init");
        auto r = runCmd("SCRIPT 11");
        expectTrue(r.error == TT2EvalError::OutOfRange, "SCRIPT 11 rejected (only 10 editable)");
    }
}
