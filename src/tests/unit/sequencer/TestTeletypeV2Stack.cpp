#include "UnitTest.h"

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
    void load(int scriptIndex, const char *text) {
        int n = loadScriptText(program, scriptIndex, text);
        expectTrue(n >= 0, "script text loads");
    }
    void run(int scriptIndex) {
        runScript(program, runtime, output, uint8_t(scriptIndex));
    }
};

} // namespace

UNIT_TEST("TeletypeV2Stack") {

    CASE("push_does_not_run_inline") {
        Fixture f;
        f.load(0, "S: CV 1 9999\n");
        f.run(0);
        expectTrue((f.output.cvDirty & 1) == 0, "pushed body not run");
        expectEqual(int(f.runtime.stack.top), 1, "one command on the stack");
    }

    CASE("s_l_reads_depth") {
        Fixture f;
        f.load(0, "S: CV 1 1\nS: CV 2 2\nX S.L\n");
        f.run(0);
        expectEqual(f.runtime.variables.x, int16_t(2), "S.L reads depth 2");
        expectEqual(int(f.runtime.stack.top), 2, "S.L does not pop");
    }

    CASE("s_all_runs_all_then_clears") {
        Fixture f;
        f.load(0, "S: CV 1 100\nS: CV 2 200\nS.ALL\n");
        f.run(0);
        expectEqual(int(f.output.cv[0].targetRaw), 100, "first body ran");
        expectEqual(int(f.output.cv[1].targetRaw), 200, "second body ran");
        expectEqual(int(f.runtime.stack.top), 0, "stack cleared after S.ALL");
    }

    CASE("s_pop_runs_top_lifo") {
        Fixture f;
        f.load(0, "S: A 10\nS: A 20\nS.POP\n");
        f.run(0);
        expectEqual(f.runtime.variables.a, int16_t(20), "S.POP runs most-recent");
        expectEqual(int(f.runtime.stack.top), 1, "one command remains");
    }

    CASE("s_clr_empties") {
        Fixture f;
        f.load(0, "S: A 1\nS: A 2\nS.CLR\n");
        f.run(0);
        expectEqual(int(f.runtime.stack.top), 0, "S.CLR empties the stack");
        // Bodies never ran; A keeps its init default (1).
        expectEqual(f.runtime.variables.a, int16_t(1), "S.CLR does not run bodies");
    }

    CASE("s_all_empty_is_noop") {
        Fixture f;
        f.load(0, "S.ALL\n");
        f.run(0);
        expectEqual(int(f.runtime.stack.top), 0, "empty S.ALL stays empty");
    }
}
