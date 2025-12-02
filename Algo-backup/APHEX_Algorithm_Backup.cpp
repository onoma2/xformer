// APHEX Algorithm Backup
//
// This is the APHEX algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 18
//
// This algorithm generates complex polyrhythmic patterns from three independent tracks.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
uint8_t _aphex_track1_pattern[4]; // 4-step melodic pattern
uint8_t _aphex_track2_pattern[3]; // 3-step modifier pattern (0=off, 1=stutter, 2=slide)
uint8_t _aphex_track3_pattern[5]; // 5-step bass/override pattern
uint8_t _aphex_pos1, _aphex_pos2, _aphex_pos3; // Current positions
*/

// Initialization code for APHEX algorithm
void TuesdayTrackEngine::initAlgorithm_APHEX() {
    _rng = Random((flow - 1) << 4);
    _extraRng = Random((ornament - 1) << 4);
    
    // Generate 4-step melodic pattern
    for (int i = 0; i < 4; i++) {
        _aphex_track1_pattern[i] = _rng.next() % 12;  // 0-11 for chromatic
    }
    // Generate 3-step modifier pattern (0=off, 1=stutter, 2=slide)
    for (int i = 0; i < 3; i++) {
        _aphex_track2_pattern[i] = _rng.next() % 3;  // 0, 1, or 2
    }
    // Generate 5-step bass/override pattern
    for (int i = 0; i < 5; i++) {
        _aphex_track3_pattern[i] = _rng.next() % 5;  // 0-4, sparse
    }
    
    _aphex_pos1 = 0;
    _aphex_pos2 = 0;
    _aphex_pos3 = 0;
}

// Buffer generation code for APHEX algorithm
void TuesdayTrackEngine::generateBuffer_APHEX() {
    // APHEX: Polyrhythmic Event Sequencer
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        // 1. Get note from track 1 at current position
        int pos1 = (_aphex_pos1 + i) % 4;
        uint8_t baseNote = _aphex_track1_pattern[pos1];
        
        // 2. Apply modifier from track 2 (0=off, 1=stutter, 2=slide)
        int pos2 = (_aphex_pos2 + i) % 3;
        uint8_t modifier = _aphex_track2_pattern[pos2];
        
        // 3. Apply override from track 3 (0=off, 1-4=bass note override)
        int pos3 = (_aphex_pos3 + i) % 5;
        uint8_t overrideNote = _aphex_track3_pattern[pos3];
        
        // Determine final note value
        if (overrideNote > 0) {
            // Override with bass note
            step.note = (overrideNote + baseNote) % 12;  // Combine for more complex harmony
            step.octave = 0;  // Lower octave for bass
        } else {
            // Use base note from track 1
            step.note = baseNote;
            // Determine octave based on polyrhythmic calculation
            int polyCalc = ((pos1 * 7) ^ (pos2 * 5) ^ (pos3 * 3)) % 4;
            step.octave = polyCalc % 3;  // 0, 1, or 2
        }
        
        // Set gate length based on modifier
        switch (modifier) {
            case 0: // Off - shorter gate
                step.gatePercent = 40 + (_rng.next() % 30);  // 40-70%
                break;
            case 1: // Stutter - very short gate
                step.gatePercent = 10 + (_rng.next() % 20);  // 10-30%
                break;
            case 2: // Slide - longer gate
                step.gatePercent = 70 + (_rng.next() % 40);  // 70-110%
                break;
        }
        
        // Apply slide based on modifier and glide setting
        if (modifier == 2 || (_rng.nextRange(100) < glide)) {
            step.slide = 1;
        } else {
            step.slide = 0;
        }
        
        // Apply trill based on complex polyrhythmic calculation
        int polyCollision = ((pos1 * 7) ^ (pos2 * 5) ^ (pos3 * 3)) % 12;
        step.isTrill = (polyCollision > 8 && _rng.nextRange(100) < trill) ? true : false;
        
        // Set gate offset based on polyrhythm factor
        int polyRhythmFactor = (pos1 + pos2 + pos3) % 12;
        step.gateOffset = polyRhythmFactor * 8;  // Variable offset based on position
        
        // Update positions (they advance at different rates)
        if (i % 1 == 0) _aphex_pos1 = (_aphex_pos1 + 1) % 4;  // Track 1: every step
        if (i % 1 == 0) _aphex_pos2 = (_aphex_pos2 + 1) % 3;  // Track 2: every step  
        if (i % 1 == 0) _aphex_pos3 = (_aphex_pos3 + 1) % 5;  // Track 3: every step
    }
}

// Tick processing code for APHEX algorithm
void TuesdayTrackEngine::tick_APHEX() {
    // In APHEX, advance all positions in sync but they wrap at different rates
    _aphex_pos1 = (_aphex_pos1 + 1) % 4;
    _aphex_pos2 = (_aphex_pos2 + 1) % 3;
    _aphex_pos3 = (_aphex_pos3 + 1) % 5;
    
    // Occasionally modify patterns based on flow and ornament parameters
    if (_rng.nextRange(100) < flow / 2) {
        // Modify track 1 pattern based on flow
        int modifyPos = _rng.next() % 4;
        _aphex_track1_pattern[modifyPos] = (_aphex_track1_pattern[modifyPos] + 1 + _rng.next() % 3) % 12;
    }
    
    if (_rng.nextRange(100) < ornament / 3) {
        // Modify track 2 pattern based on ornament
        int modifyPos = _rng.next() % 3;
        _aphex_track2_pattern[modifyPos] = _rng.next() % 3;  // 0, 1, or 2
    }
}