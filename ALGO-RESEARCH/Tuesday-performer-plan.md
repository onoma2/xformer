# Tuesday-Style Track for Performer: Research Summary

## Overview
Tuesday is an algorithmic sequencer that generates gate+pitch pairs with sophisticated algorithmic behaviors. For the Performer implementation, we'll focus on a simplified version where the algorithm generates the sequence automatically, requiring only a few global parameters to be set by the user.

## Core Tuesday Concepts

### 1. Integrated Gate+Pitch Architecture
- Each algorithmically-generated "tick" contains both note and velocity (gate) information
- Structure from Tuesday_Tick_t:
  - `unsigned char vel` - Controls gate trigger (0 = rest, >0 = gate)
  - `signed char note` - Controls pitch (note value)
  - `unsigned char accent :1` - Accent gate
  - `unsigned char slide : 2` - Slide/glide information
  - `unsigned char maxsubticklength : 4` - Gate length

### 2. Algorithmic Generation
- Multiple algorithm types (Markov, Stomper, ScaleWalker, etc.)
- Each algorithm generates a complete sequence based on parameters
- No per-step programming required

## Simplified Tuesday-Style Track for Performer

### Core Concept: Algorithm-Generated Sequences
- Single track with algorithmically generated gate+pitch pairs
- Only 5 parameters for user control:
  1. Algorithm selection (Markov, Stomper, ScaleWalker, etc.)
  2. Flow (musical flow parameter)
  3. Ornament (ornamentation parameter)
  4. Intensity (controls algorithmic variation)
  5. Loop Length (1-64 steps or infinite)
- All other parameters inherited from Note track type and set globally in Seq and Track menus

### Implementation Approach

#### 1. Model Layer Changes
- Add `TuesdayTrack` class with global algorithm parameters
- Store only 5 user-controlled parameters:
  - `algorithm : 5` - Algorithm selection (up to 32 algorithms)
  - `flow : 8` - Flow parameter (0-255, controls musical flow)
  - `ornament : 8` - Ornament parameter (0-255, controls ornamentation)
  - `intensity : 8` - Intensity parameter (0-255)
  - `loopLength : 7` - Loop length (1-64 steps, with 64+ being infinite)
- Runtime algorithm state stored separately

#### 2. Algorithm Integration
- Algorithm generates sequence based on flow, ornament, intensity, and loopLength
- Algorithm state maintained per track with loop position tracking
- No per-step data required from user
- Algorithmic generation replaces manual step programming
- Loop length determines when the algorithmic sequence repeats or continues infinitely

#### 3. UI Layer Integration (Completely Different)
- **Main Tuesday Page**: Displays current algorithm and the 5 parameters
  - ALGO: [Markov/Stomper/ScaleWalker...]
  - FLOW: [0-255 value, musical flow]
  - ORN: [0-255 value, ornamentation]
  - INTEN: [0-255 value]
  - LOOP: [1-64 steps or ∞ symbol for infinite]
- **No step input**: No 16-step buttons or per-step editing
- **Parameter navigation**: Use encoder to move between the 5 parameters
- **Algorithm selection**: Button or encoder to cycle through algorithm types
- Integrate with existing Seq/Track menus for global parameters (octave, transpose, etc.)

#### 4. Engine Layer Implementation
- TuesdayTrackEngine generates next gate+pitch pair using algorithm
- Gate output based on algorithm-generated velocity value
- Pitch output mapped to voltage based on algorithm-generated note value
- Clock synchronization with existing system
- Runtime algorithm state advances with each clock tick

### Technical Considerations

#### Memory Efficiency
- Minimal parameter storage (only 4 main parameters)
- Runtime state separate from saved parameters
- Efficient algorithm implementations

#### Performance
- Algorithm execution optimized for STM32
- Pre-computed state transition tables where possible
- Minimal processing per clock tick

#### Integration with Existing Features
- Compatible with pulse count and gate modes (applied to algorithm output)
- Accumulator can modulate algorithm output
- Harmony feature could work with Tuesday tracks (following master track)
- Clock synchronization with existing system

### Algorithm Integration with Existing Features

- **Scale**: Follow existing Performer scale settings from Seq menu
- **Octave/Transpose**: Use existing NoteTrack parameters from Seq/Track menus
- **Slide**: Apply to algorithm-generated pitch transitions
- **Gate Probability**: Modify algorithm-generated gate velocity
- **Retrigger**: Generate multiple events based on algorithm output

### UI Navigation

#### Global Menu Integration
- Tuesday track appears in Track mode selection
- Seq menu contains standard NoteTrack parameters (they apply to algorithm output)
- Track menu contains standard NoteTrack parameters

#### Tuesday-Specific Menu
- Access via sequence button when track mode is Tuesday
- Only 4 parameters to adjust: Algorithm, Seed1, Seed2, Intensity
- Visual feedback showing current algorithm behavior
- Real-time parameter adjustment

### Algorithm Options

1. **Markov Chain** - Note-to-note probability relationships
2. **Stomper** - Pattern-based with pauses and slides
3. **Scale Walker** - Sequential walking through scales
4. **Random Walk** - Simple random note selection
5. **Wobble** - Oscillating patterns
6. **Chip Arps** - Arpeggiated patterns
7. **Cellular Automata** - Pattern evolution based on simple rules
8. **Chaos Theory** - Logistic map-based sequence generation
9. **Fractal Pattern** - Mathematical fractal-based note selection
10. **Wave Interference** - Multiple oscillator interference patterns
11. **DNA Sequence** - Biological DNA-inspired mutation patterns
12. **Turing Machine** - Computational model-based sequence generation
13. **Ambient/Ethereal** - Evolving textures using noise-based patterns
14. **Techno/Rave** - Driving patterns with syncopated rhythms
15. **Jazz Improvisation** - Chord progressions with swing feel
16. **Classical Counterpoint** - Voice leading with independent melodic lines
17. **Minimalist Repetition** - Phasing patterns in the style of Steve Reich
18. **Breakbeat/DnB** - Complex rhythmic patterns with syncopated bass
19. **Droning/Bowed Strings** - Sustained textures with microtonal variations
20. **Arpeggiated Patterns** - Lush evolving arpeggios (Vangelis-style)
21. **Funk Rhythm** - Syncopated rhythms with backbeat emphasis
22. **Indian Classical Raga** - Modal scales with traditional melodic patterns

## Memory and Processing Constraints Analysis

Based on STM32F4 constraints (192KB RAM, 1MB Flash, 168MHz):

### Memory Requirements per Track Instance:
- **Minimal algorithms**: Scale Walker (9 bytes), Stomper (12 bytes)
- **Moderate algorithms**: Markov (138 bytes), DNA (24 bytes), Cellular (22 bytes)
- **High memory algorithms**: Turing Machine (103 bytes with large rule table)

### Recommended Algorithm Set for 8-Track Usage:
- **Phase 1 (Essential)**: Markov, Stomper, Scale Walker, DNA, Cellular, Chaos (6 algorithms)
  - Total memory: ~400 bytes across 8 tracks
  - CPU usage: Moderate, well within STM32F4 capabilities
  - Musical diversity: Good range of behaviors

### Maximum Feasible Count:
- **Conservative estimate**: 8-10 algorithms total
- **Excluded**: Turing Machine due to high memory requirements (103 bytes per track)
- **Advanced additions**: Fractal and Wave Interference with careful optimization

### Implementation Strategy:
1. **Core Set (6 algorithms)**: Markov, Stomper, Scale Walker, DNA, Cellular, Chaos
2. **Musical Style Additions**: Up to 8 more from the new musical style algorithms (Ambient, Techno, Jazz, etc.)
3. **Advanced Algorithms**: Additional from Fractal, Wave Interference, Wobble, Chip Arps
4. **Excluded**: Turing Machine (too memory intensive for 8-track usage)

## Implementation Benefits

### Musical
- Algorithmic behavior adds generative elements without user programming
- Algorithm selection provides variety
- Flow and Ornament parameters offer intuitive musical control

### Technical
- Minimal memory usage for core algorithms (with 5 parameters per track)
- Extends existing sequencer architecture
- Reuses engine infrastructure
- Careful algorithm selection to maintain performance within STM32F4 constraints

### User Experience
- Simple, focused interface with only essential controls
- No step programming needed - algorithm generates sequence automatically
- Familiar algorithm types from original Tuesday module
- Integrates with existing global settings

## Potential Challenges

1. **Algorithm Complexity vs. Predictability** - Balance sophistication with user control
2. **Memory Management** - Maintain efficient algorithm state without excessive usage
3. **Real-time Parameter Changes** - Ensure smooth parameter updates during playback
4. **UI Differentiation** - Make interface clearly distinct from step sequencer
5. **Processing Performance** - Fractal and Wave Interference require careful optimization for real-time performance

## Musical Style Algorithms Enhancement

Adding musical style algorithms significantly expands the creative possibilities of the Tuesday track:

### Style-Based Algorithm Categories:
- **Atmospheric**: Ambient/Ethereal, Droning/Bowed Strings
- **Rhythmic**: Techno/Rave, Breakbeat/DnB, Funk Rhythm
- **Harmonic**: Jazz Improvisation, Classical Counterpoint, Arpeggiated Patterns
- **Cultural**: Indian Classical Raga
- **Experimental**: Minimalist Repetition

### Parameter Mapping for Musical Styles:
- **Algorithm**: Selects the musical style (0-31 with expanded range)
- **Flow**: Controls musical flow/pattern within the style
- **Ornament**: Influences ornamental elements or cultural variations
- **Intensity**: Modulates energy level or expressiveness within the style
- **Loop Length**: Sets the sequence length (1-64 steps or infinite)

### Creative Benefits:
- Enables genre-specific musical generation without manual programming
- Provides authentic musical patterns for each selected style
- Allows for cross-genre experimentation by combining with other track types

## Success Metrics

- Algorithm generates interesting, musical sequences
- Only 5 parameters needed for full track control
- No per-step UI elements
- Global NoteTrack parameters still accessible and applied to algorithm output
- Performance doesn't impact other tracks
- Memory usage is optimized (with 5 parameters per track)
- UI clearly different from standard NoteTrack step entry
- At least 12-14 diverse algorithmic behaviors available (core + musical styles)