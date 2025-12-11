// KRAFT Algorithm Backup
//
// This is the KRAFT algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 17
//
// This algorithm generates precise mechanical sequences reminiscent of Kraftwerk.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
uint8_t _kraftSequence[8] = {0};         // 8-step mechanical sequence
uint8_t _kraftPosition = 0;              // Current position in sequence
uint8_t _kraftLockTimer = 0;             // Timer for locked groove (16-32 steps)
uint8_t _kraftTranspose = 0;             // Current transposition
uint8_t _kraftTranspCount = 0;           // Counter for transposition changes
int8_t _kraftBaseNote = 0;               // Base note of sequence
uint8_t _kraftGhostMask = 0;             // Bitmask for ghost notes
*/

// Initialization code for KRAFT algorithm
void TuesdayTrackEngine::initAlgorithm_KRAFT() {
    _rng = Random((flow - 1) << 4);
    _extraRng = Random((ornament - 1) << 4);
    
    // Generate repetitive mechanical pattern
    _kraftBaseNote = _rng.next() % 12;
    for (int i = 0; i < 8; i++) {
        // Kraftwerk patterns often alternate between 2-3 notes
        _kraftSequence[i] = (_kraftBaseNote + ((i % 2) ? 7 : 0)) % 12;  // Root and fifth
    }
    _kraftPosition = 0;
    _kraftLockTimer = 16 + (_rng.next() % 16);  // Lock for 16-31 steps
    _kraftTranspose = 0;
    _kraftTranspCount = 0;
    _kraftGhostMask = _extraRng.next() & 0x55;  // Every other step ghost
}

// Buffer generation code for KRAFT algorithm
void TuesdayTrackEngine::generateBuffer_KRAFT() {
    // KRAFT: precise mechanical sequences
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        // Get current position in sequence
        int seqPos = (_kraftPosition + i) % 8;
        uint8_t sequenceNote = _kraftSequence[seqPos];
        
        // Apply transposition to the base note
        uint8_t transposedNote = (sequenceNote + _kraftTranspose) % 12;
        
        // Determine octave based on pattern - Kraftwerk uses consistent octaves
        uint8_t octave = 1;  // Mostly in middle octave
        
        // Occasionally use different octaves for emphasis
        if ((seqPos == 0 || seqPos == 4) && (_kraftTranspCount % 3 == 0)) {
            octave = 2;  // Higher octave on strong beats periodically
        } else if (_rng.nextRange(20) == 0) {  // Rarely drop down
            octave = 0;  // Lower octave for variation
        }
        
        // Set the step parameters
        step.note = transposedNote;
        step.octave = octave;
        
        // Kraftwerk-style gate lengths - precise and rhythmic
        if ((seqPos % 2) == 0) {
            // Strong beats get longer gates
            step.gatePercent = 70 + _rng.nextRange(20);  // 70-90%
        } else {
            // Weak beats get shorter gates
            step.gatePercent = 40 + _rng.nextRange(25);  // 40-65%
        }
        
        // Check if this is a ghost note position
        bool isGhostNote = (_kraftGhostMask >> seqPos) & 1;
        
        if (isGhostNote) {
            // Ghost notes are quieter and shorter
            step.gatePercent = step.gatePercent / 2;  // Half the gate length
            step.octave = octave - 1;  // Usually one octave lower
            if (step.octave > 3) step.octave = 0;  // Handle underflow
        }
        
        // Apply slides based on user glide setting
        // Kraftwerk uses slides but sparingly
        if (_rng.nextRange(100) < (glide / 3) && seqPos % 4 != 0) {
            // Rarely for non-beat steps
            step.slide = 1;
        } else if (_rng.nextRange(100) < (glide / 2) && seqPos % 2 == 0) {
            // More commonly on beat steps
            step.slide = 1;
        } else {
            step.slide = 0;
        }
        
        // Apply trills based on user setting
        // Kraftwerk uses trills minimally for precision
        step.isTrill = (isGhostNote) ? false : (_rng.nextRange(100) < (trill / 5)) ? true : false;
        
        // Precise timing typical of mechanical sequences
        step.gateOffset = -1 + _rng.nextRange(3);  // -1% to +1% (very precise)
        
        // Update tracking variables
        _kraftTranspCount++;
    }
}

// Tick processing code for KRAFT algorithm
void TuesdayTrackEngine::tick_KRAFT() {
    // Update position with mechanical precision
    _kraftPosition = (_kraftPosition + 1) % 8;
    
    // Update lock timer
    if (_kraftLockTimer > 0) {
        _kraftLockTimer--;
    }
    
    // Check if we should change transposition
    if (_kraftLockTimer == 0) {
        // If lock timer expired, reset lock and potentially transpose
        if (_rng.nextRange(100) < power) {
            // Transposition based on power parameter
            _kraftTranspose = (_kraftTranspose + 1 + (_rng.next() % (power / 10 + 1))) % 12;
        }
        
        // Renew lock timer based on flow
        _kraftLockTimer = 16 + (_rng.next() % (flow / 2 + 1));
    }
    
    // Occasionally update ghost pattern based on ornament
    if (_rng.nextRange(200) < ornament) {
        _kraftGhostMask = (_kraftGhostMask ^ (1 << _rng.next() % 8));  // Toggle random bit
    }
}