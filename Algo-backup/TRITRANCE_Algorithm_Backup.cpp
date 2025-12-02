// TRITRANCE Algorithm Backup
//
// This is the TRITRANCE algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 1
//
// This algorithm generates German minimal-style arpeggios using 3-phase cycling with octave jumps.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
uint8_t _triB1 = 0;  // High note for case 2
uint8_t _triB2 = 0;  // Phase offset
int8_t _triB3 = 0;   // Note offset for octave 0/1
*/

// Initialization code for TRITRANCE algorithm
void TuesdayTrackEngine::initAlgorithm_TRITRANCE() {
    // seed1 (flow) seeds main RNG for b1, b2
    // seed2 (ornament) seeds extra RNG for b3
    _rng = Random((flow - 1) << 4);
    _extraRng = Random((ornament - 1) << 4);

    _triB1 = (_rng.next() & 0x7);   // High note for case 2
    _triB2 = (_rng.next() & 0x7);   // Phase offset for mod 3

    // b3: note offset for octave 0/1
    _triB3 = (_extraRng.next() & 0x15);
    if (_triB3 >= 7) _triB3 -= 7; else _triB3 = 0;
    _triB3 -= 4;  // Range: -4 to +3
}

// Buffer generation code for TRITRANCE algorithm
void TuesdayTrackEngine::generateBuffer_TRITRANCE() {
    // TRITRANCE: Phase-based triadic patterns with octave jumps
    // Use original phase value to determine note
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        int phase = (i + _triB2) % 3;

        switch (phase) {
            case 0:
                // Phase 0: Note offset with octave 0
                step.note = _triB3;
                step.octave = 0;
                step.gatePercent = 50 + (_rng.next() % 37);  // 50-86% gates (tight)
                step.gateOffset = 0 - (5 + _rng.next() % 15); // 10-25% early
                break;
            case 1:
                // Phase 1: Note offset with octave 1
                step.note = _triB3;
                step.octave = 1;
                step.gatePercent = 100 + (_rng.next() % 76); // 100-175% (wide)
                step.gateOffset = 45 + _rng.next() % 11;     // 45-55% (centered)
                break;
            case 2:
                // Phase 2: High note with octave 2
                step.note = _triB1;
                step.octave = 2;
                step.gatePercent = 200 + (_rng.next() % 201); // 200-400% (very wide)
                step.gateOffset = 60 + _rng.next() % 21;     // 60-80% delay
                break;
        }

        // Apply slide based on user setting (glide probability 0-100%)
        step.slide = (_rng.nextRange(100) < glide) ? 1 : 0;
        
        // Apply trill based on user setting (trill probability 0-100%)
        step.isTrill = (_rng.nextRange(100) < trill) ? true : false;
    }
}

// Tick processing code for TRITRANCE algorithm
void TuesdayTrackEngine::tick_TRITRANCE() {
    // In tritrance, the main processing happens in buffer generation
    // Tick processing might handle real-time parameter changes
}