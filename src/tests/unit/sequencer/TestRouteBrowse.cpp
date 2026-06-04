#include "UnitTest.h"

#include "apps/sequencer/model/RouteBrowse.h"
#include "apps/sequencer/model/RouteFork.h"
#include "apps/sequencer/model/RouteParamKey.h"

// Param-aggregation read model for the tab editor (lens UI). A tab band maps to a
// fixed list of ParamKeys; matches() resolves whether a given route targets a band
// param within a scope (track mask; mask 0 = global). Pure helpers — the 16-route
// loop that uses them is a thin wrapper tested via the model, not here.

using Band = RouteBrowse::Band;

namespace {
Routing::Route routeFor(Routing::Target target, uint8_t tracks) {
    Routing::Route r;
    r.clear();
    r.setTarget(target);
    r.setTracks(tracks);
    return r;
}
}

UNIT_TEST("RouteBrowse") {

CASE("bandParams: PITCH lists the pitch band keys") {
    uint8_t keys[8];
    int n = RouteBrowse::bandParams(Band::Pitch, keys, 8);
    expectEqual(n, 4, "4 pitch params");
    expectEqual(int(keys[0]), int(ParamKey::Scale), "Scale");
    expectEqual(int(keys[1]), int(ParamKey::RootNote), "RootNote");
    expectEqual(int(keys[2]), int(ParamKey::Transpose), "Transpose");
    expectEqual(int(keys[3]), int(ParamKey::Octave), "Octave");
}

CASE("bandParams: CLOCK / PROB / GLOBAL counts") {
    uint8_t keys[8];
    expectEqual(RouteBrowse::bandParams(Band::Clock, keys, 8), 2, "Divisor, ClockMult");
    expectEqual(RouteBrowse::bandParams(Band::Prob, keys, 8), 4, "4 biases");
    int g = RouteBrowse::bandParams(Band::Global, keys, 8);
    expectEqual(g, 4, "Tempo/Swing/CVRscan/CVRroute");
    expectEqual(int(keys[0]), int(ParamKey::Tempo), "Tempo first");
}

CASE("matches: per-track route matches its paramKey when scope overlaps") {
    auto r = routeFor(Routing::Target::Transpose, 0b00000101); // T1, T3
    expectTrue(RouteBrowse::matches(r, ParamKey::Transpose, 0b00000001), "T1 overlaps");
    expectTrue(RouteBrowse::matches(r, ParamKey::Transpose, 0b00000100), "T3 overlaps");
    expectFalse(RouteBrowse::matches(r, ParamKey::Transpose, 0b00000010), "T2 no overlap");
    expectFalse(RouteBrowse::matches(r, ParamKey::Scale, 0b00000001), "wrong paramKey");
}

CASE("matches: inactive route never matches") {
    Routing::Route r; r.clear(); // target None
    expectFalse(RouteBrowse::matches(r, ParamKey::Transpose, 0b11111111), "cleared route");
}

CASE("matches: global route matches at global scope (mask 0) only") {
    auto r = routeFor(Routing::Target::Tempo, 0); // project target, no tracks
    expectTrue(RouteBrowse::matches(r, ParamKey::Tempo, 0), "global scope");
    expectFalse(RouteBrowse::matches(r, ParamKey::Tempo, 0b00000001), "per-track scope misses global");
    expectFalse(RouteBrowse::matches(r, ParamKey::Swing, 0), "wrong global key");
}

CASE("matches: per-track param does not match at global scope") {
    auto r = routeFor(Routing::Target::Transpose, 0b00000001);
    expectFalse(RouteBrowse::matches(r, ParamKey::Transpose, 0), "per-track route, global scope query");
}

CASE("paramKeyToTarget: round-trips every band param key") {
    for (int b = 0; b < 4; ++b) {
        uint8_t keys[8];
        int n = RouteBrowse::bandParams(RouteBrowse::Band(b), keys, 8);
        for (int i = 0; i < n; ++i) {
            Routing::Target t = RouteBrowse::paramKeyToTarget(keys[i]);
            expectTrue(int(t) != int(Routing::Target::None), "band key maps to a real target");
            expectEqual(int(RouteFork::targetToParamKey(t)), int(keys[i]), "round-trips key");
        }
    }
}

CASE("paramKeyToTarget: unknown key -> None") {
    expectEqual(int(RouteBrowse::paramKeyToTarget(ParamKey::None)), int(Routing::Target::None), "None");
    expectEqual(int(RouteBrowse::paramKeyToTarget(200)), int(Routing::Target::None), "out of range");
}

} // UNIT_TEST
