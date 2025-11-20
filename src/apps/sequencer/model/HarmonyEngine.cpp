#include "HarmonyEngine.h"
#include "Serialize.h"

// Scale interval table (semitones from root)
const uint8_t HarmonyEngine::ScaleIntervals[7][7] = {
    {0, 2, 4, 5, 7, 9, 11},  // Ionian: W-W-H-W-W-W-H
    {0, 2, 3, 5, 7, 9, 10},  // Dorian (Phase 2)
    {0, 1, 3, 5, 7, 8, 10},  // Phrygian (Phase 2)
    {0, 2, 4, 6, 7, 9, 11},  // Lydian (Phase 2)
    {0, 2, 4, 5, 7, 9, 10},  // Mixolydian (Phase 2)
    {0, 2, 3, 5, 7, 8, 10},  // Aeolian (Phase 2)
    {0, 1, 3, 5, 6, 8, 10}   // Locrian (Phase 2)
};

// Diatonic chord qualities for each mode
const HarmonyEngine::ChordQuality HarmonyEngine::DiatonicChords[7][7] = {
    // Ionian: I∆7, ii-7, iii-7, IV∆7, V7, vi-7, viiø
    {Major7, Minor7, Minor7, Major7, Dominant7, Minor7, HalfDim7},

    // Dorian: i-7, ii-7, ♭III∆7, IV7, v-7, viø, ♭VII∆7
    {Minor7, Minor7, Major7, Dominant7, Minor7, HalfDim7, Major7},

    // Phrygian: i-7, ♭II∆7, ♭III7, iv-7, vø, ♭VI∆7, ♭vii-7
    {Minor7, Major7, Dominant7, Minor7, HalfDim7, Major7, Minor7},

    // Lydian: I∆7, II7, iii-7, #ivø, V∆7, vi-7, vii-7
    {Major7, Dominant7, Minor7, HalfDim7, Major7, Minor7, Minor7},

    // Mixolydian: I7, ii-7, iiiø, IV∆7, v-7, vi-7, ♭VII∆7
    {Dominant7, Minor7, HalfDim7, Major7, Minor7, Minor7, Major7},

    // Aeolian: i-7, iiø, ♭III∆7, iv-7, v-7, ♭VI∆7, ♭VII7
    {Minor7, HalfDim7, Major7, Minor7, Minor7, Major7, Dominant7},

    // Locrian: iø, ♭II∆7, ♭iii-7, iv-7, ♭V∆7, ♭VI7, ♭vii-7
    {HalfDim7, Major7, Minor7, Minor7, Major7, Dominant7, Minor7}
};

// Chord interval table (semitones from root)
const uint8_t HarmonyEngine::ChordIntervalsTable[4][4] = {
    {0, 3, 7, 10},  // Minor7: R, ♭3, 5, ♭7
    {0, 4, 7, 10},  // Dominant7: R, 3, 5, ♭7
    {0, 4, 7, 11},  // Major7: R, 3, 5, 7
    {0, 3, 6, 10}   // HalfDim7: R, ♭3, ♭5, ♭7
};

HarmonyEngine::HarmonyEngine()
    : _mode(Ionian)
    , _diatonicMode(true)
    , _inversion(0)
    , _voicing(Close)
    , _transpose(0)
{}

int16_t HarmonyEngine::getScaleInterval(uint8_t degree) const {
    if (degree >= 7) degree %= 7;
    return ScaleIntervals[_mode][degree];
}

HarmonyEngine::ChordQuality HarmonyEngine::getDiatonicQuality(uint8_t scaleDegree) const {
    if (scaleDegree >= 7) scaleDegree %= 7;
    return DiatonicChords[_mode][scaleDegree];
}

HarmonyEngine::ChordIntervals HarmonyEngine::getChordIntervals(ChordQuality quality) const {
    ChordIntervals result;
    for (int i = 0; i < 4; i++) {
        result.intervals[i] = ChordIntervalsTable[quality][i];
    }
    return result;
}

HarmonyEngine::ChordNotes HarmonyEngine::harmonize(int16_t rootNote, uint8_t scaleDegree) const {
    ChordNotes chord;

    // Get chord quality for this scale degree (diatonic mode in Phase 1)
    ChordQuality quality = getDiatonicQuality(scaleDegree);

    // Get chord intervals
    ChordIntervals intervals = getChordIntervals(quality);

    // Apply intervals to root note
    chord.root = applyInterval(rootNote, intervals[0]);
    chord.third = applyInterval(rootNote, intervals[1]);
    chord.fifth = applyInterval(rootNote, intervals[2]);
    chord.seventh = applyInterval(rootNote, intervals[3]);

    // Phase 2: Apply inversion
    applyInversion(chord);

    // Phase 3: Apply voicing
    applyVoicing(chord);

    // Phase 2: Apply transpose
    applyTranspose(chord);

    return chord;
}

int16_t HarmonyEngine::applyInterval(int16_t baseNote, int16_t interval) const {
    int16_t result = baseNote + interval;
    return clamp(result, int16_t(0), int16_t(127)); // MIDI range 0-127
}

void HarmonyEngine::applyInversion(ChordNotes &chord) const {
    // Inversion reorders the chord tones by moving notes up an octave
    // Root position (0): R-3-5-7 (no change)
    // 1st inversion (1): 3-5-7-R (root up octave, third becomes bass)
    // 2nd inversion (2): 5-7-R-3 (root and third up octave, fifth becomes bass)
    // 3rd inversion (3): 7-R-3-5 (root, third, fifth up octave, seventh becomes bass)

    switch (_inversion) {
        case 0:
            // Root position - no change
            break;

        case 1:
            // 1st inversion: third becomes bass, root moves up an octave
            chord.root = applyInterval(chord.root, 12);
            break;

        case 2:
            // 2nd inversion: fifth becomes bass, root and third move up an octave
            chord.root = applyInterval(chord.root, 12);
            chord.third = applyInterval(chord.third, 12);
            break;

        case 3:
            // 3rd inversion: seventh becomes bass, root, third, and fifth move up an octave
            chord.root = applyInterval(chord.root, 12);
            chord.third = applyInterval(chord.third, 12);
            chord.fifth = applyInterval(chord.fifth, 12);
            break;

        default:
            // Should not reach here due to clamping in setInversion()
            break;
    }
}

void HarmonyEngine::applyVoicing(ChordNotes &chord) const {
    if (_voicing == Close) {
        return; // No transformation for close voicing
    }

    switch (_voicing) {
        case Drop2: {
            // Drop 2nd highest note down an octave
            // Rank each note by how many others it's higher than (0=lowest, 3=highest)
            int rootRank = (chord.root > chord.third) + (chord.root > chord.fifth) + (chord.root > chord.seventh);
            int thirdRank = (chord.third > chord.root) + (chord.third > chord.fifth) + (chord.third > chord.seventh);
            int fifthRank = (chord.fifth > chord.root) + (chord.fifth > chord.third) + (chord.fifth > chord.seventh);
            int seventhRank = (chord.seventh > chord.root) + (chord.seventh > chord.third) + (chord.seventh > chord.fifth);

            // 2nd highest has rank 2
            if (rootRank == 2) chord.root = applyInterval(chord.root, -12);
            else if (thirdRank == 2) chord.third = applyInterval(chord.third, -12);
            else if (fifthRank == 2) chord.fifth = applyInterval(chord.fifth, -12);
            else if (seventhRank == 2) chord.seventh = applyInterval(chord.seventh, -12);
            break;
        }
        case Drop3: {
            // Drop 3rd highest note down an octave
            int rootRank = (chord.root > chord.third) + (chord.root > chord.fifth) + (chord.root > chord.seventh);
            int thirdRank = (chord.third > chord.root) + (chord.third > chord.fifth) + (chord.third > chord.seventh);
            int fifthRank = (chord.fifth > chord.root) + (chord.fifth > chord.third) + (chord.fifth > chord.seventh);
            int seventhRank = (chord.seventh > chord.root) + (chord.seventh > chord.third) + (chord.seventh > chord.fifth);

            // 3rd highest has rank 1
            if (rootRank == 1) chord.root = applyInterval(chord.root, -12);
            else if (thirdRank == 1) chord.third = applyInterval(chord.third, -12);
            else if (fifthRank == 1) chord.fifth = applyInterval(chord.fifth, -12);
            else if (seventhRank == 1) chord.seventh = applyInterval(chord.seventh, -12);
            break;
        }
        case Spread: {
            // Wide voicing: root stays as bass, others move up an octave
            chord.third = applyInterval(chord.third, 12);
            chord.fifth = applyInterval(chord.fifth, 12);
            chord.seventh = applyInterval(chord.seventh, 12);
            break;
        }
        default:
            break;
    }
}

void HarmonyEngine::applyTranspose(ChordNotes &chord) const {
    // Transpose shifts all chord notes by the same interval
    // Range: -24 to +24 semitones (±2 octaves)
    // applyInterval() handles MIDI range clamping (0-127)

    if (_transpose == 0) {
        return; // No transpose
    }

    // Apply transpose to all chord notes
    chord.root = applyInterval(chord.root, _transpose);
    chord.third = applyInterval(chord.third, _transpose);
    chord.fifth = applyInterval(chord.fifth, _transpose);
    chord.seventh = applyInterval(chord.seventh, _transpose);
}

void HarmonyEngine::write(VersionedSerializedWriter &writer) const {
    // Pack harmony parameters into bytes (bit-packed pattern)
    uint8_t flags = (static_cast<uint8_t>(_mode) << 0) |       // 3 bits (0-6)
                    (static_cast<uint8_t>(_diatonicMode) << 3) | // 1 bit
                    (static_cast<uint8_t>(_inversion) << 4) |     // 2 bits (0-3)
                    (static_cast<uint8_t>(_voicing) << 6);        // 2 bits (0-3)
    writer.write(flags);
    writer.write(_transpose);
}

void HarmonyEngine::read(VersionedSerializedReader &reader) {
    uint8_t flags;
    reader.read(flags);

    _mode = static_cast<Mode>((flags >> 0) & 0x7);      // 3 bits
    _diatonicMode = ((flags >> 3) & 0x1) != 0;           // 1 bit
    _inversion = (flags >> 4) & 0x3;                     // 2 bits
    _voicing = static_cast<Voicing>((flags >> 6) & 0x3); // 2 bits

    reader.read(_transpose);
}
