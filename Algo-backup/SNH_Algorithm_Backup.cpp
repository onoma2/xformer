// SNH Algorithm Backup
//
// This is the SNH (Sample & Hold) algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 6
//
// This algorithm generates random voltage sequences similar to analog sample & hold circuits.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
uint32_t _snhPhase = 0;
uint32_t _snhPhaseSpeed = 0;
uint8_t _snhLastVal = 0;
int32_t _snhTarget = 0;
int32_t _snhCurrent = 0;
int32_t _snhCurrentDelta = 0;
*/

// Initialization code for SNH algorithm
void TuesdayTrackEngine::initAlgorithm_SNH() {
    // Flow seeds main RNG, Ornament seeds extra RNG
    _rng = Random((flow - 1) << 4);
    _extraRng = Random((ornament - 1) << 4);
    
    _snhPhase = 0;
    _snhPhaseSpeed = 0xffffffff / 16;  // Default speed based on 16 steps
    _snhLastVal = 0;
    _snhTarget = _rng.next() << 10;
    _snhCurrent = _snhCurrent = _rng.next() << 10;
    _snhCurrentDelta = 0;
}

// Buffer generation code for SNH algorithm
void TuesdayTrackEngine::generateBuffer_SNH() {
    // SNH: Sample & Hold - generates random voltages with smooth transitions
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        // Calculate phase-based random value
        uint32_t phase = _snhPhase + i * _snhPhaseSpeed;
        
        // Generate new target at specific phase points (random-like)
        if (phase > 0x80000000) { // Arbitrary threshold to trigger new value
            _snhTarget = _rng.next() << 10;
            _snhPhase = 0;  // Reset phase
        }
        
        // Calculate step toward target
        int32_t delta = (_snhTarget - _snhCurrent) / (power + 1); // Power affects smoothness
        _snhCurrent += delta;
        
        // Determine note and octave from current value
        // Map the 32-bit value to note and octave
        uint8_t note = (_snhCurrent >> 18) & 0x0F;  // Use upper bits for note (0-15)
        uint8_t octave = (_snhCurrent >> 22) & 0x03;  // Use higher bits for octave (0-3)
        
        // Ensure note stays in range
        note = note % 12;
        
        // Set the step parameters
        step.note = note;
        step.octave = octave;
        
        // Gate percentage for S&H - tends to be medium to allow for transitions
        step.gatePercent = 60 + (_rng.next() % 30);  // 60-90%
        
        // Apply slide to mimic analog S&H behavior
        if (_rng.nextRange(100) < glide) {
            step.slide = 1;
        } else {
            step.slide = 0;
        }
        
        // Apply trill based on user setting
        step.isTrill = (_rng.nextRange(100) < trill) ? true : false;
        
        // Add some variation to gate offset to make it feel less digital
        step.gateOffset = -10 + (_rng.next() % 20);  // -10% to +10%
    }
}

// Tick processing code for SNH algorithm
void TuesdayTrackEngine::tick_SNH() {
    // Update phase continuously for smooth voltage changes
    _snhPhase += _snhPhaseSpeed; // This could be scaled by clock speed
    
    // Update current position towards target if needed
    if (_snhCurrent != _snhTarget) {
        int32_t delta = (_snhTarget - _snhCurrent) / 100; // Adjust smoothing factor
        if (delta != 0) {
            _snhCurrent += delta;
        } else {
            _snhCurrent = _snhTarget; // Snap to target if very close
        }
    }
}