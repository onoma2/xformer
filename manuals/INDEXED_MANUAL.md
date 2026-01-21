# Indexed Track User Manual

## 1. Introduction

The Indexed Track is a sophisticated sequencer track type that maps step indices to discrete output voltages using duration-based timing. Each step has an independent duration in clock ticks (scaled by the sequence divisor) and a note index that determines the output voltage when the step is active.

This track type is ideal for:
- Precise timing control with arbitrary step durations
- Complex rhythmic patterns with non-musical divisions
- CV sequences with custom timing relationships
- External modulation of step parameters

The Indexed track provides:
- Up to 48 controllable steps per sequence
- Independent duration control for each step (0-1023 ticks)
- Direct voltage lookup without octave math
- Group-based CV modulation
- Flexible sync modes

## 2. Indexed Track Overview

### 2.1 What is Indexed?

The Indexed track creates a sequence where each step has an independent duration and note value. Unlike traditional sequencers that divide a measure into equal parts, each step in an Indexed sequence can have completely different timing, allowing for complex rhythmic patterns and precise timing control.

Key concepts:
- **Steps**: 48 controllable positions with independent duration and note values
- **Durations**: Clock tick counts (0-1023) that determine step length (then scaled by divisor)
- **Note Indices**: Voltage lookup indices (-63 to +63) that determine output voltage
- **Groups**: A, B, C, D group membership for conditional CV modulation

### 2.2 Timing System

The Indexed track uses a duration-based timing system:

**Step Timer**:
- Counts up from 0 to the current step's effective duration
- Effective duration = step duration × divisor
- When reaching the duration value, advances to the next step
- Provides precise timing control with tick-level accuracy

**Gate Timer**:
- Counts down from the effective gate length in ticks
- Effective gate = step gate × divisor
- Determines how long gate output remains high
- Gate length is clamped to step duration (OFF = 0, FULL = duration)

**Duration Control**:
- Each step has its own duration in clock ticks (0-1023)
- Divisor scales all step durations and gate lengths
- Allows arbitrary timing relationships while keeping a consistent scale for gate and duration

### 2.3 Output Modes

The track has two main output behaviors:

**Gate Mode** (default): CV output only updates when a step becomes active
**Always Mode**: CV output continuously updates regardless of step changes

## 3. UI Overview

### 3.1 Main Sequence Page

The main page displays sequence settings:

- **Top Row**: Divisor, Clock Mult, loop mode, active length settings
- **Middle Row**: Scale selection and root note settings
- **Bottom Row**: Sync mode and reset measure settings
- **Context Menu**: Init and Route options via long-press

### 3.2 Step Editing Page (Page+S2)

The step editing page shows three rows per step:

- **Note Row**: Edit note index for voltage output (±1 semitone, Shift=±12)
- **Duration Row**: Edit step duration in ticks (±1 tick, Shift=±divisor)
- **Gate Row**: Edit gate length in ticks (OFF or 1..1023; FULL equals duration). Normal steps 5 ticks, Shift steps 1.

**Function Keys (Step Edit)**:
- **F1**: Note edit mode
- **F2**: Duration edit mode; press again to toggle **DUR-TR** (duration transfer)
- **F3**: Gate edit mode
- **F4**: Context cycle **SEQ → STEP → GROUPS**
- **F5**: Math page
- **Shift+F5**: Routes page

**Groups Context**:
- **F1-F4**: Toggle group A-D for selected steps
- **F5**: Back to edit context

### 3.3 Footer Controls (Function Keys)

**F1 - Clock Settings**:
- **Divisor**: Time scale (1-768) applied to duration and gate ticks
- **Clock Mult**: Per-sequence rate multiplier (0.50x-1.50x) that scales tick rate
- **Loop**: On/Once mode for sequence behavior
- **Active Length**: Dynamic step count (1-48) for sequence length

**F2 - Scale Settings**:
- **Scale**: Project scale or track-specific scale selection
- **Root**: Root note for chromatic scale mapping (C to B)
- **First Step**: Rotation offset for sequence starting point

**F3 - Sync Settings**:
- **Sync Mode**: Off, Reset (bar-based), External sync
- **Reset Measure**: Bars to reset sequence position
- **CV Update**: Gate (only on step change) or Always (continuous)

**F4 - Track Settings**:
- **Octave**: Octave offset for all output voltages
- **Transpose**: Semitone offset for all output voltages
- **CV Update**: Gate/Always mode selection

### 3.4 Step Operations

Context menu provides:
- **Insert**: Add new step at cursor position, shifting subsequent steps
- **Make First**: Set selected step as the sequence start
- **Delete**: Remove step, shifting subsequent steps
- **Copy**: Copy current step to clipboard
- **Paste**: Paste clipboard content to current step

## 4. Step Parameters

### 4.1 Duration Parameters

Duration determines how long a step remains active:

**Range**: 0 to 1023 clock ticks
- 0 duration steps are skipped immediately
- Higher values create longer step durations
- Independent for each step in sequence

**Timing Control**:
- Normal encoder: ±1 tick adjustment
- Shift + encoder: ±divisor adjustment for musical timing
- Allows for precise timing relationships

**Practical Ranges**:
- 1 tick: Shortest possible step
- 48 ticks: 32nd note at 192 PPQN (divisor = 1)
- 192 ticks: Quarter note at 192 PPQN (divisor = 1)
- 768 ticks: Whole note at 192 PPQN (divisor = 1)

### 4.2 Gate Parameters

Gate length determines how long gate output remains high:

**Gate Scale**:
- OFF (0) keeps the gate low
- 1..1023 are direct tick lengths
- FULL equals the step duration

**Clamping**:
- Gate length is clamped to step duration
- If you dial past duration it snaps to FULL (display shows duration ticks)

**Editing**:
- Normal encoder: coarse steps (5 ticks)
- Shift + encoder: fine steps (1 tick)

### 4.3 Note Index Parameters

Note index determines output voltage when step is active:

**Range**: -63 to +63 maps to output voltage
- Direct lookup in selected scale
- No octave math or modulo operations
- -63 = lowest possible voltage, +63 = highest

**Scale Mapping**:
- Based on selected scale and root note settings
- -63 = lowest scale note, +63 = highest scale note
- Converted using scale's note-to-voltage mapping

## 5. Advanced Features

### 5.1 Step Operations

The Indexed track includes powerful step operations:

**Insert Function**:
- Add new step at current position
- Shifts subsequent steps to the right
- Clones properties from previous step

**Make First Function**:
- Sets the current step as the sequence start
- Rotates playback so the selected step is first

**Split Function**:
- Divides a step's duration into two equal parts
- First half gets ceil(duration/2), second gets floor(duration/2)
- Preserves all other step properties

**Delete Function**:
- Removes current step from sequence
- Shifts subsequent steps to the left
- Maintains sequence continuity

### 5.2 Group-Based Modulation

Each step can belong to one or more groups (A, B, C, D):

**Group Assignment**:
- Steps can be in multiple groups simultaneously
- Groups A-D provide routing targets for CV modulation
- Groups can be toggled per step

**Route Configuration**:
- Route A and Route B can target different parameters
- Targets: Duration, Gate Length, Note Index
- Amount: -200% to +200% modulation depth
- Enabled: On/Off for each route

### 5.3 Route Combination Modes

Multiple routes can be combined in different ways:

**A to B**: Apply Route A then Route B sequentially
**Mux**: Average of Route A and Route B values
**Min**: Use minimum of Route A and Route B values
**Max**: Use maximum of Route A and Route B values

### 5.4 Routing Improvements

Recent updates to the routing system include:

**Enhanced Indexed Routing**:
- Improved routing note handling for more accurate modulation
- Better integration between groups and routing parameters
- Corrected routing source selection for Indexed tracks
- Enhanced LED feedback for routing status

**Group Routing Enhancements**:
- More responsive group assignment
- Improved visual feedback when groups are active
- Better handling of multiple simultaneous group assignments

### 5.5 Math Operations (F5)

The Math page allows you to apply batch operations to selected steps (or all steps/groups).

**Available Operations**:
- **Add/Sub/Mul/Div**: Basic arithmetic
- **Set**: Set all values to a fixed number
- **Rand**: Randomize values
- **Jitter**: Add random jitter to existing values
- **Ramp**: Create a linear ramp across selected steps (start to end)
- **Quant**: Quantize values to nearest multiple (e.g., quantize duration to 16 ticks)

## 6. External CV Modulation

### 6.1 Setup for External CV

To use external CV modulation with Indexed track:

1. Configure Route A or Route B parameters
2. Set target groups (A-D) that will respond to this route
3. Select target parameter (Duration/Gate/Note)
4. Set modulation amount (-200% to +200%)
5. Enable the route
6. Route CV sources to IndexedA or IndexedB targets

### 6.2 Applications

**Duration Modulation**:
- LFOs can stretch or compress step timing
- Envelopes can dynamically change sequence speed
- Random CV creates timing variations

**Gate Length Modulation**:
- LFOs can modulate note lengths
- Envelopes can create dynamic gate patterns
- Random CV varies note durations

**Note Index Modulation**:
- External CV transposes entire sequences
- LFOs create pitch vibrato
- Envelopes create pitch bends

## 7. Internal Sequencing

### 7.1 Duration-Based Sequencing

The Indexed track uses accumulator-based timing:
- Step timer counts up to current step's duration
- When threshold reached, advances to next step
- Gate timer counts down independently from calculated gate duration

### 7.2 Loop vs Once Mode

**Loop Mode**:
- Sequence restarts from first step after last step
- Continuous playback of entire sequence
- Good for rhythmic patterns

**Once Mode**:
- Sequence stops after reaching last step
- Gate goes low when sequence stops
- Good for one-time melodic phrases

### 7.3 Sync Modes

**Off**: No external sync, runs independently
**Reset Measure**: Reset to first step at specified bar intervals
**External**: Reset to first step on external gate/clock input

## 8. Practical Examples

### 8.1 Basic Rhythmic Pattern

1. Set up 8 steps with varying durations
2. Assign gate length around half the duration (e.g., 96 ticks for 192-tick steps)
3. Set different note indices for CV output
4. Set Loop mode to On for continuous playback
5. Use short durations for fast notes, longer for sustained notes

### 8.2 External LFO Modulation

1. Create sequence with moderate durations
2. Set up Route A to target Duration
3. Assign steps to Group A
4. Route external LFO to IndexedA target
5. Set modulation amount to ±50%
6. LFO will stretch and compress timing

### 8.3 Polyrythmic Sequence

1. Create sequence with different duration ratios (e.g., 3:4:5)
2. Use different note indices for musical interest
3. Set appropriate gate lengths for desired note overlap
4. Use external sync for precise timing with other tracks

## 9. Shortcuts and Tips

### 9.1 Quick Navigation

- **Page+S1**: Jump to sequence settings page
- **Page+S2**: Jump to step editing page
- **Page+S3**: Jump to track settings page
- **Context Menu**: Long press on context key for options

### 9.2 Quick Edit (Page+Steps 8-15)

- **Page+Step 8**: Split (requires step selection)
- **Page+Step 9**: Merge with next (first selected step only)
- **Page+Step 10**: Swap (hold + encoder to choose offset, release to apply)
- **Page+Step 11**: Run Mode (quick edit list)
- **Page+Step 12**: Piano Voicings (hold + encoder)
- **Page+Step 13**: Guitar Voicings (hold + encoder)
- **Steps 14-15**: Reserved for macros

**Voicing Quick Edit**:
- Hold the quick edit key and turn the encoder to choose a voicing, release to apply
- "OFF" leaves notes unchanged
- "ROOT" uses the first selected step's note index as the root
- "C0" uses the track/project root from C0 (non-accumulating)
- One selected step applies across active steps; multiple selected steps apply only to those steps

### 9.3 Macro Shortcuts (Page+Steps 4, 5, 6, 14)

Macros provide powerful generative and transformative operations on sequences. All macros operate on:
- **Selected steps** (if any are selected)
- **Full active length** (if no selection)

**Page+Step 4 - Rhythm Generators** (YELLOW LED):
- **3/9**: Cycles even subdivisions with 3 steps, then 9 steps (fills 768 ticks)
- **5/20**: Cycles even subdivisions with 5 steps, then 20 steps (fills 768 ticks)
- **7/28**: Cycles even subdivisions with 7 steps, then 28 steps (fills 768 ticks)
- **3-5/5-7**: Cycles grouped patterns (3+5) and (5+7), each fit to 768 ticks
- **M-TALA**: Cycles Khand (5+7), Tihai (2-1-2 x3), Dhamar (5-2-3-4)

**Page+Step 5 - Waveforms** (YELLOW LED):
- **TRI**: Single triangle dip (shorter in the middle, longer at the ends)
- **2TRI**: Two triangle dips across the range
- **3TRI**: Three triangle dips across the range
- **2SAW**: Two saw ramps (short at the start of each segment)
- **3SAW**: Three saw ramps (short at the start of each segment)

**Page+Step 6 - Melodic Generators** (YELLOW LED):
- **SCALE**: Scale fill generator - ascending/descending major scale
- **ARP**: Arpeggio pattern generator - cycles through Up, Down, Up-Down, Triad patterns
- **CHORD**: Chord progression generator - I-IV-V, I-V-vi, ii-V-I progressions
- **MODAL**: Modal melody generator - Dorian, Phrygian, Lydian, Mixolydian modes
- **M-MEL**: Random melody generator - pentatonic scale with random octaves

**Page+Step 14 - Duration & Transform** (YELLOW LED):
- **D-LOG**: Duration logarithmic curve - slow start, accelerating end
- **D-EXP**: Duration exponential curve - fast start, slowing end
- **D-TRI**: Duration triangle curve - accelerate then decelerate
- **REV**: Reverse step order - mirrors all step properties
- **MIRR**: Mirror steps around midpoint - copies first half to second half

**New Macro Features**:
- **Improved Macro Selection**: Macros now operate more reliably with step selection
- **Enhanced Rhythm Generators**: Better distribution algorithms for more musical results
- **Advanced Melodic Generators**: Improved scale and chord generation algorithms
- **Waveform Macros**: More sophisticated waveform generation with better timing quantization
- **Macro Persistence**: Macro settings are now properly saved and restored
- **Enhanced Group Operations**: Better handling of group-based operations in macros

### 9.4 Editing Tips

- Use Shift+encoder for fine duration adjustments
- Split function is great for subdividing longer steps
- Use rotation (First Step) to start sequence from different points
- Group modulation allows for complex interactions
- Minimum gate length (1 tick) creates precise short pulses

### 9.4 Performance Considerations

- Duration calculations use minimal CPU resources
- Group-based modulation adds some processing overhead
- Complex scale mappings may have slight performance impact
- Maximum 48 steps keeps memory usage reasonable

## 10. Troubleshooting

### 10.1 No Output

- Verify track is not muted
- Check that sequence has active steps
- Ensure step durations are not all zero
- Confirm scale settings are appropriate

### 10.2 Unexpected Timing

- Check step durations for expected values
- Verify divisor settings match expectations
- Ensure loop/once mode is set correctly
- Check for zero-duration steps that get skipped

### 10.3 Sync Issues

- Verify sync mode settings
- Check if external sync source is properly configured
- Ensure reset measure values are appropriate
- Confirm sequence is not getting stuck on zero-duration steps
