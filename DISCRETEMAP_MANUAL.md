# DiscreteMap Track User Manual

## 1. Introduction

The DiscreteMap Track is a sophisticated sequencer track type that maps input voltages or internal ramps to discrete output stages. Each stage has a threshold, direction (Rise, Fall, Off), and note value that determines when and what voltage is output.

This track type is ideal for:
- External CV processing and mapping
- Complex internal voltage sequences
- Threshold-based musical triggers
- LFO and envelope following applications

The DiscreteMap track provides:
- Up to 32 controllable stages per sequence
- Flexible input sources (Internal ramps or external CV)
- Position and Length threshold modes
- Voltage range control
- Built-in slew processing

## 2. DiscreteMap Track Overview

### 2.1 What is DiscreteMap?

The DiscreteMap track creates a mapping between input values (from internal ramps or external CV sources) and output stages. When the input crosses a stage's threshold in the specified direction, that stage becomes active and its note value is converted to CV output.

Key concepts:
- **Stages**: 32 controllable positions that respond to input thresholds
- **Thresholds**: Voltage levels that trigger stage activation (-100 to +100)
- **Directions**: Rise (↑), Fall (↓), or Off for each stage
- **Note Values**: Output voltages derived from scale and root note settings

### 2.2 Input Sources

The DiscreteMap track supports multiple input sources:

**Internal Sawtooth**:
- A bipolar ±5V ramp that cycles based on the track divisor
- Provides consistent, clocked input for predictable stage triggering
- Good for creating complex internal sequences

**Internal Triangle**:
- A bipolar ±5V triangle wave that cycles based on the track divisor
- Provides smooth transitions with more even distribution across voltage range
- Ideal for smoother threshold crossings

**External**:
- Uses routed CV input as the comparison source
- Allows external LFOs, envelopes, or other CV sources to drive the mapping
- Requires routing setup to connect CV input to the track

### 2.3 Output Modes

The track has two main output behaviors:

**Gate Mode** (default): CV output only updates when a stage becomes active
**Always Mode**: CV output continuously updates regardless of stage changes

## 3. UI Overview

### 3.1 Main Sequence Page

The main page displays the threshold mapping:

- **Top Row**: Stage selection and direction editing (↑/↓/Off)
- **Middle Row**: Threshold adjustment for selected stage
- **Bottom Row**: Note value editing for selected stage
- **Threshold Bar**: Visual representation of all 32 stages and current input position
- **Stage Info**: Current note names and values for all stages
- **Active Stage**: Highlighted in both visual bar and stage info

### 3.2 Footer Controls (Function Keys)

**F1 - Clock Settings**:
- **Source**: Internal Saw, Internal Tri, External
- **Divisor**: Clock division (1-768) with power-of-2 adjustments
- **Loop**: On/Once mode for sweep behavior
- **Gate Length**: 0% (pulse) to 100% gate duration

**F2 - Threshold Settings**:
- **Mode**: Position (absolute voltage) or Length (proportional distribution)
- **Range**: ±5V, 0-5V, 0-2.5V options for voltage window
- **Sync Mode**: Off, Reset (bar-based), External sync

**F3 - Scale/Output Settings**:
- **Scale**: Project scale or track-specific scale selection
- **Root**: Root note for chromatic scale mapping (C to B)
- **Slew**: On/Off to enable smooth voltage transitions
- **CV Update**: Gate (only on stage change) or Always (continuous)

**F4 - Routing Settings**:
- **CV In**: Select routed CV input for External mode
- **Gate**: Routing for gate behavior
- **Fill**: Fill pattern routing
- **Mute**: Track muting routing
- **Reset**: Triggers a hard reset of the ramp and active stage on rising edge
- **Scan**: Interactive stage direction toggling (see 5.4 Scanner)

### 3.3 Stage Editing

The top two rows allow multi-selection editing:
- Hold multiple stage buttons to select multiple stages
- Encoder adjusts all selected stages simultaneously
- Shift + Encoder for fine adjustments (±1 vs ±8 steps)

## 4. Stage Parameters

### 4.1 Direction Settings

Each stage has three direction modes:

**Rise (↑)**:
- Stage triggers when input crosses threshold from below to above
- Visualized as upward arrow in stage display
- Common for envelope followers and attack detection

**Fall (↓)**:
- Stage triggers when input crosses threshold from above to below
- Visualized as downward arrow in stage display
- Common for decay detection and falling LFOs

**Off (-)**:
- Stage is disabled and will not trigger
- Useful for creating gaps in sequences or disabling unused stages

### 4.2 Threshold Parameters

Thresholds define voltage levels where stages trigger:

**Range**: -100 to +100 maps to configured voltage window
- In Position mode: Each threshold maps to absolute voltage
- In Length mode: Thresholds determine proportional segment lengths

**Position Mode**:
- Each threshold maps directly to a voltage in the range window
- Useful for precise voltage mapping
- Example: With ±5V range, threshold 0 = 0V, threshold 50 = +2.5V

**Length Mode**:
- Thresholds determine proportional segment lengths in voltage range
- Total of all threshold values determines distribution
- Useful for creating musical scales and proportional mappings

### 4.3 Note Values

Note values determine output voltage when a stage is active:

**Range**: -63 to +64 maps to output voltage
- Based on selected scale and root note settings
- -63 = lowest possible voltage, +64 = highest
- Converted using scale's note-to-voltage mapping

## 5. Advanced Features

### 5.1 Generator Functions

The DiscreteMap track includes powerful generator functions:

**GEN Function**:
- Access via long-press on STEP button
- Choose generator type:
  - **Random**: Classic randomization
  - **Linear**: Linear ramp/distribution
  - **Logarithmic**: Logarithmic curve
  - **Exponential**: Exponential curve
- Apply to thresholds, notes, or both:
  - **NOTE5** (Wide): Generates notes in full range (-63 to +64)
  - **NOTE2** (Narrow): Generates notes in narrow range (0 to +32)
  - **THR**: Applies to thresholds only
  - **TOG**: Toggle directions (Random mode only)

### 5.2 Range Macros

Quick voltage range setup:
- **Full**: -5V to +5V range
- **Inv**: +5V to -5V range (inverted)
- **Pos**: 0V to +5V range
- **Neg**: -5V to 0V range
- **Top**: +4V to +5V range (narrow top)
- **Btm**: -5V to -4V range (narrow bottom)
- **Ass**: -1V to +4V range (assymetric)
- **BAss**: -2V to +3V range (balanced assymetric)

### 5.3 Group Operations

Multiple stages can be selected and edited simultaneously:
- Hold multiple top row buttons to select multiple stages
- Encoder adjusts all selected stages together
- Shift + encoder for fine adjustment of all selected stages
- Multi-stage threshold editing affects all selected stages

### 5.4 Scanner (DMap Scan)

The Scanner is a unique performance feature that allows external CV to dynamically modify the sequence structure.

- **How it works**: Route a CV source to the **DMap Scan** target.
- **Behavior**: As the CV value changes, it "scans" through the 32 stages. When the scanner enters a stage's segment, it **toggles the direction** of that stage (cycling through ↑ → ↓ → Off).
- **Application**: Use a manual fader or a slow LFO to "draw" or evolve the sequence pattern in real-time.

## 6. External CV Processing

### 6.1 Setup for External CV

To use external CV with DiscreteMap:

1. Set Input Source to "External"
2. Configure CV Input routing to specify source track
3. Set appropriate voltage Range for incoming signal
4. Adjust stage thresholds to match expected CV range
5. Configure appropriate Gate Length for desired response

### 6.2 Applications

**Envelope Following**:
- Feed audio-rate signals through a rectifier or envelope follower
- Map envelope levels to discrete stages for dynamic control
- Use with Length mode for proportional response

**LFO Shaping**:
- Use complex LFO waveforms as input
- Map different LFO voltages to different stages/notes
- Create stepped versions of complex modulation sources

**CV Quantization**:
- Map continuous CV to discrete musical notes
- Use chromatic scale for voltage-to-note conversion
- Apply scale quantization for musical intervals

## 7. Internal Sequencing

### 7.1 Sawtooth Mode

The internal sawtooth provides a linear voltage ramp:
- Sweeps from -5V to +5V based on divisor setting
- Consistent slope for predictable threshold crossings
- Good for creating ascending/descending sequences

### 7.2 Triangle Mode

The internal triangle provides a smooth triangle wave:
- Bipolar ±5V triangle wave based on divisor
- Smooth transitions perfect for Length mode
- Symmetric up/down triggering opportunities

### 7.3 Sweep Control

**Loop vs Once**:
- **Loop**: Internal ramp repeats continuously
- **Once**: Internal ramp runs once and holds final value
- **Gate Length**: Controls duration of gate output per stage

## 8. Practical Examples

### 8.1 Basic Melody Generation

1. Set Input Source to "Internal Saw"
2. Create 8 stages with directions alternating between Rise and Fall
3. Set thresholds evenly spaced across voltage range
4. Assign ascending note values (0, 2, 4, 5, 7, 9, 11, 12) for scale
5. Set Loop mode to On for continuous playback

### 8.2 External LFO Shaping

1. Set Input Source to "External"
2. Route CV Input to source track (e.g., LFO on Track 1)
3. Set Range to ±5V to match typical LFO outputs
4. Create stages with Rise/Fall directions matching LFO shape
5. Assign desired note values for each voltage threshold
6. Enable Slew for smooth transitions between notes

### 8.3 Envelope Follower

1. Connect external envelope follower CV to CV input
2. Set Input Source to "External"
3. Configure 4-6 stages with appropriate thresholds
4. Use Rise directions for attack detection
5. Use Fall directions for release detection
6. Map to different note values for dynamic pitch changes

## 9. Shortcuts and Tips

### 9.1 Quick Navigation

- **F1-F4**: Jump directly to footer parameter pages
- **Long press STEP**: Access Generator functions
- **Long press any stage**: Enter multi-selection mode
- **Shift + Stage**: Select multiple stages for batch editing

### 9.2 Quick Edit (Page+Steps 9-16)

- **Page+Step 9**: Range High
- **Page+Step 10**: Range Low
- **Page+Step 11**: Divisor
- **Page+Step 12**: Even (Distribute active stages evenly)
- **Page+Step 13**: Flip (Invert all directions)
- **Page+Step 14**: Piano Voicing (hold, rotate to choose; release to apply)
- **Page+Step 15**: Guitar Voicing (hold, rotate to choose; release to apply)

**Voicing Quick Edit behavior**:
- First press shows the current bank/voicing; no notes are changed until the encoder is moved.
- Applies to active stages only (direction not Off); uses selected stage note as root.
- If there are fewer active stages than notes in the voicing, it fills as many as possible.
- If there are more active stages, it repeats the voicing with transposition 0, +7, then -7 semitones (cycle).
- Reapplying with the same root/scale/voicing yields the same result.

### 9.3 Editing Tips

- Use Length mode for proportional musical mappings
- Use Position mode for precise voltage control
- Increase Gate Length for longer note sustains
- Use Slew for smooth transitions between stages
- Multi-select stages to edit multiple parameters simultaneously

### 9.4 Performance Considerations

- More active stages may consume slightly more CPU
- External CV routing works with standard routing matrix
- Internal ramps use minimal CPU resources
- Complex scales may have slight performance impact

## 10. Troubleshooting

### 10.1 No Output

- Verify track is not muted
- Check gate mode settings
- Ensure stages have proper directions (not all Off)
- Confirm input voltage is within expected range

### 10.2 Unexpected Behavior

- Check threshold values aren't all the same
- Verify Range settings match input/output expectations 
- Ensure appropriate Input Source is selected
- Check if Length mode vs Position mode is appropriate

### 10.3 Sync Issues

- Verify track divisor settings
- Check if sync mode is interfering with timing
- Ensure external sync source is properly configured if used
