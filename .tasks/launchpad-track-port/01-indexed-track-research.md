# Launchpad Support - IndexedTrack Interface Research

## Task Summary
Complete research on IndexedTrack interface and implementation peculiarities for Launchpad integration.

## Track Overview
IndexedTrack is a step-based sequencer with unique time-based properties and advanced routing capabilities.

## Research Findings

### 1. Data Structure Analysis

**IndexedTrack Structure** (/src/apps/sequencer/model/IndexedTrack.h):
- Contains array of `IndexedSequence` objects (CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT)
- Supports CvUpdateMode: Gate (only on trigger) or Always (continuous)
- Supports PlayMode: Aligned (same as NoteTrack)
- Global properties: Octave (-10..10), Transpose (-60..60), Slide Time (0..100%)

**IndexedSequence Structure** (/src/apps/sequencer/model/IndexedSequence.h):
- **MaxSteps**: 48 steps (dynamic active length: 1-48)
- **Step Structure** (bit-packed 32-bit + 8-bit group mask):
  - Note index: 7 bits (-63..63)
  - Duration: 10 bits (0..1023 ticks)
  - Gate length: 10 bits (0..1023 ticks)  
  - Slide: 1 bit (boolean)
  - Group mask: 4 bits (Groups A-D)
- **Sequence Properties**:
  - Divisor (1..768 ticks)
  - Clock multiplier (50..150%)
  - Loop mode (enabled/disabled)
  - Run mode (Forward, Reverse, Ping-Pong, Random)
  - Active length (1..48 steps)
  - Scale selection (-1 = Project scale, 0..N = Track scale)
  - Root note (0..11: C-B)
  - First step (rotation offset: 0..activeLength-1)
  - Sync mode (Off, Reset Measure, External)
  - Reset measure (0 = off, 1..128 bars)
  - Advanced routing system with two routes (A and B) and combine modes

### 2. Layer Identification

**Primary Layers for Launchpad Control**:
```cpp
enum class Layer {
    NoteIndex,        // Note index (-63 to +63)
    Duration,         // Duration (0 to 1023 ticks)
    GateLength,       // Gate length (0 to 1023 ticks)
    Slide,            // Slide toggle (on/off)
    GroupMask,        // Group mask (4 bits for groups A-D)
    GlobalOctave,     // Track octave (-10..10)
    GlobalTranspose,  // Track transpose (-60..60)
    GlobalSlide,      // Track slide time (0..100%)
};
```

### 3. Value Ranges

**Parameter Ranges with Launchpad Mapping**:
```cpp
// Note index (-63 to +63) mapped to 0-7 grid
static const RangeMap noteIndexRangeMap = { { -63, 0 }, { 63, 7 } };

// Duration (0-1023 ticks) mapped to 0-7 grid
static const RangeMap durationRangeMap = { { 0, 0 }, { 1023, 7 } };

// Gate length (0-1023 ticks) mapped to 0-7 grid
static const RangeMap gateLengthRangeMap = { { 0, 0 }, { 1023, 7 } };

// Octave (-10..10) mapped to 0-7 grid
static const RangeMap octaveRangeMap = { { -10, 0 }, { 10, 7 } };

// Transpose (-60..60) mapped to 0-7 grid
static const RangeMap transposeRangeMap = { { -60, 0 }, { 60, 7 } };

// Slide time (0-100%) mapped to 0-7 grid
static const RangeMap slideTimeRangeMap = { { 0, 0 }, { 100, 7 } };
```

### 4. Visualization Requirements

**Stages Per Page**: 8 stages per page (Launchpad grid height)
**Pagination**: 6 pages needed for 48 steps

**Visualization Methods**:
- **Note Index**: Note name or numeric value
- **Duration**: Bar height (0-1023 ticks)
- **Gate Length**: Bar height or percentage
- **Slide**: LED indicator (on/off)
- **Group Mask**: Color coding for groups A-D
- **Global Parameters**: Dedicated section on function buttons

### 5. Editing Patterns (Proposed Design)

**Page 1 - Main Step View**:
- **Stage Selection**: Grid buttons (row 0-7, column 0-7) - 8 steps per page
- **Note Index Adjustment**: Use Up/Down buttons to change selected step's note
- **Duration Adjustment**: Use Up/Down buttons + Shift to change duration
- **Gate Length Adjustment**: Use Up/Down buttons + Ctrl to change gate length
- **Slide Toggle**: Press step button + Shift to toggle slide

**Page 2 - Global Parameters**:
- **Octave Adjustment**: Function button 0
- **Transpose Adjustment**: Function button 1
- **Slide Time Adjustment**: Function button 2
- **Scale Selection**: Function button 3
- **Root Note Adjustment**: Function button 4
- **Loop Toggle**: Function button 5
- **Run Mode Selection**: Function button 6
- **Sync Mode Selection**: Function button 7

**Page 3 - Quickactions & Macros**:
- **Insert Step**: Insert new step at current position
- **Split Step**: Split step into two halves
- **Delete Step**: Remove current step
- **Copy Step**: Copy current step to clipboard
- **Paste Step**: Paste step from clipboard
- **Quick Edit**: Hold Shift for fine adjustments
- **Multi-Step Selection**: Hold multiple stage buttons for batch operations

**Navigation Between Pages**:
- Hold Shift + Navigate button to switch between Page 1 (Main), Page 2 (Global), and Page 3 (Quickactions)
- Visual feedback: Navigate button lights up when in toggle page

**Multiple Button Hold Support**:
- **Shift + Stage Button**: Toggle slide
- **Shift + Up/Down**: Fine adjustment
- **Ctrl + Up/Down**: Gate length adjustment
- **Multiple Stage Buttons**: Batch selection/editing
- **Function Buttons with Grid**: Layer-specific operations

### 6. Quickactions and Macros

**Context Menu Operations**:
```
Main Context Menu (IndexedSequencePage):
- INIT: Clear entire sequence to default state
- ROUTE: Configure parameter routing

Steps Context Menu (IndexedStepsPage):
- INSERT: Insert new step at current position
- SPLIT: Split step into two halves (first half ceil duration, second half floor)
- DELETE: Remove current step (minimum 1 step)
- COPY: Copy current step to clipboard
- PASTE: Paste step from clipboard

Step Editing Actions:
- Note Index: Shift = octave (12 steps), normal = semitone
- Duration: Shift = divisor, normal = 1 tick
- Gate Length: Shift = fine (1 tick), normal = coarse (5 ticks)
```

### 7. Advanced Features

**Routing System**:
- Two independent routes (A and B)
- Route sources: Off, A, B
- Route targets: Duration, Gate Length, Note Index
- Combine modes: A to B, Mux, Min, Max
- Target groups: All, Ungrouped, Selected, or specific groups (A-D)
- Amount scaling: -200% to +200%

**Scale and Root Note**:
- Track-scale selection (-1 = Project scale, 0..N = Track scale)
- Root note configuration (0-11: C-B)
- Note display with scale note names
- Voltage calculation based on scale

**Sync and Timing**:
- Sync modes: Off, Reset Measure, External
- Reset measure (0-128 bars)
- Divisor (1-768 ticks)
- Clock multiplier (50-150%)

## Files to Examine
- `/src/apps/sequencer/model/IndexedTrack.h`
- `/src/apps/sequencer/model/IndexedTrack.cpp`
- `/src/apps/sequencer/model/IndexedSequence.h`
- `/src/apps/sequencer/model/IndexedSequence.cpp`
- `/src/apps/sequencer/engine/IndexedTrackEngine.h`
- `/src/apps/sequencer/engine/IndexedTrackEngine.cpp`
- `/src/apps/sequencer/ui/pages/IndexedSequencePage.h`
- `/src/apps/sequencer/ui/pages/IndexedSequencePage.cpp`
- `/src/apps/sequencer/ui/pages/IndexedStepsPage.h`
- `/src/apps/sequencer/ui/pages/IndexedStepsPage.cpp`

## Expected Output
- Layer mapping definition
- Range mapping tables
- Visualization approach
- Editing interaction patterns

## Status
Complete

## Priority
High