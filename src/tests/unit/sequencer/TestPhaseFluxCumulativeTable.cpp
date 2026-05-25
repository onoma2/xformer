#include "UnitTest.h"

#include "apps/sequencer/model/PhaseFluxMath.h"

#include <array>
#include <cstdint>

using Slot = PhaseFluxSequence::StageDivisorSlot;

// Units throughout these tests are CONFIG_SEQUENCE_PPQN-48 ticks — same scale
// as the KnownDivisor table entries. measureDivisor=192 = 4/4 measure at
// PPQN-48, so kMinCycleTicks floor = 192/32 = 6.
static constexpr int kMeasureDivisor = 192;

static void fillSlot(int out[16], Slot slot) {
    int t = PhaseFluxMath::stageDivisorTicks(slot);
    for (int i = 0; i < 16; ++i) out[i] = t;
}

static void clearSkip(bool skip[16]) {
    for (int i = 0; i < 16; ++i) skip[i] = false;
}

UNIT_TEST("PhaseFluxCumulativeTable") {

CASE("all_default_divisor_no_skip") {
    int ticks[16];
    bool skip[16];
    fillSlot(ticks, Slot::Div1_16);
    clearSkip(skip);

    int cum[17];
    int cycleTicks = PhaseFluxMath::computeCumulativeTicks(ticks, skip, kMeasureDivisor, 100, cum);

    int expectedStage = 12;     // Div1_16 at seq-PPQN-48
    int expectedCycle = 16 * 12;

    expectEqual(cycleTicks, expectedCycle, "16x 1/16 must sum to 192 ticks (one measure)");
    expectEqual(cum[0], 0, "first cumulative is 0");
    for (int i = 0; i < 16; ++i) {
        expectEqual(cum[i + 1] - cum[i], expectedStage, "each stage contributes 12 ticks");
    }
}

CASE("skip_contributes_zero") {
    int ticks[16];
    bool skip[16];
    fillSlot(ticks, Slot::Div1_16);
    clearSkip(skip);
    skip[0] = true;
    skip[5] = true;
    skip[10] = true;

    int cum[17];
    int cycleTicks = PhaseFluxMath::computeCumulativeTicks(ticks, skip, kMeasureDivisor, 100, cum);

    int expectedCycle = 13 * 12;
    expectEqual(cycleTicks, expectedCycle, "3 skipped of 16 leaves 13 contributing stages");

    // The skip flags are indexed by cell ID. Look up which slot positions
    // hold cells 0/5/10 via the snake order — those slot deltas must be 0.
    const auto &snake = PhaseFluxMath::snakeOrder();
    for (int i = 0; i < 16; ++i) {
        int cell = int(snake[i]);
        int delta = cum[i + 1] - cum[i];
        if (cell == 0 || cell == 5 || cell == 10) {
            expectEqual(delta, 0, "skipped cells contribute 0 to cumulative");
        } else {
            expectEqual(delta, 12, "unskipped cells contribute their full divisor");
        }
    }
}

CASE("mixed_divisors_sum_correctly") {
    int ticks[16];
    bool skip[16];
    clearSkip(skip);
    // 4x Div1_16 (12), 4x Div1_8 (24), 4x Div1_4 (48), 4x Div1_2 (96)
    for (int i = 0; i < 4; ++i) {
        ticks[i] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_16);
        ticks[i + 4] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_8);
        ticks[i + 8] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_4);
        ticks[i + 12] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_2);
    }

    int cum[17];
    int cycleTicks = PhaseFluxMath::computeCumulativeTicks(ticks, skip, kMeasureDivisor, 100, cum);

    int expected = 4 * (12 + 24 + 48 + 96);
    expectEqual(cycleTicks, expected, "4*(12+24+48+96) = 720 ticks");
}

CASE("kMinCycleTicks_floor_engaged") {
    // Single contributing cell (cell 3) with clockMultiplier=150 makes raw
    // cycle = 6 * 100/150 = 4 ticks, below the floor of measureDivisor/32 = 6.
    // The §3.1 contract: scale BOTH cycleTicks AND every cumulativeTicks[i]
    // proportionally so per-tick derivation walks the stretched table.
    int ticks[16];
    bool skip[16];
    for (int i = 0; i < 16; ++i) {
        ticks[i] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_32);  // 6 ticks each
        skip[i] = true;
    }
    skip[3] = false;

    int cum[17];
    int cycleTicks = PhaseFluxMath::computeCumulativeTicks(ticks, skip, kMeasureDivisor, 150, cum);

    int floor = kMeasureDivisor / 32;
    expectEqual(cycleTicks, floor, "stretched cycle must hit the floor exactly");

    // Cell 3 sits at snake index 3, so cum[3]=0 and cum[4] holds its endpoint.
    // Pre-stretch cum[4] would be 4; post-stretch it must be the floor (6).
    // Catches a regression that sets cycleTicks=floor without scaling cum[].
    expectEqual(cum[3], 0, "pre-stretch cum at slot 3 stays at 0 (cells 0..2 skipped)");
    expectEqual(cum[4], floor, "cum[4] must scale up to the floor after stretch");
    expectEqual(cum[16], floor, "cum[16] must equal returned cycleTicks");
    for (int i = 5; i <= 15; ++i) {
        expectEqual(cum[i], floor, "post-cell-3 slots are skipped — cum stays at floor");
    }

    // At-floor case (clockMultiplier=100): cycle already equals floor, no scaling.
    int cum0[17];
    int cycleTicks0 = PhaseFluxMath::computeCumulativeTicks(ticks, skip, kMeasureDivisor, 100, cum0);
    expectEqual(cycleTicks0, floor, "at-floor cycle is left unchanged");
    expectEqual(cum0[4], floor, "at-floor cum[4] equals raw contribution");
}

CASE("all_skipped_returns_zero") {
    int ticks[16];
    bool skip[16];
    fillSlot(ticks, Slot::Div1_16);
    for (int i = 0; i < 16; ++i) skip[i] = true;

    int cum[17];
    int cycleTicks = PhaseFluxMath::computeCumulativeTicks(ticks, skip, kMeasureDivisor, 100, cum);

    expectEqual(cycleTicks, 0, "all-skipped must report cycleTicks=0 (idle)");
    for (int i = 0; i <= 16; ++i) {
        expectEqual(cum[i], 0, "all-skipped cumulative entries are 0");
    }
}

CASE("snake_order_applied") {
    int ticks[16];
    bool skip[16];
    clearSkip(skip);
    // Distinct divisor per cell so the snake permutation is visible.
    // Cell 0 = Div1_2 (96), cell 4 = Div1_16 (12) — at slot 7 (row 1 R->L
    // hits cell 4 last in that row), so cum[8]-cum[7] should be 12, not 96.
    for (int i = 0; i < 16; ++i) {
        ticks[i] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_8);  // baseline 24
    }
    ticks[0] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_2);   // 96
    ticks[4] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_16);  // 12

    int cum[17];
    int cycleTicks = PhaseFluxMath::computeCumulativeTicks(ticks, skip, kMeasureDivisor, 100, cum);

    const auto &snake = PhaseFluxMath::snakeOrder();
    for (int i = 0; i < 16; ++i) {
        int cell = int(snake[i]);
        int delta = cum[i + 1] - cum[i];
        int expectedTicks = ticks[cell];
        expectEqual(delta, expectedTicks, "slot delta must match the cell at snake position");
    }
    // Sanity: sum matches expected sum.
    int expectedSum = 14 * 24 + 96 + 12;
    expectEqual(cycleTicks, expectedSum, "sum equals 14x24 + 96 + 12");
}

CASE("clock_multiplier_50_doubles") {
    int ticks[16];
    bool skip[16];
    fillSlot(ticks, Slot::Div1_16);
    clearSkip(skip);

    int cum100[17];
    int cum50[17];
    int cycle100 = PhaseFluxMath::computeCumulativeTicks(ticks, skip, kMeasureDivisor, 100, cum100);
    int cycle50  = PhaseFluxMath::computeCumulativeTicks(ticks, skip, kMeasureDivisor, 50,  cum50);

    expectEqual(cycle50, cycle100 * 2, "clockMultiplier 50 doubles cycle length");
}

CASE("clock_multiplier_150_shrinks") {
    int ticks[16];
    bool skip[16];
    // Use Div1_4 (48 ticks) so 100/150 = 2/3 lands on a clean integer (32).
    fillSlot(ticks, Slot::Div1_4);
    clearSkip(skip);

    int cum[17];
    int cycleTicks = PhaseFluxMath::computeCumulativeTicks(ticks, skip, kMeasureDivisor, 150, cum);

    int expected = 16 * ((48 * 100) / 150);  // = 16 * 32 = 512
    expectEqual(cycleTicks, expected, "clockMultiplier 150 shrinks to ~2/3");
}

CASE("each_slot_value_used_at_least_once") {
    // Sanity sweep across all 8 slots; ensures stageDivisorTicks covers the
    // enum range and no slot crashes the math.
    Slot slots[8] = {
        Slot::Div1_32, Slot::Div1_16T, Slot::Div1_16, Slot::Div1_8T,
        Slot::Div1_8,  Slot::Div1_4T,  Slot::Div1_4,  Slot::Div1_2,
    };
    int expected[8] = { 6, 8, 12, 16, 24, 32, 48, 96 };
    for (int i = 0; i < 8; ++i) {
        expectEqual(PhaseFluxMath::stageDivisorTicks(slots[i]), expected[i],
                    "stageDivisorTicks slot mapping must match KnownDivisor");
    }
}

}
