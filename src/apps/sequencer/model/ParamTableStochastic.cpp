#include "ParamTableStochastic.h"
#include "RouteParamKey.h"

namespace {

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    // track-level
    { ParamKey::Octave,          "Octave",     { -10.f,  10.f }, RouteParam::Continuous },
    { ParamKey::Transpose,       "Transpose",  { -60.f,  60.f }, RouteParam::Continuous },
    { ParamKey::SlideTime,       "Slide Time", {   0.f, 100.f }, RouteParam::Continuous },
    // sequence-level routed
    { ParamKey::Complexity,      "Complexity", {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Variation,       "Variation",  { -100.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Rest,            "Rest",       {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Slide,           "Slide",      {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Burst,           "Burst",      {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Sleep,           "Sleep",      {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Mutate,          "Mutate",     {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Jump,            "Jump",       {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Contour,         "Contour",    { -100.f, 100.f }, RouteParam::Continuous },
    { ParamKey::MaskRhythm,      "Mask Rhy",   {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::TiltRhythm,      "Tilt Rhy",   { -100.f, 100.f }, RouteParam::Continuous },
    { ParamKey::StochasticGateLength, "Gate Length", { 0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::PatienceRhythm,  "Patience",   {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::NoteDuration,    "Note Dur",   {   0.f,   7.f }, RouteParam::Discrete },
    { ParamKey::Rotate,          "Rotate",     { -64.f,  64.f }, RouteParam::Continuous },
    { ParamKey::ClockMultiplier, "Clock Mult", {  50.f, 150.f }, RouteParam::Continuous },
    { ParamKey::Feel,            "Feel",       {   0.f, 100.f }, RouteParam::Continuous },
    // sequence-level base write (defect-parity; closes at U6b)
    { ParamKey::Scale,           "Scale",      {   0.f,  23.f }, RouteParam::Discrete },
    { ParamKey::RootNote,        "Root Note",  {   0.f,  11.f }, RouteParam::Discrete },
    { ParamKey::Divisor,         "Divisor",    {   1.f, 768.f }, RouteParam::Discrete },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &StochasticParamTable::table() {
    return kTable;
}
