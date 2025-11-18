#include "UnitTest.h"

#include "apps/sequencer/model/Model.h"
#include "apps/sequencer/model/HarmonyEngine.h"

UNIT_TEST("Model") {

CASE("model_has_harmony_engine") {
    Model model;

    // Model should have a HarmonyEngine
    HarmonyEngine& harmonyEngine = model.harmonyEngine();

    // Should be able to use it
    harmonyEngine.setMode(HarmonyEngine::Ionian);
    expectEqual(static_cast<int>(harmonyEngine.mode()), static_cast<int>(HarmonyEngine::Ionian), "HarmonyEngine mode should be Ionian");

    // Should be able to harmonize
    auto chord = harmonyEngine.harmonize(60, 0);
    expectEqual(static_cast<int>(chord.root), 60, "Should be able to harmonize using Model's HarmonyEngine");
}

CASE("model_harmony_engine_is_persistent") {
    Model model;

    // Configure harmony engine
    model.harmonyEngine().setMode(HarmonyEngine::Dorian);
    model.harmonyEngine().setTranspose(5);

    // Should persist
    expectEqual(static_cast<int>(model.harmonyEngine().mode()), static_cast<int>(HarmonyEngine::Dorian), "HarmonyEngine mode should persist");
    expectEqual(static_cast<int>(model.harmonyEngine().transpose()), 5, "HarmonyEngine transpose should persist");
}

CASE("model_coordinates_harmony_between_sequences") {
    Model model;

    // Configure Model's HarmonyEngine for C Ionian
    model.harmonyEngine().setMode(HarmonyEngine::Ionian);
    model.harmonyEngine().setTranspose(0);

    // Get first track and configure sequence as HarmonyFollowerRoot
    auto& track1 = model.project().track(0);
    auto& seq1 = track1.noteTrack().sequence(0);
    seq1.setHarmonyRole(NoteSequence::HarmonyFollowerRoot);
    seq1.setHarmonyScale(0); // Use Ionian (matching HarmonyEngine)

    // Verify configuration persists
    expectEqual(static_cast<int>(seq1.harmonyRole()), static_cast<int>(NoteSequence::HarmonyFollowerRoot), "Sequence harmony role should be FollowerRoot");
    expectEqual(seq1.harmonyScale(), 0, "Sequence harmony scale should be 0 (Ionian)");

    // Verify Model's HarmonyEngine can harmonize notes
    // Simulate what engine would do: harmonize master note (C/60, scale degree 0)
    auto chord = model.harmonyEngine().harmonize(60, 0);

    // FollowerRoot should get root note
    expectEqual(static_cast<int>(chord.root), 60, "FollowerRoot should get root (C/60) from harmonized chord");

    // Test different follower roles with second track
    auto& track2 = model.project().track(1);
    auto& seq2 = track2.noteTrack().sequence(0);
    seq2.setHarmonyRole(NoteSequence::HarmonyFollower3rd);

    // Follower3rd should get third note
    expectEqual(static_cast<int>(chord.third), 64, "Follower3rd should get third (E/64) from harmonized chord");
}

} // UNIT_TEST("Model")
