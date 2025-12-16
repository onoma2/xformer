#include "UnitTest.h"

#include "apps/sequencer/model/DiscreteMapSequence.h"

UNIT_TEST("DiscreteMapSequence") {

CASE("default_values") {
    DiscreteMapSequence seq;
    seq.clear();
    expectEqual(static_cast<int>(seq.clockSource()), static_cast<int>(DiscreteMapSequence::ClockSource::Internal), "clock source");
    expectEqual(seq.divisor(), 192, "divisor");
    expectEqual(seq.gateLength(), 1, "gate length");
    expectTrue(seq.loop(), "loop enabled");
    expectEqual(static_cast<int>(seq.thresholdMode()), static_cast<int>(DiscreteMapSequence::ThresholdMode::Position), "threshold mode");
    expectEqual(seq.scale(), -1, "scale default (project)");
    expectEqual(seq.rootNote(), 0, "root note");
    expectFalse(seq.slewEnabled(), "slew off");
    for (int i = 0; i < DiscreteMapSequence::StageCount; ++i) {
        const auto &stage = seq.stage(i);
        expectEqual(int(stage.threshold()), 0, "threshold init");
        expectEqual(static_cast<int>(stage.direction()), static_cast<int>(DiscreteMapSequence::Stage::TriggerDir::Off), "direction init");
        expectEqual(int(stage.noteIndex()), 0, "note index init");
    }
}

CASE("stage_threshold_clamp") {
    DiscreteMapSequence seq;
    auto &stage = seq.stage(0);
    stage.setThreshold(-100);
    expectEqual(int(stage.threshold()), -100, "negative threshold");
    stage.setThreshold(120);
    expectEqual(int(stage.threshold()), 120, "positive threshold");
    stage.setThreshold(200);
    expectEqual(int(stage.threshold()), 127, "clamped max");
    stage.setThreshold(-200);
    expectEqual(int(stage.threshold()), -127, "clamped min");
}

CASE("stage_note_index_clamp") {
    DiscreteMapSequence seq;
    auto &stage = seq.stage(0);
    stage.setNoteIndex(60);
    expectEqual(int(stage.noteIndex()), 60, "within range");
    stage.setNoteIndex(90);
    expectEqual(int(stage.noteIndex()), 64, "clamped max");
    stage.setNoteIndex(-90);
    expectEqual(int(stage.noteIndex()), -63, "clamped min");
}

CASE("toggle_methods") {
    DiscreteMapSequence seq;
    seq.toggleClockSource();
    expectEqual(static_cast<int>(seq.clockSource()), static_cast<int>(DiscreteMapSequence::ClockSource::External), "clock toggled");
    seq.toggleThresholdMode();
    expectEqual(static_cast<int>(seq.thresholdMode()), static_cast<int>(DiscreteMapSequence::ThresholdMode::Length), "threshold toggled");
    seq.toggleLoop();
    expectFalse(seq.loop(), "loop toggled");
    seq.toggleSlew();
    expectTrue(seq.slewEnabled(), "slew toggled");
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

} // UNIT_TEST
