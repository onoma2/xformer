#include "UnitTest.h"
#include "apps/sequencer/model/TuesdayTrack.h"
#include "apps/sequencer/model/TuesdaySequence.h"
#include "apps/sequencer/engine/TuesdayTrackEngine.h"
#include "apps/sequencer/model/Project.h"
#include "utils/MemoryFile.h"

using namespace fs;

UNIT_TEST("TuesdayTrackPatterns") {

    // Test Case 1: Independent Storage
    // Verify that Pattern A and Pattern B can store different Algorithm values.
    CASE("Independent Pattern Storage") {
        // We need to manually instantiate TuesdaySequence because TuesdayTrack
        // hasn't been refactored to use it yet. This test is preparing for that.
        
        TuesdaySequence seq0;
        TuesdaySequence seq1;

        // Initially should be default
        expectEqual(seq0.algorithm(), 0, "Default algorithm should be 0");
        expectEqual(seq1.algorithm(), 0, "Default algorithm should be 0");

        // Set different values
        seq0.setAlgorithm(5);
        seq1.setAlgorithm(10);
        
        seq0.setDivisor(12);
        seq1.setDivisor(24);

        // Verify independence
        expectEqual(seq0.algorithm(), 5, "Seq0 should have algorithm 5");
        expectEqual(seq1.algorithm(), 10, "Seq1 should have algorithm 10");
        
        expectEqual(seq0.divisor(), 12, "Seq0 divisor should be 12");
        expectEqual(seq1.divisor(), 24, "Seq1 divisor should be 24");
    }

    // Test Case 2: Settings Transfer
    CASE("Settings Transfer") {
        TuesdaySequence source;
        TuesdaySequence dest;

        source.setAlgorithm(15);
        source.setFlow(8);
        source.setOrnament(4);
        source.setDivisor(48);

        // Simulate "Copy" logic
        dest.setAlgorithm(source.algorithm());
        dest.setFlow(source.flow());
        dest.setOrnament(source.ornament());
        dest.setDivisor(source.divisor());

        expectEqual(dest.algorithm(), 15, "Dest should inherit Algorithm");
        expectEqual(dest.flow(), 8, "Dest should inherit Flow");
        expectEqual(dest.ornament(), 4, "Dest should inherit Ornament");
        expectEqual(dest.divisor(), 48, "Dest should inherit Divisor");
    }
}
