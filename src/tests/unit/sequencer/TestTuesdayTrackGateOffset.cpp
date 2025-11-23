#include "UnitTest.h"

#include "apps/sequencer/model/TuesdayTrack.h"

UNIT_TEST("TuesdayTrackGateOffset") {

CASE("gate_offset_default_value") {
    TuesdayTrack track;
    expectEqual(track.gateOffset(), 0, "GateOffset should default to 0");
}

CASE("gate_offset_setter_getter") {
    TuesdayTrack track;
    
    track.setGateOffset(50);
    expectEqual(track.gateOffset(), 50, "GateOffset should be 50 after setting");
    
    track.setGateOffset(100);
    expectEqual(track.gateOffset(), 100, "GateOffset should be 100 after setting");
}

CASE("gate_offset_clamping") {
    TuesdayTrack track;
    
    // Test clamping at upper bound
    track.setGateOffset(150);
    expectEqual(track.gateOffset(), 100, "GateOffset should clamp to 100 at upper bound");
    
    // Test clamping at lower bound
    track.setGateOffset(-10);
    expectEqual(track.gateOffset(), 0, "GateOffset should clamp to 0 at lower bound");
    
    // Test valid range still works
    track.setGateOffset(75);
    expectEqual(track.gateOffset(), 75, "GateOffset should be 75 within valid range");
}

CASE("gate_offset_edit_function") {
    TuesdayTrack track;
    
    // Test normal edit
    track.setGateOffset(30);
    track.editGateOffset(10, false);  // shift = false, so add 10
    expectEqual(track.gateOffset(), 40, "GateOffset should be 40 after adding 10");
    
    // Test shift edit (multiplier = 10)
    track.editGateOffset(5, true);    // shift = true, so add 5*10 = 50
    expectEqual(track.gateOffset(), 90, "GateOffset should be 90 after adding 50");
    
    // Test clamping during edit
    track.editGateOffset(20, true);   // Would be 90+200 = 290, should clamp to 100
    expectEqual(track.gateOffset(), 100, "GateOffset should clamp to 100 during edit");
}

CASE("gate_offset_print_function") {
    TuesdayTrack track;

    track.setGateOffset(42);
    char buf[64];
    StringBuilder str(buf, sizeof(buf));
    track.printGateOffset(str);
    expectTrue(strcmp(buf, "42%") == 0, "printGateOffset should format percentage correctly");
}

} // UNIT_TEST("TuesdayTrackGateOffset")