#include "UnitTest.h"

#include "EngineTestFixture.h"

#include "apps/sequencer/model/Track.h"
#include "apps/sequencer/model/Routing.h"
#include "apps/sequencer/model/RouteApply.h"
#include "apps/sequencer/model/CvRoute.h"

#include <algorithm>
#include <cmath>

// The routing bus compose runs twice on a gated ticking frame (frame-level +
// in-tick recompute). With a single per-lane written flag the second pass adds
// the routing contribution again, doubling a constant-source bus route.
UNIT_TEST("RoutingBusDoubleCount") {

CASE("routing writer holds bus depth across ticking frames") {
    EngineTestFixture fixture;

    // Track 0 = Note with a gated step so it produces CV updates each cycle —
    // that is what arms the in-tick second compose.
    fixture.project().setTrackMode(0, Track::TrackMode::Note);
    fixture.project().track(0).noteTrack().sequence(0).step(0).setGate(true);

    // Constant route: CvIn1 -> BusCv1, Absolute, full depth.
    auto &route = fixture.project().routing().route(0);
    route.setSource(Routing::Source::CvIn1);
    route.setTarget(Routing::Target::BusCv1);
    route.setDepthPct(0, 100);
    route.setCombine(RouteApply::Combine::Absolute);
    // Keep the single contribution low (~1.5V) so a doubled value (~3V) stays
    // under the +-5V read clamp that would otherwise hide the bug.
    fixture.setCvIn(0, -2.f);

    fixture.start();
    fixture.advance(50);   // warm up (route resolves, track starts ticking)

    // The source is constant, so a correct bus reads a stable value on every
    // frame. Today it jumps to ~2x on the frames where the second compose runs.
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < 300; ++i) {
        fixture.advance(2);
        float v = fixture.engine().busCv(0);
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }

    expect(hi > 0.1f, "route actually drives the bus");
    expect(hi - lo < 0.05f, "bus value stable across frames (not doubled)");
}

CASE("cv-router writer not doubled across ticking frames") {
    EngineTestFixture fixture;
    fixture.project().setTrackMode(0, Track::TrackMode::Note);
    fixture.project().track(0).noteTrack().sequence(0).step(0).setGate(true);

    // CV-router lane 0: CvIn -> Bus, scan/route pinned to lane 0.
    auto &cv = fixture.project().cvRoute();
    cv.setInputSource(0, CvRoute::InputSource::CvIn);
    cv.setOutputDest(0, CvRoute::OutputDest::Bus);
    cv.setScan(0);
    cv.setRoute(0);
    fixture.setCvIn(0, -2.f);

    fixture.start();
    fixture.advance(50);
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < 300; ++i) {
        fixture.advance(2);
        float v = fixture.engine().busCv(0);
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    expect(std::fabs(hi) > 0.1f || std::fabs(lo) > 0.1f, "cv-route drives the bus");
    expect(hi - lo < 0.05f, "cv-router bus value stable (not doubled)");
}

CASE("cv-router bus contribution clears under cv-output override") {
    EngineTestFixture fixture;
    auto &cv = fixture.project().cvRoute();
    cv.setInputSource(0, CvRoute::InputSource::CvIn);
    cv.setOutputDest(0, CvRoute::OutputDest::Bus);
    cv.setScan(0);
    cv.setRoute(0);
    fixture.setCvIn(0, -2.f);

    fixture.start();
    fixture.advance(20);
    expect(std::fabs(fixture.engine().busCv(0)) > 0.1f, "cv-route drives the bus");

    // Override skips the CV-router compose; its slot must drop to 0, not stale-hold.
    fixture.engine().setCvOutputOverride(true);
    fixture.advance(20);
    expect(std::fabs(fixture.engine().busCv(0)) < 0.05f, "cv-router slot cleared, no stale value");
}

CASE("teletype writes sum within a frame") {
    EngineTestFixture fixture;
    fixture.engine().setBusCv(1, 1.5f, Engine::BusWriterTeletype);
    fixture.engine().setBusCv(1, 1.0f, Engine::BusWriterTeletype);
    expectEqual(fixture.engine().busCv(1), 2.5f, "two teletype writes sum");
}

}
