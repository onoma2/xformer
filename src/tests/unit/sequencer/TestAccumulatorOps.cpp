#include "UnitTest.h"

#include "apps/sequencer/engine/AccumulatorOps.h"
#include "apps/sequencer/model/Accumulator.h"

// Unified convention: every op takes (direction +1/-1/0, magnitude) with
// direction == 0 a first-class no-op. Pendulum travels by the caller-owned
// pendulumDir (direction is its freeze gate); wrap/hold apply direction to an
// unsigned magnitude; RTZ advances by direction*magnitude then snaps.

UNIT_TEST("AccumulatorOps") {

// ---- Wrap ----

CASE("tick_wrap_overflow_returns_to_min_when_direction_up") {
    int counter = 7;
    AccumulatorOps::tickWrap(counter, +1, 1, 0, 7);
    expectEqual(counter, 0, "should wrap to min");
}

CASE("tick_wrap_underflow_returns_to_max_when_direction_down") {
    int counter = 0;
    AccumulatorOps::tickWrap(counter, -1, 1, 0, 7);
    expectEqual(counter, 7, "should wrap to max");
}

CASE("tick_wrap_bipolar_overflow_wraps_to_neg_lim") {
    int counter = 4;
    AccumulatorOps::tickWrap(counter, +1, 1, -4, 4);
    expectEqual(counter, -4, "bipolar overflow wraps to opposite limit");
}

CASE("tick_wrap_bipolar_underflow_wraps_to_pos_lim") {
    int counter = -4;
    AccumulatorOps::tickWrap(counter, -1, 1, -4, 4);
    expectEqual(counter, 4, "bipolar underflow wraps to opposite limit");
}

CASE("tick_wrap_step_within_range_single_overshoot") {
    // step <= range: lands exactly where the old single-subtract did.
    int counter = 6;
    AccumulatorOps::tickWrap(counter, +1, 3, 0, 7);
    expectEqual(counter, 1, "9 wraps to 1");
}

CASE("tick_wrap_step_exceeds_range_stays_in_bounds") {
    // step > range (range 3): modulo keeps it in [min,max] (the corrected case).
    int counter = 2;
    AccumulatorOps::tickWrap(counter, +1, 7, 0, 2);
    expectTrue(counter >= 0 && counter <= 2, "overshoot beyond range stays in bounds");
}

// ---- Pendulum ----

CASE("tick_pendulum_overflow_reflects_at_max_and_flips_dir") {
    int counter = 6;
    int pendulumDir = 1;
    AccumulatorOps::tickPendulum(counter, pendulumDir, +1, 2, 0, 7);
    expectEqual(counter, 7, "should clamp to max on overflow");
    expectEqual(pendulumDir, -1, "should flip direction down");
}

CASE("tick_pendulum_underflow_reflects_at_min_and_flips_dir") {
    int counter = 1;
    int pendulumDir = -1;
    AccumulatorOps::tickPendulum(counter, pendulumDir, +1, 2, 0, 7);
    expectEqual(counter, 0, "should clamp to min on underflow");
    expectEqual(pendulumDir, 1, "should flip direction up");
}

CASE("tick_pendulum_full_cycle_ascend_descend") {
    int counter = 0;
    int pendulumDir = 1;
    int mn = 0, mx = 3, mag = 1;
    AccumulatorOps::tickPendulum(counter, pendulumDir, +1, mag, mn, mx);
    expectEqual(counter, 1, "ascend to 1");
    AccumulatorOps::tickPendulum(counter, pendulumDir, +1, mag, mn, mx);
    expectEqual(counter, 2, "ascend to 2");
    AccumulatorOps::tickPendulum(counter, pendulumDir, +1, mag, mn, mx);
    expectEqual(counter, 3, "ascend to 3 -> flip");
    expectEqual(pendulumDir, -1, "flipped down at max");
    AccumulatorOps::tickPendulum(counter, pendulumDir, +1, mag, mn, mx);
    expectEqual(counter, 2, "descend to 2");
    AccumulatorOps::tickPendulum(counter, pendulumDir, +1, mag, mn, mx);
    expectEqual(counter, 1, "descend to 1");
    AccumulatorOps::tickPendulum(counter, pendulumDir, +1, mag, mn, mx);
    expectEqual(counter, 0, "descend to 0 -> flip");
    expectEqual(pendulumDir, 1, "flipped up at min");
}

CASE("tick_pendulum_down_drift_via_seeded_dir") {
    // Caller seeds pendulumDir = -1 to start descending (PhaseFlux pattern).
    int counter = 3;
    int pendulumDir = -1;
    int mn = 0, mx = 5, mag = 1;
    AccumulatorOps::tickPendulum(counter, pendulumDir, -1, mag, mn, mx);
    expectEqual(counter, 2, "descends first");
    AccumulatorOps::tickPendulum(counter, pendulumDir, -1, mag, mn, mx);
    expectEqual(counter, 1, "descends");
    AccumulatorOps::tickPendulum(counter, pendulumDir, -1, mag, mn, mx);
    expectEqual(counter, 0, "hits min -> flip");
    expectEqual(pendulumDir, 1, "flipped up at min");
}

CASE("tick_pendulum_clamp_to_exact_bounds") {
    int counter = 5;
    int pendulumDir = 1;
    AccumulatorOps::tickPendulum(counter, pendulumDir, +1, 3, 0, 7);
    expectEqual(counter, 7, "clamped to exact max");
    expectEqual(pendulumDir, -1, "flipped");
}

// ---- Hold ----

CASE("tick_hold_overflow_pins_at_max") {
    int counter = 6;
    AccumulatorOps::tickHold(counter, +1, 3, 0, 7);
    expectEqual(counter, 7, "should pin at max on overflow");
    AccumulatorOps::tickHold(counter, +1, 3, 0, 7);
    expectEqual(counter, 7, "should remain pinned at max");
}

CASE("tick_hold_underflow_pins_at_min") {
    int counter = 1;
    AccumulatorOps::tickHold(counter, -1, 3, 0, 7);
    expectEqual(counter, 0, "should pin at min on underflow");
    AccumulatorOps::tickHold(counter, -1, 3, 0, 7);
    expectEqual(counter, 0, "should remain pinned at min");
}

// ---- Random ----

CASE("tick_random_stays_in_range") {
    int counter = 0;
    for (int i = 0; i < 200; ++i) {
        AccumulatorOps::tickRandom(counter, +1, -3, 5);
        expectTrue(counter >= -3 && counter <= 5, "random must stay in [min, max]");
    }
}

CASE("tick_random_min_equals_max_is_constant") {
    int counter = 99;
    AccumulatorOps::tickRandom(counter, +1, 4, 4);
    expectEqual(counter, 4, "should set to that single value");
}

// ---- RTZ (advance + snap) ----

CASE("tick_rtz_overflow_snaps_to_zero") {
    int counter = 7;
    AccumulatorOps::tickRTZ(counter, +1, 1, 0, 7);
    expectEqual(counter, 0, "advance past max snaps to zero");
}

CASE("tick_rtz_underflow_snaps_to_zero") {
    int counter = -7;
    AccumulatorOps::tickRTZ(counter, -1, 1, -7, 0);
    expectEqual(counter, 0, "advance past min snaps to zero");
}

CASE("tick_rtz_in_range_advances") {
    int counter = 3;
    AccumulatorOps::tickRTZ(counter, +1, 1, 0, 7);
    expectEqual(counter, 4, "in-range advance, no snap");
}

// ---- Freeze (direction == 0) is a no-op for every op ----

CASE("freeze_direction_is_noop_for_all_ops") {
    int counter = 3;
    AccumulatorOps::tickWrap(counter, 0, 1, 0, 7);
    expectEqual(counter, 3, "wrap noop");
    AccumulatorOps::tickHold(counter, 0, 1, 0, 7);
    expectEqual(counter, 3, "hold noop");
    AccumulatorOps::tickRandom(counter, 0, 0, 7);
    expectEqual(counter, 3, "random noop");
    AccumulatorOps::tickRTZ(counter, 0, 1, 0, 7);
    expectEqual(counter, 3, "rtz noop");
    int pendulumDir = 1;
    AccumulatorOps::tickPendulum(counter, pendulumDir, 0, 1, 0, 7);
    expectEqual(counter, 3, "pendulum noop");
    expectEqual(pendulumDir, 1, "pendulum dir unchanged on freeze");
}

// ---- Faithfulness vs the NoteTrack Accumulator class ----

CASE("extracted_wrap_matches_notetrack_wrap") {
    Accumulator acc;
    acc.setMinValue(0); acc.setMaxValue(7); acc.setStepValue(1);
    acc.setOrder(Accumulator::Order::Wrap);
    acc.setDirection(Accumulator::Direction::Up);
    acc.setEnabled(true);
    acc.tick(); // first-tick delay
    int classSeq[10];
    for (int i = 0; i < 10; ++i) { acc.tick(); classSeq[i] = int(acc.currentValue()); }

    int counter = 0;
    for (int i = 0; i < 10; ++i) {
        AccumulatorOps::tickWrap(counter, +1, 1, 0, 7);
        expectEqual(counter, classSeq[i], "free wrap matches class");
    }
}

CASE("extracted_pendulum_matches_notetrack_pendulum") {
    Accumulator acc;
    acc.setMinValue(0); acc.setMaxValue(3); acc.setStepValue(1);
    acc.setOrder(Accumulator::Order::Pendulum);
    acc.setDirection(Accumulator::Direction::Up);
    acc.setEnabled(true);
    acc.tick();
    int classSeq[8];
    for (int i = 0; i < 8; ++i) { acc.tick(); classSeq[i] = int(acc.currentValue()); }

    int counter = 0;
    int pendulumDir = 1;
    for (int i = 0; i < 8; ++i) {
        AccumulatorOps::tickPendulum(counter, pendulumDir, +1, 1, 0, 3);
        expectEqual(counter, classSeq[i], "free pendulum matches class");
    }
}

CASE("extracted_hold_matches_notetrack_hold") {
    Accumulator acc;
    acc.setMinValue(0); acc.setMaxValue(5); acc.setStepValue(2);
    acc.setOrder(Accumulator::Order::Hold);
    acc.setDirection(Accumulator::Direction::Up);
    acc.setEnabled(true);
    acc.tick();
    int classSeq[6];
    for (int i = 0; i < 6; ++i) { acc.tick(); classSeq[i] = int(acc.currentValue()); }

    int counter = 0;
    for (int i = 0; i < 6; ++i) {
        AccumulatorOps::tickHold(counter, +1, 2, 0, 5);
        expectEqual(counter, classSeq[i], "free hold matches class");
    }
}

}
