#include "AccumulatorOps.h"

#include "core/utils/Random.h"

namespace AccumulatorOps {

void tickWrap(int &counter, int direction, int min, int max, int step) {
    // Safe positive-remainder modulo: handles arbitrary overshoot in both
    // directions (step can exceed range; single-subtract would leave counter
    // outside [min,max]).
    if (direction == 0) return;
    if (direction > 0) counter += step;
    else               counter -= step;
    const int range = max - min + 1;
    if (range <= 0) return;
    int delta = counter - min;
    delta = ((delta % range) + range) % range;
    counter = min + delta;
}

void tickPendulum(int &counter, int &pendulumDir, int min, int max, int step) {
    // Diverges from NoteTrack's tickWithPendulum: PhaseFlux passes a signed
    // step, so we flip pendulumDir at boundary rather than forcing ±1.
    counter += step * pendulumDir;
    if (counter >= max) {
        counter = max;
        pendulumDir = -pendulumDir;
    } else if (counter <= min) {
        counter = min;
        pendulumDir = -pendulumDir;
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
