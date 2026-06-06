#include "UnitTest.h"

#include "model/ParamTableTuesday.h"
#include "model/RouteParamKey.h"

// U4 of the routing mod-matrix overhaul: the Tuesday per-type param table.
// Characterization: the table owns the expected keys and resolves each to its
// declared name/flags. Track scope. The apply hooks the table used to carry are
// gone (the override path reads rows by key, never applies via hooks).

UNIT_TEST("ParamTableTuesday") {

CASE("table owns the expected keys with declared names and flags") {
    struct Spec { uint8_t key; const char *name; uint8_t flag; };
    const Spec specs[] = {
        { ParamKey::Algorithm,       "Algorithm",   RouteParam::Discrete },
        { ParamKey::Flow,            "Flow",        RouteParam::Discrete },
        { ParamKey::Ornament,        "Ornament",    RouteParam::Discrete },
        { ParamKey::Power,           "Power",       RouteParam::Discrete },
        { ParamKey::Glide,           "Glide",       RouteParam::Continuous },
        { ParamKey::Trill,           "Trill",       RouteParam::Continuous },
        { ParamKey::StepTrill,       "Step Trill",  RouteParam::Continuous },
        { ParamKey::GateLength,      "Gate Length", RouteParam::Continuous },
        { ParamKey::GateOffset,      "Gate Offset", RouteParam::Continuous },
        { ParamKey::Octave,          "Octave",      RouteParam::Continuous },
        { ParamKey::Transpose,       "Transpose",   RouteParam::Continuous },
        { ParamKey::Rotate,          "Rotate",      RouteParam::Continuous },
        { ParamKey::Divisor,         "Divisor",     RouteParam::Discrete },
        { ParamKey::ClockMultiplier, "Clock Mult",  RouteParam::Continuous },
        { ParamKey::Scale,           "Scale",       RouteParam::Discrete },
        { ParamKey::RootNote,        "Root Note",   RouteParam::Discrete },
    };
    const RouteParam::Table &t = TuesdayParamTable::table();
    for (const auto &s : specs) {
        const RouteParam::Row *row = t.find(s.key);
        expectTrue(row != nullptr, s.name);
        expectEqual(row->name, s.name, s.name);
        expectTrue((row->flags & s.flag) != 0, s.name);
    }
}

CASE("unowned keys do not resolve") {
    const RouteParam::Table &t = TuesdayParamTable::table();
    expectTrue(t.find(0) == nullptr, "key 0 (None) not owned");
}

} // UNIT_TEST
