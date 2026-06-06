#include "UnitTest.h"

#include "model/ParamTablePhaseFlux.h"
#include "model/RouteParamKey.h"

// U4 of the routing mod-matrix overhaul: the PhaseFlux per-type param table.
// Characterization: the table owns the expected keys (parity rows plus the five
// LAUNCH nudges and Phase) and resolves each to its declared name/flags. Track
// scope. The apply hooks the table used to carry are gone (the override path
// reads rows by key, never applies via hooks).

UNIT_TEST("ParamTablePhaseFlux") {

CASE("table owns the expected keys with declared names and flags") {
    struct Spec { uint8_t key; const char *name; uint8_t flag; };
    const Spec specs[] = {
        { ParamKey::Octave,          "Octave",      RouteParam::Continuous },
        { ParamKey::Transpose,       "Transpose",   RouteParam::Continuous },
        { ParamKey::SlideTime,       "Slide Time",  RouteParam::Continuous },
        { ParamKey::Divisor,         "Divisor",     RouteParam::Discrete },
        { ParamKey::ClockMultiplier, "Clock Mult",  RouteParam::Continuous },
        { ParamKey::Scale,           "Scale",       RouteParam::Discrete },
        { ParamKey::RootNote,        "Root Note",   RouteParam::Discrete },
        { ParamKey::Phase,           "Phase",       RouteParam::Continuous },
        { ParamKey::WarpNudge,       "Warp Nudge",  RouteParam::Continuous },
        { ParamKey::ResponseNudge,   "Resp Nudge",  RouteParam::Continuous },
        { ParamKey::LenNudge,        "Len Nudge",   RouteParam::Continuous },
        { ParamKey::CyclePhaseWarp,  "Cyc Warp",    RouteParam::Continuous },
        { ParamKey::PulseNudge,      "Pulse Nudge", RouteParam::Continuous },
    };
    const RouteParam::Table &t = PhaseFluxParamTable::table();
    for (const auto &s : specs) {
        const RouteParam::Row *row = t.find(s.key);
        expectTrue(row != nullptr, s.name);
        expectEqual(row->name, s.name, s.name);
        expectTrue((row->flags & s.flag) != 0, s.name);
    }
}

CASE("unowned keys do not resolve") {
    const RouteParam::Table &t = PhaseFluxParamTable::table();
    expectTrue(t.find(0) == nullptr, "key 0 (None) not owned");
}

} // UNIT_TEST
