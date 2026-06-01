#pragma once

#include "RouteParam.h"

// DiscreteMap per-type param table (U4 of the routing mod-matrix overhaul).
//
// Track-scope: Scope.object is a Track* in DiscreteMap mode. Direct rows mirror
// Routing::writeTarget for a DiscreteMap track, name-agnostic. Inlet rows
// Input/Scanner/Sync (R12) fill the per-track routed-CV fields
// (_routedInput/Scanner/Sync, raw denormalized float), flagged RouteParam::Inlet;
// DiscreteMap owns these (the Sync the Indexed engine borrows lives here).
//
// SlewTime == SlideTime (owner): DiscreteMap has no track _slideTime; the slide
// concept is the sequence _slewTime, so the shared SlideTime key routes to
// setSlewTime. Parity: only SlideTime(slewTime) and ClockMult write the routed
// slot; Octave/Transpose/Offset/Divisor/Scale/RootNote/RangeHigh/RangeLow write
// the base (ungated). Direct rows fan out across patterns; inlets are track-level.
// Built alongside the live old dispatch; nothing wired live yet.

struct DiscreteMapParamTable {
    static const RouteParam::Table &table();
};
