#include "UnitTest.h"

#include "engine/TT2ScriptLoader.h"
#include "engine/TT2Runner.h"

namespace {

struct Fixture {
    TeletypeProgram program;
    TT2Runtime runtime;
    TT2OutputState output;
    int metroAccum = 0;
    Fixture() {
        init(program);
        init(runtime);
        init(output);
    }
    void load(int idx, const char *text) {
        expectTrue(loadScriptText(program, idx, text) >= 0, "metro script loads");
    }
    void advance(int ms) {
        tt2AdvanceMetro(program, runtime, output, ms, metroAccum);
    }
};

} // namespace

UNIT_TEST("TeletypeV2Metro") {

CASE("fires_at_m_interval") {
    Fixture f;
    f.load(TT2_METRO_SCRIPT, "A ADD A 1\n"); // each fire: A += 1 (A starts at 1)
    f.runtime.variables.m = 100;
    f.runtime.variables.m_act = 1;
    f.advance(50);
    expectEqual(int(f.runtime.variables.a), 1, "no fire before interval");
    f.advance(60); // cumulative 110 >= 100
    expectEqual(int(f.runtime.variables.a), 2, "fires once past 100ms");
    f.advance(100); // 10 leftover + 100 = 110
    expectEqual(int(f.runtime.variables.a), 3, "fires again");
}

CASE("inactive_does_not_fire") {
    Fixture f;
    f.load(TT2_METRO_SCRIPT, "A ADD A 1\n");
    f.runtime.variables.m = 50;
    f.runtime.variables.m_act = 0;
    f.advance(500);
    expectEqual(int(f.runtime.variables.a), 1, "no fire when M.ACT off");
}

CASE("catches_up_multiple_intervals") {
    Fixture f;
    f.load(TT2_METRO_SCRIPT, "A ADD A 1\n");
    f.runtime.variables.m = 10;
    f.runtime.variables.m_act = 1;
    f.advance(35); // 10,20,30 -> 3 fires, 5 leftover
    expectEqual(int(f.runtime.variables.a), 4, "three fires in one advance");
}

CASE("empty_metro_script_is_noop") {
    Fixture f;
    f.runtime.variables.m = 10;
    f.runtime.variables.m_act = 1;
    f.advance(100);
    expectEqual(f.metroAccum, 0, "empty metro script does not accumulate");
}

CASE("interval_clamps_to_2ms") {
    Fixture f;
    f.load(TT2_METRO_SCRIPT, "A ADD A 1\n");
    f.runtime.variables.m = 1; // below 2 -> clamped
    f.runtime.variables.m_act = 1;
    f.advance(2);
    expectEqual(int(f.runtime.variables.a), 2, "fires at clamped 2ms");
}

} // UNIT_TEST("TeletypeV2Metro")
