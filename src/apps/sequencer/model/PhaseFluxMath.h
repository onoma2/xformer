#pragma once

#include "PhaseFluxSequence.h"

#include <array>
#include <cstdint>

// Stateless math helpers for the PhaseFlux track. All functions are pure —
// no engine state, no globals — so they're trivially unit-testable and the
// engine composes them in Phase B. See docs/phaseflux-spec.md §2, §3.1,
// §3.2, §6 for the algorithms these implement.
class PhaseFluxMath {
public:
    static constexpr int kStageCount = PhaseFluxSequence::StageCount;

    // §2 — boustrophedon snake permutation of the 4x4 grid.
    // Returns a reference to a static const lookup, not a copy.
    static const std::array<uint8_t, kStageCount> &snakeOrder();

    // Slot enum → ticks-per-stage in CONFIG_SEQUENCE_PPQN (48) units.
    // Matches the comment in PhaseFluxSequence.h ("12 ticks at PPQN 48").
    // Engine converts to main-PPQN ticks where needed.
    static int stageDivisorTicks(PhaseFluxSequence::StageDivisorSlot slot);

    // §6 — PowerBend(z, p) = z ^ ((1 - p) / (1 + p))
    // Safe domain: z in [0, 1], p in (-1, +1). Encoded knob values land
    // naturally in this domain via powerBendKnobToParam below.
    static float powerBend(float z, float p);

    // §16 — SignedValue<7> encoded ±63 maps to user-float via /64, so
    // max-knob lands at ±0.984375, never the degenerate ±1 boundary.
    static float powerBendKnobToParam(int encoded);

    // §3.1 — Recompute the cumulative duration table.
    //
    //   stageDivisorTicksArr[i] — ticks per stage at clockMultiplier=100,
    //                             ordered by *cell index* (0..15)
    //   skip[i]                 — true if cell i contributes 0 ticks
    //   measureDivisor          — per-measure tick count (same units as
    //                             stageDivisorTicksArr); used for the
    //                             kMinCycleTicks = measureDivisor/32 floor
    //   clockMultiplier         — 50..150, /100 = factor
    //   cumulativeTicks[17]     — output partial sums in snake-walk order;
    //                             cumulativeTicks[0]=0, [16]=cycleTicks
    //
    // Returns cycleTicks after the §3.1 minimum-cycle floor and §3.5 idle
    // handling have been applied:
    //   - all-skipped     → cycleTicks=0, all cumulativeTicks zero (idle)
    //   - 0 < cycle < min → proportional stretch to kMinCycleTicks
    //   - cycle >= min    → unchanged
    static int computeCumulativeTicks(
        const int stageDivisorTicksArr[kStageCount],
        const bool skip[kStageCount],
        int measureDivisor,
        int clockMultiplier,
        int cumulativeTicks[kStageCount + 1]);

    // §3.2 — Derive the active slot/cell and intra-stage phase for a tick.
    // Returns true if a valid position was derived. Returns false in the
    // idle case (cycleTicks == 0); outputs left untouched.
    static bool deriveTickPosition(
        uint32_t relativeTick,
        const int cumulativeTicks[kStageCount + 1],
        int cycleTicks,
        int &slotIdx,
        int &activeCell,
        float &stagePhase);
};
