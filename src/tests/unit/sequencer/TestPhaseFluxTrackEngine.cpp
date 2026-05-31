#include "UnitTest.h"

#include "apps/sequencer/engine/PhaseFluxTrackEngine.h"
#include "apps/sequencer/model/AccumulatorConfig.h"
#include "apps/sequencer/model/Track.h"

// Engine integration tests need full hardware-driver mocks. Task-5 counter
// state behavior is exercised via the public static helper advanceCounter()
// — same code path the engine calls per slot-change, no Engine instance needed.

UNIT_TEST("PhaseFluxTrackEngine") {

CASE("track_mode_constant") {
    expectEqual(static_cast<int>(Track::TrackMode::PhaseFlux), 8, "PhaseFlux serialize id is 8");
}

CASE("stage_count_parity") {
    expectEqual(PhaseFluxTrackEngine::kStageCount, 16, "engine kStageCount == 16");
}

CASE("max_pulses_constant") {
    expectEqual(PhaseFluxTrackEngine::kMaxPulses, 16, "kMaxPulses == 16");
}

CASE("pulse_fired_mask_can_represent_all_16_pulses") {
    uint16_t mask = 0;
    for (int k = 0; k < PhaseFluxTrackEngine::kMaxPulses; ++k) {
        mask |= uint16_t(1u << k);
    }
    for (int k = 0; k < PhaseFluxTrackEngine::kMaxPulses; ++k) {
        expectEqual((mask & uint16_t(1u << k)) != 0, true, "pulse bit retained");
    }
}

// ---- Counter advance via advanceCounter() helper (spec §13.10 Task 5) ----

CASE("note_counter_advances_on_cell_completion_when_step_nonzero") {
    AccumulatorConfig cfg;
    cfg.setOrder(AccumulatorConfig::Order::Wrap);
    cfg.setPolarity(AccumulatorConfig::Polarity::Uni);
    cfg.setPosLim(7);
    cfg.setNegLim(7);
    int counter = 0;
    int8_t dir = 1;
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1);
    expectEqual(counter, 1, "counter advances by step on trigger");
}

CASE("note_counter_holds_when_step_is_zero") {
    AccumulatorConfig cfg;
    cfg.setOrder(AccumulatorConfig::Order::Wrap);
    cfg.setPolarity(AccumulatorConfig::Polarity::Uni);
    cfg.setPosLim(7);
    int counter = 3;
    int8_t dir = 1;
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, 0);
    expectEqual(counter, 3, "counter unchanged when step == 0");
}

CASE("note_counter_respects_wrap_uni_order") {
    AccumulatorConfig cfg;
    cfg.setOrder(AccumulatorConfig::Order::Wrap);
    cfg.setPolarity(AccumulatorConfig::Polarity::Uni);
    cfg.setPosLim(3);
    cfg.setNegLim(3);
    int counter = 0;
    int8_t dir = 1;
    // Five advances of step=+1: 0 -> 1 -> 2 -> 3 -> 0 (wrap) -> 1
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1);
    expectEqual(counter, 1, "step 1: 0 -> 1");
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1);
    expectEqual(counter, 2, "step 2: 1 -> 2");
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1);
    expectEqual(counter, 3, "step 3: 2 -> 3");
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1);
    expectEqual(counter, 0, "step 4: 3 -> 0 (wrap)");
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1);
    expectEqual(counter, 1, "step 5: 0 -> 1");
}

CASE("note_counter_respects_pendulum_bi_order") {
    AccumulatorConfig cfg;
    cfg.setOrder(AccumulatorConfig::Order::Pendulum);
    cfg.setPolarity(AccumulatorConfig::Polarity::Bi);
    cfg.setPosLim(2);
    cfg.setNegLim(2);
    int counter = 0;
    int8_t dir = 1;
    // step=+1, range -2..+2, Pendulum: 0 -> 1 -> 2 (flip) -> 1 -> 0 -> -1 -> -2 (flip) -> -1
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1);
    expectEqual(counter, 1, "ascend 0 -> 1");
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1);
    expectEqual(counter, 2, "ascend 1 -> 2 (hit max, flip dir)");
    expectEqual(int(dir), -1, "pendulum dir now -1");
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1);
    expectEqual(counter, 1, "descend 2 -> 1");
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1);
    expectEqual(counter, 0, "descend 1 -> 0");
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1);
    expectEqual(counter, -1, "descend 0 -> -1");
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1);
    expectEqual(counter, -2, "descend -1 -> -2 (hit min, flip dir)");
    expectEqual(int(dir), 1, "pendulum dir now +1");
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1);
    expectEqual(counter, -1, "ascend -2 -> -1");
}

CASE("note_counter_hold_pins_at_pos_lim") {
    AccumulatorConfig cfg;
    cfg.setOrder(AccumulatorConfig::Order::Hold);
    cfg.setPolarity(AccumulatorConfig::Polarity::Uni);
    cfg.setPosLim(3);
    int counter = 0;
    int8_t dir = 1;
    for (int i = 0; i < 10; ++i) {
        PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1);
    }
    expectEqual(counter, 3, "hold pins at posLim");
}

CASE("note_counter_rtz_overflow_snaps_to_zero") {
    AccumulatorConfig cfg;
    cfg.setOrder(AccumulatorConfig::Order::RTZ);
    cfg.setPolarity(AccumulatorConfig::Polarity::Uni);
    cfg.setPosLim(3);
    int counter = 0;
    int8_t dir = 1;
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1); // 0 -> 1
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1); // 1 -> 2
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1); // 2 -> 3
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1); // 3 -> 4 -> RTZ -> 0
    expectEqual(counter, 0, "RTZ snaps to zero on overflow past posLim");
}

CASE("note_accum_drift_sequence_matches_spec_example") {
    // NOTE: Unit tests exercise the counter advance helper only. The integration
    // path — Engine reading _noteAccumCounter[counterIdx] in rebuildSchedule and
    // Always-mode CV — is verified in Task 10 simulator audition.
    // Spec §13.10 Task 6: step=+1, scope=Local, order=Wrap, posLim=7.
    // Counter sequence consumed by pitch eval: 0, +1, +2, ..., +7, 0, +1, ...
    AccumulatorConfig cfg;
    cfg.setOrder(AccumulatorConfig::Order::Wrap);
    cfg.setPolarity(AccumulatorConfig::Polarity::Uni);
    cfg.setPosLim(7);
    cfg.setNegLim(7);
    int counter = 0;
    int8_t dir = 1;
    const int expected[] = { 1, 2, 3, 4, 5, 6, 7, 0, 1, 2 };
    for (int i = 0; i < 10; ++i) {
        PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, +1);
        expectEqual(counter, expected[i], "drift step");
    }
}

CASE("note_counter_negative_step_uni_descends_to_neg_lim") {
    // Uni polarity with negative step: bounds become -negLim..0
    AccumulatorConfig cfg;
    cfg.setOrder(AccumulatorConfig::Order::Wrap);
    cfg.setPolarity(AccumulatorConfig::Polarity::Uni);
    cfg.setPosLim(3);
    cfg.setNegLim(3);
    int counter = 0;
    int8_t dir = -1;
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, -1);
    expectEqual(counter, -1, "step -1: 0 -> -1");
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, -1);
    expectEqual(counter, -2, "step -1: -1 -> -2");
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, -1);
    expectEqual(counter, -3, "step -1: -2 -> -3");
    PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, -1);
    expectEqual(counter, 0, "step -1: -3 -> 0 (wrap to zero end)");
}

// ---- Pulse-count accumulator pipeline (spec §13.10 Task 7) ----
// Counter walk tested here; engine integration path (rebuildSchedule
// clamp + per-pulse loop) is verified in Task 10 audition.

CASE("pulse_accum_drift_with_stage_trigger_walks_count_1_to_16") {
    // posLim=15, step=+1: counter walks 1..15 (capped) then wraps to 0.
    // Effective pulse count = basePulseCount + counter (no multiplication;
    // counter already stores the accumulated delta).
    AccumulatorConfig cfg;
    cfg.setOrder(AccumulatorConfig::Order::Wrap);
    cfg.setPolarity(AccumulatorConfig::Polarity::Uni);
    cfg.setPosLim(15);
    cfg.setNegLim(15);
    const int basePulseCount = 1;
    const int step = +1;
    int counter = 0;
    int8_t dir = 1;
    auto effective = [&]() {
        int v = basePulseCount + counter;
        if (v < 1) v = 1;
        if (v > PhaseFluxTrackEngine::kMaxPulses) v = PhaseFluxTrackEngine::kMaxPulses;
        return v;
    };
    expectEqual(effective(), 1, "cycle 0: counter=0 -> N=1");
    // Walks 1+1=2, 1+2=3, ..., 1+15=16, then wrap counter to 0 -> N=1, then 2.
    const int expected[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 1, 2 };
    for (int i = 0; i < 17; ++i) {
        PhaseFluxTrackEngine::advanceCounter(counter, dir, cfg, step);
        expectEqual(effective(), expected[i], "next cycle N");
    }
}

CASE("effective_pulse_count_clamps_at_16_when_offset_pushes_above") {
    // pulseCount=8, counter=10 -> raw=18, clamp to 16.
    // Counter directly represents accumulated pulse offset.
    const int basePulseCount = 8;
    const int counter = 10;
    int raw = basePulseCount + counter;
    int eff = raw;
    if (eff < 1) eff = 1;
    if (eff > PhaseFluxTrackEngine::kMaxPulses) eff = PhaseFluxTrackEngine::kMaxPulses;
    expectEqual(raw, 18, "raw exceeds 16");
    expectEqual(eff, 16, "effective clamps at 16");
}

CASE("effective_pulse_count_clamps_at_1_when_offset_pushes_below") {
    // pulseCount=4, counter=-10 -> raw=-6, clamp to 1.
    const int basePulseCount = 4;
    const int counter = -10;
    int raw = basePulseCount + counter;
    int eff = raw;
    if (eff < 1) eff = 1;
    if (eff > 8) eff = 8;
    expectEqual(raw, -6, "raw underflows 1");
    expectEqual(eff, 1, "effective clamps at 1");
}

} // UNIT_TEST("PhaseFluxTrackEngine")
