#pragma once

#include "RouteParam.h"

// PhaseFlux per-type param table (U4 of the routing mod-matrix overhaul).
//
// Track-scope: Scope.object is a Track* in PhaseFlux mode. Parity rows mirror
// PhaseFluxTrack/PhaseFluxSequence writeRouted: Octave/Transpose/SlideTime are
// track-level routed; Divisor/ClockMult are sequence-level routed; Scale/RootNote
// are sequence-level base writes (plain ints, defect closes at U6b -- see
// ParamTableStochastic.h).
//
// LAUNCH additions (new routables, no writeTarget equivalent), sequence-level
// fan-out, tested directly: the five nudges (WarpNudge/ResponseNudge/LenNudge/
// CyclePhaseWarp/PulseNudge) and Phase (globalPhase, shared key with Curve). These
// fields are plain (ungated), so the hooks call the setter and the value lands.
//
// DEFERRED: A/B inlets. PhaseFlux has no inlet storage or stage-group engine
// consumption yet (unlike Indexed's pre-existing _routedIndexedA/B); wiring them is
// a separate model+engine task, not a table row.
//
// Built alongside the live old dispatch; nothing wired live yet.

struct PhaseFluxParamTable {
    static const RouteParam::Table &table();
};
