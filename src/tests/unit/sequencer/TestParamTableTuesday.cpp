#include "UnitTest.h"

#include "model/Project.h"
#include "model/ParamTableTuesday.h"
#include "model/RouteParamKey.h"

// U4 of the routing mod-matrix overhaul: the Tuesday per-type param table.
// Characterization: each row's applyRouted hook must mutate the Tuesday track
// identically to Routing::writeTarget. Track scope, scope.object = Track* (Tuesday
// mode). All rows are sequence-level, routed slot, pattern fan-out.

namespace {

void makeTuesdayTrack0(Project &p) {
    p.clear();
    p.setTrackMode(0, Track::TrackMode::Note);    // force a clean container init
    p.setTrackMode(0, Track::TrackMode::Tuesday);
}

Project &tuesdayProject() {
    static Project p;
    makeTuesdayTrack0(p);
    return p;
}

} // namespace

UNIT_TEST("ParamTableTuesday") {

CASE("rows match writeTarget for every normalized input") {
    struct Spec {
        Routing::Target target;
        uint8_t key;
        float (*read)(Project &);
        const char *name;
    };
    const Spec specs[] = {
        { Routing::Target::Algorithm, ParamKey::Algorithm,       [](Project &p){ return float(p.track(0).tuesdayTrack().sequence(0).algorithm()); },  "Algorithm" },
        { Routing::Target::Flow,      ParamKey::Flow,            [](Project &p){ return float(p.track(0).tuesdayTrack().sequence(0).flow()); },       "Flow" },
        { Routing::Target::Ornament,  ParamKey::Ornament,        [](Project &p){ return float(p.track(0).tuesdayTrack().sequence(0).ornament()); },   "Ornament" },
        { Routing::Target::Power,     ParamKey::Power,           [](Project &p){ return float(p.track(0).tuesdayTrack().sequence(0).power()); },      "Power" },
        { Routing::Target::Glide,     ParamKey::Glide,           [](Project &p){ return float(p.track(0).tuesdayTrack().sequence(0).glide()); },      "Glide" },
        { Routing::Target::Trill,     ParamKey::Trill,           [](Project &p){ return float(p.track(0).tuesdayTrack().sequence(0).trill()); },      "Trill" },
        { Routing::Target::StepTrill, ParamKey::StepTrill,       [](Project &p){ return float(p.track(0).tuesdayTrack().sequence(0).stepTrill()); },  "StepTrill" },
        { Routing::Target::GateLength,ParamKey::GateLength,      [](Project &p){ return float(p.track(0).tuesdayTrack().sequence(0).gateLength()); }, "GateLength" },
        { Routing::Target::GateOffset,ParamKey::GateOffset,      [](Project &p){ return float(p.track(0).tuesdayTrack().sequence(0).gateOffset()); }, "GateOffset" },
        { Routing::Target::Octave,    ParamKey::Octave,          [](Project &p){ return float(p.track(0).tuesdayTrack().sequence(0).octave()); },     "Octave" },
        { Routing::Target::Transpose, ParamKey::Transpose,       [](Project &p){ return float(p.track(0).tuesdayTrack().sequence(0).transpose()); },  "Transpose" },
        { Routing::Target::Rotate,    ParamKey::Rotate,          [](Project &p){ return float(p.track(0).tuesdayTrack().sequence(0).rotate()); },     "Rotate" },
        { Routing::Target::Divisor,   ParamKey::Divisor,         [](Project &p){ return float(p.track(0).tuesdayTrack().sequence(0).divisor()); },    "Divisor" },
        { Routing::Target::ClockMult, ParamKey::ClockMultiplier, [](Project &p){ return float(p.track(0).tuesdayTrack().sequence(0).clockMultiplier()); }, "ClockMult" },
    };
    const float inputs[] = { 0.f, 0.25f, 0.5f, 0.75f, 1.f };

    for (const auto &s : specs) {
        Routing::setRouted(s.target, 0xFF, true);   // all routed-slot; expose via getters

        for (float n : inputs) {
            Project &oldP = tuesdayProject();
            oldP.routing().writeTarget(s.target, 1 << 0, n);

            static Project newP;
            makeTuesdayTrack0(newP);
            RouteParam::Scope scope;
            scope.kind = RouteParam::Scope::Kind::Track;
            scope.object = &newP.track(0);
            scope.trackIndex = 0;
            bool applied = TuesdayParamTable::table().applyRouted(scope, s.key, n);
            expectTrue(applied, s.name);
            expectEqual(s.read(newP), s.read(oldP), s.name);
        }
    }
}

CASE("sequence rows fan out to every pattern") {
    Routing::setRouted(Routing::Target::Algorithm, 0xFF, true);
    Project &p = tuesdayProject();
    RouteParam::Scope scope;
    scope.kind = RouteParam::Scope::Kind::Track;
    scope.object = &p.track(0);
    TuesdayParamTable::table().applyRouted(scope, ParamKey::Algorithm, 0.5f);

    int a0 = p.track(0).tuesdayTrack().sequence(0).algorithm();
    int a15 = p.track(0).tuesdayTrack().sequence(CONFIG_PATTERN_COUNT - 1).algorithm();
    expectEqual(a15, a0, "all patterns received the routed algorithm");
}

} // UNIT_TEST
