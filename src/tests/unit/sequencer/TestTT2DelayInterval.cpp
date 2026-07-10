#include "UnitTest.h"

#include "engine/TT2Evaluator.h"

// DEL.G scales its delay interval by num/denom each step. In int32 the interval
// grew unbounded (only the deadline was clamped), so repeated acceleration
// overflowed the multiply. tt2ScaleDelayInterval scales in 64-bit and saturates
// into the [1, 32767] delay range.
UNIT_TEST("TT2DelayInterval") {

CASE("geometric interval saturates without overflow") {
    expectEqual(tt2ScaleDelayInterval(32767, 4, 1), 32767, "acceleration saturates at 32767");
    expectEqual(tt2ScaleDelayInterval(2000000000, 2, 1), 32767, "huge interval saturates, no int32 overflow");
    expectEqual(tt2ScaleDelayInterval(100, 3, 2), 150, "normal ratio scales up");
    expectEqual(tt2ScaleDelayInterval(100, 1, 4), 25, "deceleration scales down");
    expectEqual(tt2ScaleDelayInterval(2, 1, 100), 1, "deceleration clamps to >= 1");
    expectEqual(tt2ScaleDelayInterval(500, 5, 0), 500, "denom 0 leaves interval unchanged");
}

}
