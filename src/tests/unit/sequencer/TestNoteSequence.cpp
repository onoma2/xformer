#include "UnitTest.h"

#include "apps/sequencer/model/NoteSequence.h"

UNIT_TEST("NoteSequence") {

CASE("step_is_accumulator_trigger") {
    NoteSequence::Step step;
    step.setAccumulatorTrigger(true);
    expectTrue(step.isAccumulatorTrigger(), "isAccumulatorTrigger should be true");
    step.setAccumulatorTrigger(false);
    expectFalse(step.isAccumulatorTrigger(), "isAccumulatorTrigger should be false");
    step.toggleAccumulatorTrigger();
    expectTrue(step.isAccumulatorTrigger(), "isAccumulatorTrigger should be true after toggle");
}

CASE("note_sequence_has_accumulator") {
    NoteSequence noteSequence;
    Accumulator &accumulator = noteSequence.accumulator();
    accumulator.setEnabled(true);
    expectTrue(noteSequence.accumulator().enabled(), "accumulator should be enabled");
}

CASE("harmony_properties") {
    NoteSequence seq(0);

    // Test default values
    expectEqual(static_cast<int>(seq.harmonyRole()), static_cast<int>(NoteSequence::HarmonyOff), "default harmonyRole should be HarmonyOff");
    expectEqual(seq.masterTrackIndex(), 0, "default masterTrackIndex should be 0");
    expectEqual(seq.harmonyScale(), 0, "default harmonyScale should be 0");

    // Test harmonyScale setter and getter
    seq.setHarmonyScale(3); // Lydian mode
    expectEqual(seq.harmonyScale(), 3, "harmonyScale should be 3 after setting");

    // Test clamping (0-6 for 7 modes)
    seq.setHarmonyScale(10); // Should clamp to 6
    expectEqual(seq.harmonyScale(), 6, "harmonyScale should clamp to 6");

    seq.setHarmonyScale(-5); // Should clamp to 0
    expectEqual(seq.harmonyScale(), 0, "harmonyScale should clamp to 0");

    // Test masterTrackIndex setter and getter
    seq.setMasterTrackIndex(4); // Track 5
    expectEqual(seq.masterTrackIndex(), 4, "masterTrackIndex should be 4 after setting");

    // Test clamping (0-7 for 8 tracks)
    seq.setMasterTrackIndex(12); // Should clamp to 7
    expectEqual(seq.masterTrackIndex(), 7, "masterTrackIndex should clamp to 7");

    seq.setMasterTrackIndex(-3); // Should clamp to 0
    expectEqual(seq.masterTrackIndex(), 0, "masterTrackIndex should clamp to 0");

    // Test all HarmonyRole values
    seq.setHarmonyRole(NoteSequence::HarmonyFollowerRoot);
    expectEqual(static_cast<int>(seq.harmonyRole()), static_cast<int>(NoteSequence::HarmonyFollowerRoot), "harmonyRole should be HarmonyFollowerRoot");

    seq.setHarmonyRole(NoteSequence::HarmonyFollower5th);
    expectEqual(static_cast<int>(seq.harmonyRole()), static_cast<int>(NoteSequence::HarmonyFollower5th), "harmonyRole should be HarmonyFollower5th");

    seq.setHarmonyRole(NoteSequence::HarmonyFollower7th);
    expectEqual(static_cast<int>(seq.harmonyRole()), static_cast<int>(NoteSequence::HarmonyFollower7th), "harmonyRole should be HarmonyFollower7th");
}

CASE("harmony_inversion_and_voicing") {
    NoteSequence seq(0);

    // Test harmonyInversion default value
    expectEqual(seq.harmonyInversion(), 0, "default harmonyInversion should be 0 (root position)");

    // Test harmonyInversion setter and getter
    seq.setHarmonyInversion(1); // 1st inversion
    expectEqual(seq.harmonyInversion(), 1, "harmonyInversion should be 1 after setting");

    seq.setHarmonyInversion(2); // 2nd inversion
    expectEqual(seq.harmonyInversion(), 2, "harmonyInversion should be 2 after setting");

    seq.setHarmonyInversion(3); // 3rd inversion
    expectEqual(seq.harmonyInversion(), 3, "harmonyInversion should be 3 after setting");

    // Test clamping (0-3 for 4 inversions)
    seq.setHarmonyInversion(5); // Should clamp to 3
    expectEqual(seq.harmonyInversion(), 3, "harmonyInversion should clamp to 3");

    seq.setHarmonyInversion(-2); // Should clamp to 0
    expectEqual(seq.harmonyInversion(), 0, "harmonyInversion should clamp to 0");

    // Test harmonyVoicing default value
    expectEqual(seq.harmonyVoicing(), 0, "default harmonyVoicing should be 0 (Close)");

    // Test harmonyVoicing setter and getter
    seq.setHarmonyVoicing(1); // Drop2
    expectEqual(seq.harmonyVoicing(), 1, "harmonyVoicing should be 1 (Drop2) after setting");

    seq.setHarmonyVoicing(2); // Drop3
    expectEqual(seq.harmonyVoicing(), 2, "harmonyVoicing should be 2 (Drop3) after setting");

    seq.setHarmonyVoicing(3); // Spread
    expectEqual(seq.harmonyVoicing(), 3, "harmonyVoicing should be 3 (Spread) after setting");

    // Test clamping (0-3 for 4 voicings)
    seq.setHarmonyVoicing(7); // Should clamp to 3
    expectEqual(seq.harmonyVoicing(), 3, "harmonyVoicing should clamp to 3");

    seq.setHarmonyVoicing(-1); // Should clamp to 0
    expectEqual(seq.harmonyVoicing(), 0, "harmonyVoicing should clamp to 0");
}

} // UNIT_TEST("NoteSequence")
