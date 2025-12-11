// GOACID Algorithm Backup
//
// This is the GOACID algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 5
//
// This algorithm generates Goa/psytrance-style acid patterns with systematic transposition.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
uint8_t _goaB1 = 0;  // Pattern transpose flag 1
uint8_t _goaB2 = 0;  // Pattern transpose flag 2
*/

// Initialization code for GOACID algorithm
void TuesdayTrackEngine::initAlgorithm_GOACID() {
    // Flow seeds main RNG, Ornament seeds extra RNG
    _rng = Random((flow - 1) << 4);
    _extraRng = Random((ornament - 1) << 4);
    
    _goaB1 = _extraRng.nextBinary() ? 1 : 0;
    _goaB2 = _extraRng.nextBinary() ? 1 : 0;
}

// Buffer generation code for GOACID algorithm
void TuesdayTrackEngine::generateBuffer_GOACID() {
    // GOACID: Goa/psytrance acid patterns with systematic transposition
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        // Generate Goa-style acid patterns
        // Use systematic transposition and octave jumping
        
        // Main pattern: cycle through base notes
        uint8_t baseNote;
        if (i % 3 == 0) {
            baseNote = 0; // C (tonic)
        } else if (i % 3 == 1) {
            baseNote = 7; // G (dominant)
        } else {
            baseNote = 4; // E (mediant)
        }
        
        // Apply transposition based on flags and step
        uint8_t transpose = 0;
        if (_goaB1) {
            // Add transposition based on position (every 4th step)
            transpose += (i % 4 == 0) ? 5 : 0;  // Up a fourth
        }
        
        if (_goaB2) {
            // Add different transposition based on position
            transpose += (i % 6 == 0) ? 7 : 0;  // Up a fifth
        }
        
        // Apply transposition to base note
        uint8_t noteWithTranspose = (baseNote + transpose) % 12;
        
        // Octave decisions: goa often goes up and down in octaves
        uint8_t octave;
        if (i % 8 < 4) {
            octave = 1;  // Lower octave for first half of 8-step pattern
        } else {
            octave = 2;  // Higher octave for second half
        }
        
        // Apply occasional octave jumps for acid feel
        if (_rng.nextRange(10) < 2) {  // 20% chance for different octave
            if (octave == 1) octave = 3;
            else if (octave == 2) octave = 0;
        }
        
        // Set note and octave 
        step.note = noteWithTranspose;
        step.octave = octave;
        
        // Gate settings for acid feel: sometimes longer, sometimes shorter
        if (i % 4 == 0) {
            // Strong beats get longer gates
            step.gatePercent = 80 + _rng.nextRange(40);  // 80-120%
        } else {
            // Weak beats get shorter gates
            step.gatePercent = 40 + _rng.nextRange(50);  // 40-90%
        }
        
        // Acid-like slides: frequent slides between notes
        // Especially common for octave jumps
        if (_rng.nextRange(100) < glide || (i % 8 == 0)) {
            step.slide = 1;
        } else {
            step.slide = 0;
        }
        
        // Characteristic acid trills/retriggers
        if (_rng.nextRange(100) < trill) {
            step.isTrill = true;
        } else {
            step.isTrill = false;
        }
        
        // Timing offset characteristic of acid sequences
        step.gateOffset = -2 + _rng.nextRange(5);  // Small timing variations
    }
}

// Tick processing code for GOACID algorithm
void TuesdayTrackEngine::tick_GOACID() {
    // GOACID mainly operates during buffer generation
    // Could apply real-time transposition changes based on external input
}