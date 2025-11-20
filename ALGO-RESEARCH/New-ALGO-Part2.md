# Additional Tuesday Algorithm Implementations for Performer

This document provides complete C code implementations for the remaining 4 proposed algorithmic approaches, following the structure and conventions of the original Tuesday algorithms.

## 7. Particle System Sequencer

```c
#include "Algo.h"

// Particle System Algorithm Implementation
typedef struct Particle {
    float x, y;
    float vx, vy;
    float life;
    uint8_t active : 1;
} Particle;

typedef struct PatternStruct_Algo_Particle {
    Particle particles[8];  // Limited to 8 particles for performance
    uint8_t active_count;
    float gravity;
    float boundary_x, boundary_y;
    uint8_t next_particle;
    uint8_t trigger_threshold;
} PatternStruct_Algo_Particle;

void Algo_Particle_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                      Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Initialize particles based on seeds
    Output->Particle.active_count = 3 + (T->seed1 % 5);  // 3-7 particles
    Output->Particle.gravity = -0.05 + (T->seed2 % 100) / 1000.0;
    Output->Particle.boundary_x = 1.0 + (T->intensity % 50) / 50.0;
    Output->Particle.boundary_y = 1.0 + (T->intensity % 50) / 50.0;
    Output->Particle.next_particle = 0;
    Output->Particle.trigger_threshold = 50 + (T->intensity / 2);  // 50-175
    
    for(int i = 0; i < 8; i++) {
        Output->Particle.particles[i].x = (Tuesday_Rand(R) % 200) / 200.0 * Output->Particle.boundary_x * 0.8;
        Output->Particle.particles[i].y = (Tuesday_Rand(R) % 200) / 200.0 * Output->Particle.boundary_y * 0.8;
        Output->Particle.particles[i].vx = (Tuesday_Rand(R) % 200 - 100) / 1000.0;
        Output->Particle.particles[i].vy = (Tuesday_Rand(R) % 200 - 100) / 1000.0;
        Output->Particle.particles[i].life = 100;
        Output->Particle.particles[i].active = (i < Output->Particle.active_count) ? 1 : 0;
    }
}

void Algo_Particle_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                      Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);
    
    uint8_t triggered_count = 0;
    
    // Update particles
    for(int i = 0; i < 8; i++) {
        if(!PS->Particle.particles[i].active) continue;
        
        Particle *p = &PS->Particle.particles[i];
        
        // Update position
        p->x += p->vx;
        p->y += p->vy;
        
        // Apply gravity
        p->vy += PS->Particle.gravity;
        
        // Boundary collision (X)
        if(p->x > PS->Particle.boundary_x) {
            p->x = PS->Particle.boundary_x;
            p->vx *= -0.8;  // Bounce with energy loss
        } else if(p->x < 0) {
            p->x = 0;
            p->vx *= -0.8;
        }
        
        // Boundary collision (Y)
        if(p->y > PS->Particle.boundary_y) {
            p->y = PS->Particle.boundary_y;
            p->vy *= -0.8;
            p->vx *= 0.9;  // Friction
        } else if(p->y < 0) {
            p->y = 0;
            p->vy *= -0.8;
        }
        
        // Trigger note if particle crosses Y threshold
        if(p->y > 0.5) {
            int note = (int)(p->x * 8) % 8;
            int octave = (int)(p->y * 3) % 3;
            
            NOTE(octave, note);
            Output->vel = (int)(p->y * 127);
            Output->accent = Tuesday_PercChance(R, PS->Particle.trigger_threshold);
            triggered_count++;
        }
    }
    
    // Occasionally add or remove particles based on intensity
    if(Tuesday_PercChance(R, T->intensity / 8)) {
        // Add a new particle
        if(PS->Particle.active_count < 8) {
            int new_idx = PS->Particle.next_particle;
            PS->Particle.particles[new_idx].x = (Tuesday_Rand(R) % 200) / 200.0 * PS->Particle.boundary_x * 0.5;
            PS->Particle.particles[new_idx].y = 0.1;
            PS->Particle.particles[new_idx].vx = (Tuesday_Rand(R) % 200 - 100) / 2000.0;
            PS->Particle.particles[new_idx].vy = (Tuesday_Rand(R) % 100) / 1000.0 + 0.05;
            PS->Particle.particles[new_idx].life = 100;
            PS->Particle.particles[new_idx].active = 1;
            PS->Particle.active_count++;
            PS->Particle.next_particle = (PS->Particle.next_particle + 1) % 8;
        }
    }
    
    if(triggered_count > 0) {
        Output->note = ScaleToNote(&SN, T, P, S);
    } else {
        NOTEOFF();
        Output->note = TUESDAY_NOTEOFF;
    }
    
    RandomSlideAndLength(Output, R);
}

const PatternFunctions Algo_Particle = {
    Algo_Particle_Gen,
    Algo_Particle_Init,
    NoPatternInit,
    0
};
```

## 8. Neural Network Simulator

```c
// Neural Network Simulator Implementation
typedef struct PatternStruct_Algo_Neural {
    float neurons[16];           // Simple neural network (16 neurons for 16 steps)
    float weights[16][16];       // Connection weights
    float activation_threshold;  // Activation threshold
    int last_active;             // Last active neuron
    int step_cycle;              // Current step in the cycle
    float learning_rate;         // Learning rate for weight modification
} PatternStruct_Algo_Neural;

void Algo_Neural_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                     Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Initialize neurons with values based on seeds
    for(int i = 0; i < 16; i++) {
        Output->Neural.neurons[i] = (Tuesday_Rand(R) % 200 - 100) / 100.0; // Random values between -1 and 1
    }
    
    // Initialize weights based on seeds to create interesting patterns
    for(int i = 0; i < 16; i++) {
        for(int j = 0; j < 16; j++) {
            Output->Neural.weights[i][j] = (Tuesday_Rand(R) % 200 - 100) / 1000.0; // Small random weights
        }
    }
    
    Output->Neural.activation_threshold = -0.2 + (T->intensity % 40) / 200.0; // Variable threshold
    Output->Neural.last_active = 0;
    Output->Neural.step_cycle = 0;
    Output->Neural.learning_rate = 0.01 + (T->seed1 % 50) / 5000.0; // Learning rate based on seed
}

void Algo_Neural_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                    Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);
    
    // Propagate activation through network based on current state
    float new_activations[16];
    for(int i = 0; i < 16; i++) {
        float sum = 0;
        for(int j = 0; j < 16; j++) {
            sum += PS->Neural.neurons[j] * PS->Neural.weights[j][i];
        }
        
        // Apply activation function (sigmoid)
        new_activations[i] = 1.0 / (1.0 + exp(-sum)) * 2.0 - 1.0; // Normalize to [-1, 1]
    }
    
    // Copy back activations
    for(int i = 0; i < 16; i++) {
        PS->Neural.neurons[i] = new_activations[i];
    }
    
    // Trigger note if activation exceeds threshold
    float current_activation = PS->Neural.neurons[I % 16];
    
    if(fabs(current_activation) > PS->Neural.activation_threshold) {
        int note = (int)(fabs(current_activation) * 8) % 8;
        int octave = (current_activation > 0) ? 1 : 0;
        NOTE(octave, note);
        
        Output->vel = (int)(fabs(current_activation) * 127);
        Output->accent = (fabs(current_activation) > 0.8) ? 1 : 0;
        
        // Update last active
        PS->Neural.last_active = I % 16;
    } else {
        NOTEOFF();
    }
    
    // Apply learning - modify weights based on intensity
    if(Tuesday_PercChance(R, T->intensity / 10)) {  // Learn occasionally based on intensity
        int neuron1 = Tuesday_Rand(R) % 16;
        int neuron2 = Tuesday_Rand(R) % 16;
        
        // Hebbian learning: "neurons that fire together, wire together"
        float correlation = PS->Neural.neurons[neuron1] * PS->Neural.neurons[neuron2];
        PS->Neural.weights[neuron1][neuron2] += PS->Neural.learning_rate * correlation;
        
        // Keep weights in reasonable range
        if(PS->Neural.weights[neuron1][neuron2] > 1.0) PS->Neural.weights[neuron1][neuron2] = 1.0;
        if(PS->Neural.weights[neuron1][neuron2] < -1.0) PS->Neural.weights[neuron1][neuron2] = -1.0;
    }
    
    Output->note = ScaleToNote(&SN, T, P, S);
    RandomSlideAndLength(Output, R);
    
    PS->Neural.step_cycle++;
}

const PatternFunctions Algo_Neural = {
    Algo_Neural_Gen,
    Algo_Neural_Init,
    NoPatternInit,
    0
};
```

## 9. Quantum Probability Sequencer

```c
// Quantum Probability Algorithm Implementation
typedef struct PatternStruct_Algo_Quantum {
    float probability_map[16][8];  // Probability for each note at each step
    float wave_function[8];        // Current wave function
    uint8_t superposition_state;   // Current quantum state
    uint8_t collapse_threshold;    // Threshold for wave function collapse
    uint8_t quantum_entropy;       // Quantum randomness factor
} PatternStruct_Algo_Quantum;

void Algo_Quantum_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                      Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Initialize probability maps based on seeds
    for(int step = 0; step < 16; step++) {
        float total_prob = 0;
        for(int note = 0; note < 8; note++) {
            // Create initial probability distribution based on seeds
            Output->Quantum.probability_map[step][note] = 
                (Tuesday_Rand(R) % 100) / 100.0 * (0.5 + (T->seed1 % 50) / 100.0);
            total_prob += Output->Quantum.probability_map[step][note];
        }
        
        // Normalize probabilities
        if(total_prob > 0) {
            for(int note = 0; note < 8; note++) {
                Output->Quantum.probability_map[step][note] /= total_prob;
            }
        }
    }
    
    // Initialize wave function
    for(int note = 0; note < 8; note++) {
        Output->Quantum.wave_function[note] = (Tuesday_Rand(R) % 200 - 100) / 200.0; // Complex amplitude
    }
    
    Output->Quantum.superposition_state = 0;
    Output->Quantum.collapse_threshold = 50 + (T->intensity / 2);  // 50-175%
    Output->Quantum.quantum_entropy = 30 + (T->seed2 % 70);        // 30-100%
}

void Algo_Quantum_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                     Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);
    
    // Calculate cumulative probability distribution for current step
    float cumulative_prob = 0;
    for(int note = 0; note < 8; note++) {
        cumulative_prob += PS->Quantum.probability_map[I % 16][note];
    }
    
    if(cumulative_prob > 0) {
        // "Collapse" the wave function based on probability
        float random_val = Tuesday_Rand(R) / (float)0xFFFF;
        float current_prob = 0;
        
        for(int note = 0; note < 8; note++) {
            current_prob += PS->Quantum.probability_map[I % 16][note] / cumulative_prob;
            if(random_val <= current_prob) {
                NOTE(0, note);
                Output->vel = (int)(PS->Quantum.probability_map[I % 16][note] * 127);
                
                // Determine accent based on quantum probability
                Output->accent = Tuesday_PercChance(R, PS->Quantum.collapse_threshold);
                
                // Store quantum state for future calculations
                PS->Quantum.superposition_state = note;
                break;
            }
        }
    } else {
        NOTEOFF();
    }
    
    // Update probability map based on quantum evolution and entropy
    for(int note = 0; note < 8; note++) {
        // Apply quantum uncertainty principle - increase uncertainty over time
        float change = (Tuesday_Rand(R) - 0x8000) / 65536.0 * (PS->Quantum.quantum_entropy / 100.0);
        PS->Quantum.probability_map[I % 16][note] += change;
        
        // Ensure probability remains positive
        if(PS->Quantum.probability_map[I % 16][note] < 0) 
            PS->Quantum.probability_map[I % 16][note] = 0;
    }
    
    // Occasionally redistribute probabilities (quantum decoherence)
    if(Tuesday_PercChance(R, 10 + (T->intensity / 10))) {
        float total = 0;
        for(int note = 0; note < 8; note++) {
            total += PS->Quantum.probability_map[I % 16][note];
        }
        
        if(total > 0) {
            for(int note = 0; note < 8; note++) {
                PS->Quantum.probability_map[I % 16][note] /= total;
            }
        }
    }
    
    Output->note = ScaleToNote(&SN, T, P, S);
    RandomSlideAndLength(Output, R);
}

const PatternFunctions Algo_Quantum = {
    Algo_Quantum_Gen,
    Algo_Quantum_Init,
    NoPatternInit,
    0
};
```

## 10. L-System Generator

```c
// L-System Algorithm Implementation
typedef struct PatternStruct_Algo_LSystem {
    char axiom[16];      // Initial string
    char rules[4][2];    // Simple rules: input -> output (2 chars max)
    char current_string[32]; // Current string after iterations
    uint8_t iterations;  // Number of iterations performed
    uint8_t string_length; // Length of current string
    uint8_t current_pos; // Current position in string
    uint8_t angle;       // Angle for geometric interpretation
} PatternStruct_Algo_LSystem;

void Algo_LSystem_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                      Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Initialize axiom based on seeds
    const char* axioms[] = {"A", "AB", "A-B", "X", "F", "G", "A+B", "XY"};
    int axiom_idx = T->seed1 % 8;
    int len = strlen(axioms[axiom_idx]);
    strncpy(Output->LSystem.axiom, axioms[axiom_idx], len < 15 ? len : 15);
    Output->LSystem.axiom[len < 15 ? len : 15] = '\0';
    
    // Initialize simple rules based on seeds
    Output->LSystem.rules[0][0] = 'A';
    Output->LSystem.rules[0][1] = (T->seed1 % 26) + 'A'; // Map to letter
    
    Output->LSystem.rules[1][0] = 'B';
    Output->LSystem.rules[1][1] = (T->seed2 % 26) + 'A';
    
    Output->LSystem.rules[2][0] = 'X';
    Output->LSystem.rules[2][1] = (T->intensity % 26) + 'A';
    
    Output->LSystem.rules[3][0] = 'Y';
    Output->LSystem.rules[3][1] = 'A' + (T->seed1 + T->seed2) % 26;
    
    // Initialize current string with axiom
    strcpy(Output->LSystem.current_string, Output->LSystem.axiom);
    Output->LSystem.string_length = strlen(Output->LSystem.current_string);
    Output->LSystem.iterations = 0;
    Output->LSystem.current_pos = 0;
    Output->LSystem.angle = 0;
}

void Algo_LSystem_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                     Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);
    
    // Apply iterations based on intensity
    if(PS->LSystem.iterations < (T->intensity / 32) && Tuesday_PercChance(R, 50)) {
        // Perform one iteration of string rewriting
        char new_string[64];  // Buffer for new string
        int new_len = 0;
        
        for(int i = 0; i < PS->LSystem.string_length && new_len < 60; i++) {
            char c = PS->LSystem.current_string[i];
            
            // Apply rules
            int rule_applied = 0;
            for(int r = 0; r < 4; r++) {
                if(PS->LSystem.rules[r][0] == c) {
                    new_string[new_len++] = PS->LSystem.rules[r][1];
                    rule_applied = 1;
                    break;
                }
            }
            
            // If no rule applies, keep original character
            if(!rule_applied) {
                new_string[new_len++] = c;
            }
        }
        
        new_string[new_len] = '\0';
        strcpy(PS->LSystem.current_string, new_string);
        PS->LSystem.string_length = new_len;
        PS->LSystem.iterations++;
    }
    
    // Map current position in string to musical parameters
    if(PS->LSystem.string_length > 0) {
        char current_char = PS->LSystem.current_string[PS->LSystem.current_pos];
        
        // Map character to note (simple approach)
        int note_value = current_char % 12;  // Map any character to 0-11
        int note = note_value % 8;  // Fit within our 8-note range
        int octave = (note_value / 8) % 3;  // 3 octaves
        
        // Determine if we play based on the character
        if((current_char >= 'A' && current_char <= 'Z') || 
           (current_char >= '0' && current_char <= '9')) {
            NOTE(octave, note);
            
            // Calculate velocity based on position and character
            Output->vel = 64 + (current_char % 64);
            
            // Determine accent based on character type
            Output->accent = (current_char >= 'A' && current_char <= 'M') ? 1 : 0;
        } else {
            // Map other characters to rests or special behavior
            NOTEOFF();
        }
        
        // Advance position
        PS->LSystem.current_pos = (PS->LSystem.current_pos + 1) % PS->LSystem.string_length;
        
        // Update angle for potential geometric interpretation
        PS->LSystem.angle = (PS->LSystem.angle + (current_char % 15) * 24) % 360;
    } else {
        NOTEOFF();
    }
    
    Output->note = ScaleToNote(&SN, T, P, S);
    RandomSlideAndLength(Output, R);
}

const PatternFunctions Algo_LSystem = {
    Algo_LSystem_Gen,
    Algo_LSystem_Init,
    NoPatternInit,
    0
};
```

## Integration Notes

These implementations complete the full set of 10 algorithmic approaches for a Tuesday-style track in the Performer sequencer:

1. Cellular Automata
2. Chaos Theory
3. Fractal Pattern Generator
4. Wave Interference
5. DNA Sequence
6. Turing Machine
7. Particle System (NEW)
8. Neural Network (NEW)
9. Quantum Probability (NEW)
10. L-System Generator (NEW)

Each algorithm follows the same structure as the original Tuesday algorithms:
- A specific data structure for algorithm state
- An Init function to set up initial conditions based on seeds and intensity
- A Gen function that generates the next note/gate pair
- A PatternFunctions struct that ties them together

All algorithms incorporate the four main parameters (algorithm, seed1, seed2, intensity) as required for the simplified Tuesday track in the Performer sequencer, while maintaining the tight gate+pitch coupling characteristic of the original Tuesday module.