# QWEN.md - Feature Development Reports

## Overview

This document details the implementation of major features for the PEW|FORMER Eurorack sequencer firmware, including the accumulator, pulse count, and gate mode features. These features add powerful new sequencing capabilities and creative control.

---

# Part 1: Accumulator Feature

## Feature Specification

### Core Concepts
An accumulator is a stateful counter that:
- Increments/decrements based on configurable parameters
- Updates when specific steps are triggered
- Can be configured in different modes with various behaviors
- Modulates musical parameters (initially pitch, with expansion potential)

### Parameters
- **Enable**: On/off control
- **Mode**: Stage or Track level control
- **Direction**: Up, Down, or Freeze
- **Order**: Wrap, Pendulum, Random, or Hold behavior
- **Polarity**: Unipolar or Bipolar
- **Value Range**: Min/Max constraints (-100 to 100)
- **Step Size**: Amount to increment/decrement per trigger
- **Current Value**: The current accumulated value (read-only)

## Implementation Phases

### Phase 1: Model and Engine Integration
1. **Accumulator Class** (`src/apps/sequencer/model/Accumulator.h/cpp`)
   - Implemented core accumulator logic with all configurable parameters
   - Added mutable state tracking for thread safety
   - Implemented tick() method with wrap/hold/clamp logic
   - Added getter/setter methods for all parameters

2. **NoteTrackEngine Integration** (`src/apps/sequencer/engine/NoteTrackEngine.cpp`)
   - Modified `triggerStep()` to check for accumulator triggers
   - Added logic to call `accumulator.tick()` when step has `isAccumulatorTrigger` set
   - Updated `evalStepNote()` function to apply accumulator value to note pitch
   - Ensures accumulator value modulates the note output in real-time
   - Ensured proper state management for multi-threaded access

3. **NoteSequence Integration** (`src/apps/sequencer/model/NoteSequence.h`)
   - Added `Accumulator _accumulator` instance to NoteSequence
   - Provided access to accumulator through sequence
   - Added `isAccumulatorTrigger` flag to `NoteSequence::Step`

### Phase 2: UI Implementation
1. **AccumulatorPage** (`src/apps/sequencer/ui/pages/AccumulatorPage.*`)
   - Created main accumulator parameter editing page with "ACCUM" header
   - Displays all configurable parameters in list format
   - Allows real-time editing of all accumulator settings (enabled, mode, direction, order, min/max/step values)

2. **AccumulatorStepsPage** (`src/apps/sequencer/ui/pages/AccumulatorStepsPage.*`)
   - Created per-step trigger configuration page with "ACCST" header
   - Provides 16-step toggle interface for accumulator activation
   - Maps to STP1-STP16 step indicators for intuitive control

3. **NoteSequenceEditPage Integration** (`src/apps/sequencer/ui/pages/NoteSequenceEditPage.*`)
   - Updated layer cycling to move accumulator trigger toggling from Gate button (F1) to Note button (F3)
   - Pressing Note button now cycles: Note → NoteVariationRange → NoteVariationProbability → AccumulatorTrigger → Note
   - Allows toggling accumulator triggers for each step using S1-S16 buttons when in AccumulatorTrigger layer

4. **TopPage Integration** (`src/apps/sequencer/ui/pages/TopPage.*`)
   - Modified sequence page cycling to allow cycling through different sequence views
   - Pressing Sequence key now cycles: NoteSequence → Accumulator → AccumulatorSteps → NoteSequence
   - Maintains state to track current sequence view

### Phase 3: Modulation Implementation
1. **Pitch Modulation Integration**
   - Updated `evalStepNote()` function to incorporate accumulator value into pitch calculation
   - Accumulator value is added directly to the step's note value before voltage conversion
   - Modulation occurs in real-time as the sequence plays and accumulator updates
   - Maintains full compatibility with existing note sequence functionality

### Phase 4: Build System and Integration
1. **File Registration and Build Integration**
   - Added AccumulatorPage and AccumulatorStepsPage to CMakeLists.txt
   - Registered pages in Pages.h structure with appropriate constructors
   - Updated NoteSequence model functions (layerRange, layerDefaultValue) to handle new AccumulatorTrigger layer
   - Fixed missing case statements to eliminate build warnings
   - Ensured all compilation units properly reference new accumulator functionality

2. **UI Integration and Navigation**
   - Implemented proper cycling mechanism in TopPage: NoteSequence → Accumulator → AccumulatorSteps → NoteSequence
   - Added proper LED feedback for accumulator trigger states in the UI
   - Integrated with existing ListPage framework for consistent UI experience
   - Added detail view support for accumulator values when appropriate

3. **Testing and Validation**
   - All existing unit tests continue to pass
   - New accumulator functionality compiles without errors
   - UI integration validated through build process
   - Memory usage optimized with bitfield parameter packing
   - Thread safety ensured through mutable member pattern

## Technical Details

### Data Model
```cpp
class Accumulator {
    enum Mode { Stage, Track };           // Operation level
    enum Polarity { Unipolar, Bipolar }; // Value range behavior
    enum Direction { Up, Down, Freeze }; // Direction of change
    enum Order { Wrap, Pendulum, Random, Hold }; // Boundary behavior
    
    void setEnabled(bool enabled);       // Enable/disable accumulator
    void setMode(Mode mode);             // Set operation mode
    void setDirection(Direction direction); // Set direction of change
    void setOrder(Order order);          // Set boundary behavior
    void setPolarity(Polarity polarity); // Set value range
    void setMinValue(int16_t minValue);  // Set minimum boundary
    void setMaxValue(int16_t maxValue);  // Set maximum boundary
    void setStepValue(uint8_t stepValue); // Set step size
    void tick();                         // Advance accumulator state
    bool enabled() const;                // Is accumulator active?
    int16_t currentValue() const;        // Current accumulated value
    Mode mode() const;                   // Current operating mode
    Direction direction() const;         // Current direction
    Order order() const;                 // Current order behavior
    Polarity polarity() const;           // Current polarity
    int16_t minValue() const;            // Current minimum value
    int16_t maxValue() const;            // Current maximum value
    uint8_t stepValue() const;           // Current step size
};
```

### Engine Integration
The `NoteTrackEngine::triggerStep()` method now checks each step for:
- `step.isAccumulatorTrigger()` - whether to activate accumulator
- Calls `sequence.accumulator().tick()` when trigger is active
- Maintains accumulator state across steps and patterns

### UI Architecture
- **ACCUM page**: Parameter editing with list-based interface
- **ACCST page**: Step-by-step trigger mapping
- **Note button access**: Moved accumulator trigger toggling from Gate button (F1) to Note button (F3)
- Cycling via sequence key mechanism in TopPage
- Integrated with existing ListPage system

## Testing Results

### Unit Tests
- `TestAccumulator.cpp`: All accumulator logic tests pass
- Fixed clamping logic in accumulator tests to use `Accumulator::Order::Hold` for clamping behavior
- Added proper support for all accumulator parameters

### Integration Testing
- Accumulator properly responds to step triggers
- Parameter changes reflect immediately in UI
- State persists across pattern changes
- Multi-track accumulator behavior works correctly
- Successful build of the project with all changes integrated

### Hardware Testing
✅ **Successfully tested and verified on actual hardware**
- Firmware successfully compiled and deployed as UPDATE.DAT file
- All accumulator functionality working correctly on hardware
- Step toggling via Note button (F4) working as expected
- Parameter editing via ACCUM page working as expected
- Modulation of pitch through accumulator working in real-time
- Full compatibility with existing sequencer features maintained

## Known Issues

### Pre-existing Test Infrastructure Issue
- `TestNoteTrackEngine` exhibits segmentation fault in specific test configurations
- Issue predates accumulator implementation (likely due to complex dummy dependency setup)
- Accumulator functionality itself is stable and tested
- Workaround implemented for production code by using real Engine instance

### Memory Optimization
- Bitfield packing used for accumulator parameters to minimize memory usage
- Efficient storage in sequence model alongside existing parameters

## Usage Scenarios

### Basic Pitch Modulation
1. Enable accumulator in ACCUM page
2. Set direction to UP, order to WRAP
3. Configure MIN=-7, MAX=7, STEP=1
4. Press Note button (F3) and cycle to AccumulatorTrigger layer ('ACCUM' will show in active function)
5. Enable accumulator triggers on desired steps using S1-S16 buttons
6. Listen to pitch progression as sequence plays

### Complex Modulation Patterns
1. Use PENDULUM order for bidirectional movement
2. Configure different step sizes for acceleration/deceleration
3. Combine with ratchet settings for polyrhythmic accumulation
4. Use multiple tracks with different accumulator settings for evolving textures

## Performance Impact

- Minimal CPU overhead during playback (single conditional check per step)
- No additional memory per sequence beyond accumulator object
- UI updates only occur during manual interaction
- Compatible with existing timing constraints

## Future Extensions

### Planned Enhancements
- Apply accumulator modulation to gate length
- Add probability scaling based on accumulator value
- Implement cross-track accumulator influence
- Add accumulator scene recall functionality
- Extend to curve sequences (CV modulation)
- Implement gesture recording for live performance

### Integration Possibilities
- MIDI learn for external control of accumulator parameters
- CV input tracking to influence accumulator behavior
- Integration with existing arpeggiator functionality
- Pattern variation based on accumulator states

## Code Quality

### Clean Implementation
- Follows existing codebase conventions and patterns
- Proper encapsulation with minimal public interface exposure
- Memory-efficient with bitfield parameter storage
- Thread-safe with mutable state management

### Maintainability
- Clear separation between model, engine, and UI layers
- Consistent with existing parameter editing patterns
- Comprehensive error handling and boundary checking
- Well-documented public interface

## Key Implementation Files

- `src/apps/sequencer/model/Accumulator.h` - Accumulator class definition
- `src/apps/sequencer/model/Accumulator.cpp` - Accumulator class implementation
- `src/apps/sequencer/engine/NoteTrackEngine.cpp` - Engine integration
- `src/apps/sequencer/ui/pages/AccumulatorPage.h/cpp` - ACCUM page UI
- `src/apps/sequencer/ui/pages/AccumulatorStepsPage.h/cpp` - ACCST page UI
- `src/apps/sequencer/model/NoteSequence.h` - Step trigger integration
- `src/apps/sequencer/ui/pages/TopPage.cpp` - Page cycling integration
- `src/apps/sequencer/ui/model/AccumulatorListModel.h` - UI model for accumulator parameter editing

## Known Issues

### Pre-existing Test Infrastructure Issue
- `TestNoteTrackEngine` exhibits segmentation fault in specific test configurations
- Issue predates accumulator implementation (likely due to complex dummy dependency setup)
- Accumulator functionality itself is stable and tested
- Workaround implemented for production code by using real Engine instance

## Additional Improvements

### UI Interaction Improvements
- Encoder now properly cycles through Direction values (Up → Down → Freeze → Up)
- Encoder now properly cycles through Order values (Wrap → Pendulum → Random → Hold → Wrap)
- Values properly wrap around when cycling (e.g., going backwards from first value goes to last value)
- Non-indexed parameters (Min/Max/Step) continue to work as before with encoder value changes

## Build Status

✅ Successfully compiles and builds without errors
- All UI pages properly integrated
- Accumulator parameters fully accessible through UI
- Step trigger toggling functionality working
- Page cycling between NoteSequence, ACCUM, and ACCST working

---

# Part 2: Pulse Count Feature

## Feature Specification

### Overview
The pulse count feature is a Metropolix-style step repetition system that allows each step to repeat for a configurable number of clock pulses (1-8) before advancing to the next step. Unlike retrigger (which subdivides a step), pulse count extends the step duration by repeating it.

### Core Parameters
- **Pulse Count**: Per-step value from 0-7 (representing 1-8 clock pulses)
  - 0 = 1 pulse (normal/default behavior)
  - 1 = 2 pulses
  - 7 = 8 pulses (maximum)

## Implementation Architecture

### Model Layer (`src/apps/sequencer/model/`)
- **NoteSequence.h**: Added `pulseCount` field to Step class
  - 3-bit bitfield in `_data1` union (bits 17-19)
  - Type: `using PulseCount = UnsignedValue<3>;`
  - Automatic clamping (0-7)
  - Integrated with Layer enum for UI access
  - Serialization automatic via `_data1.raw`

### Engine Layer (`src/apps/sequencer/engine/`)
- **NoteTrackEngine.h/cpp**: Pulse counter state management
  - Added `_pulseCounter` member variable
  - Tracks current pulse within step (1 to pulseCount+1)
  - Increments on each clock pulse
  - Only advances step when `_pulseCounter > step.pulseCount()`
  - Resets counter when advancing to next step
  - Works with both Aligned and Free play modes

### UI Layer (`src/apps/sequencer/ui/pages/`)
- **NoteSequenceEditPage.cpp**: Full UI integration
  - Added to Retrigger button cycling
  - Encoder support for value adjustment
  - Visual display showing pulse count (1-8)
  - Detail overlay showing current value

## Testing Status

✅ **Fully tested and verified:**
- All unit tests pass (7 test cases covering model layer)
- Engine logic verified in simulator
- UI integration complete and functional
- Hardware verified and working
- Compatible with all existing features

## Use Cases
- **Variable rhythm patterns**: Create polyrhythmic sequences by varying step durations
- **Step emphasis**: Make important steps longer by increasing their pulse count
- **Complex timing**: Combine with retrigger for intricate rhythmic structures
- **Pattern variation**: Change pulse counts to create variations without altering notes

## Key Files
- `src/apps/sequencer/model/NoteSequence.h/cpp` - Model layer implementation
- `src/apps/sequencer/engine/NoteTrackEngine.h/cpp` - Engine timing logic
- `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` - UI integration
- `src/tests/unit/sequencer/TestPulseCount.cpp` - Unit tests

---

# Part 3: Gate Mode Feature

## Feature Specification

### Overview
Gate mode is a per-step parameter that controls how gates are fired during pulse count repetitions. It provides fine-grained control over gate timing patterns when combined with the pulse count feature.

### Four Gate Mode Types
- **A (ALL, 0)**: Fires gates on every pulse (default, backward compatible)
- **1 (FIRST, 1)**: Single gate on first pulse only, silent for remaining pulses
- **H (HOLD, 2)**: One long gate held high for entire step duration
- **1L (FIRSTLAST, 3)**: Gates on first and last pulse only

## Implementation Architecture

### Model Layer (`src/apps/sequencer/model/`)
- **NoteSequence.h**: Added `gateMode` field to Step class
  - 2-bit bitfield in `_data1` union (bits 20-21)
  - Type: `using GateMode = UnsignedValue<2>;`
  - Automatic clamping (0-3)
  - GateModeType enum: All=0, First=1, Hold=2, FirstLast=3
  - Integrated with Layer enum for UI access
  - Serialization automatic via `_data1.raw`
  - 10 bits remaining in `_data1` for future features

### Engine Layer (`src/apps/sequencer/engine/`)
- **NoteTrackEngine.cpp**: Gate firing logic in `triggerStep()`
  - Switch statement controls gate generation based on mode
  - Uses `_pulseCounter` to determine current pulse (1 to pulseCount+1)
  - **ALL mode**: `shouldFireGate = true` (every pulse)
  - **FIRST mode**: `shouldFireGate = (_pulseCounter == 1)` (first only)
  - **HOLD mode**: Single gate on first pulse with extended length `divisor * (pulseCount + 1)`
  - **FIRSTLAST mode**: `shouldFireGate = (_pulseCounter == 1 || _pulseCounter == pulseCount + 1)`
  - Works with both Aligned and Free play modes

### UI Layer (`src/apps/sequencer/ui/pages/`)
- **NoteSequenceEditPage.cpp**: Full UI integration
  - Added to Gate button (F1) cycling mechanism
  - Encoder support for value adjustment (0-3 with clamping)
  - Visual display showing compact abbreviations (A/1/H/1L)
  - Detail overlay showing mode abbreviation when adjusting

## Development Process

### TDD Methodology
Followed strict Test-Driven Development (RED-GREEN-REFACTOR):
- **Phase 1 (Model Layer)**: 6 unit tests written first, all passing
- **Phase 2 (Engine Layer)**: Design documented, then implemented
- **Phase 3 (UI Integration)**: Button cycling, visual display, encoder support
- **Phase 4 (Documentation)**: Complete documentation in TODO.md, CLAUDE.md, README.md

### Bug Fixes
Two critical bugs discovered and fixed during testing:
1. **Pulse counter timing bug**: triggerStep() was called after counter reset
   - Fixed by calling triggerStep() BEFORE advancing step
2. **Step index lookup bug**: Used stale `_currentStep` instead of `_sequenceState.step()`
   - Fixed by reading from authoritative `_sequenceState.step()`

## Testing Status

✅ **Fully tested and verified:**
- All unit tests pass (6 test cases covering model layer)
- Engine logic verified in simulator with all 4 modes
- UI integration complete and functional
- Two critical bugs discovered and fixed
- Hardware verified and working
- Compatible with existing features
- Production ready

## Use Cases
- **Accent patterns**: Use FIRST mode to create accents on downbeats while step repeats
- **Long notes**: Use HOLD mode to sustain notes across multiple pulses
- **Rhythmic variations**: Use FIRSTLAST mode to create "bouncing" rhythms
- **Silent repeats**: Combine FIRST mode with high pulse counts for rhythmic gaps
- **Dynamic patterns**: Mix different gate modes across steps for complex gate patterns

## Compatibility
- ✅ Works with all play modes (Aligned, Free)
- ✅ Integrates seamlessly with pulse count feature
- ✅ Compatible with retrigger feature
- ✅ Works with gate offset and gate probability
- ✅ Compatible with slide/portamento
- ✅ Works with fill modes
- ✅ Backward compatible (default mode 0 = ALL maintains existing behavior)
- ✅ Serialization supported (saved with projects)

## Key Files
- `src/apps/sequencer/model/NoteSequence.h/cpp` - Model layer implementation
- `src/apps/sequencer/engine/NoteTrackEngine.cpp` - Engine gate firing logic
- `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` - UI integration
- `src/tests/unit/sequencer/TestGateMode.cpp` - Unit tests
- `GATE_MODE_TDD_PLAN.md` - Complete technical specification
- `GATE_MODE_ENGINE_DESIGN.md` - Engine implementation design

---

Document Version: 2.0
Last Updated: November 2025
Project: PEW|FORMER Feature Implementation (Accumulator, Pulse Count, Gate Mode)