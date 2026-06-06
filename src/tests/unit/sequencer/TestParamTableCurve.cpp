#include "UnitTest.h"

#include "model/ParamTableCurve.h"
#include "model/RouteParamKey.h"

// U4 of the routing mod-matrix overhaul: the Curve per-type param table.
// Characterization: the table owns the expected keys and resolves each to its
// declared name/flags. Track scope. The apply hooks the table used to carry are
// gone (the override path reads rows by key, never applies via hooks).

UNIT_TEST("ParamTableCurve") {

CASE("table owns the expected keys with declared names and flags") {
    struct Spec { uint8_t key; const char *name; uint8_t flag; };
    const Spec specs[] = {
        { ParamKey::SlideTime,            "Slide Time",    RouteParam::Continuous },
        { ParamKey::Offset,               "Offset",        RouteParam::Continuous },
        { ParamKey::Rotate,               "Rotate",        RouteParam::Continuous },
        { ParamKey::ShapeProbabilityBias, "Shape P. Bias", RouteParam::Continuous },
        { ParamKey::GateProbabilityBias,  "Gate P. Bias",  RouteParam::Continuous },
        { ParamKey::CurveRate,            "Curve Rate",    RouteParam::Continuous },
        { ParamKey::Phase,                "Phase",         RouteParam::Continuous },
        { ParamKey::Divisor,              "Divisor",       RouteParam::Discrete },
        { ParamKey::ClockMultiplier,      "Clock Mult",    RouteParam::Continuous },
        { ParamKey::RunMode,              "Run Mode",      RouteParam::Discrete },
        { ParamKey::FirstStep,            "First Step",    RouteParam::Discrete },
        { ParamKey::LastStep,             "Last Step",     RouteParam::Discrete },
        { ParamKey::WavefolderFold,       "Fold",          RouteParam::Continuous },
        { ParamKey::WavefolderGain,       "Gain",          RouteParam::Continuous },
        { ParamKey::DjFilter,             "DJ Filter",     RouteParam::Continuous },
        { ParamKey::ChaosAmount,          "Chaos Amt",     RouteParam::Continuous },
        { ParamKey::ChaosRate,            "Chaos Rate",    RouteParam::Continuous },
        { ParamKey::ChaosParam1,          "Chaos P1",      RouteParam::Continuous },
        { ParamKey::ChaosParam2,          "Chaos P2",      RouteParam::Continuous },
    };
    const RouteParam::Table &t = CurveParamTable::table();
    for (const auto &s : specs) {
        const RouteParam::Row *row = t.find(s.key);
        expectTrue(row != nullptr, s.name);
        expectEqual(row->name, s.name, s.name);
        expectTrue((row->flags & s.flag) != 0, s.name);
    }
}

CASE("unowned keys do not resolve") {
    const RouteParam::Table &t = CurveParamTable::table();
    expectTrue(t.find(0) == nullptr, "key 0 (None) not owned");
}

} // UNIT_TEST
