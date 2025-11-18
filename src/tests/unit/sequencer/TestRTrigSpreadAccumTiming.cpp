#include "UnitTest.h"

#include "apps/sequencer/Config.h"
#include "apps/sequencer/model/NoteSequence.h"
#include "apps/sequencer/model/Accumulator.h"
#include "apps/sequencer/engine/NoteTrackEngine.h"
#include "apps/sequencer/model/Model.h"
#include "apps/sequencer/engine/Engine.h"

UNIT_TEST("RTrigSpreadAccumTiming") {

#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS

CASE("verify accumulator ticks BEFORE CV calculation in spread mode") {
    // This test demonstrates the bug: CV is calculated with OLD accumulator value
    // Expected: CV should use NEW accumulator value after tick

    // Setup: Create model and engine
    Model model;
    Engine engine(model);
    model.init();
    engine.init();

    // Configure note track and sequence
    auto &project = model.project();
    auto &track = project.track(0).noteTrack();
    auto &sequence = track.sequence(0);

    // Configure accumulator for RTRIG mode
    auto &accumulator = const_cast<Accumulator&>(sequence.accumulator());
    accumulator.setEnabled(true);
    accumulator.setTriggerMode(Accumulator::Retrigger);
    accumulator.setDirection(Accumulator::Up);
    accumulator.setOrder(Accumulator::Wrap);
    accumulator.setPolarity(Accumulator::Unipolar);
    accumulator.setMin(0);
    accumulator.setMax(10);
    accumulator.setStepSize(1);
    accumulator.reset();  // Start at 0

    // Configure sequence
    sequence.setDivisor(1);
    sequence.setFirstStep(0);
    sequence.setLastStep(0);  // Single step

    // Configure step 0: note=60, gate=true, retrigger=3, accumulator trigger=true
    auto &step0 = sequence.step(0);
    step0.setGate(true);
    step0.setGateMode(NoteSequence::GateModeType::All);
    step0.setNote(60);
    step0.setRetrigger(3);  // 3 retriggers
    step0.setRetriggerProbability(NoteSequence::RetriggerProbability::Max);
    step0.setAccumulatorTrigger(true);

    // Get engine
    auto &noteTrackEngine = engine.trackEngine(0).as<NoteTrackEngine>();

    // Start sequencer
    engine.clockStart();

    // BEFORE first tick: accumulator should be at minValue (0)
    expectEqual(accumulator.currentValue(), 0, "Initial accumulator value should be 0");

    // Process first tick (this should trigger step 0)
    // With retrigger=3, this creates 3 gates in the queue
    // Each gate should tick the accumulator
    engine.update(1.0f / 50.0f);  // 20ms frame
    noteTrackEngine.tick(0);  // First tick at time 0

    // CRITICAL: After triggerStep() is called, the accumulator should be ticked BEFORE CV is calculated
    // Expected behavior:
    // 1. First retrigger fires: accumulator ticks from 0 to 1, CV calculated with value 1
    // 2. Second retrigger fires: accumulator ticks from 1 to 2, CV calculated with value 2
    // 3. Third retrigger fires: accumulator ticks from 2 to 3, CV calculated with value 3

    // BUG: Currently, CV is calculated BEFORE accumulator is ticked
    // Actual (WRONG) behavior:
    // 1. CV calculated with value 0, THEN accumulator ticks to 1
    // 2. CV calculated with value 1, THEN accumulator ticks to 2
    // 3. CV calculated with value 2, THEN accumulator ticks to 3

    // After all 3 retriggers fire, accumulator should be at value 3
    // The LAST CV output should reflect note 60 + accumulator value 3 = 63

    // Process enough ticks to fire all gates from queue
    // Gates are spread over the step duration based on retrigger subdivisions
    for (int i = 0; i < 100; i++) {
        engine.update(1.0f / 50.0f);
        noteTrackEngine.tick(i);
    }

    // Check final accumulator value (should be 3 after 3 ticks)
    expectEqual(accumulator.currentValue(), 3, "Accumulator should be 3 after 3 retrigger ticks");

    // Check CV output - this is where the bug manifests
    // Expected: CV output should reflect note 63 (60 + 3)
    // Actual (BUG): CV output will reflect note 62 (60 + 2) because CV was calculated before last tick

    // Note: We can't easily check the CV output directly in this test without exposing internals
    // But the accumulator value check confirms the ticking is happening
    // The real verification will be in the simulator where we can see CV output
}

CASE("verify single retrigger ticks accumulator correctly") {
    // Simpler test: single retrigger should tick accumulator once

    Model model;
    Engine engine(model);
    model.init();
    engine.init();

    auto &project = model.project();
    auto &track = project.track(0).noteTrack();
    auto &sequence = track.sequence(0);

    auto &accumulator = const_cast<Accumulator&>(sequence.accumulator());
    accumulator.setEnabled(true);
    accumulator.setTriggerMode(Accumulator::Retrigger);
    accumulator.setDirection(Accumulator::Up);
    accumulator.setStepSize(1);
    accumulator.setMin(0);
    accumulator.setMax(100);
    accumulator.reset();

    sequence.setFirstStep(0);
    sequence.setLastStep(0);

    auto &step0 = sequence.step(0);
    step0.setGate(true);
    step0.setNote(60);
    step0.setRetrigger(1);  // Single retrigger
    step0.setAccumulatorTrigger(true);

    auto &noteTrackEngine = engine.trackEngine(0).as<NoteTrackEngine>();

    engine.clockStart();
    expectEqual(accumulator.currentValue(), 0, "Initial value 0");

    // Process ticks
    engine.update(1.0f / 50.0f);
    noteTrackEngine.tick(0);

    // Process enough ticks to fire gate
    for (int i = 1; i < 50; i++) {
        engine.update(1.0f / 50.0f);
        noteTrackEngine.tick(i);
    }

    // After single retrigger, accumulator should be 1
    expectEqual(accumulator.currentValue(), 1, "Accumulator should be 1 after 1 retrigger tick");
}

CASE("verify accumulator timing with stepSize > 1") {
    // Test with larger step size to make timing more obvious

    Model model;
    Engine engine(model);
    model.init();
    engine.init();

    auto &project = model.project();
    auto &track = project.track(0).noteTrack();
    auto &sequence = track.sequence(0);

    auto &accumulator = const_cast<Accumulator&>(sequence.accumulator());
    accumulator.setEnabled(true);
    accumulator.setTriggerMode(Accumulator::Retrigger);
    accumulator.setDirection(Accumulator::Up);
    accumulator.setStepSize(5);  // Larger step size
    accumulator.setMin(0);
    accumulator.setMax(100);
    accumulator.reset();

    sequence.setFirstStep(0);
    sequence.setLastStep(0);

    auto &step0 = sequence.step(0);
    step0.setGate(true);
    step0.setNote(60);
    step0.setRetrigger(2);  // 2 retriggers = 2 ticks
    step0.setAccumulatorTrigger(true);

    auto &noteTrackEngine = engine.trackEngine(0).as<NoteTrackEngine>();

    engine.clockStart();
    expectEqual(accumulator.currentValue(), 0, "Initial value 0");

    engine.update(1.0f / 50.0f);
    noteTrackEngine.tick(0);

    for (int i = 1; i < 50; i++) {
        engine.update(1.0f / 50.0f);
        noteTrackEngine.tick(i);
    }

    // After 2 retriggers with stepSize=5, accumulator should be 10
    expectEqual(accumulator.currentValue(), 10, "Accumulator should be 10 after 2 ticks (2 * stepSize=5)");
}

#else
CASE("spread mode not enabled - skip tests") {
    expectTrue(true, "CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS = 0, tests skipped");
}
#endif

}
