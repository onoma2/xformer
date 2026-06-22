#include "UnitTest.h"

#include "engine/TT2ScriptLoader.h"
#include "engine/TT2Runner.h"

namespace {

struct Fixture {
    TeletypeProgram program;
    TT2Runtime runtime;
    TT2OutputState output;
    int metroAccum = 0;
    float bpm = 120.f;
    Fixture() {
        init(program);
        init(runtime);
        init(output);
    }
    void load(int idx, const char *text) {
        expectTrue(loadScriptText(program, idx, text) >= 0, "metro script loads");
    }
    void advance(int ms) {
        tt2AdvanceMetro(program, runtime, output, ms, metroAccum, bpm);
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

CASE("mc_clock_sync_derives_ms_from_bpm") {
    // M.C 1 16 at 120 BPM -> 1/16 note = 125ms.
    Fixture f;
    f.bpm = 120.f;
    f.load(0, "M.C 1 16\n");
    runScript(f.program, f.runtime, f.output, 0);
    expectEqual(int(f.runtime.variables.metroSyncNum), 1, "num stored");
    expectEqual(int(f.runtime.variables.metroSyncDen), 16, "den stored (clock mode)");
    f.load(TT2_METRO_SCRIPT, "A ADD A 1\n");
    f.runtime.variables.m_act = 1;
    f.advance(120);
    expectEqual(int(f.runtime.variables.m), 125, "m derived to 125ms");
    expectEqual(int(f.runtime.variables.a), 1, "no fire before 125ms");
    f.advance(10); // 130 >= 125
    expectEqual(int(f.runtime.variables.a), 2, "fires at derived 125ms");
}

CASE("mc_retracks_tempo") {
    // Same M.C 1 16 re-derives when BPM changes (140 -> ~107ms).
    Fixture f;
    f.bpm = 140.f;
    f.load(0, "M.C 1 16\n");
    runScript(f.program, f.runtime, f.output, 0);
    f.load(TT2_METRO_SCRIPT, "A ADD A 1\n");
    f.runtime.variables.m_act = 1;
    f.advance(1);
    expectEqual(int(f.runtime.variables.m), 107, "1/16 @140bpm ~= 107ms");
}

CASE("plain_m_reverts_to_free_ms") {
    Fixture f;
    f.load(0, "M.C 3 4\n");
    runScript(f.program, f.runtime, f.output, 0);
    expectEqual(int(f.runtime.variables.metroSyncDen), 4, "clock mode on");
    f.load(1, "M 500\n");
    runScript(f.program, f.runtime, f.output, 1);
    expectEqual(int(f.runtime.variables.metroSyncDen), 0, "plain M clears sync");
    expectEqual(int(f.runtime.variables.m), 500, "m = 500 free ms");
}

} // UNIT_TEST("TeletypeV2Metro")
