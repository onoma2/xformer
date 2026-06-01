#pragma once

#include "RouteParam.h"

// Note per-type param table (U3 of the routing mod-matrix overhaul).
//
// Track-scope: Scope.object is a Track* in Note mode. Each row's hook denormalizes
// via the row range and calls the NoteTrack / NoteSequence leaf setter -- the same
// mutation Routing::writeTarget produces for a Note track, name-agnostic. Sequence
// rows fan out across all patterns (modulation is pattern-independent), the loop
// being a hook detail.
//
// Built alongside the live old dispatch; the engine routes through it at the U7
// cutover. Row keys come from the shared ParamKey registry (model/RouteParamKey.h),
// the single append-only numbering authority -- shared concepts reuse one key.

struct NoteParamTable {
    static const RouteParam::Table &table();
};
