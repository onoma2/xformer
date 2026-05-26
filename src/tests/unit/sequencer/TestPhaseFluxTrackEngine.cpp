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
    expectEqual(PhaseFluxTrackEngine::kMaxPulses, 8, "kMaxPulses == 8");
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

} // UNIT_TEST("PhaseFluxTrackEngine")
