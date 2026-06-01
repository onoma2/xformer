#include "UnitTest.h"

#include "model/Project.h"
#include "model/ParamTableNote.h"
#include "model/RouteParamKey.h"

// U3 of the routing mod-matrix overhaul: the Note per-type param table.
// Characterization: each Note row's applyRouted hook must mutate the Note track
// identically to the live Routing::writeTarget path for the same normalized
// input. Track scope, scope.object = Track* (Note mode). Built alongside the old
// dispatch -- no engine cutover (that is U7).

namespace {

void makeNoteTrack0(Project &p) {
    p.clear();
    p.setTrackMode(0, Track::TrackMode::Curve);   // force a clean container init
    p.setTrackMode(0, Track::TrackMode::Note);
}

Project &noteProject() {
    static Project p;
    makeNoteTrack0(p);
    return p;
}

} // namespace

UNIT_TEST("ParamTableNote") {

CASE("rows match writeTarget for every normalized input") {
    struct Spec {
        Routing::Target target;
        uint8_t key;
        bool sequence;                 // sequence-level (fan-out 16 patterns) vs track-level
        int (*read)(Project &);        // routed value through the gated getter
        const char *name;
    };
    const Spec specs[] = {
        // track-level
        { Routing::Target::Octave,    ParamKey::Octave,    false, [](Project &p){ return p.track(0).noteTrack().octave(); },    "Octave" },
        { Routing::Target::Transpose, ParamKey::Transpose, false, [](Project &p){ return p.track(0).noteTrack().transpose(); }, "Transpose" },
        { Routing::Target::SlideTime, ParamKey::SlideTime, false, [](Project &p){ return p.track(0).noteTrack().slideTime(); }, "SlideTime" },
        { Routing::Target::Rotate,    ParamKey::Rotate,    false, [](Project &p){ return p.track(0).noteTrack().rotate(); },    "Rotate" },
        { Routing::Target::GateProbabilityBias,      ParamKey::GateProbabilityBias,      false, [](Project &p){ return p.track(0).noteTrack().gateProbabilityBias(); },      "GateProbBias" },
        { Routing::Target::RetriggerProbabilityBias, ParamKey::RetriggerProbabilityBias, false, [](Project &p){ return p.track(0).noteTrack().retriggerProbabilityBias(); }, "RetrigProbBias" },
        { Routing::Target::LengthBias,               ParamKey::LengthBias,               false, [](Project &p){ return p.track(0).noteTrack().lengthBias(); },               "LengthBias" },
        { Routing::Target::NoteProbabilityBias,      ParamKey::NoteProbabilityBias,      false, [](Project &p){ return p.track(0).noteTrack().noteProbabilityBias(); },      "NoteProbBias" },
        // sequence-level (read pattern 0 + pattern 15 checked separately below)
        { Routing::Target::Scale,     ParamKey::Scale,     true,  [](Project &p){ return p.track(0).noteTrack().sequence(0).scale(); },     "Scale" },
        { Routing::Target::RootNote,  ParamKey::RootNote,  true,  [](Project &p){ return p.track(0).noteTrack().sequence(0).rootNote(); },  "RootNote" },
        { Routing::Target::Divisor,   ParamKey::Divisor,   true,  [](Project &p){ return p.track(0).noteTrack().sequence(0).divisor(); },   "Divisor" },
        { Routing::Target::ClockMult, ParamKey::ClockMultiplier, true,  [](Project &p){ return p.track(0).noteTrack().sequence(0).clockMultiplier(); }, "ClockMult" },
        { Routing::Target::RunMode,   ParamKey::RunMode,   true,  [](Project &p){ return int(p.track(0).noteTrack().sequence(0).runMode()); }, "RunMode" },
        { Routing::Target::FirstStep, ParamKey::FirstStep, true,  [](Project &p){ return p.track(0).noteTrack().sequence(0).firstStep(); }, "FirstStep" },
        { Routing::Target::LastStep,  ParamKey::LastStep,  true,  [](Project &p){ return p.track(0).noteTrack().sequence(0).lastStep(); },  "LastStep" },
    };
    const float inputs[] = { 0.f, 0.25f, 0.5f, 0.75f, 1.f };

    for (const auto &s : specs) {
        Routing::setRouted(s.target, 0xFF, true);   // expose routed slot through getters

        for (float n : inputs) {
            Project &oldP = noteProject();
            oldP.routing().writeTarget(s.target, 1 << 0, n);

            static Project newP;
            makeNoteTrack0(newP);
            RouteParam::Scope scope;
            scope.kind = RouteParam::Scope::Kind::Track;
            scope.object = &newP.track(0);
            scope.trackIndex = 0;
            bool applied = NoteParamTable::table().applyRouted(scope, s.key, n);
            expectTrue(applied, s.name);
            expectEqual(s.read(newP), s.read(oldP), s.name);
        }
    }
}

CASE("sequence rows fan out to every pattern") {
    Routing::setRouted(Routing::Target::Divisor, 0xFF, true);
    Project &p = noteProject();
    RouteParam::Scope scope;
    scope.kind = RouteParam::Scope::Kind::Track;
    scope.object = &p.track(0);
    NoteParamTable::table().applyRouted(scope, ParamKey::Divisor, 0.5f);

    int d0 = p.track(0).noteTrack().sequence(0).divisor();
    int d15 = p.track(0).noteTrack().sequence(CONFIG_PATTERN_COUNT - 1).divisor();
    expectEqual(d15, d0, "all patterns received the routed divisor");
}

} // UNIT_TEST
