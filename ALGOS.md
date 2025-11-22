# TuesdayTrack Algorithms

This document describes all 13 algorithms in the TuesdayTrack system, their musical characteristics, and implementation details.

## 1. TEST (Algorithm #0)
A test pattern algorithm with two modes:
- OCTSWEEPS: Sweeps through 5 octaves (0-4), with note value of 0
- SCALEWALKER: Cycles through chromatic notes (0-11) sequentially
- Parameters: Uses Flow to determine mode and sweep speed, Ornament for accent and velocity
- Gate length: Fixed at 75%
- Slide: Only occurs when glide parameter is active, amount determined by sweep speed

## 2. TRITRANCE (Algorithm #1)
German minimal style arpeggios based on a 3-phase cycling pattern:
- Phase 0 (step + b2) % 3 == 0: Plays note b3+4 in octave 0, with 25% chance to possibly change b3
- Phase 1 (step + b2) % 3 == 1: Plays note b3+4 in octave 1, with 50% chance to change b2
- Phase 2 (step + b2) % 3 == 2: Plays note b1 in octave 2, with 50% chance to change b1
- Parameters: Flow seeds main RNG for b1 and b2, Ornament seeds extra RNG for b3
- Gate length: Random, ranging from 50% to 400% of step duration
- Slide: Occurs based on glide parameter probability (1-3 amount)

## 3. STOMPER (Algorithm #2)
Acid bass patterns with slides and state machine transitions:
- Uses a complex 14-state pattern machine with different behaviors for each state
- Key states include pauses, low/high notes, slides up/down, and hi/low patterns
- Parameters: Flow seeds pattern modes (extra RNG), Ornament seeds note choices (main RNG)
- Gate length: Variable, including rest periods during countdown
- Slide: Occurs in specific pattern states when glide is active

## 4. MARKOV (Algorithm #3)
Markov chain melody generation using an 8x8x2 transition matrix:
- Uses history1 and history3 to select notes from the transition matrix
- Each note choice depends on the previous two notes in the sequence
- Parameters: Both Flow and Ornament contribute to generating the 8x8x2 transition matrix
- Gate length: Random, ranging from 50% to 400% of step duration
- Slide: Occurs based on glide parameter probability (1-3 amount)
- Octave: Randomly 0 or 1

## 5. CHIPARP (Algorithm #4)
Chiptune arpeggio patterns with chord progressions:
- Generates 4-step chord patterns, typically playing notes (0, 2, 4, 6) + base offset
- Has ascending or descending playback direction based on Ornament parameter
- Includes note-off possibility (~19% chance) and accent probability
- Parameters: Flow seeds main RNG, Ornament seeds chord RNG
- Gate length: Variable (50%, 75%, or 100%)
- Slide: Can occur algorithmically or via glide parameter

## 6. GOACID (Algorithm #5)
Goa/psytrance acid patterns with systematic transposition:
- Generates notes from a lookup table of 8 values: {0, -12, 1, 3, 7, 12, 13} with modifications based on accent
- Applies systematic transposition: +3 semitones if goaB1 is true during first 8 steps of 16-step cycle, -5 semitones if goaB2 is true during first 8 steps
- Adds a default +24 semitones (2 octaves) to all notes
- Parameters: Flow seeds note selection RNG, Ornament seeds accent RNG and initializes pattern flags
- Gate length: Fixed at 75%
- Slide: Occurs based on glide parameter probability (1-3 amount)

## 7. SNH (Sample & Hold) (Algorithm #6)
Sample & Hold random walk algorithm:
- Creates evolving CV values using a filtered random walk
- Updates target value when phase changes, with gradual transition
- Parameters: Flow seeds main RNG, Ornament seeds extra RNG
- Gate length: Fixed at 75%
- Slide: Occurs based on glide parameter probability (1-3 amount)
- Note: Converted from filtered value using bit manipulation: (absValue >> 22) % 12

## 8. WOBBLE (Algorithm #7)
Dual-phase LFO bass with alternating patterns:
- Uses two different phase oscillators to generate notes
- Alternates between "high" and "low" phases, each with different note generation
- Includes probability-based slides between phase transitions
- Parameters: Flow seeds main RNG, Ornament seeds extra RNG
- Gate length: Fixed at 75%
- Slide: Can occur algorithmically or via glide parameter (1-3 amount)

## 9. TECHNO (Algorithm #8)
Four-on-floor club patterns with kick and hi-hat patterns:
- Creates kick drum patterns with 4 different variations based on Ornament setting
- Generates hi-hat patterns on off-beats with 4 different variations based on Flow setting
- Parameters: Flow determines kick pattern type, Ornament determines hi-hat pattern type
- Note: Bass notes (0-4) for kicks, higher notes (7-9) for hi-hats
- Gate length: 80% for kicks, 40% for hi-hats
- Slide: Occurs based on glide parameter probability (1-3 amount)

## 10. FUNK (Algorithm #9)
Syncopated funk grooves with ghost notes:
- Uses 8 different 16-step patterns stored as bitmasks
- Includes 4 different syncopation levels affecting note selection
- Features ghost notes with reduced velocity (~35% gate length)
- Parameters: Flow determines pattern selection, Ornament determines syncopation and ghost probability
- Gate length: 75% for regular notes, 35% for ghost notes
- Slide: Occurs based on glide parameter probability (1-3 amount)

## 11. DRONE (Algorithm #10)
Sustained drone textures with slow movement:
- Creates sustained notes with very long gate lengths (400%)
- Changes at slow intervals based on drone speed parameter
- Uses interval patterns: unison, perfect 5th, octave, or 5th+octave
- Parameters: Flow determines base note, Ornament determines interval and variation
- Gate length: Fixed at 400% for very long sustain
- Slide: Long slides (amount 3) when glide is active

## 12. PHASE (Algorithm #11)
Minimalist phasing patterns with gradual shifts:
- Creates a pattern of 8 notes that shifts position gradually over time
- Uses a phase accumulator to create gradual positional shifts
- Parameters: Flow determines pattern content, Ornament determines phase drift speed
- Gate length: Fixed at 75%
- Slide: Occurs based on glide parameter probability (1-3 amount)

## 13. RAGA (Algorithm #12)
Indian classical melody patterns with traditional scales:
- Uses 4 different Indian classical scales (Bhairav-like, Yaman-like, Todi-like, Kafi-like)
- Follows characteristic melodic movements: stepwise, skips, direction changes
- Includes traditional ornaments (gamaka-like slides)
- Parameters: Flow determines scale type, Ornament determines ornament type
- Gate length: Fixed at 75%
- Slide: Characteristic slides based on ornament probability or glide parameter