#pragma once

#include "RouteParam.h"

// DiscreteMap per-type param table (U4 of the routing mod-matrix overhaul).
//
// Track-scope: Scope.object is a Track* in DiscreteMap mode. Direct rows mirror
// Routing::writeTarget for a DiscreteMap track, name-agnostic. Inlet rows
// Input/Scanner (R12) fill the per-track routed-CV fields (_routedInput/Scanner,
// raw denormalized float), flagged RouteParam::Inlet. Sync is NOT an inlet here:
// external re-anchor folds into the universal Align target (binds restart()) --
// see docs/plans/2026-06-02-004-align-vs-reset subspec.
//
// SlewTime == SlideTime (owner): DiscreteMap has no track _slideTime; the slide
// concept is the sequence _slewTime, so the shared SlideTime key routes to
// setSlewTime. Parity: only SlideTime(slewTime) and ClockMult write the routed
// slot; Octave/Transpose/Offset/Divisor/Scale/RootNote/RangeHigh/RangeLow write
// the base (ungated). Fan-out matches writeTarget: track-routed params (Octave/
// Transpose/Offset/SlideTime/RangeHigh/RangeLow) reach all sequences incl. snapshot
// slots; sequence-targets (Divisor/ClockMult/Scale/RootNote) touch patterns only;
// inlets are track-level. Built alongside the live old dispatch; nothing wired live.

struct DiscreteMapParamTable {
    static const RouteParam::Table &table();
};
