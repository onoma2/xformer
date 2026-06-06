#include "ParamTableDiscreteMap.h"
#include "RouteParamKey.h"

namespace {

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    // sequence-level direct (fan-out)
    { ParamKey::Octave,          "Octave",     { -10.f,  10.f }, RouteParam::Continuous },
    { ParamKey::Transpose,       "Transpose",  { -60.f,  60.f }, RouteParam::Continuous },
    { ParamKey::Offset,          "Offset",     { -500.f, 500.f }, RouteParam::Continuous },
    { ParamKey::SlideTime,       "Slide Time", {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::Divisor,         "Divisor",    {   1.f, 768.f }, RouteParam::Discrete },
    { ParamKey::ClockMultiplier, "Clock Mult", {  50.f, 150.f }, RouteParam::Continuous },
    { ParamKey::Scale,           "Scale",      {   0.f,  23.f }, RouteParam::Discrete },
    { ParamKey::RootNote,        "Root Note",  {   0.f,  11.f }, RouteParam::Discrete },
    { ParamKey::DiscreteMapRangeHigh, "Range Hi", { -5.f, 5.f }, RouteParam::Continuous },
    { ParamKey::DiscreteMapRangeLow,  "Range Lo", { -5.f, 5.f }, RouteParam::Continuous },
    // inlets (track-level) -- Sync dropped (folds into universal Align, subspec 004)
    { ParamKey::DiscreteMapInput,   "Inlet In",  { -5.f,  5.f }, RouteParam::Inlet },
    { ParamKey::DiscreteMapScanner, "Inlet Scan",{  0.f, 34.f }, RouteParam::Inlet },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &DiscreteMapParamTable::table() {
    return kTable;
}
