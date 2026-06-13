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

CASE("del_x_schedules_n_copies_at_intervals") {
    Fixture f;
    f.load(0, "DEL.X 3 10: CV 1 9\n"); // x=3 copies, delay_time=10ms -> 10,20,30 ms
    f.run(0);
    expectEqual(int(f.runtime.delay.count), 3, "three copies queued");
    f.advance(10);
    expectEqual(int(f.runtime.delay.count), 2, "first copy fired at 10ms");
    expectEqual(int(f.output.cv[0].targetRaw), 9, "body ran");
    f.advance(10);
    expectEqual(int(f.runtime.delay.count), 1, "second fired at 20ms");
    f.advance(10);
    expectEqual(int(f.runtime.delay.count), 0, "third fired at 30ms");
}

CASE("del_r_first_fires_immediately") {
    Fixture f;
    f.load(0, "DEL.R 2 100: CV 1 5\n"); // n=2, x=100 -> deadlines 1, 101
    f.run(0);
    expectEqual(int(f.runtime.delay.count), 2, "two copies queued");
    f.advance(1);
    expectEqual(int(f.runtime.delay.count), 1, "first fires at 1ms (immediate)");
    expectEqual(int(f.output.cv[0].targetRaw), 5, "body ran");
    f.advance(100);
    expectEqual(int(f.runtime.delay.count), 0, "second fires at 101ms");
}

CASE("del_b_bitmask_positions") {
    Fixture f;
    f.load(0, "DEL.B 10 6: CV 1 1\n"); // mask 6 = bits 1,2 -> 10ms, 20ms
    f.run(0);
    expectEqual(int(f.runtime.delay.count), 2, "two set bits queued");
    f.advance(10);
    expectEqual(int(f.runtime.delay.count), 1, "bit 1 fires at 10ms");
    f.advance(10);
    expectEqual(int(f.runtime.delay.count), 0, "bit 2 fires at 20ms");
}

} // UNIT_TEST("TeletypeV2Delay")
