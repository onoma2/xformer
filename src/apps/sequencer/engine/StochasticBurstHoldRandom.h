#pragma once

#include "KeyedRng.h"
#include "model/StochasticTypes.h"
#include "core/utils/Random.h"

#include <cstdint>

// Above 50% burst the per-fired-burst BurstHold mode is randomized off a salted
// per-cell RNG — deterministic and scrubbable like the rest dice (same seed +
// cell index always yields the same mode). At/below 50% it is the configured
// mode. The two axes (Roll/Hold pitch, Fit/Over timing) flip independently.
//
// Roll/Hold and Fit/Over live in different engine stages; each caller computes
// its own axis with the seed + index it owns. Within one trigger the generator
// bakes the Roll/Hold choice onto each EvaluatedBurstNote so playback agrees.
inline StochasticBurstHold stochasticBurstHoldForCell(int burst, StochasticBurstHold configured,
                                                      uint32_t seed, uint32_t cellIndex) {
    if (burst <= 50) {
        return configured;
    }
    Random rng(keyed_rng::cellSeed(seed, cellIndex) ^ 0xB47570D1u);
    bool roll = rng.nextRange(2) != 0;
    bool fit = rng.nextRange(2) != 0;
    if (roll) {
        return fit ? StochasticBurstHold::RollFit : StochasticBurstHold::RollOver;
    }
    return fit ? StochasticBurstHold::HoldFit : StochasticBurstHold::HoldOver;
}
