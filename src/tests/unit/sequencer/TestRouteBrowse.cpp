#include "UnitTest.h"

#include "apps/sequencer/model/RouteBrowse.h"
#include "apps/sequencer/model/RouteResolve.h"
#include "apps/sequencer/model/RouteParamKey.h"
#include "apps/sequencer/model/Project.h"

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
bool hasKey(const uint8_t *keys, int n, uint8_t key) {
    for (int i = 0; i < n; ++i) {
        if (keys[i] == key) return true;
    }
    return false;
}
}

UNIT_TEST("RouteBrowse") {

CASE("bandParams: PITCH lists pitch keys plus folded SlideTime + Rotate") {
    uint8_t keys[8];
    int n = RouteBrowse::bandParams(Band::Pitch, keys, 8);
    expectEqual(n, 6, "6 pitch-band params");
    expectEqual(int(keys[0]), int(ParamKey::Scale), "Scale");
    expectEqual(int(keys[3]), int(ParamKey::Octave), "Octave");
    expectEqual(int(keys[4]), int(ParamKey::SlideTime), "SlideTime folded in");
    expectEqual(int(keys[5]), int(ParamKey::Rotate), "Rotate folded in");
}

CASE("bandParams: CLOCK folds in Run + Reset") {
    uint8_t keys[8];
    int n = RouteBrowse::bandParams(Band::Clock, keys, 8);
    expectEqual(n, 4, "Divisor, ClockMult, Run, Reset");
    expectTrue(hasKey(keys, n, ParamKey::Run), "Run folded into Clock");
    expectTrue(hasKey(keys, n, ParamKey::Reset), "Reset folded into Clock");
}

CASE("bandParams: GLOBAL folds in CV/Gate output rotate") {
    uint8_t keys[8];
    int g = RouteBrowse::bandParams(Band::Global, keys, 8);
    expectEqual(g, 6, "Tempo/Swing/CVRscan/CVRroute + CvOutRot/GateOutRot");
    expectEqual(int(keys[0]), int(ParamKey::Tempo), "Tempo first");
    expectTrue(hasKey(keys, g, ParamKey::CvOutputRotate), "CvOutputRotate folded into Global");
    expectTrue(hasKey(keys, g, ParamKey::GateOutputRotate), "GateOutputRotate folded into Global");
}

CASE("paramKeyToTarget: the folded shell keys map to their targets") {
    expectEqual(int(RouteBrowse::paramKeyToTarget(ParamKey::Run)), int(Routing::Target::Run), "Run");
    expectEqual(int(RouteBrowse::paramKeyToTarget(ParamKey::Reset)), int(Routing::Target::Reset), "Reset");
    expectEqual(int(RouteBrowse::paramKeyToTarget(ParamKey::CvOutputRotate)), int(Routing::Target::CvOutputRotate), "CvOutRot");
    expectEqual(int(RouteBrowse::paramKeyToTarget(ParamKey::GateOutputRotate)), int(Routing::Target::GateOutputRotate), "GateOutRot");
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
            expectEqual(int(RouteResolve::targetToParamKey(t)), int(keys[i]), "round-trips key");
        }
    }
}

CASE("paramKeyToTarget: unknown key -> None") {
    expectEqual(int(RouteBrowse::paramKeyToTarget(ParamKey::None)), int(Routing::Target::None), "None");
    expectEqual(int(RouteBrowse::paramKeyToTarget(200)), int(Routing::Target::None), "out of range");
}

CASE("sourceList: CV-domain sources plus MIDI for a non-bus target") {
    Routing::Source out[40];
    int n = RouteBrowse::sourceList(Routing::Target::Transpose, out, 40);
    expectEqual(n, 34, "None + 4 CvIn + 8 CvOut + 4 Bus + MIDI + 8 GateOut + 8 Mod");
    expectEqual(int(out[0]), int(Routing::Source::None), "None first");
    expectEqual(int(out[1]), int(Routing::Source::CvIn1), "CvIn1 second");
    expectEqual(int(out[17]), int(Routing::Source::Midi), "MIDI after the 4 bus lanes");
    expectEqual(int(out[n - 1]), int(Routing::Source::Mod8), "Mod8 last");
    bool hasMidi = false;
    for (int i = 0; i < n; ++i) {
        if (out[i] == Routing::Source::Midi) hasMidi = true;
    }
    expectTrue(hasMidi, "MIDI entry present");
}

CASE("sourceList: excludes the self-route bus for a bus target") {
    Routing::Source out[40];
    int n = RouteBrowse::sourceList(Routing::Target::BusCv2, out, 40);
    expectEqual(n, 33, "34 minus BusCv2 self-route");
    bool hasBus2 = false, hasBus1 = false, hasBus3 = false;
    for (int i = 0; i < n; ++i) {
        if (out[i] == Routing::Source::BusCv2) hasBus2 = true;
        if (out[i] == Routing::Source::BusCv1) hasBus1 = true;
        if (out[i] == Routing::Source::BusCv3) hasBus3 = true;
    }
    expectFalse(hasBus2, "BusCv2 excluded (self-route)");
    expectTrue(hasBus1, "BusCv1 kept");
    expectTrue(hasBus3, "BusCv3 kept");
}

CASE("sourceList: honors maxOut") {
    Routing::Source out[5];
    int n = RouteBrowse::sourceList(Routing::Target::Transpose, out, 5);
    expectEqual(n, 5, "clamped to maxOut");
    expectEqual(int(out[0]), int(Routing::Source::None), "still in enum order");
}

CASE("enginePageParams: engine table minus the shared Pitch/Clock/SlideTime keys") {
    uint8_t keys[32];
    int n = RouteBrowse::enginePageParams(Track::TrackMode::Note, keys, 32);
    expectEqual(n, 7, "Note: 15 table rows minus 8 shared");
    expectTrue(hasKey(keys, n, ParamKey::GateProbabilityBias), "Gate bias kept (folded from Prob)");
    expectTrue(hasKey(keys, n, ParamKey::RunMode), "Run Mode kept");
    expectFalse(hasKey(keys, n, ParamKey::Scale), "Scale excluded (Pitch band)");
    expectFalse(hasKey(keys, n, ParamKey::Rotate), "Rotate excluded (folded to Pitch band)");
    expectFalse(hasKey(keys, n, ParamKey::Divisor), "Divisor excluded (Clock band)");
    expectFalse(hasKey(keys, n, ParamKey::SlideTime), "SlideTime excluded (Pitch band)");
}

CASE("enginePageParams: Curve is the overflow case, keeps chaos + phase") {
    uint8_t keys[32];
    int n = RouteBrowse::enginePageParams(Track::TrackMode::Curve, keys, 32);
    expectEqual(n, 15, "Curve: 19 table rows minus 4 shared (SlideTime/Rotate/Divisor/ClockMult)");
    expectTrue(hasKey(keys, n, ParamKey::ChaosParam2), "ChaosParam2 kept");
    expectTrue(hasKey(keys, n, ParamKey::Phase), "Phase kept");
    expectFalse(hasKey(keys, n, ParamKey::Rotate), "Rotate excluded (folded to Pitch band)");
    expectFalse(hasKey(keys, n, ParamKey::ClockMultiplier), "ClockMult excluded (Clock band)");
}

CASE("enginePageParams: Stochastic engine page = the 9 curated targets") {
    uint8_t keys[32];
    int n = RouteBrowse::enginePageParams(Track::TrackMode::Stochastic, keys, 32);
    expectEqual(n, 9, "Stochastic: 17 table rows minus 8 shared");
    expectTrue(hasKey(keys, n, ParamKey::Range), "Range kept");
    expectTrue(hasKey(keys, n, ParamKey::MarblesBias), "Bias kept");
    expectTrue(hasKey(keys, n, ParamKey::BurstRate), "BurstRate kept");
    expectFalse(hasKey(keys, n, ParamKey::Octave), "Octave excluded (Pitch band)");
    expectFalse(hasKey(keys, n, ParamKey::Rotate), "Rotate excluded (Pitch band)");
}

CASE("enginePageParams: MidiCv has no engine-page params (both are shared)") {
    uint8_t keys[32];
    int n = RouteBrowse::enginePageParams(Track::TrackMode::MidiCv, keys, 32);
    expectEqual(n, 0, "MidiCv: SlideTime + Transpose are both shared -> empty page");
}

CASE("midiRouteList: empty routing yields no rows") {
    Project project;
    auto &routing = project.routing();
    uint8_t idx[CONFIG_ROUTE_COUNT];
    expectEqual(RouteBrowse::midiRouteList(routing, idx, CONFIG_ROUTE_COUNT), 0, "no MIDI routes");
}

CASE("midiRouteList: collects only active MIDI-sourced routes, ascending") {
    Project project;
    auto &routing = project.routing();
    routing.route(2).setTarget(Routing::Target::Transpose);
    routing.route(2).setSource(Routing::Source::Midi);
    routing.route(3).setTarget(Routing::Target::Octave);
    routing.route(3).setSource(Routing::Source::CvIn1);     // not MIDI
    routing.route(5).setTarget(Routing::Target::Tempo);
    routing.route(5).setSource(Routing::Source::Midi);
    // a route with MIDI source but no target stays inactive -> excluded
    routing.route(7).setSource(Routing::Source::Midi);

    uint8_t idx[CONFIG_ROUTE_COUNT];
    int n = RouteBrowse::midiRouteList(routing, idx, CONFIG_ROUTE_COUNT);
    expectEqual(n, 2, "two active MIDI routes");
    expectEqual(int(idx[0]), 2, "route 2 first");
    expectEqual(int(idx[1]), 5, "route 5 second");
}

CASE("midiRouteList: honors maxOut") {
    Project project;
    auto &routing = project.routing();
    routing.route(0).setTarget(Routing::Target::Transpose);
    routing.route(0).setSource(Routing::Source::Midi);
    routing.route(1).setTarget(Routing::Target::Tempo);
    routing.route(1).setSource(Routing::Source::Midi);
    uint8_t idx[1];
    expectEqual(RouteBrowse::midiRouteList(routing, idx, 1), 1, "clamped to maxOut");
    expectEqual(int(idx[0]), 0, "first MIDI route");
}

} // UNIT_TEST
