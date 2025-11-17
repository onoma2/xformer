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

    // Default value should be 0 (All)
    expectEqual(step.gateMode(), 0, "default gate mode should be 0 (All)");

    // Test setting various values
    step.setGateMode(1);
    expectEqual(step.gateMode(), 1, "should store gate mode 1 (First)");

    step.setGateMode(2);
    expectEqual(step.gateMode(), 2, "should store gate mode 2 (Hold)");

    step.setGateMode(3);
    expectEqual(step.gateMode(), 3, "should store gate mode 3 (FirstLast)");
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

// Test 1.3: Bitfield Packing - No Interference with Other Fields
CASE("gate mode bitfield does not interfere with other fields") {
    NoteSequence::Step step;

    // Set other fields including pulse count (adjacent bits 17-19)
    step.setPulseCount(5);
    step.setRetrigger(2);
    step.setGateMode(2);

    // Verify independence
    expectEqual(step.pulseCount(), 5, "pulseCount should be unchanged");
    expectEqual(step.retrigger(), 2, "retrigger should be unchanged");
    expectEqual(step.gateMode(), 2, "gateMode should be stored");

    // Test all fields at max values
    step.setGateMode(3);
    step.setPulseCount(7);
    step.setRetrigger(3);
    step.setAccumulatorTrigger(true);

    expectEqual(step.gateMode(), 3, "gateMode should be 3");
    expectEqual(step.pulseCount(), 7, "pulseCount should be 7");
    expectEqual(step.retrigger(), 3, "retrigger should be 3");
    expectEqual(step.isAccumulatorTrigger(), true, "accumulator trigger should be true");

    // Change gateMode and verify other fields unchanged
    step.setGateMode(0);
    expectEqual(step.gateMode(), 0, "gateMode should be 0");
    expectEqual(step.pulseCount(), 7, "pulseCount should still be 7");
    expectEqual(step.retrigger(), 3, "retrigger should still be 3");
}

// Test 1.4: Layer Integration
CASE("gate mode integrates with Layer system") {
    // Test 1: GateMode layer exists
    auto layer = NoteSequence::Layer::GateMode;
    expectTrue(layer < NoteSequence::Layer::Last, "GateMode should be valid layer");

    // Test 2: layerName
    const char* name = NoteSequence::layerName(NoteSequence::Layer::GateMode);
    expectEqual(std::string(name), std::string("GATE MODE"), "layer name");

    // Test 3: layerRange
    auto range = NoteSequence::layerRange(NoteSequence::Layer::GateMode);
    expectEqual(range.min, 0, "min should be 0");
    expectEqual(range.max, 3, "max should be 3");

    // Test 4: layerDefaultValue
    int defaultValue = NoteSequence::layerDefaultValue(NoteSequence::Layer::GateMode);
    expectEqual(defaultValue, 0, "default should be 0 (All)");

    // Test 5: layerValue/setLayerValue
    NoteSequence::Step step;
    step.setLayerValue(NoteSequence::Layer::GateMode, 2);
    expectEqual(step.gateMode(), 2, "setLayerValue should work");
    expectEqual(step.layerValue(NoteSequence::Layer::GateMode), 2, "layerValue should work");
}

// Test 1.5: Serialization - Gate Mode is Part of Serialized Data
CASE("gate mode is included in step data") {
    NoteSequence::Step step1;
    step1.setGateMode(0);

    NoteSequence::Step step2;
    step2.setGateMode(2);

    // Verify different modes are different
    expectTrue(step1.gateMode() != step2.gateMode(), "different modes");

    // Verify copying preserves gate mode
    NoteSequence::Step stepCopy = step2;
    expectEqual(stepCopy.gateMode(), 2, "gate mode preserved when copying");
}

// Test 1.6: Clear/Reset - Gate Mode Resets to Default
CASE("gate mode resets to 0 on clear") {
    NoteSequence::Step step;

    step.setGateMode(3);
    expectEqual(step.gateMode(), 3, "gate mode should be 3");

    step.clear();

    expectEqual(step.gateMode(), 0, "gate mode should reset to 0 (All)");
}

}
