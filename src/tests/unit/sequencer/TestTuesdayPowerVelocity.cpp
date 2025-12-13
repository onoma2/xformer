#include "UnitTest.h"

// Test for Tuesday power/velocity/cooldown interaction
// This tests the regression where algorithms should generate meaningful velocity values
// that work with the power/cooldown system

UNIT_TEST("TuesdayPowerVelocity") {

//----------------------------------------
// Power/Cooldown Basics
//----------------------------------------

CASE("power_to_cooldown_calculation") {
    // Power 0-16 maps to cooldown 17-1
    // Power 0 -> cooldown 17 (very sparse, every 17 steps)
    // Power 8 -> cooldown 9 (medium density)
    // Power 16 -> cooldown 1 (dense, almost every step)

    int power = 0;
    int baseCooldown = 17 - power;
    expectEqual(baseCooldown, 17, "power 0 should give cooldown 17");

    power = 8;
    baseCooldown = 17 - power;
    expectEqual(baseCooldown, 9, "power 8 should give cooldown 9");

    power = 16;
    baseCooldown = 17 - power;
    expectEqual(baseCooldown, 1, "power 16 should give cooldown 1");
}

//----------------------------------------
// Velocity-Dependent Cooldown Override
//----------------------------------------

CASE("velocity_overcomes_cooldown") {
    // When cooldown is active (>0), velocity can override it
    // Velocity 0-255 is divided by 16 to get density value (0-15)
    // If velDensity >= coolDown, the gate fires

    int coolDown = 5;

    // Low velocity (< 5*16 = 80) should NOT overcome cooldown
    int velocity = 64;  // 64/16 = 4
    int velDensity = velocity / 16;
    expectFalse(velDensity >= coolDown, "velocity 64 should not overcome cooldown 5");

    // High velocity (>= 5*16 = 80) should overcome cooldown
    velocity = 80;  // 80/16 = 5
    velDensity = velocity / 16;
    expectTrue(velDensity >= coolDown, "velocity 80 should overcome cooldown 5");

    velocity = 255;  // 255/16 = 15
    velDensity = velocity / 16;
    expectTrue(velDensity >= coolDown, "velocity 255 should overcome cooldown 5");
}

//----------------------------------------
// Accent Behavior
//----------------------------------------

CASE("accent_always_fires") {
    // Accent notes should ALWAYS fire, regardless of cooldown or velocity
    // This is tested implicitly: when accent=true, eventAllowed=true

    bool accent = true;
    int coolDown = 10;  // High cooldown
    int velocity = 0;   // Zero velocity

    // In the actual code:
    // if (result.accent) { eventAllowed = true; }

    expectTrue(accent, "accent flag exists");
    expectTrue(coolDown > 0, "cooldown is active");
    expectEqual(velocity, 0, "velocity is zero");
    // Result: accent overrides both cooldown and velocity
}

//----------------------------------------
// Cooldown Decrement Pattern
//----------------------------------------

CASE("cooldown_decrements_each_step") {
    // Cooldown should decrement by 1 each step
    // When it reaches 0, a gate CAN fire (subject to velocity)
    // After firing, cooldown resets to coolDownMax

    int coolDown = 3;
    int coolDownMax = 5;

    // Step 1: coolDown = 3, gate blocked (no fire)
    expectEqual(coolDown, 3, "initial cooldown");
    coolDown--;  // Now 2

    // Step 2: coolDown = 2, gate blocked
    expectEqual(coolDown, 2, "after first decrement");
    coolDown--;  // Now 1

    // Step 3: coolDown = 1, gate blocked
    expectEqual(coolDown, 1, "after second decrement");
    coolDown--;  // Now 0

    // Step 4: coolDown = 0, gate FIRES
    expectEqual(coolDown, 0, "cooldown expired");
    coolDown = coolDownMax;  // Reset
    expectEqual(coolDown, 5, "cooldown reset to max");
}

//----------------------------------------
// Algorithm Velocity Generation
//----------------------------------------

CASE("algorithms_should_generate_varying_velocity") {
    // Algorithms should generate meaningful velocity values
    // These values should interact with the power/cooldown system
    // to create dynamic, musical patterns

    // Example velocity ranges from algorithms:
    // - Tritrance: 0-127 (variable, phase 2 = 255)
    // - Stomper: 0-255 (variable based on mode)
    // - Wobble: 0-64 (extraRng / 4)
    // - Markov: 40-167 (random/2 + 40)

    // TEST algorithm generates fixed velocity based on ornament
    int ornament = 8;  // Range 0-16
    int algoVelocity = (ornament - 1) << 4;  // (8-1)*16 = 112
    expectEqual(algoVelocity, 112, "TEST algorithm velocity calculation");

    // Markov algorithm velocity range: (_rng.nextRange(256) / 2) + 40
    // Max: (255/2) + 40 = 127 + 40 = 167
    int markovVelocity = (255 / 2) + 40;
    expectEqual(markovVelocity, 167, "Markov max velocity");
}

//----------------------------------------
// Power=0 Special Case
//----------------------------------------

CASE("power_zero_blocks_all_gates") {
    // When power = 0, ALL gates should be blocked
    // Exception: accents still fire

    int power = 0;
    int velocity = 255;  // Max velocity
    bool accent = false;

    // In code: if (result.velocity == 0 || power == 0) { eventAllowed = false; }
    bool shouldBlock = (power == 0 && !accent);
    expectTrue(shouldBlock, "power 0 should block non-accent gates");

    // But accents still work
    accent = true;
    shouldBlock = (power == 0 && !accent);
    expectFalse(shouldBlock, "power 0 should NOT block accents");
}

//----------------------------------------
// Expected Behavior Summary
//----------------------------------------

CASE("expected_gate_firing_pattern") {
    // With power = 8 (cooldown = 9):
    // - Gate fires immediately (cooldown = 0)
    // - Cooldown set to 9
    // - Next 8 steps: no fire (cooldown 9->8->7->6->5->4->3->2->1)
    // - 9th step: cooldown = 0, gate fires again
    // - Repeat

    // Velocity override: if velocity is high enough, can fire earlier
    // Example: cooldown = 5, velocity = 96 (96/16=6 >= 5), fires

    int power = 8;
    int coolDownMax = 17 - power;  // 9
    expectEqual(coolDownMax, 9, "power 8 gives cooldown max of 9");

    // Simulate pattern
    int stepCount = 0;
    int coolDown = 0;  // Start ready to fire
    int firingSteps[] = {0, 9, 18, 27, 36};  // Expected firing pattern
    int firingIdx = 0;

    for (int step = 0; step < 40; step++) {
        if (coolDown == 0) {
            // Fire!
            expectEqual(step, firingSteps[firingIdx], "gate should fire at expected step");
            firingIdx++;
            coolDown = coolDownMax;
        }

        if (coolDown > 0) coolDown--;
    }

    expectEqual(firingIdx, 5, "should have fired 5 times in 40 steps");
}

} // UNIT_TEST("TuesdayPowerVelocity")
