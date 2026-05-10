# Launchpad Track Porting

## Task Summary
Extend Launchpad controller support to all 6 track types in PER|FORMER/XFORMER sequencer.

## Phase 1 - Track Interface Research (In Progress)

### Research Tasks
1. **IndexedTrack Research** - Duration-based sequencer with 1-48 steps
2. **DiscreteMapTrack Research** - Threshold-based sequencer with 32 stages  
3. **TuesdayTrack Research** - Algorithmic/generative sequencer
4. **MidiCvTrack Research** - MIDI to CV conversion with voice management
5. **TeletypeTrack Research** - Teletype VM integration

Each research task includes:
- Data structure analysis
- Layer identification
- Value range determination
- Visualization requirements
- Editing pattern research

## Current State Analysis

### Track Support Status
- ✅ **NoteTrack**: Fully supported with complete layer-based editing
- ✅ **CurveTrack**: Fully supported with complete layer-based editing  
- ❌ **TeletypeTrack**: No support - only referenced in switch statements with `break;`
- ❌ **IndexedTrack**: No support
- ❌ **DiscreteMapTrack**: No support
- ❌ **TuesdayTrack**: No support
- ❌ **MidiCvTrack**: No support

## Current Launchpad Architecture

### Core Files
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h/cpp`: Main controller
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadDevice.h/cpp`: Base device class
- Model-specific implementations: Mk2, Mk3, Pro, Pro Mk3

### Key Features
- **Sequence Mode**: Layer selection, step range, run mode, sequence editing
- **Pattern Mode**: Pattern selection, latching, syncing, track mute
- **Performer Mode**: Not implemented

### Architecture Patterns
- Layer mapping arrays to associate track layers with grid coordinates
- Range mapping for value conversion
- Mode dispatch macros (`CALL_MODE_FUNCTION`)
- Template methods for type-safe operations
- Device abstraction for multiple Launchpad models

## Track-by-Track Implementation Requirements

### 1. IndexedTrack

**Structure**: 48-step sequences with steps containing note index, duration, gate length, slide
**Key Properties**: active length, loop, run mode, first step

**Implementation Needs**:
- Layer mapping array: `indexedSequenceLayerMap[]`
- Range mappings for each parameter type
- Drawing method: `sequenceDrawIndexedSequence()`
- Editing method: `sequenceEditIndexedStep()`
- Navigation for 48-step sequence (pagination needed)

**Challenges**: Variable step count (1-48) requires navigation/pagination system

### 2. DiscreteMapTrack

**Structure**: 32-stage maps with threshold, direction, note index
**Key Properties**: clock source, sync mode, loop, gate length

**Implementation Needs**:
- Layer mapping array: `discreteMapLayerMap[]`
- Range mappings for thresholds and note indices
- Drawing method: `sequenceDrawDiscreteMapSequence()`
- Editing method: `sequenceEditDiscreteMapStep()`
- Visualization: Thresholds as vertical bars, directions as color coding

**Challenges**: 32 stages require pagination/navigation

### 3. TuesdayTrack

**Structure**: Algorithmic sequences with flow, ornament, power, glide, trill
**Key Properties**: algorithm, flow, ornament, power, start, loop length, glide, trill

**Implementation Needs**:
- Layer mapping array: `tuesdaySequenceLayerMap[]`
- Parameter-based UI with algorithm selection
- Drawing method: `sequenceDrawTuesdaySequence()`
- Editing method: `sequenceEditTuesdayStep()`
- Creative visualization for algorithmic behavior

**Challenges**: Non-sequential nature requires innovative UI design

### 4. MidiCvTrack

**Structure**: MIDI input handling with voice configuration
**Key Properties**: voices, voice config, note priority, pitch bend range, arpeggiator

**Implementation Needs**:
- Layer mapping array: `midiCvLayerMap[]`
- Voice management and arpeggiator settings
- Drawing method: `sequenceDrawMidiCvSequence()`
- Editing method: `sequenceEditMidiCvStep()`
- Real-time MIDI visualization

**Challenges**: Real-time MIDI input requires efficient handling

### 5. TeletypeTrack

**Structure**: Teletype VM with scripts and patterns
**Key Properties**: scripts, patterns, I/O mapping, time base

**Implementation Needs**:
- Script and pattern visualization
- Track-specific UI for VM control
- Drawing method: `sequenceDrawTeletypeSequence()`
- Editing method: `sequenceEditTeletypeStep()`
- Script and pattern management

**Challenges**: Complex VM integration and state management

## Implementation Strategy

### Phase 1: Foundation (High Priority)
1. Create base layer mapping infrastructure for all track types
2. Implement IndexedTrack support (most similar to Note/Curve)
3. Add pagination/navigation system for tracks with >8 steps

### Phase 2: Track Implementations (Medium Priority)
1. Implement DiscreteMapTrack support
2. Add TuesdayTrack support with algorithmic UI
3. Implement MidiCvTrack with MIDI visualization

### Phase 3: Complex Tracks (Low Priority)
1. Implement TeletypeTrack support
2. Optimize performance and responsiveness
3. Add comprehensive test coverage

## Files to Modify
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp`
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h`
- Test files for each new track type

## Reference Implementation
The `/temp-ref/mebitek-performer/` directory contains:
- Additional track types: Stochastic, Logic, Arp
- Performer mode implementation
- Enhanced note keyboard functionality
- More layer types and visualizations

## Status
Active - analysis complete, ready to begin implementation with IndexedTrack

## Next Action
Begin implementing support for IndexedTrack following the existing patterns for Note and Curve tracks