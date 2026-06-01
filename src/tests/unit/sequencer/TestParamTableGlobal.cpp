#include "UnitTest.h"

#include "model/Project.h"
#include "model/ParamTableGlobal.h"

// U2 of the routing mod-matrix overhaul: the global (Tier-0) param table.
// Characterization: each global row's applyRouted hook must mutate the Project
// identically to the live Routing::writeTarget path for the same normalized
// input. Built alongside the old dispatch -- no engine cutover (that is U7).
// Scope here is the stateless value-writes (Tempo/Swing/CvRouteScan/CvRouteRoute);
// edge-triggered engine actions (Play/Record/...) and Bus carry state/arbitration
// settled later.

UNIT_TEST("ParamTableGlobal") {

CASE("value-write rows match writeTarget for every normalized input") {
    struct Spec { Routing::Target target; uint8_t key; const char *name; };
    const Spec specs[] = {
        { Routing::Target::Tempo,        GlobalParamTable::Tempo,        "Tempo" },
        { Routing::Target::Swing,        GlobalParamTable::Swing,        "Swing" },
        { Routing::Target::CvRouteScan,  GlobalParamTable::CvRouteScan,  "CvRouteScan" },
        { Routing::Target::CvRouteRoute, GlobalParamTable::CvRouteRoute, "CvRouteRoute" },
    };
    const float inputs[] = { 0.f, 0.25f, 0.5f, 0.75f, 1.f };

    for (const auto &s : specs) {
        // Expose the routed slot through the gated getters so a wrong
        // denormalize would surface (otherwise getters return the base value).
        Routing::setRouted(s.target, 0xFF, true);

        for (float n : inputs) {
            Project oldP, newP;
            oldP.routing().writeTarget(s.target, 0, n);

            RouteParam::Scope scope;
            scope.kind = RouteParam::Scope::Kind::Global;
            scope.object = &newP;
            bool applied = GlobalParamTable::table().applyRouted(scope, s.key, n);
            expectTrue(applied, "global row applied");

            expectEqual(newP.tempo(), oldP.tempo(), "tempo parity");
            expectEqual(newP.swing(), oldP.swing(), "swing parity");
            expectEqual(newP.cvRouteScan(), oldP.cvRouteScan(), "scan parity");
            expectEqual(newP.cvRouteRoute(), oldP.cvRouteRoute(), "route parity");
        }
    }
}

CASE("table rejects keys it does not own") {
    Project p;
    RouteParam::Scope scope;
    scope.kind = RouteParam::Scope::Kind::Global;
    scope.object = &p;
    expectFalse(GlobalParamTable::table().applyRouted(scope, 0, 0.5f), "key 0 not owned");
    expectFalse(GlobalParamTable::table().applyRouted(scope, 200, 0.5f), "unknown key not owned");
}

} // UNIT_TEST
