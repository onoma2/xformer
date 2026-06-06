#include "ParamTableTuesday.h"
#include "RouteParamKey.h"

namespace {

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    { ParamKey::Algorithm,       "Algorithm",  {   0.f,  14.f }, RouteParam::Discrete },
    { ParamKey::Flow,            "Flow",       {   0.f,  16.f }, RouteParam::Discrete },
    { ParamKey::Ornament,        "Ornament",   {   0.f,  16.f }, RouteParam::Discrete },
    { ParamKey::Power,           "Power",      {   0.f,  16.f }, RouteParam::Discrete },
    { ParamKey::Glide,           "Glide",      {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Trill,           "Trill",      {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::StepTrill,       "Step Trill", {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::GateLength,      "Gate Length",{   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::GateOffset,      "Gate Offset",{   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Octave,          "Octave",     { -10.f,  10.f }, RouteParam::Continuous },
    { ParamKey::Transpose,       "Transpose",  { -11.f,  11.f }, RouteParam::Continuous },
    { ParamKey::Rotate,          "Rotate",     { -64.f,  64.f }, RouteParam::Continuous },
    { ParamKey::Divisor,         "Divisor",    {   1.f, 768.f }, RouteParam::Discrete },
    { ParamKey::ClockMultiplier, "Clock Mult", {  50.f, 150.f }, RouteParam::Continuous },
    { ParamKey::Scale,           "Scale",      {   0.f,  23.f }, RouteParam::Discrete },
    { ParamKey::RootNote,        "Root Note",  {   0.f,  11.f }, RouteParam::Discrete },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &TuesdayParamTable::table() {
    return kTable;
}
