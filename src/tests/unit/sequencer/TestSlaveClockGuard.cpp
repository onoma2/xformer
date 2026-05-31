#include "UnitTest.h"

#include "apps/sequencer/engine/Clock.h"

// The slave-clock period guard rejects jitter spikes: once a baseline period is
// latched, a new measurement is only adopted if within 0.5x..2x of the current
// period. The first measurement (no baseline yet) is always adopted. A >2x or
// <0.5x single-tick outlier is rejected, keeping the prior period.

UNIT_TEST("SlaveClockGuard") {

CASE("zero candidate is never adopted") {
    expectTrue(!Clock::acceptSlavePeriod(false, 0u, 0u), "zero rejected (unlatched)");
    expectTrue(!Clock::acceptSlavePeriod(true, 1000u, 0u), "zero rejected (latched)");
}

CASE("first measurement is always adopted (establishes baseline)") {
    expectTrue(Clock::acceptSlavePeriod(false, 0u, 500u), "unlatched adopts any nonzero");
    expectTrue(Clock::acceptSlavePeriod(false, 99999u, 8u), "unlatched adopts a very slow clock");
}

CASE("within band is adopted") {
    expectTrue(Clock::acceptSlavePeriod(true, 1000u, 1000u), "same period");
    expectTrue(Clock::acceptSlavePeriod(true, 1000u, 1500u), "1.5x within band");
    expectTrue(Clock::acceptSlavePeriod(true, 1000u, 700u), "0.7x within band");
}

CASE("band boundaries inclusive") {
    expectTrue(Clock::acceptSlavePeriod(true, 1000u, 2000u), "exactly 2x adopted");
    expectTrue(Clock::acceptSlavePeriod(true, 1000u, 500u), "exactly 0.5x adopted");
}

CASE("outliers are rejected") {
    expectTrue(!Clock::acceptSlavePeriod(true, 1000u, 2001u), ">2x spike rejected");
    expectTrue(!Clock::acceptSlavePeriod(true, 1000u, 3000u), "3x spike rejected");
    expectTrue(!Clock::acceptSlavePeriod(true, 1000u, 499u), "<0.5x dropout rejected");
}

} // UNIT_TEST
