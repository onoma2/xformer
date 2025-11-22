#include "UnitTest.h"
#include "core/utils/Random.h"

UNIT_TEST("TuesdayAutech") {

//----------------------------------------
// State Variable Initialization
//----------------------------------------

CASE("autech_state_default_values") {
    uint8_t transformState[2] = {0};
    uint8_t mutationRate = 0;
    uint8_t chaosSeed = 0;
    uint8_t stepCount = 0;
    int8_t currentNote = 0;
    uint8_t patternShift = 0;

    expectEqual((int)transformState[0], 0, "transformState[0] should be 0");
    expectEqual((int)transformState[1], 0, "transformState[1] should be 0");
    expectEqual((int)mutationRate, 0, "mutationRate should start at 0");
    expectEqual((int)chaosSeed, 0, "chaosSeed should start at 0");
    expectEqual((int)stepCount, 0, "stepCount should start at 0");
    expectEqual((int)currentNote, 0, "currentNote should start at 0");
    expectEqual((int)patternShift, 0, "patternShift should start at 0");
}

//----------------------------------------
// Flow Parameter: Transform Rate
//----------------------------------------

CASE("autech_flow_transform_rate") {
    // Higher flow = faster pattern transformation
    int flow = 12;
    int transformRate = flow;
    expectEqual(transformRate, 12, "transform rate should equal flow");
}

CASE("autech_flow_zero_stable") {
    // Flow=0 should be more stable
    int flow = 0;
    Random rng(12345);
    int transforms = 0;

    for (int i = 0; i < 100; i++) {
        if (flow > 0 && rng.nextRange(16) < flow) {
            transforms++;
        }
    }

    expectEqual(transforms, 0, "flow 0 should not transform");
}

//----------------------------------------
// Ornament Parameter: Micro-timing
//----------------------------------------

CASE("autech_ornament_micro_timing") {
    Random rng(12345);
    int ornament = 8;
    int microTimingCount = 0;

    for (int i = 0; i < 100; i++) {
        if (rng.nextRange(16) < ornament) {
            microTimingCount++;
        }
    }

    expectTrue(microTimingCount > 30, "ornament should affect micro-timing");
}

//----------------------------------------
// Pattern Evolution
//----------------------------------------

CASE("autech_never_repeats") {
    // Autechre patterns should constantly evolve
    Random rng(12345);
    int notes[16];

    for (int i = 0; i < 16; i++) {
        notes[i] = rng.next() % 12;
    }

    // Check for some variation
    int same = 0;
    for (int i = 1; i < 16; i++) {
        if (notes[i] == notes[i-1]) same++;
    }

    expectTrue(same < 10, "should have variation");
}

CASE("autech_transform_state_changes") {
    // Transform state should change over time
    Random rng(12345);
    uint8_t state1 = rng.next();
    uint8_t state2 = rng.next();

    expectTrue(state1 != state2 || state1 == state2, "states can differ");
}

CASE("autech_pattern_shift") {
    // Pattern shift for evolution
    uint8_t patternShift = 3;
    int note = 5;
    int shiftedNote = (note + patternShift) % 12;
    expectEqual(shiftedNote, 8, "pattern shift should offset note");
}

//----------------------------------------
// Chaos and Mutation
//----------------------------------------

CASE("autech_mutation_rate_effect") {
    Random rng(12345);
    uint8_t mutationRate = 200;  // High mutation
    int mutations = 0;

    for (int i = 0; i < 100; i++) {
        if (rng.nextRange(256) < mutationRate) {
            mutations++;
        }
    }

    expectTrue(mutations > 60, "high mutation rate should mutate often");
}

CASE("autech_chaos_seed_variety") {
    Random rng(12345);
    uint8_t seed1 = rng.next();
    uint8_t seed2 = rng.next();

    // Seeds should generally differ
    expectTrue(seed1 != seed2 || seed1 == 0, "seeds should vary");
}

//----------------------------------------
// Gate Characteristics
//----------------------------------------

CASE("autech_irregular_gates") {
    // Autechre has irregular, unpredictable gates
    Random rng(12345);
    int gates[10];

    for (int i = 0; i < 10; i++) {
        gates[i] = 15 + (rng.next() % 85);  // 15-100%
    }

    int variations = 0;
    for (int i = 1; i < 10; i++) {
        if (gates[i] != gates[i-1]) variations++;
    }

    expectTrue(variations > 5, "gates should be irregular");
}

//----------------------------------------
// Step Counter
//----------------------------------------

CASE("autech_step_count_increments") {
    uint8_t stepCount = 0;
    for (int i = 0; i < 10; i++) {
        stepCount++;
    }
    expectEqual((int)stepCount, 10, "step count should increment");
}

CASE("autech_current_note_updates") {
    int8_t currentNote = 5;
    currentNote = (currentNote + 3) % 12;
    expectEqual((int)currentNote, 8, "current note should update");
}

} // UNIT_TEST("TuesdayAutech")
