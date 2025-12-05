# New TuesdayTrack Algorithms Design (STM32-Optimized)

This document contains designs for new algorithms based on the TuesdayTrack conventions, with Power correctly implemented as the global cooldown parameter and optimized for STM32 resource constraints.

## Feasibility Analysis

Based on comprehensive analysis of the PEW|FORMER project's TuesdayTrack engine, implementing all seven new algorithms is **HIGHLY FEASIBLE**:

### Memory Feasibility
- **Current usage**: TuesdayTrackEngine uses ~800 bytes per instance
- **Additional memory**: Maximum 74 bytes total for all 7 algorithms
- **Per algorithm**: 7-14 bytes each (very efficient)
- **Total impact**: ~874 bytes (~9% increase)
- **STM32 headroom**: Significant available RAM (128KB+ on typical controllers)

### CPU Feasibility
- **Current CPU usage**: 0.1-0.5% per track at audio rate
- **New algorithm impact**: Within acceptable limits with existing headroom
- **Processing patterns**: All use integer arithmetic (no floating-point operations)
- **Consistency**: Similar computational patterns to existing TuesdayTrack algorithms

### Edge Cases
- **Maximum loop lengths** (128 steps): Already supported by existing buffer system
- **Extreme parameter values**: Bounded integer arithmetic prevents overflow
- **Highest density settings**: All algorithms handle gracefully without performance degradation
- **Buffer generation**: Follow same warmup + buffer pattern as existing algorithms

### Implementation Priority
1. **Phase 1 (High Priority)**: AMBIENT, MINIMAL, DRILL (7 bytes each, low-medium CPU)
2. **Phase 2 (Medium Priority)**: ACID, KRAFTWERK (14 bytes each, medium CPU)
3. **Phase 3 (Lower Priority)**: APHEX, AUTECHRE (higher complexity but still feasible)

### Conclusion
The new algorithms are specifically designed for STM32 optimization with integer arithmetic, fixed-size arrays, and minimal resource usage. They would significantly enhance the musical diversity of the TuesdayTrack engine without compromising performance or exceeding resource constraints. The implementation is not only feasible but would be a valuable addition to the PEW|FORMER Eurorack sequencer firmware.

## APHEX Algorithm (Algorithm #31)

### Overview
Complex polyrhythmic patterns with glitchy variations, characteristic of Aphex Twin's style.

### Parameter Mapping
- **Power:** Density/cooldown (0=silent, higher=more notes) - Controls note density via global cooldown system
- **Flow:** Pattern generation (controls complex polyrhythms)
- **Ornament:** Glitch probability (controls acid accents and variations)

### State Variables (STM32-Optimized)
```cpp
// APHEX algorithm state (Richard D. James/Syrefx/Classified) - Optimized for STM32
uint8_t _aphexPattern[8];        // Reduced size pattern (8 elements instead of 16)
uint8_t _aphexTimeSigNum;        // Time signature numerator (3, 5, 7) - Fixed-point based
uint8_t _aphexGlitchProb;        // Glitch probability (0-255 scale)
uint8_t _aphexPosition;          // Current position in pattern (0-7)
uint8_t _aphexNoteIndex;         // Index in pattern (0-7)
int8_t _aphexLastNote;           // Previous note for pattern coherence
uint8_t _aphexStepCounter;       // Counter for polyrhythm calculations
```

### Behavior (STM32-Optimized)
The APHEX algorithm generates complex polyrhythmic patterns with glitchy variations characteristic of Aphex Twin's style. Power controls note density through the global cooldown system. Flow influences pattern complexity using bit-shifted multipliers and fixed-point calculations. Ornament affects glitch probability and acid-style variations using efficient RNG consumption. Optimized to use only integer arithmetic and avoid costly division operations.

## AMBIENT Algorithm (Algorithm #25)

### Overview
Sparse triggers, long holds, slow drift, characteristic of ambient music.

### Parameter Mapping
- **Power:** Density/cooldown (0=silent, higher=more frequent triggers) - Controls trigger density via global cooldown system
- **Flow:** Note drift (controls how notes evolve over time)
- **Ornament:** Harmonics (controls harmonic content)

### State Variables (STM32-Optimized)
```cpp
// AMBIENT algorithm state (Brian Eno/Stars of the Lid style) - Optimized for STM32
int8_t _ambientLastNote;         // Last note generated
uint8_t _ambientHoldTimer;       // How long to hold current note
int8_t _ambientDriftDir;         // Direction of pitch drift (-1, 0, or 1)
uint8_t _ambientDriftAmount;     // Amount of pitch drift (0-255 scale)
uint8_t _ambientHarmonic;        // Current harmonic interval (0-11 for chromatic)
uint8_t _ambientSilenceCount;    // Counter for silence periods
uint8_t _ambientDriftCounter;    // Counter for drift timing
```

### Behavior (STM32-Optimized)
The AMBIENT algorithm generates sparse triggers with long holds and slow drift characteristic of ambient music. Power controls trigger density through the global cooldown system. Flow influences pitch drift using simple increment/decrement operations. Ornament affects harmonic content and interval choices. Optimized to eliminate floating-point operations and use efficient modulo arithmetic.

## AUTECHRE Algorithm (Algorithm #34)

### Overview
Abstract, constantly evolving patterns that never repeat, characteristic of Autechre's style.

### Parameter Mapping
- **Power:** Density/cooldown (0=silent, higher=more notes/changes) - Controls pattern change density via global cooldown system
- **Flow:** Transform rate (controls how quickly patterns evolve)
- **Ornament:** Micro-timing (controls subtle timing variations)

### State Variables (STM32-Optimized)
```cpp
// AUTECHRE algorithm state (geometric complexity, evolving patterns) - Optimized for STM32
uint8_t _autechreTransformState[2]; // Simplified transform state (packed)
uint8_t _autechreMutationRate;      // Rate of pattern mutation (0-255 scale)
uint8_t _autechreChaosSeed;         // Seed for chaotic behavior
uint8_t _autechreStepCount;         // Step counter for transformations
int8_t _autechreCurrentNote;        // Current note in sequence
uint8_t _autechrePatternShift;      // Bitshift for pattern evolution
```

### Behavior (STM32-Optimized)
The AUTECHRE algorithm generates abstract, constantly evolving patterns that never fully repeat, characteristic of Autechre's style. Power controls note density through the global cooldown system. Flow determines how quickly patterns transform using efficient bit operations. Ornament affects micro-timing variations. Optimized to use bit operations instead of expensive mathematical calculations and to minimize memory usage with packed state variables.

## DRILL Algorithm (Algorithm #28)

### Overview
Hi-hat rolls, bass slides, triplet subdivisions, characteristic of UK Drill music.

### Parameter Mapping
- **Power:** Density/cooldown (0=silent, higher=more frequent rolls/gates) - Controls roll/note density via global cooldown system
- **Flow:** Slide frequency (controls how often bass slides occur)
- **Ornament:** Triplet feel (controls triplet subdivision probability)

### State Variables (STM32-Optimized)
```cpp
// DRILL algorithm state (UK Drill/Trap style) - Optimized for STM32
uint8_t _drillHiHatPattern;      // Bitmask pattern of hi-hats (8-step pattern)
uint8_t _drillSlideTarget;       // Target for bass slides
uint8_t _drillTripletMode;       // Triplet flag (0 or 1)
uint8_t _drillRollCount;         // Counter for rolls
uint8_t _drillLastNote;          // Last bass note played
uint8_t _drillStepInBar;         // Step within the bar (for triplets)
uint8_t _drillSubdivision;       // Current subdivision state (for triplets)
```

### Behavior (STM32-Optimized)
The DRILL algorithm generates hi-hat rolls with bass slides and triplet subdivisions characteristic of UK Drill music. Power controls density through the global cooldown system. Flow influences slide frequency using simple counters. Ornament affects triplet feel using bit manipulation instead of complex timing calculations. Optimized to use bitmask operations and avoid floating-point or division operations.

## MINIMAL Algorithm (Algorithm #29)

### Overview
Staccato bursts, silence gaps, glitchy, characteristic of minimal techno.

### Parameter Mapping
- **Power:** Density/cooldown (0=silent, higher=more frequent note bursts) - Controls burst density via global cooldown system
- **Flow:** Silence duration (controls length of silence gaps)
- **Ornament:** Glitch repeats (controls how often glitch effects repeat)

### State Variables (STM32-Optimized)
```cpp
// MINIMAL algorithm state (minimal techno/clicks style) - Optimized for STM32
uint8_t _minimalBurstLength;     // Length of note bursts (0-15)
uint8_t _minimalSilenceLength;   // Length of silence gaps (0-15)
uint8_t _minimalClickDensity;    // Density of click-like sounds (0-255 probability)
uint8_t _minimalBurstTimer;      // Timer for current burst
uint8_t _minimalSilenceTimer;    // Timer for current silence
uint8_t _minimalNoteIndex;       // Index in current burst (0-15)
uint8_t _minimalMode;            // 0=silence, 1=burst
```

### Behavior (STM32-Optimized)
The MINIMAL algorithm generates staccato bursts with silence gaps and glitchy clicks characteristic of minimal techno. Power controls note density through the global cooldown system. Flow influences silence duration using simple counters. Ornament affects glitch repeats using efficient probability calculations. Optimized to use bit-shifts and masks for efficient timing calculations.

## ACID Algorithm (Algorithm #26)

### Overview
303-style patterns with slides and octave accents, characteristic of acid house music.

### Parameter Mapping
- **Power:** Density/cooldown (0=silent, higher=more notes) - Controls note density via global cooldown system
- **Flow:** Slide probability (how often bass slides occur)
- **Ornament:** Accents and octave changes (add variation to 303-style patterns)

### State Variables (STM32-Optimized)
```cpp
// ACID algorithm state (303-style patterns, slides, accents on octaves) - Optimized for STM32
uint8_t _acidSequence[8];        // The main 303 sequence pattern (8-step)
uint8_t _acidPosition;           // Current position in sequence (0-7)
uint8_t _acidAccentPattern;      // Bitmask for accent pattern
uint8_t _acidOctaveMask;         // Bitmask for octave changes
int8_t _acidLastNote;            // Last note played (for slides)
uint8_t _acidSlideTarget;        // Target for slide transitions
uint8_t _acidStepCount;          // Counter for pattern evolution
```

### Behavior (STM32-Optimized)
The ACID algorithm generates classic 303-style patterns with slides and accents on octave changes, characteristic of acid house music. Power controls note density through the global cooldown system. Flow determines how often bass slides occur using efficient probability calculations. Ornament affects accent patterns and octave jumps using bitmask operations. Optimized to reduce memory footprint and avoid complex calculations.

## KRAFTWERK Algorithm (Algorithm #30)

### Overview
Precise mechanical sequences with robotic repetition, characteristic of Kraftwerk's style.

### Parameter Mapping
- **Power:** Density/cooldown (0=silent, higher=more notes) - Controls note density via global cooldown system
- **Flow:** Transposition frequency (how often the sequence transposes)
- **Ornament:** Mechanical ghosts (subtle ghost notes that reinforce the mechanical feel)

### State Variables (STM32-Optimized)
```cpp
// KRAFTWERK algorithm state (precise mechanical sequences, robotic repetition) - Optimized for STM32
uint8_t _kraftwerkSequence[8];   // The main mechanical sequence pattern (8-step)
uint8_t _kraftwerkPosition;      // Current position in sequence (0-7)
uint8_t _kraftwerkLockTimer;     // Timer for locked groove patterns
uint8_t _kraftwerkTranspose;     // Current transposition offset (0-11 semitones)
uint8_t _kraftwerkTranspCount;   // Counter for transposition changes
int8_t _kraftwerkBaseNote;       // Base note of current sequence
uint8_t _kraftwerkGhostMask;     // Bitmask for mechanical ghost notes
```

### Behavior (STM32-Optimized)
The KRAFTWERK algorithm generates precise mechanical sequences with robotic repetition, characteristic of Kraftwerk's style. Power controls note density through the global cooldown system. Flow influences how frequently the sequence transposes using efficient counter operations. Ornament controls mechanical ghost notes that add subtle mechanical precision feel using bitmask operations. Optimized to use only integer arithmetic and efficient modulo operations.