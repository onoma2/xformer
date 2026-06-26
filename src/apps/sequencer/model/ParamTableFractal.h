#pragma once

#include "RouteParam.h"

// Fractal per-type param table (KD-18).
//
// Track-scope: Scope.object is a Track* in Fractal mode. The five rows are the
// bespoke per-sequence performance controls + record arm; their getters on
// FractalSequence read Routing::routedValueInt(ParamKey::Fractal…). The table is
// the source of truth for label + range; resolution is by key.

struct FractalParamTable {
    static const RouteParam::Table &table();
};
