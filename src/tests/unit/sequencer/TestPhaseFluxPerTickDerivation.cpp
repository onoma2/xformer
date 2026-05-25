#include "UnitTest.h"

#include "apps/sequencer/model/PhaseFluxMath.h"

#include <cmath>
#include <cstdint>

static bool nearly(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) < eps;
}

// Build a uniform 16-stage table — each stage stageTicks long. cycle = 16*stage.
static void buildUniform(int cum[17], int stageTicks) {
    cum[0] = 0;
    for (int i = 1; i <= 16; ++i) cum[i] = cum[i - 1] + stageTicks;
}

UNIT_TEST("PhaseFluxPerTickDerivation") {

CASE("slot_zero_at_tick_zero") {
    int cum[17];
    buildUniform(cum, 12);

    int slot = -1, cell = -1;
    float phase = -1.0f;
    bool ok = PhaseFluxMath::deriveTickPosition(0, cum, cum[16], slot, cell, phase);

    expectTrue(ok, "deriveTickPosition must succeed for tick 0");
    expectEqual(slot, 0, "slotIdx 0 at tick 0");
    expectEqual(cell, int(PhaseFluxMath::snakeOrder()[0]), "cell 0 at slot 0");
    expectTrue(nearly(phase, 0.0f), "stagePhase 0 at tick 0");
}

CASE("slot_advance_at_boundary") {
    int cum[17];
    buildUniform(cum, 12);
    // tick 12 is exactly the start of slot 1.
    int slot = -1, cell = -1;
    float phase = -1.0f;
    bool ok = PhaseFluxMath::deriveTickPosition(12, cum, cum[16], slot, cell, phase);

    expectTrue(ok, "derivation succeeds");
    expectEqual(slot, 1, "slot advances on the boundary tick");
    expectTrue(nearly(phase, 0.0f), "stagePhase resets to 0 at new slot");
}

CASE("stage_phase_mid_slot") {
    int cum[17];
    buildUniform(cum, 12);
    int slot = -1, cell = -1;
    float phase = -1.0f;
    bool ok = PhaseFluxMath::deriveTickPosition(6, cum, cum[16], slot, cell, phase);

    expectTrue(ok, "derivation succeeds");
    expectEqual(slot, 0, "still in slot 0 at tick 6");
    expectTrue(nearly(phase, 0.5f), "stagePhase 0.5 halfway through 12-tick stage");
}

CASE("cycle_wraparound") {
    int cum[17];
    buildUniform(cum, 12);
    int cycle = cum[16];  // 192

    int slot = -1, cell = -1;
    float phase = -1.0f;
    // tick 200 mod 192 = 8 → slot 0 (0..12), phase 8/12 ≈ 0.667
    bool ok = PhaseFluxMath::deriveTickPosition(200, cum, cycle, slot, cell, phase);

    expectTrue(ok, "derivation succeeds");
    expectEqual(slot, 0, "wraps back to slot 0");
    expectTrue(nearly(phase, 8.0f / 12.0f), "phase 8/12 after wrap");
}

CASE("large_relative_tick") {
    int cum[17];
    buildUniform(cum, 12);
    int cycle = cum[16];

    int slot = -1, cell = -1;
    float phase = -1.0f;
    bool ok = PhaseFluxMath::deriveTickPosition(1000000u, cum, cycle, slot, cell, phase);

    expectTrue(ok, "derivation succeeds for large tick");
    expectTrue(slot >= 0 && slot < 16, "slotIdx within range");
    expectTrue(phase >= 0.0f && phase < 1.0f, "stagePhase within [0,1)");
    expectTrue(!std::isnan(phase) && std::isfinite(phase), "phase finite, no NaN");
}

CASE("idle_returns_false") {
    int cum[17] = { 0 };
    int slot = 99, cell = 99;
    float phase = 99.0f;
    bool ok = PhaseFluxMath::deriveTickPosition(42, cum, 0, slot, cell, phase);

    expectFalse(ok, "idle cycleTicks=0 returns false");
    expectEqual(slot, 99, "outputs left untouched in idle");
    expectEqual(cell, 99, "outputs left untouched in idle");
    expectTrue(phase == 99.0f, "phase untouched in idle");
}

CASE("snake_remap_active_cell") {
    int cum[17];
    buildUniform(cum, 10);

    const auto &snake = PhaseFluxMath::snakeOrder();
    // Sweep ticks at each slot midpoint and confirm activeCell follows snake.
    for (int i = 0; i < 16; ++i) {
        uint32_t tick = uint32_t(i * 10 + 5);
        int slot = -1, cell = -1;
        float phase = -1.0f;
        bool ok = PhaseFluxMath::deriveTickPosition(tick, cum, cum[16], slot, cell, phase);
        expectTrue(ok, "derivation succeeds for midpoint sweep");
        expectEqual(slot, i, "slot tracks i");
        expectEqual(cell, int(snake[i]), "activeCell = snakeOrder[slotIdx]");
    }
}

CASE("non_uniform_durations") {
    // Mixed pattern: alternating 12 and 24-tick stages.
    int cum[17];
    cum[0] = 0;
    for (int i = 0; i < 16; ++i) {
        int d = (i & 1) ? 24 : 12;
        cum[i + 1] = cum[i] + d;
    }
    int cycle = cum[16];

    int slot = -1, cell = -1;
    float phase = -1.0f;

    // tick at start of slot 2 should land in slot 2.
    bool ok = PhaseFluxMath::deriveTickPosition(uint32_t(cum[2]), cum, cycle, slot, cell, phase);
    expectTrue(ok, "derivation succeeds");
    expectEqual(slot, 2, "non-uniform: slot at cum[2] boundary");
    expectTrue(nearly(phase, 0.0f), "phase at boundary is 0");

    // tick at mid of slot 3 (24-tick stage): cum[3] + 12 → phase 0.5
    ok = PhaseFluxMath::deriveTickPosition(uint32_t(cum[3] + 12), cum, cycle, slot, cell, phase);
    expectTrue(ok, "derivation succeeds");
    expectEqual(slot, 3, "non-uniform: in slot 3");
    expectTrue(nearly(phase, 0.5f), "phase 0.5 halfway through 24-tick stage");
}

}
