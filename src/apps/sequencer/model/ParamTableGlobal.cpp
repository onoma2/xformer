#include "ParamTableGlobal.h"
#include "RouteParamKey.h"

namespace {

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    { ParamKey::Tempo,        "Tempo",      {    1.f, 1000.f }, RouteParam::Continuous },
    { ParamKey::Swing,        "Swing",      {   50.f,   75.f }, RouteParam::Continuous },
    { ParamKey::CvRouteScan,  "CVR Scn",    {    0.f,  100.f }, RouteParam::Continuous },
    { ParamKey::CvRouteRoute, "CVR Rte",    {    0.f,  100.f }, RouteParam::Continuous },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Global, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &GlobalParamTable::table() {
    return kTable;
}
