#include "UnitTest.h"

#include "EngineTestFixture.h"

#include "apps/sequencer/model/Track.h"

// Phase 0 smoke test: proves the headless EngineTestFixture drives real engine
// ticks through the Simulator::wait -> ClockTimer -> Clock -> Engine chain.
// This is the seam the previous two engine-test attempts failed to reach.
UNIT_TEST("EngineTestFixture") {

CASE("clock ticks advance the engine under wait") {
    EngineTestFixture fixture;
    fixture.project().setTrackMode(0, Track::TrackMode::Note);

    expect(!fixture.engine().clockRunning(), "engine stopped before start");

    fixture.start();
    fixture.advance(200);

    expect(fixture.engine().clockRunning(), "clock running after start");
    expect(fixture.engine().tick() > 0, "engine tick advanced after wait");
}

}
