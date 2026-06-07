#include "ParamTableStochastic.h"
#include "RouteParamKey.h"

namespace {

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    // shared band keys (reach the Pitch/Clock bands on Stochastic tracks)
    { ParamKey::Octave,          "Octave",     { -10.f,  10.f }, RouteParam::Continuous },
    { ParamKey::Transpose,       "Transpose",  { -60.f,  60.f }, RouteParam::Continuous },
    { ParamKey::SlideTime,       "Slide Time", {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Rotate,          "Rotate",     { -64.f,  64.f }, RouteParam::Continuous },
    { ParamKey::Scale,           "Scale",      {   0.f,  23.f }, RouteParam::Discrete },
    { ParamKey::RootNote,        "Root Note",  {   0.f,  11.f }, RouteParam::Discrete },
    { ParamKey::Divisor,         "Divisor",    {   1.f, 768.f }, RouteParam::Discrete },
    { ParamKey::ClockMultiplier, "Clock Mult", {  50.f, 150.f }, RouteParam::Continuous },
    // engine page (2026-06-08 revamp): the 9 curated routables, musical order —
    // pitch shape, rhythm/density, contour, burst detail
    { ParamKey::Range,           "Range",      {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::MarblesBias,     "Bias",       {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::MarblesSpread,   "Spread",     {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Variation,       "Variation",  { -100.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Rest,            "Rest",       {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Burst,           "Burst",      {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Contour,         "Contour",    { -100.f, 100.f }, RouteParam::Continuous },
    { ParamKey::BurstCount,      "Burst Cnt",  {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::BurstRate,       "Burst Rate", {   0.f, 100.f }, RouteParam::Continuous },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &StochasticParamTable::table() {
    return kTable;
}
