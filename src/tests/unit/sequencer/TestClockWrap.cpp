#include "UnitTest.h"

#include "apps/sequencer/engine/Clock.h"

// Two Clock defects, extracted into pure statics:
//  - slaveSubTickDue: the microsecond counters are uint32 and wrap (~71 min);
//    the old `>=` compare misses the deadline across the wrap.
//  - clockPulseTicks: the pulse width must derive from the EFFECTIVE tempo
//    (bpm(), the slave tempo while slaved); the old code used the master BPM.
UNIT_TEST("ClockWrap") {

CASE("slave sub-tick due is wrap-safe") {
    expectTrue(Clock::slaveSubTickDue(1000, 1000), "elapsed == deadline is due");
    expectTrue(Clock::slaveSubTickDue(1500, 1000), "elapsed past deadline is due");
    expectFalse(Clock::slaveSubTickDue(900, 1000), "elapsed before deadline not due");
    expectTrue(Clock::slaveSubTickDue(0x00000005, 0xFFFFFFF0), "wrapped elapsed is past a near-max deadline");
    expectFalse(Clock::slaveSubTickDue(0xFFFFFFF0, 0xFFFFFFF5), "deadline 5us ahead of near-max elapsed, not due");
}

CASE("clock pulse width derives from the effective tempo") {
    uint32_t atMaster = Clock::clockPulseTicks(120.f, 192, 1000, 1);
    uint32_t atSlave  = Clock::clockPulseTicks(60.f, 192, 1000, 1);
    expect(atMaster != atSlave, "different tempi give different pulse widths");
    expectEqual(atMaster, uint32_t(120.f * 192 * 1000 / (60 * 1000)), "master formula");
    expectEqual(atSlave, uint32_t(60.f * 192 * 1000 / (60 * 1000)), "slave formula (half the master width)");
    expectEqual(Clock::clockPulseTicks(120.f, 192, 1000, 100000), 100000u, "minTicks floor applies");
}

}
