#include "ParamTableFractal.h"
#include "RouteParamKey.h"

namespace {

// Ranges mirror targetInfos so denormalization matches the routed-value getters.
constexpr RouteParam::Row kRows[] = {
    { ParamKey::FractalBranchCount,       "Branch Count", {   0.f,   7.f }, RouteParam::Discrete },
    { ParamKey::FractalPath,              "Path",         {   0.f, 255.f }, RouteParam::Discrete },
    { ParamKey::FractalOrnamentRate,      "Orn Rate",     {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::FractalOrnamentIntensity, "Orn Intens",   {   0.f, 100.f }, RouteParam::Continuous },
    { ParamKey::FractalRecordTrigger,     "Record Arm",   {   0.f,   1.f }, RouteParam::Discrete },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Track, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &FractalParamTable::table() {
    return kTable;
}
