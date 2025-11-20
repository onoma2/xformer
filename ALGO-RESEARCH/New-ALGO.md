# New Tuesday Algorithm Implementations for Performer

This document provides complete C code implementations for the proposed algorithmic approaches, following the structure and conventions of the original Tuesday algorithms.

## 1. Cellular Automata Sequencer

```c
#include "Algo.h"

// Cellular Automata Algorithm Implementation
typedef struct PatternStruct_Algo_CellAuto {
    uint8_t cells[16];  // Current state of the cellular automata
    uint8_t rule;       // Rule set for cellular automata
    uint8_t generation; // Current generation
} PatternStruct_Algo_CellAuto;

void Algo_CellAuto_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                      Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Initialize with random pattern based on seeds
    for(int i = 0; i < 16; i++) {
        Output->CellAuto.cells[i] = (Tuesday_Rand(R) % 4) < (T->seed1 % 4 + 1) ? 1 : 0;
    }
    Output->CellAuto.rule = T->seed1 % 256;
    Output->CellAuto.generation = 0;
}

void Algo_CellAuto_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                      Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);
    
    // Apply cellular automata rules to determine next state
    uint8_t left = PS->CellAuto.cells[(I-1+16)%16];
    uint8_t center = PS->CellAuto.cells[I];
    uint8_t right = PS->CellAuto.cells[(I+1)%16];
    
    // Rule-based next state calculation (Wolfram Rule 30 style)
    uint8_t pattern = (left << 2) | (center << 1) | right;
    uint8_t new_state = (PS->CellAuto.rule >> pattern) & 1;
    
    if(new_state) {
        // Calculate note based on cell position and generation
        int note = (I + PS->CellAuto.generation) % 8;
        int octave = Tuesday_BoolChance(R) ? 1 : 0;
        NOTE(octave, note);
        Output->accent = Tuesday_PercChance(R, 30 + (T->intensity / 2));
        Output->vel = Tuesday_RandByte(R);
    } else {
        NOTEOFF();
    }
    
    Output->note = ScaleToNote(&SN, T, P, S);
    RandomSlideAndLength(Output, R);
    
    // Update generation and potentially update cells
    if(Tuesday_PercChance(R, T->intensity / 4)) {
        PS->CellAuto.generation++;
    }
}

const PatternFunctions Algo_CellAuto = {
    Algo_CellAuto_Gen,
    Algo_CellAuto_Init,
    NoPatternInit,
    0  // No dithering needed
};
```

## 2. Chaos Theory Sequencer (Logistic Map)

```c
// Chaos Theory Algorithm Implementation
typedef struct PatternStruct_Algo_Chaos {
    float x;           // Current value in logistic map
    float r;           // Growth rate parameter (chaos factor)
    float threshold;   // Threshold for note triggering
    int iteration_count; // Count for pattern variation
} PatternStruct_Algo_Chaos;

void Algo_Chaos_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                    Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    Output->Chaos.x = 0.5 + (T->seed1 / 512.0);  // Initial value
    Output->Chaos.r = 3.5 + (T->seed2 % 64) / 128.0;  // Chaos parameter
    Output->Chaos.threshold = 0.5;
    Output->Chaos.iteration_count = 0;
}

void Algo_Chaos_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                   Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);
    
    // Apply logistic map iteration
    PS->Chaos.x = PS->Chaos.r * PS->Chaos.x * (1.0 - PS->Chaos.x);
    
    // Apply additional iterations based on intensity for more chaotic behavior
    for(int i = 0; i < (T->intensity / 64); i++) {
        PS->Chaos.x = PS->Chaos.r * PS->Chaos.x * (1.0 - PS->Chaos.x);
    }
    
    // Map chaotic value to musical parameters
    if(PS->Chaos.x > PS->Chaos.threshold) {
        int note = (int)(PS->Chaos.x * 8) % 8;
        int octave = (int)(PS->Chaos.x * 3) % 3;
        NOTE(octave, note);
        
        Output->vel = (int)(PS->Chaos.x * 127);
        Output->accent = Tuesday_PercChance(R, 50 + (int)(fabs(PS->Chaos.x - 0.5) * 100));
    } else if(PS->Chaos.x > (PS->Chaos.threshold - 0.1)) {
        // Near-threshold notes have reduced velocity
        int note = (int)(PS->Chaos.x * 8) % 8;
        int octave = (int)(PS->Chaos.x * 3) % 3;
        NOTE(octave, note);
        Output->vel = (int)(PS->Chaos.x * 64);
        Output->accent = 0;
    } else {
        NOTEOFF();
    }
    
    Output->note = ScaleToNote(&SN, T, P, S);
    RandomSlideAndLength(Output, R);
    
    PS->Chaos.iteration_count++;
}

const PatternFunctions Algo_Chaos = {
    Algo_Chaos_Gen,
    Algo_Chaos_Init,
    NoPatternInit,
    0
};
```

## 3. Fractal Pattern Generator

```c
// Fractal Pattern Generator Implementation
typedef struct PatternStruct_Algo_Fractal {
    float zoom;
    float offset_x, offset_y;
    uint8_t iteration_depth;
    float last_x, last_y;
    int step_counter;
} PatternStruct_Algo_Fractal;

void Algo_Fractal_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                      Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    Output->Fractal.last_x = (T->seed1 / 255.0) - 0.5;
    Output->Fractal.last_y = (T->seed2 / 255.0) - 0.5;
    Output->Fractal.zoom = 1.0 + (T->intensity / 50.0);
    Output->Fractal.offset_x = (T->seed1 / 255.0) * 2.0 - 1.0;
    Output->Fractal.offset_y = (T->seed2 / 255.0) * 2.0 - 1.0;
    Output->Fractal.iteration_depth = 10 + (T->intensity / 20);
    Output->Fractal.step_counter = 0;
}

void Algo_Fractal_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                     Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);
    
    // Fractal coordinate calculation (Mandelbrot-style iteration)
    float x = PS->Fractal.last_x;
    float y = PS->Fractal.last_y;
    float dx = (I / 8.0) - 1.0;  // Map step index to x coordinate
    float dy = (T->T / 16.0) - 1.0;  // Map time to y coordinate
    
    // Apply fractal transformation with step-dependent parameters
    float new_x = x*x - y*y + PS->Fractal.offset_x + dx/PS->Fractal.zoom;
    float new_y = 2*x*y + PS->Fractal.offset_y + dy/PS->Fractal.zoom;
    
    // Apply additional iterations based on intensity
    for(int i = 0; i < PS->Fractal.iteration_depth; i++) {
        float temp_x = new_x*new_x - new_y*new_y + PS->Fractal.offset_x + dx/PS->Fractal.zoom;
        float temp_y = 2*new_x*new_y + PS->Fractal.offset_y + dy/PS->Fractal.zoom;
        
        // Escape condition to avoid infinite iteration
        if(temp_x*temp_x + temp_y*temp_y > 4.0) {
            break;
        }
        new_x = temp_x;
        new_y = temp_y;
    }
    
    // Map fractal values to musical parameters
    int note = (int)(fabs(new_x) * 8) % 8;
    int octave = (int)(new_y * 3) % 3;
    
    if(fabs(new_x) + fabs(new_y) > 0.5) {  // Threshold for note triggering
        NOTE(octave, note);
        Output->vel = (int)(fmin(1.0, (fabs(new_x) + fabs(new_y)) * 64)) + 30;
        Output->accent = Tuesday_PercChance(R, 20 + (int)(fabs(new_x + new_y) * 30));
    } else {
        NOTEOFF();
    }
    
    Output->note = ScaleToNote(&SN, T, P, S);
    
    // Update for next iteration
    PS->Fractal.last_x = new_x;
    PS->Fractal.last_y = new_y;
    
    RandomSlideAndLength(Output, R);
    PS->Fractal.step_counter++;
}

const PatternFunctions Algo_Fractal = {
    Algo_Fractal_Gen,
    Algo_Fractal_Init,
    NoPatternInit,
    0
};
```

## 4. Wave Interference Sequencer

```c
// Wave Interference Algorithm Implementation
typedef struct PatternStruct_Algo_WaveInterference {
    float phase1, phase2, phase3;
    float freq1, freq2, freq3;
    float amplitude1, amplitude2, amplitude3;
    float global_phase;
} PatternStruct_Algo_WaveInterference;

void Algo_WaveInterference_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                              Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Initialize based on seeds
    Output->WaveInterference.phase1 = 0;
    Output->WaveInterference.phase2 = T->seed1 * 0.01;
    Output->WaveInterference.phase3 = T->seed2 * 0.01;
    
    // Calculate frequencies from seeds and intensity
    Output->WaveInterference.freq1 = 0.1 + (T->seed1 % 100) / 1000.0;
    Output->WaveInterference.freq2 = 0.15 + (T->seed2 % 80) / 800.0;
    Output->WaveInterference.freq3 = 0.2 + (T->intensity % 60) / 600.0;
    
    Output->WaveInterference.amplitude1 = 0.3 + (T->seed1 % 70) / 200.0;
    Output->WaveInterference.amplitude2 = 0.3 + (T->seed2 % 70) / 200.0;
    Output->WaveInterference.amplitude3 = 0.4 + (T->intensity % 60) / 150.0;
    
    Output->WaveInterference.global_phase = 0;
}

void Algo_WaveInterference_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                              Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);
    
    // Calculate interference at current position with time evolution
    float wave1 = sin(PS->WaveInterference.phase1 + 
                     I * PS->WaveInterference.freq1 + 
                     PS->WaveInterference.global_phase);
    float wave2 = sin(PS->WaveInterference.phase2 + 
                     I * PS->WaveInterference.freq2 + 
                     PS->WaveInterference.global_phase * 1.5);
    float wave3 = sin(PS->WaveInterference.phase3 + 
                     I * PS->WaveInterference.freq3 + 
                     PS->WaveInterference.global_phase * 0.7);
    
    float interference = wave1 * PS->WaveInterference.amplitude1 + 
                        wave2 * PS->WaveInterference.amplitude2 + 
                        wave3 * PS->WaveInterference.amplitude3;
    
    // Normalize interference to range [-1, 1]
    interference = fmax(-1.0, fmin(1.0, interference));
    
    // Map interference to musical parameters with different thresholds
    if(interference > 0.3) {  // Threshold for note triggering
        int note = (int)(fabs(interference) * 8) % 8;
        int octave = (int)(interference * 2) % 3;
        if(octave < 0) octave = 0;
        
        NOTE(octave, note);
        Output->vel = (int)(fabs(interference) * 127);
        Output->accent = (interference > 0.7) ? 1 : 0;
    } else if(interference < -0.3) {
        // Negative interference creates different note patterns
        int note = ((int)(fabs(interference) * 8) % 8);
        int octave = 1;  // Different octave for negative values
        NOTE(octave, note);
        Output->vel = (int)(fabs(interference) * 80);
        Output->accent = 0;
    } else {
        NOTEOFF();
    }
    
    // Update phases for next iteration
    PS->WaveInterference.phase1 += PS->WaveInterference.freq1 * 0.1;
    PS->WaveInterference.phase2 += PS->WaveInterference.freq2 * 0.15;
    PS->WaveInterference.phase3 += PS->WaveInterference.freq3 * 0.2;
    PS->WaveInterference.global_phase += 0.05 + (T->intensity / 1000.0);
    
    Output->note = ScaleToNote(&SN, T, P, S);
    RandomSlideAndLength(Output, R);
}

const PatternFunctions Algo_WaveInterference = {
    Algo_WaveInterference_Gen,
    Algo_WaveInterference_Init,
    NoPatternInit,
    0
};
```

## 5. DNA Sequence Algorithm

```c
// DNA Sequence Algorithm Implementation
typedef struct PatternStruct_Algo_DNA {
    uint8_t genome[16];    // Musical "genome"
    uint8_t mutation_rate; // Probability of mutation
    uint8_t expression_pos; // Current position in genome expression
    uint8_t replication_pos; // Position in replication cycle
    uint8_t crossover_point; // Point for genetic crossover
} PatternStruct_Algo_DNA;

void Algo_DNA_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                  Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Initialize genome based on seeds
    for(int i = 0; i < 16; i++) {
        Output->DNA.genome[i] = (T->seed1 + i * T->seed2 + Tuesday_Rand(R)) % 24;  // 24 possible values
    }
    
    Output->DNA.mutation_rate = 5 + (T->intensity / 10);  // 5-30% mutation rate
    Output->DNA.expression_pos = 0;
    Output->DNA.replication_pos = 0;
    Output->DNA.crossover_point = 8 + (T->seed1 % 7);
}

void Algo_DNA_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                 Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);
    
    // Apply mutations based on rate
    if(Tuesday_PercChance(R, PS->DNA.mutation_rate)) {
        int pos = Tuesday_Rand(R) % 16;
        PS->DNA.genome[pos] = Tuesday_Rand(R) % 24;  // Mutate to new gene value
    }
    
    // Apply crossover if intensity is high enough
    if(T->intensity > 128 && Tuesday_BoolChance(R)) {
        uint8_t temp_gene = PS->DNA.genome[PS->DNA.crossover_point];
        PS->DNA.genome[PS->DNA.crossover_point] = PS->DNA.genome[PS->DNA.crossover_point + 1];
        PS->DNA.genome[PS->DNA.crossover_point + 1] = temp_gene;
    }
    
    // Express current gene
    uint8_t gene_value = PS->DNA.genome[PS->DNA.expression_pos];
    int note = gene_value % 12;  // 12 semitones
    int octave = (gene_value / 12) % 3; // 3 octaves
    
    if(gene_value > 4) {  // Threshold to prevent complete silence
        NOTE(octave, note);
        Output->vel = (gene_value * 10) % 127;
        Output->accent = (gene_value > 16) ? 1 : 0;
    } else {
        NOTEOFF();
    }
    
    // Move to next gene position
    PS->DNA.expression_pos = (PS->DNA.expression_pos + 1) % 16;
    
    // Potentially advance replication position
    if(Tuesday_BoolChance(R)) {
        PS->DNA.replication_pos = (PS->DNA.replication_pos + 1) % 16;
    }
    
    Output->note = ScaleToNote(&SN, T, P, S);
    RandomSlideAndLength(Output, R);
}

const PatternFunctions Algo_DNA = {
    Algo_DNA_Gen,
    Algo_DNA_Init,
    NoPatternInit,
    0
};
```

## 6. Turing Machine Sequencer

```c
// Turing Machine Algorithm Implementation
typedef struct PatternStruct_Algo_Turing {
    uint8_t tape[32];      // Musical tape
    uint8_t head_pos;      // Current head position
    uint8_t state;         // Current state
    uint8_t rules[8][8];   // State transition rules
    uint8_t output_note;   // Current output note
} PatternStruct_Algo_Turing;

void Algo_Turing_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                     Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Initialize tape with pattern based on seeds
    for(int i = 0; i < 32; i++) {
        Output->Turing.tape[i] = (T->seed1 + i * T->seed2 + Tuesday_Rand(R)) % 8;
    }
    
    Output->Turing.head_pos = T->seed1 % 32;
    Output->Turing.state = T->seed2 % 8;
    Output->Turing.output_note = 0;
    
    // Initialize transition rules based on seeds
    for(int state = 0; state < 8; state++) {
        for(int symbol = 0; symbol < 8; symbol++) {
            uint8_t rule = (T->seed1 + state*10 + symbol*5 + Tuesday_Rand(R)) % 256;
            Output->Turing.rules[state][symbol] = rule;
        }
    }
}

void Algo_Turing_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S, 
                    Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);
    
    // Read current tape value
    uint8_t current_symbol = PS->Turing.tape[PS->Turing.head_pos];
    
    // Apply transition rule
    uint8_t rule_output = PS->Turing.rules[PS->Turing.state][current_symbol];
    uint8_t new_symbol = rule_output & 0x7;      // Lower 3 bits: new symbol
    uint8_t new_state = (rule_output >> 3) & 0x7; // Next 3 bits: new state
    uint8_t direction = (rule_output >> 6) & 1;   // Bit 6: direction
    uint8_t write_flag = (rule_output >> 7) & 1;  // Bit 7: write flag
    
    // Optionally write new symbol to tape based on write flag
    if(write_flag) {
        PS->Turing.tape[PS->Turing.head_pos] = new_symbol;
    }
    
    // Update state
    PS->Turing.state = new_state;
    
    // Move head
    if(direction) {
        PS->Turing.head_pos = (PS->Turing.head_pos + 1) % 32;
    } else {
        PS->Turing.head_pos = (PS->Turing.head_pos - 1) % 32;
    }
    
    // Map current symbol to musical output
    int output_note = current_symbol;
    if(output_note > 0) {
        int octave = PS->Turing.state % 3;  // Use state to determine octave
        NOTE(octave, output_note % 8);
        Output->vel = (current_symbol * 15) + 40;  // Minimum velocity
        Output->accent = (PS->Turing.state > 4) ? 1 : 0;  // State affects accent
        PS->Turing.output_note = output_note;  // Store for potential use in next cycle
    } else {
        NOTEOFF();
        PS->Turing.output_note = 0;
    }
    
    Output->note = ScaleToNote(&SN, T, P, S);
    RandomSlideAndLength(Output, R);
}

const PatternFunctions Algo_Turing = {
    Algo_Turing_Gen,
    Algo_Turing_Init,
    NoPatternInit,
    0
};
```

## Integration Notes

Each algorithm follows the same structure as the original Tuesday algorithms:
1. A specific data structure for algorithm state
2. An Init function to set up initial conditions based on seeds and intensity
3. A Gen function that generates the next note/gate pair
4. A PatternFunctions struct that ties them together

These implementations are designed to work within the Tuesday/Performer architecture, using the same random generation functions, note scaling, and other utilities provided by the existing framework. The algorithms use the seed1, seed2, and intensity parameters to control their behavior, as required for the simplified Tuesday track in the Performer sequencer.