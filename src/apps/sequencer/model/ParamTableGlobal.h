#pragma once

#include "RouteParam.h"

// Global (Tier-0) param table for the routing mod-matrix overhaul (U2).
//
// Whole-Performer targets addressed at Global scope. The Scope.object is a
// Project*. Each row's apply hook denormalizes via the row range and calls the
// matching Project setter -- the same mutation the live Routing::writeTarget
// path produces, just name-agnostic.
//
// This table is built alongside the still-authoritative old dispatch; the
// engine does not route through it until the U7 cutover.
//
// Scope of U2: the stateless value-writes. Edge-triggered engine actions
// (Play/PlayToggle/Record/RecordToggle/TapTempo) carry latch state and Bus
// targets carry write arbitration -- both land later when that state model
// is settled.
//
// paramKeys are append-only numeric, never the array index (F6).

struct GlobalParamTable {
    enum Key : uint8_t {
        None         = 0,
        Tempo        = 1,
        Swing        = 2,
        CvRouteScan  = 3,
        CvRouteRoute = 4,
    };

    static const RouteParam::Table &table();
};
