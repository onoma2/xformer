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
}
