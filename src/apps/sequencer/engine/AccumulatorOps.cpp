#include "AccumulatorOps.h"

#include "core/utils/Random.h"

namespace AccumulatorOps {

void tickWrap(int &counter, int direction, int magnitude, int min, int max) {
    if (direction == 0) return;
    if (direction > 0) counter += magnitude;
    else               counter -= magnitude;
    // Safe positive-remainder modulo: handles arbitrary overshoot in both
    // directions (magnitude can exceed range; single-subtract would leave the
    // counter outside [min,max]).
    const int range = max - min + 1;
    if (range <= 0) return;
    int delta = counter - min;
    delta = ((delta % range) + range) % range;
    counter = min + delta;
}

void tickPendulum(int &counter, int &pendulumDir, int direction, int magnitude, int min, int max) {
    if (direction == 0) return; // freeze
    // Travel is governed by the caller-owned pendulumDir; `direction` is only the
    // freeze gate. The caller seeds pendulumDir (and/or signs magnitude) to set
    // the initial travel sense.
    counter += magnitude * pendulumDir;
    if (counter >= max) {
        counter = max;
        pendulumDir = -pendulumDir;
    } else if (counter <= min) {
        counter = min;
        pendulumDir = -pendulumDir;
    }
}

void tickHold(int &counter, int direction, int magnitude, int min, int max) {
    if (direction > 0) {
        counter += magnitude;
        if (counter >= max) counter = max;
    } else if (direction < 0) {
        counter -= magnitude;
        if (counter <= min) counter = min;
    }
    // direction == 0 -> no-op
}

void tickRandom(int &counter, int direction, int min, int max) {
    if (direction == 0) return; // freeze
    static ::Random rng;
    if (min == max) {
        counter = min;
    } else {
        int range = max - min + 1;
        counter = min + (int)(rng.nextRange(range));
    }
}

void tickRTZ(int &counter, int direction, int magnitude, int min, int max) {
    if (direction == 0) return; // freeze
    if (direction > 0) counter += magnitude;
    else               counter -= magnitude;
    if (counter > max || counter < min) {
        counter = 0;
    }
}

} // namespace AccumulatorOps
