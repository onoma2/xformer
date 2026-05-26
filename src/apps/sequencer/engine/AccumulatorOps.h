#pragma once

// Stateless boundary-resolution functions for accumulator counters.
// Extracted verbatim from model/Accumulator.cpp; see accumulator-v2-spec §13.9.
// Caller supplies absolute min/max; polarity-to-bounds mapping happens upstream.

namespace AccumulatorOps {

// Wrap: on overflow, wrap to opposite limit (carry remainder).
// direction: +1 ascends, -1 descends, 0 is no-op.
void tickWrap(int &counter, int direction, int min, int max, int step);

// Pendulum: bounces between min and max, flipping pendulumDir on hit.
// pendulumDir is runtime state (caller owns it across calls).
void tickPendulum(int &counter, int &pendulumDir, int min, int max, int step);

// Hold: clips at the active-direction limit and sticks.
void tickHold(int &counter, int direction, int min, int max, int step);

// Random: uniform random in [min, max]. Direction ignored.
void tickRandom(int &counter, int min, int max);

// RTZ: snap to zero when counter is out of range. Direction unchanged.
void tickRTZ(int &counter, int min, int max);

} // namespace AccumulatorOps
