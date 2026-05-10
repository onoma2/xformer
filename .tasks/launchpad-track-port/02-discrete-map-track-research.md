# Launchpad Support - DiscreteMapTrack Interface Research

## Task Summary
Research DiscreteMapTrack interface and implementation peculiarities to inform Launchpad support.

## Track Overview
DiscreteMapTrack is a threshold-based sequencer track inspired by New Systems Instruments Discrete Map.

## Key Properties
- **Stage Count**: 32 stages
- **Stage Structure**: Each stage contains threshold, direction, note index
- **Sequence Properties**: Clock source, sync mode, loop, gate length
- **Playback**: Triggered by input CV crossing thresholds

## Research Findings

### 1. Data Structure Analysis

**DiscreteMapTrack Structure** (/src/apps/sequencer/model/DiscreteMapTrack.h):
- Contains array of `DiscreteMapSequence` objects (CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT)
- Supports CvUpdateMode: Gate (only on trigger) or Always (continuous)
- Supports PlayMode: Aligned (same as NoteTrack)

**DiscreteMapSequence Structure** (/src/apps/sequencer/model/DiscreteMapSequence.h):
- 32 stages stored as array of `Stage` objects
- Each stage has:
  - Threshold (-100 to +100)
  - Direction (Rise, Fall, Off, Both)
  - Note index (-63 to +63)
- Additional properties:
  - Clock source: Internal Saw, Internal Triangle, External CV
  - Sync mode: Off, Reset Measure, External
  - Divisor (1-768 ticks)
  - Clock multiplier (50-150%)
  - Loop mode (enabled/disabled)
  - Gate length (0-100%)
  - Threshold mode: Position (absolute) or Length (proportional)
  - Scale selection (-1 = Project scale, 0..N = Track scale)
  - Root note (0-11: C-B)
  - Slew time (0-100%)
  - Pluck (-100..100)
  - Octave (-10..10)
  - Transpose (-60..60)
  - Offset (-500..500 centivolts)
  - Range high/low (-5.0V to +5.0V)

### 2. Layer Identification

**Primary Layers for Launchpad Control**:
```cpp
enum class Layer {
    Threshold,        // Threshold value (-100 to +100)
    Direction,        // Trigger direction (Rise, Fall, Off, Both)
    NoteIndex,        // Note index (-63 to +63)
    GateLength,       // Gate length (0-100%)
    SlewTime,         // Slew time (0-100%)
    Pluck,            // Pluck (-100..100)
    Octave,           // Octave (-10..10)
    Transpose         // Transpose (-60..60)
};
```

### 3. Value Ranges

**Parameter Ranges with Launchpad Mapping**:
```cpp
// Threshold (-100 to +100) mapped to 0-7 grid
static const RangeMap thresholdRangeMap = { { -100, 0 }, { 100, 7 } };

// Note index (-63 to +63) mapped to 0-7 grid
static const RangeMap noteIndexRangeMap = { { -63, 0 }, { 63, 7 } };

// Gate length (0-100%) mapped to 0-7 grid
static const RangeMap gateLengthRangeMap = { { 0, 0 }, { 100, 7 } };

// Slew time (0-100%) mapped to 0-7 grid
static const RangeMap slewTimeRangeMap = { { 0, 0 }, { 100, 7 } };

// Pluck (-100..100) mapped to 0-7 grid
static const RangeMap pluckRangeMap = { { -100, 0 }, { 100, 7 } };

// Octave (-10..10) mapped to 0-7 grid
static const RangeMap octaveRangeMap = { { -10, 0 }, { 10, 7 } };

// Transpose (-60..60) mapped to 0-7 grid
static const RangeMap transposeRangeMap = { { -60, 0 }, { 60, 7 } };
```

### 4. Visualization Requirements (Proposed Design)

**Single Page View for All 32 Stages**:
- **Top Half (Rows 0-3)**: Stage selection and thresholds
- **Bottom Half (Rows 4-7)**: Note indices and directions

**Grid Layout**:
- 4 columns × 8 rows = 32 stages total
- Each stage occupies 2 rows (top = threshold, bottom = note)
- Column 0-3: Stage groups 0-7, 8-15, 16-23, 24-31

**Visualization Methods**:
- **Threshold**: Vertical bar height (0-7)
- **Direction**: Color coding:
  - Rise: Green
  - Fall: Red  
  - Off: Black
  - Both: Yellow
- **Note Index**: Dot patterns or note names
- **Gate Length**: Bar height
- **Slew Time**: Bar height
- **Pluck**: Bar height (bipolar - center is 0)
- **Octave/Transpose**: Numeric display or bar height

### 5. Editing Patterns (Proposed Design)

**Page 1 - Main Stage View**:
- **Stage-Pitch Selection**: Press any bottom-row (note) button to select stage-pitch
- **Stage-Threshold Selection**: Press any top-row (threshold) button to select stage-threshold
- **Nudge Stage Left/Right**: Use arrow buttons (Left/Right) to move selected stage
- **Threshold Adjustment**: Use Up/Down buttons to change selected stage's threshold
- **Note Index Adjustment**: Use Up/Down buttons + Shift to change selected stage's note
- **Quick Edit**: Hold Shift for fine adjustments
- **Clear Stages**: Hold Fill + stage button to clear stage
- **Randomize**: Hold Shift + Fill for randomization
- **Multi-Stage Selection**: Hold multiple stage buttons for batch operations

**Page 2 - Toggle Controls**:
- **Direction Toggle**: 4-way toggle using function buttons:
  - Button 0: Rise
  - Button 1: Fall  
  - Button 2: Both
  - Button 3: Off
- **Threshold Mode Toggle**: Function button 4 (Position/Length)
- **Clock Source Toggle**: Function button 5 (Internal Saw/Internal Triangle/External)
- **Sync Mode Toggle**: Function button 6 (Off/Reset Measure/External)
- **Loop Toggle**: Function button 7 (Loop/Once)

**Navigation Between Pages**:
- Hold Shift + Navigate button to switch between Page 1 (Main) and Page 2 (Toggles)
- Visual feedback: Navigate button lights up when in toggle page

**Multiple Button Hold Support**:
- **Shift + Stage Button**: Fine adjustment mode
- **Fill + Stage Button**: Clear operation
- **Shift + Fill**: Randomization
- **Multiple Stage Buttons**: Batch selection/editing
- **Function Buttons with Grid**: Layer-specific operations

**Advantages of This Approach**:
- All 32 stages visible at once on Page 1 - no pagination needed
- Separate Page 2 for toggle controls keeps interface clean
- Intuitive top-bottom split for threshold/note relationships
- Direct selection of stage-pitch or stage-threshold
- Four-way toggle on separate page simplifies complex operations
- Efficient use of Launchpad's 8x8 grid real estate
- Multiple button hold support enables advanced operations

## Files to Examine
- `/src/apps/sequencer/model/DiscreteMapTrack.h`
- `/src/apps/sequencer/model/DiscreteMapTrack.cpp`
- `/src/apps/sequencer/model/DiscreteMapSequence.h`
- `/src/apps/sequencer/model/DiscreteMapSequence.cpp`
- `/src/apps/sequencer/engine/DiscreteMapTrackEngine.h`
- `/src/apps/sequencer/engine/DiscreteMapTrackEngine.cpp`
- `/src/apps/sequencer/ui/pages/DiscreteMapStagesPage.h`
- `/src/apps/sequencer/ui/pages/DiscreteMapStagesPage.cpp`
- `/src/apps/sequencer/ui/pages/DiscreteMapSequencePage.h`
- `/src/apps/sequencer/ui/pages/DiscreteMapSequencePage.cpp`

## Expected Output
- Layer mapping definition
- Range mapping tables
- Visualization approach
- Editing interaction patterns

## Status
In Progress

## Priority
High