#include "UnitTest.h"
#include "apps/sequencer/model/TuesdayTrack.h"
#include "core/utils/StringBuilder.h"
#include <cstring>

UNIT_TEST("TuesdayAlgorithmNames") {

//----------------------------------------
// Algorithm Index Values
//----------------------------------------

CASE("algorithm_max_updated_to_14") {
    TuesdaySequence track;
    track.setAlgorithm(14);
    expectEqual(track.algorithm(), 14, "max algorithm should be 14");
}

CASE("algorithm_clamped_above_14") {
    TuesdaySequence track;
    track.setAlgorithm(25);
    expectEqual(track.algorithm(), 14, "algorithm should clamp to 14");
}

CASE("minimal_algorithm_index_9") {
    TuesdaySequence track;
    track.setAlgorithm(9);
    expectEqual(track.algorithm(), 9, "MINIMAL should be algorithm 9");
}

CASE("ganz_algorithm_index_10") {
    TuesdaySequence track;
    track.setAlgorithm(10);
    expectEqual(track.algorithm(), 10, "GANZ should be algorithm 10");
}

CASE("blake_algorithm_index_11") {
    TuesdaySequence track;
    track.setAlgorithm(11);
    expectEqual(track.algorithm(), 11, "BLAKE should be algorithm 11");
}

CASE("aphex_algorithm_index_12") {
    TuesdaySequence track;
    track.setAlgorithm(12);
    expectEqual(track.algorithm(), 12, "APHEX should be algorithm 12");
}

CASE("autech_algorithm_index_13") {
    TuesdaySequence track;
    track.setAlgorithm(13);
    expectEqual(track.algorithm(), 13, "AUTECH should be algorithm 13");
}

CASE("stepwave_algorithm_index_14") {
    TuesdaySequence track;
    track.setAlgorithm(14);
    expectEqual(track.algorithm(), 14, "STEPWAVE should be algorithm 14");
}

//----------------------------------------
// Algorithm Name Strings
//----------------------------------------

CASE("minimal_algorithm_name") {
    TuesdaySequence track;
    track.setAlgorithm(9);
    FixedStringBuilder<32> str;
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "MINIMAL"), 0, "algorithm 9 should print MINIMAL");
}

CASE("ganz_algorithm_name") {
    TuesdaySequence track;
    track.setAlgorithm(10);
    FixedStringBuilder<32> str;
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "GANZ"), 0, "algorithm 10 should print GANZ");
}

CASE("blake_algorithm_name") {
    TuesdaySequence track;
    track.setAlgorithm(11);
    FixedStringBuilder<32> str;
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "BLAKE"), 0, "algorithm 11 should print BLAKE");
}

CASE("aphex_algorithm_name") {
    TuesdaySequence track;
    track.setAlgorithm(12);
    FixedStringBuilder<32> str;
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "APHEX"), 0, "algorithm 12 should print APHEX");
}

CASE("autech_algorithm_name") {
    TuesdaySequence track;
    track.setAlgorithm(13);
    FixedStringBuilder<32> str;
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "AUTECH"), 0, "algorithm 13 should print AUTECH");
}

CASE("stepwave_algorithm_name") {
    TuesdaySequence track;
    track.setAlgorithm(14);
    FixedStringBuilder<32> str;
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "STEPWAVE"), 0, "algorithm 14 should print STEPWAVE");
}

//----------------------------------------
// Existing Algorithms Still Work
//----------------------------------------

CASE("existing_algorithms_unchanged") {
    TuesdaySequence track;
    FixedStringBuilder<32> str;

    track.setAlgorithm(0);
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "TEST"), 0, "algorithm 0 should still be TEST");

    str.reset();
    track.setAlgorithm(3);
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "MARKOV"), 0, "algorithm 3 should be MARKOV");
}

} // UNIT_TEST("TuesdayAlgorithmNames")
