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
    void advance(int ms) {
        tt2AdvanceDelays(program, runtime, output, ms);
    }
};

} // namespace

UNIT_TEST("TeletypeV2Delay") {

CASE("del_fires_after_ms_delay") {
    Fixture f;
    f.load(0, "DEL 100: CV 1 5000\n");
    f.run(0);
    expectTrue((f.output.cvDirty & 1) == 0, "body not run on enqueue");
    expectEqual(int(f.runtime.delay.count), 1, "one entry queued");
    f.advance(50);
    expectTrue((f.output.cvDirty & 1) == 0, "still pending at 50ms");
    f.advance(60); // total 110 >= 100
    expectEqual(int(f.output.cv[0].targetRaw), 5000, "body fires past deadline");
    expectEqual(int(f.runtime.delay.count), 0, "slot freed after fire");
}

CASE("del_body_runs_math") {
    Fixture f;
    f.load(0, "DEL 10: CV 1 ADD 100 5\n");
    f.run(0);
    f.advance(10);
    expectEqual(int(f.output.cv[0].targetRaw), 105, "delayed ADD evaluates");
}

CASE("del_zero_clamps_to_one_ms") {
    Fixture f;
    f.load(0, "DEL 0: CV 1 7\n");
    f.run(0);
    f.advance(1);
    expectEqual(int(f.output.cv[0].targetRaw), 7, "DEL 0 fires at 1ms");
}

CASE("del_clr_cancels_pending") {
    Fixture f;
    f.load(0, "DEL 100: CV 1 5000\n");
    f.run(0);
    expectEqual(int(f.runtime.delay.count), 1, "queued");
    f.load(1, "DEL.CLR\n");
    f.run(1);
    expectEqual(int(f.runtime.delay.count), 0, "DEL.CLR clears queue");
    f.advance(200);
    expectTrue((f.output.cvDirty & 1) == 0, "nothing fires after clear");
}

CASE("queue_full_caps_at_depth") {
    Fixture f;
    TT2RuntimeCommand body = {};
    body.length = 0;
    for (int i = 0; i < TT2_DELAY_DEPTH; ++i) {
        expectTrue(tt2DelayAdd(f.runtime, body, 1000, 0, 0, 0, 0), "slot accepts");
    }
    expectTrue(!tt2DelayAdd(f.runtime, body, 1000, 0, 0, 0, 0), "full queue rejects");
    expectEqual(int(f.runtime.delay.count), TT2_DELAY_DEPTH, "count at depth");
}

CASE("sub_threshold_advance_does_not_fire") {
    Fixture f;
    f.load(0, "DEL 50: CV 1 9\n");
    f.run(0);
    f.advance(10);
    f.advance(10);
    f.advance(10);
    expectTrue((f.output.cvDirty & 1) == 0, "30ms < 50ms, no fire");
    f.advance(25); // 55 >= 50
    expectEqual(int(f.output.cv[0].targetRaw), 9, "fires once cumulative >= 50");
}

} // UNIT_TEST("TeletypeV2Delay")
