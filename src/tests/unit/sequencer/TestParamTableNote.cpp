#include "UnitTest.h"

#include "model/ParamTableNote.h"
#include "model/RouteParamKey.h"

// U3 of the routing mod-matrix overhaul: the Note per-type param table.
// Characterization: the table owns the expected keys and resolves each to its
// declared name/range/flags. Track scope. The apply hooks the table used to
// carry are gone (the override path reads rows by key, never applies via hooks).

UNIT_TEST("ParamTableNote") {

CASE("table owns the expected keys with declared names and flags") {
    struct Spec { uint8_t key; const char *name; uint8_t flag; };
    const Spec specs[] = {
        { ParamKey::SlideTime, "Slide Time", RouteParam::Continuous },
        { ParamKey::Octave,    "Octave",     RouteParam::Continuous },
        { ParamKey::Transpose, "Transp",     RouteParam::Continuous },
        { ParamKey::Rotate,    "Rotate",     RouteParam::Continuous },
        { ParamKey::GateProbabilityBias,      "Gate PB", RouteParam::Continuous },
        { ParamKey::RetriggerProbabilityBias, "Rtrg PB", RouteParam::Continuous },
        { ParamKey::LengthBias,               "Len Bias", RouteParam::Continuous },
        { ParamKey::NoteProbabilityBias,      "Note PB", RouteParam::Continuous },
        { ParamKey::Scale,     "Scale",       RouteParam::Discrete },
        { ParamKey::RootNote,  "Root N",      RouteParam::Discrete },
        { ParamKey::Divisor,   "Divisor",     RouteParam::Discrete },
        { ParamKey::ClockMultiplier, "Clk Mlt", RouteParam::Continuous },
        { ParamKey::RunMode,   "Run Mode",    RouteParam::Discrete },
        { ParamKey::FirstStep, "First Step",  RouteParam::Discrete },
        { ParamKey::LastStep,  "Last Step",   RouteParam::Discrete },
    };
    const RouteParam::Table &t = NoteParamTable::table();
    for (const auto &s : specs) {
        const RouteParam::Row *row = t.find(s.key);
        expectTrue(row != nullptr, s.name);
        expectEqual(row->name, s.name, s.name);
        expectTrue((row->flags & s.flag) != 0, s.name);
    }
}

CASE("unowned keys do not resolve") {
    const RouteParam::Table &t = NoteParamTable::table();
    expectTrue(t.find(0) == nullptr, "key 0 (None) not owned");
}

} // UNIT_TEST
