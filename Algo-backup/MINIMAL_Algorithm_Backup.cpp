// MINIMAL Algorithm Backup
//
// This is the MINIMAL algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 16
//
// This algorithm generates staccato bursts and silence with sparse rhythmic interest.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
uint8_t _minimalBurstLength = 0;    // 2-8 steps
uint8_t _minimalSilenceLength = 0;  // 4-16 steps
uint8_t _minimalClickDensity = 0;   // 0-255 scale
uint8_t _minimalBurstTimer = 0;     // Burst countdown
uint8_t _minimalSilenceTimer = 0;   // Silence countdown
uint8_t _minimalNoteIndex = 0;      // Current note in burst
uint8_t _minimalMode = 0;           // 0=silence, 1=burst
*/

// Initialization code for MINIMAL algorithm
void TuesdayTrackEngine::initAlgorithm_MINIMAL() {
    _rng = Random((flow - 1) << 4);
    _extraRng = Random((ornament - 1) << 4);
    
    _minimalBurstLength = 2 + (_rng.next() % 7);  // 2-8 steps
    _minimalSilenceLength = 4 + (flow % 13);      // 4-16 steps (based on flow)
    _minimalClickDensity = ornament * 16;         // 0-255 scale (from ornament)
    _minimalBurstTimer = 0;
    _minimalSilenceTimer = _minimalSilenceLength;  // Start in silence
    _minimalNoteIndex = 0;
    _minimalMode = 0;  // 0=silence, 1=burst
}

// Buffer generation code for MINIMAL algorithm
void TuesdayTrackEngine::generateBuffer_MINIMAL() {
    // MINIMAL: staccato bursts and silence
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        // Determine if we're in a burst or silence phase
        if (_minimalSilenceTimer > 0) {
            // We're in silence mode
            step.note = 0;
            step.octave = 0;
            step.gatePercent = 0;  // No gate = silence
            step.slide = 0;
            step.isTrill = false;
            step.gateOffset = 0;
            
            _minimalSilenceTimer--;
            
            if (_minimalSilenceTimer == 0) {
                // Transition to burst mode
                _minimalMode = 1;
                _minimalBurstTimer = _minimalBurstLength;
                _minimalNoteIndex = 0;
            }
        } else if (_minimalBurstTimer > 0) {
            // We're in burst mode
            // Generate sparse, minimal notes
            
            // Determine if this step should have a note based on density
            bool shouldPlay = (_minimalClickDensity > 0) ? 
                             (_rng.nextRange(255) < _minimalClickDensity) : 
                             true;
            
            if (shouldPlay) {
                // Generate minimalistic note pattern
                // Use simple chromatic pattern
                step.note = _minimalNoteIndex % 7;  // 0-6 (chromatic)
                step.octave = (_minimalNoteIndex / 7) % 3;  // 0-2 octaves
                
                // Short staccato gates for minimal feel
                step.gatePercent = 25 + _rng.nextRange(25);  // 25-50%
                
                // Rare slides to connect notes minimally
                step.slide = (_rng.nextRange(100) < glide/5) ? 1 : 0;
                
                // Very rare trills for texture
                step.isTrill = (_rng.nextRange(100) < trill/10) ? true : false;
                
                // Tight timing for minimal precision
                step.gateOffset = -1 + _rng.nextRange(3);  // -1% to +1%
                
                _minimalNoteIndex++;
            } else {
                // Rest in the pattern
                step.note = 0;
                step.octave = 0;
                step.gatePercent = 0;
                step.slide = 0;
                step.isTrill = false;
                step.gateOffset = 0;
            }
            
            _minimalBurstTimer--;
            
            if (_minimalBurstTimer == 0) {
                // Transition to silence mode
                _minimalMode = 0;
                _minimalSilenceTimer = _minimalSilenceLength;
                _minimalNoteIndex = 0;
            }
        } else {
            // Fallback: rest (shouldn't be reached under normal operation)
            step.note = 0;
            step.octave = 0;
            step.gatePercent = 0;
            step.slide = 0;
            step.isTrill = false;
            step.gateOffset = 0;
        }
    }
}

// Tick processing code for MINIMAL algorithm
void TuesdayTrackEngine::tick_MINIMAL() {
    // Minimal algorithm is mostly pattern-driven
    // Update timers on each tick
    
    // Adjust pattern lengths based on power parameter
    if (_rng.nextRange(100) < power/5) {
        // Rarely adjust lengths based on power
        _minimalBurstLength = 2 + (_rng.next() % (power/4 + 1));  // More bursts with higher power
        if (_minimalBurstLength > 8) _minimalBurstLength = 8;
    }
    
    // Update silence length based on ornament
    if (_rng.nextRange(200) < ornament/3) {
        _minimalSilenceLength = 4 + (_rng.next() % (13 - ornament/8));  // Less silence with higher ornament
        if (_minimalSilenceLength < 2) _minimalSilenceLength = 2;
    }
}