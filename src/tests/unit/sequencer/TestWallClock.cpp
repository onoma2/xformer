#include "UnitTest.h"

#include "apps/sequencer/engine/WallClock.h"

// WallTimer is a 32-bit µs absolute-deadline timer. The two properties that
// matter: wrap-safe comparison (signed difference, not now > end) and
// drift-free restart (endUs += delay, accumulating from the prior deadline).
// Tests drive an explicit `now` so they don't depend on the hardware clock.

UNIT_TEST("WallClock") {

CASE("not elapsed before deadline") {
    WallTimer t;
    t.start(1000u, 100u);            // now=1000, fires at 1100
    expectTrue(!t.elapsed(1000u), "not elapsed at start");
    expectTrue(!t.elapsed(1099u), "not elapsed just before deadline");
}

CASE("elapsed at and after deadline, fires once") {
    WallTimer t;
    t.start(1000u, 100u);
    expectTrue(t.elapsed(1100u), "elapsed exactly at deadline");
    expectTrue(!t.elapsed(1100u), "does not fire again until re-armed");
    expectTrue(!t.elapsed(5000u), "stays disarmed after firing");
}

CASE("schedule sets absolute deadline") {
    WallTimer t;
    t.schedule(2000u);
    expectTrue(!t.elapsed(1999u), "not elapsed before absolute deadline");
    expectTrue(t.elapsed(2000u), "elapsed at absolute deadline");
}

CASE("wrap-safe across the 32-bit boundary") {
    WallTimer t;
    uint32_t near = 0xFFFFFF00u;      // close to wrap
    t.start(near, 0x200u);            // deadline = 0x100 (wrapped past 0)
    expectTrue(!t.elapsed(near), "not elapsed before wrapped deadline");
    expectTrue(!t.elapsed(0xFFu), "not elapsed just before wrapped deadline");
    expectTrue(t.elapsed(0x100u), "elapsed at wrapped deadline (signed diff, not now>end)");
}

CASE("restart is drift-free") {
    WallTimer t;
    t.start(1000u, 100u);             // deadline 1100
    // Serviced late, at 1150. Drift-free restart accumulates from the prior
    // deadline (1100 + 100 = 1200), not from the service moment (1150 + 100).
    expectTrue(t.elapsed(1150u), "fired (late)");
    t.restart(100u);
    expectTrue(!t.elapsed(1199u), "next deadline is 1200, not 1250");
    expectTrue(t.elapsed(1200u), "fires at accumulated deadline 1200");
}

} // UNIT_TEST
