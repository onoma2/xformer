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

} // UNIT_TEST("NoteSequence")
