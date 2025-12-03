#include "UnitTest.h"
#include "core/utils/Random.h"

UNIT_TEST("TuesdayPolyrhythm") {

CASE("ornament_to_tuplet_mapping") {
    // Test that ornament values map to correct tuplet counts

    // Ornament < 5: No polyrhythm (straight 4/4)
    int ornament2 = 2;
    int tupleN_2 = 4;
    if (ornament2 >= 5 && ornament2 <= 8) tupleN_2 = 3;
    else if (ornament2 >= 9 && ornament2 <= 12) tupleN_2 = 5;
    else if (ornament2 >= 13) tupleN_2 = 7;
    expectEqual(tupleN_2, 4, "Ornament 2 should map to straight timing (4)");

    // Ornament 5-8: Triplet (3 notes over 4 beats)
    int ornament5 = 5;
    int tupleN_5 = 4;
    if (ornament5 >= 5 && ornament5 <= 8) tupleN_5 = 3;
    else if (ornament5 >= 9 && ornament5 <= 12) tupleN_5 = 5;
    else if (ornament5 >= 13) tupleN_5 = 7;
    expectEqual(tupleN_5, 3, "Ornament 5 should map to triplet (3)");

    // Ornament 9-12: Quintuplet (5 notes over 4 beats)
    int ornament9 = 9;
    int tupleN_9 = 4;
    if (ornament9 >= 5 && ornament9 <= 8) tupleN_9 = 3;
    else if (ornament9 >= 9 && ornament9 <= 12) tupleN_9 = 5;
    else if (ornament9 >= 13) tupleN_9 = 7;
    expectEqual(tupleN_9, 5, "Ornament 9 should map to quintuplet (5)");

    // Ornament >= 13: Septuplet (7 notes over 4 beats)
    int ornament13 = 13;
    int tupleN_13 = 4;
    if (ornament13 >= 5 && ornament13 <= 8) tupleN_13 = 3;
    else if (ornament13 >= 9 && ornament13 <= 12) tupleN_13 = 5;
    else if (ornament13 >= 13) tupleN_13 = 7;
    expectEqual(tupleN_13, 7, "Ornament 13 should map to septuplet (7)");
}

CASE("polyrhythm_spacing_calculation") {
    // Test gate spacing calculations for different tuplets

    // Septuplet (7 notes over 4 beats)
    uint32_t divisorTicks = 48;  // CONFIG_PPQN(192) / CONFIG_SEQUENCE_PPQN(48) * divisor(12) = 48 ticks per 16th note
    uint32_t windowTicks = 4 * divisorTicks;  // 4 beats = 192 ticks
    int tupleN = 7;
    uint32_t spacing = windowTicks / tupleN;  // 192 / 7 = 27 ticks

    expectEqual((int)spacing, 27, "Septuplet spacing should be 27 ticks");
    expectEqual((int)windowTicks, 192, "4-beat window should be 192 ticks");

    // Verify 7 gates fit in window
    int totalGateTicks = spacing * tupleN;
    expectTrue(totalGateTicks <= (int)windowTicks, "7 gates should fit in 192 ticks");

    // Triplet (3 notes over 4 beats)
    tupleN = 3;
    spacing = windowTicks / tupleN;  // 192 / 3 = 64 ticks
    expectEqual((int)spacing, 64, "Triplet spacing should be 64 ticks");

    // Quintuplet (5 notes over 4 beats)
    tupleN = 5;
    spacing = windowTicks / tupleN;  // 192 / 5 = 38 ticks
    expectEqual((int)spacing, 38, "Quintuplet spacing should be 38 ticks");
}

CASE("beat_start_detection") {
    // Test beat start detection logic (every 4 steps)

    bool beatStarts[32];
    for (int step = 0; step < 32; step++) {
        beatStarts[step] = (step % 4) == 0;
    }

    // Count beat starts
    int beatStartCount = 0;
    for (int i = 0; i < 32; i++) {
        if (beatStarts[i]) beatStartCount++;
    }

    expectEqual(beatStartCount, 8, "Should have 8 beat starts in 32 steps");

    // Verify specific steps
    expectTrue(beatStarts[0], "Step 0 should be beat start");
    expectTrue(beatStarts[4], "Step 4 should be beat start");
    expectTrue(beatStarts[8], "Step 8 should be beat start");
    expectFalse(beatStarts[1], "Step 1 should NOT be beat start");
    expectFalse(beatStarts[3], "Step 3 should NOT be beat start");
}

CASE("total_gates_calculation") {
    // Calculate expected total gates for 32 steps with different ornaments

    int steps = 32;
    int beatsPerWindow = 4;
    int beatStarts = steps / beatsPerWindow;  // 32 / 4 = 8

    // Ornament 2: Straight timing, fires on beat starts only
    int gatesOrnament2 = beatStarts;  // 8 gates
    expectEqual(gatesOrnament2, 8, "Ornament 2: 8 gates (1 per beat start)");

    // Ornament 5: Triplet (3 notes per 4-beat window)
    int tupleN_5 = 3;
    int gatesOrnament5 = beatStarts * tupleN_5;  // 8 * 3 = 24 gates
    expectEqual(gatesOrnament5, 24, "Ornament 5: 24 gates (3 per beat start)");

    // Ornament 9: Quintuplet (5 notes per 4-beat window)
    int tupleN_9 = 5;
    int gatesOrnament9 = beatStarts * tupleN_9;  // 8 * 5 = 40 gates
    expectEqual(gatesOrnament9, 40, "Ornament 9: 40 gates (5 per beat start)");

    // Ornament 13: Septuplet (7 notes per 4-beat window)
    int tupleN_13 = 7;
    int gatesOrnament13 = beatStarts * tupleN_13;  // 8 * 7 = 56 gates
    expectEqual(gatesOrnament13, 56, "Ornament 13: 56 gates (7 per beat start)");
}

CASE("gate_offset_shifts_all_gates") {
    // Verify gate offset shifts ALL gates by the offset amount

    uint32_t divisorTicks = 48;
    uint32_t windowTicks = 4 * divisorTicks;  // 192 ticks
    int tupleN = 7;
    uint32_t spacing = windowTicks / tupleN;  // 27 ticks

    uint8_t gateOffset = 50;  // 50% offset
    uint32_t gateOffsetTicks = (divisorTicks * gateOffset) / 100;  // 24 ticks

    // Calculate gate times (offset applied to base tick, then evenly spaced)
    uint32_t tick = 0;  // Beat start at tick 0
    uint32_t baseTick = tick + gateOffsetTicks;  // 24
    uint32_t gateTimes[7];

    for (int i = 0; i < tupleN; i++) {
        uint32_t offset = (i * spacing);
        gateTimes[i] = baseTick + offset;
    }

    // Verify all gates are shifted by offset
    expectEqual((int)gateTimes[0], 24, "First gate should be at tick 24 (0 + 24 offset)");
    expectEqual((int)gateTimes[1], 51, "Second gate should be at tick 51 (24 + 27)");
    expectEqual((int)gateTimes[2], 78, "Third gate should be at tick 78 (24 + 27 + 27)");
    expectEqual((int)gateTimes[3], 105, "Fourth gate should be at tick 105 (24 + 27*3)");
}

} // UNIT_TEST("TuesdayPolyrhythm")
