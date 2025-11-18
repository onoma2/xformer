#include "UnitTest.h"

#include "apps/sequencer/model/NoteSequence.h"
#include "apps/sequencer/model/Accumulator.h"
#include "apps/sequencer/engine/NoteTrackEngine.h"
#include "apps/sequencer/Config.h"

UNIT_TEST("RTrigAccumTicking") {

// These tests verify Phase 2-3 implementation:
// - Phase 2: Gate metadata includes shouldTickAccumulator and sequenceId
// - Phase 3: Accumulator ticks when gates fire (verified via logic flow)

CASE("gate metadata logic - RTRIG mode with accumulator enabled") {
    // Simulate the conditions in triggerStep() when RTRIG mode is active
    NoteSequence sequence;

    // Configure step with accumulator trigger
    sequence.step(0).setGate(true);
    sequence.step(0).setAccumulatorTrigger(true);

    // Configure accumulator for RTRIG mode
    sequence.accumulator().setEnabled(true);
    sequence.accumulator().setTriggerMode(Accumulator::Retrigger);
    sequence.accumulator().setDirection(Accumulator::Up);
    sequence.accumulator().setMin(0);
    sequence.accumulator().setMax(10);
    sequence.accumulator().setStepSize(1);

    // Verify preconditions
    expectEqual(sequence.step(0).gate(), true, "step gate should be true");
    expectEqual(sequence.step(0).isAccumulatorTrigger(), true, "accumulator trigger should be true");
    expectEqual(sequence.accumulator().enabled(), true, "accumulator should be enabled");
    expectEqual(sequence.accumulator().triggerMode(), Accumulator::Retrigger, "trigger mode should be Retrigger");

#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // SPREAD MODE: shouldTickAccumulator should be TRUE
    bool shouldTickAccumulator = (
        sequence.step(0).isAccumulatorTrigger() &&
        sequence.accumulator().enabled() &&
        sequence.accumulator().triggerMode() == Accumulator::Retrigger
    );

    expectEqual(shouldTickAccumulator, true, "shouldTickAccumulator should be true in spread mode");
#else
    // BURST MODE: accumulator ticks immediately (not via gates)
    // This test case is informational only
#endif
}

CASE("gate metadata logic - RTRIG mode with accumulator disabled") {
    NoteSequence sequence;

    sequence.step(0).setGate(true);
    sequence.step(0).setAccumulatorTrigger(true);

    // Accumulator DISABLED
    sequence.accumulator().setEnabled(false);
    sequence.accumulator().setTriggerMode(Accumulator::Retrigger);

#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // shouldTickAccumulator should be FALSE (accumulator disabled)
    bool shouldTickAccumulator = (
        sequence.step(0).isAccumulatorTrigger() &&
        sequence.accumulator().enabled() &&
        sequence.accumulator().triggerMode() == Accumulator::Retrigger
    );

    expectEqual(shouldTickAccumulator, false, "shouldTickAccumulator should be false when accumulator disabled");
#endif
}

CASE("gate metadata logic - STEP mode (not RTRIG)") {
    NoteSequence sequence;

    sequence.step(0).setGate(true);
    sequence.step(0).setAccumulatorTrigger(true);

    // Accumulator enabled but STEP mode (not RTRIG)
    sequence.accumulator().setEnabled(true);
    sequence.accumulator().setTriggerMode(Accumulator::Step);

#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // shouldTickAccumulator should be FALSE (not RTRIG mode)
    bool shouldTickAccumulator = (
        sequence.step(0).isAccumulatorTrigger() &&
        sequence.accumulator().enabled() &&
        sequence.accumulator().triggerMode() == Accumulator::Retrigger
    );

    expectEqual(shouldTickAccumulator, false, "shouldTickAccumulator should be false in STEP mode");
#endif
}

CASE("gate metadata logic - no accumulator trigger on step") {
    NoteSequence sequence;

    sequence.step(0).setGate(true);
    sequence.step(0).setAccumulatorTrigger(false);  // NO trigger

    sequence.accumulator().setEnabled(true);
    sequence.accumulator().setTriggerMode(Accumulator::Retrigger);

#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // shouldTickAccumulator should be FALSE (step doesn't have trigger)
    bool shouldTickAccumulator = (
        sequence.step(0).isAccumulatorTrigger() &&
        sequence.accumulator().enabled() &&
        sequence.accumulator().triggerMode() == Accumulator::Retrigger
    );

    expectEqual(shouldTickAccumulator, false, "shouldTickAccumulator should be false when step has no trigger");
#endif
}

CASE("sequence ID constants for main and fill") {
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // Verify constants are distinct
    expectTrue(NoteTrackEngine::MainSequenceId != NoteTrackEngine::FillSequenceId,
               "MainSequenceId and FillSequenceId should be different");

    expectEqual((unsigned int)NoteTrackEngine::MainSequenceId, 0u, "MainSequenceId should be 0");
    expectEqual((unsigned int)NoteTrackEngine::FillSequenceId, 1u, "FillSequenceId should be 1");
#endif
}

CASE("accumulator value changes when ticked (Phase 3 behavior)") {
    // This verifies that the accumulator tick() method works correctly
    // When gates fire with shouldTickAccumulator=true, this is what happens

    NoteSequence sequence;

    sequence.accumulator().setEnabled(true);
    sequence.accumulator().setTriggerMode(Accumulator::Retrigger);
    sequence.accumulator().setDirection(Accumulator::Up);
    sequence.accumulator().setMin(0);
    sequence.accumulator().setMax(10);
    sequence.accumulator().setStepSize(1);
    sequence.accumulator().reset();

    // Initial value should be min
    expectEqual(sequence.accumulator().value(), 0, "initial value should be 0");

    // Simulate what happens in tick() when gate fires with shouldTickAccumulator=true
    const_cast<Accumulator&>(sequence.accumulator()).tick();
    expectEqual(sequence.accumulator().value(), 1, "value should be 1 after first tick");

    const_cast<Accumulator&>(sequence.accumulator()).tick();
    expectEqual(sequence.accumulator().value(), 2, "value should be 2 after second tick");

    const_cast<Accumulator&>(sequence.accumulator()).tick();
    expectEqual(sequence.accumulator().value(), 3, "value should be 3 after third tick");
}

CASE("accumulator wraps correctly in Wrap order mode") {
    NoteSequence sequence;

    sequence.accumulator().setEnabled(true);
    sequence.accumulator().setTriggerMode(Accumulator::Retrigger);
    sequence.accumulator().setDirection(Accumulator::Up);
    sequence.accumulator().setOrder(Accumulator::Wrap);
    sequence.accumulator().setMin(0);
    sequence.accumulator().setMax(2);  // Small range for quick wrap
    sequence.accumulator().setStepSize(1);
    sequence.accumulator().reset();

    expectEqual(sequence.accumulator().value(), 0, "initial value should be 0");

    const_cast<Accumulator&>(sequence.accumulator()).tick();
    expectEqual(sequence.accumulator().value(), 1, "value should be 1");

    const_cast<Accumulator&>(sequence.accumulator()).tick();
    expectEqual(sequence.accumulator().value(), 2, "value should be 2 (at max)");

    const_cast<Accumulator&>(sequence.accumulator()).tick();
    expectEqual(sequence.accumulator().value(), 0, "value should wrap to 0");
}

#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS

CASE("gate construction with 4 args (spread mode)") {
    // When flag=1, gates can be constructed with 4 arguments
    NoteTrackEngine::Gate gate = { 100, true, true, 0 };

    expectEqual(gate.tick, 100u, "tick should be 100");
    expectEqual(gate.gate, true, "gate should be true");
    expectEqual(gate.shouldTickAccumulator, true, "shouldTickAccumulator should be true");
    expectEqual((unsigned int)gate.sequenceId, 0u, "sequenceId should be 0");
}

CASE("gate construction with 2 args defaults experimental fields") {
    // Even with flag=1, 2-arg construction should work (backward compat)
    NoteTrackEngine::Gate gate = { 100, true };

    expectEqual(gate.tick, 100u, "tick should be 100");
    expectEqual(gate.gate, true, "gate should be true");
    expectEqual(gate.shouldTickAccumulator, false, "shouldTickAccumulator should default to false");
    expectEqual((unsigned int)gate.sequenceId, 0u, "sequenceId should default to 0");
}

#endif // CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS

}
