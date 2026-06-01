#pragma once

#include "RouteParam.h"

// Curve per-type param table (U4 of the routing mod-matrix overhaul).
//
// Track-scope: Scope.object is a Track* in Curve mode. Each row's hook
// denormalizes via the row range and calls the CurveTrack / CurveSequence leaf
// setter -- the same mutation Routing::writeTarget produces for a Curve track,
// name-agnostic. Track-level rows write once; sequence-level rows fan out across
// all patterns (modulation is pattern-independent), the loop being a hook detail.
//
// Reuses shared keys from the registry for concepts Curve shares with Note
// (SlideTime/Rotate/GateProbabilityBias/Divisor/ClockMult/RunMode/First/LastStep)
// -- the "one key, many types" property. Phase (globalPhase) is a launch addition
// with no writeTarget equivalent; it lights up when the table becomes authoritative
// at U7. Built alongside the live old dispatch; nothing wired live yet.

struct CurveParamTable {
    static const RouteParam::Table &table();
};
