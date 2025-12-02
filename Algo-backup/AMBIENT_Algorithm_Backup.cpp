// AMBIENT Algorithm Backup
//
// This is the AMBIENT algorithm implementation that was extracted from TuesdayTrackEngine.cpp
// Algorithm ID: 13
//
// This algorithm creates harmonic drone textures and sparse melodic events for ambient textures.

#include "TuesdayTrackEngine.h"

// Algorithm-specific state variables that were part of TuesdayTrackEngine class:
/*
int8_t _ambient_root_note;
int8_t _ambient_drone_notes[3]; // The notes of the drone chord
int _ambient_event_timer;       // Countdown to the next sparse event
uint8_t _ambient_event_type;    // 0=none, 1=single note, 2=arpeggio
uint8_t _ambient_event_step;    // Position within the current event
*/

// Initialization code for AMBIENT algorithm
void TuesdayTrackEngine::initAlgorithm_AMBIENT() {
    // 1. Set up the drone chord based on Flow. Not random, but deterministic.
    _ambient_root_note = (sequence.flow() - 1) % 12;
    _ambient_drone_notes[0] = _ambient_root_note;
    _ambient_drone_notes[1] = (_ambient_root_note + 7) % 12; // Perfect 5th
    _ambient_drone_notes[2] = (_ambient_root_note + 16) % 12; // A Major 9th (as a 2nd)
    
    // 2. Init event state using Ornament for randomness
    _extraRng = Random((sequence.ornament() - 1) << 4);
    _ambient_event_timer = 16 + (_extraRng.next() % 48); // First event in 16-64 steps
    _ambient_event_type = 0;
    _ambient_event_step = 0;
}

// Buffer generation code for AMBIENT algorithm
void TuesdayTrackEngine::generateBuffer_AMBIENT() {
    // AMBIENT: Harmonic Drone & Event Scheduler
    // Algorithm 13 - Harmonic Drone & Event Scheduler
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        auto &step = _buffer[i];
        
        // Advance timers and update state
        _ambient_event_timer--;
        
        // Determine if we're in an event
        if (_ambient_event_type != 0) {
            // We're in an event - process it
            if (_ambient_event_type == 1) {  // Single note event
                step.note = _ambient_root_note;  // Use root of drone as base
                step.octave = 2 + (_ambient_event_step % 2);  // Vary octave slightly
                
                // Gentle gate length for ambient feel
                step.gatePercent = 80 + (_extraRng.next() % 40);  // 80-120%
                
                // Rare slides for ethereal feel
                step.slide = (_extraRng.nextRange(100) < glide/5) ? 1 : 0;
                
                // Very rare trills for distant shimmer
                step.isTrill = (_extraRng.nextRange(100) < trill/10) ? true : false;
                
                // Timing slightly flexible for ambient feel
                step.gateOffset = -10 + (_extraRng.next() % 20);  // -10% to +10%
                
                _ambient_event_step++;
                
                // End event after 1-3 steps
                if (_ambient_event_step > (1 + _extraRng.next() % 3)) {
                    _ambient_event_type = 0;  // End event
                }
            }
            else if (_ambient_event_type == 2) {  // Arpeggio event
                // Arpeggiate through drone chord
                int arpIndex = _ambient_event_step % 3;
                step.note = _ambient_drone_notes[arpIndex];
                step.octave = 1 + (arpIndex / 2);  // Higher for upper notes
                
                // Longer gate for arpeggiated notes
                step.gatePercent = 70 + (_extraRng.next() % 50);  // 70-120%
                
                // More likely to have slides between arpeggiated notes
                step.slide = (_extraRng.nextRange(100) < glide/2) ? 1 : 0;
                
                // Occasional trills for arpeggio notes
                step.isTrill = (_extraRng.nextRange(100) < trill/5) ? true : false;
                
                // Slightly stricter timing for arpeggio
                step.gateOffset = -5 + (_extraRng.next() % 11);  // -5% to +5%
                
                _ambient_event_step++;
                
                // End arpeggio after 6-10 steps
                if (_ambient_event_step > (5 + _extraRng.next() % 5)) {
                    _ambient_event_type = 0;  // End event
                }
            }
        } else {
            // We're in drone phase - sustain basic drone chord
            // Use first drone note for simplicity in this buffer
            step.note = _ambient_drone_notes[0];
            step.octave = 1;  // Medium octave for primary drone
            
            // Long sustained gates for drone
            step.gatePercent = 120 + (_extraRng.next() % 80);  // 120-200% (very long)
            
            // Drift and microtonal variations
            step.slide = (_extraRng.nextRange(100) < glide/10) ? 1 : 0;
            
            // Almost no trills during drone for peaceful atmosphere
            step.isTrill = false;
            
            // Very stable timing for drone foundation
            step.gateOffset = -2 + (_extraRng.next() % 5);  // -2% to +2%
            
            // Random chance to switch drone note occasionally
            if (_extraRng.nextRange(100) < 5) {  // 5% chance
                int droneChoice = _extraRng.next() % 3;
                step.note = _ambient_drone_notes[droneChoice];
                step.octave = 1 + (droneChoice > 1 ? 1 : 0);  // Higher octave for 3rd+ notes
            }
            
            // Check if it's time for a new event
            if (_ambient_event_timer <= 0) {
                // Start a new event based on probability
                int eventType = _extraRng.next() % 3;  // 0=none, 1=single, 2=arp
                
                // Adjust probabilities based on power setting
                int power = sequence.power();
                if (eventType == 0 && _extraRng.nextRange(100) > (20 + power)) {
                    eventType = 1;  // Increase event probability with power
                }
                
                if (eventType > 0) {
                    _ambient_event_type = eventType;
                    _ambient_event_step = 0;
                    // Set next event time based on current setting
                    _ambient_event_timer = 20 + (_extraRng.next() % 50); // 20-70 steps until next
                } else {
                    // No event, set timer for next check
                    _ambient_event_timer = 10 + (_extraRng.next() % 40); // 10-50 steps
                }
            }
        }
    }
}

// Tick processing code for AMBIENT algorithm
void TuesdayTrackEngine::tick_AMBIENT() {
    // Update event timer continuously
    _ambient_event_timer--;
    
    // Check if we need to schedule a new event
    if (_ambient_event_type == 0 && _ambient_event_timer <= 0) {
        // Determine if we should start a new event
        if (_rng.nextRange(100) < power) {  // Power affects event frequency
            // Start new event
            _ambient_event_type = (_rng.nextRange(100) < 70) ? 1 : 2;  // 70% single note, 30% arpeggio
            _ambient_event_step = 0;
            
            // Schedule next event based on ornament value
            _ambient_event_timer = 15 + (_rng.next() % (50 - (ornament / 2)));
        } else {
            // No event, reset timer for next check
            _ambient_event_timer = 8 + (_rng.next() % 20);
        }
    }
}