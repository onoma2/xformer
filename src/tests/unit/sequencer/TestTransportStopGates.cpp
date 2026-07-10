#include "UnitTest.h"

#include "EngineTestFixture.h"

#include "apps/sequencer/model/Track.h"

// A Clock::Stop must drop pending gates. Otherwise a gate-on whose scheduled
// gate-off is still in the queue when Stop arrives sticks HIGH forever (the
// clock stops ticking, so the queued gate-off never fires).
UNIT_TEST("TransportStopGates") {

CASE("Clock::Stop drops a pending Note gate") {
    EngineTestFixture fixture;
    fixture.project().setTrackMode(0, Track::TrackMode::Note);
    auto &seq = fixture.project().track(0).noteTrack().sequence(0);
    seq.step(0).setGate(true);
    seq.step(0).setLength(7);   // longest gate, so a stop lands mid-gate

    fixture.start();

    // advance until the gate goes high (step 0 fires)
    bool sawHigh = false;
    for (int i = 0; i < 500 && !sawHigh; ++i) {
        fixture.advance(1);
        if (fixture.engine().trackEngine(0).gateOutput(0)) sawHigh = true;
    }
    expect(sawHigh, "gate went high while running");

    // stop mid-gate; the queued gate-off is still pending
    fixture.stop();
    fixture.advance(4);
    expect(!fixture.engine().trackEngine(0).gateOutput(0), "gate dropped on stop");
}

}
