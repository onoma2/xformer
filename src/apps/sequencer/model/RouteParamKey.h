#pragma once

#include <cstdint>

// Shared param-key registry for the routing mod-matrix overhaul.
//
// The single numbering authority. A route addresses a param by (scope, paramKey);
// this is where every paramKey number is minted. Keys are APPEND-ONLY and NEVER
// REUSED after retirement (F6) -- a saved route stores the number, so reordering
// or removing concepts must never shift an existing key. Keys are never the array
// index into a Table's Row[]; the Table resolves key -> Row by scan.
//
// "One row, many types": a shared concept (Transpose, Scale, ...) gets ONE key
// here, reused verbatim by every per-type table that backs it -- the picker and
// group UI key off this number, not off any per-type label. Only the apply hook
// is per-type; the address is shared.
//
// Tiers below mirror the plan's applicability matrix. Numbers are grouped into
// reserved ranges per tier so future appends stay with their concept group;
// using a reserved-but-unminted number for the first time is a normal append,
// not a reuse. Mint a key only when a Row actually uses it (append-on-use).
//
// See docs/plans/2026-05-31-002-routing-mod-matrix-overhaul-plan.md (registry,
// concept-equivalence gate) and model/RouteParam.h (the Table/Row mechanism).

struct ParamKey {
    enum : uint8_t {
        None = 0,

        // --- Tier 0: whole-Performer, Global scope (1..19) ---
        Tempo        = 1,
        Swing        = 2,
        CvRouteScan  = 3,
        CvRouteRoute = 4,
        // reserve 5..19: Play/PlayToggle/Record/RecordToggle/TapTempo, BusCv1-4

        // --- Tier 1: universal per-track (20..39) ---
        Divisor         = 20,
        ClockMultiplier = 21,
        // reserve 22..39: Mute/Fill/FillAmount/Pattern/Run/Reset/CvOutputRotate/GateOutputRotate

        // --- Tier 2: pitched / structural shared (40..59) ---
        Scale     = 40,
        RootNote  = 41,
        Transpose = 42,
        Octave    = 43,
        SlideTime = 44,
        Rotate    = 45,
        Offset    = 46,   // Curve (DiscreteMap later)
        // reserve 47..59

        // --- Tier 2b: temporal phase (60..63) ---
        Phase = 60,   // Curve/PhaseFlux globalPhase (clamp 0..1)

        // --- Tier 3: step-range, Note/Curve family (64..79) ---
        RunMode   = 64,
        FirstStep = 65,
        LastStep  = 66,
        // reserve 67..79

        // --- Tier 4: type-specific (80..), appended as per-type tables land ---
        // Probability-bias family (shared, per-type backing):
        GateProbabilityBias      = 80,   // Note, Curve
        RetriggerProbabilityBias = 81,   // Note
        LengthBias               = 82,   // Note
        NoteProbabilityBias      = 83,   // Note
        ShapeProbabilityBias     = 84,   // Curve
        // Curve signature block (90..):
        WavefolderFold = 90,
        WavefolderGain = 91,
        DjFilter       = 92,
        ChaosAmount    = 93,
        ChaosRate      = 94,
        ChaosParam1    = 95,
        ChaosParam2    = 96,
        CurveRate      = 97,

        // --- Inlets: per-track CV buses, not direct params (R12/R13) (100..) ---
        // Each type owns its own inlets (no borrowing). Rows flagged Inlet.
        IndexedA = 100,   // Indexed
        IndexedB = 101,   // Indexed
    };
};
