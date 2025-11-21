#include "UnitTest.h"

#include "apps/sequencer/model/TuesdayTrack.h"

#include <cstring>

UNIT_TEST("TuesdayTrack") {

//----------------------------------------
// Default Values
//----------------------------------------

CASE("default_values") {
    TuesdayTrack track;
    expectEqual(track.algorithm(), 0, "default algorithm should be 0");
    expectEqual(track.flow(), 0, "default flow should be 0");
    expectEqual(track.ornament(), 0, "default ornament should be 0");
    expectEqual(track.power(), 0, "default power should be 0");
    expectEqual(track.loopLength(), 16, "default loopLength index should be 16");
    expectEqual(static_cast<int>(track.cvUpdateMode()), 0, "default cvUpdateMode should be Free (0)");
}

//----------------------------------------
// Algorithm Parameter
//----------------------------------------

CASE("algorithm_setter_getter") {
    TuesdayTrack track;
    track.setAlgorithm(12);
    expectEqual(track.algorithm(), 12, "algorithm should be 12 (RANDOM)");
    track.setAlgorithm(3);
    expectEqual(track.algorithm(), 3, "algorithm should be 3 (MARKOV)");
}

CASE("algorithm_clamping_upper") {
    TuesdayTrack track;
    track.setAlgorithm(100);
    expectEqual(track.algorithm(), 36, "algorithm should clamp to 36 (max)");
}

CASE("algorithm_clamping_lower") {
    TuesdayTrack track;
    track.setAlgorithm(-5);
    expectEqual(track.algorithm(), 0, "algorithm should clamp to 0 (min)");
}

//----------------------------------------
// Flow Parameter
//----------------------------------------

CASE("flow_setter_getter") {
    TuesdayTrack track;
    track.setFlow(8);
    expectEqual(track.flow(), 8, "flow should be 8");
}

CASE("flow_clamping_upper") {
    TuesdayTrack track;
    track.setFlow(20);
    expectEqual(track.flow(), 16, "flow should clamp to 16 (max)");
}

CASE("flow_clamping_lower") {
    TuesdayTrack track;
    track.setFlow(-1);
    expectEqual(track.flow(), 0, "flow should clamp to 0 (min)");
}

//----------------------------------------
// Ornament Parameter
//----------------------------------------

CASE("ornament_setter_getter") {
    TuesdayTrack track;
    track.setOrnament(5);
    expectEqual(track.ornament(), 5, "ornament should be 5");
}

CASE("ornament_clamping_upper") {
    TuesdayTrack track;
    track.setOrnament(20);
    expectEqual(track.ornament(), 16, "ornament should clamp to 16 (max)");
}

//----------------------------------------
// Power Parameter
//----------------------------------------

CASE("power_setter_getter") {
    TuesdayTrack track;
    track.setPower(14);
    expectEqual(track.power(), 14, "power should be 14");
}

CASE("power_clamping_upper") {
    TuesdayTrack track;
    track.setPower(20);
    expectEqual(track.power(), 16, "power should clamp to 16 (max)");
}

//----------------------------------------
// LoopLength Parameter
//----------------------------------------

CASE("loopLength_setter_getter") {
    TuesdayTrack track;
    track.setLoopLength(10);
    expectEqual(track.loopLength(), 10, "loopLength index should be 10");
}

CASE("loopLength_clamping_upper") {
    TuesdayTrack track;
    track.setLoopLength(30);
    expectEqual(track.loopLength(), 25, "loopLength should clamp to 25 (max index)");
}

CASE("actualLoopLength_infinite") {
    TuesdayTrack track;
    track.setLoopLength(0);
    expectEqual(track.actualLoopLength(), 0, "index 0 = Inf (returns 0)");
}

CASE("actualLoopLength_standard_values") {
    TuesdayTrack track;

    // Test standard values 1-16
    track.setLoopLength(1);
    expectEqual(track.actualLoopLength(), 1, "index 1 = 1");

    track.setLoopLength(8);
    expectEqual(track.actualLoopLength(), 8, "index 8 = 8");

    track.setLoopLength(16);
    expectEqual(track.actualLoopLength(), 16, "index 16 = 16");
}

CASE("actualLoopLength_extended_values") {
    TuesdayTrack track;

    // Test extended values: 19, 21, 24, 32, 35, 42, 48, 56, 64
    track.setLoopLength(17);
    expectEqual(track.actualLoopLength(), 19, "index 17 = 19");

    track.setLoopLength(18);
    expectEqual(track.actualLoopLength(), 21, "index 18 = 21");

    track.setLoopLength(19);
    expectEqual(track.actualLoopLength(), 24, "index 19 = 24");

    track.setLoopLength(20);
    expectEqual(track.actualLoopLength(), 32, "index 20 = 32");

    track.setLoopLength(21);
    expectEqual(track.actualLoopLength(), 35, "index 21 = 35");

    track.setLoopLength(22);
    expectEqual(track.actualLoopLength(), 42, "index 22 = 42");

    track.setLoopLength(23);
    expectEqual(track.actualLoopLength(), 48, "index 23 = 48");

    track.setLoopLength(24);
    expectEqual(track.actualLoopLength(), 56, "index 24 = 56");

    track.setLoopLength(25);
    expectEqual(track.actualLoopLength(), 64, "index 25 = 64");
}

//----------------------------------------
// Clear Method
//----------------------------------------

CASE("clear_resets_all_values") {
    TuesdayTrack track;

    // Set non-default values
    track.setAlgorithm(10);
    track.setFlow(8);
    track.setOrnament(12);
    track.setPower(14);
    track.setLoopLength(20);
    track.setCvUpdateMode(TuesdayTrack::CvUpdateMode::Gated);

    // Clear
    track.clear();

    // Verify defaults restored
    expectEqual(track.algorithm(), 0, "algorithm should reset to 0");
    expectEqual(track.flow(), 0, "flow should reset to 0");
    expectEqual(track.ornament(), 0, "ornament should reset to 0");
    expectEqual(track.power(), 0, "power should reset to 0");
    expectEqual(track.loopLength(), 16, "loopLength should reset to 16");
    expectEqual(static_cast<int>(track.cvUpdateMode()), 0, "cvUpdateMode should reset to Free");
}

//----------------------------------------
// Edit Methods (for UI encoder)
//----------------------------------------

CASE("editAlgorithm_increments") {
    TuesdayTrack track;
    track.setAlgorithm(5);
    track.editAlgorithm(1, false);
    expectEqual(track.algorithm(), 6, "algorithm should increment to 6");
    track.editAlgorithm(-2, false);
    expectEqual(track.algorithm(), 4, "algorithm should decrement to 4");
}

CASE("editFlow_increments") {
    TuesdayTrack track;
    track.setFlow(8);
    track.editFlow(1, false);
    expectEqual(track.flow(), 9, "flow should increment to 9");
}

CASE("editOrnament_increments") {
    TuesdayTrack track;
    track.setOrnament(5);
    track.editOrnament(3, false);
    expectEqual(track.ornament(), 8, "ornament should increment to 8");
}

CASE("editPower_increments") {
    TuesdayTrack track;
    track.setPower(10);
    track.editPower(-5, false);
    expectEqual(track.power(), 5, "power should decrement to 5");
}

CASE("editLoopLength_increments") {
    TuesdayTrack track;
    track.setLoopLength(16);
    track.editLoopLength(1, false);
    expectEqual(track.loopLength(), 17, "loopLength should increment to 17");
}

//----------------------------------------
// CvUpdateMode Parameter
//----------------------------------------

CASE("cvUpdateMode_default_value") {
    TuesdayTrack track;
    expectEqual(static_cast<int>(track.cvUpdateMode()), 0, "default cvUpdateMode should be Free (0)");
}

CASE("cvUpdateMode_setter_getter") {
    TuesdayTrack track;
    track.setCvUpdateMode(TuesdayTrack::CvUpdateMode::Gated);
    expectEqual(static_cast<int>(track.cvUpdateMode()), 1, "cvUpdateMode should be Gated (1)");
    track.setCvUpdateMode(TuesdayTrack::CvUpdateMode::Free);
    expectEqual(static_cast<int>(track.cvUpdateMode()), 0, "cvUpdateMode should be Free (0)");
}

CASE("cvUpdateMode_edit_toggles") {
    TuesdayTrack track;
    expectEqual(static_cast<int>(track.cvUpdateMode()), 0, "initial should be Free");
    track.editCvUpdateMode(1, false);
    expectEqual(static_cast<int>(track.cvUpdateMode()), 1, "should toggle to Gated");
    track.editCvUpdateMode(1, false);
    expectEqual(static_cast<int>(track.cvUpdateMode()), 0, "should toggle back to Free");
}

CASE("cvUpdateMode_clear_resets") {
    TuesdayTrack track;
    track.setCvUpdateMode(TuesdayTrack::CvUpdateMode::Gated);
    track.clear();
    expectEqual(static_cast<int>(track.cvUpdateMode()), 0, "cvUpdateMode should reset to Free after clear");
}

//----------------------------------------
// Octave Parameter (-10 to +10)
//----------------------------------------

CASE("octave_default_value") {
    TuesdayTrack track;
    expectEqual(track.octave(), 0, "default octave should be 0");
}

CASE("octave_setter_getter") {
    TuesdayTrack track;
    track.setOctave(5);
    expectEqual(track.octave(), 5, "octave should be 5");
    track.setOctave(-3);
    expectEqual(track.octave(), -3, "octave should be -3");
}

CASE("octave_clamping") {
    TuesdayTrack track;
    track.setOctave(15);
    expectEqual(track.octave(), 10, "octave should clamp to 10");
    track.setOctave(-15);
    expectEqual(track.octave(), -10, "octave should clamp to -10");
}

//----------------------------------------
// Transpose Parameter (-11 to +11)
//----------------------------------------

CASE("transpose_default_value") {
    TuesdayTrack track;
    expectEqual(track.transpose(), 0, "default transpose should be 0");
}

CASE("transpose_setter_getter") {
    TuesdayTrack track;
    track.setTranspose(7);
    expectEqual(track.transpose(), 7, "transpose should be 7");
    track.setTranspose(-5);
    expectEqual(track.transpose(), -5, "transpose should be -5");
}

CASE("transpose_clamping") {
    TuesdayTrack track;
    track.setTranspose(20);
    expectEqual(track.transpose(), 11, "transpose should clamp to 11");
    track.setTranspose(-20);
    expectEqual(track.transpose(), -11, "transpose should clamp to -11");
}

//----------------------------------------
// Divisor Parameter
//----------------------------------------

CASE("divisor_default_value") {
    TuesdayTrack track;
    expectEqual(track.divisor(), 192, "default divisor should be 192 (1/4 note)");
}

CASE("divisor_setter_getter") {
    TuesdayTrack track;
    track.setDivisor(96);
    expectEqual(track.divisor(), 96, "divisor should be 96");
}

//----------------------------------------
// ResetMeasure Parameter (0-128)
//----------------------------------------

CASE("resetMeasure_default_value") {
    TuesdayTrack track;
    expectEqual(track.resetMeasure(), 0, "default resetMeasure should be 0 (off)");
}

CASE("resetMeasure_setter_getter") {
    TuesdayTrack track;
    track.setResetMeasure(8);
    expectEqual(track.resetMeasure(), 8, "resetMeasure should be 8");
}

CASE("resetMeasure_clamping") {
    TuesdayTrack track;
    track.setResetMeasure(200);
    expectEqual(track.resetMeasure(), 128, "resetMeasure should clamp to 128");
}

//----------------------------------------
// Scale Parameter (-1 to ScaleCount)
//----------------------------------------

CASE("scale_default_value") {
    TuesdayTrack track;
    expectEqual(track.scale(), -1, "default scale should be -1 (Default/Project)");
}

CASE("scale_setter_getter") {
    TuesdayTrack track;
    track.setScale(3);
    expectEqual(track.scale(), 3, "scale should be 3");
    track.setScale(-1);
    expectEqual(track.scale(), -1, "scale should be -1 (Default)");
}

//----------------------------------------
// RootNote Parameter (-1 to 11)
//----------------------------------------

CASE("rootNote_default_value") {
    TuesdayTrack track;
    expectEqual(track.rootNote(), -1, "default rootNote should be -1 (Default/Project)");
}

CASE("rootNote_setter_getter") {
    TuesdayTrack track;
    track.setRootNote(5);
    expectEqual(track.rootNote(), 5, "rootNote should be 5 (F)");
    track.setRootNote(-1);
    expectEqual(track.rootNote(), -1, "rootNote should be -1 (Default)");
}

CASE("rootNote_clamping") {
    TuesdayTrack track;
    track.setRootNote(15);
    expectEqual(track.rootNote(), 11, "rootNote should clamp to 11");
    track.setRootNote(-5);
    expectEqual(track.rootNote(), -1, "rootNote should clamp to -1");
}

//----------------------------------------
// Sequence Parameters Clear
//----------------------------------------

CASE("sequence_params_clear") {
    TuesdayTrack track;
    track.setOctave(5);
    track.setTranspose(-3);
    track.setDivisor(96);
    track.setResetMeasure(16);
    track.setScale(2);
    track.setRootNote(7);

    track.clear();

    expectEqual(track.octave(), 0, "octave should reset to 0");
    expectEqual(track.transpose(), 0, "transpose should reset to 0");
    expectEqual(track.divisor(), 192, "divisor should reset to 192");
    expectEqual(track.resetMeasure(), 0, "resetMeasure should reset to 0");
    expectEqual(track.scale(), -1, "scale should reset to -1");
    expectEqual(track.rootNote(), -1, "rootNote should reset to -1");
}

} // UNIT_TEST("TuesdayTrack")
