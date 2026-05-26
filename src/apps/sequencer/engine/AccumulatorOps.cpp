#include "AccumulatorOps.h"

#include "core/utils/Random.h"

namespace AccumulatorOps {

void tickWrap(int &counter, int direction, int min, int max, int step) {
    if (direction > 0) {
        counter += step;
        if (counter > max) {
            counter = min + (counter - max - 1);
        }
    } else if (direction < 0) {
        counter -= step;
        if (counter < min) {
            counter = max - (min - counter - 1);
        }
    }
}

void tickPendulum(int &counter, int &pendulumDir, int min, int max, int step) {
    counter += step * pendulumDir;
    if (counter >= max) {
        counter = max;
        pendulumDir = -1;
    } else if (counter <= min) {
        counter = min;
        pendulumDir = 1;
    }
}

void tickHold(int &counter, int direction, int min, int max, int step) {
    if (direction > 0) {
        counter += step;
        if (counter >= max) {
            counter = max;
        }
    } else if (direction < 0) {
        counter -= step;
        if (counter <= min) {
            counter = min;
        }
    }
}

void tickRandom(int &counter, int min, int max) {
    // Static rng matches the original Accumulator::tickWithRandom: keeps
    // sequence continuity across calls without per-instance state.
    static ::Random rng;

    if (min == max) {
        counter = min;
    } else {
        int range = max - min + 1;
        counter = min + (int)(rng.nextRange(range));
    }
}

void tickRTZ(int &counter, int min, int max) {
    if (counter > max || counter < min) {
        counter = 0;
    }
}

} // namespace AccumulatorOps
