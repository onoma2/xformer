#include "UnitTest.h"
#include "apps/sequencer/model/CurveTrack.h"
#include "apps/sequencer/model/Curve.h"
#include <cmath>

// Simple test to verify phase offset logic
UNIT_TEST("CurveTrackEnginePhase") {

CASE("phase_offset_calculation") {
    // Test the phase offset calculation directly
    // Simulating: fmod(_currentStepFraction + phaseOffset, 1.0f)

    // Test 1: No phase offset
    float currentFraction = 0.5f;
    float phaseOffset = 0.0f;
    float phasedFraction = fmod(currentFraction + phaseOffset, 1.0f);
    expectEqual(phasedFraction, 0.5f, "No phase offset should give original fraction");

    // Test 2: 50% phase offset
    currentFraction = 0.0f;
    phaseOffset = 0.5f;
    phasedFraction = fmod(currentFraction + phaseOffset, 1.0f);
    expectEqual(phasedFraction, 0.5f, "50% phase offset from 0 should give 0.5");

    // Test 3: Wraparound
    currentFraction = 0.75f;
    phaseOffset = 0.5f;
    phasedFraction = fmod(currentFraction + phaseOffset, 1.0f);
    expectTrue(fabs(phasedFraction - 0.25f) < 0.001f, "Phase wraparound should work correctly");
}

CASE("phase_offset_with_ramp_curve") {
    // Test that phase offset shifts the curve evaluation point
    // For a ramp (0.0 to 1.0), phase offset should directly shift the value

    auto rampFunction = Curve::function(Curve::Type::RampUp);

    // At fraction 0.0, ramp gives 0.0
    float value1 = rampFunction(0.0f);
    expectTrue(fabs(value1 - 0.0f) < 0.001f, "RampUp at 0.0 should be 0.0");

    // At fraction 0.5, ramp gives 0.5
    float value2 = rampFunction(0.5f);
    expectTrue(fabs(value2 - 0.5f) < 0.001f, "RampUp at 0.5 should be 0.5");

    // At fraction 1.0, ramp gives 1.0
    float value3 = rampFunction(1.0f);
    expectTrue(fabs(value3 - 1.0f) < 0.001f, "RampUp at 1.0 should be 1.0");
}

CASE("phase_offset_range_conversion") {
    // Test converting phase offset from 0-100 range to 0.0-1.0

    int phaseOffset0 = 0;
    float normalized0 = float(phaseOffset0) / 100.f;
    expectEqual(normalized0, 0.0f, "0 should normalize to 0.0");

    int phaseOffset50 = 50;
    float normalized50 = float(phaseOffset50) / 100.f;
    expectEqual(normalized50, 0.5f, "50 should normalize to 0.5");

    int phaseOffset100 = 100;
    float normalized100 = float(phaseOffset100) / 100.f;
    expectEqual(normalized100, 1.0f, "100 should normalize to 1.0");
}

CASE("phase_offset_with_step_boundary") {
    // Verify that phase offset works correctly at step boundaries

    // Step fraction at start (0.0) with 25% phase offset
    float stepFraction = 0.0f;
    float phaseOffset = 0.25f;
    float phasedFraction = fmod(stepFraction + phaseOffset, 1.0f);
    expectEqual(phasedFraction, 0.25f, "Phase offset at step start");

    // Step fraction at end (0.99) with 25% phase offset (should wrap)
    stepFraction = 0.99f;
    phaseOffset = 0.25f;
    phasedFraction = fmod(stepFraction + phaseOffset, 1.0f);
    expectTrue(fabs(phasedFraction - 0.24f) < 0.01f, "Phase offset near step end should wrap");
}

CASE("phase_offset_preserves_curve_shape") {
    // Verify that phase offset shifts but doesn't distort the curve
    auto expFunction = Curve::function(Curve::Type::ExpUp);

    // Sample exp at 0.0
    float exp0 = expFunction(0.0f);

    // Sample exp at 0.25 (with 0% offset at step 0.25)
    float exp25 = expFunction(0.25f);

    // Simulate sampling at step 0.0 with 25% phase offset
    float phaseOffset = 0.25f;
    float stepFraction = 0.0f;
    float phasedFraction = fmod(stepFraction + phaseOffset, 1.0f);
    float expPhased = expFunction(phasedFraction);

    // They should match (phase offset shifted the sampling point)
    expectTrue(fabs(expPhased - exp25) < 0.001f, "Phase offset should shift sampling point");
}

} // UNIT_TEST("CurveTrackEnginePhase")
