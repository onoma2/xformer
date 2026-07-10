#include "UnitTest.h"

#include "EngineTestFixture.h"

#include "apps/sequencer/model/Routing.h"

// 3.3 — reassigning a route slot from a CV source to MIDI must not apply the
// stale CV value. updateSources() skips MIDI slots (their value is set only in
// receiveMidi), so without a reset on reassignment the new MIDI route inherits
// the previous CV source value until the first MIDI message arrives.
UNIT_TEST("RoutingMidiStaleSource") {

CASE("slot reassigned CV -> MIDI drops the stale source value") {
    EngineTestFixture fixture;

    auto &route = fixture.project().routing().route(0);
    route.setSource(Routing::Source::CvIn1);
    route.setTarget(Routing::Target::BusCv1);
    route.setDepthPct(0, 100);
    fixture.setCvIn(0, 4.f);                     // drive the CV source high

    fixture.start();
    fixture.advance(20);
    expect(fixture.engine().routingEngine().routeSource(0) > 0.1f, "CV source value populated");

    // Reassign the same slot to MIDI; send no MIDI message.
    route.setSource(Routing::Source::Midi);
    fixture.advance(20);
    expect(fixture.engine().routingEngine().routeSource(0) < 0.05f,
           "MIDI slot starts neutral, not the stale CV value");
}

}
