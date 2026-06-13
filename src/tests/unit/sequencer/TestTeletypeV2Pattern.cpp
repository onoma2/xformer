#include "UnitTest.h"

#include <initializer_list>

#include "engine/TT2ScriptLoader.h"
#include "engine/TT2Runner.h"

namespace {

struct Fixture {
    TeletypeProgram program;
    TT2Runtime runtime;
    TT2OutputState output;
    Fixture() {
        init(program);
        init(runtime);
        init(output);
    }
    // Run one command line, return its result (top-of-stack in .value).
    TT2EvalResult run(const char *text) {
        tele_command_t cmd = {};
        char msg[TELE_ERROR_MSG_LENGTH];
        if (parse(text, &cmd, msg) != E_OK) {
            return {TT2EvalError::UnsupportedOp, 0, 0, 0};
        }
        TT2Command lowered = {};
        lowerCommand(cmd, lowered);
        return evaluateCommand(lowered, runtime, output, &program);
    }
    TT2Pattern &pat(int i) { return program.patterns[i]; }
    void seed(int p, std::initializer_list<int16_t> vals, int len) {
        int i = 0;
        for (int16_t v : vals) program.patterns[p].val[i++] = v;
        program.patterns[p].len = uint16_t(len);
    }
};

} // namespace

UNIT_TEST("TeletypeV2Pattern") {

    CASE("p_n_get_set_clamp") {
        Fixture f;
        expectEqual(f.run("P.N").value, int16_t(0), "P.N default 0");
        f.run("P.N 2");
        expectEqual(f.runtime.variables.p_n, int16_t(2), "P.N set to 2");
        expectEqual(f.run("P.N").value, int16_t(2), "P.N reads 2");
        f.run("P.N 9");
        expectEqual(f.runtime.variables.p_n, int16_t(3), "P.N clamps high to 3");
        f.run("P.N -1");
        expectEqual(f.runtime.variables.p_n, int16_t(0), "P.N clamps low to 0");
    }

    CASE("p_value_get_set") {
        Fixture f;
        f.seed(0, {10, 20, 30}, 3);
        expectEqual(f.run("P 0").value, int16_t(10), "P 0 reads val[0]");
        expectEqual(f.run("P 2").value, int16_t(30), "P 2 reads val[2]");
        f.run("P 1 99");  // set idx 1 to 99
        expectEqual(f.pat(0).val[1], int16_t(99), "P set writes val[1]");
        expectEqual(f.run("P -1").value, int16_t(30), "P -1 reads from back (val[2])");
    }

    CASE("pn_value_get_set") {
        Fixture f;
        f.seed(1, {5, 6, 7}, 3);
        expectEqual(f.run("PN 1 2").value, int16_t(7), "PN 1 2 reads pattern1 val[2]");
        f.run("PN 1 0 42");  // pattern1 idx0 = 42
        expectEqual(f.pat(1).val[0], int16_t(42), "PN set writes pattern1 val[0]");
        expectEqual(f.runtime.variables.p_n, int16_t(0), "PN leaves working pattern untouched");
    }

    CASE("p_length") {
        Fixture f;
        f.seed(0, {1, 2, 3, 4}, 4);
        expectEqual(f.run("P.L").value, int16_t(4), "P.L reads 4");
        f.run("P.L 2");
        expectEqual(int(f.pat(0).len), 2, "P.L set to 2");
        f.run("P.L 99");
        expectEqual(int(f.pat(0).len), 64, "P.L clamps high to 64");
        f.run("P.L -1");
        expectEqual(int(f.pat(0).len), 0, "P.L clamps low to 0");
        expectEqual(f.run("PN.L 1").value, int16_t(0), "PN.L reads pattern1 len");
    }

    CASE("p_index") {
        Fixture f;
        f.seed(0, {1, 2, 3, 4}, 4);
        f.run("P.I 2");
        expectEqual(f.pat(0).idx, int16_t(2), "P.I set to 2");
        expectEqual(f.run("P.I").value, int16_t(2), "P.I reads 2");
        f.run("P.I 99");  // clamps to len-1 = 3
        expectEqual(f.pat(0).idx, int16_t(3), "P.I clamps to len-1");
    }

    CASE("p_here") {
        Fixture f;
        f.seed(0, {11, 22, 33}, 3);
        f.run("P.I 1");
        expectEqual(f.run("P.HERE").value, int16_t(22), "P.HERE reads val at idx 1");
        f.run("P.HERE 88");
        expectEqual(f.pat(0).val[1], int16_t(88), "P.HERE set writes at idx 1");
        expectEqual(f.pat(0).idx, int16_t(1), "P.HERE does not advance idx");
    }

    CASE("bounds_wrap_start_end") {
        Fixture f;
        f.run("P.WRAP 1");
        expectEqual(int(f.pat(0).wrap), 1, "P.WRAP set 1");
        f.run("P.WRAP 0");
        expectEqual(int(f.pat(0).wrap), 0, "P.WRAP set 0");
        f.seed(0, {1, 2, 3, 4}, 4);
        f.run("P.START 1");
        expectEqual(f.pat(0).start, int16_t(1), "P.START set 1");
        f.run("P.END 3");
        expectEqual(f.pat(0).end, int16_t(3), "P.END set 3");
        expectEqual(f.run("P.START").value, int16_t(1), "P.START reads 1");
        expectEqual(f.run("P.END").value, int16_t(3), "P.END reads 3");
    }

    CASE("next_wraps_within_bounds") {
        Fixture f;
        f.seed(0, {10, 11, 12, 13}, 4);
        f.run("P.START 1");
        f.run("P.END 3");
        f.run("P.WRAP 1");
        f.run("P.I 3");           // at end
        expectEqual(f.run("P.NEXT").value, int16_t(11), "NEXT from end wraps to start (idx1=11)");
        expectEqual(f.pat(0).idx, int16_t(1), "idx wrapped to start");
        expectEqual(f.run("P.NEXT").value, int16_t(12), "NEXT advances to idx2");
    }

    CASE("next_no_wrap_holds_at_end") {
        Fixture f;
        f.seed(0, {10, 11, 12, 13}, 4);
        f.run("P.END 3");
        f.run("P.WRAP 0");
        f.run("P.I 3");
        f.run("P.NEXT");
        expectEqual(f.pat(0).idx, int16_t(3), "no-wrap NEXT holds at end");
    }

    CASE("prev_wraps") {
        Fixture f;
        f.seed(0, {10, 11, 12, 13}, 4);
        f.run("P.START 0");
        f.run("P.END 3");
        f.run("P.WRAP 1");
        f.run("P.I 0");
        expectEqual(f.run("P.PREV").value, int16_t(13), "PREV from start wraps to end (idx3=13)");
        expectEqual(f.pat(0).idx, int16_t(3), "idx wrapped to end");
    }

    CASE("insert_remove") {
        Fixture f;
        f.seed(0, {1, 2, 3}, 3);
        f.run("P.INS 1 9");   // insert 9 at idx 1
        expectEqual(f.pat(0).val[1], int16_t(9), "INS places 9 at idx1");
        expectEqual(f.pat(0).val[2], int16_t(2), "INS shifts old idx1 to idx2");
        expectEqual(int(f.pat(0).len), 4, "INS bumps len to 4");
        expectEqual(f.run("P.RM 1").value, int16_t(9), "RM returns removed value");
        expectEqual(f.pat(0).val[1], int16_t(2), "RM shifts left");
        expectEqual(int(f.pat(0).len), 3, "RM drops len to 3");
    }

    CASE("push_pop") {
        Fixture f;
        f.seed(0, {1, 2, 3}, 3);
        f.run("P.PUSH 7");
        expectEqual(f.pat(0).val[3], int16_t(7), "PUSH appends at len");
        expectEqual(int(f.pat(0).len), 4, "PUSH bumps len");
        expectEqual(f.run("P.POP").value, int16_t(7), "POP returns last");
        expectEqual(int(f.pat(0).len), 3, "POP drops len");
        // POP on empty is a safe 0
        f.run("P.L 0");
        expectEqual(f.run("P.POP").value, int16_t(0), "POP empty returns 0");
    }

    CASE("pn_insert") {
        Fixture f;
        f.seed(1, {5, 6}, 2);
        f.run("PN.INS 1 0 99");  // pattern1, idx0, val99
        expectEqual(f.pat(1).val[0], int16_t(99), "PN.INS into pattern1");
        expectEqual(int(f.pat(1).len), 3, "PN.INS bumps pattern1 len");
    }

    CASE("reverse") {
        Fixture f;
        f.seed(0, {1, 2, 3, 4}, 4);
        f.run("P.END 3");
        f.run("P.REV");
        expectEqual(f.pat(0).val[0], int16_t(4), "REV [0]=4");
        expectEqual(f.pat(0).val[3], int16_t(1), "REV [3]=1");
    }

    CASE("rotate") {
        Fixture f;
        f.seed(0, {1, 2, 3, 4}, 4);
        f.run("P.END 3");
        f.run("P.ROT 1");   // rotate right by 1 -> {4,1,2,3}
        expectEqual(f.pat(0).val[0], int16_t(4), "ROT [0]=4");
        expectEqual(f.pat(0).val[1], int16_t(1), "ROT [1]=1");
    }

    CASE("shuffle_is_permutation") {
        Fixture f;
        f.seed(0, {1, 2, 3, 4}, 4);
        f.run("P.END 3");
        f.run("P.SHUF");
        bool seen[5] = { false, false, false, false, false };
        int16_t sum = 0;
        for (int i = 0; i < 4; ++i) {
            int16_t v = f.pat(0).val[i];
            if (v >= 1 && v <= 4) seen[v] = true;
            sum += v;
        }
        expectEqual(int(sum), 10, "SHUF preserves sum (permutation)");
        expectTrue(seen[1] && seen[2] && seen[3] && seen[4], "SHUF keeps all elements");
    }
}
