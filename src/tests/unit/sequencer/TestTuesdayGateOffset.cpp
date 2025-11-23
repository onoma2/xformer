#include "UnitTest.h"

#include "apps/sequencer/engine/TuesdayTrackEngine.h"

UNIT_TEST("TuesdayGateOffset") {

CASE("buffered_step_has_gate_offset_field") {
    TuesdayTrackEngine::PublicBufferedStep step;
    // Initialize with test values
    step.note = 0;
    step.octave = 0;
    step.gatePercent = 0;
    step.slide = 0;
    step.gateOffset = 50;  // Should compile and work

    expectEqual(static_cast<int>(step.gateOffset), static_cast<int>(50), "gateOffset field should store value correctly");
    expectEqual(static_cast<int>(step.gateOffset), 50, "gateOffset should be 50");
}

CASE("buffered_step_gate_offset_default_value") {
    TuesdayTrackEngine::PublicBufferedStep step = {};

    expectEqual(static_cast<int>(step.gateOffset), static_cast<int>(0), "gateOffset default value should be 0");
}

CASE("buffered_step_gate_offset_range") {
    TuesdayTrackEngine::PublicBufferedStep step;

    step.gateOffset = 0;
    expectEqual(static_cast<int>(step.gateOffset), static_cast<int>(0), "gateOffset minimum value should be 0");

    step.gateOffset = 100;
    expectEqual(static_cast<int>(step.gateOffset), static_cast<int>(100), "gateOffset maximum value should be 100");
}

} // UNIT_TEST("TuesdayGateOffset")