#include "ParamTableCurve.h"
#include "RouteParamKey.h"

namespace {

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    // track-level
    { ParamKey::SlideTime,           "Slide Time",   {    0.f,  100.f }, RouteParam::Continuous },
    { ParamKey::Offset,              "Offset",       { -500.f,  500.f }, RouteParam::Continuous },
    { ParamKey::Rotate,              "Rotate",       {  -64.f,   64.f }, RouteParam::Continuous },
    { ParamKey::ShapeProbabilityBias,"Shape P. Bias",{   -8.f,    8.f }, RouteParam::Continuous },
    { ParamKey::GateProbabilityBias, "Gate P. Bias", {   -8.f,    8.f }, RouteParam::Continuous },
    { ParamKey::CurveRate,           "Curve Rate",   {    0.f,  400.f }, RouteParam::Continuous },
    { ParamKey::Phase,               "Phase",        {    0.f,    1.f }, RouteParam::Continuous },
    // sequence-level (fan-out)
    { ParamKey::Divisor,             "Divisor",      {    1.f,  768.f }, RouteParam::Discrete },
    { ParamKey::ClockMultiplier,     "Clock Mult",   {   50.f,  150.f }, RouteParam::Continuous },
    { ParamKey::RunMode,             "Run Mode",     {    0.f,    5.f }, RouteParam::Discrete },
    { ParamKey::FirstStep,           "First Step",   {    0.f,   63.f }, RouteParam::Discrete },
    { ParamKey::LastStep,            "Last Step",    {    0.f,   63.f }, RouteParam::Discrete },
    { ParamKey::WavefolderFold,      "Fold",         {    0.f,  100.f }, RouteParam::Continuous },
    { ParamKey::WavefolderGain,      "Gain",         {    0.f,  200.f }, RouteParam::Continuous },
    { ParamKey::DjFilter,            "DJ Filter",    { -100.f,  100.f }, RouteParam::Continuous },
    { ParamKey::ChaosAmount,         "Chaos Amt",    {    0.f,  100.f }, RouteParam::Continuous },
    { ParamKey::ChaosRate,           "Chaos Rate",   {    0.f,  127.f }, RouteParam::Continuous },
    { ParamKey::ChaosParam1,         "Chaos P1",     {    0.f,  100.f }, RouteParam::Continuous },
    { ParamKey::ChaosParam2,         "Chaos P2",     {    0.f,  100.f }, RouteParam::Continuous },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &CurveParamTable::table() {
    return kTable;
}
