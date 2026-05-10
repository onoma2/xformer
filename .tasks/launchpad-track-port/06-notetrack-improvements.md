# NoteTrack Launchpad Support Improvements

## Current Status
- **Current Layers**: Gate, GateProbability, GateOffset, Slide, Retrigger, RetriggerProbability, Length, LengthVariationRange, LengthVariationProbability, Note, NoteVariationRange, NoteVariationProbability, Condition
- **Missing Features**: PulseCount, GateMode, HarmonyRoleOverride, InversionOverride, VoicingOverride, AccumulatorStepValue, various harmony features
- **Current Visualization**: Limited to bits, bars, dots, notes
- **Current Navigation**: Basic layer navigation

## Subtasks

### 1. Add Missing NoteSequence Layers
- **PulseCount Layer**: Add layer for pulse count (1-8 pulses)
- **GateMode Layer**: Add layer for gate modes (All, First, Hold, FirstLast)
- **Harmony Override Layers**: Add layers for harmony role, inversion, and voicing overrides
- **Accumulator Layer**: Add layer for accumulator step values
- **Layer Map Positioning**: Place new layers in logical positions (rows 4-6)

### 2. Improve Note Visualization
- **Circuit Keyboard Layout**: Implement mebitek-style circuit keyboard for note entry
- **Harmony Visualization**: Visual indicators for harmony roles and overrides
- **Accumulator Visualization**: Special visualization for accumulator step values
- **Pulse Count Visualization**: Bar graphs or dot patterns for pulse counts

### 3. Enhance Navigation System
- **Layer Navigation Buttons**: Better visual feedback for layer selection
- **Range Detection**: Improve navigation for different value ranges
- **Quick Jump Buttons**: Add buttons for quick access to common layers

### 4. Implement Performance Features
- **Performer Mode**: Add performer mode for NoteTrack with sequence length control
- **Live Note Entry**: Support for circuit keyboard note entry in performance mode
- **Quick Step Toggle**: Double-press functionality for quick step toggling

### 5. Add Harmony Features
- **Harmony Role Selection**: Layer for selecting harmony role (Master, Follower Root, Follower 3rd, Follower 5th, etc.)
- **Inversion Control**: Layer for chord inversion selection
- **Voicing Control**: Layer for voicing selection (Close, Drop2, Drop3, Spread)
- **Harmony Visualization**: LED indicators for harmony states

### 6. Improve Gate Control
- **Gate Mode Selection**: Visual feedback for gate modes
- **Pulse Visualization**: Show pulse patterns for different gate modes
- **Gate Offset Visualization**: Better visualization for gate offset values

### 7. Add Accumulator Support
- **Accumulator Step Editing**: Layer for editing accumulator step values
- **Accumulator Visualization**: Visual representation of accumulator behavior
- **Range Mapping**: Range maps for accumulator step values (0-15 to 0-7)

### Files to Modify
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Add layer maps, visualization methods, navigation logic
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Add new methods and layer definitions
- `/src/apps/sequencer/model/NoteSequence.h` - Ensure layer definitions are complete

### Status
Pending

### Priority
High