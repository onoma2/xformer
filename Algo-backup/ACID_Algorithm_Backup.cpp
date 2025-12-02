// ACID Algorithm Backup
//
// This is the ACID algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 14
//
// This algorithm generates 303-style patterns with slides and octave jumps.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
uint8_t _acidSequence[8] = {0};  // 8-step pattern
uint8_t _acidPosition = 0;       // Current position
uint8_t _acidAccentPattern = 0;  // Accent pattern mask
uint8_t _acidOctaveMask = 0;     // Octave mask
int8_t _acidLastNote = 0;        // Last played note
int8_t _acidSlideTarget = 0;     // Target for slide behavior
int8_t _acidStepCount = 0;       // Step counter
*/

// Initialization code for ACID algorithm
void TuesdayTrackEngine::initAlgorithm_ACID() {
    _rng = Random((flow - 1) << 4);
    _extraRng = Random((ornament - 1) << 4);
    
    // Generate 8-step 303-style sequence
    for (int i = 0; i < 8; i++) {
        _acidSequence[i] = _rng.next() % 12;  // 0-11 for chromatic
    }
    
    _acidPosition = 0;
    _acidAccentPattern = _extraRng.next() & 0x55;  // Sparse accents (every other step)
    _acidOctaveMask = _extraRng.next() & 0x33;     // Some octave jumps
    _acidLastNote = _acidSequence[0];
    _acidSlideTarget = 0;
    _acidStepCount = 0;
}

// Buffer generation code for ACID algorithm
void TuesdayTrackEngine::generateBuffer_ACID() {
    // ACID: 303-style patterns with slides and octave jumps
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        // Get current note from sequence
        int seqPos = (_acidPosition + i) % 8;
        uint8_t note = _acidSequence[seqPos];
        
        // Apply octave changes based on mask
        uint8_t octave = 1;  // Default mid-octave for 303
        if ((_acidOctaveMask >> seqPos) & 1) {
            octave = 2;  // Higher octave
        } else if (_acidStepCount % 12 == 0 && _rng.nextBinary()) {
            octave = 0;  // Occasionally drop to lower octave
        }
        
        // Set the step parameters
        step.note = note;
        step.octave = octave;
        
        // Characteristic 303 gate lengths
        if ((_acidAccentPattern >> seqPos) & 1) {
            // Accented steps get longer gates
            step.gatePercent = 100 + _rng.nextRange(40);  // 100-140%
        } else {
            // Unaccented steps get shorter gates
            step.gatePercent = 40 + _rng.nextRange(40);   // 40-80%
        }
        
        // Heavy slide usage for classic 303 sound
        // Slide to 303-style destinations
        if (_rng.nextRange(100) < glide || (abs((int)note - _acidLastNote) <= 3)) {
            step.slide = 1;
        } else {
            step.slide = 0;
        }
        
        // 303-style trills based on user setting
        step.isTrill = (_rng.nextRange(100) < trill) ? true : false;
        
        // Classic 303 timing - slightly on the beat
        step.gateOffset = -3 + _rng.nextRange(7);  // -3% to +3%
        
        // Update tracking variables
        _acidLastNote = note;
        _acidStepCount++;
    }
}

// Tick processing code for ACID algorithm
void TuesdayTrackEngine::tick_ACID() {
    // Update position based on timing
    _acidPosition = (_acidPosition + 1) % 8;
    
    // Occasionally change sequence based on power parameter
    if (_rng.nextRange(100) < power / 2) {
        int changePos = _rng.next() % 8;
        _acidSequence[changePos] = (_acidSequence[changePos] + 1 + _rng.next() % 3) % 12;
    }
    
    // Adapt accent pattern based on user settings
    if (_rng.nextRange(200) < ornament) {
        _acidAccentPattern ^= (1 << _rng.next() % 8);  // Toggle accent
    }
}