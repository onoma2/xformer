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
    /** Traversal patterns — 8 different walks across the 4×4 grid.
     *  Index 0 = PhaseFlux original top-down snake (default for back-compat).
     *  Indices 1..7 = Make Noise René mk2 Access Patterns 1..7 (rows,
     *  snake-up, columns, column-zigzag, spirals, diagonals).
     *  Used by engine + UI to map slotIdx → cellIdx. */
    static constexpr int kTraversalPatternCount = 8;
    static const std::array<uint8_t, kStageCount> &traversalOrder(int patternIdx);
    static const char *traversalPatternName(int patternIdx);

    /** Back-compat alias — returns traversalOrder(0). Existing call sites
     *  that don't yet thread the pattern argument keep working. */
    static const std::array<uint8_t, kStageCount> &snakeOrder();


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

    /** §14.2 Pitch Window — hidden pitch bands hold at the nearest visible
     *  boundary instead of deleting pulses. Off/visible values are identity. */
    static float holdPitchWindowBoundary(float phi, PhaseFluxSequence::WindowType v);

    /** §14.2 Window + Repeat combined eval. Returns false if phi is in the
     *  hidden band (engine should drop the pulse / hold the CV). On true,
     *  writes the post-Window-post-Repeat phi to *phiOut (caller then runs
     *  the warp / curve / response pipeline on it). */
    static bool evalWindowRepeat(float phi,
                                 PhaseFluxSequence::WindowType window,
                                 PhaseFluxSequence::RepeatType repeat,
                                 float &phiOut);

    /** v0.2 pitch phase chain: Warp -> Repeat -> Window(hold) -> FlipH.
     *  Returns phi_input for Curve::eval. warpKnob in [-64,64], 0 = identity.
     *  repeat is the integer multiplier R (1/2/3/5). */
    static float evalPitchPhase(float phi, int warpKnob, int repeat,
                                PhaseFluxSequence::WindowType window, bool flipH);

    /** v0.2 value tail: Response -> FlipV, on the raw curve output.
     *  respKnob in [-64,64], 0 = identity. */
    static float evalResponseFlipV(float pCurved, int respKnob, bool flipV);

    /** v0.2 temporal Warp -> Repeat allocation. Seeds pulse at pulseIndex /
     *  pulseCount, warps that whole-cell position, then splits by R: writes
     *  subIdx (0..R-1) and the local position within the sub-section. warp 0
     *  is globally even (sub+local)/R == pulseIndex/pulseCount. */
    static void evalTemporalAlloc(int pulseIndex, int pulseCount, int R,
                                  int warpKnob, int &subIdx, float &tLocal);

    /**
     * §3.1 — Snake-walk cumulative duration table (FLUX bottom-up).
     *   lengthArr[i]    — stage length as a count of divisor units, by cell index
     *   skip[i]         — true if cell contributes 0 (Adaptive)
     *   sequenceDivisor — ticks per divisor unit (×4 to master inside)
     *   measureDivisor  — floor source: kMinCycleTicks = measureDivisor/32
     *   clockMultiplier — 50..150 (/100 = factor)
     *   cumulativeTicks[17] — output, snake-walk order, [0]=0, [16]=cycleTicks
     * stage ticks = length × (sequenceDivisor × master/seq ratio) × 100/clockMult.
     * Returns cycleTicks; idle (all-skipped) returns 0; cycle < floor stretched.
     */
    static int computeCumulativeTicks(
        const std::array<uint8_t, kStageCount> &traversal,
        const int lengthArr[kStageCount],
        const bool skip[kStageCount],
        int sequenceDivisor,
        int measureDivisor,
        int clockMultiplier,
        int cumulativeTicks[kStageCount + 1],
        bool fixedCycleLength = false);

    /**
     * §3.2 — Derive slot/cell/phase for a relativeTick. Returns false on idle
     * (cycleTicks == 0); outputs untouched in that case.
     */
    static bool deriveTickPosition(
        const std::array<uint8_t, kStageCount> &traversal,
        uint32_t relativeTick,
        const int cumulativeTicks[kStageCount + 1],
        int cycleTicks,
        int &slotIdx,
        int &activeCell,
        float &stagePhase);
};
