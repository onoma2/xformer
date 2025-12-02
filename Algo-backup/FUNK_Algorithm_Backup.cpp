// FUNK Algorithm Backup
//
// This is the FUNK algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 9
//
// This algorithm generates syncopated funk grooves with ghost notes and off-beat rhythms.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
uint8_t _funkPattern = 0;        // 8 funk patterns
uint8_t _funkSyncopation = 0;    // 0-3 syncopation level
uint8_t _funkGhostProb = 0;      // 32-96 ghost note probability
*/

// Initialization code for FUNK algorithm
void TuesdayTrackEngine::initAlgorithm_FUNK() {
    // Flow seeds main RNG, Ornament seeds extra RNG
    _rng = Random((flow - 1) << 4);
    _extraRng = Random((ornament - 1) << 4);
    
    _funkPattern = _rng.next() % 8;  // 8 funk patterns
    _funkSyncopation = _extraRng.next() % 4;  // 0-3 syncopation level
    _funkGhostProb = 32 + (_extraRng.next() % 64);  // 32-96 ghost note probability
}

// Buffer generation code for FUNK algorithm
void TuesdayTrackEngine::generateBuffer_FUNK() {
    // FUNK: Syncopated grooves with ghost notes
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        // Calculate beat position within measure
        // Funk often uses 16th note divisions for syncopation
        int beatPos = i % 16;
        
        // Determine the base pattern based on selected funk pattern
        bool isStrongBeat = false;
        bool isWeakBeat = false;
        bool isGhostNote = false;
        
        // Apply selected funk pattern
        switch (_funkPattern) {
            case 0: // Classic funk bassline
                isStrongBeat = (beatPos == 0 || beatPos == 4 || beatPos == 8 || beatPos == 12);
                isWeakBeat = (beatPos == 6 || beatPos == 14);
                break;
            case 1: // James Brown style
                isStrongBeat = (beatPos == 0 || beatPos == 8);
                isWeakBeat = (beatPos == 4 || beatPos == 10 || beatPos == 12);
                break;
            case 2: // Syncopated guitar
                isStrongBeat = (beatPos == 2 || beatPos == 6 || beatPos == 10 || beatPos == 14);
                isWeakBeat = (beatPos == 0 || beatPos == 8);
                break;
            case 3: // Bootsy Collins style
                isStrongBeat = (beatPos == 0 || beatPos == 4 || beatPos == 8);
                isWeakBeat = (beatPos == 3 || beatPos == 7 || beatPos == 11 || beatPos == 15);
                break;
            case 4: // Strutting bass
                isStrongBeat = (beatPos == 0 || beatPos == 6 || beatPos == 12);
                isWeakBeat = (beatPos == 4 || beatPos == 10);
                break;
            case 5: // Rhythm guitar
                isStrongBeat = (beatPos == 1 || beatPos == 5 || beatPos == 9 || beatPos == 13);
                isWeakBeat = (beatPos == 0 || beatPos == 4 || beatPos == 8 || beatPos == 12);
                break;
            case 6: // Off-beat stab
                isStrongBeat = (beatPos == 3 || beatPos == 7 || beatPos == 11 || beatPos == 15);
                break;
            case 7: // Complex polyrhythm
                isStrongBeat = (beatPos % 5 == 0);
                isWeakBeat = (beatPos % 3 == 0 && beatPos % 5 != 0);
                break;
        }
        
        // Apply syncopation level to vary the pattern
        if (_funkSyncopation > 0 && isStrongBeat) {
            // Add displacement for syncopation
            if ((beatPos + _funkSyncopation) % 8 == 4) {
                // Shift some strong beats slightly
                isStrongBeat = false;
                isWeakBeat = true;
            }
        }
        
        // Determine if this should be a ghost note (quiet, off-beat)
        if (_rng.nextRange(100) < _funkGhostProb) {
            isGhostNote = !isStrongBeat && !isWeakBeat && (_rng.nextBinary() || beatPos % 2 == 1);
        }
        
        // Assign notes based on pattern type
        if (isStrongBeat) {
            // Strong beats - use root notes
            step.note = 0;  // C (root)
            step.octave = 1;  // Medium-low octave for funk bass
            
            // Longer gate for strong hits
            step.gatePercent = 85 + _rng.next() % 15;  // 85-100%
            
            // Sometimes add slide on strong notes for funk feel
            step.slide = (_rng.nextRange(100) < glide) ? 1 : 0;
            
            // No trill on strong beats
            step.isTrill = false;
        }
        else if (isWeakBeat) {
            // Weak beats - use harmony notes (thirds, fifths, etc.)
            int harmonyNote = (beatPos % 4 == 2) ? 4 : 7;  // Third or fifth
            step.note = harmonyNote;
            step.octave = 1;  // Same octave for cohesion
            
            // Medium gate
            step.gatePercent = 65 + _rng.next() % 20;  // 65-85%
            
            // Less likely to slide on weak beats
            step.slide = (_rng.nextRange(100) < glide/2) ? 1 : 0;
            
            // Maybe add trill for rhythmic interest
            step.isTrill = (_rng.nextRange(100) < trill) ? true : false;
        }
        else if (isGhostNote) {
            // Ghost notes - soft, off-beat notes
            step.note = 2 + (_rng.next() % 3);  // Second, third, or fourth (2-4)
            step.octave = 1;
            
            // Short gate for ghost notes
            step.gatePercent = 20 + _rng.next() % 25;  // 20-45%
            
            step.slide = 0;  // No slide for ghost notes
            
            step.isTrill = false;  // Generally no trill for ghost notes
        }
        else {
            // Rest or fill - decide based on funk probability
            if (_rng.nextRange(100) < 20) {  // 20% chance for fill notes
                // Fill with passing tone or chord tone
                step.note = 5;  // Fourth (passing tone) 
                step.octave = 1;
                
                step.gatePercent = 45 + _rng.next() % 20;  // 45-65%
                
                step.slide = (_rng.nextRange(100) < glide/3) ? 1 : 0;  // Rarely slide
                
                step.isTrill = false;
            } else {
                // Rest
                step.note = 0;
                step.octave = 0;
                step.gatePercent = 0;  // Gate off
                step.slide = 0;
                step.isTrill = false;
            }
        }
        
        // Funk timing is often slightly ahead of or behind the beat
        if (isStrongBeat) {
            step.gateOffset = -10 + (_rng.next() % 10);  // Slightly early
        } else if (isWeakBeat) {
            step.gateOffset = 0 + (_rng.next() % 15);  // Slightly late
        } else if (isGhostNote) {
            step.gateOffset = -5 + (_rng.next() % 10);  // Variable timing
        } else {
            step.gateOffset = -2 + (_rng.next() % 5);   // Neutral timing
        }
    }
}

// Tick processing code for FUNK algorithm
void TuesdayTrackEngine::tick_FUNK() {
    // Funk algorithms might evolve patterns over time
    // Or respond to real-time input for feel adjustments
}