#pragma once

#include "RouteParam.h"

// Stochastic per-type param table (U4 of the routing mod-matrix overhaul).
//
// Track-scope: Scope.object is a Track* in Stochastic mode. Most rows mirror
// StochasticSequence/StochasticTrack writeRouted (routed slot, gated getters);
// Octave/Transpose/SlideTime are track-level, the rest sequence-level pattern
// fan-out.
//
// Clean parity port: routing all Stochastic-specific targets was dead in the old
// dispatch (the writeTarget outer guard omitted isStochasticTarget; Feel had no
// writeRouted case; Rotate took the track branch which has no Rotate case). Those
// three live bugs were fixed alongside this table, so every row now mirrors a
// working writeTarget path. Octave/Transpose/SlideTime are track-level; the rest
// are sequence-level pattern fan-out.
//
// One residual quirk mirrored as parity: Scale/RootNote/Divisor are plain ints
// (not Routable), so the live path writes the SERIALIZED BASE. The structural fix
// is the U6b override table (transient delta), NOT a routed slot here (R14 removes
// per-pattern routed storage); this table mirrors the base write for now.
//
// Built alongside the live old dispatch; nothing wired live yet.

struct StochasticParamTable {
    static const RouteParam::Table &table();
};
