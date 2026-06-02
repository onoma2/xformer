#pragma once

#include "RouteParam.h"

// Tuesday per-type param table (U4 of the routing mod-matrix overhaul).
//
// Track-scope: Scope.object is a Track* in Tuesday mode. All rows are
// sequence-level and write the routed slot (TuesdaySequence::writeRouted writes
// every param via setX(.., true) / Routable.write), fanning out across patterns.
// Reuses shared keys (Octave/Transpose/Rotate/Divisor/ClockMult) and appends the
// Tuesday signature block. GateLength/GateOffset write the routed slot via the
// clamped setter -- identical to the unclamped Routable.write for the in-range
// denormalized value. Scale/RootNote write the BASE (plain ints) like the other
// pitched tracks; their writeRouted cases were added alongside this table (they were
// dispatched no-ops before), so the rows are parity. The base-write defect closes at
// U6b (override table). Built alongside the live old dispatch; nothing wired live yet.

struct TuesdayParamTable {
    static const RouteParam::Table &table();
};
