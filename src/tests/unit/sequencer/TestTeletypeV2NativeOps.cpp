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
        expectEqual(int(sizeof(TT2TrackEngine)), 40, "TT2TrackEngine size");
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

        // SUB is parsed but not yet implemented in the native table.
        auto r = tryParse("SUB 1 2");
        expectEqual(int(r.error), int(E_OK), "parse SUB 1 2");
        auto result = evaluateCommand(lower(r.cmd), runtime, output);
        expectEqual(int(result.error), int(TT2EvalError::UnsupportedOp),
                    "SUB unsupported");
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
}
