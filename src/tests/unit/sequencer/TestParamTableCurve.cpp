#include "UnitTest.h"

#include "model/Project.h"
#include "model/ParamTableCurve.h"
#include "model/RouteParamKey.h"

// U4 of the routing mod-matrix overhaul: the Curve per-type param table.
// Characterization: each Curve row's applyRouted hook must mutate the Curve
// track identically to the live Routing::writeTarget path for the same normalized
// input. Track scope, scope.object = Track* (Curve mode). Sequence rows fan out
// across patterns. Phase has no writeTarget equivalent (launch addition) and is
// checked directly. Built alongside the old dispatch -- no engine cutover (U7).

namespace {

void makeCurveTrack0(Project &p) {
    p.clear();
    p.setTrackMode(0, Track::TrackMode::Note);    // force a clean container init
    p.setTrackMode(0, Track::TrackMode::Curve);
}

Project &curveProject() {
    static Project p;
    makeCurveTrack0(p);
    return p;
}

} // namespace

UNIT_TEST("ParamTableCurve") {

CASE("rows match writeTarget for every normalized input") {
    struct Spec {
        Routing::Target target;
        uint8_t key;
        float (*read)(Project &);
        const char *name;
    };
    const Spec specs[] = {
        // track-level
        { Routing::Target::SlideTime,            ParamKey::SlideTime,            [](Project &p){ return float(p.track(0).curveTrack().slideTime()); },            "SlideTime" },
        { Routing::Target::Offset,               ParamKey::Offset,               [](Project &p){ return float(p.track(0).curveTrack().offset()); },               "Offset" },
        { Routing::Target::Rotate,               ParamKey::Rotate,               [](Project &p){ return float(p.track(0).curveTrack().rotate()); },               "Rotate" },
        { Routing::Target::ShapeProbabilityBias, ParamKey::ShapeProbabilityBias, [](Project &p){ return float(p.track(0).curveTrack().shapeProbabilityBias()); }, "ShapeBias" },
        { Routing::Target::GateProbabilityBias,  ParamKey::GateProbabilityBias,  [](Project &p){ return float(p.track(0).curveTrack().gateProbabilityBias()); },  "GateBias" },
        { Routing::Target::CurveRate,            ParamKey::CurveRate,            [](Project &p){ return p.track(0).curveTrack().curveRate(); },                    "CurveRate" },
        // sequence-level (fan-out; read pattern 0)
        { Routing::Target::Divisor,              ParamKey::Divisor,              [](Project &p){ return float(p.track(0).curveTrack().sequence(0).divisor()); },         "Divisor" },
        { Routing::Target::ClockMult,            ParamKey::ClockMultiplier,      [](Project &p){ return float(p.track(0).curveTrack().sequence(0).clockMultiplier()); }, "ClockMult" },
        { Routing::Target::RunMode,              ParamKey::RunMode,              [](Project &p){ return float(int(p.track(0).curveTrack().sequence(0).runMode())); },   "RunMode" },
        { Routing::Target::FirstStep,            ParamKey::FirstStep,            [](Project &p){ return float(p.track(0).curveTrack().sequence(0).firstStep()); },       "FirstStep" },
        { Routing::Target::LastStep,             ParamKey::LastStep,             [](Project &p){ return float(p.track(0).curveTrack().sequence(0).lastStep()); },        "LastStep" },
        { Routing::Target::WavefolderFold,       ParamKey::WavefolderFold,       [](Project &p){ return p.track(0).curveTrack().sequence(0).wavefolderFold(); },        "WavefolderFold" },
        { Routing::Target::WavefolderGain,       ParamKey::WavefolderGain,       [](Project &p){ return p.track(0).curveTrack().sequence(0).wavefolderGain(); },        "WavefolderGain" },
        { Routing::Target::DjFilter,             ParamKey::DjFilter,             [](Project &p){ return p.track(0).curveTrack().sequence(0).djFilter(); },              "DjFilter" },
        { Routing::Target::ChaosAmount,          ParamKey::ChaosAmount,          [](Project &p){ return float(p.track(0).curveTrack().sequence(0).chaosAmount()); },     "ChaosAmount" },
        { Routing::Target::ChaosRate,            ParamKey::ChaosRate,            [](Project &p){ return float(p.track(0).curveTrack().sequence(0).chaosRate()); },       "ChaosRate" },
        { Routing::Target::ChaosParam1,          ParamKey::ChaosParam1,          [](Project &p){ return float(p.track(0).curveTrack().sequence(0).chaosParam1()); },     "ChaosParam1" },
        { Routing::Target::ChaosParam2,          ParamKey::ChaosParam2,          [](Project &p){ return float(p.track(0).curveTrack().sequence(0).chaosParam2()); },     "ChaosParam2" },
    };
    const float inputs[] = { 0.f, 0.25f, 0.5f, 0.75f, 1.f };

    for (const auto &s : specs) {
        Routing::setRouted(s.target, 0xFF, true);   // expose routed slot through getters

        for (float n : inputs) {
            Project &oldP = curveProject();
            oldP.routing().writeTarget(s.target, 1 << 0, n);

            static Project newP;
            makeCurveTrack0(newP);
            RouteParam::Scope scope;
            scope.kind = RouteParam::Scope::Kind::Track;
            scope.object = &newP.track(0);
            scope.trackIndex = 0;
            bool applied = CurveParamTable::table().applyRouted(scope, s.key, n);
            expectTrue(applied, s.name);
            expectEqual(s.read(newP), s.read(oldP), s.name);
        }
    }
}

CASE("Phase row writes globalPhase (launch addition, no writeTarget parity)") {
    Project p;
    makeCurveTrack0(p);
    RouteParam::Scope scope;
    scope.kind = RouteParam::Scope::Kind::Track;
    scope.object = &p.track(0);
    bool applied = CurveParamTable::table().applyRouted(scope, ParamKey::Phase, 0.75f);
    expectTrue(applied, "phase applied");
    expectEqual(p.track(0).curveTrack().globalPhase(), 0.75f, "globalPhase set to normalized value");
}

CASE("sequence rows fan out to every pattern") {
    Routing::setRouted(Routing::Target::Divisor, 0xFF, true);
    Project &p = curveProject();
    RouteParam::Scope scope;
    scope.kind = RouteParam::Scope::Kind::Track;
    scope.object = &p.track(0);
    CurveParamTable::table().applyRouted(scope, ParamKey::Divisor, 0.5f);

    int d0 = p.track(0).curveTrack().sequence(0).divisor();
    int d15 = p.track(0).curveTrack().sequence(CONFIG_PATTERN_COUNT - 1).divisor();
    expectEqual(d15, d0, "all patterns received the routed divisor");
}

} // UNIT_TEST
