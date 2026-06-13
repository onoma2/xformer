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
    void load(int idx, const char *text) {
        expectTrue(loadScriptText(program, idx, text) >= 0, "script loads");
    }
    void run(int idx) { runScript(program, runtime, output, uint8_t(idx)); }
};

} // namespace

UNIT_TEST("TeletypeV2BreakKill") {

CASE("break_stops_l_loop_after_first_iteration") {
    Fixture f;
    // Without BREAK, A would reach 1 + 5 = 6. With BREAK after the first body
    // it stops at 2.
    f.load(0, "L 1 5: A ADD A 1; BREAK\n");
    f.run(0);
    expectEqual(int(f.runtime.variables.a), 2, "loop broke after one iteration");
}

CASE("break_flag_resets_between_loops") {
    Fixture f;
    // Line 0 breaks after one iter (A: 1->2); line 1 must run fully (B starts
    // 2, +1 three times -> 5) proving the BREAK flag was reset.
    f.load(0, "L 1 3: A ADD A 1; BREAK\nL 1 3: B ADD B 1\n");
    f.run(0);
    expectEqual(int(f.runtime.variables.a), 2, "first loop broke");
    expectEqual(int(f.runtime.variables.b), 5, "second loop ran fully (flag reset)");
}

CASE("brk_alias_breaks") {
    Fixture f;
    f.load(0, "L 1 5: A ADD A 1; BRK\n");
    f.run(0);
    expectEqual(int(f.runtime.variables.a), 2, "BRK alias breaks the loop");
}

CASE("kill_clears_delays_and_disables_metro") {
    Fixture f;
    f.runtime.variables.m_act = 1;
    // DEL consumes the rest of its line as the delayed body, so KILL must be a
    // separate line. Line 0 queues a delay; line 1 KILLs.
    f.load(0, "DEL 100: A 10\nKILL\n");
    f.run(0);
    expectEqual(int(f.runtime.delay.count), 0, "KILL cleared the delay queue");
    expectEqual(int(f.runtime.variables.m_act), 0, "KILL disabled the metro");
}

} // UNIT_TEST("TeletypeV2BreakKill")
