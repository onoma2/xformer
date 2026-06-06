#include "ParamTableNote.h"
#include "RouteParamKey.h"

namespace {

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    { ParamKey::SlideTime, "Slide Time",  {   0.f,  100.f }, RouteParam::Continuous },
    { ParamKey::Octave,    "Octave",      { -10.f,   10.f }, RouteParam::Continuous },
    { ParamKey::Transpose, "Transp",      { -60.f,   60.f }, RouteParam::Continuous },
    { ParamKey::Rotate,    "Rotate",      { -64.f,   64.f }, RouteParam::Continuous },
    { ParamKey::GateProbabilityBias,      "Gate PB",        { -8.f, 8.f }, RouteParam::Continuous },
    { ParamKey::RetriggerProbabilityBias, "Rtrg PB",        { -8.f, 8.f }, RouteParam::Continuous },
    { ParamKey::LengthBias,               "Len Bias",       { -8.f, 8.f }, RouteParam::Continuous },
    { ParamKey::NoteProbabilityBias,      "Note PB",        { -8.f, 8.f }, RouteParam::Continuous },
    { ParamKey::Scale,     "Scale",       {   0.f,   23.f }, RouteParam::Discrete },
    { ParamKey::RootNote,  "Root N",      {   0.f,   11.f }, RouteParam::Discrete },
    { ParamKey::Divisor,   "Divisor",     {   1.f,  768.f }, RouteParam::Discrete },
    { ParamKey::ClockMultiplier, "Clk Mlt",     {  50.f,  150.f }, RouteParam::Continuous },
    { ParamKey::RunMode,   "Run Mode",    {   0.f,    5.f }, RouteParam::Discrete },
    { ParamKey::FirstStep, "First Step",  {   0.f,   63.f }, RouteParam::Discrete },
    { ParamKey::LastStep,  "Last Step",   {   0.f,   63.f }, RouteParam::Discrete },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &NoteParamTable::table() {
    return kTable;
}
