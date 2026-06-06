#include "UnitTest.h"

#include "model/ParamTableDiscreteMap.h"
#include "model/RouteParamKey.h"

// U4 of the routing mod-matrix overhaul: the DiscreteMap per-type param table.
// Characterization: the table owns the expected keys, resolves each to its
// declared name/flags, and flags inlet rows distinctly from direct rows. Track
// scope. The apply hooks the table used to carry are gone (the override path
// reads rows by key, never applies via hooks).

UNIT_TEST("ParamTableDiscreteMap") {

CASE("table owns the expected keys with declared names and flags") {
    struct Spec { uint8_t key; const char *name; uint8_t flag; };
    const Spec specs[] = {
        { ParamKey::Octave,          "Octave",     RouteParam::Continuous },
        { ParamKey::Transpose,       "Transpose",  RouteParam::Continuous },
        { ParamKey::Offset,          "Offset",     RouteParam::Continuous },
        { ParamKey::SlideTime,       "Slide Time", RouteParam::Continuous },
        { ParamKey::Divisor,         "Divisor",    RouteParam::Discrete },
        { ParamKey::ClockMultiplier, "Clock Mult", RouteParam::Continuous },
        { ParamKey::Scale,           "Scale",      RouteParam::Discrete },
        { ParamKey::RootNote,        "Root Note",  RouteParam::Discrete },
        { ParamKey::DiscreteMapRangeHigh, "Range Hi", RouteParam::Continuous },
        { ParamKey::DiscreteMapRangeLow,  "Range Lo", RouteParam::Continuous },
        { ParamKey::DiscreteMapInput,   "Inlet In",   RouteParam::Inlet },
        { ParamKey::DiscreteMapScanner, "Inlet Scan", RouteParam::Inlet },
    };
    const RouteParam::Table &t = DiscreteMapParamTable::table();
    for (const auto &s : specs) {
        const RouteParam::Row *row = t.find(s.key);
        expectTrue(row != nullptr, s.name);
        expectEqual(row->name, s.name, s.name);
        expectTrue((row->flags & s.flag) != 0, s.name);
    }
}

CASE("inlet rows are flagged Inlet, direct rows are not") {
    const RouteParam::Table &t = DiscreteMapParamTable::table();
    expectTrue((t.find(ParamKey::DiscreteMapInput)->flags & RouteParam::Inlet) != 0, "Input is an inlet");
    expectTrue((t.find(ParamKey::DiscreteMapScanner)->flags & RouteParam::Inlet) != 0, "Scanner is an inlet");
    expectTrue((t.find(ParamKey::Octave)->flags & RouteParam::Inlet) == 0, "Octave is not an inlet");
}

CASE("unowned keys do not resolve") {
    const RouteParam::Table &t = DiscreteMapParamTable::table();
    expectTrue(t.find(0) == nullptr, "key 0 (None) not owned");
}

} // UNIT_TEST
