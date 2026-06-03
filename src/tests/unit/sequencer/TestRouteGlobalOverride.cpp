#include "UnitTest.h"

#include "apps/sequencer/model/Routing.h"
#include "apps/sequencer/model/RouteParamKey.h"

#include <cmath>

// Global routes (Tempo/Swing/CVR) migrate to the base-anchored model, same as the
// per-track params: out = clamp(base + delta). Global has no track dimension, so its
// override lives in a reserved slot (Routing::GlobalTrack) of the same override table.
// This unit covers the read primitives; nothing wired live yet (engine fork + getter
// migration is the next unit).

namespace {
bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }
} // namespace

UNIT_TEST("RouteGlobalOverride") {

CASE("no global override reads base") {
    Routing::clearRouteOverrides();
    expectTrue(near(Routing::routedValueGlobal(ParamKey::Tempo, 120.f, 1.f, 1000.f), 120.f), "base");
    expectEqual(Routing::routedValueGlobalInt(ParamKey::Swing, 55, 50, 75), 55, "base int");
}

CASE("global override -> clamp(base + delta)") {
    Routing::clearRouteOverrides();
    Routing::writeRouteOverride(ParamKey::Tempo, Routing::GlobalTrack, 30.f);
    expectTrue(near(Routing::routedValueGlobal(ParamKey::Tempo, 120.f, 1.f, 1000.f), 150.f), "120 + 30");
    Routing::writeRouteOverride(ParamKey::Swing, Routing::GlobalTrack, 8.f);
    expectEqual(Routing::routedValueGlobalInt(ParamKey::Swing, 55, 50, 75), 63, "55 + 8");
}

CASE("global slot is distinct from per-track slot") {
    Routing::clearRouteOverrides();
    // a per-track override of the same key must not satisfy the global read
    Routing::writeRouteOverride(ParamKey::Tempo, 0, 30.f);
    expectTrue(near(Routing::routedValueGlobal(ParamKey::Tempo, 120.f, 1.f, 1000.f), 120.f),
               "track-0 override does not leak into global");
    // and a global override must not satisfy a per-track read
    Routing::clearRouteOverrides();
    Routing::writeRouteOverride(ParamKey::Tempo, Routing::GlobalTrack, 30.f);
    expectTrue(near(Routing::routedValue(ParamKey::Tempo, 0, 120.f, 1.f, 1000.f), 120.f),
               "global override does not leak into track 0");
}

CASE("global override clamps to range") {
    Routing::clearRouteOverrides();
    Routing::writeRouteOverride(ParamKey::Swing, Routing::GlobalTrack, 100.f);
    expectEqual(Routing::routedValueGlobalInt(ParamKey::Swing, 55, 50, 75), 75, "clamps hi");
    Routing::writeRouteOverride(ParamKey::Swing, Routing::GlobalTrack, -100.f);
    expectEqual(Routing::routedValueGlobalInt(ParamKey::Swing, 55, 50, 75), 50, "clamps lo");
}

CASE("global int rounds to nearest") {
    Routing::clearRouteOverrides();
    Routing::writeRouteOverride(ParamKey::Swing, Routing::GlobalTrack, 2.6f);
    expectEqual(Routing::routedValueGlobalInt(ParamKey::Swing, 55, 50, 75), 58, "55 + 2.6 -> 58");
}

CASE("clear empties the global slot") {
    Routing::clearRouteOverrides();
    Routing::writeRouteOverride(ParamKey::Tempo, Routing::GlobalTrack, 30.f);
    Routing::clearRouteOverrides();
    expectTrue(near(Routing::routedValueGlobal(ParamKey::Tempo, 120.f, 1.f, 1000.f), 120.f), "base after clear");
}

} // UNIT_TEST
