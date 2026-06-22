// Verbatim port of the scale-table subset of table.c (VM-free).
#include "TT2Helpers.h"

const uint16_t table_nr[32] = { 0x8888, 0x888A, 0x8892, 0x8894, 0x88A2, 0x88A4,
                                0x8912, 0x8914, 0x8922, 0x8924, 0x8A8A, 0x8AAA,
                                0x9292, 0x92AA, 0x94AA, 0x952A, 0x8282, 0x828A,
                                0x8292, 0x82A2, 0x8484, 0x848A, 0x8492, 0x8494,
                                0x84A2, 0x84A4, 0x850A, 0x8512, 0x8514, 0x8522,
                                0x8524, 0x8544 };

// scales for N.S op
const uint8_t table_n_s[9][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },  // Major
    { 0, 2, 3, 5, 7, 8, 10 },  // Natural Minor
    { 0, 2, 3, 5, 7, 8, 11 },  // Harmonic Minor
    { 0, 2, 3, 5, 7, 9, 11 },  // Melodic Minor
    { 0, 2, 3, 5, 7, 9, 10 },  // Dorian
    { 0, 1, 3, 5, 7, 8, 10 },  // Phrygian
    { 0, 2, 4, 6, 7, 9, 11 },  // Lydian
    { 0, 2, 4, 5, 7, 9, 10 },  // Mixolydian
    { 0, 1, 3, 5, 6, 8, 10 },  // Locrian
};

// chords for N.C op
const uint8_t table_n_c[13][4] = {
    { 0, 4, 7, 11 },  // Major 7th       - 0
    { 0, 3, 7, 10 },  // Minor 7th       - 1
    { 0, 4, 7, 10 },  // Dominant 7th    - 2
    { 0, 3, 6, 9 },   // Diminished 7th  - 3
    { 0, 4, 8, 10 },  // Augmented 7th   - 4
    { 0, 4, 6, 10 },  // Dominant 7b5    - 5
    { 0, 3, 6, 10 },  // Minor 7b5       - 6
    { 0, 4, 8, 11 },  // Major 7#5       - 7
    { 0, 3, 7, 11 },  // Minor major 7th - 8
    { 0, 3, 6, 11 },  // Dim Major 7th   - 9
    { 0, 4, 7, 9 },   // Major 6th       - 10
    { 0, 3, 7, 9 },   // Minor 6th       - 11
    { 0, 5, 7, 10 },  // 7th sus 4       - 12
};

// chord scales for N.CS op - values are indices into table_n_c
const uint8_t table_n_cs[9][7] = {
    { 0, 1, 1, 0, 2, 1, 6 },  // Major
    { 1, 6, 0, 1, 1, 0, 2 },  // Natural Minor
    { 8, 6, 7, 1, 2, 0, 3 },  // Harmonic Minor
    { 8, 1, 7, 2, 2, 6, 6 },  // Melodic Minor
    { 1, 1, 0, 2, 1, 6, 0 },  // Dorian
    { 1, 0, 2, 1, 6, 0, 1 },  // Phrygian
    { 0, 2, 1, 6, 0, 1, 1 },  // Lydian
    { 2, 1, 6, 0, 1, 1, 0 },  // Mixolydian
    { 6, 0, 1, 1, 0, 2, 1 },  // Locrian
};

// preset bit mask scales for N.B and N.BX
const uint16_t table_n_b[] = {
    0b101011010101,  // ionian (major)
    0b101101010110,  // dorian
    0b110101011010,  // phrygian
    0b101010110101,  // lydian
    0b101011010110,  // mixolydian
    0b101101011010,  // aeolean (natural minor)
    0b110101101010,  // locrian
    0b101101010101,  // melodic minor
    0b101101011001,  // harmonic minor
    0b101010010100,  // major pentatonic
    0b100101010010,  // minor pentatonic
    0b101010101010,  // whole note (1st messiaen mode)
    0b110110110110,  // octatonic (half-whole, 2nd messiaen mode)
    0b101101101101,  // octatonic (whole-half)
    0b101110111011,  // 3rd messiaen mode
    0b111001111001,  // 4th messiaen mode
    0b110001110001,  // 5th messiaen mode
    0b101011101011,  // 6th mesiaen mode
    0b111101111101,  // 7th messiaen mode
    0b100110011001,  // augmented
};
