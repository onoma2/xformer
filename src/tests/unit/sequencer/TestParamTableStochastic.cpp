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
        { ParamKey::Octave,          "Octave",     RouteParam::Continuous },
        { ParamKey::Transpose,       "Transpose",  RouteParam::Continuous },
        { ParamKey::SlideTime,       "Slide Time", RouteParam::Continuous },
        { ParamKey::Complexity,      "Complexity", RouteParam::Continuous },
        { ParamKey::Variation,       "Variation",  RouteParam::Continuous },
        { ParamKey::Rest,            "Rest",       RouteParam::Continuous },
        { ParamKey::Slide,           "Slide",      RouteParam::Continuous },
        { ParamKey::Burst,           "Burst",      RouteParam::Continuous },
        { ParamKey::Sleep,           "Sleep",      RouteParam::Continuous },
        { ParamKey::Mutate,          "Mutate",     RouteParam::Continuous },
        { ParamKey::Jump,            "Jump",       RouteParam::Continuous },
        { ParamKey::Contour,         "Contour",    RouteParam::Continuous },
        { ParamKey::MaskRhythm,      "Mask Rhy",   RouteParam::Continuous },
        { ParamKey::TiltRhythm,      "Tilt Rhy",   RouteParam::Continuous },
        { ParamKey::StochasticGateLength, "Gate Length", RouteParam::Continuous },
        { ParamKey::PatienceRhythm,  "Patience",   RouteParam::Continuous },
        { ParamKey::NoteDuration,    "Note Dur",   RouteParam::Discrete },
        { ParamKey::Rotate,          "Rotate",     RouteParam::Continuous },
        { ParamKey::ClockMultiplier, "Clock Mult", RouteParam::Continuous },
        { ParamKey::Feel,            "Feel",       RouteParam::Continuous },
        { ParamKey::Scale,           "Scale",      RouteParam::Discrete },
        { ParamKey::RootNote,        "Root Note",  RouteParam::Discrete },
        { ParamKey::Divisor,         "Divisor",    RouteParam::Discrete },
    };
    const RouteParam::Table &t = StochasticParamTable::table();
    for (const auto &s : specs) {
        const RouteParam::Row *row = t.find(s.key);
        expectTrue(row != nullptr, s.name);
        expectEqual(row->name, s.name, s.name);
        expectTrue((row->flags & s.flag) != 0, s.name);
    }
}

CASE("unowned keys do not resolve") {
    const RouteParam::Table &t = StochasticParamTable::table();
    expectTrue(t.find(0) == nullptr, "key 0 (None) not owned");
}

} // UNIT_TEST
