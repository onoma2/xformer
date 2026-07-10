#include "UnitTest.h"

#include "engine/TT2Runner.h"

#include "model/Types.h"

extern "C" {
#include "command.h"
#include "tt_parser.h"
#include "ops/op_enum.h"
}

// The iterating mods (L / W / P.MAP / PN.MAP) run their bodies synchronously in
// one engine frame. A shared per-line op budget must stop a runaway loop with an
// error rather than let it starve audio on the shared engine thread.
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

TT2EvalResult runLine(const char *text) {
    TeletypeProgram program = {};
    init(program);
    writeLine(program.scripts[0], 0, text);
    program.scripts[0].length = 1;

    TT2Runtime runtime = {};
    init(runtime);
    TT2OutputState output = {};
    init(output);

    return runScript(program, runtime, output, 0);
}

} // namespace

UNIT_TEST("TT2ExecutionBudget") {

CASE("L exceeding the budget errors instead of running to completion") {
    auto r = runLine("L 1 30000: A + A 1");
    expectEqual(int(r.error), int(TT2EvalError::BudgetExceeded), "L runaway capped");
}

CASE("W exceeding the budget errors") {
    auto r = runLine("W 1: A + A 1");   // condition always true
    expectEqual(int(r.error), int(TT2EvalError::BudgetExceeded), "W runaway capped");
}

CASE("nested loops compound within one budget") {
    auto r = runLine("L 1 100: L 1 100: A + A 1");   // 10000 body evals
    expectEqual(int(r.error), int(TT2EvalError::BudgetExceeded), "nested compounds");
}

CASE("short loop is unaffected") {
    auto r = runLine("L 1 10: A + A 1");
    expectEqual(int(r.error), int(TT2EvalError::None), "short loop ok");
}

CASE("DEL.X with a huge copy count honors the budget") {
    auto r = runLine("DEL.X 30000 1: A 1");
    expectEqual(int(r.error), int(TT2EvalError::BudgetExceeded), "DEL.X enqueue runaway capped");
}

}
