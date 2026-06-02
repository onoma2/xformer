#include "UnitTest.h"

#include "model/Project.h"
#include "model/ParamTablePhaseFlux.h"
#include "model/RouteParamKey.h"

#include <cmath>

// U4 of the routing mod-matrix overhaul: the PhaseFlux per-type param table.
// Parity rows are characterized against Routing::writeTarget; LAUNCH rows (the
// five nudges + Phase) are new routables with no writeTarget equivalent and are
// tested directly. Track scope, scope.object = Track* (PhaseFlux mode).

namespace {

void makePhaseFluxTrack0(Project &p) {
    p.clear();
    p.setTrackMode(0, Track::TrackMode::Note);    // force a clean container init
    p.setTrackMode(0, Track::TrackMode::PhaseFlux);
}

Project &pfProject() {
    static Project p;
    makePhaseFluxTrack0(p);
    return p;
}

} // namespace

UNIT_TEST("ParamTablePhaseFlux") {

CASE("parity rows match writeTarget for every normalized input") {
    struct Spec {
        Routing::Target target;
        uint8_t key;
        bool routedWrite;
        float (*read)(Project &);
        const char *name;
    };
    const Spec specs[] = {
        { Routing::Target::Octave,    ParamKey::Octave,          true,  [](Project &p){ return float(p.track(0).phaseFluxTrack().octave()); },    "Octave" },
        { Routing::Target::Transpose, ParamKey::Transpose,       true,  [](Project &p){ return float(p.track(0).phaseFluxTrack().transpose()); }, "Transpose" },
        { Routing::Target::SlideTime, ParamKey::SlideTime,       true,  [](Project &p){ return float(p.track(0).phaseFluxTrack().slideTime()); }, "SlideTime" },
        { Routing::Target::Divisor,   ParamKey::Divisor,         true,  [](Project &p){ return float(p.track(0).phaseFluxTrack().sequence(0).divisor()); },         "Divisor" },
        { Routing::Target::ClockMult, ParamKey::ClockMultiplier, true,  [](Project &p){ return float(p.track(0).phaseFluxTrack().sequence(0).clockMultiplier()); }, "ClockMult" },
        { Routing::Target::Scale,     ParamKey::Scale,           false, [](Project &p){ return float(p.track(0).phaseFluxTrack().sequence(0).scale()); },           "Scale" },
        { Routing::Target::RootNote,  ParamKey::RootNote,        false, [](Project &p){ return float(p.track(0).phaseFluxTrack().sequence(0).rootNote()); },        "RootNote" },
    };
    const float inputs[] = { 0.f, 0.25f, 0.5f, 0.75f, 1.f };

    for (const auto &s : specs) {
        Routing::setRouted(s.target, 0xFF, s.routedWrite);

        for (float n : inputs) {
            Project &oldP = pfProject();
            oldP.routing().writeTarget(s.target, 1 << 0, n);

            static Project newP;
            makePhaseFluxTrack0(newP);
            RouteParam::Scope scope;
            scope.kind = RouteParam::Scope::Kind::Track;
            scope.object = &newP.track(0);
            scope.trackIndex = 0;
            bool applied = PhaseFluxParamTable::table().applyRouted(scope, s.key, n);
            expectTrue(applied, s.name);
            expectEqual(s.read(newP), s.read(oldP), s.name);
        }
    }
}

CASE("LAUNCH nudges write the field (new routables, fan-out)") {
    struct Spec {
        uint8_t key;
        float min, max;
        int (*read)(Project &);
        const char *name;
    };
    const Spec specs[] = {
        { ParamKey::WarpNudge,      -64.f, 64.f, [](Project &p){ return p.track(0).phaseFluxTrack().sequence(0).warpNudge(); },      "WarpNudge" },
        { ParamKey::ResponseNudge,  -64.f, 64.f, [](Project &p){ return p.track(0).phaseFluxTrack().sequence(0).responseNudge(); },  "ResponseNudge" },
        { ParamKey::LenNudge,       -64.f, 64.f, [](Project &p){ return p.track(0).phaseFluxTrack().sequence(0).lenNudge(); },       "LenNudge" },
        { ParamKey::CyclePhaseWarp, -64.f, 64.f, [](Project &p){ return p.track(0).phaseFluxTrack().sequence(0).cyclePhaseWarp(); }, "CyclePhaseWarp" },
        { ParamKey::PulseNudge,     -15.f, 15.f, [](Project &p){ return p.track(0).phaseFluxTrack().sequence(0).pulseNudge(); },     "PulseNudge" },
    };
    const float inputs[] = { 0.f, 0.25f, 0.5f, 0.75f, 1.f };

    for (const auto &s : specs) {
        for (float n : inputs) {
            Project p;
            makePhaseFluxTrack0(p);
            RouteParam::Scope scope;
            scope.kind = RouteParam::Scope::Kind::Track;
            scope.object = &p.track(0);
            PhaseFluxParamTable::table().applyRouted(scope, s.key, n);
            int expected = int(std::round(n * (s.max - s.min) + s.min));
            expectEqual(s.read(p), expected, s.name);
        }
    }
}

CASE("Phase writes globalPhase across patterns") {
    Project p;
    makePhaseFluxTrack0(p);
    RouteParam::Scope scope;
    scope.kind = RouteParam::Scope::Kind::Track;
    scope.object = &p.track(0);
    PhaseFluxParamTable::table().applyRouted(scope, ParamKey::Phase, 0.75f);

    expectEqual(p.track(0).phaseFluxTrack().sequence(0).globalPhase(), 0.75f, "pattern 0 globalPhase");
    expectEqual(p.track(0).phaseFluxTrack().sequence(CONFIG_PATTERN_COUNT - 1).globalPhase(), 0.75f, "last pattern globalPhase");
}

} // UNIT_TEST
