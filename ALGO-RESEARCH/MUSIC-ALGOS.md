# MUSIC-ALGOS.md

## Compatibility Information

All algorithms in this document are designed to be fully compatible with the following hardware configuration:
- **1 GATE+Pitch CV pair**: Each algorithm generates the necessary pitch CV and gate signals
- **3 External Controls**: Algorithms can be modulated by 3 external parameters

### Control Mapping Options:
- **Option 1**: Intensity (from existing algorithm parameter) / Flow / Ornamentation
- **Option 2**: Seed1 / Seed2 / Intensity (from existing algorithm parameters)
- **Option 3**: Algorithm Selection / Style Parameter / Intensity

Each algorithm maintains the tight gate+pitch coupling that defines the Tuesday module experience and works within the existing timing and scale systems.

## Music Genre and Artist Inspired Algorithms for Tuesday Module

This document contains algorithm implementations inspired by various music genres and artists, following the Tuesday module architecture and conventions. Each algorithm is designed to generate authentic musical patterns characteristic of its specific style while maintaining the gate+pitch coupling concept.

## 1. Ambient/Ethereal Algorithm (Vangelis/Tangerine Dream inspired)

```c
// Ambient atmospheric pad algorithm with evolving textures
typedef struct PatternStruct_Algo_Ambient {
    float phase;              // Main oscillator phase
    float phase2;             // Secondary oscillator phase
    float filter_freq;        // Filter frequency for evolving textures
    uint8_t note_buffer[8];   // Buffer for sustained tones
    uint8_t buffer_pos;       // Current position in note buffer
    uint8_t sustain_count;    // Counter for sustained notes
} PatternStruct_Algo_Ambient;

void Algo_Ambient_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                      Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    Output->Ambient.phase = 0;
    Output->Ambient.phase2 = T->seed1 * 0.1;
    Output->Ambient.filter_freq = 0.2 + (T->seed2 % 80) / 400.0;  // Slow evolution
    
    // Initialize note buffer with sustained tones
    for(int i = 0; i < 8; i++) {
        Output->Ambient.note_buffer[i] = (T->seed1 + i * 3 + Tuesday_Rand(R)) % 8;
    }
    
    Output->Ambient.buffer_pos = 0;
    Output->Ambient.sustain_count = 0;
}

void Algo_Ambient_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                     Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);

    // Update phases slowly to create evolving textures
    PS->Ambient.phase += 0.1 + (T->intensity / 2000.0);
    PS->Ambient.phase2 += 0.05 + (T->seed1 / 5000.0);
    
    // Calculate evolving pitch based on both phases
    float pitch1 = sin(PS->Ambient.phase);
    float pitch2 = cos(PS->Ambient.phase2 * 1.618);  // Golden ratio for organic feel
    float combined = (pitch1 + pitch2) / 2.0;
    
    // Map to sustained notes with occasional movement
    if(Tuesday_PercChance(R, 40 + (T->intensity / 4))) {  // New note occasionally
        PS->Ambient.buffer_pos = (PS->Ambient.buffer_pos + 1) % 8;
    }
    
    int note = PS->Ambient.note_buffer[PS->Ambient.buffer_pos];
    int octave = 0;  // Lower octaves for ambient feel
    
    // Add microtonal variations for ethereal quality
    if(fabs(combined) > 0.3) {
        NOTE(octave, note);
        Output->vel = 40 + (T->intensity / 3) + (int)(fabs(combined) * 40);
        Output->accent = 0;  // No accents for ambient feel
        Output->slide = 1;   // Always slide for smooth transitions
    } else {
        // Sustain current note with lower velocity occasionally
        if(Tuesday_PercChance(R, 20)) {
            NOTE(octave, PS->Ambient.note_buffer[PS->Ambient.buffer_pos]);
            Output->vel = 20 + (int)(fabs(combined) * 30);
            Output->accent = 0;
            Output->slide = 1;
        } else {
            NOTEOFF();
        }
    }

    Output->note = ScaleToNote(&SN, T, P, S);
    
    // Apply random slide and length for organic feel
    if(Tuesday_PercChance(R, 10)) {
        Output->maxsubticklength = ((1 + (Tuesday_Rand(R) % 6)) * TUESDAY_SUBTICKRES) - 2;
    }
}
```

## 2. Techno/Rave Algorithm (Kraftwerk/Aphex Twin inspired)

```c
// Driving techno patterns with syncopated rhythms
typedef struct PatternStruct_Algo_Techno {
    uint8_t pattern_step;     // Current step in pattern
    uint8_t bass_pattern[8];  // Bass note pattern
    uint8_t hi_hat_pattern[8]; // Hi-hat pattern
    uint8_t accent_pattern[8]; // Accent pattern
    uint8_t pattern_type;     // Current pattern variation
    uint8_t syncopation;      // Syncopation level
} PatternStruct_Algo_Techno;

void Algo_Techno_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                     Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    Output->Techno.pattern_step = 0;
    Output->Techno.pattern_type = (T->seed1 + T->seed2) % 4;  // 4 pattern types
    Output->Techno.syncopation = 2 + (T->intensity / 64);     // 2-6 syncopation
    
    // Initialize driving bass pattern
    for(int i = 0; i < 8; i++) {
        Output->Techno.bass_pattern[i] = (Tuesday_Rand(R) % 3);  // Low notes 0-2
        Output->Techno.hi_hat_pattern[i] = (Tuesday_Rand(R) % 3) < 2 ? 1 : 0;  // Hihat pattern
        Output->Techno.accent_pattern[i] = (i % (Output->Techno.syncopation)) == 0 ? 1 : 0;
    }
    
    // Make sure first step always has a bass note
    Output->Techno.bass_pattern[0] = 0;
    Output->Techno.accent_pattern[0] = 1;
}

void Algo_Techno_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                     Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);

    int step = I % 8;
    
    // Reset pattern every 8 steps or based on intensity
    if(step == 0 && Tuesday_PercChance(R, 10 + (T->intensity / 10))) {
        PS->Techno.pattern_type = (PS->Techno.pattern_type + 1) % 4;
    }
    
    // Determine what to play based on current step and pattern
    if(PS->Techno.bass_pattern[step] < 3) {
        // Bass note
        NOTE(0, PS->Techno.bass_pattern[step]);
        Output->vel = 90 + (T->intensity / 4);  // Strong bass
        Output->accent = PS->Techno.accent_pattern[step];
    } else if(PS->Techno.hi_hat_pattern[step]) {
        // High hat or percussive element
        NOTE(2, Tuesday_Rand(R) % 3 + 5);  // High percussive notes
        Output->vel = 60 + (Tuesday_Rand(R) % 40);  // Varying velocity
        Output->accent = 0;
    } else {
        // Rest or minimal element
        if(Tuesday_PercChance(R, 30 + (T->intensity / 8))) {
            // Occasional high element for interest
            NOTE(1, Tuesday_Rand(R) % 4);
            Output->vel = 40 + (Tuesday_Rand(R) % 30);
            Output->accent = 0;
        } else {
            NOTEOFF();
        }
    }
    
    // Apply slide occasionally for techno glide effects
    if(Tuesday_PercChance(R, 8 + (T->intensity / 20)) && Output->vel > 0) {
        Output->slide = 1;
    }

    Output->note = ScaleToNote(&SN, T, P, S);
    
    // Short note lengths for tight techno feel
    if(Output->vel > 0) {
        Output->maxsubticklength = ((1 + (Tuesday_Rand(R) % 3)) * TUESDAY_SUBTICKRES) - 2;
    }
}
```

## 3. Jazz Improvisation Algorithm (Miles Davis/John Coltrane inspired)

```c
// Jazz improvisation with chord tones and blue notes
typedef struct PatternStruct_Algo_Jazz {
    uint8_t chord_tones[6];   // Current chord tones
    uint8_t scale_pattern[8]; // Scale pattern for movement
    uint8_t phrase_start;     // Start of current phrase
    uint8_t phrase_length;    // Length of current phrase
    uint8_t swing_factor;     // Swing feel factor
    uint8_t current_note;     // Current note in phrase
    uint8_t blue_note_chance; // Probability of blue notes
} PatternStruct_Algo_Jazz;

void Algo_Jazz_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                   Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Determine chord tones based on scale and seeds
    for(int i = 0; i < 6; i++) {
        Output->Jazz.chord_tones[i] = (i * 2) % 8;  // Major scale degrees
    }
    
    // Add blue note
    Output->Jazz.chord_tones[3] = 3;  // Blue note (flattened 5th)
    
    // Initialize scale pattern based on seed
    for(int i = 0; i < 8; i++) {
        Output->Jazz.scale_pattern[i] = (T->seed1 + i * 3) % 8;
    }
    
    Output->Jazz.phrase_start = 0;
    Output->Jazz.phrase_length = 4 + (T->seed2 % 5);  // 4-8 notes per phrase
    Output->Jazz.swing_factor = 2 + (T->intensity / 64);  // 2-6 swing
    Output->Jazz.current_note = 0;
    Output->Jazz.blue_note_chance = 15 + (T->intensity / 8);  // 15-30% blue notes
}

void Algo_Jazz_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                  Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);

    // Determine if we're starting a new phrase
    if(PS->Jazz.current_note >= PS->Jazz.phrase_length) {
        PS->Jazz.current_note = 0;
        PS->Jazz.phrase_start = Tuesday_Rand(R) % 8;
        
        // Occasionally change chord tones
        if(Tuesday_PercChance(R, 20)) {
            // Simulate chord change by shifting chord tones
            for(int i = 0; i < 6; i++) {
                PS->Jazz.chord_tones[i] = (PS->Jazz.chord_tones[i] + (Tuesday_Rand(R) % 3) - 1) % 8;
            }
        }
    }
    
    // Choose between chord tones and scale tones
    int note;
    int octave = 1;  // Jazz typically has melodic interest in middle register
    
    if(Tuesday_PercChance(R, 60)) {
        // Use chord tone
        int chord_idx = Tuesday_Rand(R) % 6;
        note = PS->Jazz.chord_tones[chord_idx];
        
        // Occasionally use blue note
        if(Tuesday_PercChance(R, PS->Jazz.blue_note_chance)) {
            note = 3;  // Blue note
        }
    } else {
        // Use scale tone with some variation
        int scale_idx = (PS->Jazz.current_note + PS->Jazz.phrase_start) % 8;
        note = PS->Jazz.scale_pattern[scale_idx];
    }
    
    // Apply swing feel by occasionally lengthening notes
    if((I % PS->Jazz.swing_factor) == 0 && Output->vel > 0) {
        Output->maxsubticklength = ((2 + (Tuesday_Rand(R) % 3)) * TUESDAY_SUBTICKRES) - 2;
    }
    
    // Play the note with jazz-like articulation
    NOTE(octave, note);
    Output->vel = 70 + (Tuesday_Rand(R) % 50);  // Variable velocity for expression
    Output->accent = Tuesday_PercChance(R, 20 + (T->intensity / 10));  // Accents for expression
    
    // Apply slide occasionally for jazz gliding
    if(Tuesday_PercChance(R, 15) && PS->Jazz.current_note > 0) {
        Output->slide = 1;
    }
    
    PS->Jazz.current_note++;

    Output->note = ScaleToNote(&SN, T, P, S);
}
```

## 4. Classical Counterpoint Algorithm (Bach inspired)

```c
// Classical counterpoint with independent melodic lines
typedef struct PatternStruct_Algo_Counterpoint {
    uint8_t voice1_pattern[16];  // Main melodic voice
    uint8_t voice2_pattern[16];  // Secondary voice
    uint8_t current_voice;       // Current active voice
    uint8_t interval_type;       // Current interval relationship
    uint8_t resolution_count;    // Counter for voice resolution
    uint8_t last_note[2];        // Last note of each voice
    uint8_t voice_octave[2];     // Octave for each voice
} PatternStruct_Algo_Counterpoint;

void Algo_Counterpoint_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                           Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Initialize main melodic voice
    for(int i = 0; i < 16; i++) {
        Output->Counterpoint.voice1_pattern[i] = (T->seed1 + i * 2) % 8;
        Output->Counterpoint.voice2_pattern[i] = (T->seed2 + i * 3) % 8;
    }
    
    Output->Counterpoint.current_voice = 0;
    Output->Counterpoint.interval_type = 0;  // Start with unison
    Output->Counterpoint.resolution_count = 0;
    Output->Counterpoint.last_note[0] = 0;
    Output->Counterpoint.last_note[1] = 4;  // Start 5th apart
    Output->Counterpoint.voice_octave[0] = 0;  // Lower voice
    Output->Counterpoint.voice_octave[1] = 1;  // Higher voice
}

void Algo_Counterpoint_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                          Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);

    // Determine which voice to play based on classical voice leading
    if(Tuesday_PercChance(R, 70 - (T->intensity / 8))) {  // 70-55% chance for main voice
        PS->Counterpoint.current_voice = 0;
    } else {
        PS->Counterpoint.current_voice = 1;
    }
    
    int voice_idx = PS->Counterpoint.current_voice;
    
    // Get note from pattern
    int note, octave;
    if(voice_idx == 0) {
        note = PS->Counterpoint.voice1_pattern[I % 16];
        octave = PS->Counterpoint.voice_octave[0];
    } else {
        note = PS->Counterpoint.voice2_pattern[I % 16];
        octave = PS->Counterpoint.voice_octave[1];
    }
    
    // Apply classical voice leading rules
    int last_note = PS->Counterpoint.last_note[voice_idx];
    int interval = abs(note - last_note);
    
    // Avoid parallel fifths and octaves if possible
    if(interval == 5 && PS->Counterpoint.interval_type == 5) {
        // Adjust the note to avoid parallel fifths
        note = (note + 1) % 8;
    } else if(interval == 0 && PS->Counterpoint.interval_type == 0) {
        // Avoid parallel octaves
        note = (note + 2) % 8;
    }
    
    PS->Counterpoint.interval_type = interval;
    PS->Counterpoint.last_note[voice_idx] = note;
    
    // Play the note with classical articulation
    NOTE(octave, note);
    Output->vel = 65 + (Tuesday_Rand(R) % 40);  // Moderate velocity
    Output->accent = Tuesday_PercChance(R, 15 + (T->intensity / 15));  // Selective accents
    
    // Classical music typically has smooth transitions
    if(Tuesday_PercChance(R, 25)) {
        Output->slide = 1;
    }
    
    // Standard classical note length
    Output->maxsubticklength = ((2 + (Tuesday_Rand(R) % 4)) * TUESDAY_SUBTICKRES) - 2;

    Output->note = ScaleToNote(&SN, T, P, S);
}
```

## 5. Minimalist Repetition Algorithm (Steve Reich inspired)

```c
// Minimalist phasing patterns in the style of Steve Reich
typedef struct PatternStruct_Algo_Minimalist {
    uint8_t base_pattern[8];    // Base pattern
    uint8_t pattern_length;     // Length of base pattern
    uint8_t phase1;             // Phase position for voice 1
    uint8_t phase2;             // Phase position for voice 2
    uint8_t shift_amount;       // Amount to shift phase
    uint8_t shift_counter;      // Counter for phase shifting
    uint8_t current_note;       // Current note being played
    uint8_t accent_pattern[4];  // Pattern of accents
} PatternStruct_Algo_Minimalist;

void Algo_Minimalist_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                         Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Initialize base pattern based on seeds
    for(int i = 0; i < 8; i++) {
        Output->Minimalist.base_pattern[i] = (T->seed1 + i) % 6;  // Limited range for minimalism
    }
    
    Output->Minimalist.pattern_length = 6;  // Common minimalism pattern length
    Output->Minimalist.phase1 = 0;
    Output->Minimalist.phase2 = 0;  // Both start in sync
    Output->Minimalist.shift_amount = 1 + (T->intensity / 128);   // 1-3 shift amount
    Output->Minimalist.shift_counter = 0;
    Output->Minimalist.current_note = 0;
    
    // Initialize accent pattern for pulse
    for(int i = 0; i < 4; i++) {
        Output->Minimalist.accent_pattern[i] = (i == 0) ? 1 : 0;  // Strong downbeat
    }
}

void Algo_Minimalist_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                        Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);

    // Update phases - shift voice 2 slightly faster for phasing effect
    PS->Minimalist.phase2 = (PS->Minimalist.phase2 + 1) % PS->Minimalist.pattern_length;
    PS->Minimalist.shift_counter++;
    
    // Apply phase shift based on intensity (more intense = faster phasing)
    if(PS->Minimalist.shift_counter >= (64 - (T->intensity / 4))) {
        PS->Minimalist.phase1 = (PS->Minimalist.phase1 + PS->Minimalist.shift_amount) % PS->Minimalist.pattern_length;
        PS->Minimalist.shift_counter = 0;
    }
    
    // Get note from base pattern at current phase
    int note = PS->Minimalist.base_pattern[PS->Minimalist.phase1];
    
    // Occasionally play both phases simultaneously
    if(Tuesday_PercChance(R, 10 + (T->intensity / 15))) {
        // Get note from second phase as harmony
        int harmony_note = PS->Minimalist.base_pattern[PS->Minimalist.phase2];
        if(abs(note - harmony_note) > 1) {
            note = harmony_note;  // Occasionally play harmony note
        }
    }
    
    // Play the note with minimal articulation
    NOTE(0, note);
    Output->vel = 70 + (Tuesday_Rand(R) % 30);  // Consistent velocity
    Output->accent = PS->Minimalist.accent_pattern[(I % 4)];  // Regular pulse
    
    // Minimal slides for smooth transitions
    if(Tuesday_PercChance(R, 8)) {
        Output->slide = 1;
    }
    
    // Consistent note lengths for minimalist feel
    Output->maxsubticklength = ((2 + (Tuesday_Rand(R) % 2)) * TUESDAY_SUBTICKRES) - 2;

    Output->note = ScaleToNote(&SN, T, P, S);
}
```

## 6. Breakbeat/DnB Algorithm (LTJ Bukem/Aphrodite inspired)

```c
// Complex breakbeat patterns with syncopated bass
typedef struct PatternStruct_Algo_Breakbeat {
    uint8_t bass_line[16];      // Complex bass pattern
    uint8_t snare_pattern[16];  // Snare/backbeat pattern
    uint8_t hi_hat_pattern[16]; // Hi-hat pattern
    uint8_t pattern_index;      // Current position in pattern
    uint8_t fill_probability;   // Probability of fills
    uint8_t double_time;        // Double-time patterns
    uint8_t syncopation_level;  // Level of syncopation
} PatternStruct_Algo_Breakbeat;

void Algo_Breakbeat_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                        Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Initialize breakbeat patterns
    for(int i = 0; i < 16; i++) {
        // Bass pattern - emphasize off-beats in DnB style
        Output->Breakbeat.bass_line[i] = 0;
        if((i % 4) == 0 || i == 2 || i == 6 || i == 10 || i == 14) {
            Output->Breakbeat.bass_line[i] = Tuesday_Rand(R) % 3;  // Bass notes on strong beats + offbeats
        }
        
        // Snare pattern - classic breakbeat snare positions
        Output->Breakbeat.snare_pattern[i] = 0;
        if(i == 4 || i == 12) {
            Output->Breakbeat.snare_pattern[i] = 1;
        }
        
        // Hi-hat pattern - fast, syncopated pattern
        Output->Breakbeat.hi_hat_pattern[i] = Tuesday_BoolChance(R) ? 1 : 0;
    }
    
    // Make sure first beat has bass
    Output->Breakbeat.bass_line[0] = 0;
    
    Output->Breakbeat.pattern_index = 0;
    Output->Breakbeat.fill_probability = 10 + (T->intensity / 12);  // 10-30% fills
    Output->Breakbeat.double_time = 0;
    Output->Breakbeat.syncopation_level = 5 + (T->intensity / 25);  // 5-9 syncopation
}

void Algo_Breakbeat_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                       Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);

    int idx = I % 16;
    PS->Breakbeat.pattern_index = idx;
    
    // Determine what to play based on current step
    if(PS->Breakbeat.bass_line[idx] < 3) {
        // Bass note (DnB style - aggressive, syncopated)
        NOTE(0, PS->Breakbeat.bass_line[idx]);
        Output->vel = 95 + (Tuesday_Rand(R) % 30);  // Strong bass
        Output->accent = Tuesday_PercChance(R, 30);  // Some accents on bass
    } else if(PS->Breakbeat.snare_pattern[idx]) {
        // Snare/backbeat
        NOTE(2, 5);  // Mid-high percussion note
        Output->vel = 80 + (Tuesday_Rand(R) % 40);
        Output->accent = 1;
    } else if(PS->Breakbeat.hi_hat_pattern[idx]) {
        // Hi-hat pattern
        NOTE(2, 7);  // High percussion note
        Output->vel = 50 + (Tuesday_Rand(R) % 25);
        Output->accent = 0;
    } else if(Tuesday_PercChance(R, 8)) {
        // Occasional rimshot or percussion fill
        NOTE(1, 4 + (Tuesday_Rand(R) % 3));
        Output->vel = 60 + (Tuesday_Rand(R) % 30);
        Output->accent = Tuesday_BoolChance(R);
    } else {
        NOTEOFF();
    }
    
    // Apply DnB-style slides occasionally
    if(Tuesday_PercChance(R, 12) && Output->note != TUESDAY_NOTEOFF) {
        Output->slide = 1;
    }
    
    // DnB has varied note lengths but tight feel
    if(Output->vel > 0) {
        Output->maxsubticklength = ((1 + (Tuesday_Rand(R) % 3)) * TUESDAY_SUBTICKRES) - 2;
    }
    
    // Occasionally apply fills based on intensity
    if(Tuesday_PercChance(R, PS->Breakbeat.fill_probability) && Output->vel > 0) {
        Output->accent = 1;
        Output->vel += 20;  // Emphasize fills
    }

    Output->note = ScaleToNote(&SN, T, P, S);
}
```

## 7. Droning/Bowed Strings Algorithm (Eliane Radigue/La Monte Young inspired)

```c
// Sustained drone textures with microtonal variations
typedef struct PatternStruct_Algo_Drone {
    uint8_t drone_note;         // Main drone note
    uint8_t harmonic_notes[4];  // Harmonic overtones
    uint8_t current_harmony;    // Current harmony element
    float microtone_offset;     // Microtonal offset
    uint8_t sustain_mode;       // Sustain behavior
    uint8_t texture_change;     // Rate of texture change
    uint8_t last_octave;        // Previous octave for smoothness
} PatternStruct_Algo_Drone;

void Algo_Drone_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                    Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Initialize drone based on seeds
    Output->Drone.drone_note = T->seed1 % 3;  // Root note 0-2 for drone feel
    Output->Drone.last_octave = 0;
    
    // Set up harmonic overtones
    Output->Drone.harmonic_notes[0] = (Output->Drone.drone_note + 4) % 8;  // Major third
    Output->Drone.harmonic_notes[1] = (Output->Drone.drone_note + 7) % 8;  // Perfect fifth
    Output->Drone.harmonic_notes[2] = (Output->Drone.drone_note + 12) % 8; // Octave
    Output->Drone.harmonic_notes[3] = (Output->Drone.drone_note + 2) % 8;  // Major second
    
    Output->Drone.current_harmony = 0;
    Output->Drone.microtone_offset = (T->seed2 % 100) / 1000.0;  // Small microtonal shift
    Output->Drone.sustain_mode = 1;
    Output->Drone.texture_change = 5 + (T->intensity / 25);  // 5-9% change rate
}

void Algo_Drone_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                   Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);

    // Decide whether to play drone, harmony, or rest
    int choice = Tuesday_Rand(R) % 100;
    
    if(choice < (60 + (T->intensity / 6))) {
        // Play main drone note
        NOTE(PS->Drone.last_octave, PS->Drone.drone_note);
        Output->vel = 75 + (Tuesday_Rand(R) % 20);  // Consistent volume
        Output->accent = 0;  // No accents for drone
        Output->slide = 1;   // Always slide for string feel
    } else if(choice < (85 + (T->intensity / 8))) {
        // Play harmonic overtone
        int harmony_idx = Tuesday_Rand(R) % 4;
        NOTE(PS->Drone.last_octave, PS->Drone.harmonic_notes[harmony_idx]);
        Output->vel = 50 + (Tuesday_Rand(R) % 25);  // Softer harmony
        Output->accent = 0;
        Output->slide = 1;
    } else if(Tuesday_PercChance(R, PS->Drone.texture_change)) {
        // Change octave occasionally
        int octave_choice = Tuesday_Rand(R) % 2;  // 0 or 1
        if(octave_choice != PS->Drone.last_octave) {
            PS->Drone.last_octave = octave_choice;
        }
        
        // Play note in new octave
        NOTE(PS->Drone.last_octave, PS->Drone.drone_note);
        Output->vel = 65 + (Tuesday_Rand(R) % 20);
        Output->accent = 0;
        Output->slide = 1;
    } else {
        // Rest occasionally for texture
        if(Tuesday_PercChance(R, 10)) {
            NOTEOFF();
        } else {
            // Play drone note at lower velocity
            NOTE(PS->Drone.last_octave, PS->Drone.drone_note);
            Output->vel = 30 + (Tuesday_Rand(R) % 20);
            Output->accent = 0;
            Output->slide = 1;
        }
    }
    
    // Very long note lengths for drone effect
    Output->maxsubticklength = ((6 + (Tuesday_Rand(R) % 4)) * TUESDAY_SUBTICKRES) - 2;

    Output->note = ScaleToNote(&SN, T, P, S);
}
```

## 8. Arpeggiated Patterns Algorithm (Vangelis/Tangerine Dream inspired)

```c
// Lush evolving arpeggios in the style of vintage synth music
typedef struct PatternStruct_Algo_Arpeggio {
    uint8_t chord_notes[6];     // Current chord notes
    uint8_t arpeggio_pattern[8]; // Arpeggio sequence
    uint8_t current_note_pos;   // Current position in arpeggio
    uint8_t arpeggio_direction; // Direction: 0=up, 1=down, 2=up/down
    uint8_t pattern_speed;      // Speed of pattern movement
    uint8_t chord_change_rate;  // Rate of chord changes
    uint8_t velocity_curve;     // How velocity changes in pattern
} PatternStruct_Algo_Arpeggio;

void Algo_Arpeggio_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                       Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Set up initial chord based on seeds
    for(int i = 0; i < 6; i++) {
        Output->Arpeggio.chord_notes[i] = (i * 2) % 8;  // Basic chord
    }
    
    // Add extended chord tones
    Output->Arpeggio.chord_notes[3] = 4;  // 7th
    Output->Arpeggio.chord_notes[4] = 6;  // 9th
    Output->Arpeggio.chord_notes[5] = 3;  // 6th
    
    // Create arpeggio pattern based on seed
    for(int i = 0; i < 8; i++) {
        Output->Arpeggio.arpeggio_pattern[i] = i % 6;  // Cycle through chord notes
    }
    
    Output->Arpeggio.current_note_pos = 0;
    Output->Arpeggio.arpeggio_direction = T->seed1 % 3;
    Output->Arpeggio.pattern_speed = 1;
    Output->Arpeggio.chord_change_rate = 15 + (T->intensity / 10);  // 15-40% rate
    Output->Arpeggio.velocity_curve = 0;
}

void Algo_Arpeggio_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                      Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);

    // Select note from arpeggio pattern
    int pattern_idx = PS->Arpeggio.current_note_pos % 8;
    int chord_idx = PS->Arpeggio.arpeggio_pattern[pattern_idx];
    
    // Apply direction pattern
    if(PS->Arpeggio.arpeggio_direction == 2 && (PS->Arpeggio.current_note_pos / 8) % 2) {
        // For up/down pattern, reverse direction on odd cycles
        chord_idx = 5 - chord_idx;
    } else if(PS->Arpeggio.arpeggio_direction == 1) {
        // For down pattern, reverse all
        chord_idx = 5 - chord_idx;
    }
    
    int note = PS->Arpeggio.chord_notes[chord_idx % 6];
    int octave = 1;  // Middle register for lush arpeggios
    
    // Occasionally add chord movement based on intensity
    if(Tuesday_PercChance(R, PS->Arpeggio.chord_change_rate)) {
        // Shift chord notes for progression
        for(int i = 0; i < 6; i++) {
            PS->Arpeggio.chord_notes[i] = (PS->Arpeggio.chord_notes[i] + 1) % 8;
        }
    }
    
    // Play the arpeggio note
    NOTE(octave, note);
    Output->vel = 60 + (Tuesday_Rand(R) % 50);  // Variable but substantial velocity
    Output->accent = (pattern_idx % 4) == 0 ? 1 : 0;  // Accent every 4th note
    
    // Apply slide for smooth arpeggio feel
    if(Tuesday_PercChance(R, 40)) {
        Output->slide = 1;
    }
    
    // Fast arpeggio timing
    Output->maxsubticklength = ((1 + (Tuesday_Rand(R) % 2)) * TUESDAY_SUBTICKRES) - 2;
    
    PS->Arpeggio.current_note_pos++;

    Output->note = ScaleToNote(&SN, T, P, S);
}
```

## 9. Funk Rhythm Algorithm (James Brown/Parliament Funkadelic inspired)

```c
// Syncopated funk rhythms with backbeat emphasis
typedef struct PatternStruct_Algo_Funk {
    uint8_t bass_pattern[16];   // Syncopated bass pattern
    uint8_t rhythm_pattern[16]; // Rhythm guitar pattern
    uint8_t accent_pattern[16]; // Accent pattern for backbeat
    uint8_t pattern_type;       // Current pattern variation
    uint8_t syncopation_level;  // Level of syncopation
    int8_t last_note;           // Last played note
    uint8_t fill_counter;       // Counter for fills
} PatternStruct_Algo_Funk;

void Algo_Funk_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                   Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Initialize syncopated funk patterns
    for(int i = 0; i < 16; i++) {
        // Funk bass - emphasizes upbeats and syncopation
        Output->Funk.bass_pattern[i] = 0;  // Start with root
        
        // Emphasize upbeats in funk style
        if(i == 2 || i == 6 || i == 10 || i == 14) {
            Output->Funk.bass_pattern[i] = Tuesday_Rand(R) % 3;  // Syncopated bass
        } else if(i == 0 || i == 8) {
            Output->Funk.bass_pattern[i] = 0;  // Strong downbeats
        }
        
        // Rhythm pattern - chord stabs and rhythmic elements
        Output->Funk.rhythm_pattern[i] = Tuesday_BoolChance(R) ? 1 : 0;
        
        // Accent pattern for classic funk backbeat
        Output->Funk.accent_pattern[i] = 0;  // Start with no accents
    }
    
    // Add classic funk backbeat (2 and 4)
    Output->Funk.accent_pattern[4] = 1;  // Second beat
    Output->Funk.accent_pattern[12] = 1; // Fourth beat
    
    Output->Funk.pattern_type = 0;
    Output->Funk.syncopation_level = 20 + (T->intensity / 6);  // 20-45% syncopation
    Output->Funk.last_note = -1;
    Output->Funk.fill_counter = 0;
}

void Algo_Funk_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                  Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);

    int idx = I % 16;
    
    // Determine what to play based on funk patterns
    if(PS->Funk.bass_pattern[idx] < 3) {
        // Syncopated bass
        NOTE(0, PS->Funk.bass_pattern[idx]);
        Output->vel = 80 + (Tuesday_Rand(R) % 35);  // Strong bass
        Output->accent = PS->Funk.accent_pattern[idx];  // Backbeat accents
    } else if(PS->Funk.rhythm_pattern[idx] && Tuesday_PercChance(R, 60)) {
        // Rhythm chord stabs (higher notes for rhythmic elements)
        NOTE(1, 4 + (Tuesday_Rand(R) % 3));
        Output->vel = 60 + (Tuesday_Rand(R) % 30);
        Output->accent = PS->Funk.accent_pattern[idx];
    } else if(Tuesday_PercChance(R, PS->Funk.syncopation_level)) {
        // Funk fills and rhythmic elements
        NOTE(0, Tuesday_Rand(R) % 4);  // Low rhythmic note
        Output->vel = 70 + (Tuesday_Rand(R) % 25);
        Output->accent = 0;
    } else {
        // Funk is about the spaces too - occasional rests
        if(Tuesday_PercChance(R, 30)) {
            NOTEOFF();
        } else {
            // Play root note softly
            NOTE(0, 0);
            Output->vel = 25 + (Tuesday_Rand(R) % 20);
            Output->accent = 0;
        }
    }
    
    // Funk has tight, percussive articulation
    if(Output->note != TUESDAY_NOTEOFF && Tuesday_PercChance(R, 10)) {
        Output->slide = 1;
    }
    
    // Funk has tight, short note lengths
    if(Output->vel > 0) {
        Output->maxsubticklength = ((1 + (Tuesday_Rand(R) % 2)) * TUESDAY_SUBTICKRES) - 2;
    }
    
    // Apply fills on pattern boundaries
    PS->Funk.fill_counter++;
    if(PS->Funk.fill_counter == 16) {
        PS->Funk.fill_counter = 0;
        if(Tuesday_PercChance(R, 20)) {
            // Full pattern accent
            Output->accent = 1;
            Output->vel += 15;
        }
    }

    Output->note = ScaleToNote(&SN, T, P, S);
}
```

## 10. Indian Classical Raga Algorithm (Ravi Shankar/Hariprasad Chaurasia inspired)

```c
// Indian classical raga patterns with traditional melodic movement
typedef struct PatternStruct_Algo_Raga {
    uint8_t raga_notes[7];      // The seven notes of the raga
    uint8_t arohana[8];         // Ascending scale pattern
    uint8_t avarohana[8];       // Descending scale pattern
    uint8_t current_phrase;     // Current melodic phrase
    uint8_t phrase_pattern[16]; // Pattern of phrases
    uint8_t emphasis_note;      // Note for emphasis (vadi/samvadi)
    uint8_t tala_position;      // Position in Indian rhythmic cycle
    uint8_t gamaka_probability; // Probability of ornamental slides
} PatternStruct_Algo_Raga;

void Algo_Raga_Init(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                   Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *Output) {
    // Initialize raga notes based on Indian classical tradition
    // Using 7-note scale: Sa Re Ga Ma Pa Dha Ni (Sa)
    for(int i = 0; i < 7; i++) {
        Output->Raga.raga_notes[i] = i;  // Standard positions
    }
    
    // Create ascending pattern
    for(int i = 0; i < 8; i++) {
        Output->Raga.arohana[i] = (i < 7) ? Output->Raga.raga_notes[i] : 0;  // Return to Sa
    }
    
    // Create descending pattern (traditional Indian classical)
    for(int i = 0; i < 8; i++) {
        if(i < 4) {
            Output->Raga.avarohana[i] = Output->Raga.raga_notes[6-i];  // Ni to Ma
        } else if(i == 4) {
            Output->Raga.avarohana[i] = Output->Raga.raga_notes[3];    // Ma
        } else {
            Output->Raga.avarohana[i] = Output->Raga.raga_notes[2-i+5]; // Re to Sa
        }
    }
    
    // Initialize phrase patterns
    for(int i = 0; i < 16; i++) {
        Output->Raga.phrase_pattern[i] = Tuesday_Rand(R) % 8;
    }
    
    Output->Raga.current_phrase = 0;
    Output->Raga.emphasis_note = 4;  // Emphasize Pa (fifth) as in many ragas
    Output->Raga.tala_position = 0;  // Start of rhythmic cycle
    Output->Raga.gamaka_probability = 25 + (T->intensity / 6);  // 25-50% ornaments
}

void Algo_Raga_Gen(Tuesday_PatternGen *T, Tuesday_Params *P, Tuesday_Settings *S,
                  Tuesday_RandomGen *R, Tuesday_PatternFuncSpecific *PS, int I, Tuesday_Tick_t *Output) {
    struct ScaledNote SN;
    DefaultTick(Output);

    int idx = I % 16;
    PS->Raga.tala_position = idx;
    
    // Determine melodic movement based on position in raga
    int choice = Tuesday_Rand(R) % 100;
    int note;
    int octave = 1;  // Middle register for traditional melodic lines
    
    if(choice < 40) {
        // Ascending movement (arohana)
        int pattern_idx = PS->Raga.current_phrase % 8;
        note = PS->Raga.arohana[pattern_idx];
        PS->Raga.current_phrase++;
    } else if(choice < 75) {
        // Descending movement (avarohana)
        int pattern_idx = PS->Raga.current_phrase % 8;
        note = PS->Raga.avarohana[pattern_idx];
        PS->Raga.current_phrase--;
        if(PS->Raga.current_phrase > 100) PS->Raga.current_phrase = 7;  // Handle underflow
    } else {
        // Emphasis on important notes (vadi/samvadi)
        note = PS->Raga.emphasis_note;
    }
    
    // Apply traditional Indian classical ornamentation
    NOTE(octave, note);
    Output->vel = 65 + (Tuesday_Rand(R) % 30);  // Moderate and expressive
    Output->accent = (note == PS->Raga.emphasis_note) ? 1 : 0;  // Emphasize important notes
    
    // Apply gamaka (ornamental slides) based on intensity
    if(Tuesday_PercChance(R, PS->Raga.gamaka_probability)) {
        Output->slide = 1;
    }
    
    // Apply rhythmic emphasis at traditional points in 16-step cycle
    if((idx % 8) == 0) {  // Emphasize every 8 steps (like tala)
        Output->accent = 1;
        Output->vel += 10;
    }
    
    // Variable note lengths for expressive feel
    Output->maxsubticklength = ((2 + (Tuesday_Rand(R) % 3)) * TUESDAY_SUBTICKRES) - 2;

    Output->note = ScaleToNote(&SN, T, P, S);
}
```

## Integration Notes

These music genre and artist inspired algorithms follow the same structure as the original Tuesday algorithms:

1. Each algorithm has a dedicated state structure containing required variables
2. An Init function that sets up initial conditions based on seeds and intensity
3. A Gen function that generates the next note/gate pair following the musical style
4. All algorithms work within the core Tuesday framework using the same utilities

Each algorithm captures the essential musical characteristics of its respective genre/style while maintaining the tight gate+pitch coupling that defines the Tuesday module experience. The algorithms respond to intensity parameters to vary complexity and expression while preserving the authentic feel of each musical style.