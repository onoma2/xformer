#include "UnitTest.h"
#include "apps/sequencer/model/CurveTrack.h"
#include "apps/sequencer/engine/CurveTrackEngine.h"
#include "apps/sequencer/model/Project.h"
#include "utils/MemoryFile.h"

using namespace fs;

UNIT_TEST("CurveTrackPatterns") {

    // Test Case 1: Independent Storage
    // Verify that Pattern A and Pattern B can store different Chaos/Wavefolder values.
    CASE("Independent Pattern Storage") {
        CurveTrack track;
        // We need to initialize the track to ensure sequences are ready? 
        // CurveTrack constructor calls clear() which clears sequences.
        
        auto &seq0 = track.sequence(0);
        auto &seq1 = track.sequence(1);

        // Initially should be default
        expectEqual(seq0.chaosAmount(), 0, "Default chaos amount should be 0");
        expectEqual(seq1.chaosAmount(), 0, "Default chaos amount should be 0");

        // Set different values
        seq0.setChaosAmount(10);
        seq1.setChaosAmount(90);
        
        seq0.setWavefolderFold(0.2f);
        seq1.setWavefolderFold(0.8f);

        // Verify independence
        expectEqual(seq0.chaosAmount(), 10, "Seq0 should have chaos 10");
        expectEqual(seq1.chaosAmount(), 90, "Seq1 should have chaos 90");
        
        expect(std::abs(seq0.wavefolderFold() - 0.2f) < 0.001f, "Seq0 fold should be 0.2");
        expect(std::abs(seq1.wavefolderFold() - 0.8f) < 0.001f, "Seq1 fold should be 0.8");
    }

    // Test Case 2: Copy/Paste Logic
    // Since we handle copy/paste via a Settings struct, let's verify that logic holds up
    // when moving between sequences.
    CASE("Settings Transfer") {
        CurveTrack track;
        auto &source = track.sequence(0);
        auto &dest = track.sequence(1);

        source.setChaosRate(50);
        source.setChaosAlgo(CurveSequence::ChaosAlgorithm::Lorenz);
        source.setDjFilter(0.5f);

        // Simulate "Copy" (Manual field transfer for now, simulating what the UI does)
        dest.setChaosRate(source.chaosRate());
        dest.setChaosAlgo(source.chaosAlgo());
        dest.setDjFilter(source.djFilter());

        expectEqual(dest.chaosRate(), 50, "Dest should inherit Rate");
        expectEqual(int(dest.chaosAlgo()), int(CurveSequence::ChaosAlgorithm::Lorenz), "Dest should inherit Algo");
        expect(std::abs(dest.djFilter() - 0.5f) < 0.001f, "Dest should inherit Filter");
    }
}
