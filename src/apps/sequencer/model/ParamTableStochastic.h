#pragma once

#include "RouteParam.h"

// Stochastic per-type param table — live on the override path.
//
// Track-scope: Scope.object is a Track* in Stochastic mode. 8 shared band keys
// (Octave/Transpose/SlideTime/Rotate/Scale/RootNote/Divisor/ClockMult) plus the 9
// curated engine-page routables (2026-06-08 revamp): Range, Bias, Spread, Variation,
// Rest, Burst, Contour, BurstCount, BurstRate. Each getter reads
// Routing::routedValueInt(key, ...) = clamp(base + override delta).

struct StochasticParamTable {
    static const RouteParam::Table &table();
};
