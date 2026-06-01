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
// cutover. paramKeys are provisional per-type here; the shared Tier-1/2/3 key
// registry is factored when the second type lands (U4).

struct NoteParamTable {
    enum Key : uint8_t {
        None      = 0,
        // track-level
        SlideTime = 1,
        Octave    = 2,
        Transpose = 3,
        Rotate    = 4,
        GateProbabilityBias      = 5,
        RetriggerProbabilityBias = 6,
        LengthBias               = 7,
        NoteProbabilityBias      = 8,
        // sequence-level (fan-out)
        Scale     = 9,
        RootNote  = 10,
        Divisor   = 11,
        ClockMult = 12,
        RunMode   = 13,
        FirstStep = 14,
        LastStep  = 15,
    };

    static const RouteParam::Table &table();
};
