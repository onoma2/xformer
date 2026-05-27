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

    /** Slot enum → ticks-per-stage AT the 1/16 reference base (sequence
     *  divisor = 12 at PPQN-48). Stage divisor is therefore a relative
     *  multiplier of the sequence base period — sequence divisor governs
     *  the cycle length; stage slot picks each stage's ratio within. */
    static int stageDivisorTicks(PhaseFluxSequence::StageDivisorSlot slot);

    /** Reference sequence divisor (= 1/16 at PPQN-48). Stage tick table is
     *  calibrated against this; other divisor values scale uniformly. */
    static constexpr int kReferenceSequenceDivisor = 12;

    /** §6 — PowerBend(z, p) = z ^ ((1 − p) / (1 + p)), z∈[0,1], p∈(−1,+1). */
    static float powerBend(float z, float p);

    /** §16 — encoded ±63 → ±0.984375 via /64; ±1 degeneracy excluded by design. */
    static float powerBendKnobToParam(int encoded);

    /** §14.2 Repeat — enum value to integer multiplier (1 / 2 / 3 / 5). */
    static int repeatMultiplier(PhaseFluxSequence::RepeatType v);

    /** §14.2 Window — visible-band check on input phi ∈ [0, 1].
     *  Off: always true. Focus N: phi in centered N% band. Polarize N:
     *  phi in outer N% (split N/2 each side). */
    static bool isInWindow(float phi, PhaseFluxSequence::WindowType v);

    /** §14.2 Window + Repeat combined eval. Returns false if phi is in the
     *  hidden band (engine should drop the pulse / hold the CV). On true,
     *  writes the post-Window-post-Repeat phi to *phiOut (caller then runs
     *  the warp / curve / response pipeline on it). */
    static bool evalWindowRepeat(float phi,
                                 PhaseFluxSequence::WindowType window,
                                 PhaseFluxSequence::RepeatType repeat,
                                 float &phiOut);

    /**
     * §3.1 — Snake-walk cumulative duration table.
     *   stageDivisorTicksArr[i] — ticks/stage at clockMultiplier=100, by cell index
     *   stageLenArr[i]          — per-stage ±100 length multiplier (factor = 1 + v/100)
     *   skip[i]                 — true if cell contributes 0
     *   measureDivisor          — floor source: kMinCycleTicks = measureDivisor/32
     *   clockMultiplier         — 50..150 (/100 = factor)
     *   cumulativeTicks[17]     — output, snake-walk order, [0]=0, [16]=cycleTicks
     * Returns cycleTicks; idle (all-skipped) returns 0 with array zeroed;
     * cycle < floor is proportionally stretched.
     */
    static int computeCumulativeTicks(
        const int stageDivisorTicksArr[kStageCount],
        const int stageLenArr[kStageCount],
        const bool skip[kStageCount],
        int sequenceDivisor,
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
