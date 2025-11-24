#include "UnitTest.h"
#include "apps/sequencer/model/CurveSequence.h"
#include <cmath>

UNIT_TEST("CurveTrackEngineGlobalPhase") {

CASE("global_phase_offset_logic") {
    const int sequenceLength = 4;
    const float globalPhase = 0.25f; // 1 step offset

    // Simulate being at the very beginning of step 0
    int currentStep = 0;
    float currentStepFraction = 0.0f;

    // Calculate global position
    float globalPos = float(currentStep) + currentStepFraction;
    
    // Apply phase offset
    float phasedGlobalPos = fmod(globalPos + globalPhase * sequenceLength, float(sequenceLength));

    // Determine lookup step and fraction
    int lookupStep = int(phasedGlobalPos);
    float lookupFraction = fmod(phasedGlobalPos, 1.0f);

    expectEqual(lookupStep, 1, "lookup step should be 1");
    expectTrue(fabs(lookupFraction - 0.0f) < 0.001f, "lookup fraction should be 0.0");
}

CASE("global_phase_offset_logic_with_wraparound") {
    const int sequenceLength = 4;
    const float globalPhase = 0.25f; // 1 step offset

    // Simulate being at the very beginning of step 3
    int currentStep = 3;
    float currentStepFraction = 0.5f;

    // Calculate global position
    float globalPos = float(currentStep) + currentStepFraction;
    
    // Apply phase offset
    float phasedGlobalPos = fmod(globalPos + globalPhase * sequenceLength, float(sequenceLength));

    // Determine lookup step and fraction
    int lookupStep = int(phasedGlobalPos);
    float lookupFraction = fmod(phasedGlobalPos, 1.0f);

    // globalPos = 3.5, globalPhase * sequenceLength = 1.0 -> 4.5
    // fmod(4.5, 4.0) -> 0.5
    // lookupStep should be 0, lookupFraction should be 0.5

    expectEqual(lookupStep, 0, "lookup step with wraparound should be 0");
    expectTrue(fabs(lookupFraction - 0.5f) < 0.001f, "lookup fraction with wraparound should be 0.5");
}

} // UNIT_TEST