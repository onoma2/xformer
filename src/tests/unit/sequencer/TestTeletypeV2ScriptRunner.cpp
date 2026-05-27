#include "UnitTest.h"

#include "engine/TT2Runner.h"

// Pre-include model/Types.h in C++ mode so that when teletype.h pulls in
// state.h -> types.h inside extern "C", the C++ templates are already
// processed and skipped by #pragma once.
#include "model/Types.h"

extern "C" {
#include "command.h"
#include "teletype.h"
#include "ops/op_enum.h"
}

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

UNIT_TEST("TeletypeV2ScriptRunner") {

    CASE("empty_script_succeeds") {
        TeletypeProgram program = {};
        init(program);
        program.scripts[0].length = 0;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "empty ok");
    }

    CASE("single_line_cv") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "CV 1 5000");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.cv[0], int16_t(5000), "CV[0]");
        expectEqual(output.cv[0].targetRaw, int16_t(5000), "targetRaw");
        expectEqual(int(output.cvDirty), 1, "dirty");
    }

    CASE("multi_line_script") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "A 10");
        writeLine(program.scripts[0], 1, "B + A 5");
        writeLine(program.scripts[0], 2, "CV 1 B");
        program.scripts[0].length = 3;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(10), "A = 10");
        expectEqual(runtime.variables.b, int16_t(15), "B = 15");
        expectEqual(runtime.variables.cv[0], int16_t(15), "CV[0] = 15");
        expectEqual(int(output.cvDirty), 1, "dirty");
    }

    CASE("execution_context_tracked") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "A 1");
        writeLine(program.scripts[0], 1, "A 2");
        program.scripts[0].length = 2;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        // After runScript, depth is back to 0 (frame popped).
        // frame[0] still contains the last script context.
        expectEqual(int(runtime.exec.depth), 0, "depth popped");
        expectEqual(int(runtime.exec.frames[0].script_number), 0, "script number");
        // Last line executed was line 1.
        expectEqual(int(runtime.exec.frames[0].line_number), 1, "line number");
    }

    CASE("invalid_script_index") {
        TeletypeProgram program = {};
        init(program);

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 99);
        expectEqual(int(result.error), int(TT2EvalError::InvalidScriptIndex),
                    "invalid index rejected");
    }

    CASE("unsupported_op_stops_and_reports_line") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "A 10");
        writeLine(program.scripts[0], 1, "SUB 1 2"); // unsupported in native table
        writeLine(program.scripts[0], 2, "B 20");
        program.scripts[0].length = 3;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::UnsupportedOp),
                    "unsupported op stops");
        // A should have been set, B should not.
        expectEqual(runtime.variables.a, int16_t(10), "A set before error");
        expectEqual(runtime.variables.b, int16_t(2), "B not set after error");
        // Execution context should show the failing line.
        expectEqual(int(runtime.exec.frames[0].line_number), 1,
                    "line 1 reported");
    }

    CASE("blank_line_skipped") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "A 10");
        program.scripts[0].commands[1].length = 0; // blank line
        writeLine(program.scripts[0], 2, "B 20");
        program.scripts[0].length = 3;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(10), "A set");
        expectEqual(runtime.variables.b, int16_t(20), "B set after blank");
    }

    CASE("script_length_overflow_rejected") {
        TeletypeProgram program = {};
        init(program);
        program.scripts[0].length = TT2_COMMANDS_PER_SCRIPT + 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::ScriptLengthOverflow),
                    "overflow rejected");
    }

    CASE("script_index_at_boundary") {
        TeletypeProgram program = {};
        init(program);
        // TT2_SCRIPT_COUNT - 1 is the last valid script.
        writeLine(program.scripts[TT2_SCRIPT_COUNT - 1], 0, "A 42");
        program.scripts[TT2_SCRIPT_COUNT - 1].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output,
                                TT2_SCRIPT_COUNT - 1);
        expectEqual(int(result.error), int(TT2EvalError::None), "boundary ok");
        expectEqual(runtime.variables.a, int16_t(42), "A set");
    }

    CASE("eval_error_stops_execution") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "A 10");
        writeLine(program.scripts[0], 1, "CV 9 1"); // out of range
        writeLine(program.scripts[0], 2, "B 20");
        program.scripts[0].length = 3;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::OutOfRange),
                    "out of range stops");
        expectEqual(runtime.variables.a, int16_t(10), "A set before error");
        expectEqual(runtime.variables.b, int16_t(2), "B not set after error");
        expectEqual(int(runtime.exec.frames[0].line_number), 1,
                    "line 1 reported");
    }

    CASE("sub_separator_executes_both_segments") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "X 1; B 2");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.x, int16_t(1), "X = 1");
        expectEqual(runtime.variables.b, int16_t(2), "B = 2");
    }

    CASE("sub_separator_two_cv_sets") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "CV 1 100; CV 2 200");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.cv[0], int16_t(100), "CV[0] = 100");
        expectEqual(runtime.variables.cv[1], int16_t(200), "CV[1] = 200");
        expectEqual(int(output.cvDirty), 0x03, "both CV dirty");
    }

    CASE("if_true_executes_body") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "IF 1: CV 1 1000");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.cv[0], int16_t(1000), "CV[0] = 1000");
        expectEqual(int(output.cvDirty), 1, "CV dirty");
    }

    CASE("if_false_skips_body") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "IF 0: CV 1 1000");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.cv[0], int16_t(0), "CV[0] unchanged");
        expectEqual(int(output.cvDirty), 0, "no CV dirty");
    }

    CASE("sub_separator_failure_stops_later_segment") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "CV 1 100; CV 9 1");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::OutOfRange),
                    "out of range stops");
        expectEqual(runtime.variables.cv[0], int16_t(100),
                    "CV[0] set before fail");
        expectEqual(int(output.cvDirty), 1, "first CV dirty only");
    }

    CASE("if_variable_condition_true") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "IF A: CV 1 1000");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        runtime.variables.a = 1;
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.cv[0], int16_t(1000), "CV[0] = 1000");
    }

    CASE("if_variable_condition_false") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "IF A: CV 1 1000");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        runtime.variables.a = 0;
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.cv[0], int16_t(0), "CV[0] unchanged");
    }

    CASE("if_expression_condition") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "IF + A 0: CV 1 1000");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        runtime.variables.a = 1;
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.cv[0], int16_t(1000), "CV[0] = 1000");
    }

    CASE("unsupported_mod_rejected_before_prefix_side_effects") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "W CV 1 100: B 2");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::UnsupportedMod),
                    "W rejected");
        expectEqual(runtime.variables.cv[0], int16_t(0),
                    "CV[0] not set by prefix");
        expectEqual(runtime.variables.b, int16_t(2),
                    "B not set by body");
    }

    CASE("unsupported_mod_rejected_before_body_side_effects") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "W 1: B 2");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::UnsupportedMod),
                    "W rejected");
        expectEqual(runtime.variables.b, int16_t(2),
                    "B not set by body");
    }

    CASE("if_empty_prefix_invalid_arity") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "IF: CV 1 1000");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::InvalidModArity),
                    "empty IF prefix rejected");
        expectEqual(runtime.variables.cv[0], int16_t(0), "CV[0] unchanged");
    }

    CASE("line_number_correct_on_separator_failure") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "A 10");
        writeLine(program.scripts[0], 1, "CV 1 100; CV 9 1");
        writeLine(program.scripts[0], 2, "B 20");
        program.scripts[0].length = 3;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::OutOfRange),
                    "out of range stops");
        expectEqual(int(runtime.exec.frames[0].line_number), 1,
                    "line 1 reported");
    }

    CASE("exec_frame_cleared") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "A 42");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        // Poison frame 0 with stale values from a prior (imaginary) run.
        TT2ExecFrame &frame = runtime.exec.frames[0];
        frame.if_else_condition = 1;
        frame.i = 99;
        frame.while_continue = 1;
        frame.while_depth = 42;
        frame.breaking = 1;
        frame.delayed = 1;
        frame.fparam1 = 77;
        frame.fparam2 = 88;
        frame.fresult = 123;
        frame.fresult_set = 1;
        // script_number and line_number are also poisoned.
        frame.script_number = 7;
        frame.line_number = 5;

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(42), "A set");

        // Stale flags must be cleared.
        expectEqual(int(frame.if_else_condition), 0, "if_else cleared");
        expectEqual(frame.i, int16_t(0), "i cleared");
        expectEqual(int(frame.while_continue), 0, "while_continue cleared");
        expectEqual(int(frame.while_depth), 0, "while_depth cleared");
        expectEqual(int(frame.breaking), 0, "breaking cleared");
        expectEqual(int(frame.delayed), 0, "delayed cleared");
        expectEqual(frame.fparam1, int16_t(0), "fparam1 cleared");
        expectEqual(frame.fparam2, int16_t(0), "fparam2 cleared");
        expectEqual(frame.fresult, int16_t(0), "fresult cleared");
        expectEqual(int(frame.fresult_set), 0, "fresult_set cleared");

        // Active context must be set.
        expectEqual(int(frame.script_number), 0, "script_number set");
        expectEqual(int(frame.line_number), 0, "line_number set");
    }

    CASE("if_true_skips_else") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "IF 1: A 10; ELSE: A 20");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(10), "IF branch executed");
    }

    CASE("if_false_executes_else") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "IF 0: A 10; ELSE: A 20");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(20), "ELSE branch executed");
    }

    CASE("elif_true_executes_and_skips_else") {
        TeletypeProgram program = {};
        init(program);
        // Empty IF body (": ;") keeps token count under the 16 limit.
        writeLine(program.scripts[0], 0, "IF 0: ; ELIF 1: B 10; ELSE: X 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(1), "A unchanged (default 1)");
        expectEqual(runtime.variables.b, int16_t(10), "ELIF branch executed");
        expectEqual(runtime.variables.x, int16_t(0), "X unchanged (default 0)");
    }

    CASE("all_false_executes_else") {
        TeletypeProgram program = {};
        init(program);
        // Empty bodies for IF and ELIF to stay under 16 tokens.
        writeLine(program.scripts[0], 0, "IF 0: ; ELIF 0: ; ELSE: X 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(1), "A unchanged");
        expectEqual(runtime.variables.b, int16_t(2), "B unchanged");
        expectEqual(runtime.variables.x, int16_t(10), "ELSE branch executed");
    }

    CASE("all_false_no_else_noops") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "IF 0: A 10; ELIF 0: B 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(1), "A unchanged");
        expectEqual(runtime.variables.b, int16_t(2), "B unchanged");
    }

    CASE("orphan_else_fails") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "ELSE: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::OrphanElse),
                    "orphan ELSE rejected");
        expectEqual(runtime.variables.a, int16_t(1), "A unchanged");
    }

    CASE("orphan_elif_fails") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "ELIF 1: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::OrphanElif),
                    "orphan ELIF rejected");
        expectEqual(runtime.variables.a, int16_t(1), "A unchanged");
    }

    CASE("duplicate_else_fails") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "IF 0: A 10; ELSE: A 20; ELSE: A 30");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::DuplicateElse),
                    "duplicate ELSE rejected");
        expectEqual(runtime.variables.a, int16_t(20), "first ELSE executed");
    }

    CASE("prob_100_runs") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "PROB 100: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(10), "PROB 100 runs");
    }

    CASE("prob_0_skips") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "PROB 0: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(1), "PROB 0 skips");
    }

    CASE("prob_negative_skips") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "PROB -1: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(1), "PROB -1 skips");
    }

    CASE("prob_101_runs") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "PROB 101: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(10), "PROB 101 runs");
    }

    CASE("prob_seeded_mid_value_deterministic") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "PROB 50: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        // With Prob state = 0, LCG first value is 1013904223.
        // roll = 1013904223 / 42949672 = 23, so 23 < 50 → runs.
        runtime.rng.state[static_cast<uint8_t>(TT2RngSlot::Prob)] = 0;
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(10),
                    "PROB 50 runs with seeded roll 23");
    }

    CASE("prob_body_side_effects_only_when_runs") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "PROB 0: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(1), "A unchanged when PROB skips");
        expectEqual(int(output.cvDirty), 0, "no CV side effects");
    }

    CASE("prob_empty_prefix_invalid_arity") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "PROB: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::InvalidModArity),
                    "empty PROB prefix rejected");
        expectEqual(runtime.variables.a, int16_t(1), "A unchanged");
    }

    CASE("prob_extra_params_invalid_arity") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "PROB 50 60: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::InvalidModArity),
                    "PROB with extra params rejected");
        expectEqual(runtime.variables.a, int16_t(1), "A unchanged");
    }

    CASE("prob_skipped_allows_else") {
        // PROB participates in conditional chains.
        // A skipped PROB leaves the chain Pending, so ELSE can run.
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "PROB 0: A 10; ELSE: A 20");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(20), "ELSE executed after PROB 0");
    }

    CASE("prob_in_if_chain_blocks_else") {
        // IF sets Pending; PROB executes and sets Taken; ELSE is skipped.
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "IF 0: ; PROB 100: A 10; ELSE: A 20");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(10), "PROB executed");
    }

    CASE("prob_skipped_when_prior_if_taken") {
        // IF sets Taken; PROB sees Taken and skips its body.
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "IF 1: ; PROB 100: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(1), "PROB skipped because IF was taken");
    }


    CASE("l_1_to_3_sets_a_to_final_i") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "L 1 3: A I");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(3), "A = final I");
    }

    CASE("l_0_to_2_accumulates_count") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "L 0 2: A + A 1");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(4), "A += 1 three times");
    }

    CASE("l_reversed_bounds_descends") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "L 3 1: A I");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(1), "A = final I (descended to 1)");
    }

    CASE("l_invalid_arity_1_param_rejected") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "L 1: A I");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::InvalidModArity),
                    "L with 1 param rejected");
        expectEqual(runtime.variables.a, int16_t(1), "A unchanged");
    }

    CASE("l_invalid_arity_3_params_rejected") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "L 1 3 5: A I");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::InvalidModArity),
                    "L with 3 params rejected");
        expectEqual(runtime.variables.a, int16_t(1), "A unchanged");
    }

    CASE("l_nested_two_levels") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "L 0 1: L 0 1: X + X 1");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.x, int16_t(4), "X = 4 after nested loops");
    }

    CASE("l_body_failure_stops_loop_and_reports") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "L 1 3: CV 9 1");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::OutOfRange),
                    "body failure stops L loop");
    }

    CASE("l_i_left_at_final_in_range_value") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "L 1 3: A I");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(3), "A = final I");
        // I is left at the final in-range value.
        expectEqual(runtime.exec.frames[0].i, int16_t(3), "I left at final loop value");
    }

    CASE("l_single_iteration_equal_bounds") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "L 5 5: A I");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(5), "A = 5");
    }

    CASE("l_negative_to_positive_bounds") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "L -2 1: A I");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(1), "A = final I = 1");
    }

    CASE("l_variable_bounds") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "X 1");
        writeLine(program.scripts[0], 1, "B 3");
        writeLine(program.scripts[0], 2, "L X B: A I");
        program.scripts[0].length = 3;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(3), "A = final I = 3");
    }

    CASE("l_body_mutation_carries_across_iterations") {
        TeletypeProgram program = {};
        init(program);
        // L 0 1: I + I 10; A I
        // Iter 1: I=0, set I=10, A=10. Loop bumps I=11.
        // Iter 2: I=11, set I=21, A=21. Loop bumps I=22, back-off to 21.
        writeLine(program.scripts[0], 0, "L 0 1: I + I 10; A I");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(21), "A saw mutated I=21");
        expectEqual(runtime.exec.frames[0].i, int16_t(21), "I = 21 after loop");
    }

    CASE("l_does_not_double_execute_trailing_segment") {
        TeletypeProgram program = {};
        init(program);
        // L 1 1: A I; B + B 10
        // One iteration: A=1, B=2+10=12.
        // If the trailing segment ran again outside L: B=12+10=22 (wrong).
        writeLine(program.scripts[0], 0, "L 1 1: A I; B + B 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(1), "A = 1");
        expectEqual(runtime.variables.b, int16_t(12), "B = 12, not double-executed");
    }


    // Old Teletype semantics: EVERY tick does count++ then count %= mod.
    // EVERY fires when count == 0 after modulo.
    // SKIP fires when count != 0 after modulo.

    CASE("every_1_fires_every_call") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "EVERY 1: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r1 = runScript(program, runtime, output, 0);
        expectEqual(int(r1.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(10), "fires call 1");

        auto r2 = runScript(program, runtime, output, 0);
        expectEqual(int(r2.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(10), "fires call 2");
    }

    CASE("every_2_fires_on_second_call") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "EVERY 2: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r1 = runScript(program, runtime, output, 0);
        expectEqual(int(r1.error), int(TT2EvalError::None), "pass1 skip");
        expectEqual(runtime.variables.a, int16_t(1), "skip");

        auto r2 = runScript(program, runtime, output, 0);
        expectEqual(int(r2.error), int(TT2EvalError::None), "pass2 fire");
        expectEqual(runtime.variables.a, int16_t(10), "fire");

        auto r3 = runScript(program, runtime, output, 0);
        expectEqual(int(r3.error), int(TT2EvalError::None), "pass3 skip again");
        expectEqual(runtime.variables.a, int16_t(10), "A stays (skip)");

        auto r4 = runScript(program, runtime, output, 0);
        expectEqual(int(r4.error), int(TT2EvalError::None), "pass4 fire again");
        expectEqual(runtime.variables.a, int16_t(10), "fire");
    }

    CASE("every_3_fires_on_third_call") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "EVERY 3: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r1 = runScript(program, runtime, output, 0);
        expectEqual(int(r1.error), int(TT2EvalError::None), "pass1 skip");
        expectEqual(runtime.variables.a, int16_t(1), "skip");

        auto r2 = runScript(program, runtime, output, 0);
        expectEqual(int(r2.error), int(TT2EvalError::None), "pass2 skip");
        expectEqual(runtime.variables.a, int16_t(1), "skip");

        auto r3 = runScript(program, runtime, output, 0);
        expectEqual(int(r3.error), int(TT2EvalError::None), "pass3 fire");
        expectEqual(runtime.variables.a, int16_t(10), "fire");
    }

    CASE("every_per_script_isolation") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "A + A 1");
        writeLine(program.scripts[0], 1, "EVERY 2: A + A 1");
        writeLine(program.scripts[1], 0, "EVERY 1: X 42");
        program.scripts[0].length = 2;
        program.scripts[1].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r0a = runScript(program, runtime, output, 0);
        expectEqual(int(r0a.error), int(TT2EvalError::None), "s0 pass1");
        expectEqual(runtime.variables.a, int16_t(2), "A=2");

        auto r1 = runScript(program, runtime, output, 1);
        expectEqual(int(r1.error), int(TT2EvalError::None), "s1 pass1");
        expectEqual(runtime.variables.x, int16_t(42), "X=42");

        auto r0b = runScript(program, runtime, output, 0);
        expectEqual(int(r0b.error), int(TT2EvalError::None), "s0 pass2");
        expectEqual(runtime.variables.a, int16_t(4), "A=4");
    }

    CASE("every_per_line_persistence") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "EVERY 2: A 10");
        writeLine(program.scripts[0], 1, "EVERY 3: X 20");
        program.scripts[0].length = 2;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r1 = runScript(program, runtime, output, 0);
        expectEqual(int(r1.error), int(TT2EvalError::None), "pass1");
        expectEqual(runtime.variables.a, int16_t(1), "A default");
        expectEqual(runtime.variables.x, int16_t(0), "X default");

        auto r2 = runScript(program, runtime, output, 0);
        expectEqual(int(r2.error), int(TT2EvalError::None), "pass2");
        expectEqual(runtime.variables.a, int16_t(10), "A=10");
        expectEqual(runtime.variables.x, int16_t(0), "X=0");

        auto r3 = runScript(program, runtime, output, 0);
        expectEqual(int(r3.error), int(TT2EvalError::None), "pass3");
        expectEqual(runtime.variables.a, int16_t(10), "A stays 10");
        expectEqual(runtime.variables.x, int16_t(20), "X=20");
    }

    CASE("every_init_resets_counters") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "EVERY 2: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r1 = runScript(program, runtime, output, 0);
        expectEqual(int(r1.error), int(TT2EvalError::None), "pass1");
        expectEqual(runtime.variables.a, int16_t(1), "skip");

        init(runtime);

        auto r2 = runScript(program, runtime, output, 0);
        expectEqual(int(r2.error), int(TT2EvalError::None), "pass2");
        expectEqual(runtime.variables.a, int16_t(1), "skip after reset");
    }

    CASE("skip_2_runs_first_skips_second") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "SKIP 2: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r1 = runScript(program, runtime, output, 0);
        expectEqual(int(r1.error), int(TT2EvalError::None), "pass1 fire");
        expectEqual(runtime.variables.a, int16_t(10), "A set call 1");

        auto r2 = runScript(program, runtime, output, 0);
        expectEqual(int(r2.error), int(TT2EvalError::None), "pass2 skip");
        expectEqual(runtime.variables.a, int16_t(10), "A unchanged call 2");

        auto r3 = runScript(program, runtime, output, 0);
        expectEqual(int(r3.error), int(TT2EvalError::None), "pass3 fire");
        expectEqual(runtime.variables.a, int16_t(10), "A set call 3");
    }

    CASE("skip_and_every_opposite_phase") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "EVERY 2: A 10");
        writeLine(program.scripts[0], 1, "SKIP 2: X 20");
        program.scripts[0].length = 2;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        // Pass 1: every count=1%2=1 skip; skip count=1%2=1 fire.
        auto r1 = runScript(program, runtime, output, 0);
        expectEqual(int(r1.error), int(TT2EvalError::None), "pass1");
        expectEqual(runtime.variables.a, int16_t(1), "EVERY skip");
        expectEqual(runtime.variables.x, int16_t(20), "SKIP fire");

        // Pass 2: every count=2%2=0 fire; skip count=2%2=0 skip.
        auto r2 = runScript(program, runtime, output, 0);
        expectEqual(int(r2.error), int(TT2EvalError::None), "pass2");
        expectEqual(runtime.variables.a, int16_t(10), "EVERY fire");
        expectEqual(runtime.variables.x, int16_t(20), "X stays 20 (SKIP skip)");
    }

    CASE("every_ev_alias_works") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "EV 1: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(10), "A set via EV");
    }

    CASE("every_side_effects_only_when_fires") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "EVERY 2: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(1), "A unchanged");
        expectEqual(int(output.cvDirty), 0, "no CV side effects");
    }

    CASE("every_arity_error") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "EVERY: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::InvalidModArity),
                    "EMPTY prefix rejected");
    }

    CASE("every_negative_mod_treated_as_positive") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "EVERY -2: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r1 = runScript(program, runtime, output, 0);
        expectEqual(int(r1.error), int(TT2EvalError::None), "pass1 skip");
        expectEqual(runtime.variables.a, int16_t(1), "skip");

        auto r2 = runScript(program, runtime, output, 0);
        expectEqual(int(r2.error), int(TT2EvalError::None), "pass2 fire");
        expectEqual(runtime.variables.a, int16_t(10), "A set");
    }

    CASE("every_zero_mod_treated_as_one") {
        // EVERY 0 normalizes to EVERY 1: fires every call.
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "EVERY 0: A 10");
        program.scripts[0].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r1 = runScript(program, runtime, output, 0);
        expectEqual(int(r1.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(10), "fires on EVERY 0");
    }

    // --- SCRIPT nested execution tests ---

    CASE("script_calls_child_and_continues") {
        // Parent script 0 has two lines:
        //   SCRIPT 2 ; B 20
        // Child script 1 sets A = 10.
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "SCRIPT 2; B 20");
        program.scripts[0].length = 1;
        writeLine(program.scripts[1], 0, "A 10");
        program.scripts[1].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(10), "A set by child");
        expectEqual(runtime.variables.b, int16_t(20), "B set after SCRIPT");
    }

    CASE("script_child_error_stops_parent") {
        // Parent calls a child that errors (CV 9 1 out of range).
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "A 10; SCRIPT 2; B 20");
        program.scripts[0].length = 1;
        writeLine(program.scripts[1], 0, "CV 9 1");
        program.scripts[1].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::OutOfRange), "child error");
        expectEqual(runtime.variables.a, int16_t(10), "A set before child");
        expectEqual(runtime.variables.b, int16_t(2), "B not set after child error");
    }

    CASE("script_parent_i_restored_after_child") {
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "I 5; A I; SCRIPT 2; B I");
        program.scripts[0].length = 1;
        writeLine(program.scripts[1], 0, "I 99; X 10");
        program.scripts[1].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        expectEqual(runtime.variables.a, int16_t(5), "A = parent I before child");
        expectEqual(runtime.variables.b, int16_t(5), "B = parent I = 5 (restored)");
        expectEqual(runtime.variables.x, int16_t(10), "X set by child");
    }

    CASE("script_child_i_isolated_from_parent") {
        // Parent: I=5, calls SCRIPT 2, reads B=I. Child: I=99.
        // Child's I mutation must not leak to parent.
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "I 5; SCRIPT 2; B I");
        program.scripts[0].length = 1;
        writeLine(program.scripts[1], 0, "I 99");
        program.scripts[1].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::None), "ok");
        // I was set to 5 in parent; child sets 99 in its own frame.
        // After child returns, parent's B should read parent's I=5.
        expectEqual(runtime.variables.a, int16_t(1), "A unchanged");
        expectEqual(runtime.variables.b, int16_t(5), "B = parent I = 5 (child isolated)");
    }

    CASE("script_depth_overflow_stop") {
        // s0 -> s1 -> s0 -> s1 ... until depth overflow at TT2_EXEC_DEPTH.
        TeletypeProgram program = {};
        init(program);
        writeLine(program.scripts[0], 0, "SCRIPT 2"); // calls s1
        program.scripts[0].length = 1;
        writeLine(program.scripts[1], 0, "SCRIPT 1"); // calls s0
        program.scripts[1].length = 1;

        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto result = runScript(program, runtime, output, 0);
        expectEqual(int(result.error), int(TT2EvalError::ExecDepthOverflow), "overflow");
        expectEqual(int(runtime.exec.overflow), 1, "overflow flagged");
    }
}

