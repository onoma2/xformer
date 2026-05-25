#pragma once

#include "PhaseFluxSequence.h"

#include <array>
#include <cstdint>

/**
 * Stateless math helpers for the PhaseFlux track. Pure functions, no
 * engine state — see docs/phaseflux-spec.md §2, §3, §6.
 */
class PhaseFluxMath {
public:
    static constexpr int kStageCount = PhaseFluxSequence::StageCount;

    /** §2 — boustrophedon snake permutation of the 4×4 grid. */
    static const std::array<uint8_t, kStageCount> &snakeOrder();

    /** Slot enum → ticks-per-stage in CONFIG_SEQUENCE_PPQN (48) units. */
    static int stageDivisorTicks(PhaseFluxSequence::StageDivisorSlot slot);

    /** §6 — PowerBend(z, p) = z ^ ((1 − p) / (1 + p)), z∈[0,1], p∈(−1,+1). */
    static float powerBend(float z, float p);

    /** §16 — encoded ±63 → ±0.984375 via /64; ±1 degeneracy excluded by design. */
    static float powerBendKnobToParam(int encoded);

    /**
     * §3.1 — Snake-walk cumulative duration table.
     *   stageDivisorTicksArr[i] — ticks/stage at clockMultiplier=100, by cell index
     *   skip[i]                 — true if cell contributes 0
     *   measureDivisor          — floor source: kMinCycleTicks = measureDivisor/32
     *   clockMultiplier         — 50..150 (/100 = factor)
     *   cumulativeTicks[17]     — output, snake-walk order, [0]=0, [16]=cycleTicks
     * Returns cycleTicks; idle (all-skipped) returns 0 with array zeroed;
     * cycle < floor is proportionally stretched.
     */
    static int computeCumulativeTicks(
        const int stageDivisorTicksArr[kStageCount],
        const bool skip[kStageCount],
        int measureDivisor,
        int clockMultiplier,
        int cumulativeTicks[kStageCount + 1]);

    /**
     * §3.2 — Derive slot/cell/phase for a relativeTick. Returns false on idle
     * (cycleTicks == 0); outputs untouched in that case.
     */
    static bool deriveTickPosition(
        uint32_t relativeTick,
        const int cumulativeTicks[kStageCount + 1],
        int cycleTicks,
        int &slotIdx,
        int &activeCell,
        float &stagePhase);
};
