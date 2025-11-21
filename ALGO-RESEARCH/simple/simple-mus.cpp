/*
 * Simplified Music Algorithms for Tuesday Algorithmic Sequencer
 * These are simplified versions of the original MUSIC-ALGOS.md algorithms
 * with only Gate, Pitch CV, Slide, and Gate Length parameters
 */

#include <stdint.h>
#include <math.h>
#include <stdlib.h>

// Structure representing algorithm state
struct SimpleAlgorithmState {
    uint32_t rng1;      // RNG seed 1 (FLOW)
    uint32_t rng2;      // RNG seed 2 (ORNAMENT)
    uint32_t step;      // Current step in pattern
    uint32_t pattern;   // Current pattern state
};

// Random number generator (simple LCG - Linear Congruential Generator)
static inline uint32_t simple_rng(uint32_t *seed) {
    *seed = (*seed * 1103515245 + 12345) & 0x7fffffff;
    return *seed;
}

// Helper to get next random in range [0, max)
static inline int rng_range(uint32_t *seed, int max) {
    return simple_rng(seed) % max;
}

// Algorithm definitions
enum SimpleAlgorithm {
    SIMPLE_AMBIENT = 0,
    SIMPLE_TECHNO,
    SIMPLE_JAZZ,
    SIMPLE_CLASSICAL,
    SIMPLE_MINIMALIST,
    SIMPLE_BREAKBEAT,
    SIMPLE_DRONE,
    SIMPLE_ARPEGGIO,
    SIMPLE_FUNK,
    SIMPLE_RAGA
};

// Structure for algorithm output
struct SimpleNoteOutput {
    uint8_t gate;        // Gate on/off (0 = off, 1 = on)
    uint8_t note;        // Note value (0-127, represents pitch CV)
    uint8_t slide;       // Slide on/off (0 = off, 1 = on)
    uint8_t gateLength;  // Gate length in steps (1-8)
};

// Initialize algorithm state
void simple_init(SimpleAlgorithmState *state, int flow, int ornament) {
    state->rng1 = (uint32_t)(flow + 1) << 16;  // Seed from flow parameter
    state->rng2 = (uint32_t)(ornament + 1) << 8; // Seed from ornament parameter
    state->step = 0;
    state->pattern = 0;
}

// Generate output for Ambient algorithm
void simple_ambient(SimpleAlgorithmState *state, SimpleNoteOutput *output) {
    // Generate gate
    output->gate = (state->step % 2 == 0) ? 1 : 0;  // Sparse pattern
    
    // Generate note (slowly evolving)
    int note_base = 36 + (state->rng1 >> 28);  // C2 to D#2 range
    output->note = note_base + (state->step / 4) % 4;  // Slow note evolution
    
    // Generate slide (frequent for ambient feel)
    output->slide = (state->step % 3 == 0) ? 1 : 0;
    
    // Generate gate length (longer for ambient feel)
    output->gateLength = output->gate ? 4 + (state->rng1 >> 29) % 4 : 1;  // 4-7 steps when gated
    
    state->step++;
}

// Generate output for Techno algorithm
void simple_techno(SimpleAlgorithmState *state, SimpleNoteOutput *output) {
    // Generate gate (driving 4/4 with syncopation)
    output->gate = (state->step % 4 == 0 || 
                    (state->step % 4 == 2 && rng_range(&state->rng1, 2) == 1)) ? 1 : 0;
    
    // Generate note (bass focused)
    if (state->step % 4 == 0) {
        output->note = 36;  // C2 (bass)
    } else {
        output->note = 36 + rng_range(&state->rng2, 4); // Higher notes occasionally
    }
    
    // Generate slide (occasional slides)
    output->slide = (rng_range(&state->rng1, 10) < 2) ? 1 : 0;
    
    // Generate gate length (solid for techno)
    output->gateLength = output->gate ? 1 + (rng_range(&state->rng2, 3)) : 1;  // 1-3 steps
    
    state->step++;
}

// Generate output for Jazz algorithm
void simple_jazz(SimpleAlgorithmState *state, SimpleNoteOutput *output) {
    // Generate gate (swing feel)
    int swing_pattern = state->step % 8;
    output->gate = (swing_pattern == 0 || swing_pattern == 2 || swing_pattern == 4 || 
                    swing_pattern == 6 || (rng_range(&state->rng1, 5) == 0)) ? 1 : 0;
    
    // Generate note (chord tones and scale degrees)
    int chord_tones[4] = {36, 40, 43, 47};  // C major 7th chord
    int scale_notes[8] = {36, 38, 40, 41, 43, 45, 47, 49};  // C major scale
    
    if (rng_range(&state->rng1, 3) == 0) {
        output->note = chord_tones[rng_range(&state->rng1, 4)];
    } else {
        output->note = scale_notes[rng_range(&state->rng2, 8)];
    }
    
    // Generate slide (jazz gliding)
    output->slide = (rng_range(&state->rng1, 8) == 0) ? 1 : 0;
    
    // Generate gate length (varied for expression)
    output->gateLength = output->gate ? 1 + (rng_range(&state->rng2, 4)) : 1;  // 1-4 steps
    
    state->step++;
}

// Generate output for Classical Counterpoint algorithm
void simple_classical(SimpleAlgorithmState *state, SimpleNoteOutput *output) {
    // Generate gate (steady, classical rhythm)
    output->gate = 1;  // Nearly always gated
    
    // Generate note (counterpoint melody)
    int voice1_pattern[8] = {36, 40, 43, 47, 45, 40, 38, 43};
    int voice2_pattern[8] = {48, 45, 43, 40, 42, 45, 48, 43};
    
    int voice_idx = state->step % 8;
    if (state->pattern % 2 == 0) {
        output->note = voice1_pattern[voice_idx];
    } else {
        output->note = voice2_pattern[voice_idx];
    }
    
    // Generate slide (classical legato)
    output->slide = (rng_range(&state->rng1, 6) < 2) ? 1 : 0;
    
    // Generate gate length (moderate for classical feel)
    output->gateLength = 1 + (rng_range(&state->rng2, 3));  // 1-3 steps
    
    state->step++;
    if (state->step % 8 == 0) state->pattern++;
}

// Generate output for Minimalist algorithm
void simple_minimalist(SimpleAlgorithmState *state, SimpleNoteOutput *output) {
    // Generate gate (consistent pattern)
    output->gate = 1;
    
    // Generate note (phasing pattern)
    int base_pattern[6] = {36, 38, 40, 36, 38, 41};  // Simple pattern
    int pattern_pos = state->step % 6;
    output->note = base_pattern[pattern_pos];
    
    // Generate slide (minimalist smoothness)
    output->slide = (pattern_pos == 0) ? 1 : 0;  // Slide at pattern boundaries
    
    // Generate gate length (consistent)
    output->gateLength = 1 + (rng_range(&state->rng2, 2));  // 1-2 steps
    
    state->step++;
}

// Generate output for Breakbeat algorithm
void simple_breakbeat(SimpleAlgorithmState *state, SimpleNoteOutput *output) {
    // Generate gate (breakbeat pattern)
    int break_pattern[16] = {
        1, 0, 1, 0,   // Bass drum pattern
        1, 0, 1, 0,   // Snare/backbeat
        1, 0, 1, 0,   // Bass drum pattern
        1, 0, 1, 0    // Snare/backbeat
    };
    
    output->gate = break_pattern[state->step % 16];
    
    // Generate note (bass and snare sounds)
    if (state->step % 8 == 0 || (state->step + 4) % 8 == 0) {
        output->note = 36;  // Bass drum
    } else {
        output->note = 50 + rng_range(&state->rng1, 8);  // Hi-hat/percussive
    }
    
    // Generate slide (occasional for bass slides)
    output->slide = (rng_range(&state->rng1, 20) == 0) ? 1 : 0;
    
    // Generate gate length (short for percussive feel)
    output->gateLength = output->gate ? 1 : 1;  // 1 step (tight feel)
    
    state->step++;
}

// Generate output for Drone algorithm
void simple_drone(SimpleAlgorithmState *state, SimpleNoteOutput *output) {
    // Generate gate (mostly on for drone feel)
    output->gate = (rng_range(&state->rng1, 10) > 1) ? 1 : 0;  // 90% chance of gate
    
    // Generate note (drone with harmonic movement)
    int drone_note = 36;  // C2 drone
    int harmony_note = 43;  // G2 harmony (5th)
    
    if (rng_range(&state->rng1, 5) == 0) {
        output->note = harmony_note;  // Occasional harmony
    } else {
        output->note = drone_note;    // Main drone
    }
    
    // Generate slide (frequent for drone feel)
    output->slide = 1;  // Always slide for smooth drone
    
    // Generate gate length (long for drone feel)
    output->gateLength = output->gate ? 6 + (rng_range(&state->rng2, 3)) : 1;  // 6-8 steps
    
    state->step++;
}

// Generate output for Arpeggio algorithm
void simple_arpeggio(SimpleAlgorithmState *state, SimpleNoteOutput *output) {
    // Generate gate (consistent for arpeggio)
    output->gate = 1;
    
    // Generate note (arpeggiated pattern)
    int chord_notes[4] = {36, 40, 43, 47};  // C major 7th
    int arp_pos = state->step % 8;
    int chord_idx = 0;
    
    // Ascending arpeggio
    if (arp_pos < 4) {
        chord_idx = arp_pos;
    } else {
        chord_idx = 7 - arp_pos;  // Descending
    }
    
    output->note = chord_notes[chord_idx] + (state->pattern / 4) * 12;  // Octave changes
    
    // Generate slide (arpeggio smoothness)
    output->slide = (rng_range(&state->rng1, 4) == 0) ? 1 : 0;
    
    // Generate gate length (varied for arpeggio feel)
    output->gateLength = 1 + (rng_range(&state->rng2, 2));  // 1-2 steps
    
    state->step++;
    if (state->step % 8 == 0) state->pattern++;
}

// Generate output for Funk algorithm
void simple_funk(SimpleAlgorithmState *state, SimpleNoteOutput *output) {
    // Generate gate (funk syncopation)
    int funk_pattern[8] = {1, 0, 1, 1, 1, 0, 1, 0};  // Classic funk rhythm
    output->gate = funk_pattern[state->step % 8];
    
    // Generate note (bass line with syncopation)
    if (state->step % 8 == 0) {
        output->note = 36;  // Strong downbeat
    } else if (output->gate) {
        output->note = 36 + rng_range(&state->rng1, 5);  // Syncopated notes
    } else {
        output->note = 36;  // Rest positions get bass as reference
    }
    
    // Generate slide (funk slides)
    output->slide = (rng_range(&state->rng1, 8) == 0) ? 1 : 0;
    
    // Generate gate length (varied for funk feel)
    output->gateLength = output->gate ? 1 + (rng_range(&state->rng2, 3)) : 1;  // 1-3 steps
    
    state->step++;
}

// Generate output for Indian Classical Raga algorithm
void simple_raga(SimpleAlgorithmState *state, SimpleNoteOutput *output) {
    // Generate gate (raga feel)
    output->gate = 1;  // Mostly gated
    
    // Generate note (raga scale movement)
    int raga_notes[7] = {36, 38, 40, 41, 43, 45, 47};  // Sa Re Ga Ma Pa Dha Ni
    int arohana[8] = {0, 1, 2, 3, 4, 5, 6, 4};  // Ascending (with return)
    int avarohana[8] = {4, 6, 5, 4, 3, 2, 1, 0};  // Descending
    
    int position = state->step % 8;
    int note_idx;
    
    if (state->pattern % 2 == 0) {
        note_idx = arohana[position];  // Ascending
    } else {
        note_idx = avarohana[position];  // Descending
    }
    
    output->note = raga_notes[note_idx];
    
    // Generate slide (gamaka - ornamental slides)
    output->slide = (rng_range(&state->rng1, 5) < 2) ? 1 : 0;
    
    // Generate gate length (varied for expression)
    output->gateLength = 1 + (rng_range(&state->rng2, 3));  // 1-3 steps
    
    state->step++;
    if (state->step % 8 == 0) state->pattern++;
}

// Main algorithm generator function
void simple_algorithm_generator(int algorithm, SimpleAlgorithmState *state, SimpleNoteOutput *output) {
    switch(algorithm) {
        case SIMPLE_AMBIENT:
            simple_ambient(state, output);
            break;
        case SIMPLE_TECHNO:
            simple_techno(state, output);
            break;
        case SIMPLE_JAZZ:
            simple_jazz(state, output);
            break;
        case SIMPLE_CLASSICAL:
            simple_classical(state, output);
            break;
        case SIMPLE_MINIMALIST:
            simple_minimalist(state, output);
            break;
        case SIMPLE_BREAKBEAT:
            simple_breakbeat(state, output);
            break;
        case SIMPLE_DRONE:
            simple_drone(state, output);
            break;
        case SIMPLE_ARPEGGIO:
            simple_arpeggio(state, output);
            break;
        case SIMPLE_FUNK:
            simple_funk(state, output);
            break;
        case SIMPLE_RAGA:
            simple_raga(state, output);
            break;
        default:
            // Default: simple chromatic sequence
            output->gate = 1;
            output->note = 36 + (state->step % 12);
            output->slide = 0;
            output->gateLength = 1;
            state->step++;
            break;
    }
}