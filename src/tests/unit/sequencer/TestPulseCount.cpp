#include "UnitTest.h"

#include "apps/sequencer/model/NoteSequence.h"

UNIT_TEST("PulseCount") {

CASE("infrastructure") {
    // Placeholder test to verify test infrastructure is working
    expectTrue(true, "test infrastructure working");
}

// ============================================================================
// Phase 1: Model Layer Tests
// ============================================================================

// Test 1.1: Basic Storage and Retrieval
CASE("step stores and retrieves pulse count") {
    NoteSequence::Step step;

    // Default pulse count should be 0
    expectEqual(step.pulseCount(), 0, "default pulse count should be 0");

    // Can set and get pulse count
    step.setPulseCount(3);
    expectEqual(step.pulseCount(), 3, "should store pulse count 3");

    // Can set minimum value
    step.setPulseCount(0);
    expectEqual(step.pulseCount(), 0, "should store minimum value 0");

    // Can set maximum value
    step.setPulseCount(7);
    expectEqual(step.pulseCount(), 7, "should store maximum value 7");
}

// Test 1.2: Value Clamping
CASE("pulse count clamps out-of-range values") {
    NoteSequence::Step step;

    // Negative values should clamp to 0
    step.setPulseCount(-5);
    expectEqual(step.pulseCount(), 0, "negative value should clamp to 0");

    step.setPulseCount(-1);
    expectEqual(step.pulseCount(), 0, "-1 should clamp to 0");

    // Values above max should clamp to 7
    step.setPulseCount(10);
    expectEqual(step.pulseCount(), 7, "10 should clamp to 7");

    step.setPulseCount(100);
    expectEqual(step.pulseCount(), 7, "100 should clamp to 7");

    step.setPulseCount(255);
    expectEqual(step.pulseCount(), 7, "255 should clamp to 7");

    // Values within range should be preserved
    for (int i = 0; i <= 7; ++i) {
        step.setPulseCount(i);
        expectEqual(step.pulseCount(), i, "value within range should be preserved");
    }
}

}
