#include "UnitTest.h"

#include "engine/TT2Evaluator.h"
#include "engine/TT2TrackEngine.h"

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

} // namespace

UNIT_TEST("TeletypeV2NativeOps") {

    CASE("output_state_minimal_size_and_init") {
        TT2OutputState output;
        init(output);

        expectEqual(int(sizeof(TT2CvOutput)), 2, "TT2CvOutput size");
        expectEqual(int(sizeof(TT2TrOutput)), 1, "TT2TrOutput size");
        expectEqual(int(sizeof(TT2OutputState)), 26, "TT2OutputState size");
        expectEqual(int(sizeof(TT2TrackEngine)), 96, "TT2TrackEngine size");
        expectEqual(int(output.cvDirty), 0, "CV dirty clear");
        expectEqual(int(output.trDirty), 0, "TR dirty clear");
        for (int i = 0; i < TT2_OUTPUT_CV_COUNT; i++) {
            expectEqual(output.cv[i].targetRaw, int16_t(0), "CV target init");
        }
        for (int i = 0; i < TT2_OUTPUT_TR_COUNT; i++) {
            expectEqual(int(output.tr[i].level), 0, "TR level init");
        }
    }

    CASE("runtime_scale_init") {
        TT2Runtime runtime = {};
        init(runtime);
        for (int i = 0; i < TT2_NB_SCALES; ++i) {
            expectEqual(runtime.variables.n_scale_bits[i], int16_t(0x0AB5),
                        "scale bits default");
            expectEqual(runtime.variables.n_scale_root[i], int16_t(0),
                        "scale root default");
        }
    }

    CASE("runtime_turtle_init") {
        TT2Runtime runtime = {};
        init(runtime);
        expectEqual(int(runtime.turtle.fence.x1), 0, "turtle fence x1");
        expectEqual(int(runtime.turtle.fence.y1), 0, "turtle fence y1");
        expectEqual(int(runtime.turtle.fence.x2), 3, "turtle fence x2");
        expectEqual(int(runtime.turtle.fence.y2), 63, "turtle fence y2");
        expectEqual(int(runtime.turtle.mode), int(TT2TurtleMode::Bump),
                    "turtle mode");
        expectEqual(int(runtime.turtle.heading), 180, "turtle heading");
        expectEqual(runtime.turtle.speed, int16_t(100), "turtle speed");
        expectEqual(int(runtime.turtle.scriptNumber), TT2_SCRIPT_COUNT,
                    "turtle script NO_SCRIPT");
        expectEqual(runtime.turtle.position.x, int32_t(0), "turtle x");
        expectEqual(runtime.turtle.position.y, int32_t(0), "turtle y");
        expectEqual(runtime.turtle.last.x, int32_t(0), "turtle last x");
        expectEqual(runtime.turtle.last.y, int32_t(0), "turtle last y");
    }

    CASE("variable_a_set_and_get") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        // A 10
        auto r = tryParse("A 10");
        expectEqual(int(r.error), int(E_OK), "parse A 10");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(runtime.variables.a, int16_t(10), "A set to 10");

        // A (get)
        r = tryParse("A");
        expectEqual(int(r.error), int(E_OK), "parse A");
        result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(result.value, int16_t(10), "A reads 10");
    }

    CASE("variable_b_set_and_get") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r = tryParse("B 20");
        expectEqual(int(r.error), int(E_OK), "parse B 20");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(runtime.variables.b, int16_t(20), "B set to 20");

        r = tryParse("B");
        expectEqual(int(r.error), int(E_OK), "parse B");
        result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(result.value, int16_t(20), "B reads 20");
    }

    CASE("variable_x_set_and_get") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r = tryParse("X 30");
        expectEqual(int(r.error), int(E_OK), "parse X 30");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(runtime.variables.x, int16_t(30), "X set to 30");
    }

    CASE("add_and_symbol_plus") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        // ADD 1 2 -> 3
        auto r = tryParse("ADD 1 2");
        expectEqual(int(r.error), int(E_OK), "parse ADD 1 2");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(result.value, int16_t(3), "ADD 1 2 = 3");

        // + 1 2 -> 3
        r = tryParse("+ 1 2");
        expectEqual(int(r.error), int(E_OK), "parse + 1 2");
        result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(result.value, int16_t(3), "+ 1 2 = 3");
    }

    CASE("math_with_variable_set") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        // B + 1 2  ->  B = 3
        auto r = tryParse("B + 1 2");
        expectEqual(int(r.error), int(E_OK), "parse B + 1 2");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(runtime.variables.b, int16_t(3), "B set to 3");
    }

    CASE("cv_set_and_dirty") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r = tryParse("CV 1 5000");
        expectEqual(int(r.error), int(E_OK), "parse CV 1 5000");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(runtime.variables.cv[0], int16_t(5000), "runtime CV[0]");
        expectEqual(output.cv[0].targetRaw, int16_t(5000), "output targetRaw[0]");
        expectEqual(int(output.cvDirty), 1, "dirty bit 0");
    }

    CASE("cv_output_8") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r = tryParse("CV 8 1234");
        expectEqual(int(r.error), int(E_OK), "parse CV 8 1234");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(runtime.variables.cv[7], int16_t(1234), "runtime CV[7]");
        expectEqual(output.cv[7].targetRaw, int16_t(1234), "output targetRaw[7]");
        expectEqual(int(output.cvDirty), 1 << 7, "dirty bit 7");
    }

    CASE("cv_out_of_range") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r = tryParse("CV 9 1");
        expectEqual(int(r.error), int(E_OK), "parse CV 9 1");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::OutOfRange),
                    "CV 9 out of range");
    }

    CASE("tr_set_and_dirty") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r = tryParse("TR 1 1");
        expectEqual(int(r.error), int(E_OK), "parse TR 1 1");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(runtime.variables.tr[0], int16_t(1), "runtime TR[0]");
        expectEqual(int(output.tr[0].level), 1, "output tr level 0");
        expectEqual(int(output.trDirty), 1, "tr dirty bit 0");
    }

    CASE("tr_output_8") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r = tryParse("TR 8 1");
        expectEqual(int(r.error), int(E_OK), "parse TR 8 1");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(runtime.variables.tr[7], int16_t(1), "runtime TR[7]");
        expectEqual(int(output.tr[7].level), 1, "output tr level 7");
        expectEqual(int(output.trDirty), 1 << 7, "tr dirty bit 7");
    }

    CASE("tr_out_of_range") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r = tryParse("TR 9 1");
        expectEqual(int(r.error), int(E_OK), "parse TR 9 1");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::OutOfRange),
                    "TR 9 out of range");
    }

    CASE("metro_interval_set") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r = tryParse("M 250");
        expectEqual(int(r.error), int(E_OK), "parse M 250");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(runtime.variables.m, int16_t(250), "metro interval");
    }

    CASE("metro_act_set") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r = tryParse("M.ACT 1");
        expectEqual(int(r.error), int(E_OK), "parse M.ACT 1");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(int(runtime.variables.m_act), 1, "metro running");

        r = tryParse("M.ACT 0");
        expectEqual(int(r.error), int(E_OK), "parse M.ACT 0");
        result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(int(runtime.variables.m_act), 0, "metro stopped");
    }

    CASE("unsupported_op") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        // N (pitch) is parsed but not yet implemented in the native table.
        auto r = tryParse("N 0");
        expectEqual(int(r.error), int(E_OK), "parse N 0");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::UnsupportedOp),
                    "N unsupported");
    }

    // -- batch 1: arithmetic / min-max / comparison / unary --------------

    auto evalValue = [](const char *text, int16_t &out) -> TT2EvalError {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);
        tele_command_t cmd = {};
        char msg[TELE_ERROR_MSG_LENGTH];
        if (parse(text, &cmd, msg) != E_OK) return TT2EvalError::UnsupportedOp;
        TT2Command lowered = {};
        lowerCommand(cmd, lowered);
        auto result = evaluateCommand(lowered, runtime, output);
        out = result.value;
        return result.error;
    };

    CASE("sub_order_and_symbol") {
        int16_t v = 0;
        expectEqual(int(evalValue("SUB 5 3", v)), int(TT2EvalError::None), "SUB ok");
        expectEqual(v, int16_t(2), "SUB 5 3 = 2");
        expectEqual(int(evalValue("- 5 3", v)), int(TT2EvalError::None), "- ok");
        expectEqual(v, int16_t(2), "- 5 3 = 2");
        expectEqual(int(evalValue("SUB 3 5", v)), int(TT2EvalError::None), "SUB ok");
        expectEqual(v, int16_t(-2), "SUB 3 5 = -2");
    }

    CASE("mul_and_clamp") {
        int16_t v = 0;
        expectEqual(int(evalValue("MUL 4 3", v)), int(TT2EvalError::None), "MUL ok");
        expectEqual(v, int16_t(12), "MUL 4 3 = 12");
        expectEqual(int(evalValue("* 4 3", v)), int(TT2EvalError::None), "* ok");
        expectEqual(v, int16_t(12), "* 4 3 = 12");
        expectEqual(int(evalValue("MUL 1000 1000", v)), int(TT2EvalError::None), "MUL ok");
        expectEqual(v, int16_t(32767), "MUL 1000 1000 clamps to INT16_MAX");
        expectEqual(int(evalValue("MUL -1000 1000", v)), int(TT2EvalError::None), "MUL ok");
        expectEqual(v, int16_t(-32768), "MUL -1000 1000 clamps to INT16_MIN");
    }

    CASE("div_trunc_and_zero_guard") {
        int16_t v = 0;
        expectEqual(int(evalValue("DIV 10 3", v)), int(TT2EvalError::None), "DIV ok");
        expectEqual(v, int16_t(3), "DIV 10 3 = 3 (truncated)");
        expectEqual(int(evalValue("/ 10 3", v)), int(TT2EvalError::None), "/ ok");
        expectEqual(v, int16_t(3), "/ 10 3 = 3");
        expectEqual(int(evalValue("DIV -7 2", v)), int(TT2EvalError::None), "DIV ok");
        expectEqual(v, int16_t(-3), "DIV -7 2 = -3 (trunc toward zero)");
        expectEqual(int(evalValue("DIV 7 0", v)), int(TT2EvalError::None), "DIV by 0 ok");
        expectEqual(v, int16_t(0), "DIV 7 0 = 0 (zero guard)");
    }

    CASE("mod_sign_and_zero_guard") {
        int16_t v = 0;
        expectEqual(int(evalValue("MOD 7 3", v)), int(TT2EvalError::None), "MOD ok");
        expectEqual(v, int16_t(1), "MOD 7 3 = 1");
        expectEqual(int(evalValue("% 7 3", v)), int(TT2EvalError::None), "% ok");
        expectEqual(v, int16_t(1), "% 7 3 = 1");
        expectEqual(int(evalValue("MOD -7 3", v)), int(TT2EvalError::None), "MOD ok");
        expectEqual(v, int16_t(-1), "MOD -7 3 = -1 (sign follows dividend)");
        expectEqual(int(evalValue("MOD 7 0", v)), int(TT2EvalError::None), "MOD by 0 ok");
        expectEqual(v, int16_t(0), "MOD 7 0 = 0 (zero guard)");
    }

    CASE("min_max") {
        int16_t v = 0;
        expectEqual(int(evalValue("MIN 3 5", v)), int(TT2EvalError::None), "MIN ok");
        expectEqual(v, int16_t(3), "MIN 3 5 = 3");
        expectEqual(int(evalValue("MAX 3 5", v)), int(TT2EvalError::None), "MAX ok");
        expectEqual(v, int16_t(5), "MAX 3 5 = 5");
    }

    CASE("comparisons") {
        int16_t v = 0;
        expectEqual(int(evalValue("EQ 4 4", v)), int(TT2EvalError::None), "EQ ok");
        expectEqual(v, int16_t(1), "EQ 4 4 = 1");
        expectEqual(int(evalValue("== 4 5", v)), int(TT2EvalError::None), "== ok");
        expectEqual(v, int16_t(0), "== 4 5 = 0");
        expectEqual(int(evalValue("NE 4 5", v)), int(TT2EvalError::None), "NE ok");
        expectEqual(v, int16_t(1), "NE 4 5 = 1");
        expectEqual(int(evalValue("!= 4 4", v)), int(TT2EvalError::None), "!= ok");
        expectEqual(v, int16_t(0), "!= 4 4 = 0");
        expectEqual(int(evalValue("LT 3 5", v)), int(TT2EvalError::None), "LT ok");
        expectEqual(v, int16_t(1), "LT 3 5 = 1");
        expectEqual(int(evalValue("< 5 3", v)), int(TT2EvalError::None), "< ok");
        expectEqual(v, int16_t(0), "< 5 3 = 0");
        expectEqual(int(evalValue("GT 5 3", v)), int(TT2EvalError::None), "GT ok");
        expectEqual(v, int16_t(1), "GT 5 3 = 1");
        expectEqual(int(evalValue("> 3 5", v)), int(TT2EvalError::None), "> ok");
        expectEqual(v, int16_t(0), "> 3 5 = 0");
        expectEqual(int(evalValue("LTE 4 4", v)), int(TT2EvalError::None), "LTE ok");
        expectEqual(v, int16_t(1), "LTE 4 4 = 1");
        expectEqual(int(evalValue("<= 5 4", v)), int(TT2EvalError::None), "<= ok");
        expectEqual(v, int16_t(0), "<= 5 4 = 0");
        expectEqual(int(evalValue("GTE 4 4", v)), int(TT2EvalError::None), "GTE ok");
        expectEqual(v, int16_t(1), "GTE 4 4 = 1");
        expectEqual(int(evalValue(">= 4 5", v)), int(TT2EvalError::None), ">= ok");
        expectEqual(v, int16_t(0), ">= 4 5 = 0");
    }

    CASE("abs_sgn") {
        int16_t v = 0;
        expectEqual(int(evalValue("ABS -7", v)), int(TT2EvalError::None), "ABS ok");
        expectEqual(v, int16_t(7), "ABS -7 = 7");
        expectEqual(int(evalValue("ABS 7", v)), int(TT2EvalError::None), "ABS ok");
        expectEqual(v, int16_t(7), "ABS 7 = 7");
        expectEqual(int(evalValue("SGN -5", v)), int(TT2EvalError::None), "SGN ok");
        expectEqual(v, int16_t(-1), "SGN -5 = -1");
        expectEqual(int(evalValue("SGN 0", v)), int(TT2EvalError::None), "SGN ok");
        expectEqual(v, int16_t(0), "SGN 0 = 0");
        expectEqual(int(evalValue("SGN 9", v)), int(TT2EvalError::None), "SGN ok");
        expectEqual(v, int16_t(1), "SGN 9 = 1");
    }

    CASE("cv_get") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        // Pre-seed runtime CV[0]
        runtime.variables.cv[0] = 1234;

        // CV 1 (get)
        auto r = tryParse("CV 1");
        expectEqual(int(r.error), int(E_OK), "parse CV 1");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(result.value, int16_t(1234), "CV 1 reads 1234");
    }

    CASE("tr_get") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        runtime.variables.tr[0] = 1;

        auto r = tryParse("TR 1");
        expectEqual(int(r.error), int(E_OK), "parse TR 1");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(result.value, int16_t(1), "TR 1 reads 1");
    }

    CASE("metro_get_default") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r = tryParse("M");
        expectEqual(int(r.error), int(E_OK), "parse M");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(result.value, int16_t(1000), "M reads default 1000");
    }

    CASE("metro_get_seeded") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        runtime.variables.m = 500;

        auto r = tryParse("M");
        expectEqual(int(r.error), int(E_OK), "parse M");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(result.value, int16_t(500), "M reads 500");
    }

    CASE("metro_clamp_minimum") {
        TT2Runtime runtime = {};
        init(runtime);
        TT2OutputState output = {};
        init(output);

        auto r = tryParse("M 1");
        expectEqual(int(r.error), int(E_OK), "parse M 1");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(runtime.variables.m, int16_t(2), "M clamped to 2");

        r = tryParse("M 0");
        expectEqual(int(r.error), int(E_OK), "parse M 0");
        result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::None), "eval ok");
        expectEqual(runtime.variables.m, int16_t(2), "M clamped to 2");
    }

    // -- batch 2: logic / range -----------------------------------------

    CASE("logic_and_or") {
        int16_t v = 0;
        expectEqual(int(evalValue("AND 1 1", v)), int(TT2EvalError::None), "AND ok");
        expectEqual(v, int16_t(1), "AND 1 1 = 1");
        expectEqual(int(evalValue("AND 1 0", v)), int(TT2EvalError::None), "AND ok");
        expectEqual(v, int16_t(0), "AND 1 0 = 0");
        expectEqual(int(evalValue("&& 5 3", v)), int(TT2EvalError::None), "&& ok");
        expectEqual(v, int16_t(1), "&& 5 3 = 1 (truthy operands)");
        expectEqual(int(evalValue("OR 0 0", v)), int(TT2EvalError::None), "OR ok");
        expectEqual(v, int16_t(0), "OR 0 0 = 0");
        expectEqual(int(evalValue("|| 0 7", v)), int(TT2EvalError::None), "|| ok");
        expectEqual(v, int16_t(1), "|| 0 7 = 1");
    }

    CASE("logic_and3_or3_and4_or4") {
        int16_t v = 0;
        expectEqual(int(evalValue("AND3 1 1 1", v)), int(TT2EvalError::None), "AND3 ok");
        expectEqual(v, int16_t(1), "AND3 1 1 1 = 1");
        expectEqual(int(evalValue("&&& 1 0 1", v)), int(TT2EvalError::None), "&&& ok");
        expectEqual(v, int16_t(0), "&&& 1 0 1 = 0");
        expectEqual(int(evalValue("OR3 0 0 0", v)), int(TT2EvalError::None), "OR3 ok");
        expectEqual(v, int16_t(0), "OR3 0 0 0 = 0");
        expectEqual(int(evalValue("||| 0 1 0", v)), int(TT2EvalError::None), "||| ok");
        expectEqual(v, int16_t(1), "||| 0 1 0 = 1");
        expectEqual(int(evalValue("AND4 1 1 1 1", v)), int(TT2EvalError::None), "AND4 ok");
        expectEqual(v, int16_t(1), "AND4 all 1 = 1");
        expectEqual(int(evalValue("&&&& 1 1 0 1", v)), int(TT2EvalError::None), "&&&& ok");
        expectEqual(v, int16_t(0), "&&&& 1 1 0 1 = 0");
        expectEqual(int(evalValue("OR4 0 0 0 0", v)), int(TT2EvalError::None), "OR4 ok");
        expectEqual(v, int16_t(0), "OR4 all 0 = 0");
        expectEqual(int(evalValue("|||| 0 0 1 0", v)), int(TT2EvalError::None), "|||| ok");
        expectEqual(v, int16_t(1), "|||| 0 0 1 0 = 1");
    }

    CASE("nz_ez") {
        int16_t v = 0;
        expectEqual(int(evalValue("NZ 5", v)), int(TT2EvalError::None), "NZ ok");
        expectEqual(v, int16_t(1), "NZ 5 = 1");
        expectEqual(int(evalValue("NZ 0", v)), int(TT2EvalError::None), "NZ ok");
        expectEqual(v, int16_t(0), "NZ 0 = 0");
        expectEqual(int(evalValue("EZ 0", v)), int(TT2EvalError::None), "EZ ok");
        expectEqual(v, int16_t(1), "EZ 0 = 1");
        expectEqual(int(evalValue("EZ 5", v)), int(TT2EvalError::None), "EZ ok");
        expectEqual(v, int16_t(0), "EZ 5 = 0");
    }

    CASE("lim_clamp") {
        int16_t v = 0;
        expectEqual(int(evalValue("LIM 5 0 10", v)), int(TT2EvalError::None), "LIM ok");
        expectEqual(v, int16_t(5), "LIM 5 0 10 = 5");
        expectEqual(int(evalValue("LIM -3 0 10", v)), int(TT2EvalError::None), "LIM ok");
        expectEqual(v, int16_t(0), "LIM -3 0 10 = 0");
        expectEqual(int(evalValue("LIM 99 0 10", v)), int(TT2EvalError::None), "LIM ok");
        expectEqual(v, int16_t(10), "LIM 99 0 10 = 10");
    }

    CASE("wrap_and_wrp") {
        int16_t v = 0;
        expectEqual(int(evalValue("WRAP 12 0 10", v)), int(TT2EvalError::None), "WRAP ok");
        expectEqual(v, int16_t(1), "WRAP 12 0 10 = 1");
        expectEqual(int(evalValue("WRAP -1 0 10", v)), int(TT2EvalError::None), "WRAP ok");
        expectEqual(v, int16_t(10), "WRAP -1 0 10 = 10");
        expectEqual(int(evalValue("WRP 12 0 10", v)), int(TT2EvalError::None), "WRP ok");
        expectEqual(v, int16_t(1), "WRP aliases WRAP");
    }

    CASE("avg_round") {
        int16_t v = 0;
        expectEqual(int(evalValue("AVG 4 6", v)), int(TT2EvalError::None), "AVG ok");
        expectEqual(v, int16_t(5), "AVG 4 6 = 5");
        expectEqual(int(evalValue("AVG 3 4", v)), int(TT2EvalError::None), "AVG ok");
        expectEqual(v, int16_t(4), "AVG 3 4 = 4 (rounds up)");
    }

    CASE("range_tests_inr_outr") {
        int16_t v = 0;
        // arg order matches upstream: lo x hi
        expectEqual(int(evalValue("INR 0 5 10", v)), int(TT2EvalError::None), "INR ok");
        expectEqual(v, int16_t(1), "INR 0 5 10 = 1");
        expectEqual(int(evalValue("INR 0 10 10", v)), int(TT2EvalError::None), "INR ok");
        expectEqual(v, int16_t(0), "INR exclusive upper = 0");
        expectEqual(int(evalValue("INRI 0 10 10", v)), int(TT2EvalError::None), "INRI ok");
        expectEqual(v, int16_t(1), "INRI inclusive upper = 1");
        expectEqual(int(evalValue("OUTR 0 5 10", v)), int(TT2EvalError::None), "OUTR ok");
        expectEqual(v, int16_t(0), "OUTR inside = 0");
        expectEqual(int(evalValue("OUTR 0 -1 10", v)), int(TT2EvalError::None), "OUTR ok");
        expectEqual(v, int16_t(1), "OUTR below lo = 1");
        expectEqual(int(evalValue("OUTRI 0 5 10", v)), int(TT2EvalError::None), "OUTRI ok");
        expectEqual(v, int16_t(0), "OUTRI strictly inside = 0");
        expectEqual(int(evalValue("OUTRI 0 0 10", v)), int(TT2EvalError::None), "OUTRI ok");
        expectEqual(v, int16_t(1), "OUTRI on lo bound = 1 (inclusive outside)");
    }
}
