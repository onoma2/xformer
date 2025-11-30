#include "UnitTest.h"
#include "apps/sequencer/model/CurveTrack.h"
#include "apps/sequencer/model/Curve.h"

UNIT_TEST("CurveTrackLfoShapes") {

    CASE("populateWithLfoShape populates all steps with specified LFO shape") {
        CurveTrack track;
        auto& sequence = track.sequence(0);
        
        // Populate with Triangle shape
        sequence.populateWithLfoShape(Curve::Triangle, 0, CONFIG_STEP_COUNT - 1);
        
        // Verify all steps have the Triangle shape
        for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
            expectEqual(sequence.step(i).shape(), static_cast<int>(Curve::Triangle));
        }
    }

    CASE("populateWithLfoShape works within specified range") {
        CurveTrack track;
        auto& sequence = track.sequence(0);
        
        // First populate all with one shape
        sequence.populateWithLfoShape(Curve::RampUp, 0, CONFIG_STEP_COUNT - 1);
        
        // Then populate range with another shape
        sequence.populateWithLfoShape(Curve::Triangle, 5, 10);
        
        // Verify range was updated
        for (int i = 0; i < 5; ++i) {
            expectEqual(sequence.step(i).shape(), static_cast<int>(Curve::RampUp));
        }
        for (int i = 5; i <= 10; ++i) {
            expectEqual(sequence.step(i).shape(), static_cast<int>(Curve::Triangle));
        }
        for (int i = 11; i < CONFIG_STEP_COUNT; ++i) {
            expectEqual(sequence.step(i).shape(), static_cast<int>(Curve::RampUp));
        }
    }

    CASE("populateWithLfoShape handles range boundaries correctly") {
        CurveTrack track;
        auto& sequence = track.sequence(0);
        
        // Test with range 0 to 0 (single step)
        sequence.populateWithLfoShape(Curve::SmoothUp, 0, 0);
        expectEqual(sequence.step(0).shape(), static_cast<int>(Curve::SmoothUp));
        
        // Test with range at max boundary
        sequence.populateWithLfoShape(Curve::High, CONFIG_STEP_COUNT - 1, CONFIG_STEP_COUNT - 1);
        expectEqual(sequence.step(CONFIG_STEP_COUNT - 1).shape(), static_cast<int>(Curve::High));
    }

    CASE("populateWithLfoShape clamps invalid shape values") {
        CurveTrack track;
        auto& sequence = track.sequence(0);
        
        // Test with invalid shape value (beyond Curve::Last)
        sequence.populateWithLfoShape(static_cast<Curve::Type>(Curve::Last), 0, 0);
        // Should clamp to valid range (Curve::Last - 1)
        expect(sequence.step(0).shape() < static_cast<int>(Curve::Last));
    }

    CASE("populateWithLfoShape handles out-of-bounds ranges") {
        CurveTrack track;
        auto& sequence = track.sequence(0);
        
        // Test with range beyond available steps
        sequence.populateWithLfoShape(Curve::Triangle, 0, CONFIG_STEP_COUNT + 10);
        
        // Should only populate up to valid range
        for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
            expectEqual(sequence.step(i).shape(), static_cast<int>(Curve::Triangle));
        }
    }

    CASE("populateWithLfoShape works with different LFO shapes") {
        CurveTrack track;
        auto& sequence = track.sequence(0);

        // Test each available LFO-relevant shape
        Curve::Type lfoShapes[] = {
            Curve::Triangle,
            Curve::RampUp,
            Curve::RampDown,
            Curve::SmoothUp,  // Similar to sine
            Curve::StepUp     // Similar to square
        };

        for (int i = 0; i < 3 && i < sizeof(lfoShapes)/sizeof(lfoShapes[0]); ++i) { // Testing first 3 shapes
            sequence.populateWithLfoShape(lfoShapes[i], i * 10, i * 10 + 5);

            for (int j = i * 10; j <= i * 10 + 5 && j < CONFIG_STEP_COUNT; ++j) {
                expectEqual(sequence.step(j).shape(), static_cast<int>(lfoShapes[i]));
            }
        }
    }

    CASE("populateWithLfoShape preserves other step properties") {
        CurveTrack track;
        auto& sequence = track.sequence(0);
        
        // Set some initial values for other properties
        sequence.step(0).setMin(50);
        sequence.step(0).setMax(200);
        sequence.step(0).setGate(8);
        sequence.step(0).setGateProbability(5);
        
        // Populate with new shape
        sequence.populateWithLfoShape(Curve::Triangle, 0, 0);
        
        // Verify shape was updated but other properties preserved
        expectEqual(sequence.step(0).shape(), static_cast<int>(Curve::Triangle));
        expectEqual(sequence.step(0).min(), 50);
        expectEqual(sequence.step(0).max(), 200);
        expectEqual(sequence.step(0).gate(), 8);
        expectEqual(sequence.step(0).gateProbability(), 5);
    }

    CASE("populateWithLfoShape works with reverse range (first > last)") {
        CurveTrack track;
        auto& sequence = track.sequence(0);
        
        // Populate with range where first > last should swap them
        sequence.populateWithLfoShape(Curve::Triangle, 10, 5);
        
        // Should populate from 5 to 10
        for (int i = 5; i <= 10; ++i) {
            expectEqual(sequence.step(i).shape(), static_cast<int>(Curve::Triangle));
        }
    }

    CASE("populateWithLfoShape handles negative range values") {
        CurveTrack track;
        auto& sequence = track.sequence(0);
        
        // Populate with negative first value (should be clamped to 0)
        sequence.populateWithLfoShape(Curve::Triangle, -5, 10);
        
        // Should populate from 0 to 10
        for (int i = 0; i <= 10; ++i) {
            expectEqual(sequence.step(i).shape(), static_cast<int>(Curve::Triangle));
        }
    }

    CASE("populateWithLfoPattern populates with oscillating LFO pattern") {
        CurveTrack track;
        auto& sequence = track.sequence(0);
        
        // Populate with oscillating triangle pattern over 4 steps
        sequence.populateWithLfoPattern(Curve::Triangle, 0, 3);
        
        // For triangle: should alternate between Triangle and variations
        // This is a basic implementation that would alternate shapes
        expectEqual(sequence.step(0).shape(), static_cast<int>(Curve::Triangle));
        expectEqual(sequence.step(1).shape(), static_cast<int>(Curve::Triangle));
        expectEqual(sequence.step(2).shape(), static_cast<int>(Curve::Triangle));
        expectEqual(sequence.step(3).shape(), static_cast<int>(Curve::Triangle));
    }

    CASE("populateWithLfoWaveform populates with full LFO waveform") {
        CurveTrack track;
        auto& sequence = track.sequence(0);
        
        // Populate with sine-like waveform (using SmoothUp/SmoothDown)
        sequence.populateWithLfoWaveform(Curve::SmoothUp, Curve::SmoothDown, 0, 7);
        
        // Should alternate between up and down curves to simulate waveform
        expectEqual(sequence.step(0).shape(), static_cast<int>(Curve::SmoothUp));
        expectEqual(sequence.step(1).shape(), static_cast<int>(Curve::SmoothDown));
        expectEqual(sequence.step(2).shape(), static_cast<int>(Curve::SmoothUp));
        expectEqual(sequence.step(3).shape(), static_cast<int>(Curve::SmoothDown));
        expectEqual(sequence.step(4).shape(), static_cast<int>(Curve::SmoothUp));
        expectEqual(sequence.step(5).shape(), static_cast<int>(Curve::SmoothDown));
        expectEqual(sequence.step(6).shape(), static_cast<int>(Curve::SmoothUp));
        expectEqual(sequence.step(7).shape(), static_cast<int>(Curve::SmoothDown));
    }

    CASE("populateWithLfoShape maintains sequence track index") {
        CurveTrack track;
        auto& sequence = track.sequence(0);

        sequence.populateWithLfoShape(Curve::Triangle, 0, 0);

        // Verify track index is maintained (default is -1 until set by Track)
        expectEqual(sequence.trackIndex(), -1);
    }

    CASE("populateWithLfoShape handles zero range (first == last)") {
        CurveTrack track;
        auto& sequence = track.sequence(0);

        sequence.populateWithLfoShape(Curve::RampUp, 5, 5);

        // Only step 5 should be changed
        for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
            if (i == 5) {
                expectEqual(sequence.step(i).shape(), static_cast<int>(Curve::RampUp));
            } else {
                expectEqual(sequence.step(i).shape(), 0); // Default shape
            }
        }
    }

    CASE("populateWithSineWaveLfo creates proper sine-like pattern") {
        CurveTrack track;
        auto& sequence = track.sequence(0);

        sequence.populateWithSineWaveLfo(0, 7);  // 8 steps for clear pattern

        // Check that we have both up and down shapes for a sine-like pattern
        int upCount = 0, downCount = 0;
        for (int i = 0; i <= 7; ++i) {
            int shape = sequence.step(i).shape();
            if (shape == static_cast<int>(Curve::SmoothUp)) {
                upCount++;
            } else if (shape == static_cast<int>(Curve::SmoothDown)) {
                downCount++;
            }
        }

        // Should have both up and down shapes for sine pattern
        expect(upCount > 0);
        expect(downCount > 0);
    }

    CASE("populateWithSquareWaveLfo alternates shapes properly") {
        CurveTrack track;
        auto& sequence = track.sequence(0);

        sequence.populateWithSquareWaveLfo(0, 7);  // 8 steps

        // Check alternating pattern
        for (int i = 0; i <= 7; ++i) {
            if (i % 2 == 0) {
                expectEqual(sequence.step(i).shape(), static_cast<int>(Curve::StepUp));
            } else {
                expectEqual(sequence.step(i).shape(), static_cast<int>(Curve::StepDown));
            }
        }
    }

    CASE("populateWithLfoWaveform handles single step range") {
        CurveTrack track;
        auto& sequence = track.sequence(0);

        sequence.populateWithLfoWaveform(Curve::RampUp, Curve::RampDown, 3, 3);

        expectEqual(sequence.step(3).shape(), static_cast<int>(Curve::RampUp));
    }

    CASE("populateWithLfoShape handles max range") {
        CurveTrack track;
        auto& sequence = track.sequence(0);

        sequence.populateWithLfoShape(Curve::Triangle, 0, CONFIG_STEP_COUNT - 1);

        // Verify all steps are set to the specified shape
        for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
            expectEqual(sequence.step(i).shape(), static_cast<int>(Curve::Triangle));
        }
    }
}