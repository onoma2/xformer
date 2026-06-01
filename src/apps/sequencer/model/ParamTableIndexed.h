#pragma once

#include "RouteParam.h"

// Indexed per-type param table (U4 of the routing mod-matrix overhaul).
//
// Track-scope: Scope.object is a Track* in Indexed mode. Direct rows mirror
// Routing::writeTarget for an Indexed track (IndexedTrack / IndexedSequence leaf
// setters), name-agnostic. Indexed is the plan's reference for the inlet kind:
// rows A/B are INLETS (R12) -- they fill the per-track routed-CV field
// (_routedIndexedA/B) instead of a direct param, flagged RouteParam::Inlet.
//
// Parity quirks mirrored exactly: Divisor/Scale write the BASE value (ungated),
// RootNote writes the base under a gated getter (existing behavior, not a fix in
// this task); the rest write the routed slot. Inlets normalize -100..100 -> -1..1
// and fan out across patterns. Built alongside the live old dispatch; nothing
// wired live yet.

struct IndexedParamTable {
    static const RouteParam::Table &table();
};
