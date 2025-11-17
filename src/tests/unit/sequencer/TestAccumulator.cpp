#include "UnitTest.h"

#include "apps/sequencer/model/Accumulator.h"

UNIT_TEST("Accumulator") {

CASE("create") {
    Accumulator accumulator;
    expectTrue(true, "dummy assertion");
}

CASE("tick_up_enabled") {
    Accumulator accumulator;
    accumulator.setDirection(Accumulator::Direction::Up);
    accumulator.setEnabled(true);
    accumulator.tick();
    expectEqual(static_cast<int>(accumulator.currentValue()), 1, "currentValue should be 1 after one tick");
}

CASE("tick_disabled") {
    Accumulator accumulator;
    accumulator.setDirection(Accumulator::Direction::Up);
    accumulator.setEnabled(false);
    accumulator.tick();
    expectEqual(static_cast<int>(accumulator.currentValue()), 0, "currentValue should remain 0 when disabled");
}

CASE("tick_down_enabled") {
    Accumulator accumulator;
    accumulator.setDirection(Accumulator::Direction::Down);
    accumulator.setEnabled(true);
    accumulator.tick();
    expectEqual(static_cast<int>(accumulator.currentValue()), -1, "currentValue should be -1 after one tick down");
}

CASE("initial_step_value") {
    Accumulator accumulator;
    expectEqual(static_cast<int>(accumulator.stepValue()), 1, "initial stepValue should be 1");
}

CASE("default_min_value_is_zero") {
    Accumulator accumulator;
    expectEqual(static_cast<int>(accumulator.minValue()), 0, "default minValue should be 0");
}

CASE("tick_with_custom_step_value") {
    Accumulator accumulator;
    accumulator.setDirection(Accumulator::Direction::Up);
    accumulator.setEnabled(true);
    accumulator.setStepValue(5);
    accumulator.tick();
    expectEqual(static_cast<int>(accumulator.currentValue()), 5, "currentValue should be 5 after one tick with stepValue 5");
}

CASE("tick_with_min_max_clamping") {
    Accumulator accumulator;
    accumulator.setDirection(Accumulator::Direction::Up);
    accumulator.setEnabled(true);
    accumulator.setMinValue(0);
    accumulator.setMaxValue(2);
    accumulator.setStepValue(1);
    accumulator.setOrder(Accumulator::Order::Hold); // Use Hold for clamping behavior

    accumulator.tick(); // 0 -> 1
    expectEqual(static_cast<int>(accumulator.currentValue()), 1, "currentValue should be 1");
    accumulator.tick(); // 1 -> 2
    expectEqual(static_cast<int>(accumulator.currentValue()), 2, "currentValue should be 2");
    accumulator.tick(); // 2 -> 2 (clamped)
    expectEqual(static_cast<int>(accumulator.currentValue()), 2, "currentValue should be 2 (clamped)");

    accumulator.setDirection(Accumulator::Direction::Down);
    accumulator.tick(); // 2 -> 1
    expectEqual(static_cast<int>(accumulator.currentValue()), 1, "currentValue should be 1");
    accumulator.tick(); // 1 -> 0
    expectEqual(static_cast<int>(accumulator.currentValue()), 0, "currentValue should be 0");
    accumulator.tick(); // 0 -> 0 (clamped)
    expectEqual(static_cast<int>(accumulator.currentValue()), 0, "currentValue should be 0 (clamped)");
}

CASE("tick_with_wrap_order") {
    Accumulator accumulator;
    accumulator.setDirection(Accumulator::Direction::Up);
    accumulator.setEnabled(true);
    accumulator.setMinValue(0);
    accumulator.setMaxValue(2);
    accumulator.setStepValue(1);
    accumulator.setOrder(Accumulator::Order::Wrap);

    accumulator.tick(); // 0 -> 1
    expectEqual(static_cast<int>(accumulator.currentValue()), 1, "currentValue should be 1");
    accumulator.tick(); // 1 -> 2
    expectEqual(static_cast<int>(accumulator.currentValue()), 2, "currentValue should be 2");
    accumulator.tick(); // 2 -> 0 (wrapped)
    expectEqual(static_cast<int>(accumulator.currentValue()), 0, "currentValue should be 0 (wrapped)");

    accumulator.setDirection(Accumulator::Direction::Down);
    accumulator.tick(); // 0 -> 2 (wrapped)
    expectEqual(static_cast<int>(accumulator.currentValue()), 2, "currentValue should be 2 (wrapped)");
    accumulator.tick(); // 2 -> 1
    expectEqual(static_cast<int>(accumulator.currentValue()), 1, "currentValue should be 1");
    accumulator.tick(); // 1 -> 0
    expectEqual(static_cast<int>(accumulator.currentValue()), 0, "currentValue should be 0");
}

CASE("tick_with_pendulum_order") {
    Accumulator accumulator;
    accumulator.setDirection(Accumulator::Direction::Up);
    accumulator.setEnabled(true);
    accumulator.setMinValue(0);
    accumulator.setMaxValue(2);
    accumulator.setStepValue(1);
    accumulator.setOrder(Accumulator::Order::Pendulum);

    accumulator.tick(); // 0 -> 1
    expectEqual(static_cast<int>(accumulator.currentValue()), 1, "currentValue should be 1");
    accumulator.tick(); // 1 -> 2
    expectEqual(static_cast<int>(accumulator.currentValue()), 2, "currentValue should be 2");
    accumulator.tick(); // 2 -> 1 (pendulum direction changes)
    expectEqual(static_cast<int>(accumulator.currentValue()), 1, "currentValue should be 1 (pendulum inverts)");
    accumulator.tick(); // 1 -> 0
    expectEqual(static_cast<int>(accumulator.currentValue()), 0, "currentValue should be 0");
    accumulator.tick(); // 0 -> 1 (pendulum direction changes again)
    expectEqual(static_cast<int>(accumulator.currentValue()), 1, "currentValue should be 1");
}

CASE("tick_with_hold_order") {
    Accumulator accumulator;
    accumulator.setDirection(Accumulator::Direction::Up);
    accumulator.setEnabled(true);
    accumulator.setMinValue(0);
    accumulator.setMaxValue(2);
    accumulator.setStepValue(1);
    accumulator.setOrder(Accumulator::Order::Hold);

    accumulator.tick(); // 0 -> 1
    expectEqual(static_cast<int>(accumulator.currentValue()), 1, "currentValue should be 1");
    accumulator.tick(); // 1 -> 2
    expectEqual(static_cast<int>(accumulator.currentValue()), 2, "currentValue should be 2");
    accumulator.tick(); // 2 -> 2 (hold at max)
    expectEqual(static_cast<int>(accumulator.currentValue()), 2, "currentValue should be 2 (hold at max)");

    accumulator.setDirection(Accumulator::Direction::Down);
    accumulator.tick(); // 2 -> 1
    expectEqual(static_cast<int>(accumulator.currentValue()), 1, "currentValue should be 1");
    accumulator.tick(); // 1 -> 0
    expectEqual(static_cast<int>(accumulator.currentValue()), 0, "currentValue should be 0");
    accumulator.tick(); // 0 -> 0 (hold at min)
    expectEqual(static_cast<int>(accumulator.currentValue()), 0, "currentValue should be 0 (hold at min)");
}

CASE("tick_with_random_order") {
    Accumulator accumulator;
    accumulator.setDirection(Accumulator::Direction::Up);
    accumulator.setEnabled(true);
    accumulator.setMinValue(0);
    accumulator.setMaxValue(10);
    accumulator.setStepValue(1);
    accumulator.setOrder(Accumulator::Order::Random);

    accumulator.tick();
    int firstValue = accumulator.currentValue();
    accumulator.tick();
    int secondValue = accumulator.currentValue();
    accumulator.tick();
    int thirdValue = accumulator.currentValue();

    // For Random order, we can't predict exact values, but we can check that the values
    // are within the expected range (0 to 10)
    expectTrue(firstValue >= 0 && firstValue <= 10, "currentValue should be within min/max bounds");
    expectTrue(secondValue >= 0 && secondValue <= 10, "currentValue should be within min/max bounds");
    expectTrue(thirdValue >= 0 && thirdValue <= 10, "currentValue should be within min/max bounds");
    // Note: Since we can't predict the exact random behavior, we're just testing that
    // it's implemented and doesn't crash
}

} // UNIT_TEST("Accumulator")
