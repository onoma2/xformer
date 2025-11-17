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

}
