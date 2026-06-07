#include "UnitTest.h"

#include "model/ParamTableStochastic.h"
#include "model/RouteParamKey.h"

// U4 of the routing mod-matrix overhaul: the Stochastic per-type param table.
// Characterization: the table owns the expected keys and resolves each to its
// declared name/flags. Track scope. The apply hooks the table used to carry are
// gone (the override path reads rows by key, never applies via hooks).

UNIT_TEST("ParamTableStochastic") {

CASE("table owns the expected keys with declared names and flags") {
    struct Spec { uint8_t key; const char *name; uint8_t flag; };
    const Spec specs[] = {
        // shared band keys
        { ParamKey::Octave,          "Octave",     RouteParam::Continuous },
        { ParamKey::Transpose,       "Transpose",  RouteParam::Continuous },
        { ParamKey::SlideTime,       "Slide Time", RouteParam::Continuous },
        { ParamKey::Rotate,          "Rotate",     RouteParam::Continuous },
        { ParamKey::Scale,           "Scale",      RouteParam::Discrete },
        { ParamKey::RootNote,        "Root Note",  RouteParam::Discrete },
        { ParamKey::Divisor,         "Divisor",    RouteParam::Discrete },
        { ParamKey::ClockMultiplier, "Clock Mult", RouteParam::Continuous },
        // engine page — the 9 curated routables (2026-06-08 revamp)
        { ParamKey::Range,           "Range",      RouteParam::Continuous },
        { ParamKey::MarblesBias,     "Bias",       RouteParam::Continuous },
        { ParamKey::MarblesSpread,   "Spread",     RouteParam::Continuous },
        { ParamKey::Variation,       "Variation",  RouteParam::Continuous },
        { ParamKey::Rest,            "Rest",       RouteParam::Continuous },
        { ParamKey::Burst,           "Burst",      RouteParam::Continuous },
        { ParamKey::Contour,         "Contour",    RouteParam::Continuous },
        { ParamKey::BurstCount,      "Burst Cnt",  RouteParam::Continuous },
        { ParamKey::BurstRate,       "Burst Rate", RouteParam::Continuous },
    };
    const RouteParam::Table &t = StochasticParamTable::table();
    for (const auto &s : specs) {
        const RouteParam::Row *row = t.find(s.key);
        expectTrue(row != nullptr, s.name);
        expectEqual(row->name, s.name, s.name);
        expectTrue((row->flags & s.flag) != 0, s.name);
    }
}

CASE("dropped routables no longer resolve") {
    const RouteParam::Table &t = StochasticParamTable::table();
    expectTrue(t.find(ParamKey::Complexity) == nullptr, "Complexity dropped");
    expectTrue(t.find(ParamKey::Mutate) == nullptr, "Mutate dropped");
    expectTrue(t.find(ParamKey::Feel) == nullptr, "Feel dropped");
    expectTrue(t.find(0) == nullptr, "key 0 (None) not owned");
}

} // UNIT_TEST
