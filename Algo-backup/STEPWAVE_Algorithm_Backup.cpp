// STEPWAVE Algorithm Backup
//
// This is the STEPWAVE algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 20
//
// This algorithm generates scale stepping with chromatic trill for melodic runs.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
int8_t _stepwave_direction = 0;     // -1=down, 0=random, +1=up
uint8_t _stepwave_step_count = 0;   // Number of trill substeps (3-7)
int8_t _stepwave_current_step = 0;  // Current position in trill (0 to step_count-1)
int8_t _stepwave_chromatic_offset = 0; // Running chromatic offset from base note
bool _stepwave_is_stepped = true;   // true=stepped, false=slide
uint8_t _stepwave_base_note = 0;    // Base note for stepping
uint8_t _stepwave_octave = 1;      // Default octave
*/

// Initialization code for STEPWAVE algorithm
void TuesdayTrackEngine::initAlgorithm_STEPWAVE() {
    _rng = Random((flow - 1) << 4);
    _extraRng = Random((ornament - 1) << 4);
    
    // Flow controls step direction: 0-7=down, 8=stationary, 9-16=up
    if (flow <= 7) {
        _stepwave_direction = -1;  // Down
    } else if (flow >= 9) {
        _stepwave_direction = 1;   // Up
    } else {
        _stepwave_direction = 0;   // Random direction
    }
    
    // Ornament controls trill direction and step size:
    // 0-5: trill down, 2 steps
    // 6-10: trill random, 2-3 steps mixed
    // 11-16: trill up, 3 steps
    if (ornament <= 5) {
        _stepwave_direction = -1;  // Trill down
    } else if (ornament >= 11) {
        _stepwave_direction = 1;   // Trill up
    } else {
        _stepwave_direction = 0;   // Random trill direction each time
    }
    
    _stepwave_step_count = 3 + (_rng.next() % 5);  // 3-7 substeps for trill
    _stepwave_current_step = 0;
    _stepwave_chromatic_offset = 0;
    _stepwave_is_stepped = true;  // Default to stepped, glide probability determines slide
    _stepwave_base_note = 0;      // Start on C
    _stepwave_octave = 1;         // Default octave
}

// Buffer generation code for STEPWAVE algorithm
void TuesdayTrackEngine::generateBuffer_STEPWAVE() {
    // STEPWAVE: Scale stepping with chromatic trill
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        // Determine base note for this step
        int8_t baseNote = _stepwave_base_note;
        
        // Apply step direction based on settings
        int8_t stepIncrement;
        if (_stepwave_direction == 0) {
            // Random direction
            stepIncrement = _rng.nextBinary() ? 1 : -1;
        } else {
            stepIncrement = _stepwave_direction;
        }
        
        // Calculate note based on current position in pattern
        int8_t noteValue = baseNote + _stepwave_chromatic_offset;
        
        // Update chromatic offset based on direction and power
        if (_stepwave_current_step < _stepwave_step_count) {
            // In a trill - apply chromatic movement
            _stepwave_chromatic_offset += stepIncrement;
            noteValue = baseNote + _stepwave_chromatic_offset;
            _stepwave_current_step++;
        } else {
            // Finished trill - advance scale position
            _stepwave_chromatic_offset = 0;
            _stepwave_current_step = 0;
            
            // Advance to next scale degree based on direction
            if (_stepwave_direction == 0) {
                // Random scale movement
                int8_t randomMove = (_rng.next() % 3) - 1;  // -1, 0, or +1
                noteValue += randomMove;
            } else {
                // Deterministic movement
                noteValue += stepIncrement;
            }
        }
        
        // Apply glide probability to determine if stepped or smooth
        _stepwave_is_stepped = (_rng.nextRange(100) >= glide) ? true : false;
        
        // Normalize note to be within chromatic range
        while (noteValue < 0) noteValue += 12;
        while (noteValue > 11) noteValue -= 12;
        
        // Determine octave for the current note
        // Use pattern to determine octave changes
        if (abs(noteValue - _stepwave_base_note) > 5) {
            // Greater movement -> adjust octave
            _stepwave_octave = 2;
        } else if (i % 6 == 0) {
            // Periodic octave variation
            _stepwave_octave = 1 + (i % 3); // 1, 2, or wrap to 0
            if (_stepwave_octave > 3) _stepwave_octave = 0;
        }
        
        // Set the step parameters
        step.note = noteValue % 12;
        step.octave = _stepwave_octave;
        
        // Gate lengths for stepwave - vary based on trill position
        if (_stepwave_current_step > 0) {
            // In a trill - shorter gate lengths
            step.gatePercent = 40 + (_rng.next() % 40);  // 40-80%
        } else {
            // Not in trill - longer gate lengths
            step.gatePercent = 60 + (_rng.next() % 40);  // 60-100%
        }
        
        // Apply slide based on user setting and stepped/smooth toggle
        step.slide = !_stepwave_is_stepped ? 1 : 0;
        
        // Trill based on user setting and whether we're currently trilling
        step.isTrill = _stepwave_current_step > 0 ? true : (_rng.nextRange(100) < trill);
        
        // Timing offset to make it feel more natural
        step.gateOffset = -3 + (_rng.next() % 7);  // -3% to +3%
        
        // Update for next step
        _stepwave_base_note = noteValue % 12;  // Update base note for next calculation
        
        // Occasionally adjust octave based on ornament
        if (_rng.nextRange(100) < ornament/2) {
            _stepwave_octave = (_stepwave_octave + (-1 + _rng.next() % 3)) % 4;  // Change by -1, 0, or +1
            if (_stepwave_octave < 0) _stepwave_octave = 3;
        }
    }
}

// Tick processing code for STEPWAVE algorithm
void TuesdayTrackEngine::tick_STEPWAVE() {
    // Update position in pattern
    _stepwave_current_step++;
    
    if (_stepwave_current_step >= _stepwave_step_count) {
        // Finished current trill, reset
        _stepwave_current_step = 0;
        
        // Occasionally modify the step count based on power
        if (_rng.nextRange(100) < power/3) {
            _stepwave_step_count = 3 + (_rng.next() % 5);  // 3-7 steps
        }
        
        // Modify direction based on settings
        if (_stepwave_direction == 0 && _rng.nextRange(100) < 10) {  // Random direction
            _stepwave_direction = _rng.nextBinary() ? 1 : -1;
        }
    }
}