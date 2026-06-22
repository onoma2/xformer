#include "UnitTest.h"

#include "engine/TT2TrackEngine.h"

// The native engine's instance methods (tick/cvOutput/gateOutput) require a
// full Engine/Model/Track and hardware-driver mocks to construct, so they are
// exercised at the simulator/integration level, not here. This test covers the
// one unit-testable engine surface: the static raw->volts CV conversion
// contract. The script -> TT2OutputState path is covered by
// TestTeletypeV2RealScript and TestTeletypeV2Delay.

namespace {
bool near(float a, float b) { return a > b - 0.01f && a < b + 0.01f; }
}

UNIT_TEST("TeletypeV2TrackEngineSmoke") {

CASE("raw_to_volts_bounds") {
    expectTrue(near(TT2TrackEngine::rawToVolts(0), -5.f), "raw 0 -> -5V");
    expectTrue(near(TT2TrackEngine::rawToVolts(16383), 5.f), "raw max -> +5V");
}

CASE("raw_to_volts_midpoint") {
    expectTrue(near(TT2TrackEngine::rawToVolts(8191), 0.f), "mid raw ~ 0V");
}

CASE("raw_to_volts_clamps_out_of_range") {
    expectTrue(near(TT2TrackEngine::rawToVolts(-100), -5.f), "negative clamps to -5V");
    expectTrue(near(TT2TrackEngine::rawToVolts(20000), 5.f), "over-max clamps to +5V");
}

CASE("raw_to_volts_known_value") {
    // 5000/16383 = 0.30519 -> *10 - 5 = -1.948V.
    expectTrue(near(TT2TrackEngine::rawToVolts(5000), -1.948f), "raw 5000 ~ -1.95V");
}

} // UNIT_TEST("TeletypeV2TrackEngineSmoke")
