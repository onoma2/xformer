#include "UnitTest.h"

#include "apps/sequencer/model/Routing.h"
#include "apps/sequencer/model/RouteParamKey.h"

#include <cmath>

// Routed-value override table (transient, not serialized): the model-owned
// (trackIndex, paramKey) -> signed delta store the migrated getters read as
// clamp(base + delta). Presence = routed (mirrors Routing::isRouted active
// semantics, track-mask dimension preserved). Cleared/rebuilt each recompute.
//
// Step 1 of the apply-fork slice: storage + accessor + clamp helper only.
// Nothing wired live yet (the engine only calls clearRouteOverrides()).

namespace {

bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

} // namespace

UNIT_TEST("RouteOverride") {

CASE("empty table reads base unchanged") {
    Routing::clearRouteOverrides();
    float d = -1.f;
    expectFalse(Routing::routeOverride(ParamKey::Scale, 2, d), "no entry");
    expectTrue(near(Routing::routedValue(ParamKey::Scale, 2, 10.f, 0.f, 23.f), 10.f), "reads base");
}

CASE("write then read returns clamp(base + delta)") {
    Routing::clearRouteOverrides();
    Routing::writeRouteOverride(ParamKey::Scale, 2, 5.f);
    float d = 0.f;
    expectTrue(Routing::routeOverride(ParamKey::Scale, 2, d), "entry present");
    expectTrue(near(d, 5.f), "delta stored");
    expectTrue(near(Routing::routedValue(ParamKey::Scale, 2, 10.f, 0.f, 23.f), 15.f), "base + delta");
}

CASE("presence is per track") {
    Routing::clearRouteOverrides();
    Routing::writeRouteOverride(ParamKey::Scale, 2, 5.f);
    float d = 0.f;
    expectFalse(Routing::routeOverride(ParamKey::Scale, 3, d), "other track not present");
    expectTrue(near(Routing::routedValue(ParamKey::Scale, 3, 10.f, 0.f, 23.f), 10.f), "other track reads base");
}

CASE("presence is per param key") {
    Routing::clearRouteOverrides();
    Routing::writeRouteOverride(ParamKey::Scale, 2, 5.f);
    float d = 0.f;
    expectFalse(Routing::routeOverride(ParamKey::RootNote, 2, d), "other key not present");
}

CASE("zero delta still counts as present") {
    Routing::clearRouteOverrides();
    Routing::writeRouteOverride(ParamKey::Scale, 2, 0.f);
    float d = -1.f;
    expectTrue(Routing::routeOverride(ParamKey::Scale, 2, d), "present even at delta 0");
    expectTrue(near(d, 0.f), "delta is zero");
    expectTrue(near(Routing::routedValue(ParamKey::Scale, 2, 10.f, 0.f, 23.f), 10.f), "reads base");
}

CASE("clear empties the table") {
    Routing::clearRouteOverrides();
    Routing::writeRouteOverride(ParamKey::Scale, 2, 5.f);
    Routing::clearRouteOverrides();
    float d = -1.f;
    expectFalse(Routing::routeOverride(ParamKey::Scale, 2, d), "cleared");
    expectTrue(near(Routing::routedValue(ParamKey::Scale, 2, 10.f, 0.f, 23.f), 10.f), "reads base after clear");
}

CASE("routedValue clamps to [lo, hi]") {
    Routing::clearRouteOverrides();
    Routing::writeRouteOverride(ParamKey::Scale, 2, 100.f);
    expectTrue(near(Routing::routedValue(ParamKey::Scale, 2, 10.f, 0.f, 23.f), 23.f), "clamps hi");
    Routing::writeRouteOverride(ParamKey::Scale, 2, -100.f);
    expectTrue(near(Routing::routedValue(ParamKey::Scale, 2, 10.f, 0.f, 23.f), 0.f), "clamps lo");
}

CASE("re-write same key/track replaces delta") {
    Routing::clearRouteOverrides();
    Routing::writeRouteOverride(ParamKey::Scale, 2, 5.f);
    Routing::writeRouteOverride(ParamKey::Scale, 2, -3.f);
    float d = 0.f;
    expectTrue(Routing::routeOverride(ParamKey::Scale, 2, d), "still present");
    expectTrue(near(d, -3.f), "last write wins");
}

} // UNIT_TEST
