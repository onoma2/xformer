# Launchpad Support - TuesdayTrack (Algo Track) Interface Research

## Task Summary
Complete research on TuesdayTrack (Algo track) interface and implementation peculiarities for Launchpad integration.

## Track Overview
TuesdayTrack is an algorithmic sequencer inspired by TINRS Tuesday, featuring 15 unique algorithms that generate evolving melodic patterns through parameter combinations.

## Research Findings

### 1. Data Structure Analysis

**TuesdayTrack Structure** (/src/apps/sequencer/model/TuesdayTrack.h):
- Contains array of `TuesdaySequence` objects (CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT)
- Supports PlayMode: Aligned (same as NoteTrack)

**TuesdaySequence Structure** (/src/apps/sequencer/model/TuesdaySequence.h):
- **Core Algorithm Parameters**:
  - Algorithm: 0-14 (15 unique algorithms)
  - Flow: 0-16 (pattern flow direction)
  - Ornament: 0-16 (melodic ornamentation)
  - Power: 0-16 (pattern complexity/intensity)
- **Pattern Control**:
  - Start: 0-16 (pattern starting position)
  - Loop Length: 0-29 (0 = infinite evolving, 1+ = finite loop)
  - Rotate: -loopLength..+loopLength (pattern rotation)
- **Performance Parameters**:
  - Glide: 0-100% (slide probability between notes)
  - Trill: 0-100% (re-trigger probability)
  - Step Trill: 0-100% (intra-step subdivision count)
  - Skew: -8 to +8 (density curve across loop)
- **Timing Parameters**:
  - CvUpdateMode: Free (every step) or Gated (only on gate fire)
  - Divisor: 1-768 ticks (time scaling)
  - Clock Multiplier: 50-150% (clock speed)
  - Reset Measure: 0-128 bars (0 = off)
- **Musical Parameters**:
  - Octave: -10..10 (octave shift)
  - Transpose: -11..11 (semitone shift)
  - Scale: -1 (Project) or 0..N (Track scale)
  - Root Note: -1 (Default) or 0-11 (C-B)
- **Gate Parameters**:
  - Gate Length: 0-100% (gate duration scaling)
  - Gate Offset: 0-100% (gate timing offset)
- **Masking System**:
  - Mask Parameter: 0=ALL, 1-14=mask values, 15=NONE (step skipping)
  - Time Mode: 0=FREE, 1=QRT, 2=1.5Q, 3=3QRT (rhythmic quantization)
  - Mask Progression: 0=NO PROG, 1=PROG+1, 2=PROG+5, 3=PROG+7 (mask evolution)

### 2. Layer Identification

**Primary Layers for Launchpad Control**:
```cpp
enum class Layer {
    Algorithm,         // 0-14 (15 unique algorithms)
    Flow,              // 0-16 (pattern flow)
    Ornament,          // 0-16 (melodic ornamentation)
    Power,             // 0-16 (pattern complexity)
    LoopLength,        // 0-29 (0 = infinite, 1+ = finite loop)
    Rotate,            // -loopLength..+loopLength (pattern rotation)
    Glide,             // 0-100% (slide probability)
    Trill,             // 0-100% (re-trigger probability)
    StepTrill,         // 0-100% (intra-step subdivision)
    Skew,              // -8 to +8 (density curve)
    CvUpdateMode,      // Free or Gated (CV update mode)
    Octave,            // -10..10 (octave shift)
    Transpose,         // -11..11 (semitone shift)
    GateLength,        // 0-100% (gate duration)
    GateOffset,        // 0-100% (gate timing offset)
    MaskParameter,     // 0=ALL, 1-14=mask values, 15=NONE
    TimeMode,          // 0=FREE, 1=QRT, 2=1.5Q, 3=3QRT (rhythmic quantization)
    MaskProgression,   // 0-3 (mask evolution pattern)
};
```

### 3. Value Ranges

**Parameter Ranges with Launchpad Mapping**:
```cpp
// Algorithm (0-14) mapped to 0-7 grid
static const RangeMap algorithmRangeMap = { { 0, 0 }, { 14, 7 } };

// Flow, Ornament, Power (0-16) mapped to 0-7 grid
static const RangeMap flowOrnamentPowerRangeMap = { { 0, 0 }, { 16, 7 } };

// Loop Length (0-29) mapped to 0-7 grid
static const RangeMap loopLengthRangeMap = { { 0, 0 }, { 29, 7 } };

// Rotate (-29..+29) mapped to 0-7 grid (center at 3.5)
static const RangeMap rotateRangeMap = { { -29, 0 }, { 29, 7 } };

// Glide, Trill, StepTrill, GateLength, GateOffset (0-100%) mapped to 0-7 grid
static const RangeMap percentageRangeMap = { { 0, 0 }, { 100, 7 } };

// Skew (-8..+8) mapped to 0-7 grid (center at 3.5)
static const RangeMap skewRangeMap = { { -8, 0 }, { 8, 7 } };

// Octave (-10..10) mapped to 0-7 grid
static const RangeMap octaveRangeMap = { { -10, 0 }, { 10, 7 } };

// Transpose (-11..11) mapped to 0-7 grid
static const RangeMap transposeRangeMap = { { -11, 0 }, { 11, 7 } };

// Mask Parameter (0-15) mapped to 0-7 grid
static const RangeMap maskParameterRangeMap = { { 0, 0 }, { 15, 7 } };
```

### 4. Visualization Requirements

**Grid Layout Strategy**:
- **Page 1 - Algorithm Parameters**: Algorithm (row 0), Flow (row 1), Ornament (row 2), Power (row 3)
- **Page 2 - Pattern Control**: Loop Length (row 0), Rotate (row 1), Skew (row 2), Start (row 3)
- **Page 3 - Performance**: Glide (row 0), Trill (row 1), Step Trill (row 2), CV Mode (row 3)
- **Page 4 - Musical Parameters**: Octave (row 0), Transpose (row 1), Scale (row 2), Root Note (row 3)
- **Page 5 - Timing**: Divisor (row 0), Clock Mult (row 1), Reset Measure (row 2), Gate Length (row 3)
- **Page 6 - Masking System**: Mask Parameter (row 0), Time Mode (row 1), Mask Progression (row 2), Gate Offset (row 3)

**Visualization Methods**:
- **Algorithm Selection**: Color-coded grid with algorithm numbers
- **Flow/Ornament/Power**: Bar heights representing intensity (0-16)
- **Loop Length**: Numeric display or LED segments (0 = infinite)
- **Rotate/Skew**: Bi-polar visual (center is neutral, left negative, right positive)
- **Percentage Parameters**: Bar heights representing percentage (0-100%)
- **Time Mode/Mask Progression**: Icon-based representation of modes
- **Gate Parameters**: Bar heights or LED segments

### 5. Editing Patterns (Proposed Design)

**Page 1 - Algorithm Parameters**:
- **Algorithm Selection**: Grid buttons (row 0, column 0-7) - 15 algorithms spread across 2 pages
- **Flow/Ornament/Power Adjustment**: Grid buttons (rows 1-3, columns 0-7) for parameter values
- **Quick Algorithm Cycle**: Press and hold algorithm button + arrow keys

**Page 2 - Pattern Control**:
- **Loop Length Adjustment**: Grid buttons (row 0, column 0-7)
- **Rotate Adjustment**: Left/Right buttons for fine adjustment, Shift + Left/Right for coarse
- **Skew Adjustment**: Up/Down buttons for bipolar adjustment
- **Start Position**: Top row buttons for pattern starting position

**Page 3 - Performance**:
- **Glide/Trill/Step Trill Adjustment**: Grid buttons (rows 0-2, columns 0-7)
- **CV Mode Toggle**: Press and hold Flow button to toggle Free/Gated

**Page 4 - Musical Parameters**:
- **Octave/Transpose Adjustment**: Grid buttons (rows 0-1, columns 0-7)
- **Scale Selection**: Hold Scale button + grid button for scale selection
- **Root Note Selection**: Hold Root Note button + grid button for root note

**Page 5 - Timing**:
- **Divisor Selection**: Hold Divisor button + grid button for divisor presets
- **Clock Mult Adjustment**: Grid buttons (row 1, column 0-7)
- **Reset Measure Adjustment**: Grid buttons (row 2, column 0-7)

**Page 6 - Masking System**:
- **Mask Parameter Selection**: Grid buttons (row 0, column 0-7)
- **Time Mode Cycle**: Press Time Mode button to cycle through modes (0-3)
- **Mask Progression Cycle**: Press Mask Progression button to cycle through patterns (0-3)

**Navigation Between Pages**:
- Hold Shift + Navigate button to switch between pages
- Visual feedback: Navigate button lights up when in page navigation mode

**Multiple Button Hold Support**:
- **Shift + Layer Button**: Fine adjustment mode
- **Ctrl + Layer Button**: Coarse adjustment mode
- **Hold Layer + Grid Button**: Parameter selection
- **Multi-Layer Selection**: Hold multiple layer buttons for batch operations

### 6. Quickactions and Macros

**Context Menu Operations**:
```
Main Context Menu (TuesdaySequencePage):
- INIT: Clear sequence to default state
- ROUTE: Configure parameter routing
```

**Layer-Specific Quickactions**:
- **Algorithm Quick Edit**: Hold Algorithm button + arrow keys to cycle through algorithms
- **Flow Quick Edit**: Hold Flow button + arrow keys to adjust flow parameter
- **Ornament Quick Edit**: Hold Ornament button + arrow keys to adjust ornamentation
- **Power Quick Edit**: Hold Power button + arrow keys to adjust pattern complexity

**Performance Quickactions**:
- **CV Mode Toggle**: Hold CvUpdateMode button + any grid button to toggle between Free and Gated
- **Glide Toggle**: Double-press Glide button to toggle glide on/off
- **Trill Toggle**: Double-press Trill button to toggle trill on/off

### 7. Advanced Features

**Algorithm System**:
- 15 unique algorithm types with distinct pattern generation characteristics
- Algorithm numbers correspond to TINRS Tuesday algorithm identifiers
- Patterns evolve based on algorithm, flow, ornament, and power parameters

**Pattern Evolution System**:
- Infinite loop mode (loopLength = 0) for continuously evolving patterns
- Finite loop mode with rotation and skew parameters
- Masking system for step skipping and rhythmic variation
- Mask progression for evolving mask patterns

**Musical Parameters**:
- Track-scale selection (-1 = Project scale, 0..N = Track scale)
- Root note configuration (0-11: C-B)
- Note display with scale note names
- Voltage calculation based on scale

**Sync and Timing**:
- Reset measure (0-128 bars)
- Divisor (1-768 ticks)
- Clock multiplier (50-150%)
- Time mode quantization (FREE, QRT, 1.5Q, 3QRT)

## Files to Examine
- `/src/apps/sequencer/model/TuesdayTrack.h`
- `/src/apps/sequencer/model/TuesdayTrack.cpp`
- `/src/apps/sequencer/model/TuesdaySequence.h`
- `/src/apps/sequencer/model/TuesdaySequence.cpp`
- `/src/apps/sequencer/engine/TuesdayTrackEngine.h`
- `/src/apps/sequencer/engine/TuesdayTrackEngine.cpp`
- `/src/apps/sequencer/ui/pages/TuesdaySequencePage.h`
- `/src/apps/sequencer/ui/pages/TuesdaySequencePage.cpp`
- `/src/apps/sequencer/ui/model/TuesdaySequenceListModel.h`

## Expected Output
- Layer mapping definition
- Range mapping tables
- Visualization approach
- Editing interaction patterns

## Status
Complete

## Priority
High