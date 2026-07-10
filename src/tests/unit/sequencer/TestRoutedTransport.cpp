#include "UnitTest.h"

#include "EngineTestFixture.h"

#include "apps/sequencer/model/Routing.h"
#include "apps/sequencer/model/RouteResolve.h"
#include "apps/sequencer/model/RouteBrowse.h"
#include "apps/sequencer/model/RouteParamKey.h"

// Global transport (Play/PlayToggle/Record/RecordToggle/TapTempo) are engine
// targets consumed by writeEngineTarget: Play/Record level-following, the toggles
// + TapTempo rising-edge. The engine side was already built; this restores the UI
// path (ParamKeys + Clock band + tab-editor eligibility) so routes can be created.
namespace {
void transportRoute(EngineTestFixture &f, Routing::Target target) {
    auto &route = f.project().routing().route(0);
    route.setSource(Routing::Source::CvIn1);
    route.setTarget(target);
}
} // namespace

UNIT_TEST("RoutedTransport") {

CASE("every transport target round-trips through a ParamKey") {
    const Routing::Target targets[] = { Routing::Target::Play, Routing::Target::PlayToggle,
                                        Routing::Target::Record, Routing::Target::RecordToggle,
                                        Routing::Target::TapTempo };
    for (auto t : targets) {
        uint8_t key = RouteResolve::targetToParamKey(t);
        expect(key != ParamKey::None, "target maps to a paramKey");
        expectEqual(int(RouteBrowse::paramKeyToTarget(key)), int(t), "paramKey maps back to the target");
    }
}

CASE("the Clock band lists the transport targets") {
    uint8_t keys[16];
    int n = RouteBrowse::bandParams(RouteBrowse::Band::Clock, keys, 16);
    auto has = [&](uint8_t k) { for (int i = 0; i < n; ++i) if (keys[i] == k) return true; return false; };
    expect(has(ParamKey::Play), "Play in Clock band");
    expect(has(ParamKey::PlayToggle), "PlayToggle in Clock band");
    expect(has(ParamKey::Record), "Record in Clock band");
    expect(has(ParamKey::RecordToggle), "RecordToggle in Clock band");
    expect(has(ParamKey::TapTempo), "TapTempo in Clock band");
}

CASE("play route follows the source level") {
    EngineTestFixture fixture;
    transportRoute(fixture, Routing::Target::Play);
    fixture.setCvIn(0, 5.f);
    fixture.advance(20);
    expect(fixture.engine().clockRunning(), "high source starts transport");
    fixture.setCvIn(0, -5.f);
    fixture.advance(20);
    expect(!fixture.engine().clockRunning(), "low source stops transport");
}

CASE("record route follows the source level") {
    EngineTestFixture fixture;
    transportRoute(fixture, Routing::Target::Record);
    fixture.setCvIn(0, 5.f);
    fixture.advance(20);
    expect(fixture.engine().recording(), "high source arms recording");
    fixture.setCvIn(0, -5.f);
    fixture.advance(20);
    expect(!fixture.engine().recording(), "low source disarms recording");
}

CASE("play-toggle route toggles transport on a rising edge") {
    EngineTestFixture fixture;
    transportRoute(fixture, Routing::Target::PlayToggle);
    fixture.setCvIn(0, -5.f);
    fixture.advance(20);
    bool before = fixture.engine().clockRunning();
    fixture.setCvIn(0, 5.f);       // rising edge
    fixture.advance(20);
    expect(fixture.engine().clockRunning() != before, "rising edge toggles transport");
}

}
