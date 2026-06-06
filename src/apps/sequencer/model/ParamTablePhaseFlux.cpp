#include "ParamTablePhaseFlux.h"
#include "RouteParamKey.h"

namespace {

// Ranges mirror Routing::targetInfos (parity rows) / the field clamp (LAUNCH rows).
constexpr RouteParam::Row kRows[] = {
    // parity: track-level
    { ParamKey::Octave,          "Octave",     { -10.f,  10.f }, RouteParam::Continuous },
    { ParamKey::Transpose,       "Transpose",  { -60.f,  60.f }, RouteParam::Continuous },
    { ParamKey::SlideTime,       "Slide Time", {   0.f, 100.f }, RouteParam::Continuous },
    // parity: sequence-level routed
    { ParamKey::Divisor,         "Divisor",    {   1.f, 768.f }, RouteParam::Discrete },
    { ParamKey::ClockMultiplier, "Clock Mult", {  50.f, 150.f }, RouteParam::Continuous },
    // parity: sequence-level base write
    { ParamKey::Scale,           "Scale",      {   0.f,  23.f }, RouteParam::Discrete },
    { ParamKey::RootNote,        "Root Note",  {   0.f,  11.f }, RouteParam::Discrete },
    // LAUNCH: new routables
    { ParamKey::Phase,           "Phase",      {   0.f,   1.f }, RouteParam::Continuous },
    { ParamKey::WarpNudge,       "Warp Nudge", { -64.f,  64.f }, RouteParam::Continuous },
    { ParamKey::ResponseNudge,   "Resp Nudge", { -64.f,  64.f }, RouteParam::Continuous },
    { ParamKey::LenNudge,        "Len Nudge",  { -64.f,  64.f }, RouteParam::Continuous },
    { ParamKey::CyclePhaseWarp,  "Cyc Warp",   { -64.f,  64.f }, RouteParam::Continuous },
    { ParamKey::PulseNudge,      "Pulse Nudge",{ -15.f,  15.f }, RouteParam::Continuous },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &PhaseFluxParamTable::table() {
    return kTable;
}
