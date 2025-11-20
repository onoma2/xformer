#include "UnitTest.h"

#include "apps/sequencer/model/HarmonyEngine.h"

UNIT_TEST("HarmonyEngine") {

CASE("default_construction") {
    HarmonyEngine engine;

    expectEqual(static_cast<int>(engine.mode()), static_cast<int>(HarmonyEngine::Ionian), "default mode should be Ionian");
    expectTrue(engine.diatonicMode(), "default diatonicMode should be true");
    expectEqual(static_cast<int>(engine.inversion()), 0, "default inversion should be 0");
    expectEqual(static_cast<int>(engine.voicing()), static_cast<int>(HarmonyEngine::Close), "default voicing should be Close");
    expectEqual(static_cast<int>(engine.transpose()), 0, "default transpose should be 0");
}

CASE("ionian_scale_intervals") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);

    // Ionian intervals: W-W-H-W-W-W-H = 0-2-4-5-7-9-11
    const int16_t expected[7] = {0, 2, 4, 5, 7, 9, 11};

    for (int degree = 0; degree < 7; degree++) {
        expectEqual(engine.getScaleInterval(degree), expected[degree], "Ionian scale interval mismatch");
    }
}

CASE("ionian_diatonic_chord_qualities") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setDiatonicMode(true);

    // Ionian: I∆7, ii-7, iii-7, IV∆7, V7, vi-7, viiø
    expectEqual(static_cast<int>(engine.getDiatonicQuality(0)), static_cast<int>(HarmonyEngine::Major7), "I should be Major7");
    expectEqual(static_cast<int>(engine.getDiatonicQuality(1)), static_cast<int>(HarmonyEngine::Minor7), "ii should be Minor7");
    expectEqual(static_cast<int>(engine.getDiatonicQuality(2)), static_cast<int>(HarmonyEngine::Minor7), "iii should be Minor7");
    expectEqual(static_cast<int>(engine.getDiatonicQuality(3)), static_cast<int>(HarmonyEngine::Major7), "IV should be Major7");
    expectEqual(static_cast<int>(engine.getDiatonicQuality(4)), static_cast<int>(HarmonyEngine::Dominant7), "V should be Dominant7");
    expectEqual(static_cast<int>(engine.getDiatonicQuality(5)), static_cast<int>(HarmonyEngine::Minor7), "vi should be Minor7");
    expectEqual(static_cast<int>(engine.getDiatonicQuality(6)), static_cast<int>(HarmonyEngine::HalfDim7), "vii should be HalfDim7");
}

CASE("chord_intervals") {
    HarmonyEngine engine;

    // Major7: 0-4-7-11
    auto maj7Intervals = engine.getChordIntervals(HarmonyEngine::Major7);
    expectEqual(static_cast<int>(maj7Intervals[0]), 0, "Major7 root interval");
    expectEqual(static_cast<int>(maj7Intervals[1]), 4, "Major7 third interval");
    expectEqual(static_cast<int>(maj7Intervals[2]), 7, "Major7 fifth interval");
    expectEqual(static_cast<int>(maj7Intervals[3]), 11, "Major7 seventh interval");

    // Minor7: 0-3-7-10
    auto min7Intervals = engine.getChordIntervals(HarmonyEngine::Minor7);
    expectEqual(static_cast<int>(min7Intervals[0]), 0, "Minor7 root interval");
    expectEqual(static_cast<int>(min7Intervals[1]), 3, "Minor7 third interval");
    expectEqual(static_cast<int>(min7Intervals[2]), 7, "Minor7 fifth interval");
    expectEqual(static_cast<int>(min7Intervals[3]), 10, "Minor7 seventh interval");

    // Dominant7: 0-4-7-10
    auto dom7Intervals = engine.getChordIntervals(HarmonyEngine::Dominant7);
    expectEqual(static_cast<int>(dom7Intervals[0]), 0, "Dominant7 root interval");
    expectEqual(static_cast<int>(dom7Intervals[1]), 4, "Dominant7 third interval");
    expectEqual(static_cast<int>(dom7Intervals[2]), 7, "Dominant7 fifth interval");
    expectEqual(static_cast<int>(dom7Intervals[3]), 10, "Dominant7 seventh interval");

    // HalfDim7: 0-3-6-10
    auto halfDimIntervals = engine.getChordIntervals(HarmonyEngine::HalfDim7);
    expectEqual(static_cast<int>(halfDimIntervals[0]), 0, "HalfDim7 root interval");
    expectEqual(static_cast<int>(halfDimIntervals[1]), 3, "HalfDim7 third interval");
    expectEqual(static_cast<int>(halfDimIntervals[2]), 6, "HalfDim7 fifth interval");
    expectEqual(static_cast<int>(halfDimIntervals[3]), 10, "HalfDim7 seventh interval");
}

CASE("basic_harmonization") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);

    // C Major 7 (I in C): C-E-G-B = 60-64-67-71
    auto cChord = engine.harmonize(60, 0);
    expectEqual(static_cast<int>(cChord.root), 60, "C Major7 root");
    expectEqual(static_cast<int>(cChord.third), 64, "C Major7 third");
    expectEqual(static_cast<int>(cChord.fifth), 67, "C Major7 fifth");
    expectEqual(static_cast<int>(cChord.seventh), 71, "C Major7 seventh");

    // D minor 7 (ii in C): D-F-A-C = 62-65-69-72
    auto dChord = engine.harmonize(62, 1);
    expectEqual(static_cast<int>(dChord.root), 62, "D minor7 root");
    expectEqual(static_cast<int>(dChord.third), 65, "D minor7 third");
    expectEqual(static_cast<int>(dChord.fifth), 69, "D minor7 fifth");
    expectEqual(static_cast<int>(dChord.seventh), 72, "D minor7 seventh");

    // G Dominant 7 (V in C): G-B-D-F = 67-71-74-77
    auto gChord = engine.harmonize(67, 4);
    expectEqual(static_cast<int>(gChord.root), 67, "G Dominant7 root");
    expectEqual(static_cast<int>(gChord.third), 71, "G Dominant7 third");
    expectEqual(static_cast<int>(gChord.fifth), 74, "G Dominant7 fifth");
    expectEqual(static_cast<int>(gChord.seventh), 77, "G Dominant7 seventh");
}

CASE("midi_range_clamping") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);

    // High note that would exceed MIDI 127
    auto highChord = engine.harmonize(120, 0); // G#8 Major7

    // All notes should clamp to 127
    expectTrue(highChord.root <= 127, "high chord root should clamp to 127");
    expectTrue(highChord.third <= 127, "high chord third should clamp to 127");
    expectTrue(highChord.fifth <= 127, "high chord fifth should clamp to 127");
    expectTrue(highChord.seventh <= 127, "high chord seventh should clamp to 127");

    // Low note boundary
    auto lowChord = engine.harmonize(0, 0); // C-1 Major7

    // All notes should be >= 0
    expectTrue(lowChord.root >= 0, "low chord root should be >= 0");
    expectTrue(lowChord.third >= 0, "low chord third should be >= 0");
    expectTrue(lowChord.fifth >= 0, "low chord fifth should be >= 0");
    expectTrue(lowChord.seventh >= 0, "low chord seventh should be >= 0");
}

// Phase 2 Task 2.2: Inversion Logic Tests

CASE("inversion_root_position") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setInversion(0); // Root position

    // C Major 7 in root position: C(60)-E(64)-G(67)-B(71)
    auto chord = engine.harmonize(60, 0);
    expectEqual(static_cast<int>(chord.root), 60, "root position: root should be C(60)");
    expectEqual(static_cast<int>(chord.third), 64, "root position: third should be E(64)");
    expectEqual(static_cast<int>(chord.fifth), 67, "root position: fifth should be G(67)");
    expectEqual(static_cast<int>(chord.seventh), 71, "root position: seventh should be B(71)");

    // Lowest note should be root
    int16_t lowestNote = chord.root;
    if (chord.third < lowestNote) lowestNote = chord.third;
    if (chord.fifth < lowestNote) lowestNote = chord.fifth;
    if (chord.seventh < lowestNote) lowestNote = chord.seventh;
    expectEqual(static_cast<int>(lowestNote), static_cast<int>(chord.root), "root position: lowest note should be root");
}

CASE("inversion_first") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setInversion(1); // 1st inversion

    // C Major 7 in 1st inversion: E(64)-G(67)-B(71)-C(72)
    // 3rd becomes bass, root moves up an octave
    auto chord = engine.harmonize(60, 0);

    // Expected: third stays at 64 (bass), root up to 72
    expectEqual(static_cast<int>(chord.third), 64, "1st inversion: third should remain at E(64) as bass");
    expectEqual(static_cast<int>(chord.fifth), 67, "1st inversion: fifth should remain at G(67)");
    expectEqual(static_cast<int>(chord.seventh), 71, "1st inversion: seventh should remain at B(71)");
    expectEqual(static_cast<int>(chord.root), 72, "1st inversion: root should move up octave to C(72)");

    // Lowest note should be 3rd
    int16_t lowestNote = chord.root;
    if (chord.third < lowestNote) lowestNote = chord.third;
    if (chord.fifth < lowestNote) lowestNote = chord.fifth;
    if (chord.seventh < lowestNote) lowestNote = chord.seventh;
    expectEqual(static_cast<int>(lowestNote), static_cast<int>(chord.third), "1st inversion: lowest note should be third");
}

CASE("inversion_second") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setInversion(2); // 2nd inversion

    // C Major 7 in 2nd inversion: G(67)-B(71)-C(72)-E(76)
    // 5th becomes bass, root and 3rd move up an octave
    auto chord = engine.harmonize(60, 0);

    // Expected: fifth stays at 67 (bass), root and third up octaves
    expectEqual(static_cast<int>(chord.fifth), 67, "2nd inversion: fifth should remain at G(67) as bass");
    expectEqual(static_cast<int>(chord.seventh), 71, "2nd inversion: seventh should remain at B(71)");
    expectEqual(static_cast<int>(chord.root), 72, "2nd inversion: root should move up octave to C(72)");
    expectEqual(static_cast<int>(chord.third), 76, "2nd inversion: third should move up octave to E(76)");

    // Lowest note should be 5th
    int16_t lowestNote = chord.root;
    if (chord.third < lowestNote) lowestNote = chord.third;
    if (chord.fifth < lowestNote) lowestNote = chord.fifth;
    if (chord.seventh < lowestNote) lowestNote = chord.seventh;
    expectEqual(static_cast<int>(lowestNote), static_cast<int>(chord.fifth), "2nd inversion: lowest note should be fifth");
}

CASE("inversion_third") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setInversion(3); // 3rd inversion

    // C Major 7 in 3rd inversion: B(71)-C(72)-E(76)-G(79)
    // 7th becomes bass, root, 3rd, and 5th move up an octave
    auto chord = engine.harmonize(60, 0);

    // Expected: seventh stays at 71 (bass), root/third/fifth up octaves
    expectEqual(static_cast<int>(chord.seventh), 71, "3rd inversion: seventh should remain at B(71) as bass");
    expectEqual(static_cast<int>(chord.root), 72, "3rd inversion: root should move up octave to C(72)");
    expectEqual(static_cast<int>(chord.third), 76, "3rd inversion: third should move up octave to E(76)");
    expectEqual(static_cast<int>(chord.fifth), 79, "3rd inversion: fifth should move up octave to G(79)");

    // Lowest note should be 7th
    int16_t lowestNote = chord.root;
    if (chord.third < lowestNote) lowestNote = chord.third;
    if (chord.fifth < lowestNote) lowestNote = chord.fifth;
    if (chord.seventh < lowestNote) lowestNote = chord.seventh;
    expectEqual(static_cast<int>(lowestNote), static_cast<int>(chord.seventh), "3rd inversion: lowest note should be seventh");
}

CASE("inversion_boundary_clamping") {
    HarmonyEngine engine;

    // Test clamping to 0-3 range
    engine.setInversion(5);
    expectEqual(static_cast<int>(engine.inversion()), 3, "inversion should clamp to max 3");

    engine.setInversion(10);
    expectEqual(static_cast<int>(engine.inversion()), 3, "inversion should clamp to max 3");

    engine.setInversion(0);
    expectEqual(static_cast<int>(engine.inversion()), 0, "inversion 0 should be valid");

    engine.setInversion(3);
    expectEqual(static_cast<int>(engine.inversion()), 3, "inversion 3 should be valid");
}

CASE("inversion_with_minor_chord") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Aeolian); // Natural minor
    engine.setInversion(1); // 1st inversion

    // D minor 7 (i in D Aeolian): D(62)-F(65)-A(69)-C(72)
    // 1st inversion: F(65)-A(69)-C(72)-D(74)
    auto chord = engine.harmonize(62, 0); // D, degree 0 in Aeolian = minor7

    expectEqual(static_cast<int>(chord.third), 65, "1st inversion minor: third at F(65) as bass");
    expectEqual(static_cast<int>(chord.fifth), 69, "1st inversion minor: fifth at A(69)");
    expectEqual(static_cast<int>(chord.seventh), 72, "1st inversion minor: seventh at C(72)");
    expectEqual(static_cast<int>(chord.root), 74, "1st inversion minor: root up octave to D(74)");

    // Verify quality is Minor7
    expectEqual(static_cast<int>(engine.getDiatonicQuality(0)), static_cast<int>(HarmonyEngine::Minor7), "Aeolian degree 0 should be Minor7");
}

// Phase 2 Task 2.3: Transpose Logic Tests

CASE("transpose_up_octave") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setTranspose(12); // Up 1 octave

    // C Major 7 transposed up 1 octave
    auto chord = engine.harmonize(60, 0);
    expectEqual(static_cast<int>(chord.root), 72, "transpose +12: root should be C5(72)");
    expectEqual(static_cast<int>(chord.third), 76, "transpose +12: third should be E5(76)");
    expectEqual(static_cast<int>(chord.fifth), 79, "transpose +12: fifth should be G5(79)");
    expectEqual(static_cast<int>(chord.seventh), 83, "transpose +12: seventh should be B5(83)");
}

CASE("transpose_down_octave") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setTranspose(-12); // Down 1 octave

    // C Major 7 transposed down 1 octave
    auto chord = engine.harmonize(60, 0);
    expectEqual(static_cast<int>(chord.root), 48, "transpose -12: root should be C3(48)");
    expectEqual(static_cast<int>(chord.third), 52, "transpose -12: third should be E3(52)");
    expectEqual(static_cast<int>(chord.fifth), 55, "transpose -12: fifth should be G3(55)");
    expectEqual(static_cast<int>(chord.seventh), 59, "transpose -12: seventh should be B3(59)");
}

CASE("transpose_parameter_clamping") {
    HarmonyEngine engine;

    // Test clamping to ±24 range
    engine.setTranspose(30);
    expectEqual(static_cast<int>(engine.transpose()), 24, "transpose should clamp to max +24");

    engine.setTranspose(-30);
    expectEqual(static_cast<int>(engine.transpose()), -24, "transpose should clamp to min -24");

    engine.setTranspose(0);
    expectEqual(static_cast<int>(engine.transpose()), 0, "transpose 0 should be valid");

    engine.setTranspose(24);
    expectEqual(static_cast<int>(engine.transpose()), 24, "transpose +24 should be valid");

    engine.setTranspose(-24);
    expectEqual(static_cast<int>(engine.transpose()), -24, "transpose -24 should be valid");
}

CASE("transpose_midi_range_clamping") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setTranspose(24); // +2 octaves

    // High root note that would exceed MIDI 127
    auto chord = engine.harmonize(120, 0); // G#8 Major7

    // All notes should clamp to 127
    expectTrue(chord.root <= 127, "transpose with high note: root should clamp to 127");
    expectTrue(chord.third <= 127, "transpose with high note: third should clamp to 127");
    expectTrue(chord.fifth <= 127, "transpose with high note: fifth should clamp to 127");
    expectTrue(chord.seventh <= 127, "transpose with high note: seventh should clamp to 127");

    // Test low boundary
    engine.setTranspose(-24); // -2 octaves
    auto lowChord = engine.harmonize(10, 0); // A#-1 Major7

    // All notes should be >= 0
    expectTrue(lowChord.root >= 0, "transpose with low note: root should be >= 0");
    expectTrue(lowChord.third >= 0, "transpose with low note: third should be >= 0");
    expectTrue(lowChord.fifth >= 0, "transpose with low note: fifth should be >= 0");
    expectTrue(lowChord.seventh >= 0, "transpose with low note: seventh should be >= 0");
}

CASE("transpose_with_inversion") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setInversion(1); // 1st inversion
    engine.setTranspose(12); // Up 1 octave

    // C Major 7 in 1st inversion, transposed up octave
    // Base 1st inversion: E(64)-G(67)-B(71)-C(72)
    // After transpose +12: E(76)-G(79)-B(83)-C(84)
    auto chord = engine.harmonize(60, 0);

    expectEqual(static_cast<int>(chord.third), 76, "1st inv + transpose: third at E5(76)");
    expectEqual(static_cast<int>(chord.fifth), 79, "1st inv + transpose: fifth at G5(79)");
    expectEqual(static_cast<int>(chord.seventh), 83, "1st inv + transpose: seventh at B5(83)");
    expectEqual(static_cast<int>(chord.root), 84, "1st inv + transpose: root at C6(84)");
}

} // UNIT_TEST("HarmonyEngine")
