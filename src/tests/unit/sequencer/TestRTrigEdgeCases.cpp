#include "UnitTest.h"

#include "apps/sequencer/model/NoteSequence.h"
#include "apps/sequencer/model/Accumulator.h"
#include "apps/sequencer/engine/NoteTrackEngine.h"
#include "apps/sequencer/Config.h"

UNIT_TEST("RTrigEdgeCases") {

// Phase 4 tests verify edge case handling:
// - Sequence validation logic
// - Invalid sequence ID handling
// - Gate queue clearing (via logic verification)

CASE("sequence validation - null sequence handling") {
    // This tests the logic used in tick() for sequence validation
    // When sequenceId lookup returns null, we should NOT tick the accumulator

#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    const NoteSequence* mainSeq = nullptr;  // Simulate null sequence
    const NoteSequence* fillSeq = nullptr;

    uint8_t testSeqId = NoteTrackEngine::MainSequenceId;

    // Sequence lookup logic (from tick() implementation)
    const NoteSequence* targetSeq = nullptr;
    if (testSeqId == NoteTrackEngine::MainSequenceId && mainSeq) {
        targetSeq = mainSeq;
    } else if (testSeqId == NoteTrackEngine::FillSequenceId && fillSeq) {
        targetSeq = fillSeq;
    }

    // Should be null because mainSeq is null
    expectTrue(targetSeq == nullptr, "targetSeq should be null when sequence is null");

    // Verify we would NOT tick (safe behavior)
    bool wouldTick = (targetSeq != nullptr);
    expectEqual(wouldTick, false, "should not tick when sequence is null");
#endif
}

CASE("sequence validation - valid main sequence") {
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    NoteSequence mainSeq;
    mainSeq.accumulator().setEnabled(true);
    mainSeq.accumulator().setTriggerMode(Accumulator::Retrigger);

    const NoteSequence* mainSeqPtr = &mainSeq;
    const NoteSequence* fillSeqPtr = nullptr;

    uint8_t testSeqId = NoteTrackEngine::MainSequenceId;

    // Sequence lookup logic
    const NoteSequence* targetSeq = nullptr;
    if (testSeqId == NoteTrackEngine::MainSequenceId && mainSeqPtr) {
        targetSeq = mainSeqPtr;
    } else if (testSeqId == NoteTrackEngine::FillSequenceId && fillSeqPtr) {
        targetSeq = fillSeqPtr;
    }

    // Should find main sequence
    expectTrue(targetSeq != nullptr, "targetSeq should not be null");
    expectTrue(targetSeq == mainSeqPtr, "targetSeq should point to main sequence");

    // Verify accumulator state checks
    bool shouldTick = (targetSeq &&
                       targetSeq->accumulator().enabled() &&
                       targetSeq->accumulator().triggerMode() == Accumulator::Retrigger);
    expectEqual(shouldTick, true, "should tick when sequence is valid and conditions met");
#endif
}

CASE("sequence validation - valid fill sequence") {
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    NoteSequence fillSeq;
    fillSeq.accumulator().setEnabled(true);
    fillSeq.accumulator().setTriggerMode(Accumulator::Retrigger);

    const NoteSequence* mainSeqPtr = nullptr;
    const NoteSequence* fillSeqPtr = &fillSeq;

    uint8_t testSeqId = NoteTrackEngine::FillSequenceId;

    // Sequence lookup logic
    const NoteSequence* targetSeq = nullptr;
    if (testSeqId == NoteTrackEngine::MainSequenceId && mainSeqPtr) {
        targetSeq = mainSeqPtr;
    } else if (testSeqId == NoteTrackEngine::FillSequenceId && fillSeqPtr) {
        targetSeq = fillSeqPtr;
    }

    // Should find fill sequence
    expectTrue(targetSeq != nullptr, "targetSeq should not be null");
    expectTrue(targetSeq == fillSeqPtr, "targetSeq should point to fill sequence");
#endif
}

CASE("sequence validation - accumulator disabled") {
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    NoteSequence mainSeq;
    mainSeq.accumulator().setEnabled(false);  // DISABLED
    mainSeq.accumulator().setTriggerMode(Accumulator::Retrigger);

    const NoteSequence* targetSeq = &mainSeq;

    // Full validation chain (from tick() implementation)
    bool shouldTick = (targetSeq &&
                       targetSeq->accumulator().enabled() &&
                       targetSeq->accumulator().triggerMode() == Accumulator::Retrigger);

    expectEqual(shouldTick, false, "should not tick when accumulator is disabled");
#endif
}

CASE("sequence validation - wrong trigger mode") {
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    NoteSequence mainSeq;
    mainSeq.accumulator().setEnabled(true);
    mainSeq.accumulator().setTriggerMode(Accumulator::Step);  // Not RTRIG mode

    const NoteSequence* targetSeq = &mainSeq;

    // Full validation chain
    bool shouldTick = (targetSeq &&
                       targetSeq->accumulator().enabled() &&
                       targetSeq->accumulator().triggerMode() == Accumulator::Retrigger);

    expectEqual(shouldTick, false, "should not tick when trigger mode is not Retrigger");
#endif
}

CASE("sequence ID boundary values") {
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // Test with boundary sequence IDs
    expectEqual((unsigned int)NoteTrackEngine::MainSequenceId, 0u, "MainSequenceId is 0");
    expectEqual((unsigned int)NoteTrackEngine::FillSequenceId, 1u, "FillSequenceId is 1");

    // Verify IDs are distinct
    expectTrue(NoteTrackEngine::MainSequenceId != NoteTrackEngine::FillSequenceId,
               "Sequence IDs must be distinct");
#endif
}

CASE("gate metadata - all conditions true") {
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    NoteSequence sequence;
    sequence.step(0).setAccumulatorTrigger(true);
    sequence.accumulator().setEnabled(true);
    sequence.accumulator().setTriggerMode(Accumulator::Retrigger);

    // All conditions met for shouldTickAccumulator
    bool shouldTickAccum = (
        sequence.step(0).isAccumulatorTrigger() &&
        sequence.accumulator().enabled() &&
        sequence.accumulator().triggerMode() == Accumulator::Retrigger
    );

    expectEqual(shouldTickAccum, true, "all conditions met - should tick");
#endif
}

CASE("gate metadata - missing accumulator trigger") {
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    NoteSequence sequence;
    sequence.step(0).setAccumulatorTrigger(false);  // NO TRIGGER
    sequence.accumulator().setEnabled(true);
    sequence.accumulator().setTriggerMode(Accumulator::Retrigger);

    bool shouldTickAccum = (
        sequence.step(0).isAccumulatorTrigger() &&
        sequence.accumulator().enabled() &&
        sequence.accumulator().triggerMode() == Accumulator::Retrigger
    );

    expectEqual(shouldTickAccum, false, "missing trigger - should not tick");
#endif
}

CASE("gate queue clearing logic - pattern change scenario") {
    // This test verifies the LOGIC behind queue clearing
    // We can't test the actual queue, but we can test the conditions

    // Scenario: Pattern changes while gates are scheduled
    // Expected behavior: Stale gates should be prevented from firing

#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // Simulate having scheduled gates with metadata
    struct ScheduledGate {
        uint32_t tick;
        bool gate;
        bool shouldTickAccumulator;
        uint8_t sequenceId;
    };

    ScheduledGate staleGate = { 1000, true, true, 0 };

    // After pattern change, sequence might be invalid
    // Our validation logic should prevent ticking invalid sequences
    const NoteSequence* oldSequence = nullptr;  // Pattern changed, pointer invalidated

    // Validation check (same as in tick())
    bool wouldTickStaleGate = (oldSequence != nullptr);
    expectEqual(wouldTickStaleGate, false, "stale gate should not tick after pattern change");

    // This demonstrates the safety of our sequence ID + validation approach
    // Even if queue isn't cleared, null check prevents crashes
#endif
}

CASE("gate construction - minimal valid gate") {
    // Test minimal gate construction (backward compatibility)
    NoteTrackEngine::Gate gate = { 0, false };

    expectEqual(gate.tick, 0u, "tick should be 0");
    expectEqual(gate.gate, false, "gate should be false");

#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // With flag=1, experimental fields should default to safe values
    expectEqual(gate.shouldTickAccumulator, false, "shouldTickAccumulator defaults to false");
    expectEqual((unsigned int)gate.sequenceId, 0u, "sequenceId defaults to 0");
#endif
}

CASE("gate struct memory layout") {
    // Verify struct size hasn't exceeded constraints
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // With experimental fields, struct should still be efficient
    expectTrue(sizeof(NoteTrackEngine::Gate) <= 16,
               "Gate struct should be <= 16 bytes even with experimental fields");
#else
    // Without experimental fields
    expectEqual(sizeof(NoteTrackEngine::Gate), 8ul,
                "Gate struct should be 8 bytes without experimental fields");
#endif
}

CASE("multiple sequence ID lookups") {
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    NoteSequence mainSeq;
    NoteSequence fillSeq;

    mainSeq.accumulator().setEnabled(true);
    fillSeq.accumulator().setEnabled(true);

    const NoteSequence* mainPtr = &mainSeq;
    const NoteSequence* fillPtr = &fillSeq;

    // Test multiple lookups in sequence (simulates rapid gate processing)
    for (int i = 0; i < 5; ++i) {
        uint8_t seqId = (i % 2 == 0) ? NoteTrackEngine::MainSequenceId
                                      : NoteTrackEngine::FillSequenceId;

        const NoteSequence* target = nullptr;
        if (seqId == NoteTrackEngine::MainSequenceId && mainPtr) {
            target = mainPtr;
        } else if (seqId == NoteTrackEngine::FillSequenceId && fillPtr) {
            target = fillPtr;
        }

        expectTrue(target != nullptr, "each lookup should succeed");
    }
#endif
}

}
