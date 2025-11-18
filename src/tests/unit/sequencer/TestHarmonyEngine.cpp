#include "catch.hpp"
#include "model/HarmonyEngine.h"

TEST_CASE("HarmonyEngine Default Construction", "[harmony]") {
    HarmonyEngine engine;

    REQUIRE(engine.mode() == HarmonyEngine::Ionian);
    REQUIRE(engine.diatonicMode() == true);
    REQUIRE(engine.inversion() == 0);
    REQUIRE(engine.voicing() == HarmonyEngine::Close);
    REQUIRE(engine.transpose() == 0);
}

TEST_CASE("HarmonyEngine Ionian Scale Intervals", "[harmony]") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);

    // Ionian intervals: W-W-H-W-W-W-H = 0-2-4-5-7-9-11
    const int16_t expected[7] = {0, 2, 4, 5, 7, 9, 11};

    for (int degree = 0; degree < 7; degree++) {
        REQUIRE(engine.getScaleInterval(degree) == expected[degree]);
    }
}

TEST_CASE("HarmonyEngine Ionian Diatonic Chord Qualities", "[harmony]") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setDiatonicMode(true);

    // Ionian: I∆7, ii-7, iii-7, IV∆7, V7, vi-7, viiø
    REQUIRE(engine.getDiatonicQuality(0) == HarmonyEngine::Major7);     // I
    REQUIRE(engine.getDiatonicQuality(1) == HarmonyEngine::Minor7);     // ii
    REQUIRE(engine.getDiatonicQuality(2) == HarmonyEngine::Minor7);     // iii
    REQUIRE(engine.getDiatonicQuality(3) == HarmonyEngine::Major7);     // IV
    REQUIRE(engine.getDiatonicQuality(4) == HarmonyEngine::Dominant7);  // V
    REQUIRE(engine.getDiatonicQuality(5) == HarmonyEngine::Minor7);     // vi
    REQUIRE(engine.getDiatonicQuality(6) == HarmonyEngine::HalfDim7);   // vii
}

TEST_CASE("HarmonyEngine Chord Intervals", "[harmony]") {
    HarmonyEngine engine;

    // Major7: 0-4-7-11
    auto maj7Intervals = engine.getChordIntervals(HarmonyEngine::Major7);
    REQUIRE(maj7Intervals[0] == 0);
    REQUIRE(maj7Intervals[1] == 4);
    REQUIRE(maj7Intervals[2] == 7);
    REQUIRE(maj7Intervals[3] == 11);

    // Minor7: 0-3-7-10
    auto min7Intervals = engine.getChordIntervals(HarmonyEngine::Minor7);
    REQUIRE(min7Intervals[0] == 0);
    REQUIRE(min7Intervals[1] == 3);
    REQUIRE(min7Intervals[2] == 7);
    REQUIRE(min7Intervals[3] == 10);

    // Dominant7: 0-4-7-10
    auto dom7Intervals = engine.getChordIntervals(HarmonyEngine::Dominant7);
    REQUIRE(dom7Intervals[0] == 0);
    REQUIRE(dom7Intervals[1] == 4);
    REQUIRE(dom7Intervals[2] == 7);
    REQUIRE(dom7Intervals[3] == 10);

    // HalfDim7: 0-3-6-10
    auto halfDimIntervals = engine.getChordIntervals(HarmonyEngine::HalfDim7);
    REQUIRE(halfDimIntervals[0] == 0);
    REQUIRE(halfDimIntervals[1] == 3);
    REQUIRE(halfDimIntervals[2] == 6);
    REQUIRE(halfDimIntervals[3] == 10);
}

TEST_CASE("HarmonyEngine Basic Harmonization", "[harmony]") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);

    // C Major 7 (I in C): C-E-G-B = 60-64-67-71
    auto cChord = engine.harmonize(60, 0);
    REQUIRE(cChord.root == 60);
    REQUIRE(cChord.third == 64);
    REQUIRE(cChord.fifth == 67);
    REQUIRE(cChord.seventh == 71);

    // D minor 7 (ii in C): D-F-A-C = 62-65-69-72
    auto dChord = engine.harmonize(62, 1);
    REQUIRE(dChord.root == 62);
    REQUIRE(dChord.third == 65);
    REQUIRE(dChord.fifth == 69);
    REQUIRE(dChord.seventh == 72);

    // G Dominant 7 (V in C): G-B-D-F = 67-71-74-77
    auto gChord = engine.harmonize(67, 4);
    REQUIRE(gChord.root == 67);
    REQUIRE(gChord.third == 71);
    REQUIRE(gChord.fifth == 74);
    REQUIRE(gChord.seventh == 77);
}

TEST_CASE("HarmonyEngine MIDI Range Clamping", "[harmony]") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);

    // High note that would exceed MIDI 127
    auto highChord = engine.harmonize(120, 0); // G#8 Major7

    // All notes should clamp to 127
    REQUIRE(highChord.root <= 127);
    REQUIRE(highChord.third <= 127);
    REQUIRE(highChord.fifth <= 127);
    REQUIRE(highChord.seventh <= 127);

    // Low note boundary
    auto lowChord = engine.harmonize(0, 0); // C-1 Major7

    // All notes should be >= 0
    REQUIRE(lowChord.root >= 0);
    REQUIRE(lowChord.third >= 0);
    REQUIRE(lowChord.fifth >= 0);
    REQUIRE(lowChord.seventh >= 0);
}
