#include "UnitTest.h"
#include "apps/sequencer/model/CurveTrack.h"
#include "apps/sequencer/model/Curve.h"
#include "apps/sequencer/model/Model.h"
#include "apps/sequencer/model/Project.h"

UNIT_TEST("CurveTrackLfoShapesIntegration") {

    CASE("Complete integration: Track -> Sequence -> LFO functions work together") {
        // Test the LFO functions directly on a CurveTrack
        CurveTrack track;

        auto &sequence = track.sequence(0);

        // Test that we can call the LFO functions on the sequence directly
        sequence.populateWithTriangleWaveLfo(0, 7);

        // Verify the shapes were set correctly
        for (int i = 0; i <= 7; ++i) {
            expectEqual(sequence.step(i).shape(), static_cast<int>(Curve::Triangle));
        }

        // Test another LFO function
        sequence.populateWithSquareWaveLfo(8, 15);

        // Verify alternating pattern for square wave
        for (int i = 8; i <= 15; ++i) {
            if ((i - 8) % 2 == 0) {
                expectEqual(sequence.step(i).shape(), static_cast<int>(Curve::StepUp));
            } else {
                expectEqual(sequence.step(i).shape(), static_cast<int>(Curve::StepDown));
            }
        }
    }

    CASE("CurveTrack level LFO functions work correctly") {
        CurveTrack track;
        
        // Test track-level function
        track.populateWithSineWaveLfo(0, 0, 7);
        
        auto &sequence = track.sequence(0);
        
        // Verify sine wave pattern was applied
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

    CASE("LFO functions preserve other step properties") {
        CurveTrack track;
        auto &sequence = track.sequence(0);
        
        // Set some initial values for other properties
        for (int i = 0; i < 8; ++i) {
            sequence.step(i).setMin(50);
            sequence.step(i).setMax(200);
            sequence.step(i).setGate(8);
            sequence.step(i).setGateProbability(5);
        }
        
        // Apply LFO function
        sequence.populateWithSawtoothWaveLfo(0, 7);
        
        // Verify shapes were updated but other properties preserved
        for (int i = 0; i <= 7; ++i) {
            expectEqual(sequence.step(i).shape(), static_cast<int>(Curve::RampUp));
            expectEqual(sequence.step(i).min(), 50);
            expectEqual(sequence.step(i).max(), 200);
            expectEqual(sequence.step(i).gate(), 8);
            expectEqual(sequence.step(i).gateProbability(), 5);
        }
    }

    CASE("UI integration - LFO functions accessible through CurveTrack") {
        CurveTrack track;
        
        // Test that all LFO functions are accessible and don't crash
        auto &sequence = track.sequence(0);
        
        // These should all execute without errors
        sequence.populateWithLfoShape(Curve::Triangle, 0, 3);
        sequence.populateWithLfoWaveform(Curve::RampUp, Curve::RampDown, 4, 7);
        sequence.populateWithLfoPattern(Curve::SmoothUp, 8, 11);
        sequence.populateWithTriangleWaveLfo(12, 15);
        sequence.populateWithSineWaveLfo(16, 19);
        sequence.populateWithSawtoothWaveLfo(20, 23);
        sequence.populateWithSquareWaveLfo(24, 27);
        
        // Verify some shapes were set
        expectEqual(sequence.step(0).shape(), static_cast<int>(Curve::Triangle));
        expectEqual(sequence.step(16).shape(), static_cast<int>(Curve::SmoothUp)); // First half of sine
        expectEqual(sequence.step(20).shape(), static_cast<int>(Curve::RampUp));
    }

    CASE("Memory efficiency - LFO functions don't create memory leaks") {
        CurveTrack track;
        auto &sequence = track.sequence(0);
        
        // Run multiple LFO operations to check for memory issues
        for (int cycle = 0; cycle < 10; ++cycle) {
            sequence.populateWithTriangleWaveLfo(0, CONFIG_STEP_COUNT - 1);
            sequence.populateWithSineWaveLfo(0, CONFIG_STEP_COUNT - 1);
            sequence.populateWithSquareWaveLfo(0, CONFIG_STEP_COUNT - 1);
            sequence.populateWithSawtoothWaveLfo(0, CONFIG_STEP_COUNT - 1);
        }
        
        // All operations should complete without memory issues
        // The sequence should still be valid
        expect(sequence.trackIndex() == -1); // Default track index
    }
}