// STOMPER Algorithm Backup
//
// This is the STOMPER algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 2
//
// This algorithm generates acid bass patterns with slide modes and countdowns.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
uint8_t _stomperMode = 0;  // Current pattern mode (0-14)
uint8_t _stomperCountDown = 0;  // Countdown timer
uint8_t _stomperLowNote = 0;  // Low note (0-2)
uint8_t _stomperHighNote[2] = {0, 0};  // High notes (0-6, 0-4)
int16_t _stomperLastNote = 0;  // Last note
uint8_t _stomperLastOctave = 0;  // Last octave
*/

// Enumeration for STOMPER modes
enum StomperMode {
    STOMPER_LOW1 = 0,
    STOMPER_LOW2,
    STOMPER_HIGH1,
    STOMPER_HIGH2,
    STOMPER_SLIDEUP1,
    STOMPER_SLIDEUP2,
    STOMPER_SLIDEDOWN1,
    STOMPER_SLIDEDOWN2,
    STOMPER_PAUSE1,
    STOMPER_PAUSE2,
    STOMPER_HILOW1,
    STOMPER_HILOW2,
    STOMPER_LOWHI1,
    STOMPER_LOWHI2,
    STOMPER_MAKENEW = 14  // Value that triggers new pattern
};

// Initialization code for STOMPER algorithm
void TuesdayTrackEngine::initAlgorithm_STOMPER() {
    // seed2 (ornament) seeds main RNG for note choices
    // seed1 (flow) seeds extra RNG for mode/pattern
    _rng = Random((ornament - 1) << 4);
    _extraRng = Random((flow - 1) << 4);

    _stomperMode = (_extraRng.next() % 7) * 2;  // Initial pattern mode (0,2,4,6,8,10,12)
    _stomperCountDown = 0;
    _stomperLowNote = _rng.next() % 3;
    _stomperLastNote = _stomperLowNote;
    _stomperLastOctave = 0;
    _stomperHighNote[0] = _rng.next() % 7;  // 0-6
    _stomperHighNote[1] = _rng.next() % 5;  // 0-4
}

// Buffer generation code for STOMPER algorithm
void TuesdayTrackEngine::generateBuffer_STOMPER() {
    // STOMPER: Acid bass patterns with slides and state machine
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        if (_stomperCountDown > 0) {
            // Still in rest period
            step.gatePercent = _stomperCountDown * 25;  // Shorter countdown = shorter gate
            _stomperCountDown--;
            step.note = _stomperLastNote;
            step.octave = _stomperLastOctave;
        } else {
            // Check if we need to make new pattern
            if (_stomperMode >= STOMPER_MAKENEW) {
                // Occasionally change low note (20% chance)
                if (_rng.nextRange(5) == 0) {
                    _stomperLowNote = _rng.next() % 3;
                }
                
                // Change high note 0 (50% chance)
                if (_rng.nextBinary()) {
                    _stomperHighNote[0] = _rng.next() % 7;
                }
                
                // Change high note 1 (30% chance)
                if (_rng.nextRange(10) < 3) {
                    _stomperHighNote[1] = _rng.next() % 5;
                }
                
                _stomperMode = 0;  // Reset to first mode
            }
            
            // Execute current mode
            switch (_stomperMode) {
                case STOMPER_LOW1:
                    step.note = _stomperLowNote;
                    step.octave = 0;
                    step.gatePercent = 75;
                    step.slide = 0;
                    step.isTrill = false;
                    break;
                    
                case STOMPER_LOW2:
                    step.note = _stomperLowNote;
                    step.octave = 1;
                    step.gatePercent = 75;
                    step.slide = 0;
                    step.isTrill = false;
                    break;
                    
                case STOMPER_HIGH1:
                    step.note = _stomperHighNote[0];
                    step.octave = 0;
                    step.gatePercent = 60;
                    step.slide = 0;
                    step.isTrill = false;
                    break;
                    
                case STOMPER_HIGH2:
                    step.note = _stomperHighNote[1];
                    step.octave = 1;
                    step.gatePercent = 60;
                    step.slide = 0;
                    step.isTrill = false;
                    break;
                    
                case STOMPER_SLIDEUP1:
                    step.note = _stomperLowNote;
                    step.octave = 0;
                    step.gatePercent = 80;
                    step.slide = 1;  // Apply slide
                    step.isTrill = false;
                    break;
                    
                case STOMPER_SLIDEUP2:
                    step.note = _stomperLowNote;
                    step.octave = 1;
                    step.gatePercent = 80;
                    step.slide = 1;  // Apply slide
                    step.isTrill = false;
                    break;
                    
                case STOMPER_SLIDEDOWN1:
                    step.note = _stomperHighNote[0];
                    step.octave = 0;
                    step.gatePercent = 80;
                    step.slide = 1;  // Apply slide
                    step.isTrill = false;
                    break;
                    
                case STOMPER_SLIDEDOWN2:
                    step.note = _stomperHighNote[1];
                    step.octave = 1;
                    step.gatePercent = 80;
                    step.slide = 1;  // Apply slide
                    step.isTrill = false;
                    break;
                    
                case STOMPER_PAUSE1:
                    step.note = _stomperLowNote;
                    step.octave = 0;
                    step.gatePercent = 0;  // Gate off
                    step.slide = 0;
                    step.isTrill = false;
                    _stomperCountDown = 2;  // 2-step rest
                    break;
                    
                case STOMPER_PAUSE2:
                    step.note = _stomperHighNote[0];
                    step.octave = 1;
                    step.gatePercent = 0;  // Gate off
                    step.slide = 0;
                    step.isTrill = false;
                    _stomperCountDown = 3;  // 3-step rest
                    break;
                    
                case STOMPER_HILOW1:
                    step.note = (i & 1) ? _stomperHighNote[0] : _stomperLowNote;
                    step.octave = (i & 1) ? 1 : 0;
                    step.gatePercent = 70;
                    step.slide = 0;
                    step.isTrill = false;
                    break;
                    
                case STOMPER_HILOW2:
                    step.note = (i & 1) ? _stomperHighNote[1] : _stomperLowNote;
                    step.octave = (i & 1) ? 1 : 0;
                    step.gatePercent = 70;
                    step.slide = 0;
                    step.isTrill = false;
                    break;
                    
                case STOMPER_LOWHI1:
                    step.note = (i & 1) ? _stomperLowNote : _stomperHighNote[0];
                    step.octave = (i & 1) ? 0 : 1;
                    step.gatePercent = 70;
                    step.slide = 0;
                    step.isTrill = false;
                    break;
                    
                case STOMPER_LOWHI2:
                    step.note = (i & 1) ? _stomperLowNote : _stomperHighNote[1];
                    step.octave = (i & 1) ? 0 : 1;
                    step.gatePercent = 70;
                    step.slide = 0;
                    step.isTrill = false;
                    break;
            }
            
            _stomperLastNote = step.note;
            _stomperLastOctave = step.octave;
            
            // Advance to next mode
            _stomperMode++;
        }
    }
}

// Tick processing code for STOMPER algorithm
void TuesdayTrackEngine::tick_STOMPER() {
    // STOMPER mainly operates during buffer generation
    // Could apply real-time adjustments to the state machine if needed
}