#include "UnitTest.h"

#include "apps/sequencer/engine/NoteTrackEngine.h"
#include "apps/sequencer/model/NoteSequence.h"

// The pulse-hold (how long a step holds / retriggers) must read its pulse count
// from the AUDIBLE, rotated step — not the raw sequence-state index. With
// rotate != 0 the two diverge, so reading the unrotated index holds the wrong
// step. rotatedStepPulseCount() is the pure lookup the engine now uses.
UNIT_TEST("NoteTrackPulseHold") {

CASE("pulse count comes from the rotated audible step") {
    NoteSequence seq;
    seq.setFirstStep(0);
    seq.setLastStep(3);
    seq.step(0).setPulseCount(0);
    seq.step(1).setPulseCount(0);
    seq.step(2).setPulseCount(0);
    seq.step(3).setPulseCount(5);

    // state step 2, rotate 1 -> audible step 3 (pulseCount 5).
    int rotated = NoteTrackEngine::rotatedStepPulseCount(seq, 2, 0, 3, 1);
    int unrotated = seq.step(2).pulseCount();   // the old (buggy) read

    expectEqual(rotated, 5, "reads the rotated step 3 pulse count");
    expectEqual(unrotated, 0, "the unrotated read would take step 2 (wrong)");
    expect(rotated != unrotated, "rotation changes which step's pulse count is used");
}

CASE("rotate 0 reads the state step itself") {
    NoteSequence seq;
    seq.setFirstStep(0);
    seq.setLastStep(3);
    seq.step(2).setPulseCount(4);
    expectEqual(NoteTrackEngine::rotatedStepPulseCount(seq, 2, 0, 3, 0), 4, "rotate 0 unchanged");
}

CASE("rotation wraps within the first..last window") {
    NoteSequence seq;
    seq.setFirstStep(0);
    seq.setLastStep(3);
    seq.step(0).setPulseCount(7);
    // state step 3, rotate 1 -> wraps to step 0 (pulseCount 7).
    expectEqual(NoteTrackEngine::rotatedStepPulseCount(seq, 3, 0, 3, 1), 7, "wraps to firstStep");
}

}
