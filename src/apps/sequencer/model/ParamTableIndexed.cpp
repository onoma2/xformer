#include "ParamTableIndexed.h"
#include "RouteParamKey.h"

namespace {

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    // track-level direct
    { ParamKey::Octave,          "Octave",     { -10.f,  10.f }, RouteParam::Continuous },
    { ParamKey::Transpose,       "Transpose",  { -60.f,  60.f }, RouteParam::Continuous },
    { ParamKey::SlideTime,       "Slide Time", {   0.f, 100.f }, RouteParam::Continuous },
    // sequence-level direct (fan-out)
    { ParamKey::Divisor,         "Divisor",    {   1.f, 768.f }, RouteParam::Discrete },
    { ParamKey::ClockMultiplier, "Clock Mult", {  50.f, 150.f }, RouteParam::Continuous },
    { ParamKey::Scale,           "Scale",      {   0.f,  23.f }, RouteParam::Discrete },
    { ParamKey::RootNote,        "Root Note",  {   0.f,  11.f }, RouteParam::Discrete },
    { ParamKey::FirstStep,       "First Step", {   0.f,  63.f }, RouteParam::Discrete },
    { ParamKey::RunMode,         "Run Mode",   {   0.f,   5.f }, RouteParam::Discrete },
    // inlets
    { ParamKey::IndexedA,        "Inlet A",    { -100.f, 100.f }, RouteParam::Inlet },
    { ParamKey::IndexedB,        "Inlet B",    { -100.f, 100.f }, RouteParam::Inlet },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &IndexedParamTable::table() {
    return kTable;
}
