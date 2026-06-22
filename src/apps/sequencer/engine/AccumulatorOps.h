#pragma once

// Stateless boundary-resolution functions for accumulator counters.
// Unified convention: each op takes an explicit `direction` (+1 ascend, -1
// descend, 0 = freeze / no-op) and a `magnitude`. Wrap/Hold/RTZ apply direction
// to magnitude; Pendulum travels by the caller-owned `pendulumDir` (direction is
// its freeze gate, and `magnitude` is the per-tick delta — the caller controls
// its sign via pendulumDir or by passing a signed value). Caller supplies
// absolute min/max; polarity-to-bounds mapping happens upstream. See
// accumulator-v2-spec §13.9.

namespace AccumulatorOps {

// Wrap: apply direction*magnitude, then positive-remainder modulo into [min,max].
void tickWrap(int &counter, int direction, int magnitude, int min, int max);

// Pendulum: counter += magnitude*pendulumDir; flip pendulumDir at a bound.
// direction == 0 freezes (no advance, no flip).
void tickPendulum(int &counter, int &pendulumDir, int direction, int magnitude, int min, int max);

// Hold: apply direction*magnitude, clip at the active-direction limit.
void tickHold(int &counter, int direction, int magnitude, int min, int max);

// Random: uniform random in [min, max]. direction == 0 freezes.
void tickRandom(int &counter, int direction, int min, int max);

// RTZ: apply direction*magnitude, then snap to zero if out of [min,max].
void tickRTZ(int &counter, int direction, int magnitude, int min, int max);

} // namespace AccumulatorOps
