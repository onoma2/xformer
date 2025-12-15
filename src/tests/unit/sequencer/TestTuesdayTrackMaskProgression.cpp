#include "UnitTest.h"

#include "apps/sequencer/model/TuesdaySequence.h"

UNIT_TEST("TuesdayTrackMaskProgression") {

CASE("maskProgression_default_value") {
    TuesdaySequence sequence;
    expectEqual(sequence.maskProgression(), 0, "default maskProgression should be 0 (NO PROG)");
}

CASE("maskProgression_setter_getter") {
    TuesdaySequence sequence;
    sequence.setMaskProgression(1);
    expectEqual(sequence.maskProgression(), 1, "maskProgression should be 1 (PROG+1)");
    
    sequence.setMaskProgression(2);
    expectEqual(sequence.maskProgression(), 2, "maskProgression should be 2 (PROG+5)");
    
    sequence.setMaskProgression(3);
    expectEqual(sequence.maskProgression(), 3, "maskProgression should be 3 (PROG+7)");
}

CASE("maskProgression_clamping") {
    TuesdaySequence sequence;
    sequence.setMaskProgression(5);
    expectEqual(sequence.maskProgression(), 3, "maskProgression should clamp to 3 (max)");
    
    sequence.setMaskProgression(-1);
    expectEqual(sequence.maskProgression(), 0, "maskProgression should clamp to 0 (min)");
}

CASE("maskProgression_edit_cycles") {
    TuesdaySequence sequence;
    sequence.setMaskProgression(0);
    sequence.editMaskProgression(1, false);
    expectEqual(sequence.maskProgression(), 1, "should cycle to 1");
    sequence.editMaskProgression(1, false);
    expectEqual(sequence.maskProgression(), 2, "should cycle to 2");
    sequence.editMaskProgression(1, false);
    expectEqual(sequence.maskProgression(), 3, "should cycle to 3");
    sequence.editMaskProgression(1, false);
    expectEqual(sequence.maskProgression(), 0, "should cycle back to 0");
}

CASE("maskProgression_print_output") {
    TuesdaySequence sequence;
    
    sequence.setMaskProgression(0);
    StringBuilder str0;
    sequence.printMaskProgression(str0);
    expectEqual(strcmp(str0, "NO PROG"), 0, "should print 'NO PROG' for 0");
    
    sequence.setMaskProgression(1);
    StringBuilder str1;
    sequence.printMaskProgression(str1);
    expectEqual(strcmp(str1, "PROG+1"), 0, "should print 'PROG+1' for 1");
    
    sequence.setMaskProgression(2);
    StringBuilder str2;
    sequence.printMaskProgression(str2);
    expectEqual(strcmp(str2, "PROG+5"), 0, "should print 'PROG+5' for 2");
    
    sequence.setMaskProgression(3);
    StringBuilder str3;
    sequence.printMaskProgression(str3);
    expectEqual(strcmp(str3, "PROG+7"), 0, "should print 'PROG+7' for 3");
}

CASE("maskParameter_and_progression_independence") {
    TuesdaySequence sequence;
    
    // Set both parameters and verify independence
    sequence.setMaskParameter(5); // Maps to a specific mask value
    sequence.setMaskProgression(2); // Should advance every 5 steps
    
    expectEqual(sequence.maskParameter(), 5, "maskParameter should be 5");
    expectEqual(sequence.maskProgression(), 2, "maskProgression should be 2");
}

CASE("default_values_compatibility") {
    TuesdaySequence sequence;
    
    // Verify all related parameters have correct defaults
    expectEqual(sequence.maskParameter(), 0, "maskParameter should default to 0 (ALL)");
    expectEqual(sequence.maskProgression(), 0, "maskProgression should default to 0 (NO PROG)");
    expectEqual(sequence.timeMode(), 0, "timeMode should default to 0 (FREE)");
}

} // UNIT_TEST("TuesdayTrackMaskProgression")