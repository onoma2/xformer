#include "UnitTest.h"

#include "apps/sequencer/engine/Clock.h"

// The slave-clock period guard rejects single-tick jitter spikes: once a
// baseline is latched, a measurement is adopted if within 0.5x..2x of the
// current period, OR if it agrees with the previous raw measurement (a
// sustained new tempo confirmed over two consecutive ticks). The first
// measurement (no baseline) is always adopted. Signature:
// acceptSlavePeriod(latched, current, lastRaw, candidate).

UNIT_TEST("SlaveClockGuard") {

CASE("zero candidate is never adopted") {
    expectTrue(!Clock::acceptSlavePeriod(false, 0u, 0u, 0u), "zero rejected (unlatched)");
    expectTrue(!Clock::acceptSlavePeriod(true, 1000u, 1000u, 0u), "zero rejected (latched)");
}

CASE("first measurement is always adopted (establishes baseline)") {
    expectTrue(Clock::acceptSlavePeriod(false, 0u, 0u, 500u), "unlatched adopts any nonzero");
    expectTrue(Clock::acceptSlavePeriod(false, 99999u, 0u, 8u), "unlatched adopts a very slow clock");
}

CASE("within band is adopted") {
    expectTrue(Clock::acceptSlavePeriod(true, 1000u, 1000u, 1000u), "same period");
    expectTrue(Clock::acceptSlavePeriod(true, 1000u, 1000u, 1500u), "1.5x within band");
    expectTrue(Clock::acceptSlavePeriod(true, 1000u, 1000u, 700u), "0.7x within band");
}

CASE("band boundaries inclusive") {
    expectTrue(Clock::acceptSlavePeriod(true, 1000u, 1000u, 2000u), "exactly 2x adopted");
    expectTrue(Clock::acceptSlavePeriod(true, 1000u, 1000u, 500u), "exactly 0.5x adopted");
}

CASE("isolated spike is rejected") {
    // prior raw was in-band (1000), so a lone 3x candidate matches neither the
    // current period nor the previous raw -> rejected.
    expectTrue(!Clock::acceptSlavePeriod(true, 1000u, 1000u, 3000u), "3x spike rejected");
    expectTrue(!Clock::acceptSlavePeriod(true, 1000u, 1000u, 499u), "<0.5x dropout rejected");
}

CASE("sustained out-of-band tempo change is adopted on confirmation") {
    // tick 1: jump to 3x, prev raw still the old baseline -> rejected.
    expectTrue(!Clock::acceptSlavePeriod(true, 1000u, 1000u, 3000u), "first jump tick rejected");
    // tick 2: period stays ~3x; now prev raw (3000) agrees -> adopted.
    expectTrue(Clock::acceptSlavePeriod(true, 1000u, 3000u, 3050u), "confirmed jump adopted");
}

} // UNIT_TEST
