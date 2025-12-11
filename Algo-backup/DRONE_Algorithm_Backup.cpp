// DRONE Algorithm Backup
//
// This is the DRONE algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 10
//
// This algorithm generates sustained drone textures with minimal movement for ambient/pad sounds.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
uint8_t _droneBaseNote = 0;     // Base note of drone
uint8_t _droneInterval = 0;     // 0=unison, 1=5th, 2=octave, 3=5th+octave
uint8_t _droneSpeed = 1;        // Change rate for evolution (1-4)
*/

// Initialization code for DRONE algorithm
void TuesdayTrackEngine::initAlgorithm_DRONE() {
    // Flow seeds main RNG, Ornament seeds extra RNG
    _rng = Random((flow - 1) << 4);
    _extraRng = Random((ornament - 1) << 4);
    
    _droneBaseNote = _rng.next() % 12;  // Root note
    _droneInterval = _extraRng.next() % 4;  // 0=unison, 1=5th, 2=octave, 3=5th+octave
    _droneSpeed = 1 + (_rng.next() % 4);  // Change rate 1-4 (avoid zero)
}

// Buffer generation code for DRONE algorithm
void TuesdayTrackEngine::generateBuffer_DRONE() {
    // DRONE: Sustained drone textures that evolve slowly
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        // Calculate base note for drone
        uint8_t note = _droneBaseNote;
        uint8_t octave = 0;  // Low for drones
        
        // Add harmonic intervals based on drone interval selection
        switch (_droneInterval) {
            case 0: // unison - just the root
                note = _droneBaseNote;
                octave = 0;  // Low for sustained drone
                break;
            case 1: // 5th - add perfect fifth
                if (i % _droneSpeed == 0) {
                    // Slowly evolve the drone by adding fifth occasionally
                    note = (_droneBaseNote + 7) % 12;  // Perfect fifth
                    octave = 1;  // Middle octave
                } else {
                    note = _droneBaseNote;
                    octave = 0;  // Base octave
                }
                break;
            case 2: // octave - root and octave up
                if (i % (_droneSpeed * 2) < _droneSpeed) {
                    note = _droneBaseNote;
                    octave = 0;  // Lower octave
                } else {
                    note = _droneBaseNote;
                    octave = 1;  // Higher octave
                }
                break;
            case 3: // 5th+octave - combination of both
                if (i % (_droneSpeed * 3) < _droneSpeed) {
                    note = _droneBaseNote;
                    octave = 0;  // Root
                } else if (i % (_droneSpeed * 3) < (_droneSpeed * 2)) {
                    note = (_droneBaseNote + 7) % 12;  // Fifth
                    octave = 1;  // Middle
                } else {
                    note = _droneBaseNote;
                    octave = 2;  // Octave up
                }
                break;
        }
        
        // For drone, we want sustained notes
        step.note = note;
        step.octave = octave;
        
        // Gates should be long for drone effect
        step.gatePercent = 95 + (_rng.next() % 5);  // 95-100% sustained
        
        // Very rarely add slides for subtle movement
        step.slide = (_rng.nextRange(100) < glide/10) ? 1 : 0;
        
        // Drastically reduce trills for drone - should be more peaceful
        step.isTrill = (_rng.nextRange(100) < trill/10) ? true : false;
        
        // Minimal timing variation for stable drone feel
        step.gateOffset = -2 + (_rng.next() % 5);  // Very minimal variation
    }
}

// Tick processing code for DRONE algorithm
void TuesdayTrackEngine::tick_DRONE() {
    // In drone mode, slowly evolve the note pattern over time
    // Maybe change interval or base note very slowly
    if (_stepIndex % (200 * _droneSpeed) == 0) {  // Slow evolution
        // Slowly adjust drone characteristics
        if (_rng.nextRange(20) == 0) {  // Once in a while
            _droneInterval = (_droneInterval + 1) % 4;
        }
        if (_rng.nextRange(50) == 0) {  // Even more rarely
            _droneBaseNote = (_droneBaseNote + (_rng.next() % 3) - 1) % 12;  // Change by -1, 0, or +1 semitones
        }
    }
}