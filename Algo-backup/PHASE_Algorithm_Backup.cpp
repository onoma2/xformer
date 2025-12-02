// PHASE Algorithm Backup
//
// This is the PHASE algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 11
//
// This algorithm generates minimalist phasing patterns with gradual shifts and mathematical precision.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
uint32_t _phaseAccum = 0;
uint32_t _phaseSpeed = 0;
uint32_t _phasePattern[8] = {0};  // Simple melodic cell (0-7)
uint8_t _phaseLength = 4;  // Pattern length 3-8 (safe default to avoid division by zero)
*/

// Initialization code for PHASE algorithm
void TuesdayTrackEngine::initAlgorithm_PHASE() {
    // Flow seeds main RNG, Ornament seeds extra RNG
    _rng = Random((flow - 1) << 4);
    _extraRng = Random((ornament - 1) << 4);
    
    _phaseAccum = 0;
    _phaseSpeed = 0x1000000 + (_extraRng.next() & 0xffffff);  // Slow phase drift
    _phaseLength = 3 + (_rng.next() % 6);  // Pattern length 3-8
    
    // Simple melodic cell (0-7)
    for (int i = 0; i < 8; i++) {
        _phasePattern[i] = _rng.next() % 8;  // 0-7 simple melodic cell
    }
}

// Buffer generation code for PHASE algorithm
void TuesdayTrackEngine::generateBuffer_PHASE() {
    // PHASE: Minimalist phasing patterns with gradual shifts
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        // Update phase accumulator
        _phaseAccum += _phaseSpeed;
        
        // Calculate position in pattern using phase
        int patternPos = (_phaseAccum >> 24) % _phaseLength;  // Use high bits for position
        patternPos = patternPos % 8;  // Keep within pattern array bounds
        
        // Get note from pattern cell at position
        uint8_t note = _phasePattern[patternPos];
        
        // Add phasing effect by modifying note based on lower phase bits
        uint8_t phaseVariation = (_phaseAccum >> 16) & 0x03;  // Use middle bits for variation
        note = (note + phaseVariation) % 12;  // Keep in chromatic range
        
        // Determine octave based on phase as well
        uint8_t octave = 0;
        if ((_phaseAccum >> 28) & 1) octave++;  // Bit 28 affects octave
        if ((_phaseAccum >> 26) & 1) octave++;  // Bit 26 affects octave
        
        // Cap at reasonable octave range
        if (octave > 3) octave = 3;
        
        // Set the basic parameters
        step.note = note;
        step.octave = octave;
        
        // Phasing patterns have gradually evolving gate lengths
        // Use phase information to create gradual changes
        uint8_t baseGate = 60;  // Base gate length
        uint8_t phaseModulation = (_phaseAccum >> 20) & 0x3F;  // Lower 6 bits for gate variation
        step.gatePercent = baseGate + phaseModulation;  // 60-127%
        
        // Apply slides based on phase relationships
        // Gradual changes more likely to slide than big jumps
        uint8_t nextPatternPos = ((_phaseAccum + _phaseSpeed) >> 24) % _phaseLength;
        nextPatternPos = nextPatternPos % 8;
        uint8_t nextNote = _phasePattern[nextPatternPos];
        nextNote = (nextNote + ((_phaseAccum + _phaseSpeed) >> 16) & 0x03) % 12;
        
        // If notes are close, add slide
        if (abs((int)note - (int)nextNote) <= 2 && _rng.nextRange(100) < glide) {
            step.slide = 1;
        } else {
            step.slide = 0;
        }
        
        // Trills based on phase coincidence
        // When phase values align in certain ways, add trill
        uint8_t phaseCoincidence = ((_phaseAccum >> 20) ^ (_phaseAccum >> 16)) & 0x07;
        if (phaseCoincidence == 0 && _rng.nextRange(100) < trill) {
            step.isTrill = true;
        } else {
            step.isTrill = false;
        }
        
        // Timing offset also follows phase pattern
        int16_t phaseOffset = (_phaseAccum >> 22) & 0x1F;  // 0-31
        // Center around 0: -15 to +16
        step.gateOffset = phaseOffset - 15;
        
        // Apply gradual slowing of pattern length changes based on power
        if (i % (8 + power) == 0) {  // Change pattern characteristics gradually
            // Slowly evolve pattern based on power setting
            if (_rng.nextRange(10) < power) {
                int changePos = _rng.next() % _phaseLength;
                _phasePattern[changePos] = (_phasePattern[changePos] + 1) % 8;
            }
        }
    }
}

// Tick processing code for PHASE algorithm
void TuesdayTrackEngine::tick_PHASE() {
    // In phase mode, continuously update the accumulator
    _phaseAccum += _phaseSpeed;
    
    // Adjust phase speed based on real-time parameters if needed
    // This allows for live control of phasing rate
    _phaseSpeed = 0x1000000 + ((_phaseSpeed >> 8) & 0xFFFFFF) + (glide << 12);  // Glide affects phase speed
}