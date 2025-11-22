#include "UnitTest.h"
#include "apps/sequencer/model/TuesdayTrack.h"
#include "core/utils/StringBuilder.h"
#include <cstring>

UNIT_TEST("TuesdayAlgorithmNames") {

//----------------------------------------
// Algorithm Index Values
//----------------------------------------

CASE("algorithm_max_updated_to_19") {
    TuesdayTrack track;
    track.setAlgorithm(19);
    expectEqual(track.algorithm(), 19, "max algorithm should be 19");
}

CASE("algorithm_clamped_above_19") {
    TuesdayTrack track;
    track.setAlgorithm(25);
    expectEqual(track.algorithm(), 19, "algorithm should clamp to 19");
}

CASE("ambient_algorithm_index_13") {
    TuesdayTrack track;
    track.setAlgorithm(13);
    expectEqual(track.algorithm(), 13, "AMBIENT should be algorithm 13");
}

CASE("acid_algorithm_index_14") {
    TuesdayTrack track;
    track.setAlgorithm(14);
    expectEqual(track.algorithm(), 14, "ACID should be algorithm 14");
}

CASE("drill_algorithm_index_15") {
    TuesdayTrack track;
    track.setAlgorithm(15);
    expectEqual(track.algorithm(), 15, "DRILL should be algorithm 15");
}

CASE("minimal_algorithm_index_16") {
    TuesdayTrack track;
    track.setAlgorithm(16);
    expectEqual(track.algorithm(), 16, "MINIMAL should be algorithm 16");
}

CASE("kraft_algorithm_index_17") {
    TuesdayTrack track;
    track.setAlgorithm(17);
    expectEqual(track.algorithm(), 17, "KRAFT should be algorithm 17");
}

CASE("aphex_algorithm_index_18") {
    TuesdayTrack track;
    track.setAlgorithm(18);
    expectEqual(track.algorithm(), 18, "APHEX should be algorithm 18");
}

CASE("autech_algorithm_index_19") {
    TuesdayTrack track;
    track.setAlgorithm(19);
    expectEqual(track.algorithm(), 19, "AUTECH should be algorithm 19");
}

//----------------------------------------
// Algorithm Name Strings
//----------------------------------------

CASE("ambient_algorithm_name") {
    TuesdayTrack track;
    track.setAlgorithm(13);
    FixedStringBuilder<32> str;
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "AMBIENT"), 0, "algorithm 13 should print AMBIENT");
}

CASE("acid_algorithm_name") {
    TuesdayTrack track;
    track.setAlgorithm(14);
    FixedStringBuilder<32> str;
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "ACID"), 0, "algorithm 14 should print ACID");
}

CASE("drill_algorithm_name") {
    TuesdayTrack track;
    track.setAlgorithm(15);
    FixedStringBuilder<32> str;
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "DRILL"), 0, "algorithm 15 should print DRILL");
}

CASE("minimal_algorithm_name") {
    TuesdayTrack track;
    track.setAlgorithm(16);
    FixedStringBuilder<32> str;
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "MINIMAL"), 0, "algorithm 16 should print MINIMAL");
}

CASE("kraft_algorithm_name") {
    TuesdayTrack track;
    track.setAlgorithm(17);
    FixedStringBuilder<32> str;
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "KRAFT"), 0, "algorithm 17 should print KRAFT");
}

CASE("aphex_algorithm_name") {
    TuesdayTrack track;
    track.setAlgorithm(18);
    FixedStringBuilder<32> str;
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "APHEX"), 0, "algorithm 18 should print APHEX");
}

CASE("autech_algorithm_name") {
    TuesdayTrack track;
    track.setAlgorithm(19);
    FixedStringBuilder<32> str;
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "AUTECH"), 0, "algorithm 19 should print AUTECH");
}

//----------------------------------------
// Existing Algorithms Still Work
//----------------------------------------

CASE("existing_algorithms_unchanged") {
    TuesdayTrack track;
    FixedStringBuilder<32> str;

    track.setAlgorithm(0);
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "TEST"), 0, "algorithm 0 should still be TEST");

    str.reset();
    track.setAlgorithm(12);
    track.printAlgorithm(str);
    expectEqual(strcmp(str, "RAGA"), 0, "algorithm 12 should still be RAGA");
}

} // UNIT_TEST("TuesdayAlgorithmNames")
