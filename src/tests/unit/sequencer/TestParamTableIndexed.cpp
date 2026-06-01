#include "UnitTest.h"

#include "model/Project.h"
#include "model/ParamTableIndexed.h"
#include "model/RouteParamKey.h"

// U4 of the routing mod-matrix overhaul: the Indexed per-type param table, and
// the first table to carry INLET rows. Characterization: each row's applyRouted
// hook must mutate the Indexed track identically to Routing::writeTarget for the
// same normalized input. Track scope, scope.object = Track* (Indexed mode).
//
// Parity quirks: Divisor/Scale/RootNote write the BASE (no routed flag) while the
// rest write the routed slot -- so the spec carries routedWrite to know whether to
// expose the routed slot (setRouted) before reading. Inlets (A/B) fill the
// ungated _routedIndexedA/B field and are read directly. Built alongside the old
// dispatch -- no engine cutover (U7).

namespace {

void makeIndexedTrack0(Project &p) {
    p.clear();
    p.setTrackMode(0, Track::TrackMode::Note);    // force a clean container init
    p.setTrackMode(0, Track::TrackMode::Indexed);
}

Project &indexedProject() {
    static Project p;
    makeIndexedTrack0(p);
    return p;
}

} // namespace

UNIT_TEST("ParamTableIndexed") {

CASE("rows match writeTarget for every normalized input") {
    struct Spec {
        Routing::Target target;
        uint8_t key;
        bool routedWrite;            // writes the routed slot (expose via setRouted) vs base
        float (*read)(Project &);
        const char *name;
    };
    const Spec specs[] = {
        // track-level direct (routed slot)
        { Routing::Target::Octave,    ParamKey::Octave,          true,  [](Project &p){ return float(p.track(0).indexedTrack().octave()); },    "Octave" },
        { Routing::Target::Transpose, ParamKey::Transpose,       true,  [](Project &p){ return float(p.track(0).indexedTrack().transpose()); }, "Transpose" },
        { Routing::Target::SlideTime, ParamKey::SlideTime,       true,  [](Project &p){ return float(p.track(0).indexedTrack().slideTime()); }, "SlideTime" },
        // sequence-level direct
        { Routing::Target::Divisor,   ParamKey::Divisor,         false, [](Project &p){ return float(p.track(0).indexedTrack().sequence(0).divisor()); },         "Divisor" },
        { Routing::Target::ClockMult, ParamKey::ClockMultiplier, true,  [](Project &p){ return float(p.track(0).indexedTrack().sequence(0).clockMultiplier()); }, "ClockMult" },
        { Routing::Target::Scale,     ParamKey::Scale,           false, [](Project &p){ return float(p.track(0).indexedTrack().sequence(0).scale()); },           "Scale" },
        { Routing::Target::RootNote,  ParamKey::RootNote,        false, [](Project &p){ return float(p.track(0).indexedTrack().sequence(0).rootNote()); },        "RootNote" },
        { Routing::Target::FirstStep, ParamKey::FirstStep,       true,  [](Project &p){ return float(p.track(0).indexedTrack().sequence(0).firstStep()); },       "FirstStep" },
        { Routing::Target::RunMode,   ParamKey::RunMode,         true,  [](Project &p){ return float(int(p.track(0).indexedTrack().sequence(0).runMode())); },    "RunMode" },
        // inlets (ungated routed-CV field)
        { Routing::Target::IndexedA,  ParamKey::IndexedA,        false, [](Project &p){ return p.track(0).indexedTrack().sequence(0).routedIndexedA(); },         "IndexedA" },
        { Routing::Target::IndexedB,  ParamKey::IndexedB,        false, [](Project &p){ return p.track(0).indexedTrack().sequence(0).routedIndexedB(); },         "IndexedB" },
    };
    const float inputs[] = { 0.f, 0.25f, 0.5f, 0.75f, 1.f };

    for (const auto &s : specs) {
        if (s.routedWrite) {
            Routing::setRouted(s.target, 0xFF, true);   // expose routed slot through gated getters
        }

        for (float n : inputs) {
            Project &oldP = indexedProject();
            oldP.routing().writeTarget(s.target, 1 << 0, n);

            static Project newP;
            makeIndexedTrack0(newP);
            RouteParam::Scope scope;
            scope.kind = RouteParam::Scope::Kind::Track;
            scope.object = &newP.track(0);
            scope.trackIndex = 0;
            bool applied = IndexedParamTable::table().applyRouted(scope, s.key, n);
            expectTrue(applied, s.name);
            expectEqual(s.read(newP), s.read(oldP), s.name);
        }
    }
}

CASE("inlet rows are flagged Inlet, direct rows are not") {
    const RouteParam::Table &t = IndexedParamTable::table();
    expectTrue((t.find(ParamKey::IndexedA)->flags & RouteParam::Inlet) != 0, "A is an inlet");
    expectTrue((t.find(ParamKey::IndexedB)->flags & RouteParam::Inlet) != 0, "B is an inlet");
    expectTrue((t.find(ParamKey::Octave)->flags & RouteParam::Inlet) == 0, "Octave is not an inlet");
}

CASE("inlet fans out to every pattern") {
    Project &p = indexedProject();
    RouteParam::Scope scope;
    scope.kind = RouteParam::Scope::Kind::Track;
    scope.object = &p.track(0);
    IndexedParamTable::table().applyRouted(scope, ParamKey::IndexedA, 1.f);   // +100 -> +1.0

    float a0 = p.track(0).indexedTrack().sequence(0).routedIndexedA();
    float a15 = p.track(0).indexedTrack().sequence(CONFIG_PATTERN_COUNT - 1).routedIndexedA();
    expectEqual(a0, 1.f, "inlet normalized -100..100 to -1..1");
    expectEqual(a15, a0, "all patterns received the inlet CV");
}

} // UNIT_TEST
