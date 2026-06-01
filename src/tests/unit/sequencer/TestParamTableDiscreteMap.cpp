#include "UnitTest.h"

#include "model/Project.h"
#include "model/ParamTableDiscreteMap.h"
#include "model/RouteParamKey.h"

// U4 of the routing mod-matrix overhaul: the DiscreteMap per-type param table.
// Characterization: each row's applyRouted hook must mutate the DiscreteMap track
// identically to Routing::writeTarget for the same normalized input. Track scope,
// scope.object = Track* (DiscreteMap mode).
//
// Parity: SlideTime(slewTime) and ClockMult write the routed slot; the rest write
// the base (all ungated getters). Inlets Input/Scanner/Sync fill the track-level
// routed-CV fields (raw float, no scaling). Built alongside the old dispatch.

namespace {

void makeDiscreteMapTrack0(Project &p) {
    p.clear();
    p.setTrackMode(0, Track::TrackMode::Note);    // force a clean container init
    p.setTrackMode(0, Track::TrackMode::DiscreteMap);
}

Project &dmProject() {
    static Project p;
    makeDiscreteMapTrack0(p);
    return p;
}

} // namespace

UNIT_TEST("ParamTableDiscreteMap") {

CASE("rows match writeTarget for every normalized input") {
    struct Spec {
        Routing::Target target;
        uint8_t key;
        bool routedWrite;            // routed slot (expose via setRouted) vs base
        float (*read)(Project &);
        const char *name;
    };
    const Spec specs[] = {
        // sequence-level direct
        { Routing::Target::Octave,    ParamKey::Octave,          false, [](Project &p){ return float(p.track(0).discreteMapTrack().sequence(0).octave()); },          "Octave" },
        { Routing::Target::Transpose, ParamKey::Transpose,       false, [](Project &p){ return float(p.track(0).discreteMapTrack().sequence(0).transpose()); },       "Transpose" },
        { Routing::Target::Offset,    ParamKey::Offset,          false, [](Project &p){ return float(p.track(0).discreteMapTrack().sequence(0).offset()); },          "Offset" },
        { Routing::Target::SlideTime, ParamKey::SlideTime,       true,  [](Project &p){ return float(p.track(0).discreteMapTrack().sequence(0).slewTime()); },        "SlideTime" },
        { Routing::Target::Divisor,   ParamKey::Divisor,         false, [](Project &p){ return float(p.track(0).discreteMapTrack().sequence(0).divisor()); },         "Divisor" },
        { Routing::Target::ClockMult, ParamKey::ClockMultiplier, true,  [](Project &p){ return float(p.track(0).discreteMapTrack().sequence(0).clockMultiplier()); }, "ClockMult" },
        { Routing::Target::Scale,     ParamKey::Scale,           false, [](Project &p){ return float(p.track(0).discreteMapTrack().sequence(0).scale()); },           "Scale" },
        { Routing::Target::RootNote,  ParamKey::RootNote,        false, [](Project &p){ return float(p.track(0).discreteMapTrack().sequence(0).rootNote()); },        "RootNote" },
        { Routing::Target::DiscreteMapRangeHigh, ParamKey::DiscreteMapRangeHigh, false, [](Project &p){ return p.track(0).discreteMapTrack().sequence(0).rangeHigh(); }, "RangeHigh" },
        { Routing::Target::DiscreteMapRangeLow,  ParamKey::DiscreteMapRangeLow,  false, [](Project &p){ return p.track(0).discreteMapTrack().sequence(0).rangeLow(); },  "RangeLow" },
        // inlets (track-level routed-CV field)
        { Routing::Target::DiscreteMapInput,   ParamKey::DiscreteMapInput,   false, [](Project &p){ return p.track(0).discreteMapTrack().routedInput(); },   "Input" },
        { Routing::Target::DiscreteMapScanner, ParamKey::DiscreteMapScanner, false, [](Project &p){ return p.track(0).discreteMapTrack().routedScanner(); }, "Scanner" },
        { Routing::Target::DiscreteMapSync,    ParamKey::DiscreteMapSync,    false, [](Project &p){ return p.track(0).discreteMapTrack().routedSync(); },    "Sync" },
    };
    const float inputs[] = { 0.f, 0.25f, 0.5f, 0.75f, 1.f };

    for (const auto &s : specs) {
        Routing::setRouted(s.target, 0xFF, s.routedWrite);   // pin routed flag per spec

        for (float n : inputs) {
            Project &oldP = dmProject();
            oldP.routing().writeTarget(s.target, 1 << 0, n);

            static Project newP;
            makeDiscreteMapTrack0(newP);
            RouteParam::Scope scope;
            scope.kind = RouteParam::Scope::Kind::Track;
            scope.object = &newP.track(0);
            scope.trackIndex = 0;
            bool applied = DiscreteMapParamTable::table().applyRouted(scope, s.key, n);
            expectTrue(applied, s.name);
            expectEqual(s.read(newP), s.read(oldP), s.name);
        }
    }
}

CASE("inlet rows are flagged Inlet, direct rows are not") {
    const RouteParam::Table &t = DiscreteMapParamTable::table();
    expectTrue((t.find(ParamKey::DiscreteMapInput)->flags & RouteParam::Inlet) != 0, "Input is an inlet");
    expectTrue((t.find(ParamKey::DiscreteMapScanner)->flags & RouteParam::Inlet) != 0, "Scanner is an inlet");
    expectTrue((t.find(ParamKey::DiscreteMapSync)->flags & RouteParam::Inlet) != 0, "Sync is an inlet");
    expectTrue((t.find(ParamKey::Octave)->flags & RouteParam::Inlet) == 0, "Octave is not an inlet");
}

CASE("sequence rows fan out to every pattern") {
    Routing::setRouted(Routing::Target::Divisor, 0xFF, false);
    Project &p = dmProject();
    RouteParam::Scope scope;
    scope.kind = RouteParam::Scope::Kind::Track;
    scope.object = &p.track(0);
    DiscreteMapParamTable::table().applyRouted(scope, ParamKey::Divisor, 0.5f);

    int d0 = p.track(0).discreteMapTrack().sequence(0).divisor();
    int d15 = p.track(0).discreteMapTrack().sequence(CONFIG_PATTERN_COUNT - 1).divisor();
    expectEqual(d15, d0, "all patterns received the routed divisor");
}

} // UNIT_TEST
