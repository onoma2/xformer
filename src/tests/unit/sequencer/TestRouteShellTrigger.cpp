#include "UnitTest.h"

#include "apps/sequencer/engine/RouteShellTrigger.h"

// Run gate consumes the raw normalized source: on above 0.55, off at/below.

UNIT_TEST("RouteShellTrigger") {

CASE("routeRunGate: on above 0.55, off at/below") {
    expectFalse(routeRunGate(0.0f), "0.0 off");
    expectFalse(routeRunGate(0.5f), "0.5 off");
    expectFalse(routeRunGate(0.55f), "0.55 off (threshold is exclusive)");
    expectTrue(routeRunGate(0.56f), "0.56 on");
    expectTrue(routeRunGate(1.0f), "1.0 on");
}

} // UNIT_TEST
