// TEST Algorithm Backup
//
// This is the TEST algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 0
//
// This algorithm serves as a simple test pattern generator with different modes.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
uint8_t _testMode = 0;      // 0=OCTSWEEPS, 1=SCALEWALKER
uint8_t _testSweepSpeed = 0;
uint8_t _testAccent = 0;
uint8_t _testVelocity = 0;
int16_t _testNote = 0;
*/

// Initialization code for TEST algorithm
void TuesdayTrackEngine::initAlgorithm_TEST() {
    // seed1 (flow) determines mode and sweep speed
    // seed2 (ornament) determines accent and velocity
    _testMode = (flow - 1) >> 3;  // 0 or 1 (1-8 = mode 0, 9-16 = mode 1)
    _testSweepSpeed = ((flow - 1) & 0x3);  // 0-3
    _testAccent = (ornament - 1) >> 3;  // 0 or 1
    _testVelocity = ((ornament - 1) << 4);  // 0-240
    _testNote = 0;
}

// Buffer generation code for TEST algorithm
void TuesdayTrackEngine::generateBuffer_TEST() {
    // TEST: Fixed 4-step pattern: [1, 0, 0, 0] with varying CV
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        // Mode 0: OCTSWEEPS - sweep up octaves with decreasing duration
        if (_testMode == 0) {
            int octave = (i >> _testSweepSpeed) % 4;  // Faster sweep = higher octave more often
            step.note = 0;  // C root
            step.octave = octave;
            
            // Accent every 4th note
            step.gatePercent = (_testAccent && (i % 4 == 0)) ? _testVelocity : 75;
            
            // No slides or trills in this mode
            step.slide = 0;
            step.isTrill = false;
        }
        // Mode 1: SCALEWALKER - walk up/down a scale with random steps
        else {
            // Walk up or down based on some pattern
            int direction = (i & 0x1) ? 1 : -1;  // Alternate direction
            _testNote += direction;
            
            // Keep in reasonable range
            if (_testNote < 0) _testNote = 11;
            if (_testNote > 11) _testNote = 0;
            
            step.note = _testNote % 12;  // Map to chromatic scale
            step.octave = _testNote / 12;
            step.gatePercent = 75 + ((_i % 3) * 15);  // Vary gate length slightly
            step.slide = 0;
            step.isTrill = false;
        }
    }
}

// Tick processing code for TEST algorithm
void TuesdayTrackEngine::tick_TEST(uint32_t tick) {
    // This would contain the specific logic for the TEST algorithm during tick processing
    // Implementation would depend on the specific behavior desired for this test algorithm
}