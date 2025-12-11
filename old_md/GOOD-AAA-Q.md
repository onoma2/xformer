# Enhanced TuesdayTrack Algorithms: Aphex Twin, Ambient, Autechre

This document describes three new algorithms for the TuesdayTrack engine that follow the good algorithm design principles outlined in GOOD-ALGO.md. Each algorithm is purpose-driven, has a single clear concept, uses constrained pitch palettes, and employs intelligent randomness.

## Algorithm 18: APHEX TWIN (Pin-sort Style Rhythmic Complexity)

### Core Concept
"Pin-sort" style rhythmic complexity with microtonal shifting characteristic of Aphex Twin's work.

### Design Principles Applied
- **Single Clear Concept**: Complex polyrhythmic patterns with microtonal shifts
- **Rhythm First Structure**: Uses 3-against patterns as the foundation
- **Constrained Pitch Palette**: Based on Aphex's characteristic scales
- **Intelligent Randomness**: Used for microtonal shifts and pattern evolution, not note generation

### Algorithm Parameters
- **Flow**: Determines complexity level (2-4 internal voices)
- **Ornament**: Affects microtonal content and pitch selection
- **Gate Length**: 25%, 50%, 75%, or 100% based on rhythmic complexity

### Pitch Generation
- Uses 4 characteristic scales: Phrygian, Atonal, Whole tone, Augmented
- Interleaves internal "voices" through time creating a single melodic line
- Applies microtonal shifts (-2 to +2 semitones) based on Ornament parameter
- Creates monophonic sequence that captures the effect of multiple voices

### Implementation State Variables
```
uint8_t _aphexStep;              // Current step in the pin-sort sequence
uint8_t _aphexVoice;             // Current voice index for internal processing
uint8_t _aphexRhythmPattern[16]; // Complex polyrhythmic pattern
int8_t _aphexPitchSequence[16];  // Microtonal pitch sequence (-12 to +12)
uint8_t _aphexVoiceCount;        // Number of internal voices
int8_t _aphexMicrotonalShift;    // Current microtonal offset
uint8_t _aphexPatternIndex;      // Index in the pattern sequence
uint8_t _aphexActiveVoice;       // Which voice sounds this step
```

---

## Algorithm 13: AMBIENT (Evolutionary Drone with Micro-Tonal Harmonic Progression)

### Core Concept
Evolutionary drone with harmonic progression characteristic of ambient music.

### Design Principles Applied
- **Single Clear Concept**: Slow harmonic evolution with sustained textures
- **Strong Pitch Structure**: Harmonic series with sustained notes as foundation
- **Constrained Pitch Palette**: Based on select harmonic choices (chords, fifths)
- **Intelligent Randomness**: Used for evolution timing and texture selection

### Algorithm Parameters
- **Flow**: Determines harmonic complexity and evolution speed
- **Ornament**: Affects harmonic series choice and evolution rules
- **Gate Length**: 150%-400% for sustained, evolving textures

### Pitch Generation
- Uses 4 harmonic chord types: Major 7th, Minor 7th, Suspended, Second inversion
- Creates sustained, evolving textures through time-based sequencing
- Applies long gate lengths for ambient feel
- Implements slow pitch drift for ambient atmosphere

### Implementation State Variables
```
uint8_t _ambientDroneNote;       // Root drone note
uint8_t _ambientHarmonicIndex;   // Current position in harmonic series
int8_t _ambientHarmonicStack[8]; // Harmonic stack for internal harmony
uint8_t _ambientEvolutionStep;   // Step counter for harmonic evolution
uint8_t _ambientTexture;         // Current texture type
uint8_t _ambientDensity;         // Note density (0-16)
uint8_t _ambientDrift;           // Slow pitch drift amount
int8_t _ambientCurrentHeldNote;  // Currently held note for evolution
uint8_t _ambientPhase;           // Current phase of harmonic evolution
```

---

## Algorithm 19: AUTECHRE (Algorithmic Counterpoint with Mathematical Sequence Evolution)

### Core Concept
Algorithmic counterpoint with mathematical sequence evolution characteristic of Autechre's style.

### Design Principles Applied
- **Single Clear Concept**: Mathematical sequence counterpoint
- **Strong Structural Foundation**: Mathematical progressions as the base
- **Constrained Pitch Palette**: Derived from mathematical progressions
- **Intelligent Randomness**: Used to vary mathematical rules and sequence interaction

### Algorithm Parameters
- **Flow**: Determines mathematical sequence type (Fibonacci, Prime, Powers of 2)
- **Ornament**: Affects sequence interaction rules and evolution
- **Gate Length**: Variable (25%-175%) based on mathematical relationships

### Pitch Generation
- Uses 3 mathematical progressions: Fibonacci, Prime numbers, Powers of 2
- Creates single melodic line that alternates between sequence elements
- Applies 4 mathematical interaction rules: Addition, Subtraction, Multiplication, Interpolation
- Presents algorithmic counterpoint as a single evolving melodic sequence

### Implementation State Variables
```
uint8_t _autechreSequenceA[16];  // Primary mathematical sequence
uint8_t _autechreSequenceB[16];  // Secondary mathematical sequence
uint8_t _autechreSeqIndexA;      // Index in primary sequence
uint8_t _autechreSeqIndexB;      // Index in secondary sequence
uint8_t _autechreMathRule;       // Current mathematical rule being applied
uint8_t _autechreStep;           // Step counter for evolution
int8_t _autechreCurrentNote;     // Current note for this step
uint8_t _autechrePatternDensity; // Pattern complexity based on Flow
uint8_t _autechreCounterpoint;   // Counterpoint phase
```

---

## Design Principles Summary

All three algorithms follow the design principles from GOOD-ALGO.md:

### 1. Single, Clear Concept
Each algorithm focuses on one specific musical idea: Aphex Twin's pin-sort rhythms, Ambient's harmonic evolution, or Autechre's mathematical patterns.

### 2. Rhythm or Pitch Structure First
- Aphex: Rhythmic complexity foundation with 3-against patterns
- Ambient: Harmonic series foundation with sustained textures
- Autechre: Mathematical sequences as the structural base

### 3. Constrained, Purposeful Pitch Palettes
- Aphex: Scales characteristic of Aphex Twin's work
- Ambient: Harmonic chord choices and circle-of-fifths extensions
- Autechre: Notes derived from mathematical progressions

### 4. Intelligent Use of Randomness
- Aphex: Randomness for microtonal shifts and voice selection
- Ambient: Randomness for evolution timing and texture changes
- Autechre: Randomness for mathematical rule changes and sequence interaction

These algorithms avoid the issue of generic random note generation by focusing on specific musical concepts characteristic of each artist's work, using constrained pitch palettes, and employing randomness for variation within structured frameworks.

---

## Analysis of Current TuesdayTrack Implementation

### 1. The 256 Warmup Loop

The algorithm uses a 256-step warmup phase to mature the pattern before capturing the final sequence:

```
// Warmup phase: run algorithm for 256 steps to get mature pattern
// This allows capturing evolved patterns instead of initial state
// Must match exact RNG consumption pattern of buffer generation
const int WARMUP_STEPS = 256;
for (int step = 0; step < WARMUP_STEPS; step++) {
    // Execute algorithm-specific warmup logic
    // Each algorithm consumes RNG in the same pattern as real generation
    switch (algorithm) {
        case 18: // APHEX warmup
            // Advance position with odd time signature
            _aphexPosition = (_aphexPosition + 1) % _aphexTimeSigNum;
            // Update note index
            if (_aphexPosition == 0) {
                _aphexNoteIndex = (_aphexNoteIndex + 1) % 8;
            }
            // Glitch probability check
            if (_extraRng.nextRange(256) < _aphexGlitchProb) {
                _extraRng.next();  // Additional randomness for glitch
            }
            // Glide check
            if (glide > 0 && _rng.nextRange(100) < glide) {
                _rng.nextRange(3);
            }
            _aphexStepCounter++;
            break;
        // Other algorithms have similar warmup patterns...
    }
}
```

**Purpose**: The warmup ensures that the algorithm reaches a more evolved and interesting state rather than starting from the initial condition set by Flow/Ornament parameters. This is crucial for algorithms with state evolution like Markov chains or state machines.

### 2. Flow and Ornament Randomness Injection

**Flow Parameter:**
- Seeds the main RNG (_rng) for most algorithms
- Determines structural aspects like pattern complexity, time signatures, base notes, etc.
- Example in APHEX: _aphexTimeSigNum = 3 + (flow % 5) - controls time signature (3-7)
- Example in AMBIENT: _ambientDriftAmount = flow - controls pitch drift speed
- Example in AUTECH: _autechreMutationRate = flow * 16 - controls how often patterns change

**Ornament Parameter:**
- Seeds the extra RNG (_extraRng) for most algorithms
- Controls probabilistic aspects like glitch probability, harmonic intervals, etc.
- Example in APHEX: _aphexGlitchProb = ornament * 16 - glitch probability (0-255 scale)
- Example in AMBIENT: _ambientHarmonic = _extraRng.next() % 4 - harmonic interval choice
- Example in AUTECH: Affects transform state during initialization

**Implementation Pattern:**

```
// In initAlgorithm():
_rng = Random((flow - 1) << 4);      // Flow seeds main RNG
_extraRng = Random((ornament - 1) << 4);  // Ornament seeds extra RNG
```

### 3. Gate Length and Glide Override Implementation

**Gate Length Implementation:**
- Each algorithm generates its own default gate percentage
- These are then overridden by the global cooldown system based on the Power parameter
- Example in APHEX buffer generation: gatePercent = 25 + (_extraRng.next() % 75); (25-100%)
- Example in AMBIENT: _gatePercent = 200; (long sustained notes)
- Example in AUTECH: gatePercent = 15 + (_extraRng.next() % 85); (15-100%)

**Glide Override Implementation:**
- Each algorithm has its own potential slide values
- These are then overridden by the global glide parameter
- Example in APHEX:

```
// Algorithm-based slide
if (glide > 0 && _rng.nextRange(100) < glide) {
    slide = 1 + _rng.nextRange(2);  // 1-2 based on algorithm
}

// Similar in AMBIENT:
if (glide > 0 && _rng.nextRange(100) < glide) {
    slide = 3;  // Long slide for ambient feel
}
```

**Coexistence of Algorithm and Global Controls:**
The system maintains both algorithm-specific probabilities and global controls:
- Algorithm defines the base behavior and character
- Global controls (glide, power) provide user override
- The system uses _rng.nextRange(100) < glide pattern to determine when to apply global glide
- Power parameter controls the global cooldown system, affecting how often gates fire regardless of algorithm's internal gate length

This dual approach allows algorithms to maintain their distinctive character while still responding to user controls for performance and variation.

## Specific Implementation Guidance

When implementing the proposed algorithms, consider these TuesdayTrack implementation details:

### 1. 256-Step Warmup Loop
- Must implement both the algorithm logic and its warmup version
- Warmup logic must consume RNG in the exact same pattern as the real generation
- This ensures deterministic and repeatable pattern generation

### 2. Flow/Ornament Integration
- Flow should seed the main RNG for structural elements
- Ornament should seed the extra RNG for probabilistic elements
- Both parameters should meaningfully affect algorithm behavior

### 3. Gate Length Handling
- Each algorithm should generate its own characteristic gate length pattern
- Algorithm gate lengths will be used in conjunction with the global cooldown system
- Consider how your algorithm's gate lengths will interact with the power-based cooldown

### 4. Glide Integration
- Each algorithm should have its own slide logic as appropriate
- Global glide parameter will override algorithm's slide probability using the pattern `_rng.nextRange(100) < glide`
- Maintain algorithmic character while supporting global glide control

### 5. Buffer Generation for Finite Loops
- For finite loops, the algorithm pre-generates BUFFER_SIZE (128) steps during buffer generation
- The warmup and generation patterns must match exactly
- Infinite loops generate in real-time without buffering

### 6. State Variable Constraints
- Consider STM32 resource constraints when designing state variables
- Keep the number and size of state variables as minimal as possible
- Each algorithm already has dual RNG system (_rng and _extraRng)

These guidelines ensure that the new artist-specific algorithms will integrate properly with the existing TuesdayTrack engine architecture while maintaining their distinctive musical character.