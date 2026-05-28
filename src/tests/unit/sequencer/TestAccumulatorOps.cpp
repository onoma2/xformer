#include "UnitTest.h"

#include "apps/sequencer/engine/AccumulatorOps.h"
#include "apps/sequencer/model/Accumulator.h"

UNIT_TEST("AccumulatorOps") {

// ---- Boundary cases per Order x overflow-direction x polarity-bounds ----

CASE("tick_wrap_overflow_returns_to_min_when_direction_up") {
    int counter = 7;
    AccumulatorOps::tickWrap(counter, +1, 0, 7, 1);
    expectEqual(counter, 0, "should wrap to min");
}

CASE("tick_wrap_underflow_returns_to_max_when_direction_down") {
    int counter = 0;
    AccumulatorOps::tickWrap(counter, -1, 0, 7, 1);
    expectEqual(counter, 7, "should wrap to max");
}

CASE("tick_wrap_bipolar_overflow_wraps_to_neg_lim") {
    int counter = 4;
    AccumulatorOps::tickWrap(counter, +1, -4, 4, 1);
    expectEqual(counter, -4, "bipolar overflow wraps to opposite limit");
}

CASE("tick_wrap_bipolar_underflow_wraps_to_pos_lim") {
    int counter = -4;
    AccumulatorOps::tickWrap(counter, -1, -4, 4, 1);
    expectEqual(counter, 4, "bipolar underflow wraps to opposite limit");
}

CASE("tick_pendulum_overflow_reflects_at_max_and_flips_dir") {
    int counter = 6;
    int pendulumDir = 1;
    AccumulatorOps::tickPendulum(counter, pendulumDir, 0, 7, 2);
    // 6 + 2 = 8 > 7 -> clamp to 7, flip dir to -1
    expectEqual(counter, 7, "should clamp to max on overflow");
    expectEqual(pendulumDir, -1, "should flip direction down");
}

CASE("tick_pendulum_underflow_reflects_at_min_and_flips_dir") {
    int counter = 1;
    int pendulumDir = -1;
    AccumulatorOps::tickPendulum(counter, pendulumDir, 0, 7, 2);
    // 1 + (2 * -1) = -1 < 0 -> clamp to 0, flip dir to +1
    expectEqual(counter, 0, "should clamp to min on underflow");
    expectEqual(pendulumDir, 1, "should flip direction up");
}

CASE("tick_pendulum_at_zero_min_descends_then_reflects") {
    // Pendulum + Uni positive-step case (caller sets min=0).
    // Counter goes up to max, reverses, descends to 0, reverses again.
    int counter = 0;
    int pendulumDir = 1;
    int min = 0, max = 3, step = 1;
    // Ascend 0 -> 1 -> 2 -> 3 (flip)
    AccumulatorOps::tickPendulum(counter, pendulumDir, min, max, step);
    expectEqual(counter, 1, "ascend to 1");
    expectEqual(pendulumDir, 1, "still up");
    AccumulatorOps::tickPendulum(counter, pendulumDir, min, max, step);
    expectEqual(counter, 2, "ascend to 2");
    AccumulatorOps::tickPendulum(counter, pendulumDir, min, max, step);
    expectEqual(counter, 3, "ascend to 3 -> flip");
    expectEqual(pendulumDir, -1, "flipped down at max");
    // Descend 3 -> 2 -> 1 -> 0 (flip)
    AccumulatorOps::tickPendulum(counter, pendulumDir, min, max, step);
    expectEqual(counter, 2, "descend to 2");
    AccumulatorOps::tickPendulum(counter, pendulumDir, min, max, step);
    expectEqual(counter, 1, "descend to 1");
    AccumulatorOps::tickPendulum(counter, pendulumDir, min, max, step);
    expectEqual(counter, 0, "descend to 0 -> flip");
    expectEqual(pendulumDir, 1, "flipped up at min");
}

CASE("tick_hold_overflow_pins_at_max") {
    int counter = 6;
    AccumulatorOps::tickHold(counter, +1, 0, 7, 3);
    // 6 + 3 = 9 >= 7 -> clamp to 7
    expectEqual(counter, 7, "should pin at max on overflow");
    // Subsequent ticks stay at max
    AccumulatorOps::tickHold(counter, +1, 0, 7, 3);
    expectEqual(counter, 7, "should remain pinned at max");
}

CASE("tick_hold_underflow_pins_at_min") {
    int counter = 1;
    AccumulatorOps::tickHold(counter, -1, 0, 7, 3);
    // 1 - 3 = -2 <= 0 -> clamp to 0
    expectEqual(counter, 0, "should pin at min on underflow");
    AccumulatorOps::tickHold(counter, -1, 0, 7, 3);
    expectEqual(counter, 0, "should remain pinned at min");
}

CASE("tick_random_stays_in_range") {
    int counter = 0;
    for (int i = 0; i < 200; ++i) {
        AccumulatorOps::tickRandom(counter, -3, 5);
        expectTrue(counter >= -3 && counter <= 5, "random must stay in [min, max]");
    }
}

CASE("tick_random_min_equals_max_is_constant") {
    int counter = 99;
    AccumulatorOps::tickRandom(counter, 4, 4);
    expectEqual(counter, 4, "should set to that single value");
}

CASE("tick_rtz_overflow_snaps_to_zero") {
    int counter = 10;
    AccumulatorOps::tickRTZ(counter, 0, 7);
    expectEqual(counter, 0, "overflow snaps to zero");
}

CASE("tick_rtz_underflow_snaps_to_zero") {
    int counter = -2;
    AccumulatorOps::tickRTZ(counter, 0, 7);
    expectEqual(counter, 0, "underflow snaps to zero");
}

CASE("tick_rtz_in_range_unchanged") {
    int counter = 3;
    AccumulatorOps::tickRTZ(counter, 0, 7);
    expectEqual(counter, 3, "in-range value unchanged");
}

// ---- Faithfulness tests vs NoteTrack Accumulator class ----

CASE("extracted_wrap_matches_notetrack_wrap") {
    Accumulator acc;
    acc.setMinValue(0); acc.setMaxValue(7); acc.setStepValue(1);
    acc.setOrder(Accumulator::Order::Wrap);
    acc.setDirection(Accumulator::Direction::Up);
    acc.setEnabled(true);
    acc.tick(); // first-tick delay
    int classSeq[10];
    for (int i = 0; i < 10; ++i) {
        acc.tick();
        classSeq[i] = int(acc.currentValue());
    }

    int counter = 0;
    for (int i = 0; i < 10; ++i) {
        AccumulatorOps::tickWrap(counter, +1, 0, 7, 1);
        expectEqual(counter, classSeq[i], "free wrap step matches class");
    }
}

CASE("extracted_pendulum_matches_notetrack_pendulum") {
    // Drive class to bound and read public currentValue across the bounce.
    Accumulator acc;
    acc.setMinValue(0); acc.setMaxValue(3); acc.setStepValue(1);
    acc.setOrder(Accumulator::Order::Pendulum);
    acc.setDirection(Accumulator::Direction::Up);
    acc.setEnabled(true);
    acc.tick(); // first-tick delay
    int classSeq[8];
    for (int i = 0; i < 8; ++i) {
        acc.tick();
        classSeq[i] = int(acc.currentValue());
    }

    int counter = 0;
    int pendulumDir = 1;
    for (int i = 0; i < 8; ++i) {
        AccumulatorOps::tickPendulum(counter, pendulumDir, 0, 3, 1);
        expectEqual(counter, classSeq[i], "free pendulum step matches class");
    }
}

CASE("extracted_hold_matches_notetrack_hold") {
    Accumulator acc;
    acc.setMinValue(0); acc.setMaxValue(5); acc.setStepValue(2);
    acc.setOrder(Accumulator::Order::Hold);
    acc.setDirection(Accumulator::Direction::Up);
    acc.setEnabled(true);
    acc.tick(); // first-tick delay
    int classSeq[6];
    for (int i = 0; i < 6; ++i) {
        acc.tick();
        classSeq[i] = int(acc.currentValue());
    }

    int counter = 0;
    for (int i = 0; i < 6; ++i) {
        AccumulatorOps::tickHold(counter, +1, 0, 5, 2);
        expectEqual(counter, classSeq[i], "free hold step matches class");
    }
}

// ---- Edge cases ----

CASE("pendulum_after_flip_descends_correctly_until_next_limit") {
    int counter = 7;
    int pendulumDir = 1;
    // Push past max to force flip first.
    AccumulatorOps::tickPendulum(counter, pendulumDir, 0, 7, 1);
    expectEqual(counter, 7, "clamped to max");
    expectEqual(pendulumDir, -1, "flipped to down");
    // Now descend.
    AccumulatorOps::tickPendulum(counter, pendulumDir, 0, 7, 1);
    expectEqual(counter, 6, "descend 1");
    AccumulatorOps::tickPendulum(counter, pendulumDir, 0, 7, 1);
    expectEqual(counter, 5, "descend 2");
}

CASE("wrap_step_of_3_handles_overshoot_correctly") {
    // min=0, max=7, step=3, dir=+1.
    // 6 + 3 = 9 > 7 -> wrap: minValue + (currentValue - maxValue - 1) = 0 + (9 - 7 - 1) = 1.
    int counter = 6;
    AccumulatorOps::tickWrap(counter, +1, 0, 7, 3);
    expectEqual(counter, 1, "overshoot wraps with remainder");
}

CASE("pendulum_clamp_to_exact_min_max_on_hit") {
    // step=3 should not push past max; pendulum clamps to exact bound.
    int counter = 5;
    int pendulumDir = 1;
    AccumulatorOps::tickPendulum(counter, pendulumDir, 0, 7, 3);
    // 5 + 3 = 8 > 7 -> clamp to 7
    expectEqual(counter, 7, "clamped to exact max");
    expectEqual(pendulumDir, -1, "flipped");

    counter = 2;
    pendulumDir = -1;
    AccumulatorOps::tickPendulum(counter, pendulumDir, 0, 7, 3);
    // 2 + (-3) = -1 < 0 -> clamp to 0
    expectEqual(counter, 0, "clamped to exact min");
    expectEqual(pendulumDir, 1, "flipped");
}

CASE("freeze_direction_is_noop_for_wrap_and_hold") {
    int counter = 3;
    AccumulatorOps::tickWrap(counter, 0, 0, 7, 1);
    expectEqual(counter, 3, "wrap noop when direction=0");
    AccumulatorOps::tickHold(counter, 0, 0, 7, 1);
    expectEqual(counter, 3, "hold noop when direction=0");
}

}
