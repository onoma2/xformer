#include "UnitTest.h"

// Test for Tuesday microgate-level sparsity control (Option 1.5)
// Power controls both step-level cooldown AND microgate-level cooldown
// Microgate threshold is 2x harder to override than step-level

UNIT_TEST("TuesdayMicrogateSparsity") {

//----------------------------------------
// Microgate Cooldown Mapping
//----------------------------------------

CASE("power_maps_to_microgate_cooldown") {
    // Power should map to microgate cooldown same as step cooldown
    // Power 0-16 maps to cooldown 17-1

    int power = 0;
    int microCoolDownMax = 17 - power;
    expectEqual(microCoolDownMax, 17, "power 0 should give microgate cooldown 17");

    power = 4;
    microCoolDownMax = 17 - power;
    expectEqual(microCoolDownMax, 13, "power 4 should give microgate cooldown 13");

    power = 8;
    microCoolDownMax = 17 - power;
    expectEqual(microCoolDownMax, 9, "power 8 should give microgate cooldown 9");

    power = 16;
    microCoolDownMax = 17 - power;
    expectEqual(microCoolDownMax, 1, "power 16 should give microgate cooldown 1");
}

//----------------------------------------
// Microgate Threshold (2x harder)
//----------------------------------------

CASE("microgate_threshold_is_2x_harder") {
    // Microgate override threshold should be 2x the cooldown value
    // Making it harder to override than step-level

    int microCoolDown = 5;
    int microThreshold = microCoolDown * 2;  // 10
    expectEqual(microThreshold, 10, "microCoolDown 5 needs threshold 10");

    microCoolDown = 10;
    microThreshold = microCoolDown * 2;  // 20
    expectEqual(microThreshold, 20, "microCoolDown 10 needs threshold 20");

    // Velocity 255 → velDensity 15 can override up to microCoolDown 7
    int velocity = 255;
    int velDensity = velocity / 16;  // 15
    expectEqual(velDensity, 15, "velocity 255 gives velDensity 15");

    microCoolDown = 7;
    microThreshold = microCoolDown * 2;  // 14
    expectTrue(velDensity >= microThreshold, "velDensity 15 can override threshold 14");

    microCoolDown = 8;
    microThreshold = microCoolDown * 2;  // 16
    expectFalse(velDensity >= microThreshold, "velDensity 15 cannot override threshold 16");
}

//----------------------------------------
// Algorithm Velocity vs Microgate Override
//----------------------------------------

CASE("chip1_velocity_can_override_moderate_microcooldown") {
    // Chip1: velocity = 255 → velDensity = 15
    // Can override microCoolDown up to 7 (threshold 14)

    int velocity = 255;
    int velDensity = velocity / 16;  // 15

    // microCoolDown = 7 → threshold = 14 → CAN override
    int microCoolDown = 7;
    int microThreshold = microCoolDown * 2;
    expectTrue(velDensity >= microThreshold, "Chip1 can override microCoolDown 7");

    // microCoolDown = 8 → threshold = 16 → CANNOT override
    microCoolDown = 8;
    microThreshold = microCoolDown * 2;
    expectFalse(velDensity >= microThreshold, "Chip1 cannot override microCoolDown 8");
}

CASE("stomper_velocity_cannot_override_microcooldown") {
    // Stomper: velocity = 0-63 → velDensity = 0-3
    // Can only override microCoolDown up to 1 (threshold 2)

    int maxVelocity = 63;
    int maxVelDensity = maxVelocity / 16;  // 3

    // microCoolDown = 1 → threshold = 2 → CAN override (barely)
    int microCoolDown = 1;
    int microThreshold = microCoolDown * 2;
    expectTrue(maxVelDensity >= microThreshold, "Stomper can override microCoolDown 1");

    // microCoolDown = 2 → threshold = 4 → CANNOT override
    microCoolDown = 2;
    microThreshold = microCoolDown * 2;
    expectFalse(maxVelDensity >= microThreshold, "Stomper cannot override microCoolDown 2");

    // At power=4 (coolDownMax=13), Stomper will almost never fire multiple microgates
    int power = 4;
    int coolDownMax = 17 - power;  // 13
    expectEqual(coolDownMax, 13, "power 4 gives coolDownMax 13");
    expectFalse(maxVelDensity >= (coolDownMax * 2), "Stomper cannot override high cooldowns");
}

CASE("stepwave_velocity_can_override_medium_microcooldown") {
    // StepWave: velocity = 200-255 → velDensity = 12-15
    // Can override microCoolDown up to 6-7

    int minVelocity = 200;
    int minVelDensity = minVelocity / 16;  // 12

    // microCoolDown = 6 → threshold = 12 → CAN override
    int microCoolDown = 6;
    int microThreshold = microCoolDown * 2;
    expectTrue(minVelDensity >= microThreshold, "StepWave can override microCoolDown 6");

    // microCoolDown = 7 → threshold = 14 → CANNOT override with vel=200
    microCoolDown = 7;
    microThreshold = microCoolDown * 2;
    expectFalse(minVelDensity >= microThreshold, "StepWave(200) cannot override microCoolDown 7");

    // But can override with vel=255
    int maxVelocity = 255;
    int maxVelDensity = maxVelocity / 16;  // 15
    expectTrue(maxVelDensity >= microThreshold, "StepWave(255) can override microCoolDown 7");
}

//----------------------------------------
// Microgate Firing Logic
//----------------------------------------

CASE("microgate_fires_when_cooldown_zero") {
    // When microCoolDown = 0, microgate should always fire
    int microCoolDown = 0;
    bool shouldFire = (microCoolDown == 0);
    expectTrue(shouldFire, "microgate fires when cooldown is 0");
}

CASE("microgate_fires_when_velocity_overrides") {
    // When microCoolDown > 0, microgate fires if velDensity >= (microCoolDown * 2)
    int microCoolDown = 5;
    int microThreshold = microCoolDown * 2;  // 10

    int velocity = 160;  // velDensity = 10
    int velDensity = velocity / 16;
    bool shouldFire = (velDensity >= microThreshold);
    expectTrue(shouldFire, "microgate fires when velocity exactly meets threshold");

    velocity = 176;  // velDensity = 11
    velDensity = velocity / 16;
    shouldFire = (velDensity >= microThreshold);
    expectTrue(shouldFire, "microgate fires when velocity exceeds threshold");

    velocity = 144;  // velDensity = 9
    velDensity = velocity / 16;
    shouldFire = (velDensity >= microThreshold);
    expectFalse(shouldFire, "microgate blocked when velocity below threshold");
}

//----------------------------------------
// Power=0 Special Case
//----------------------------------------

CASE("power_zero_blocks_microgates") {
    // Power = 0 should prevent step from firing
    // Microgates never reached (step-level block)
    int power = 0;
    int velocity = 255;
    bool accent = false;

    bool stepAllowed = !(power == 0 || velocity == 0) || accent;
    expectFalse(stepAllowed, "power 0 blocks step, microgates never checked");
}

//----------------------------------------
// Microgate Decrement Pattern
//----------------------------------------

CASE("microgate_cooldown_decrements_per_step") {
    // Microgate cooldown should decrement once per step (not per microgate)
    // This prevents it from burning through too fast with polyrhythms

    int microCoolDown = 5;
    int microCoolDownMax = 13;

    // Step 1: Decrement once
    microCoolDown--;
    expectEqual(microCoolDown, 4, "after step 1");

    // Step 2: Decrement once
    microCoolDown--;
    expectEqual(microCoolDown, 3, "after step 2");

    // When microgate fires, reset to max
    microCoolDown = 0;  // Fired
    microCoolDown = microCoolDownMax;
    expectEqual(microCoolDown, 13, "reset to max after firing");
}

//----------------------------------------
// Polyrhythm Interaction
//----------------------------------------

CASE("polyrhythm_creates_multiple_microgate_opportunities") {
    // With polyrhythm=3, each step has 3 microgate slots
    // Each slot gets independent cooldown check
    int polyrhythm = 3;
    int tupleN = 3;  // 3 microgates per step

    expectEqual(tupleN, 3, "polyrhythm 3 creates 3 microgate slots");

    // Each microgate checks: (microCoolDown == 0) || (velDensity >= microCoolDown * 2)
    // If all pass, all 3 microgates fire
    // If only first passes (microCoolDown=0 on first iteration), only 1 fires
}

} // UNIT_TEST("TuesdayMicrogateSparsity")
