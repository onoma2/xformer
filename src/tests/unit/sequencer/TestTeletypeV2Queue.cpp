#include "UnitTest.h"

#include "engine/TT2Evaluator.h"
#include "engine/TT2TrackEngine.h"

#include "model/Types.h"

extern "C" {
#include "command.h"
#include "tt_parser.h"
#include "ops/op_enum.h"
}

namespace {

TT2EvalResult run(TT2Runtime &rt, TT2OutputState &out, const char *text) {
    tele_command_t cmd = {};
    char msg[TELE_ERROR_MSG_LENGTH];
    if (parse(text, &cmd, msg) != E_OK) {
        return {TT2EvalError::UnsupportedOp, 0, 0, 0};
    }
    TT2Command lowered = {};
    lowerCommand(cmd, lowered);
    return evaluateCommand(lowered, rt, out);
}

} // namespace

UNIT_TEST("TeletypeV2Queue") {

    auto seed = [](TT2Runtime &rt, const int16_t *vals, int n) {
        rt.variables.q_n = int16_t(n);
        for (int i = 0; i < n; ++i) rt.variables.q[i] = vals[i];
    };

    CASE("push_and_pop") {
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        // q_n=1, q_grow=0 after init.
        run(rt, out, "Q 5");
        run(rt, out, "Q 7");
        expectEqual(rt.variables.q[0], int16_t(7), "q[0] = last pushed");
        expectEqual(rt.variables.q[1], int16_t(5), "q[1] = prior");
        auto r = run(rt, out, "Q");
        expectEqual(r.value, int16_t(7), "Q reads top (q_n=1)");
    }

    CASE("q_n_set_get_clamp") {
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        run(rt, out, "Q.N 4");
        expectEqual(run(rt, out, "Q.N").value, int16_t(4), "Q.N reads 4");
        run(rt, out, "Q.N 0");
        expectEqual(rt.variables.q_n, int16_t(1), "Q.N clamps low to 1");
        run(rt, out, "Q.N 99");
        expectEqual(rt.variables.q_n, int16_t(64), "Q.N clamps high to 64");
    }

    CASE("grow_flag_and_push") {
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        run(rt, out, "Q.GRW 1");
        expectEqual(run(rt, out, "Q.GRW").value, int16_t(1), "Q.GRW reads 1");
        run(rt, out, "Q 9");
        expectEqual(rt.variables.q_n, int16_t(2), "grow push grows q_n");
        expectEqual(rt.variables.q[0], int16_t(9), "pushed value at head");
    }

    CASE("avg_get_set") {
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        const int16_t s[] = {2, 4, 6};
        seed(rt, s, 3);
        expectEqual(run(rt, out, "Q.AVG").value, int16_t(4), "avg {2,4,6} = 4");
        const int16_t s2[] = {1, 2};
        seed(rt, s2, 2);
        expectEqual(run(rt, out, "Q.AVG").value, int16_t(2), "avg {1,2} = 2 (rounds up)");
        run(rt, out, "Q.AVG 9");
        expectEqual(rt.variables.q[0], int16_t(9), "Q.AVG set fills all [0]");
        expectEqual(rt.variables.q[5], int16_t(9), "Q.AVG set fills all [5]");
    }

    CASE("clr_get_set") {
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        const int16_t s[] = {1, 2, 3};
        seed(rt, s, 3);
        run(rt, out, "Q.CLR");
        expectEqual(rt.variables.q_n, int16_t(1), "Q.CLR resets q_n to 1");
        expectEqual(rt.variables.q[0], int16_t(0), "Q.CLR zeroes [0]");
        expectEqual(rt.variables.q[2], int16_t(0), "Q.CLR zeroes [2]");
        run(rt, out, "Q.CLR 5");
        expectEqual(rt.variables.q[0], int16_t(5), "Q.CLR set seeds [0]");
        expectEqual(rt.variables.q_n, int16_t(1), "Q.CLR set q_n=1");
    }

    CASE("sum") {
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        const int16_t s[] = {2, 4, 6};
        seed(rt, s, 3);
        expectEqual(run(rt, out, "Q.SUM").value, int16_t(12), "sum = 12");
    }

    CASE("min_max_get_set") {
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        const int16_t s[] = {5, 2, 8};
        seed(rt, s, 3);
        expectEqual(run(rt, out, "Q.MIN").value, int16_t(2), "min = 2");
        expectEqual(run(rt, out, "Q.MAX").value, int16_t(8), "max = 8");
        run(rt, out, "Q.MIN 4");  // floor each below 4 up to 4
        expectEqual(rt.variables.q[1], int16_t(4), "Q.MIN set floors 2->4");
        expectEqual(rt.variables.q[0], int16_t(5), "Q.MIN set leaves 5");
        const int16_t s2[] = {5, 2, 8};
        seed(rt, s2, 3);
        run(rt, out, "Q.MAX 6");  // cap each above 6 down to 6
        expectEqual(rt.variables.q[2], int16_t(6), "Q.MAX set caps 8->6");
        expectEqual(rt.variables.q[0], int16_t(5), "Q.MAX set leaves 5");
    }

    CASE("sort") {
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        const int16_t s[] = {5, 2, 8, 1};
        seed(rt, s, 4);
        run(rt, out, "Q.SRT");
        expectEqual(rt.variables.q[0], int16_t(1), "sorted [0]=1");
        expectEqual(rt.variables.q[1], int16_t(2), "sorted [1]=2");
        expectEqual(rt.variables.q[2], int16_t(5), "sorted [2]=5");
        expectEqual(rt.variables.q[3], int16_t(8), "sorted [3]=8");
    }

    CASE("reverse") {
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        const int16_t s[] = {1, 2, 3, 4};
        seed(rt, s, 4);
        run(rt, out, "Q.REV");
        expectEqual(rt.variables.q[0], int16_t(4), "rev [0]=4");
        expectEqual(rt.variables.q[3], int16_t(1), "rev [3]=1");
    }

    CASE("shift") {
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        const int16_t s[] = {1, 2, 3, 4};
        seed(rt, s, 4);
        run(rt, out, "Q.SH");  // rotate right by 1
        expectEqual(rt.variables.q[0], int16_t(4), "shift1 [0]=4");
        expectEqual(rt.variables.q[1], int16_t(1), "shift1 [1]=1");
        const int16_t s2[] = {1, 2, 3, 4};
        seed(rt, s2, 4);
        run(rt, out, "Q.SH 2");  // rotate by 2
        expectEqual(rt.variables.q[0], int16_t(3), "shift2 [0]=3");
        expectEqual(rt.variables.q[2], int16_t(1), "shift2 [2]=1");
    }

    CASE("add_all_and_indexed") {
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        const int16_t s[] = {1, 2, 3};
        seed(rt, s, 3);
        run(rt, out, "Q.ADD 10");  // get form: all + 10
        expectEqual(rt.variables.q[0], int16_t(11), "add-all [0]=11");
        expectEqual(rt.variables.q[2], int16_t(13), "add-all [2]=13");
        const int16_t s2[] = {1, 2, 3};
        seed(rt, s2, 3);
        run(rt, out, "Q.ADD 10 1");  // set form: amount 10 to index 1
        expectEqual(rt.variables.q[1], int16_t(12), "add-idx [1]=12");
        expectEqual(rt.variables.q[0], int16_t(1), "add-idx leaves [0]");
    }

    CASE("mul_div_mod") {
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        const int16_t s[] = {4, 8, 12};
        seed(rt, s, 3);
        run(rt, out, "Q.DIV 2");
        expectEqual(rt.variables.q[0], int16_t(2), "div2 [0]=2");
        expectEqual(rt.variables.q[2], int16_t(6), "div2 [2]=6");
        run(rt, out, "Q.DIV 0");  // guard: no-op
        expectEqual(rt.variables.q[0], int16_t(2), "div0 no-op");
        const int16_t s2[] = {4, 8, 12};
        seed(rt, s2, 3);
        run(rt, out, "Q.MOD 3");
        expectEqual(rt.variables.q[0], int16_t(1), "mod3 [0]=1");
        expectEqual(rt.variables.q[2], int16_t(0), "mod3 [2]=0");
        const int16_t s3[] = {2, 4, 6};
        seed(rt, s3, 3);
        run(rt, out, "Q.MUL 2");
        expectEqual(rt.variables.q[1], int16_t(8), "mul2 [1]=8");
    }

    CASE("indexed_get_set") {
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        const int16_t s[] = {10, 20, 30};
        seed(rt, s, 3);
        expectEqual(run(rt, out, "Q.I 1").value, int16_t(20), "Q.I 1 reads 20");
        run(rt, out, "Q.I 1 99");  // set index 1 to 99
        expectEqual(rt.variables.q[1], int16_t(99), "Q.I set writes 99");
    }
}
