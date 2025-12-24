# Algo Track User Manual

## 1. Introduction

The Algo Track (formerly known as Tuesday Track) is an advanced sequencer track type that uses algorithmic generation to create complex, evolving musical sequences. Rather than storing fixed step values, the Algo track uses mathematical algorithms to generate note values, rhythms, and patterns in real-time.

This track type is ideal for:
- Generative music and evolving patterns
- Complex rhythmic structures
- Algorithmic composition
- Soundtrack and ambient music
- Random but controlled musical output

The Algo track provides:
- Multiple algorithm types (Test, Tri, Stomper, Markov, Chip, Wobble, etc.)
- Parameter control (Flow, Ornament, Power, etc.)
- Loop length and rotation controls
- Glide and trill effects
- Scale-based voltage output

## 2. Algo Track Overview

### 2.1 What is Algo?

The Algo track generates musical sequences using mathematical algorithms rather than fixed step data. Each algorithm has its own unique approach to generating note values, rhythms, and patterns. The track processes these algorithms on each clock tick to determine the current note and gate outputs.

Key concepts:
- **Algorithms**: 15+ different generation algorithms with unique behaviors
- **Parameters**: Flow, Ornament, and Power control algorithm behavior
- **Loop Length**: Controls pattern repetition (0 = infinite/evolving)
- **Glide/Trill**: Smooth transitions and rapid note repetitions

### 2.2 Algorithm Types

The Algo track includes several algorithm categories:

**Melodic Algorithms**:
- **ScaleWalker**: Walks up/down scale degrees
- **Markov**: Markov chain-based note generation
- **Blake**: Breath-like melodic phrases
- **Window**: Slow anchor notes with fast texture

**Rhythmic Algorithms**:
- **Tritrance**: Bouncy, rhythmic patterns
- **Stomper**: Stomping rhythmic sequences
- **Autechre**: Complex pattern-based sequences
- **Minimal**: Burst/silence rhythmic patterns

**Harmonic Algorithms**:
- **Chip Arp 1/2**: Arpeggiated chord progressions
- **Aphex**: Multi-voice pattern sequences
- **Ganz**: Multi-layered harmonic structures

**Experimental Algorithms**:
- **Wobble**: Phase-based oscillating patterns
- **Autechre**: Rule-based pattern generation
- **Step**: Stepped wave patterns

### 2.3 Output Modes

The track has two main output behaviors:

**Free Mode**: CV output updates every step regardless of gate state
**Gated Mode**: CV output only updates when a gate fires

## 3. UI Overview

### 3.1 Main Sequence Page

The main page displays sequence settings:

- **Top Row**: Algorithm selection and parameter controls
- **Middle Row**: Loop and timing controls
- **Bottom Row**: Gate and start parameters
- **Status Box**: Visual feedback showing current note (with gate indicator), output voltage, and step position
- **Context Menu**: Init and Reseed options via long-press

### 3.2 Parameter Pages

The Algo track has three parameter pages accessible via F1-F4:

**Page 1** (F1-F4): Algorithm, Flow, Ornament, Power
- **Algorithm**: Selects the generation algorithm (0-14)
- **Flow**: Controls primary algorithm parameter (0-16)
- **Ornament**: Controls secondary algorithm parameter (0-16)
- **Power**: Controls note density/cooldown (0-16)

**Page 2** (F1-F4): Loop, Rotate, Glide, Skew
- **Loop**: Pattern length (0=Infinite, 1-29=2-128 steps)
- **Rotate**: Loop rotation offset (±63 steps when loop enabled)
- **Glide**: Slide probability (0-100%)
- **Skew**: Density curve across loop (-8 to +8)

**Page 3** (F1-F4): Gate Length, Gate Offset, Trill, Start
- **Gate Length**: Gate duration scaling (0-100%)
- **Gate Offset**: Gate timing offset (0-100%)
- **Trill**: Intra-step subdivision probability (0-100%)
- **Start**: Starting position (0-16)

### 3.3 Footer Controls (Function Keys)

**F1-F4**: Select parameter on current page for encoder control
**F5**: Next parameter page (or Shift+F5 to reseed)
**NEXT**: Visual indicator showing current parameter page

### 3.4 Context Menu

**Init**: Clear and reset sequence to defaults
**Reseed**: Restart algorithm with new random seed

## 4. Algorithm Parameters

### 4.1 Algorithm Parameter

Controls which generation algorithm is active:

**Range**: 0 to 20 with specific algorithms:
- 0: Test - Basic algorithm for testing
- 1: Tri - Triangular/trance-like patterns
- 2: Stomper - Stomping rhythmic patterns
- 6: Markov - Markov chain note generation
- 7: Chip1 - Arpeggiated chip-tune patterns
- 8: Chip2 - Alternative chip-tune patterns
- 9: Wobble - Phase-based oscillating patterns
- 10: ScaleWalk - Walking scale patterns
- 11: Window - Anchor notes with texture
- 12: Minimal - Burst/silence patterns
- 13: Ganz - Multi-layered structures
- 14: Blake - Breath-like phrases
- 18: Aphex - Multi-voice patterns
- 19: Autechre - Rule-based patterns
- 20: Step - Stepped wave patterns

### 4.2 Flow and Ornament Parameters

These control algorithm-specific parameters:

**Flow** (0-16):
- Primary algorithm control parameter
- Different meaning per algorithm
- Often controls pattern complexity or behavior

**Ornament** (0-16):
- Secondary algorithm control parameter
- Different meaning per algorithm
- Often controls variation or ornamentation

### 4.3 Power Parameter

Controls note density and cooldown:

**Range**: 0 to 16
- Lower values = higher cooldown = sparser patterns
- Higher values = lower cooldown = denser patterns
- Acts as a note probability/gating parameter

## 5. Advanced Features

### 5.1 Loop Controls

The Algo track includes sophisticated loop controls:

**Loop Length**:
- 0: Infinite/evolving patterns (default)
- 1-29: Finite loops of 2-128 steps
- Allows for repeating patterns within algorithmic generation

**Rotation**:
- Shifts the loop start position (±63 steps)
- Only available when loop length > 0
- Creates variations without changing algorithm parameters

**Skew**:
- Bipolar (-8 to +8) distribution control
- Affects density curve across loop length
- Creates front-loaded or back-loaded patterns

### 5.2 Glide and Trill Effects

**Glide** (0-100%):
- Probability of smooth transitions between notes
- Creates portamento/sliding effects
- Adds expressiveness to algorithmic output

**Trill** (0-100%):
- Probability of rapid note subdivisions
- Creates rapid note repetitions
- Adds rhythmic complexity

**Gate Offset** (0-100%):
- Timing offset for gate firing
- Allows for syncopation and timing variation
- Adjusts gate timing relative to step

### 5.3 Reseed Function

**Reseed** (Shift+F5):
- Restarts algorithm with new random seed
- Creates new variation of same algorithm
- Useful for live performance to refresh patterns

## 6. External CV Integration

### 6.1 Routing Setup

The Algo track supports CV routing to parameters:

**Routable Parameters**:
- Algorithm, Flow, Ornament, Power
- Glide, Trill, Gate Length, Gate Offset
- Divisor, Rotate, Octave, Transpose

**Usage**:
- Route CV sources to algorithm parameters
- Create evolving parameter changes
- Modulate algorithm behavior with external sources

### 6.2 Applications

**Parameter Modulation**:
- LFOs modulate Flow/Ornament for evolving patterns
- Envelopes modulate Power for dynamic density
- Random CV modulates Algorithm for morphing

**Scale Integration**:
- Output quantized to selected scale
- Supports chromatic and named scales
- Root note and octave transposition available

## 7. Internal Sequencing

### 7.1 Algorithmic Generation

The Algo track uses mathematical algorithms to generate:
- Note values based on scale degrees
- Rhythmic patterns and gate timing
- Probability-based variations
- Stateful pattern evolution

### 7.2 Timing Control

**Divisor**: Clock division (1-768) controls step rate
**Gate Length**: Controls gate duration scaling (0-100%)
**Reset Measure**: Bar-based reset timing (0-128 bars)

### 7.3 Scale Quantization

**Scale Selection**:
- Project scale (default)
- Track-specific scales (0-19+)
- Chromatic/Semitones option

**Transposition**:
- Octave offset (-10 to +10)
- Semitone offset (-11 to +11)
- Root note override (C to B)

## 8. Practical Examples

### 8.1 Ambient Pad Generation

1. Select Algorithm 10 (ScaleWalk)
2. Set Flow to 8, Ornament to 6
3. Set Power to 4 for sparse notes
4. Set Glide to 75% for smooth transitions
5. Use a slow divisor (e.g., 768 for whole notes)
6. Select a rich harmonic scale

### 8.2 Rhythmic Pattern Generation

1. Select Algorithm 2 (Stomper)
2. Set Flow to 10 for active patterns
3. Set Power to 12 for dense notes
4. Set Trill to 40% for rhythmic complexity
5. Use medium divisor (e.g., 48 for 8th notes)
6. Apply to drum CV/Gate outputs

### 8.3 Melodic Variation

1. Select Algorithm 6 (Markov)
2. Set Flow to 5 for conservative changes
3. Set Ornament to 12 for variation
4. Set Gate Length to 50% for note separation
5. Use Scale and Root settings for tonal center
6. Apply Reseed periodically for new variations

## 9. Shortcuts and Tips

### 9.1 Quick Navigation

- **F1-F4**: Select parameter for encoder control
- **F5**: Cycle between parameter pages
- **Shift+F5**: Reseed algorithm with new seed
- **Context Menu**: Long press for Init/Reseed

### 9.2 Quick Edit (Page+Steps 9-16)

- **Page+Step 9**: Copy parameters
- **Page+Step 10**: Paste parameters
- **Page+Step 15**: Randomize (randomizes all parameters)

### 9.3 Editing Tips

- Start with Algorithm 10 (ScaleWalk) for melodic output
- Use Power parameter to control note density
- Set Loop Length to 0 for infinite evolution
- Use Gate Length to control note overlap
- Try different scales for harmonic variety

### 9.3 Jam Surface (Step Buttons)

| Step | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Top | Octave + | Transpose + | Root Note + | Div up (straight) | Div up (triplet) | Div /2 (faster) | Mask + | Loop +<br>Shift: Run momentary |
| Bottom | Octave - | Transpose - | Root Note - | Div down (straight) | Div down (triplet) | Div x2 (slower) | Mask - | Loop -<br>Shift: Mute momentary |

Shift modifiers:
- Shift+Step 8: Run momentary (press = stop)
- Shift+Step 16: Mute momentary (press = mute)

Behavior notes:
- Top row moves "up" (faster/greater), bottom row moves "down" (slower/less); for divisors, smaller = faster.
- Divisor up/down only walks known divisors for straight or triplet types.

### 9.4 Performance Considerations

- Algorithm processing uses moderate CPU resources
- Complex algorithms may have slightly higher load
- Glide and trill add additional processing overhead
- All algorithms are optimized for real-time performance

## 10. Troubleshooting

### 10.1 No Output

- Verify track is not muted
- Check that algorithm is generating notes
- Ensure divisor is set appropriately
- Confirm scale settings are valid

### 10.2 Unexpected Patterns

- Check algorithm parameters (Flow, Ornament, Power)
- Verify loop length settings
- Ensure appropriate scale is selected
- Try Reseed to refresh algorithm state

### 10.3 Sync Issues

- Verify divisor settings match expectations
- Check reset measure settings
- Ensure external sync sources are properly configured
- Confirm algorithm is not getting stuck in state
