# Indexed Track User Manual

## 1. Introduction

The Indexed Track is a sophisticated sequencer track type that maps step indices to discrete output voltages using duration-based timing. Each step has an independent duration in clock ticks and a note index that determines the output voltage when the step is active.

This track type is ideal for:
- Precise timing control with arbitrary step durations
- Complex rhythmic patterns with non-musical divisions
- CV sequences with custom timing relationships
- External modulation of step parameters

The Indexed track provides:
- Up to 32 controllable steps per sequence
- Independent duration control for each step (0-65535 ticks)
- Direct voltage lookup without octave math
- Group-based CV modulation
- Flexible sync modes

## 2. Indexed Track Overview

### 2.1 What is Indexed?

The Indexed track creates a sequence where each step has an independent duration and note value. Unlike traditional sequencers that divide a measure into equal parts, each step in an Indexed sequence can have completely different timing, allowing for complex rhythmic patterns and precise timing control.

Key concepts:
- **Steps**: 32 controllable positions with independent duration and note values
- **Durations**: Clock tick counts (0-65535) that determine step length
- **Note Indices**: Voltage lookup indices (-63 to +64) that determine output voltage
- **Groups**: A, B, C, D group membership for conditional CV modulation

### 2.2 Timing System

The Indexed track uses a duration-based timing system:

**Step Timer**:
- Counts up from 0 to the current step's duration
- When reaching the duration value, advances to the next step
- Provides precise timing control with tick-level accuracy

**Gate Timer**:
- Counts down independently from calculated gate duration
- Determines how long gate output remains high
- Can be percentage-based or trigger mode

**Duration Control**:
- Each step has its own duration in clock ticks
- Allows for completely arbitrary timing relationships
- Can create complex polyrhythms and non-musical divisions

### 2.3 Output Modes

The track has two main output behaviors:

**Gate Mode** (default): CV output only updates when a step becomes active
**Always Mode**: CV output continuously updates regardless of step changes

## 3. UI Overview

### 3.1 Main Sequence Page

The main page displays sequence settings:

- **Top Row**: Divisor, loop mode, active length settings
- **Middle Row**: Scale selection and root note settings
- **Bottom Row**: Sync mode and reset measure settings
- **Context Menu**: Init and Route options via long-press

### 3.2 Step Editing Page (Page+S2)

The step editing page shows three rows per step:

- **Note Row**: Edit note index for voltage output (±1 semitone, Shift=±12)
- **Duration Row**: Edit step duration in clock ticks (±1 tick, Shift=±divisor)
- **Gate Row**: Edit gate length percentage or trigger mode (±10%, Shift=±1%)

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
- **Divisor**: Clock division (1-768) with power-of-2 adjustments
- **Loop**: On/Once mode for sequence behavior
- **Active Length**: Dynamic step count (1-32) for sequence length

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
- **Split**: Divide a step's duration into two equal parts
- **Delete**: Remove step, shifting subsequent steps
- **Copy**: Copy current step to clipboard
- **Paste**: Paste clipboard content to current step

## 4. Step Parameters

### 4.1 Duration Parameters

Duration determines how long a step remains active:

**Range**: 0 to 65535 clock ticks
- 0 duration steps are skipped immediately
- Higher values create longer step durations
- Independent for each step in sequence

**Timing Control**:
- Normal encoder: ±1 tick adjustment
- Shift + encoder: ±divisor adjustment for musical timing
- Allows for precise timing relationships

**Practical Ranges**:
- 1 tick: Shortest possible step (rarely used)
- 48 ticks: 32nd note at 192 PPQN
- 192 ticks: Quarter note at 192 PPQN
- 768 ticks: Whole note at 192 PPQN

### 4.2 Gate Parameters

Gate length determines how long gate output remains high:

**Percentage Mode** (0-100%):
- Gate duration is percentage of step duration
- 50% means gate is high for half the step duration
- 100% means gate stays high for entire step

**Trigger Mode**:
- Special value that creates fixed short pulse (3 ticks)
- Independent of step duration
- Useful for triggering drums or envelopes

**Editing**:
- Normal encoder: ±10% adjustment
- Shift + encoder: ±1% adjustment for fine control

### 4.3 Note Index Parameters

Note index determines output voltage when step is active:

**Range**: -63 to +64 maps to output voltage
- Direct lookup in selected scale
- No octave math or modulo operations
- -63 = lowest possible voltage, +64 = highest

**Scale Mapping**:
- Based on selected scale and root note settings
- -63 = lowest scale note, +64 = highest scale note
- Converted using scale's note-to-voltage mapping

## 5. Advanced Features

### 5.1 Step Operations

The Indexed track includes powerful step operations:

**Insert Function**:
- Add new step at current position
- Shifts subsequent steps to the right
- Clones properties from previous step

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

### 5.4 Math Operations (F5)

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
2. Assign gate length of 50% to each step
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

### 9.2 Quick Edit (Page+Steps 9-16)

- **Page+Step 9**: Split (requires step selection)
- **Page+Step 10**: Swap (requires step selection, hold + rotate to choose offset)
- **Page+Step 11**: Merge with next (first selected step only)
- **Page+Step 12**: Set First Step (rotates sequence to start at selected step)
- **Page+Step 13**: Active Length
- **Page+Step 14**: Run Mode
- **Page+Step 15**: Reset Measure

### 9.3 Editing Tips

- Use Shift+encoder for fine duration adjustments
- Split function is great for subdividing longer steps
- Use rotation (First Step) to start sequence from different points
- Group modulation allows for complex interactions
- Trigger mode creates precise short pulses

### 9.4 Performance Considerations

- Duration calculations use minimal CPU resources
- Group-based modulation adds some processing overhead
- Complex scale mappings may have slight performance impact
- Maximum 32 steps keeps memory usage reasonable

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
