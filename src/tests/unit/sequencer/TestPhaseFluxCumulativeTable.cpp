#include "UnitTest.h"

#include "apps/sequencer/model/PhaseFluxMath.h"

#include <array>
#include <cstdint>

using Slot = PhaseFluxSequence::StageDivisorSlot;

// Units throughout these tests are CONFIG_PPQN-192 (master) ticks — same scale
// as the kStageDivisorTicks table entries. measureDivisor=768 = 4/4 measure at
// master-PPQN-192, so kMinCycleTicks floor = 768/32 = 24.
static constexpr int kMeasureDivisor = 768;

static void fillSlot(int out[16], Slot slot) {
    int t = PhaseFluxMath::stageDivisorTicks(slot);
    for (int i = 0; i < 16; ++i) out[i] = t;
}

static void clearSkip(bool skip[16]) {
    for (int i = 0; i < 16; ++i) skip[i] = false;
}

// New stageLen encoding is unipolar 0..127 with 64 = ×1 transparent default,
// so a "transparent" helper sets all stages to 64 (the no-effect value).
static void clearLens(int lens[16]) {
    for (int i = 0; i < 16; ++i) lens[i] = 64;
}

UNIT_TEST("PhaseFluxCumulativeTable") {

CASE("all_default_divisor_no_skip") {
    int ticks[16];
    bool skip[16];
    int lens[16];
    fillSlot(ticks, Slot::Div1_16);
    clearSkip(skip);
    clearLens(lens);

    int cum[17];
    int cycleTicks = PhaseFluxMath::computeCumulativeTicks(PhaseFluxMath::snakeOrder(), ticks, lens, skip, 12, kMeasureDivisor, 100, cum);

    int expectedStage = 48;     // Div1_16 at master-PPQN-192
    int expectedCycle = 16 * 48;

    expectEqual(cycleTicks, expectedCycle, "16x 1/16 must sum to 768 ticks (one measure)");
    expectEqual(cum[0], 0, "first cumulative is 0");
    for (int i = 0; i < 16; ++i) {
        expectEqual(cum[i + 1] - cum[i], expectedStage, "each stage contributes 48 ticks");
    }
}

CASE("skip_contributes_zero") {
    int ticks[16];
    bool skip[16];
    int lens[16];
    fillSlot(ticks, Slot::Div1_16);
    clearSkip(skip);
    clearLens(lens);
    skip[0] = true;
    skip[5] = true;
    skip[10] = true;

    int cum[17];
    int cycleTicks = PhaseFluxMath::computeCumulativeTicks(PhaseFluxMath::snakeOrder(), ticks, lens, skip, 12, kMeasureDivisor, 100, cum);

    int expectedCycle = 13 * 48;
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
            expectEqual(delta, 48, "unskipped cells contribute their full divisor");
        }
    }
}

CASE("mixed_divisors_sum_correctly") {
    int ticks[16];
    bool skip[16];
    int lens[16];
    clearSkip(skip);
    clearLens(lens);
    // 4x Div1_16 (48), 4x Div1_8 (96), 4x Div1_4 (192), 4x Div1_2 (384)
    for (int i = 0; i < 4; ++i) {
        ticks[i] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_16);
        ticks[i + 4] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_8);
        ticks[i + 8] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_4);
        ticks[i + 12] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_2);
    }

    int cum[17];
    int cycleTicks = PhaseFluxMath::computeCumulativeTicks(PhaseFluxMath::snakeOrder(), ticks, lens, skip, 12, kMeasureDivisor, 100, cum);

    int expected = 4 * (48 + 96 + 192 + 384);
    expectEqual(cycleTicks, expected, "4*(48+96+192+384) = 2880 ticks");
}

CASE("kMinCycleTicks_floor_engaged") {
    // Single contributing cell (cell 3) with clockMultiplier=150 makes raw
    // cycle = 24 * 100/150 = 16 ticks, below the floor of measureDivisor/32 = 24.
    // The §3.1 contract: scale BOTH cycleTicks AND every cumulativeTicks[i]
    // proportionally so per-tick derivation walks the stretched table.
    int ticks[16];
    bool skip[16];
    int lens[16];
    clearLens(lens);
    for (int i = 0; i < 16; ++i) {
        ticks[i] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_32);  // 24 ticks each
        skip[i] = true;
    }
    skip[3] = false;

    int cum[17];
    int cycleTicks = PhaseFluxMath::computeCumulativeTicks(PhaseFluxMath::snakeOrder(), ticks, lens, skip, 12, kMeasureDivisor, 150, cum);

    int floor = kMeasureDivisor / 32;
    expectEqual(cycleTicks, floor, "stretched cycle must hit the floor exactly");

    // Cell 3 sits at snake index 3, so cum[3]=0 and cum[4] holds its endpoint.
    // Pre-stretch cum[4] would be 16; post-stretch it must be the floor (24).
    // Catches a regression that sets cycleTicks=floor without scaling cum[].
    expectEqual(cum[3], 0, "pre-stretch cum at slot 3 stays at 0 (cells 0..2 skipped)");
    expectEqual(cum[4], floor, "cum[4] must scale up to the floor after stretch");
    expectEqual(cum[16], floor, "cum[16] must equal returned cycleTicks");
    for (int i = 5; i <= 15; ++i) {
        expectEqual(cum[i], floor, "post-cell-3 slots are skipped — cum stays at floor");
    }

    // At-floor case (clockMultiplier=100): cycle already equals floor, no scaling.
    int cum0[17];
    int cycleTicks0 = PhaseFluxMath::computeCumulativeTicks(PhaseFluxMath::snakeOrder(), ticks, lens, skip, 12, kMeasureDivisor, 100, cum0);
    expectEqual(cycleTicks0, floor, "at-floor cycle is left unchanged");
    expectEqual(cum0[4], floor, "at-floor cum[4] equals raw contribution");
}

CASE("all_skipped_returns_zero") {
    int ticks[16];
    bool skip[16];
    int lens[16];
    fillSlot(ticks, Slot::Div1_16);
    clearLens(lens);
    for (int i = 0; i < 16; ++i) skip[i] = true;

    int cum[17];
    int cycleTicks = PhaseFluxMath::computeCumulativeTicks(PhaseFluxMath::snakeOrder(), ticks, lens, skip, 12, kMeasureDivisor, 100, cum);

    expectEqual(cycleTicks, 0, "all-skipped must report cycleTicks=0 (idle)");
    for (int i = 0; i <= 16; ++i) {
        expectEqual(cum[i], 0, "all-skipped cumulative entries are 0");
    }
}

CASE("snake_order_applied") {
    int ticks[16];
    bool skip[16];
    int lens[16];
    clearSkip(skip);
    clearLens(lens);
    // Distinct divisor per cell so the snake permutation is visible.
    // Cell 0 = Div1_2 (384), cell 4 = Div1_16 (48) — at slot 7 (row 1 R->L
    // hits cell 4 last in that row), so cum[8]-cum[7] should be 48, not 384.
    for (int i = 0; i < 16; ++i) {
        ticks[i] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_8);  // baseline 96
    }
    ticks[0] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_2);   // 384
    ticks[4] = PhaseFluxMath::stageDivisorTicks(Slot::Div1_16);  // 48

    int cum[17];
    int cycleTicks = PhaseFluxMath::computeCumulativeTicks(PhaseFluxMath::snakeOrder(), ticks, lens, skip, 12, kMeasureDivisor, 100, cum);

    const auto &snake = PhaseFluxMath::snakeOrder();
    for (int i = 0; i < 16; ++i) {
        int cell = int(snake[i]);
        int delta = cum[i + 1] - cum[i];
        int expectedTicks = ticks[cell];
        expectEqual(delta, expectedTicks, "slot delta must match the cell at snake position");
    }
    // Sanity: sum matches expected sum.
    int expectedSum = 14 * 96 + 384 + 48;
    expectEqual(cycleTicks, expectedSum, "sum equals 14x96 + 384 + 48");
}

CASE("clock_multiplier_50_doubles") {
    int ticks[16];
    bool skip[16];
    int lens[16];
    fillSlot(ticks, Slot::Div1_16);
    clearSkip(skip);
    clearLens(lens);

    int cum100[17];
    int cum50[17];
    int cycle100 = PhaseFluxMath::computeCumulativeTicks(PhaseFluxMath::snakeOrder(), ticks, lens, skip, 12, kMeasureDivisor, 100, cum100);
    int cycle50  = PhaseFluxMath::computeCumulativeTicks(PhaseFluxMath::snakeOrder(), ticks, lens, skip, 12, kMeasureDivisor, 50,  cum50);

    expectEqual(cycle50, cycle100 * 2, "clockMultiplier 50 doubles cycle length");
}

CASE("clock_multiplier_150_shrinks") {
    int ticks[16];
    bool skip[16];
    int lens[16];
    // Use Div1_4 (192 ticks) so 100/150 = 2/3 lands on a clean integer (128).
    fillSlot(ticks, Slot::Div1_4);
    clearSkip(skip);
    clearLens(lens);

    int cum[17];
    int cycleTicks = PhaseFluxMath::computeCumulativeTicks(PhaseFluxMath::snakeOrder(), ticks, lens, skip, 12, kMeasureDivisor, 150, cum);

    int expected = 16 * ((192 * 100) / 150);  // = 16 * 128 = 2048
    expectEqual(cycleTicks, expected, "clockMultiplier 150 shrinks to ~2/3");
}

CASE("sequence_divisor_scales_cycle_uniformly") {
    // Stage table is calibrated at divisor=12 (1/16 reference). Divisor=24
    // (1/8) should double cycle length; divisor=6 (1/32) should halve it.
    int ticks[16];
    bool skip[16];
    int lens[16];
    fillSlot(ticks, Slot::Div1_16);
    clearSkip(skip);
    clearLens(lens);

    int cum12[17], cum24[17], cum6[17];
    int cyc12 = PhaseFluxMath::computeCumulativeTicks(PhaseFluxMath::snakeOrder(), ticks, lens, skip, 12, kMeasureDivisor, 100, cum12);
    int cyc24 = PhaseFluxMath::computeCumulativeTicks(PhaseFluxMath::snakeOrder(), ticks, lens, skip, 24, kMeasureDivisor, 100, cum24);
    int cyc6  = PhaseFluxMath::computeCumulativeTicks(PhaseFluxMath::snakeOrder(), ticks, lens, skip,  6, kMeasureDivisor, 100, cum6);

    expectEqual(cyc12, 768, "divisor=12 (default 1/16) keeps cycle at 768");
    expectEqual(cyc24, 1536, "divisor=24 (1/8) doubles cycle to 1536");
    expectEqual(cyc6,   384, "divisor=6 (1/32) halves cycle to 384");

    for (int i = 0; i < 16; ++i) {
        expectEqual(cum24[i + 1] - cum24[i], 96, "divisor=24 stage delta = 96 (was 48)");
        expectEqual(cum6[i + 1] - cum6[i],   24, "divisor=6 stage delta = 24 (was 48)");
    }
}

CASE("stage_len_scales_per_stage") {
    // New unipolar encoding: stored 64 = ×1 transparent, 0 = silent, 127 ≈ ×2.
    // Math: effectiveTicks = enumTicks × stageLen / 64.
    int ticks[16]; bool skip[16]; int lens[16];
    fillSlot(ticks, Slot::Div1_16);
    clearSkip(skip);
    clearLens(lens);                // all stages transparent (64)
    lens[0] = 96;                   // 48 × 96 / 64 = 72   (1.5×)
    lens[1] = 32;                   // 48 × 32 / 64 = 24   (0.5×)
    lens[2] = 0;                    // 48 × 0  / 64 = 0    (silent)
    int cum[17];
    int cyc = PhaseFluxMath::computeCumulativeTicks(PhaseFluxMath::snakeOrder(), ticks, lens, skip, 12, kMeasureDivisor, 100, cum);

    expectEqual(cum[1] - cum[0], 72, "stageLen=96 stretches Div1_16 (48 ticks) to 72 (1.5×)");
    expectEqual(cum[2] - cum[1], 24, "stageLen=32 shrinks Div1_16 to 24 (0.5×)");
    expectEqual(cum[3] - cum[2],  0, "stageLen=0 silences the stage (0 ticks)");
    expectEqual(cyc, 72 + 24 + 0 + 13 * 48, "cycle = sum of scaled per-stage durations");
}

CASE("each_slot_value_used_at_least_once") {
    // Sanity sweep across all 8 slots; ensures stageDivisorTicks covers the
    // enum range and no slot crashes the math.
    Slot slots[8] = {
        Slot::Div1_32, Slot::Div1_16T, Slot::Div1_16, Slot::Div1_8T,
        Slot::Div1_8,  Slot::Div1_4T,  Slot::Div1_4,  Slot::Div1_2,
    };
    int expected[8] = { 24, 32, 48, 64, 96, 128, 192, 384 };
    for (int i = 0; i < 8; ++i) {
        expectEqual(PhaseFluxMath::stageDivisorTicks(slots[i]), expected[i],
                    "stageDivisorTicks slot mapping must match KnownDivisor");
    }
}

}
