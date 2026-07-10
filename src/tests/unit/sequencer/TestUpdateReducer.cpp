#include "UnitTest.h"

#include "engine/UpdateReducer.h"

// UpdateReducer gates work to once per Interval ticks. The old compare
// (now >= _lastUpdate + Interval) breaks when the os::ticks() counter wraps
// past uint32 max: enough time has elapsed but the wrapped sum makes it miss.
// The signed-difference idiom (int32_t(now - _lastUpdate) >= Interval) is
// wrap-safe.
UNIT_TEST("UpdateReducer") {

CASE("fires once the interval elapses in the normal range") {
    UpdateReducer<10> r;
    expectTrue(r.update(1000), "first update fires");
    expectFalse(r.update(1005), "5 ticks later does not fire");
    expectTrue(r.update(1010), "10 ticks later fires");
}

CASE("fires across a uint32 tick-counter wrap") {
    UpdateReducer<10> r;
    // Walk _lastUpdate up to just below the wrap in valid (< 2^31) steps.
    expectTrue(r.update(0x7FFFFFF0), "prime step 1 fires");
    expectTrue(r.update(0xFFFFFFE0), "prime step 2 fires (near wrap)");

    expectFalse(r.update(0xFFFFFFE5), "5 ticks later, no wrap, does not fire");
    expectTrue(r.update(0x00000005), "37 ticks elapsed across the wrap fires");
}

}
