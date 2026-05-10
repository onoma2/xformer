# Launchpad Track Porting - Phase 2: NoteTrack & CurveTrack Improvements

## Overview
This phase focuses on enhancing the existing NoteTrack and CurveTrack Launchpad support by adding missing features and implementing mebitek-inspired improvements.

## Current Status
- **NoteTrack**: Currently supports 13 layers, missing harmony, accumulator, and advanced gate features
- **CurveTrack**: Currently supports 7 layers, missing chaos, filter/folder, and advanced gate features
- **Both Tracks**: Missing performer mode, circuit keyboard, and advanced visualization

## Goals
1. Add all missing layers for both track types
2. Implement mebitek-inspired circuit keyboard for note entry
3. Add performer mode for both track types
4. Enhance visualization with new LED styles
5. Improve navigation system
6. Add advanced harmony and chaos features

## Subtasks

### NoteTrack Improvements (06-notetrack-improvements.md)
1. Add missing NoteSequence layers: PulseCount, GateMode, HarmonyRoleOverride, InversionOverride, VoicingOverride, AccumulatorStepValue
2. Implement circuit keyboard layout for note entry
3. Add harmony features: role selection, inversion control, voicing control
4. Improve gate control and visualization
5. Add accumulator support with special visualization
6. Enhance navigation system

### CurveTrack Improvements (07-curvetrack-improvements.md)
1. Add missing CurveSequence layers: Chaos, FilterFolder, AdvancedGateMode, EventGateBits
2. Implement advanced gate features with visualization
3. Add chaos system support with dynamic visualization
4. Add filter/folder support
5. Improve curve shape control and visualization
6. Enhance navigation system

### Shared Improvements
1. **Performer Mode**: Implement performer mode for both tracks with sequence length control and follow mode
2. **Circuit Keyboard**: Add circuit-style keyboard for NoteTrack
3. **Visualization Enhancements**: Add new LED styles for different parameter types
4. **Navigation System**: Improve layer navigation with better visual feedback
5. **Performance Optimizations**: Implement double-press functionality and quick step toggling

## Files to Modify
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Layer maps, visualization methods, navigation logic
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - New methods and layer definitions
- `/src/apps/sequencer/model/NoteSequence.h` - Layer definitions and range information
- `/src/apps/sequencer/model/CurveSequence.h` - Layer definitions and range information

## Phase 2 Timeline
1. **Week 1**: Add missing layers for both track types
2. **Week 2**: Implement performer mode and circuit keyboard
3. **Week 3**: Add harmony and chaos features
4. **Week 4**: Finalize visualization and navigation improvements
5. **Week 5**: Testing and bug fixing

## Success Criteria
- All NoteSequence layers are accessible via Launchpad
- All CurveSequence layers are accessible via Launchpad  
- Performer mode is implemented for both tracks
- Circuit keyboard layout works for note entry
- All new features have appropriate visualization
- Navigation system is intuitive and responsive

## Status
Active - Ready to begin implementation

## Priority
High