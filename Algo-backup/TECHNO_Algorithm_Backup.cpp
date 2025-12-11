// TECHNO Algorithm Backup
//
// This is the TECHNO algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 8
//
// This algorithm generates four-on-the-floor club patterns with hi-hat variations.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
uint8_t _technoKickPattern = 0;  // 4 kick variations (0-3)
uint8_t _technoHatPattern = 0;   // 4 hat variations (0-3)
uint8_t _technoBassNote = 0;     // Bass note (0-4)
*/

// Initialization code for TECHNO algorithm
void TuesdayTrackEngine::initAlgorithm_TECHNO() {
    // Flow seeds main RNG, Ornament seeds extra RNG
    _rng = Random((flow - 1) << 4);
    _extraRng = Random((ornament - 1) << 4);
    
    _technoKickPattern = _rng.next() % 4;  // 4 kick variations
    _technoHatPattern = _extraRng.next() % 4;  // 4 hat variations
    _technoBassNote = _rng.next() % 5;  // Bass note 0-4
}

// Buffer generation code for TECHNO algorithm
void TuesdayTrackEngine::generateBuffer_TECHNO() {
    // TECHNO: Four-on-floor club patterns with kick and hi-hat
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        // Determine if this is a kick step (usually on beat)
        bool isKickStep = (i % 4 == 0);  // Standard 4/4 kick pattern
        
        // Kick variations based on pattern
        if (isKickStep) {
            step.note = _technoBassNote;  // Use assigned bass note
            step.octave = 0;  // Low octave for kick
            
            // Gate length for kick - typically longer than hats
            step.gatePercent = 95;  // Strong kick sound
            
            // No slides for kicks (they're percussive)
            step.slide = 0;
            
            // No trills on kick (wouldn't sound right)
            step.isTrill = false;
        } 
        // Hi-hat patterns based on selected pattern
        else if ((i % 2 == 1 && _technoHatPattern < 2) ||  // Pattern 0-1: every off-beat
                 (i % 4 == 2 && _technoHatPattern == 2) ||  // Pattern 2: only middle
                 (_technoHatPattern == 3)) {                // Pattern 3: all non-kick
            // Hi-hat pattern
            step.note = 11;  // B (high note for hi-hat)
            step.octave = 2;  // High octave for hi-hat
            
            // Shorter gate for hi-hat
            step.gatePercent = 30 + (_rng.next() % 40);  // 30-70% (short)
            
            // No slides for hi-hats
            step.slide = 0;
            
            // Maybe add trill based on setting
            step.isTrill = (_rng.nextRange(100) < trill) ? true : false;
        }
        else {
            // Snare or other percussion, or just rest
            // Based on kick pattern variation, add snares on different beats
            bool isSnareStep = false;
            switch (_technoKickPattern) {
                case 0: isSnareStep = (i % 4 == 2); break;  // Standard snare on 2
                case 1: isSnareStep = (i % 8 == 6); break;  // Snare every 2.5 beats
                case 2: isSnareStep = (i % 4 == 2 || i % 3 == 0); break;  // Complex pattern
                case 3: isSnareStep = (i % 4 == 2) && (_rng.nextBinary()); break;  // Randomized
            }
            
            if (isSnareStep) {
                step.note = 9;  // A (for snare sound)
                step.octave = 1;  // Mid-high octave
                
                step.gatePercent = 50;  // Medium gate for snare
                
                step.slide = 0;
                
                step.isTrill = false;  // No trill on snare
            } else {
                // Rest or bass fill - fill in gaps with bass notes
                if (_technoKickPattern > 1 && i % 3 == 0) {
                    step.note = (_technoBassNote + 7) % 12;  // Fifth above bass
                    step.octave = 1;  // Mid octave
                    
                    step.gatePercent = 60;  // Medium gate
                    
                    step.slide = (_rng.nextRange(100) < glide) ? 1 : 0;
                    
                    step.isTrill = false;
                } else {
                    // Rest
                    step.note = 0;
                    step.octave = 0;
                    step.gatePercent = 0;  // Gate off for rest
                    step.slide = 0;
                    step.isTrill = false;
                }
            }
        }
        
        // Apply timing variation based on pattern
        if (isKickStep) {
            step.gateOffset = -2 + _rng.next() % 5;  // Tight kick timing
        } else {
            step.gateOffset = -5 + _rng.next() % 11;  // Slightly looser timing for others
        }
    }
}

// Tick processing code for TECHNO algorithm
void TuesdayTrackEngine::tick_TECHNO() {
    // TECHNO is mostly preset patterns, but could vary patterns over time
    // Or respond to real-time parameter changes
    if (_rng.nextRange(100) < power) {  // Vary based on power setting
        // Potentially switch to different pattern after a full measure
    }
}