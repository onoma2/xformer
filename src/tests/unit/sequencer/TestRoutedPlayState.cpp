#include "UnitTest.h"

#include "EngineTestFixture.h"

#include "apps/sequencer/model/Routing.h"
#include "apps/sequencer/model/RouteResolve.h"
#include "apps/sequencer/model/RouteBrowse.h"
#include "apps/sequencer/model/RouteParamKey.h"

// 3.2 — PlayState routing port: Mute/Fill/FillAmount/Pattern are per-track,
// level-driven route targets (no rising edge), re-evaluated each frame and
// idempotent via PlayState::writeRouted's equality guards. Our fork left them
// as no-ops; this wires them the Run/Reset way (raw source -> writeRouted).
namespace {
void playRoute(EngineTestFixture &f, Routing::Target target, int track) {
    auto &route = f.project().routing().route(0);
    route.setSource(Routing::Source::CvIn1);
    route.setTarget(target);
    route.setTracks(1 << track);
}
} // namespace

UNIT_TEST("RoutedPlayState") {

CASE("every PlayState target round-trips through a ParamKey") {
    const Routing::Target targets[] = { Routing::Target::Mute, Routing::Target::Fill,
                                        Routing::Target::FillAmount, Routing::Target::Pattern };
    for (auto t : targets) {
        uint8_t key = RouteResolve::targetToParamKey(t);
        expect(key != ParamKey::None, "target maps to a paramKey");
        expectEqual(int(RouteBrowse::paramKeyToTarget(key)), int(t), "paramKey maps back to the target");
    }
}

CASE("mute route mutes the masked track at high source, unmutes at low") {
    EngineTestFixture fixture;
    playRoute(fixture, Routing::Target::Mute, 3);
    fixture.setCvIn(0, 5.f);

    fixture.start();
    fixture.advance(20);
    expect(fixture.project().playState().trackState(3).mute(), "track 3 muted when source high");

    fixture.setCvIn(0, -5.f);
    fixture.advance(20);
    expect(!fixture.project().playState().trackState(3).mute(), "track 3 unmuted when source low");
}

CASE("fill route flips the masked track fill") {
    EngineTestFixture fixture;
    playRoute(fixture, Routing::Target::Fill, 2);
    fixture.setCvIn(0, 5.f);

    fixture.start();
    fixture.advance(20);
    expect(fixture.project().playState().trackState(2).fill(), "track 2 fill on when source high");

    fixture.setCvIn(0, -5.f);
    fixture.advance(20);
    expect(!fixture.project().playState().trackState(2).fill(), "track 2 fill off when source low");
}

CASE("fillAmount route scales 0..100 with the source") {
    EngineTestFixture fixture;
    playRoute(fixture, Routing::Target::FillAmount, 1);

    fixture.setCvIn(0, 5.f);
    fixture.start();
    fixture.advance(20);
    int hi = fixture.project().playState().trackState(1).fillAmount();

    fixture.setCvIn(0, -5.f);
    fixture.advance(20);
    int lo = fixture.project().playState().trackState(1).fillAmount();

    expect(hi > lo + 40, "fillAmount tracks the source magnitude");
}

CASE("pattern route selects the rounded index") {
    EngineTestFixture fixture;
    playRoute(fixture, Routing::Target::Pattern, 0);
    fixture.setCvIn(0, 5.f);

    fixture.start();
    fixture.advance(20);
    expectEqual(fixture.project().playState().trackState(0).requestedPattern(), 15, "high source selects pattern 15");
}

CASE("track mask scopes the effect") {
    EngineTestFixture fixture;
    playRoute(fixture, Routing::Target::Mute, 3);
    fixture.setCvIn(0, 5.f);

    fixture.start();
    fixture.advance(20);
    expect(fixture.project().playState().trackState(3).mute(), "masked track muted");
    expect(!fixture.project().playState().trackState(5).mute(), "unmasked track untouched");
}

}
