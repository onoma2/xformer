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

} // UNIT_TEST("Model")
