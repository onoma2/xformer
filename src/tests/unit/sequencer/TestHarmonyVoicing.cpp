#include "UnitTest.h"

#include "apps/sequencer/model/HarmonyEngine.h"

// =============================================================================
// Unit tests for HarmonyEngine::applyVoicing()
//
// Voicing Types:
// - Close (0): No transformation - notes in close position
// - Drop2 (1): Drop 2nd highest note down an octave
// - Drop3 (2): Drop 3rd highest note down an octave
// - Spread (3): Wide voicing - root low, others spread higher
//
// For a C Major7 chord in root position: C(60)-E(64)-G(67)-B(71)
// Notes from low to high: root(60), third(64), fifth(67), seventh(71)
// =============================================================================

UNIT_TEST("Harmony Voicing") {

// =============================================================================
// SECTION 1: Close Voicing (No Change)
// =============================================================================

CASE("close_voicing_no_transformation") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setVoicing(HarmonyEngine::Close);

    // C Major7: C(60)-E(64)-G(67)-B(71)
    auto chord = engine.harmonize(60, 0);

    expectEqual(static_cast<int>(chord.root), 60, "close: root at 60");
    expectEqual(static_cast<int>(chord.third), 64, "close: third at 64");
    expectEqual(static_cast<int>(chord.fifth), 67, "close: fifth at 67");
    expectEqual(static_cast<int>(chord.seventh), 71, "close: seventh at 71");
}

// =============================================================================
// SECTION 2: Drop2 Voicing
// =============================================================================

// Drop2: Take 2nd highest note and drop it an octave
// Close position: root(60)-third(64)-fifth(67)-seventh(71)
// 2nd highest = fifth(67)
// Drop2 result: fifth(55)-root(60)-third(64)-seventh(71)

CASE("drop2_voicing_basic") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setVoicing(HarmonyEngine::Drop2);

    // C Major7 with Drop2
    auto chord = engine.harmonize(60, 0);

    // 2nd highest (fifth=67) drops to 55
    expectEqual(static_cast<int>(chord.fifth), 55, "drop2: fifth drops to 55 (-12)");
    expectEqual(static_cast<int>(chord.root), 60, "drop2: root stays at 60");
    expectEqual(static_cast<int>(chord.third), 64, "drop2: third stays at 64");
    expectEqual(static_cast<int>(chord.seventh), 71, "drop2: seventh stays at 71");
}

CASE("drop2_creates_wider_voicing") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setVoicing(HarmonyEngine::Drop2);

    auto chord = engine.harmonize(60, 0);

    // Verify the spread is wider than close voicing
    // Close: 71-60 = 11 semitones
    // Drop2: 71-55 = 16 semitones
    int lowestNote = chord.fifth; // Should be 55
    int highestNote = chord.seventh; // Should be 71

    expectTrue(highestNote - lowestNote > 11, "drop2 should create wider spread than close");
}

CASE("drop2_with_first_inversion") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setInversion(1); // 1st inversion
    engine.setVoicing(HarmonyEngine::Drop2);

    // 1st inversion: third(64)-fifth(67)-seventh(71)-root(72)
    // 2nd highest = seventh(71)
    // Drop2: seventh(59)-third(64)-fifth(67)-root(72)
    auto chord = engine.harmonize(60, 0);

    expectEqual(static_cast<int>(chord.seventh), 59, "drop2 + 1st inv: seventh drops to 59");
    expectEqual(static_cast<int>(chord.third), 64, "drop2 + 1st inv: third at 64");
    expectEqual(static_cast<int>(chord.fifth), 67, "drop2 + 1st inv: fifth at 67");
    expectEqual(static_cast<int>(chord.root), 72, "drop2 + 1st inv: root at 72");
}

CASE("drop2_with_minor_chord") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Dorian); // Dorian I = minor7
    engine.setVoicing(HarmonyEngine::Drop2);

    // D minor7: D(62)-F(65)-A(69)-C(72)
    // 2nd highest = A(69)
    // Drop2: A(57)-D(62)-F(65)-C(72)
    auto chord = engine.harmonize(62, 0);

    expectEqual(static_cast<int>(chord.fifth), 57, "drop2 minor: fifth drops to 57");
    expectEqual(static_cast<int>(chord.root), 62, "drop2 minor: root at 62");
    expectEqual(static_cast<int>(chord.third), 65, "drop2 minor: third at 65");
    expectEqual(static_cast<int>(chord.seventh), 72, "drop2 minor: seventh at 72");
}

// =============================================================================
// SECTION 3: Drop3 Voicing
// =============================================================================

// Drop3: Take 3rd highest note and drop it an octave
// Close position: root(60)-third(64)-fifth(67)-seventh(71)
// 3rd highest = third(64)
// Drop3 result: third(52)-root(60)-fifth(67)-seventh(71)

CASE("drop3_voicing_basic") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setVoicing(HarmonyEngine::Drop3);

    // C Major7 with Drop3
    auto chord = engine.harmonize(60, 0);

    // 3rd highest (third=64) drops to 52
    expectEqual(static_cast<int>(chord.third), 52, "drop3: third drops to 52 (-12)");
    expectEqual(static_cast<int>(chord.root), 60, "drop3: root stays at 60");
    expectEqual(static_cast<int>(chord.fifth), 67, "drop3: fifth stays at 67");
    expectEqual(static_cast<int>(chord.seventh), 71, "drop3: seventh stays at 71");
}

CASE("drop3_creates_widest_drop_voicing") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setVoicing(HarmonyEngine::Drop3);

    auto chord = engine.harmonize(60, 0);

    // Verify spread is even wider
    // Drop3: 71-52 = 19 semitones
    int lowestNote = chord.third; // Should be 52
    int highestNote = chord.seventh; // Should be 71

    expectTrue(highestNote - lowestNote > 16, "drop3 should be wider than drop2");
}

CASE("drop3_with_second_inversion") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setInversion(2); // 2nd inversion
    engine.setVoicing(HarmonyEngine::Drop3);

    // 2nd inversion: fifth(67)-seventh(71)-root(72)-third(76)
    // 3rd highest = seventh(71)
    // Drop3: seventh(59)-fifth(67)-root(72)-third(76)
    auto chord = engine.harmonize(60, 0);

    expectEqual(static_cast<int>(chord.seventh), 59, "drop3 + 2nd inv: seventh drops to 59");
    expectEqual(static_cast<int>(chord.fifth), 67, "drop3 + 2nd inv: fifth at 67");
    expectEqual(static_cast<int>(chord.root), 72, "drop3 + 2nd inv: root at 72");
    expectEqual(static_cast<int>(chord.third), 76, "drop3 + 2nd inv: third at 76");
}

// =============================================================================
// SECTION 4: Spread Voicing
// =============================================================================

// Spread: Wide open voicing - root stays, others move up octaves
// Common approach: root low, 5th up octave, 3rd up octave, 7th stays or up
// Close: root(60)-third(64)-fifth(67)-seventh(71)
// Spread: root(60)-fifth(79)-third(76)-seventh(83)
// Or simpler: root stays, 3rd up octave, 5th up octave, 7th up octave

CASE("spread_voicing_basic") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setVoicing(HarmonyEngine::Spread);

    // C Major7 with Spread
    auto chord = engine.harmonize(60, 0);

    // Root should stay at bass
    expectEqual(static_cast<int>(chord.root), 60, "spread: root stays at 60");

    // Other notes should be spread higher (at least one octave difference)
    // The exact algorithm may vary, but the spread should be significant
    int lowestNote = chord.root;
    int highestNote = chord.seventh;
    if (chord.third > highestNote) highestNote = chord.third;
    if (chord.fifth > highestNote) highestNote = chord.fifth;

    // Spread should create at least 23 semitones of range (nearly 2 octaves)
    // Close voicing: 71-60 = 11, Spread: 83-60 = 23
    expectTrue(highestNote - lowestNote >= 23, "spread should span at least 23 semitones");
}

CASE("spread_voicing_all_notes_distinct") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setVoicing(HarmonyEngine::Spread);

    auto chord = engine.harmonize(60, 0);

    // All 4 notes should be distinct
    expectTrue(chord.root != chord.third, "spread: root != third");
    expectTrue(chord.root != chord.fifth, "spread: root != fifth");
    expectTrue(chord.root != chord.seventh, "spread: root != seventh");
    expectTrue(chord.third != chord.fifth, "spread: third != fifth");
    expectTrue(chord.third != chord.seventh, "spread: third != seventh");
    expectTrue(chord.fifth != chord.seventh, "spread: fifth != seventh");
}

CASE("spread_voicing_proper_ordering") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setVoicing(HarmonyEngine::Spread);

    auto chord = engine.harmonize(60, 0);

    // In spread voicing, root should be the lowest
    int lowestNote = chord.root;
    if (chord.third < lowestNote) lowestNote = chord.third;
    if (chord.fifth < lowestNote) lowestNote = chord.fifth;
    if (chord.seventh < lowestNote) lowestNote = chord.seventh;

    expectEqual(static_cast<int>(lowestNote), static_cast<int>(chord.root), "spread: root should be lowest");
}

// =============================================================================
// SECTION 5: Voicing with Transpose
// =============================================================================

CASE("voicing_with_transpose") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setVoicing(HarmonyEngine::Drop2);
    engine.setTranspose(12); // Up one octave

    auto chord = engine.harmonize(60, 0);

    // Drop2 then transpose: (55,60,64,71) -> (67,72,76,83)
    expectEqual(static_cast<int>(chord.fifth), 67, "drop2 + transpose: fifth at 67");
    expectEqual(static_cast<int>(chord.root), 72, "drop2 + transpose: root at 72");
    expectEqual(static_cast<int>(chord.third), 76, "drop2 + transpose: third at 76");
    expectEqual(static_cast<int>(chord.seventh), 83, "drop2 + transpose: seventh at 83");
}

// =============================================================================
// SECTION 6: Voicing Edge Cases
// =============================================================================

CASE("voicing_midi_range_clamping_high") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setVoicing(HarmonyEngine::Spread);

    // High note that might exceed 127
    auto chord = engine.harmonize(108, 0); // C7

    // All notes should clamp to <= 127
    expectTrue(chord.root <= 127, "spread high: root <= 127");
    expectTrue(chord.third <= 127, "spread high: third <= 127");
    expectTrue(chord.fifth <= 127, "spread high: fifth <= 127");
    expectTrue(chord.seventh <= 127, "spread high: seventh <= 127");
}

CASE("voicing_midi_range_clamping_low") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setVoicing(HarmonyEngine::Drop3);

    // Low note that might go below 0 after drop
    auto chord = engine.harmonize(12, 0); // C0

    // All notes should be >= 0
    expectTrue(chord.root >= 0, "drop3 low: root >= 0");
    expectTrue(chord.third >= 0, "drop3 low: third >= 0");
    expectTrue(chord.fifth >= 0, "drop3 low: fifth >= 0");
    expectTrue(chord.seventh >= 0, "drop3 low: seventh >= 0");
}

CASE("voicing_parameter_persistence") {
    HarmonyEngine engine;

    engine.setVoicing(HarmonyEngine::Drop2);
    expectEqual(static_cast<int>(engine.voicing()), static_cast<int>(HarmonyEngine::Drop2), "voicing should persist as Drop2");

    engine.setVoicing(HarmonyEngine::Drop3);
    expectEqual(static_cast<int>(engine.voicing()), static_cast<int>(HarmonyEngine::Drop3), "voicing should persist as Drop3");

    engine.setVoicing(HarmonyEngine::Spread);
    expectEqual(static_cast<int>(engine.voicing()), static_cast<int>(HarmonyEngine::Spread), "voicing should persist as Spread");
}

// =============================================================================
// SECTION 7: Different Voicings Produce Different Results
// =============================================================================

CASE("all_voicings_produce_different_results") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);

    // Get chords for all voicings
    HarmonyEngine::ChordNotes chords[4];

    engine.setVoicing(HarmonyEngine::Close);
    chords[0] = engine.harmonize(60, 0);

    engine.setVoicing(HarmonyEngine::Drop2);
    chords[1] = engine.harmonize(60, 0);

    engine.setVoicing(HarmonyEngine::Drop3);
    chords[2] = engine.harmonize(60, 0);

    engine.setVoicing(HarmonyEngine::Spread);
    chords[3] = engine.harmonize(60, 0);

    // Each voicing should produce at least one different note
    // Close vs Drop2
    bool closeVsDrop2 = (chords[0].root != chords[1].root) ||
                        (chords[0].third != chords[1].third) ||
                        (chords[0].fifth != chords[1].fifth) ||
                        (chords[0].seventh != chords[1].seventh);
    expectTrue(closeVsDrop2, "Close and Drop2 should differ");

    // Drop2 vs Drop3
    bool drop2VsDrop3 = (chords[1].root != chords[2].root) ||
                        (chords[1].third != chords[2].third) ||
                        (chords[1].fifth != chords[2].fifth) ||
                        (chords[1].seventh != chords[2].seventh);
    expectTrue(drop2VsDrop3, "Drop2 and Drop3 should differ");

    // Drop3 vs Spread
    bool drop3VsSpread = (chords[2].root != chords[3].root) ||
                         (chords[2].third != chords[3].third) ||
                         (chords[2].fifth != chords[3].fifth) ||
                         (chords[2].seventh != chords[3].seventh);
    expectTrue(drop3VsSpread, "Drop3 and Spread should differ");
}

CASE("each_voicing_has_unique_bass_note") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);

    // Helper to find lowest note
    auto findLowest = [](const HarmonyEngine::ChordNotes& c) {
        int lowest = c.root;
        if (c.third < lowest) lowest = c.third;
        if (c.fifth < lowest) lowest = c.fifth;
        if (c.seventh < lowest) lowest = c.seventh;
        return lowest;
    };

    engine.setVoicing(HarmonyEngine::Close);
    int closeLowest = findLowest(engine.harmonize(60, 0));

    engine.setVoicing(HarmonyEngine::Drop2);
    int drop2Lowest = findLowest(engine.harmonize(60, 0));

    engine.setVoicing(HarmonyEngine::Drop3);
    int drop3Lowest = findLowest(engine.harmonize(60, 0));

    engine.setVoicing(HarmonyEngine::Spread);
    int spreadLowest = findLowest(engine.harmonize(60, 0));

    // Close: root (60), Drop2: fifth (55), Drop3: third (52), Spread: root (60)
    expectEqual(closeLowest, 60, "close lowest should be root (60)");
    expectEqual(drop2Lowest, 55, "drop2 lowest should be fifth (55)");
    expectEqual(drop3Lowest, 52, "drop3 lowest should be third (52)");
    expectEqual(spreadLowest, 60, "spread lowest should be root (60)");
}

} // UNIT_TEST("Harmony Voicing")
