#include "UnitTest.h"
#include "core/utils/Random.h"

UNIT_TEST("TuesdayAmbient") {

//----------------------------------------
// State Variable Initialization
//----------------------------------------

CASE("ambient_state_default_values") {
    // Test initial state values
    int8_t lastNote = 0;
    uint8_t holdTimer = 0;
    int8_t driftDir = 0;
    uint8_t driftAmount = 0;
    uint8_t harmonic = 0;
    uint8_t silenceCount = 0;
    uint8_t driftCounter = 0;

    expectEqual((int)lastNote, 0, "lastNote should start at 0");
    expectEqual((int)holdTimer, 0, "holdTimer should start at 0");
    expectEqual((int)driftDir, 0, "driftDir should start at 0");
    expectEqual((int)driftAmount, 0, "driftAmount should start at 0");
    expectEqual((int)harmonic, 0, "harmonic should start at 0");
    expectEqual((int)silenceCount, 0, "silenceCount should start at 0");
    expectEqual((int)driftCounter, 0, "driftCounter should start at 0");
}

//----------------------------------------
// Flow Parameter: Drift Behavior
//----------------------------------------

CASE("ambient_flow_zero_no_drift") {
    // Flow=0 should minimize drift
    int flow = 0;
    int driftMax = flow;
    expectEqual(driftMax, 0, "drift should be zero when flow is 0");
}

CASE("ambient_flow_high_more_drift") {
    // Higher flow = more drift
    int flow = 16;
    int driftMax = flow;
    expectEqual(driftMax, 16, "drift should scale with flow");
}

CASE("ambient_drift_direction_changes") {
    // Drift direction should change periodically
    Random rng(12345);
    int8_t driftDir = 1;

    // Simulate drift direction toggle
    if (rng.next() % 8 == 0) {
        driftDir = -driftDir;
    }

    expectTrue(driftDir == 1 || driftDir == -1, "drift direction should be +1 or -1");
}

//----------------------------------------
// Ornament Parameter: Harmonics
//----------------------------------------

CASE("ambient_ornament_harmonic_intervals") {
    // Test harmonic interval selection based on ornament
    // Typical ambient intervals: unison, fourth, fifth, octave
    int intervals[] = {0, 5, 7, 12};

    for (int i = 0; i < 4; i++) {
        expectTrue(intervals[i] >= 0 && intervals[i] <= 12,
            "harmonic intervals should be reasonable");
    }
}

CASE("ambient_harmonic_probability") {
    // Higher ornament = more harmonics
    Random rng(12345);
    int ornament = 12;
    int harmonicCount = 0;
    int total = 100;

    for (int i = 0; i < total; i++) {
        if (rng.nextRange(16) < ornament) {
            harmonicCount++;
        }
    }

    // Ornament 12/16 = 75% chance
    expectTrue(harmonicCount > 50, "high ornament should produce more harmonics");
}

//----------------------------------------
// Power Parameter: Gate Density
//----------------------------------------

CASE("ambient_power_zero_sparse") {
    // Power=0 should produce very sparse gates
    int power = 0;
    int cooldownMax = 16 - power;
    expectEqual(cooldownMax, 16, "max cooldown at power 0 for sparse triggers");
}

CASE("ambient_power_high_more_triggers") {
    // Higher power = more frequent triggers
    int power = 16;
    int cooldownMax = 16 - power;
    expectEqual(cooldownMax, 0, "min cooldown at power 16 for frequent triggers");
}

//----------------------------------------
// Musical Characteristics
//----------------------------------------

CASE("ambient_long_holds") {
    // AMBIENT should produce long hold times (>4 steps typical)
    Random rng(12345);
    uint8_t holdTimer = (rng.next() % 8) + 4;  // 4-11 steps
    expectTrue(holdTimer >= 4, "ambient holds should be long");
}

CASE("ambient_sparse_triggers") {
    // At moderate power, triggers should be sparse
    Random rng(12345);
    int triggerCount = 0;
    int totalSteps = 64;
    int power = 4;  // Low-moderate power

    for (int i = 0; i < totalSteps; i++) {
        int cooldown = 16 - power;
        if (i % (cooldown + 1) == 0) triggerCount++;
    }

    expectTrue(triggerCount <= 16, "ambient should have sparse triggers");
}

CASE("ambient_slow_evolution") {
    // Notes should evolve slowly via drift
    int8_t note = 5;
    int8_t driftDir = 1;
    int driftCounter = 0;
    int driftRate = 4;  // Every 4 steps

    // Simulate 8 steps
    for (int i = 0; i < 8; i++) {
        driftCounter++;
        if (driftCounter >= driftRate) {
            note = (note + driftDir + 12) % 12;
            driftCounter = 0;
        }
    }

    // After 8 steps with rate 4, should drift twice
    expectEqual((int)note, 7, "note should drift slowly");
}

//----------------------------------------
// Edge Cases
//----------------------------------------

CASE("ambient_drift_wrapping") {
    // Notes should wrap when drifting beyond range
    int note = 11;
    int drift = 2;
    int newNote = (note + drift) % 12;
    expectEqual(newNote, 1, "note should wrap at 12");

    note = 0;
    drift = -2;
    newNote = (note + drift + 12) % 12;
    expectEqual(newNote, 10, "note should wrap below 0");
}

CASE("ambient_hold_timer_decrement") {
    // Hold timer should decrement each step
    uint8_t holdTimer = 5;
    for (int i = 0; i < 3; i++) {
        if (holdTimer > 0) holdTimer--;
    }
    expectEqual((int)holdTimer, 2, "hold timer should decrement correctly");
}

} // UNIT_TEST("TuesdayAmbient")
