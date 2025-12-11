// WOBBLE Algorithm Backup
//
// This is the WOBBLE algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 7
//
// This algorithm generates dual-oscillator wobble patterns for dubstep-style bass lines.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
uint32_t _wobblePhase = 0;
uint32_t _wobblePhaseSpeed = 0;
uint32_t _wobblePhase2 = 0;
uint32_t _wobblePhaseSpeed2 = 0;
uint8_t _wobbleLastWasHigh = 0;
*/

// Initialization code for WOBBLE algorithm
void TuesdayTrackEngine::initAlgorithm_WOBBLE() {
    // Flow seeds main RNG, Ornament seeds extra RNG
    _rng = Random((flow - 1) << 4);
    _extraRng = Random((ornament - 1) << 4);
    
    _wobblePhase = 0;
    _wobblePhaseSpeed = 0xffffffff / 16;  // Default based on 16-step pattern
    _wobblePhase2 = 0;
    _wobblePhaseSpeed2 = 0xcfffffff / 4;  // Faster second oscillator
    _wobbleLastWasHigh = 0;
}

// Buffer generation code for WOBBLE algorithm
void TuesdayTrackEngine::generateBuffer_WOBBLE() {
    // WOBBLE: Dual-oscillator wobble with phase interactions
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        // Calculate phase positions for both oscillators
        uint32_t phase1 = _wobblePhase + i * _wobblePhaseSpeed;
        uint32_t phase2 = _wobblePhase2 + i * _wobblePhaseSpeed2;
        
        // Determine note based on phase combination
        // Use both phases to create complex interaction
        uint8_t phase1Note = (phase1 >> 24) & 0x0F;  // Extract upper bits for note
        uint8_t phase2Note = (phase2 >> 25) & 0x0F;  // Extract upper bits for second oscillator
        
        // Combine both oscillators with some XOR logic
        uint8_t combinedNote = (phase1Note ^ phase2Note ^ (phase2Note >> 1)) & 0x0F;
        
        // Map to chromatic scale (0-11)
        uint8_t note = combinedNote % 12;
        
        // Determine octave based on phase interaction
        uint8_t octave = 0;
        if ((phase1 >> 28) & 1) octave++;  // Bit 28 affects octave
        if ((phase2 >> 27) & 1) octave++;  // Bit 27 affects octave
        
        // Cap octave to reasonable range (0-3)
        if (octave > 3) octave = 3;
        
        // Set the step parameters
        step.note = note;
        step.octave = octave;
        
        // Gate pattern for wobble - mix of long and short gates
        // Reflects the wobbly nature of dubstep bass
        if ((phase1 >> 20) & 0x03) {  // Use phase to determine gate length
            step.gatePercent = 70 + (phase1 >> 28);  // Variable based on phase
        } else {
            step.gatePercent = 120 + (phase2 >> 26); // Or different pattern
        }
        
        // Apply slide heavily to create typical wobble effect
        if (_wobbleLastWasHigh) {
            step.slide = (_rng.nextRange(100) < glide) ? 1 : 0;
        } else {
            step.slide = (_rng.nextRange(100) < glide * 2) ? 1 : 0;  // More slides
        }
        
        // Apply trill for extra wobble and texture
        step.isTrill = (_rng.nextRange(100) < trill) ? true : false;
        
        // Phase-based offset that creates the wobble timing feel
        step.gateOffset = -15 + ((phase1 + phase2) >> 28) % 30; // Variable offset
        
        // Update state based on current state
        _wobbleLastWasHigh = (octave > 1) ? 1 : 0;  // Remember if we were in high register
    }
}

// Tick processing code for WOBBLE algorithm
void TuesdayTrackEngine::tick_WOBBLE() {
    // Update phases for both oscillators
    _wobblePhase += _wobblePhaseSpeed;
    _wobblePhase2 += _wobblePhaseSpeed2;
    
    // Update based on power parameter changes
    _wobblePhaseSpeed = (0xffffffff / 16) * (power / 8 + 1);  // Adjust based on power
    _wobblePhaseSpeed2 = (0xcfffffff / 4) * (power / 6 + 1);  // Second osc different
}