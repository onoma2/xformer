// RAGA Algorithm Backup
//
// This is the RAGA algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 12
//
// This algorithm generates Indian classical-style melodies with traditional scales and ornamental patterns.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
uint8_t _ragaScale[7] = {0};    // 7-note raga scale
uint8_t _ragaDirection = 0;    // 0=ascending, 1=descending
uint8_t _ragaPosition = 0;     // Current position in scale
uint8_t _ragaOrnament = 0;     // 0=none, 1=meends, 2=taans, 3=murki
*/

// Initialization code for RAGA algorithm
void TuesdayTrackEngine::initAlgorithm_RAGA() {
    // Flow seeds main RNG, Ornament seeds extra RNG
    _rng = Random((flow - 1) << 4);
    _extraRng = Random((ornament - 1) << 4);
    
    // Select Indian classical scale based on seed
    int scaleType = _rng.next() % 4;
    
    switch (scaleType) {
        case 0: // Bhairav-like (morning raga)
            _ragaScale[0] = 0; _ragaScale[1] = 1; _ragaScale[2] = 4;  // sa ri# ga ma
            _ragaScale[3] = 5; _ragaScale[4] = 7; _ragaScale[5] = 8;  // pa dha ni#
            _ragaScale[6] = 11; // sa (octave)
            break;
        case 1: // Yaman-like (evening raga)
            _ragaScale[0] = 0; _ragaScale[1] = 2; _ragaScale[2] = 4;  // sa ri ga# ma
            _ragaScale[3] = 6; _ragaScale[4] = 7; _ragaScale[5] = 9;  // pa dha# ni
            _ragaScale[6] = 11; // sa (octave)
            break;
        case 2: // Todi-like
            _ragaScale[0] = 0; _ragaScale[1] = 1; _ragaScale[2] = 3;  // sa ri# ge ma
            _ragaScale[3] = 6; _ragaScale[4] = 7; _ragaScale[5] = 8;  // pa dha ni#
            _ragaScale[6] = 11; // sa (octave)
            break;
        default: // Kafi-like (Dorian)
            _ragaScale[0] = 0; _ragaScale[1] = 2; _ragaScale[2] = 3;  // sa ri ge ma
            _ragaScale[3] = 5; _ragaScale[4] = 7; _ragaScale[5] = 9;  // pa dha ni
            _ragaScale[6] = 10; // sa (seventh)
            break;
    }
    
    _ragaDirection = 0;  // 0=ascending, 1=descending normally
    _ragaPosition = 0;   // Start at root
    _ragaOrnament = _extraRng.next() % 4;  // 0=none, 1=meends, 2=taans, 3=murki
}

// Buffer generation code for RAGA algorithm
void TuesdayTrackEngine::generateBuffer_RAGA() {
    // RAGA: Indian classical melodies with traditional scales
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        // Determine note based on current position in raga scale
        uint8_t note = _ragaScale[_ragaPosition];
        
        // Determine octave based on position (go up octave after halfway)
        uint8_t octave = 1;  // Default to middle octave
        if (_ragaPosition > 4) {
            octave = 2;  // Higher octave for upper tetrachord
        } else if (_ragaPosition == 0 && i > 0 && _ragaPosition == 0) {
            // When returning to root, occasionally drop an octave
            octave = _rng.nextBinary() ? 0 : 1;
        }
        
        // Set the basic parameters
        step.note = note;
        step.octave = octave;
        
        // Indian classical music has varied gate lengths based on note importance
        // Sa (root) and Pa (fifth) are stronger, others variable
        if (note == _ragaScale[0] || note == _ragaScale[4]) {  // Sa or Pa
            step.gatePercent = 85 + _rng.next() % 15;  // 85-100% (strong notes)
        } else if (note == _ragaScale[2] || note == _ragaScale[1]) {  // Ga or Ri
            step.gatePercent = 70 + _rng.next() % 20;  // 70-90% (medium notes)
        } else {
            step.gatePercent = 50 + _rng.next() % 40;  // 50-90% (variable)
        }
        
        // Apply ornamentations based on raga tradition
        switch (_ragaOrnament) {
            case 0: // No ornamentation
                step.slide = 0;
                step.isTrill = false;
                break;
            case 1: // Meends (glides)
                // Apply slides to connect notes melodically
                step.slide = (_rng.nextRange(100) < glide) ? 1 : 0;
                step.isTrill = false;
                break;
            case 2: // Taans (fast melodic passages)
                // Add possibility of quick runs/trills
                step.slide = (_rng.nextRange(100) < glide/2) ? 1 : 0;  // Fewer slides
                step.isTrill = (_rng.nextRange(100) < trill) ? true : false;  // More trills
                break;
            case 3: // Murki (grace notes)
                // Quick grace notes around main note
                step.slide = 0;  // No slides for murki
                step.isTrill = (_rng.nextRange(100) < trill) ? true : false;  // Trills for grace notes
                break;
        }
        
        // Advance position in raga based on traditional movements
        // Raga phrases have specific patterns (achieved through conditional logic)
        if (_rng.nextRange(10) == 0) {  // Occasionally change direction
            _ragaDirection = !_ragaDirection;  // Flip direction
        }
        
        // Traditional raga movements:
        if (_ragaDirection == 0) {  // Ascending
            // In tradition, don't jump from Ri directly to Pa, go Ri-Ga-Ma-Pa
            if (_ragaPosition == 1) {  // At Ri
                _ragaPosition = 2;  // Go to Ga instead of jumping to Pa
            } else if (_ragaPosition < 6) {  // Not at the top
                _ragaPosition++;
            } else {  // At the top (Sa octave)
                if (_rng.nextBinary()) {  // Sometimes descend
                    _ragaPosition--;
                    _ragaDirection = 1;  // Start descending
                } else {  // Stay or go to higher octave
                    _ragaPosition = 0;  // Jump down an octave to root
                }
            }
        } else {  // Descending
            // In tradition, don't jump from Dha directly to Ri, go Dha-Pa-Ma-Ga-Ri
            if (_ragaPosition == 5) {  // At Dha
                _ragaPosition = 4;  // Go to Pa instead of jumping to Ri
            } else if (_ragaPosition > 0) {  // Not at root
                _ragaPosition--;
            } else {  // At root
                if (_rng.nextBinary()) {  // Sometimes ascend
                    _ragaPosition++;
                    _ragaDirection = 0;  // Start ascending
                } else {  // Stay at root or go to lower octave
                    _ragaPosition = 6;  // Jump up an octave to upper Sa
                    _ragaDirection = 0;  // Start ascending from top
                }
            }
        }
        
        // Indian music timing is often flexible - apply gamak (oscillation) timing
        step.gateOffset = -10 + _rng.next() % 20;  // Flexible timing (-10% to +10%)
        
        // Apply characteristic microtonal bends/oscillations in Indian music
        if (_ragaOrnament > 0 && _rng.nextRange(100) < power) {
            // Power affects frequency of ornamental gestures
            step.gateOffset += -5 + _rng.next() % 10;  // Extra timing flexibility
        }
    }
}

// Tick processing code for RAGA algorithm
void TuesdayTrackEngine::tick_RAGA() {
    // RAGA may evolve over time, introducing vadi (main) and samvadi (secondary) notes emphasis
    // This could be tied to longer-term rhythmic cycles
    if (_stepIndex % 16 == 0) {  // Every 16 steps, possibly emphasize different notes
        // Adjust emphasis based on power/intensity
        if (_rng.nextRange(100) < power) {
            // Occasionally change emphasized notes based on raga grammar
            _ragaOrnament = (_ragaOrnament + 1) % 4; // Cycle through ornamentation styles
        }
    }
}