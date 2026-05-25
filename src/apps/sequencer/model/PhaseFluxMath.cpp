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

// Seq-PPQN-48 ticks per StageDivisorSlot. Matches KnownDivisor table
// entries; if the slot enum is extended, extend this table in lockstep.
static const int kStageDivisorTicks[8] = {
     6,   // Div1_32
     8,   // Div1_16T
    12,   // Div1_16
    16,   // Div1_8T
    24,   // Div1_8
    32,   // Div1_4T
    48,   // Div1_4
    96,   // Div1_2
};

const std::array<uint8_t, PhaseFluxMath::kStageCount> &PhaseFluxMath::snakeOrder() {
    return kSnakeOrder;
}

int PhaseFluxMath::stageDivisorTicks(PhaseFluxSequence::StageDivisorSlot slot) {
    int idx = int(slot);
    if (idx < 0) idx = 0;
    if (idx > 7) idx = 7;
    return kStageDivisorTicks[idx];
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

int PhaseFluxMath::computeCumulativeTicks(
    const int stageDivisorTicksArr[kStageCount],
    const bool skip[kStageCount],
    int measureDivisor,
    int clockMultiplier,
    int cumulativeTicks[kStageCount + 1]) {

    if (clockMultiplier <= 0) clockMultiplier = 100;

    int durationTicks[kStageCount];
    cumulativeTicks[0] = 0;
    for (int i = 0; i < kStageCount; ++i) {
        int cell = int(kSnakeOrder[i]);
        int raw = skip[cell] ? 0 : stageDivisorTicksArr[cell];
        // Apply clockMultiplier: 100=identity, 50=×2 length, 150=×2/3 length.
        int scaled = (raw * 100) / clockMultiplier;
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
