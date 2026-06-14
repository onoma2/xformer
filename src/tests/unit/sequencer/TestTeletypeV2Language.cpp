#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"

#include "model/Types.h"

extern "C" {
#include "command.h"
#include "teletype.h"
#include "ops/op_enum.h"
}

namespace {

TT2EvalResult evalText(const char *text, TT2Runtime &runtime, TT2OutputState &output,
                       const TeletypeProgram *program = nullptr) {
    tele_command_t src = {};
    char errMsg[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, errMsg);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    return evaluateCommand(cmd, runtime, output, program);
}

int16_t getv(const char *text, TT2Runtime &runtime, TT2OutputState &output,
             const TeletypeProgram *program = nullptr) {
    return evalText(text, runtime, output, program).value;
}

} // namespace

UNIT_TEST("TeletypeV2Language") {

    CASE("simple_variables_set_get") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("C 33", runtime, output);
        evalText("D 44", runtime, output);
        evalText("Y 55", runtime, output);
        evalText("Z 66", runtime, output);
        evalText("T 77", runtime, output);
        expectEqual(int(getv("C", runtime, output)), 33, "C");
        expectEqual(int(getv("D", runtime, output)), 44, "D");
        expectEqual(int(getv("Y", runtime, output)), 55, "Y");
        expectEqual(int(getv("Z", runtime, output)), 66, "Z");
        expectEqual(int(getv("T", runtime, output)), 77, "T");
    }

    CASE("jk_indexed_by_script") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("J 7", runtime, output);
        evalText("K 9", runtime, output);
        expectEqual(int(getv("J", runtime, output)), 7, "J round-trips");
        expectEqual(int(getv("K", runtime, output)), 9, "K round-trips");
    }

    CASE("o_auto_increment_wraps") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("O.MIN 0", runtime, output);
        evalText("O.MAX 3", runtime, output);
        evalText("O.WRAP 1", runtime, output);
        evalText("O.INC 1", runtime, output);
        evalText("O 0", runtime, output);
        expectEqual(int(getv("O", runtime, output)), 0, "O reads 0");
        expectEqual(int(getv("O", runtime, output)), 1, "then 1");
        expectEqual(int(getv("O", runtime, output)), 2, "then 2");
        expectEqual(int(getv("O", runtime, output)), 3, "then 3");
        expectEqual(int(getv("O", runtime, output)), 0, "wraps to 0");
    }

    CASE("drunk_walks_within_bounds") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("DRUNK.MIN 0", runtime, output);
        evalText("DRUNK.MAX 5", runtime, output);
        evalText("DRUNK.WRAP 0", runtime, output);
        evalText("DRUNK 2", runtime, output);
        for (int i = 0; i < 20; ++i) {
            int16_t v = getv("DRUNK", runtime, output);
            expectTrue(v >= 0 && v <= 5, "drunk stays in [0,5]");
        }
    }

    CASE("flip_toggles") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        expectEqual(int(getv("FLIP", runtime, output)), 0, "first 0");
        expectEqual(int(getv("FLIP", runtime, output)), 1, "then 1");
        expectEqual(int(getv("FLIP", runtime, output)), 0, "then 0");
    }

    CASE("time_reads_clock") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        runtime.clockMs = 500;
        expectEqual(int(getv("TIME", runtime, output)), 500, "elapsed since reset");
        evalText("TIME 0", runtime, output);  // reset elapsed to 0 at now
        expectEqual(int(getv("TIME", runtime, output)), 0, "reset to 0");
        runtime.clockMs = 750;
        expectEqual(int(getv("TIME", runtime, output)), 250, "250ms later");
    }

    CASE("last_reads_script_elapsed") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        runtime.clockMs = 350;
        runtime.scriptLastMs[0] = 100;
        expectEqual(int(getv("LAST 1", runtime, output)), 250, "script 1 ran 250ms ago");
    }

    CASE("rand_ranges") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        expectEqual(int(getv("RAND 0", runtime, output)), 0, "RAND 0 == 0");
        for (int i = 0; i < 20; ++i) {
            int16_t v = getv("RAND 10", runtime, output);
            expectTrue(v >= 0 && v <= 10, "RAND 10 in [0,10]");
            int16_t a = getv("RND 10", runtime, output);   // alias
            expectTrue(a >= 0 && a <= 10, "RND alias in [0,10]");
            int16_t b = getv("RRND 5 8", runtime, output); // alias
            expectTrue(b >= 5 && b <= 8, "RRND alias in [5,8]");
        }
    }

    CASE("rrand_and_r_range") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        for (int i = 0; i < 20; ++i) {
            int16_t v = getv("RRAND 5 8", runtime, output);
            expectTrue(v >= 5 && v <= 8, "RRAND 5 8 in [5,8]");
        }
        evalText("R.MIN 20", runtime, output);
        evalText("R.MAX 25", runtime, output);
        for (int i = 0; i < 20; ++i) {
            int16_t v = getv("R", runtime, output);
            expectTrue(v >= 20 && v <= 25, "R in [R.MIN,R.MAX]");
        }
    }

    CASE("host_ops_null_safe") {
        // No active TT2Host (no engine) -> reads return 0, writes are no-ops.
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        expectEqual(int(getv("WBPM", runtime, output)), 0, "WBPM no host -> 0");
        expectEqual(int(getv("WR", runtime, output)), 0, "WR no host -> 0");
        expectEqual(int(getv("BUS 1", runtime, output)), 0, "BUS no host -> 0");
        expectEqual(int(getv("WNG 1 0", runtime, output)), 0, "WNG no host -> 0");
        expectEqual(int(getv("RT 1", runtime, output)), 0, "RT no host -> 0");
        evalText("W.ACT 1", runtime, output);  // no-op, must not crash
    }

    CASE("misc_engine_ops") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        // CV.GET / CV.SET (snap, no slew)
        evalText("CV.SET 1 8000", runtime, output);
        expectEqual(runtime.variables.cv[0], int16_t(8000), "CV.SET stores value");
        expectEqual(int(getv("CV.GET 1", runtime, output)), 8000, "CV.GET reads it");
        expectTrue(output.cv[0].remainingMs == 0, "CV.SET snaps (no slew)");
        // M! / M.A / M.ACT.A
        evalText("M.A 500", runtime, output);
        expectEqual(int(getv("M!", runtime, output)), 500, "M! reads metro period");
        evalText("M.ACT.A 0", runtime, output);
        expectEqual(int(runtime.variables.m_act), 0, "M.ACT.A 0 deactivates");
        // SCRIPT.POL
        evalText("SCRIPT.POL 1 2", runtime, output);
        expectEqual(int(getv("SCRIPT.POL 1", runtime, output)), 2, "SCRIPT.POL set/get");
        evalText("SCRIPT.POL 0 1", runtime, output);  // all
        expectEqual(int(getv("SCRIPT.POL 3", runtime, output)), 1, "SCRIPT.POL 0 sets all");
    }

    CASE("tr_width_divisor") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("TR.W 1 50", runtime, output);
        expectEqual(int(getv("TR.W 1", runtime, output)), 50, "TR.W set/get");
        evalText("TR.D 1 2", runtime, output);
        expectEqual(int(getv("TR.D 1", runtime, output)), 2, "TR.D set/get");
        // divisor 2: first TR.P skipped, second fires
        evalText("TR.W 1 0", runtime, output);   // disable width so tr_time used
        evalText("TR.TIME 1 40", runtime, output);
        evalText("TR.P 1", runtime, output);
        expectTrue(output.tr[0].pulseRemainingMs == 0, "divisor skips 1st pulse");
        evalText("TR.P 1", runtime, output);
        expectTrue(output.tr[0].pulseRemainingMs == 40, "divisor fires 2nd pulse");
    }

    CASE("init_family_resets_state") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        TeletypeProgram program; init(program);
        // CV reset
        evalText("CV.OFF 1 500", runtime, output);
        evalText("CV 1 9000", runtime, output);
        evalText("INIT.CV 1", runtime, output);
        expectEqual(runtime.variables.cv[0], int16_t(0), "INIT.CV clears cv");
        expectEqual(runtime.variables.cv_off[0], int16_t(0), "INIT.CV clears offset");
        expectEqual(runtime.variables.cv_slew[0], int16_t(1), "INIT.CV resets slew=1");
        // TR reset
        evalText("TR.TIME 1 50", runtime, output);
        evalText("INIT.TR 1", runtime, output);
        expectEqual(runtime.variables.tr_time[0], int16_t(100), "INIT.TR resets time=100");
        // pattern reset
        evalText("P 5 99", runtime, output, &program);
        evalText("INIT.P 0", runtime, output, &program);
        expectEqual(int(program.patterns[0].val[5]), 0, "INIT.P clears pattern");
        // data reset
        evalText("A 77", runtime, output);
        evalText("INIT.DATA", runtime, output);
        expectEqual(runtime.variables.a, int16_t(1), "INIT.DATA restores A default");
        // script clear
        program.scripts[1].length = 3;
        evalText("INIT.SCRIPT 2", runtime, output, &program);
        expectEqual(int(program.scripts[1].length), 0, "INIT.SCRIPT clears script");
        // time reset
        runtime.clockMs = 1000;
        evalText("INIT.TIME", runtime, output);
        expectEqual(int(getv("TIME", runtime, output)), 0, "INIT.TIME zeroes elapsed");
    }

    CASE("scale_bank_nb_roundtrip") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        // N.B scale_bits root -> stores into slot 0; QT.B quantizes into it.
        evalText("N.B 2773 0", runtime, output);   // major scale mask, root 0
        expectEqual(runtime.variables.n_scale_bits[0], int16_t(2773), "N.B set bits");
        expectEqual(runtime.variables.n_scale_root[0], int16_t(0), "N.B set root");
        int16_t q = getv("QT.B 1638", runtime, output);  // ~1 semitone -> nearest scale note
        expectTrue(q >= 0 && q <= 16383, "QT.B quantizes in range");
        // N.BX writes a chosen slot.
        evalText("N.BX 2 1387 0", runtime, output);
        expectEqual(runtime.variables.n_scale_bits[2], int16_t(1387), "N.BX set slot 2");
    }

    CASE("chaos_get_set") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("CHAOS.ALG 1", runtime, output);
        expectEqual(int(getv("CHAOS.ALG", runtime, output)), 1, "alg set/get");
        evalText("CHAOS.R 5000", runtime, output);
        expectEqual(int(getv("CHAOS.R", runtime, output)), 5000, "r set/get");
        evalText("CHAOS 100", runtime, output);
        getv("CHAOS", runtime, output);  // advances; just confirm no crash
    }

    CASE("seeds_set_get") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("SEED 42", runtime, output);
        expectEqual(int(getv("SEED", runtime, output)), 42, "global seed");
        evalText("RAND.SEED 7", runtime, output);
        expectEqual(int(getv("RAND.SEED", runtime, output)), 7, "rand slot seed");
        // R.SD aliases RAND.SEED slot
        expectEqual(int(getv("R.SD", runtime, output)), 7, "R.SD alias reads rand slot");
    }

    CASE("toss_is_binary") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        for (int i = 0; i < 20; ++i) {
            int16_t v = getv("TOSS", runtime, output);
            expectTrue(v == 0 || v == 1, "TOSS in {0,1}");
        }
    }
}
