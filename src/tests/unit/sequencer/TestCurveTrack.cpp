#include "UnitTest.h"

#include "apps/sequencer/model/CurveTrack.h"

UNIT_TEST("CurveTrack") {

CASE("globalPhase") {
    CurveTrack track;

    // test default value
    expectEqual(track.globalPhase(), 0.f, "default value");

    // test setter
    track.setGlobalPhase(0.5f);
    expectEqual(track.globalPhase(), 0.5f, "set value");

    // test clamping
    track.setGlobalPhase(1.5f);
    expectEqual(track.globalPhase(), 1.f, "clamp upper bound");

    track.setGlobalPhase(-0.5f);
    expectEqual(track.globalPhase(), 0.f, "clamp lower bound");

    // test edit
    track.setGlobalPhase(0.5f);
    track.editGlobalPhase(1, false); // 0.5 + 0.01
    expectEqual(track.globalPhase(), 0.51f, "edit value");
}

} // UNIT_TEST