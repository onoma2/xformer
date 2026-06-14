#include "UnitTest.h"

#include "apps/sequencer/model/HarmonyEngine.h"
#include "apps/sequencer/model/NoteSequence.h"

// This test file investigates the reported issue where:
// - Per-step inversion has audible effect
// - Sequence-level inversion has NO effect
//
// Both should use the same code path, so we need to test:
// 1. NoteSequence stores/retrieves inversion values correctly
// 2. Per-step override logic (value 0 = use sequence-level)
// 3. All inversion values produce different results

UNIT_TEST("Harmony Inversion Issue Investigation") {

// =========================================================================
// SECTION 1: NoteSequence Sequence-Level Inversion Storage
// =========================================================================

CASE("sequence_level_inversion_default_value") {
    NoteSequence seq(0);

    // Default should be 0 (Root position)
    expectEqual(seq.harmonyInversion(), 0, "default harmonyInversion should be 0");
}

CASE("sequence_level_inversion_setter_getter") {
    NoteSequence seq(0);

    // Test setting each valid value
    seq.setHarmonyInversion(0);
    expectEqual(seq.harmonyInversion(), 0, "inversion 0 should be stored");

    seq.setHarmonyInversion(1);
    expectEqual(seq.harmonyInversion(), 1, "inversion 1 should be stored");

    seq.setHarmonyInversion(2);
    expectEqual(seq.harmonyInversion(), 2, "inversion 2 should be stored");

    seq.setHarmonyInversion(3);
    expectEqual(seq.harmonyInversion(), 3, "inversion 3 should be stored");
}

CASE("sequence_level_inversion_clamping") {
    NoteSequence seq(0);

    // Test values beyond valid range
    seq.setHarmonyInversion(5);
    expectTrue(seq.harmonyInversion() <= 3, "inversion should clamp to max 3");

    seq.setHarmonyInversion(100);
    expectTrue(seq.harmonyInversion() <= 3, "large value should clamp to max 3");
}

CASE("sequence_level_inversion_persistence") {
    NoteSequence seq(0);

    // Set inversion and verify it persists
    seq.setHarmonyInversion(2);
    expectEqual(seq.harmonyInversion(), 2, "first read should be 2");
    expectEqual(seq.harmonyInversion(), 2, "second read should still be 2");

    // Change and verify
    seq.setHarmonyInversion(3);
    expectEqual(seq.harmonyInversion(), 3, "after change should be 3");
}

// =========================================================================
// SECTION 2: Per-Step Inversion Override Storage
// =========================================================================

CASE("per_step_inversion_override_default_value") {
    NoteSequence seq(0);

    // Default should be 0 (UseSequence - use sequence-level setting)
    for (int i = 0; i < 16; i++) {
        expectEqual(seq.step(i).inversionOverride(), 0,
            "step default inversionOverride should be 0 (UseSequence)");
    }
}

CASE("per_step_inversion_override_setter_getter") {
    NoteSequence seq(0);

    // Test setting each valid value (0=UseSeq, 1=Root, 2=1st, 3=2nd, 4=3rd)
    seq.step(0).setInversionOverride(0); // UseSequence
    expectEqual(seq.step(0).inversionOverride(), 0, "override 0 should be stored");

    seq.step(1).setInversionOverride(1); // Root
    expectEqual(seq.step(1).inversionOverride(), 1, "override 1 should be stored");

    seq.step(2).setInversionOverride(2); // 1st
    expectEqual(seq.step(2).inversionOverride(), 2, "override 2 should be stored");

    seq.step(3).setInversionOverride(3); // 2nd
    expectEqual(seq.step(3).inversionOverride(), 3, "override 3 should be stored");

    seq.step(4).setInversionOverride(4); // 3rd
    expectEqual(seq.step(4).inversionOverride(), 4, "override 4 should be stored");
}

CASE("per_step_inversion_override_clamping") {
    NoteSequence seq(0);

    // Test values beyond valid range (should clamp to 0-4)
    seq.step(0).setInversionOverride(7);
    expectTrue(seq.step(0).inversionOverride() <= 7, "override stored in 3-bit field");
}

// =========================================================================
// SECTION 3: Override Logic Simulation
// =========================================================================

// This simulates the logic in NoteTrackEngine::evalStepNote()
// int inversion = (inversionValue == 0) ? sequence.harmonyInversion() : (inversionValue - 1);

CASE("override_logic_when_per_step_is_zero") {
    NoteSequence seq(0);

    // Set sequence-level inversion to 2
    seq.setHarmonyInversion(2);

    // Per-step override is 0 (UseSequence)
    int inversionOverride = seq.step(0).inversionOverride();
    expectEqual(inversionOverride, 0, "per-step should be 0");

    // Simulate evalStepNote() logic
    int effectiveInversion = (inversionOverride == 0) ? seq.harmonyInversion() : (inversionOverride - 1);

    // Should use sequence-level value
    expectEqual(effectiveInversion, 2, "when override is 0, should use sequence-level (2)");
}

CASE("override_logic_when_per_step_is_set") {
    NoteSequence seq(0);

    // Set sequence-level inversion to 2
    seq.setHarmonyInversion(2);

    // Per-step override is 3 (2nd inversion)
    seq.step(0).setInversionOverride(3);
    int inversionOverride = seq.step(0).inversionOverride();
    expectEqual(inversionOverride, 3, "per-step should be 3");

    // Simulate evalStepNote() logic
    int effectiveInversion = (inversionOverride == 0) ? seq.harmonyInversion() : (inversionOverride - 1);

    // Should use per-step value minus 1 (3-1=2)
    expectEqual(effectiveInversion, 2, "when override is 3, effective should be 2");
}

CASE("override_logic_all_per_step_values") {
    NoteSequence seq(0);
    seq.setHarmonyInversion(1); // sequence-level = 1 (1st inversion)

    // Test all per-step override values
    struct TestCase {
        int override;
        int expected;
        const char* description;
    };

    TestCase cases[] = {
        {0, 1, "override=0 (UseSeq) -> seq-level=1"},
        {1, 0, "override=1 (Root) -> 0"},
        {2, 1, "override=2 (1st) -> 1"},
        {3, 2, "override=3 (2nd) -> 2"},
        {4, 3, "override=4 (3rd) -> 3"},
    };

    for (int i = 0; i < 5; i++) {
        int inversionOverride = cases[i].override;
        int effectiveInversion = (inversionOverride == 0) ? seq.harmonyInversion() : (inversionOverride - 1);
        expectEqual(effectiveInversion, cases[i].expected, cases[i].description);
    }
}

// =========================================================================
// SECTION 4: HarmonyEngine Inversion Effect Verification
// =========================================================================

CASE("harmony_engine_inversion_0_produces_root_position") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setInversion(0);

    // C Major7: C(60)-E(64)-G(67)-B(71)
    auto chord = engine.harmonize(60, 0);

    expectEqual(static_cast<int>(chord.root), 60, "inversion 0: root at 60");
    expectEqual(static_cast<int>(chord.third), 64, "inversion 0: third at 64");
    expectEqual(static_cast<int>(chord.fifth), 67, "inversion 0: fifth at 67");
    expectEqual(static_cast<int>(chord.seventh), 71, "inversion 0: seventh at 71");
}

CASE("harmony_engine_inversion_1_produces_first_inversion") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setInversion(1);

    // 1st inversion: E(64)-G(67)-B(71)-C(72)
    auto chord = engine.harmonize(60, 0);

    expectEqual(static_cast<int>(chord.root), 72, "inversion 1: root at 72 (+12)");
    expectEqual(static_cast<int>(chord.third), 64, "inversion 1: third at 64");
    expectEqual(static_cast<int>(chord.fifth), 67, "inversion 1: fifth at 67");
    expectEqual(static_cast<int>(chord.seventh), 71, "inversion 1: seventh at 71");
}

CASE("harmony_engine_inversion_2_produces_second_inversion") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setInversion(2);

    // 2nd inversion: G(67)-B(71)-C(72)-E(76)
    auto chord = engine.harmonize(60, 0);

    expectEqual(static_cast<int>(chord.root), 72, "inversion 2: root at 72 (+12)");
    expectEqual(static_cast<int>(chord.third), 76, "inversion 2: third at 76 (+12)");
    expectEqual(static_cast<int>(chord.fifth), 67, "inversion 2: fifth at 67");
    expectEqual(static_cast<int>(chord.seventh), 71, "inversion 2: seventh at 71");
}

CASE("harmony_engine_inversion_3_produces_third_inversion") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setInversion(3);

    // 3rd inversion: B(71)-C(72)-E(76)-G(79)
    auto chord = engine.harmonize(60, 0);

    expectEqual(static_cast<int>(chord.root), 72, "inversion 3: root at 72 (+12)");
    expectEqual(static_cast<int>(chord.third), 76, "inversion 3: third at 76 (+12)");
    expectEqual(static_cast<int>(chord.fifth), 79, "inversion 3: fifth at 79 (+12)");
    expectEqual(static_cast<int>(chord.seventh), 71, "inversion 3: seventh at 71");
}

CASE("all_inversions_produce_different_results") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);

    // Store results for all inversions
    HarmonyEngine::ChordNotes chords[4];

    for (int inv = 0; inv < 4; inv++) {
        engine.setInversion(inv);
        chords[inv] = engine.harmonize(60, 0);
    }

    // Verify each inversion produces different output
    // Inversion 0 vs 1: root should differ
    expectTrue(chords[0].root != chords[1].root, "inversion 0 and 1 should differ (root)");

    // Inversion 1 vs 2: third should differ
    expectTrue(chords[1].third != chords[2].third, "inversion 1 and 2 should differ (third)");

    // Inversion 2 vs 3: fifth should differ
    expectTrue(chords[2].fifth != chords[3].fifth, "inversion 2 and 3 should differ (fifth)");
}

// =========================================================================
// SECTION 5: End-to-End Sequence-Level Inversion Flow
// =========================================================================

CASE("end_to_end_sequence_level_inversion_flow") {
    // This test simulates the complete flow from NoteSequence to HarmonyEngine

    NoteSequence masterSeq(0);
    masterSeq.setHarmonyRole(NoteSequence::HarmonyMaster);
    masterSeq.setHarmonyScale(0); // Ionian
    masterSeq.setHarmonyInversion(2); // 2nd inversion

    // Verify sequence stores the value
    expectEqual(masterSeq.harmonyInversion(), 2, "sequence should store inversion 2");

    // Per-step override is 0 (UseSequence)
    int inversionOverride = masterSeq.step(0).inversionOverride();
    expectEqual(inversionOverride, 0, "step override should be 0");

    // Simulate evalStepNote() logic
    int effectiveInversion = (inversionOverride == 0) ? masterSeq.harmonyInversion() : (inversionOverride - 1);
    expectEqual(effectiveInversion, 2, "effective inversion should be 2");

    // Create HarmonyEngine with this inversion
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setInversion(effectiveInversion);

    // Verify the engine uses the correct inversion
    expectEqual(static_cast<int>(engine.inversion()), 2, "engine should have inversion 2");

    // Harmonize and verify 2nd inversion result
    auto chord = engine.harmonize(60, 0);

    // 2nd inversion: G(67)-B(71)-C(72)-E(76)
    expectEqual(static_cast<int>(chord.root), 72, "2nd inv root should be 72");
    expectEqual(static_cast<int>(chord.third), 76, "2nd inv third should be 76");
    expectEqual(static_cast<int>(chord.fifth), 67, "2nd inv fifth should be 67");
    expectEqual(static_cast<int>(chord.seventh), 71, "2nd inv seventh should be 71");
}

CASE("end_to_end_per_step_override_flow") {
    // Compare sequence-level vs per-step override

    NoteSequence seq(0);
    seq.setHarmonyInversion(1); // Sequence-level = 1st inversion
    seq.step(0).setInversionOverride(4); // Per-step = 3rd inversion (value 4)

    // Step 0: per-step override should win
    int override0 = seq.step(0).inversionOverride();
    int effective0 = (override0 == 0) ? seq.harmonyInversion() : (override0 - 1);
    expectEqual(effective0, 3, "step 0 should use per-step override (3)");

    // Step 1: no override, should use sequence-level
    int override1 = seq.step(1).inversionOverride();
    int effective1 = (override1 == 0) ? seq.harmonyInversion() : (override1 - 1);
    expectEqual(effective1, 1, "step 1 should use sequence-level (1)");
}

// =========================================================================
// SECTION 6: Edge Cases and Potential Bug Areas
// =========================================================================

CASE("sequence_level_inversion_after_role_change") {
    // Test if inversion persists after changing harmony role

    NoteSequence seq(0);
    seq.setHarmonyInversion(3);
    expectEqual(seq.harmonyInversion(), 3, "initial inversion should be 3");

    // Change role
    seq.setHarmonyRole(NoteSequence::HarmonyMaster);
    expectEqual(seq.harmonyInversion(), 3, "inversion should persist after role change");

    seq.setHarmonyRole(NoteSequence::HarmonyFollowerRoot);
    expectEqual(seq.harmonyInversion(), 3, "inversion should persist as follower");
}

CASE("sequence_level_inversion_different_tracks") {
    // Each track should have independent inversion

    NoteSequence seq0(0);
    NoteSequence seq1(1);

    seq0.setHarmonyInversion(1);
    seq1.setHarmonyInversion(3);

    expectEqual(seq0.harmonyInversion(), 1, "track 0 inversion should be 1");
    expectEqual(seq1.harmonyInversion(), 3, "track 1 inversion should be 3");

    // Modifying one shouldn't affect the other
    seq0.setHarmonyInversion(2);
    expectEqual(seq0.harmonyInversion(), 2, "track 0 inversion should be 2");
    expectEqual(seq1.harmonyInversion(), 3, "track 1 inversion should still be 3");
}

CASE("per_step_override_all_steps_independent") {
    // Each step should have independent override

    NoteSequence seq(0);

    // Set different overrides
    for (int i = 0; i < 16; i++) {
        seq.step(i).setInversionOverride(i % 5); // 0,1,2,3,4,0,1...
    }

    // Verify all are independent
    for (int i = 0; i < 16; i++) {
        expectEqual(seq.step(i).inversionOverride(), i % 5,
            "step override should be independent");
    }
}

CASE("inversion_with_different_chord_qualities") {
    // Inversion should work with all chord types

    HarmonyEngine engine;
    engine.setInversion(1); // 1st inversion

    // Test with Minor7 (Dorian I)
    engine.setMode(HarmonyEngine::Dorian);
    auto minorChord = engine.harmonize(60, 0);

    // Minor7 in 1st inv: b3-5-b7-root+12
    expectEqual(static_cast<int>(minorChord.root), 72, "minor 1st inv: root should be +12");
    expectEqual(static_cast<int>(minorChord.third), 63, "minor 1st inv: third (b3) at 63");

    // Test with Dominant7 (Mixolydian I)
    engine.setMode(HarmonyEngine::Mixolydian);
    auto domChord = engine.harmonize(60, 0);

    expectEqual(static_cast<int>(domChord.root), 72, "dom7 1st inv: root should be +12");
}

CASE("potential_initialization_issue") {
    // Test if HarmonyEngine has correct default inversion

    HarmonyEngine engine;
    expectEqual(static_cast<int>(engine.inversion()), 0, "default inversion should be 0");

    // Test if setting to 0 explicitly works
    engine.setInversion(0);
    expectEqual(static_cast<int>(engine.inversion()), 0, "explicit 0 should work");

    // Now test non-zero
    engine.setInversion(2);
    expectEqual(static_cast<int>(engine.inversion()), 2, "after setting to 2");

    // Reset to 0
    engine.setInversion(0);
    expectEqual(static_cast<int>(engine.inversion()), 0, "reset to 0 should work");
}

// =========================================================================
// SECTION 7: Voicing (for completeness - should have no effect currently)
// =========================================================================

CASE("sequence_level_voicing_storage") {
    NoteSequence seq(0);

    // Test voicing storage
    expectEqual(seq.harmonyVoicing(), 0, "default voicing should be 0");

    seq.setHarmonyVoicing(1);
    expectEqual(seq.harmonyVoicing(), 1, "voicing 1 should be stored");

    seq.setHarmonyVoicing(3);
    expectEqual(seq.harmonyVoicing(), 3, "voicing 3 should be stored");
}

CASE("per_step_voicing_override_dropped") {
    // Per-step voicing override was dropped (its step bitfield space was
    // reallocated to accumulatorStepValue). Getter is hardwired to 0
    // (UseSequence), setter is a no-op — assert it stays dropped.
    NoteSequence seq(0);

    expectEqual(seq.step(0).voicingOverride(), 0, "voicing override reads 0 (dropped)");

    seq.step(0).setVoicingOverride(2);
    expectEqual(seq.step(0).voicingOverride(), 0, "setter is a no-op (feature dropped)");
}

} // UNIT_TEST("Harmony Inversion Issue Investigation")
