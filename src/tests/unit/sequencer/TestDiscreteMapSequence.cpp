#include "UnitTest.h"

#include "apps/sequencer/model/DiscreteMapSequence.h"

// clear() seeds a musical fret-pattern of thresholds (not all-zero); the
// randomize change-detection cases need a known all-zero baseline, so they zero
// the stages explicitly after clear().
static void zeroStages(DiscreteMapSequence &seq) {
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        seq.stage(i).clear();
    }
}

UNIT_TEST("DiscreteMapSequence") {

CASE("default_values") {
    DiscreteMapSequence seq;
    seq.clear();
    expectEqual(static_cast<int>(seq.clockSource()), static_cast<int>(DiscreteMapSequence::ClockSource::Internal), "clock source");
    expectEqual(seq.divisor(), 192, "divisor");
    expectEqual(seq.gateLength(), 0, "gate length (0 = 1T pulse)");
    expectTrue(seq.loop(), "loop enabled");
    expectEqual(static_cast<int>(seq.thresholdMode()), static_cast<int>(DiscreteMapSequence::ThresholdMode::Position), "threshold mode");
    expectEqual(seq.scale(), -1, "scale default (project)");
    expectEqual(seq.rootNote(), 0, "root note");
    expectFalse(seq.slewEnabled(), "slew off");
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        const auto &stage = seq.stage(i);
        // clear() seeds a fret pattern, so threshold defaults are non-zero but in range
        expectTrue(int(stage.threshold()) >= -100 && int(stage.threshold()) <= 100, "threshold in range");
        expectEqual(static_cast<int>(stage.direction()), static_cast<int>(DiscreteMapSequence::Stage::TriggerDir::Off), "direction init");
        expectEqual(int(stage.noteIndex()), 0, "note index init");
    }
}

CASE("stage_threshold_clamp") {
    DiscreteMapSequence seq;
    auto &stage = seq.stage(0);
    stage.setThreshold(-100);
    expectEqual(int(stage.threshold()), -100, "negative threshold");
    stage.setThreshold(50);
    expectEqual(int(stage.threshold()), 50, "positive threshold");
    stage.setThreshold(200);
    expectEqual(int(stage.threshold()), 100, "clamped max");
    stage.setThreshold(-200);
    expectEqual(int(stage.threshold()), -100, "clamped min");
}

CASE("stage_note_index_clamp") {
    DiscreteMapSequence seq;
    auto &stage = seq.stage(0);
    stage.setNoteIndex(60);
    expectEqual(int(stage.noteIndex()), 60, "within range");
    stage.setNoteIndex(90);
    expectEqual(int(stage.noteIndex()), 63, "clamped max");
    stage.setNoteIndex(-90);
    expectEqual(int(stage.noteIndex()), -63, "clamped min");
}

CASE("toggle_methods") {
    DiscreteMapSequence seq;
    seq.toggleClockSource();   // 3-way cycle: Internal -> InternalTriangle -> External
    expectEqual(static_cast<int>(seq.clockSource()), static_cast<int>(DiscreteMapSequence::ClockSource::InternalTriangle), "clock toggled to InternalTriangle");
    seq.toggleThresholdMode();
    expectEqual(static_cast<int>(seq.thresholdMode()), static_cast<int>(DiscreteMapSequence::ThresholdMode::Length), "threshold toggled");
    seq.toggleLoop();
    expectFalse(seq.loop(), "loop toggled");
    seq.setSlewTime(50);
    expectTrue(seq.slewEnabled(), "slew enabled");
}

CASE("gate_length_clamp") {
    DiscreteMapSequence seq;
    seq.setGateLength(-10);
    expectEqual(seq.gateLength(), 0, "clamp min");
    seq.setGateLength(150);
    expectEqual(seq.gateLength(), 100, "clamp max");
    seq.setGateLength(50);
    expectEqual(seq.gateLength(), 50, "valid");
}

CASE("randomize_thresholds") {
    DiscreteMapSequence seq;
    seq.clear(); zeroStages(seq); // clear() seeds a fret pattern; zero for change-detection

    // Verify initial state
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        expectEqual(int(seq.stage(i).threshold()), 0, "initial threshold");
        expectEqual(static_cast<int>(seq.stage(i).direction()), static_cast<int>(DiscreteMapSequence::Stage::TriggerDir::Off), "initial direction");
        expectEqual(int(seq.stage(i).noteIndex()), 0, "initial note index");
    }

    // Randomize only thresholds
    seq.randomizeThresholds();

    // Verify thresholds are randomized but other values remain unchanged
    bool anyThresholdChanged = false;
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        if (seq.stage(i).threshold() != 0) {
            anyThresholdChanged = true;
        }
        expectEqual(static_cast<int>(seq.stage(i).direction()), static_cast<int>(DiscreteMapSequence::Stage::TriggerDir::Off), "direction unchanged");
        expectEqual(int(seq.stage(i).noteIndex()), 0, "note index unchanged");
    }
    expectTrue(anyThresholdChanged, "at least one threshold changed");
}

CASE("randomize_notes") {
    DiscreteMapSequence seq;
    seq.clear(); zeroStages(seq); // clear() seeds a fret pattern; zero for change-detection

    // Verify initial state
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        expectEqual(int(seq.stage(i).threshold()), 0, "initial threshold");
        expectEqual(static_cast<int>(seq.stage(i).direction()), static_cast<int>(DiscreteMapSequence::Stage::TriggerDir::Off), "initial direction");
        expectEqual(int(seq.stage(i).noteIndex()), 0, "initial note index");
    }

    // Randomize only notes
    seq.randomizeNotes();

    // Verify notes are randomized but other values remain unchanged
    bool anyNoteChanged = false;
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        if (seq.stage(i).noteIndex() != 0) {
            anyNoteChanged = true;
        }
        expectEqual(int(seq.stage(i).threshold()), 0, "threshold unchanged");
        expectEqual(static_cast<int>(seq.stage(i).direction()), static_cast<int>(DiscreteMapSequence::Stage::TriggerDir::Off), "direction unchanged");
    }
    expectTrue(anyNoteChanged, "at least one note changed");
}

CASE("randomize_directions") {
    DiscreteMapSequence seq;
    seq.clear(); zeroStages(seq); // clear() seeds a fret pattern; zero for change-detection

    // Verify initial state
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        expectEqual(int(seq.stage(i).threshold()), 0, "initial threshold");
        expectEqual(static_cast<int>(seq.stage(i).direction()), static_cast<int>(DiscreteMapSequence::Stage::TriggerDir::Off), "initial direction");
        expectEqual(int(seq.stage(i).noteIndex()), 0, "initial note index");
    }

    // Randomize only directions
    seq.randomizeDirections();

    // Verify directions are randomized but other values remain unchanged
    bool anyDirectionChanged = false;
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        if (seq.stage(i).direction() != DiscreteMapSequence::Stage::TriggerDir::Off) {
            anyDirectionChanged = true;
        }
        expectEqual(int(seq.stage(i).threshold()), 0, "threshold unchanged");
        expectEqual(int(seq.stage(i).noteIndex()), 0, "note index unchanged");
    }
    expectTrue(anyDirectionChanged, "at least one direction changed");
}

CASE("randomize_all") {
    DiscreteMapSequence seq;
    seq.clear(); zeroStages(seq); // clear() seeds a fret pattern; zero for change-detection

    // Verify initial state
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        expectEqual(int(seq.stage(i).threshold()), 0, "initial threshold");
        expectEqual(static_cast<int>(seq.stage(i).direction()), static_cast<int>(DiscreteMapSequence::Stage::TriggerDir::Off), "initial direction");
        expectEqual(int(seq.stage(i).noteIndex()), 0, "initial note index");
    }

    // Randomize all parameters
    seq.randomize();

    // Verify all values are randomized
    bool anyThresholdChanged = false;
    bool anyNoteChanged = false;
    bool anyDirectionChanged = false;
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        if (seq.stage(i).threshold() != 0) anyThresholdChanged = true;
        if (seq.stage(i).noteIndex() != 0) anyNoteChanged = true;
        if (seq.stage(i).direction() != DiscreteMapSequence::Stage::TriggerDir::Off) anyDirectionChanged = true;
    }
    // Note: The updated randomize() method now randomizes thresholds, notes, and directions
    expectTrue(anyThresholdChanged, "at least one threshold changed");
    expectTrue(anyNoteChanged, "at least one note changed");
    expectTrue(anyDirectionChanged, "at least one direction changed");
}

CASE("randomize_methods_combined") {
    DiscreteMapSequence seq;
    seq.clear(); zeroStages(seq); // clear() seeds a fret pattern; zero for change-detection

    // First randomize only thresholds
    seq.randomizeThresholds();
    bool anyThresholdChanged = false;
    bool anyNoteChanged = false;
    bool anyDirectionChanged = false;
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        if (seq.stage(i).threshold() != 0) anyThresholdChanged = true;
        if (seq.stage(i).noteIndex() != 0) anyNoteChanged = true;
        if (seq.stage(i).direction() != DiscreteMapSequence::Stage::TriggerDir::Off) anyDirectionChanged = true;
    }
    expectTrue(anyThresholdChanged, "thresholds changed after randomizeThresholds");
    expectFalse(anyNoteChanged, "notes unchanged after randomizeThresholds");
    expectFalse(anyDirectionChanged, "directions unchanged after randomizeThresholds");

    // Reset for next test
    seq.clear(); zeroStages(seq);

    // Now randomize only notes
    seq.randomizeNotes();
    anyThresholdChanged = false;
    anyNoteChanged = false;
    anyDirectionChanged = false;
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        if (seq.stage(i).threshold() != 0) anyThresholdChanged = true;
        if (seq.stage(i).noteIndex() != 0) anyNoteChanged = true;
        if (seq.stage(i).direction() != DiscreteMapSequence::Stage::TriggerDir::Off) anyDirectionChanged = true;
    }
    expectFalse(anyThresholdChanged, "thresholds unchanged after randomizeNotes");
    expectTrue(anyNoteChanged, "notes changed after randomizeNotes");
    expectFalse(anyDirectionChanged, "directions unchanged after randomizeNotes");

    // Reset for next test
    seq.clear(); zeroStages(seq);

    // Now randomize only directions
    seq.randomizeDirections();
    anyThresholdChanged = false;
    anyNoteChanged = false;
    anyDirectionChanged = false;
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        if (seq.stage(i).threshold() != 0) anyThresholdChanged = true;
        if (seq.stage(i).noteIndex() != 0) anyNoteChanged = true;
        if (seq.stage(i).direction() != DiscreteMapSequence::Stage::TriggerDir::Off) anyDirectionChanged = true;
    }
    expectFalse(anyThresholdChanged, "thresholds unchanged after randomizeDirections");
    expectFalse(anyNoteChanged, "notes unchanged after randomizeDirections");
    expectTrue(anyDirectionChanged, "directions changed after randomizeDirections");
}

} // UNIT_TEST
