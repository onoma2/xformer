#include "UnitTest.h"

#include "apps/sequencer/model/NoteSequence.h"

UNIT_TEST("NoteSequence") {

CASE("step_is_accumulator_trigger") {
    NoteSequence::Step step;
    step.setAccumulatorTrigger(true);
    expectTrue(step.isAccumulatorTrigger(), "isAccumulatorTrigger should be true");
    step.setAccumulatorTrigger(false);
    expectFalse(step.isAccumulatorTrigger(), "isAccumulatorTrigger should be false");
    step.toggleAccumulatorTrigger();
    expectTrue(step.isAccumulatorTrigger(), "isAccumulatorTrigger should be true after toggle");
}

CASE("note_sequence_has_accumulator") {
    NoteSequence noteSequence;
    Accumulator &accumulator = noteSequence.accumulator();
    accumulator.setEnabled(true);
    expectTrue(noteSequence.accumulator().enabled(), "accumulator should be enabled");
}

CASE("harmony_track_position_constraints") {
    // Track 1 (index 0) should be able to be HarmonyMaster
    NoteSequence seq1(0);
    expectTrue(seq1.canBeHarmonyMaster(), "Track 1 should be able to be HarmonyMaster");

    seq1.setHarmonyRole(NoteSequence::HarmonyMaster);
    expectEqual(static_cast<int>(seq1.harmonyRole()), static_cast<int>(NoteSequence::HarmonyMaster), "Track 1 should be HarmonyMaster");

    // Track 2 (index 1) should NOT be able to be HarmonyMaster
    NoteSequence seq2(1);
    expectFalse(seq2.canBeHarmonyMaster(), "Track 2 should NOT be able to be HarmonyMaster");

    seq2.setHarmonyRole(NoteSequence::HarmonyMaster);
    expectTrue(static_cast<int>(seq2.harmonyRole()) != static_cast<int>(NoteSequence::HarmonyMaster), "Track 2 should auto-revert from HarmonyMaster");
    expectEqual(static_cast<int>(seq2.harmonyRole()), static_cast<int>(NoteSequence::HarmonyFollower3rd), "Track 2 should auto-assign to HarmonyFollower3rd");

    // Track 5 (index 4) should be able to be HarmonyMaster
    NoteSequence seq5(4);
    expectTrue(seq5.canBeHarmonyMaster(), "Track 5 should be able to be HarmonyMaster");

    seq5.setHarmonyRole(NoteSequence::HarmonyMaster);
    expectEqual(static_cast<int>(seq5.harmonyRole()), static_cast<int>(NoteSequence::HarmonyMaster), "Track 5 should be HarmonyMaster");
}

} // UNIT_TEST("NoteSequence")
