#include "ParamTableGlobal.h"
#include "RouteParamKey.h"

#include "Project.h"

#include <array>
#include <cmath>

namespace {

inline Project &project(const RouteParam::Scope &scope) {
    return *static_cast<Project *>(scope.object);
}

inline float denormalize(const RouteParam::Range &range, float normalized) {
    return normalized * (range.max - range.min) + range.min;
}

void applyTempo(const RouteParam::Scope &scope, const RouteParam::Range &range, float normalized) {
    project(scope).setTempo(denormalize(range, normalized), true);
}

void applySwing(const RouteParam::Scope &scope, const RouteParam::Range &range, float normalized) {
    project(scope).setSwing(int(std::round(denormalize(range, normalized))), true);
}

void applyCvRouteScan(const RouteParam::Scope &scope, const RouteParam::Range &range, float normalized) {
    project(scope).setCvRouteScan(int(std::round(denormalize(range, normalized))), true);
}

void applyCvRouteRoute(const RouteParam::Scope &scope, const RouteParam::Range &range, float normalized) {
    project(scope).setCvRouteRoute(int(std::round(denormalize(range, normalized))), true);
}

// Ranges mirror Routing::targetInfos so denormalization matches writeTarget.
constexpr RouteParam::Row kRows[] = {
    { ParamKey::Tempo,        "Tempo",      {    1.f, 1000.f }, RouteParam::Continuous, applyTempo        },
    { ParamKey::Swing,        "Swing",      {   50.f,   75.f }, RouteParam::Continuous, applySwing        },
    { ParamKey::CvRouteScan,  "CVR Scn",    {    0.f,  100.f }, RouteParam::Continuous, applyCvRouteScan  },
    { ParamKey::CvRouteRoute, "CVR Rte",    {    0.f,  100.f }, RouteParam::Continuous, applyCvRouteRoute },
};

constexpr RouteParam::Table kTable{ RouteParam::Scope::Kind::Global, kRows, sizeof(kRows) / sizeof(kRows[0]) };

} // namespace

const RouteParam::Table &GlobalParamTable::table() {
    return kTable;
}
