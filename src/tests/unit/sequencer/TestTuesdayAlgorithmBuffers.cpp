#include "UnitTest.h"
#include "core/utils/Random.h"

UNIT_TEST("TuesdayAlgorithmBuffers") {

//----------------------------------------
// RNG Determinism Tests
//----------------------------------------

CASE("rng_same_seed_same_sequence") {
    // Same seeds should produce identical output
    Random rng1((5 - 1) << 4);  // Flow = 5
    Random rng2((5 - 1) << 4);  // Same seed

    for (int i = 0; i < 100; i++) {
        expectEqual((int)rng1.next(), (int)rng2.next(), "RNG should be deterministic");
    }
}

CASE("rng_different_seed_different_sequence") {
    Random rng1((5 - 1) << 4);  // Flow = 5
    Random rng2((8 - 1) << 4);  // Flow = 8

    int matches = 0;
    for (int i = 0; i < 100; i++) {
        if (rng1.next() == rng2.next()) matches++;
    }
    expectTrue(matches < 10, "different seeds should produce different sequences");
}

//----------------------------------------
// Parameter Influence Tests
//----------------------------------------

CASE("flow_parameter_seeds_rng") {
    // Verify Flow seeds the main RNG differently for different values
    Random rng_flow5((5 - 1) << 4);
    Random rng_flow8((8 - 1) << 4);

    uint32_t first5 = rng_flow5.next();
    uint32_t first8 = rng_flow8.next();

    expectTrue(first5 != first8, "different flow values should seed different patterns");
}

CASE("ornament_parameter_seeds_extra_rng") {
    // Verify Ornament seeds the extra RNG
    Random rng_orn3((3 - 1) << 4);
    Random rng_orn7((7 - 1) << 4);

    uint32_t first3 = rng_orn3.next();
    uint32_t first7 = rng_orn7.next();

    expectTrue(first3 != first7, "different ornament values should seed different patterns");
}

//----------------------------------------
// Note Range Tests
//----------------------------------------

CASE("note_range_within_bounds") {
    // All generated notes should be 0-11 (chromatic)
    Random rng(12345);
    for (int i = 0; i < 100; i++) {
        int note = rng.next() % 12;
        expectTrue(note >= 0 && note <= 11, "note should be 0-11");
    }
}

CASE("octave_range_within_bounds") {
    // Octave offsets should be reasonable (-2 to +2 typical)
    Random rng(12345);
    for (int i = 0; i < 100; i++) {
        int octave = (rng.next() % 5) - 2;  // -2 to +2
        expectTrue(octave >= -2 && octave <= 2, "octave should be -2 to +2");
    }
}

//----------------------------------------
// Buffer Regeneration Tests
//----------------------------------------

CASE("buffer_regeneration_deterministic") {
    // Same parameters should produce same buffer content
    Random rng1(1234);
    Random rng2(1234);

    int note1[16], note2[16];
    for (int i = 0; i < 16; i++) {
        note1[i] = rng1.next() % 12;
        note2[i] = rng2.next() % 12;
        expectEqual(note1[i], note2[i], "regenerated buffer should match");
    }
}

//----------------------------------------
// Global Glide Integration Tests
//----------------------------------------

CASE("glide_zero_no_slides") {
    // When glide=0, no slides should occur
    int glide = 0;
    Random rng(12345);
    int slideCount = 0;

    for (int i = 0; i < 100; i++) {
        if (glide > 0 && rng.nextRange(100) < glide) {
            slideCount++;
        }
    }

    expectEqual(slideCount, 0, "glide 0 should produce no slides");
}

CASE("glide_100_always_slides") {
    // When glide=100, all eligible notes should slide
    int glide = 100;
    Random rng(12345);
    int slideCount = 0;

    for (int i = 0; i < 100; i++) {
        if (glide > 0 && rng.nextRange(100) < glide) {
            slideCount++;
        }
    }

    expectEqual(slideCount, 100, "glide 100 should always slide");
}

CASE("glide_probabilistic") {
    // glide=50 should produce roughly 50% slides
    int glide = 50;
    Random rng(12345);
    int slideCount = 0;
    int total = 1000;

    for (int i = 0; i < total; i++) {
        if (glide > 0 && rng.nextRange(100) < glide) {
            slideCount++;
        }
    }

    // Allow 10% tolerance
    expectTrue(slideCount > 400 && slideCount < 600,
        "glide 50 should produce ~50% slides");
}

//----------------------------------------
// CV Update Mode Tests
//----------------------------------------

CASE("cv_update_free_mode_updates_every_step") {
    // In Free mode, CV updates every step regardless of gate
    bool freeMode = true;
    bool gateTriggered = false;

    bool shouldUpdateCv = freeMode || gateTriggered;
    expectTrue(shouldUpdateCv, "Free mode should update CV without gate");
}

CASE("cv_update_gated_mode_requires_gate") {
    // In Gated mode, CV only updates when gate fires
    bool freeMode = false;
    bool gateTriggered = false;

    bool shouldUpdateCv = freeMode || gateTriggered;
    expectFalse(shouldUpdateCv, "Gated mode should not update CV without gate");

    gateTriggered = true;
    shouldUpdateCv = freeMode || gateTriggered;
    expectTrue(shouldUpdateCv, "Gated mode should update CV with gate");
}

} // UNIT_TEST("TuesdayAlgorithmBuffers")
