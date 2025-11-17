#include "UnitTest.h"

#include "apps/sequencer/model/NoteSequence.h"

#include <string>

UNIT_TEST("GateMode") {

CASE("infrastructure") {
    // Placeholder test to verify test infrastructure is working
    expectTrue(true, "test infrastructure working");
}

// ============================================================================
// Phase 1: Model Layer Tests
// ============================================================================

// Test 1.1: Basic Storage and Retrieval
CASE("step stores and retrieves gate mode") {
    NoteSequence::Step step;

    // Default value should be 0 (MULTI)
    expectEqual(step.gateMode(), 0, "default gate mode should be 0 (MULTI)");

    // Test setting various values
    step.setGateMode(1);
    expectEqual(step.gateMode(), 1, "should store gate mode 1 (SINGLE)");

    step.setGateMode(2);
    expectEqual(step.gateMode(), 2, "should store gate mode 2 (HOLD)");

    step.setGateMode(3);
    expectEqual(step.gateMode(), 3, "should store gate mode 3 (FIRST_LAST)");
}

// Test 1.2: Value Clamping
CASE("gate mode clamps out-of-range values") {
    NoteSequence::Step step;

    // Negative values should clamp to 0
    step.setGateMode(-5);
    expectEqual(step.gateMode(), 0, "negative value should clamp to 0");

    step.setGateMode(-1);
    expectEqual(step.gateMode(), 0, "-1 should clamp to 0");

    // Values above max should clamp to 3
    step.setGateMode(4);
    expectEqual(step.gateMode(), 3, "4 should clamp to 3");

    step.setGateMode(10);
    expectEqual(step.gateMode(), 3, "10 should clamp to 3");

    step.setGateMode(100);
    expectEqual(step.gateMode(), 3, "100 should clamp to 3");

    // Values within range should be preserved
    for (int i = 0; i <= 3; ++i) {
        step.setGateMode(i);
        expectEqual(step.gateMode(), i, "value within range should be preserved");
    }
}

}
