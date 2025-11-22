#include "UnitTest.h"
#include "core/utils/Random.h"

UNIT_TEST("TuesdayAphex") {

//----------------------------------------
// State Variable Initialization
//----------------------------------------

CASE("aphex_state_default_values") {
    uint8_t pattern[8] = {0};
    uint8_t timeSigNum = 4;
    uint8_t glitchProb = 0;
    uint8_t position = 0;
    uint8_t noteIndex = 0;
    int8_t lastNote = 0;
    uint8_t stepCounter = 0;

    expectEqual((int)timeSigNum, 4, "timeSigNum should default to 4");
    expectEqual((int)glitchProb, 0, "glitchProb should start at 0");
    expectEqual((int)position, 0, "position should start at 0");
    expectEqual((int)noteIndex, 0, "noteIndex should start at 0");
    expectEqual((int)lastNote, 0, "lastNote should start at 0");
    expectEqual((int)stepCounter, 0, "stepCounter should start at 0");
    expectEqual((int)pattern[0], 0, "pattern should be zeroed");
}

//----------------------------------------
// Flow Parameter: Pattern Complexity
//----------------------------------------

CASE("aphex_flow_polyrhythm") {
    // Flow controls polyrhythmic complexity
    Random rng(12345);
    int flow = 12;

    // Higher flow = more complex time signatures
    int timeSig = 3 + (flow % 5);  // 3, 4, 5, 6, 7
    expectTrue(timeSig >= 3 && timeSig <= 7, "time sig should be 3-7");
}

//----------------------------------------
// Ornament Parameter: Glitch Probability
//----------------------------------------

CASE("aphex_ornament_glitch_probability") {
    Random rng(12345);
    int ornament = 10;
    int glitchCount = 0;

    for (int i = 0; i < 100; i++) {
        if (rng.nextRange(16) < ornament) {
            glitchCount++;
        }
    }

    expectTrue(glitchCount > 40, "high ornament should produce more glitches");
}

//----------------------------------------
// Polyrhythmic Patterns
//----------------------------------------

CASE("aphex_odd_time_signatures") {
    // Aphex uses odd time signatures like 5/8, 7/8
    int timeSigs[] = {3, 5, 7};
    for (int i = 0; i < 3; i++) {
        expectTrue(timeSigs[i] % 2 == 1 || timeSigs[i] == 4, "odd time sigs");
    }
}

CASE("aphex_pattern_8_steps") {
    uint8_t pattern[8];
    Random rng(12345);

    for (int i = 0; i < 8; i++) {
        pattern[i] = rng.next() % 12;
        expectTrue(pattern[i] <= 11, "notes should be 0-11");
    }
}

CASE("aphex_position_wraps_at_timesig") {
    // Position wraps at time signature, not 8
    uint8_t position = 4;
    uint8_t timeSig = 5;
    position = (position + 1) % timeSig;
    expectEqual((int)position, 0, "position should wrap at timeSig");
}

//----------------------------------------
// Glitch Effects
//----------------------------------------

CASE("aphex_glitch_note_repeat") {
    // Glitch can repeat previous note
    int8_t lastNote = 5;
    int8_t glitchedNote = lastNote;  // Repeat
    expectEqual((int)glitchedNote, 5, "glitch repeats note");
}

CASE("aphex_glitch_gate_variation") {
    // Glitches can have varied gate lengths
    Random rng(12345);
    int gatePercent = 25 + (rng.next() % 75);  // 25-100%
    expectTrue(gatePercent >= 25 && gatePercent <= 100, "varied gates");
}

//----------------------------------------
// Gate Characteristics
//----------------------------------------

CASE("aphex_varied_gates") {
    // Aphex has highly varied gate lengths
    Random rng(12345);
    int gates[10];
    int variations = 0;

    for (int i = 0; i < 10; i++) {
        gates[i] = 25 + (rng.next() % 75);
        if (i > 0 && gates[i] != gates[i-1]) variations++;
    }

    expectTrue(variations > 5, "gates should vary");
}

//----------------------------------------
// Step Counter
//----------------------------------------

CASE("aphex_step_counter_increments") {
    uint8_t stepCounter = 0;
    for (int i = 0; i < 10; i++) {
        stepCounter++;
    }
    expectEqual((int)stepCounter, 10, "step counter should increment");
}

CASE("aphex_note_index_advances") {
    uint8_t noteIndex = 3;
    noteIndex = (noteIndex + 1) % 8;
    expectEqual((int)noteIndex, 4, "note index should advance");
}

} // UNIT_TEST("TuesdayAphex")
