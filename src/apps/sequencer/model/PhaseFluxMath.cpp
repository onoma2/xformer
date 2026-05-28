#include "PhaseFluxMath.h"

#include <cmath>

// Boustrophedon snake: row 0 L->R, row 1 R->L, row 2 L->R, row 3 R->L.
// Stored as a static const so the engine can take a reference cheaply.
static const std::array<uint8_t, PhaseFluxMath::kStageCount> kSnakeOrder = {{
    0,  1,  2,  3,    // row 0  L -> R
    7,  6,  5,  4,    // row 1  R -> L
    8,  9, 10, 11,    // row 2  L -> R
   15, 14, 13, 12,    // row 3  R -> L
}};

// Per-stage divisor as a fraction (num/den) multiplied against the sequence
// divisor — Stochastic's `kStochasticDurationLut` pattern. At the NoteTrack
// reference seqDivisor=12, slot labels match their musical name; change
// seqDivisor and every cell scales uniformly. Slot 6 (Bar) is the default.
static const PhaseFluxMath::StageDivisorFraction kStageDivisorFrac[8] = {
    {  1, 1 },   // 0 Sixteenth — ×1     → 1/16 at seqDiv=12
    {  4, 3 },   // 1 EighthT   — ×4/3   → 1/8T
    {  2, 1 },   // 2 Eighth    — ×2     → 1/8
    {  8, 3 },   // 3 QuarterT  — ×8/3   → 1/4T
    {  4, 1 },   // 4 Quarter   — ×4     → 1/4
    {  8, 1 },   // 5 Half      — ×8     → 1/2
    { 16, 1 },   // 6 Bar       — ×16    → 1 bar  (DEFAULT)
    { 32, 1 },   // 7 TwoBar    — ×32    → 2 bars
};

const std::array<uint8_t, PhaseFluxMath::kStageCount> &PhaseFluxMath::snakeOrder() {
    return kSnakeOrder;
}

PhaseFluxMath::StageDivisorFraction PhaseFluxMath::stageDivisorFraction(PhaseFluxSequence::StageDivisorSlot slot) {
    int idx = int(slot);
    if (idx < 0) idx = 0;
    if (idx > 7) idx = 7;
    return kStageDivisorFrac[idx];
}

int PhaseFluxMath::stageDivisorTicks(PhaseFluxSequence::StageDivisorSlot slot) {
    // Master-PPQN-192 ticks at the calibration point (seqDivisor=12).
    // kReferenceSequenceDivisor (= 12 in sequencer-PPQN-48) × 4 (master/seq ratio)
    // = 48 master ticks per ×1 multiplier. Multiply by the slot's fraction.
    auto frac = stageDivisorFraction(slot);
    return (48 * int(frac.num)) / int(frac.den);
}

float PhaseFluxMath::powerBend(float z, float p) {
    // Caller guarantees z in [0,1] and p in (-1,+1). Endpoints are exact —
    // std::pow(0, k) == 0 for k > 0 and std::pow(1, k) == 1 for all k.
    float exponent = (1.0f - p) / (1.0f + p);
    return std::pow(z, exponent);
}

float PhaseFluxMath::powerBendKnobToParam(int encoded) {
    return float(encoded) / 64.0f;
}

int PhaseFluxMath::repeatMultiplier(PhaseFluxSequence::RepeatType v) {
    switch (v) {
    case PhaseFluxSequence::RepeatType::x1: return 1;
    case PhaseFluxSequence::RepeatType::x2: return 2;
    case PhaseFluxSequence::RepeatType::x3: return 3;
    case PhaseFluxSequence::RepeatType::x5: return 5;
    }
    return 1;
}

bool PhaseFluxMath::isInWindow(float phi, PhaseFluxSequence::WindowType v) {
    switch (v) {
    case PhaseFluxSequence::WindowType::Off:        return true;
    case PhaseFluxSequence::WindowType::Focus70:    return phi >= 0.15f && phi <= 0.85f;
    case PhaseFluxSequence::WindowType::Focus50:    return phi >= 0.25f && phi <= 0.75f;
    case PhaseFluxSequence::WindowType::Polarize70: return phi <= 0.35f || phi >= 0.65f;
    case PhaseFluxSequence::WindowType::Polarize50: return phi <= 0.25f || phi >= 0.75f;
    }
    return true;
}

bool PhaseFluxMath::evalWindowRepeat(float phi,
                                     PhaseFluxSequence::WindowType window,
                                     PhaseFluxSequence::RepeatType repeat,
                                     float &phiOut) {
    if (!isInWindow(phi, window)) return false;
    const int R = repeatMultiplier(repeat);
    if (R > 1) {
        float scaled = phi * float(R);
        phiOut = scaled - std::floor(scaled);
    } else {
        phiOut = phi;
    }
    return true;
}

int PhaseFluxMath::computeCumulativeTicks(
    const int stageDivisorTicksArr[kStageCount],
    const int stageLenArr[kStageCount],
    const bool skip[kStageCount],
    int sequenceDivisor,
    int measureDivisor,
    int clockMultiplier,
    int cumulativeTicks[kStageCount + 1]) {

    if (clockMultiplier <= 0) clockMultiplier = 100;
    if (sequenceDivisor <= 0) sequenceDivisor = kReferenceSequenceDivisor;

    int durationTicks[kStageCount];
    cumulativeTicks[0] = 0;
    for (int i = 0; i < kStageCount; ++i) {
        int cell = int(kSnakeOrder[i]);
        int raw = skip[cell] ? 0 : stageDivisorTicksArr[cell];
        // Sequence divisor scales the whole cycle uniformly. Stage slot table
        // is calibrated at the 1/16 reference (= kReferenceSequenceDivisor);
        // raw * (sequenceDivisor / reference) gives the per-stage tick count.
        int scaledBySeq = (raw * sequenceDivisor) / kReferenceSequenceDivisor;
        // Per-stage length multiplier: factor = (100 + stageLen) / 100.
        // stageLen = -100 → factor 0; +100 → factor 2×. kMinCycleTicks floor
        // stretches all-zero cycles proportionally.
        // stageLen unipolar 0..127 mapped to factor 0..~2 via /64 (= Phaseque
        // STEP_LEN pattern). Stored 64 = factor 1 = transparent.
        int scaledByLen = (scaledBySeq * stageLenArr[cell]) / 64;
        // Apply clockMultiplier: 100=identity, 50=×2 length, 150=×2/3 length.
        int scaled = (scaledByLen * 100) / clockMultiplier;
        durationTicks[i] = scaled;
        cumulativeTicks[i + 1] = cumulativeTicks[i] + scaled;
    }

    int cycleTicks = cumulativeTicks[kStageCount];
    if (cycleTicks == 0) {
        return 0;
    }

    int kMinCycleTicks = measureDivisor / 32;
    if (kMinCycleTicks > 0 && cycleTicks < kMinCycleTicks) {
        // Proportional stretch — preserves relative cell weights.
        // Round each cumulative endpoint, then derive cycleTicks from the
        // final endpoint so the table stays consistent under rounding.
        for (int i = 1; i <= kStageCount; ++i) {
            cumulativeTicks[i] = int(
                (int64_t(cumulativeTicks[i]) * int64_t(kMinCycleTicks) + cycleTicks / 2)
                / cycleTicks);
        }
        cycleTicks = cumulativeTicks[kStageCount];
    }

    return cycleTicks;
}

bool PhaseFluxMath::deriveTickPosition(
    uint32_t relativeTick,
    const int cumulativeTicks[kStageCount + 1],
    int cycleTicks,
    int &slotIdx,
    int &activeCell,
    float &stagePhase) {

    if (cycleTicks <= 0) {
        return false;
    }

    uint32_t cycleTick = relativeTick % uint32_t(cycleTicks);

    // Linear scan suffices for 16 slots — clarity beats a binary search.
    int slot = 0;
    for (int i = 0; i < kStageCount; ++i) {
        if (int(cycleTick) >= cumulativeTicks[i] && int(cycleTick) < cumulativeTicks[i + 1]) {
            slot = i;
            break;
        }
    }
    slotIdx = slot;
    activeCell = int(kSnakeOrder[slot]);

    int slotTicks = cumulativeTicks[slot + 1] - cumulativeTicks[slot];
    if (slotTicks <= 0) {
        stagePhase = 0.0f;
    } else {
        int posInStage = int(cycleTick) - cumulativeTicks[slot];
        stagePhase = float(posInStage) / float(slotTicks);
    }
    return true;
}
