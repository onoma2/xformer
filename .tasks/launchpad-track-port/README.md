# Launchpad Track Porting

## Task Summary
Extend Launchpad controller support to all 5 grid-editable track types in PER|FORMER/XFORMER sequencer.

## Phase 1 - Track Interface Research (In Progress)

### Research Tasks
1. **IndexedTrack Research** - Duration-based sequencer with 1-48 steps
2. **DiscreteMapTrack Research** - Threshold-based sequencer with 32 stages  
3. **TuesdayTrack Research** - Algorithmic/generative sequencer

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
- ❌ **IndexedTrack**: No support
- ❌ **DiscreteMapTrack**: No support
- ❌ **TuesdayTrack**: No support
- ❌ **MidiCvTrack**: No support — removed from plan (no sequence data)
- ❌ **TeletypeTrack**: No support — discarded from plan (VM text-input, not grid-editable)

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

**Status**: REMOVED — no sequence data, arpeggiator only. Lowest value for Launchpad grid editing.

### 5. TeletypeTrack

**Status**: DISCARDED — not a grid-editable track type. Teletype is a VM-based script environment; text input and script management do not map to an 8×8 step grid. Any future Teletype Launchpad integration should be a separate dedicated task.

## Implementation Strategy

### Phase 1: Foundation (High Priority)
1. Create base layer mapping infrastructure for all track types
2. Implement IndexedTrack support (most similar to Note/Curve)
3. Add pagination/navigation system for tracks with >8 steps

### Phase 2: Track Implementations (Medium Priority)
1. Implement TuesdayTrack support with algorithmic UI
2. Implement DiscreteMapTrack support
3. Implement IndexedTrack support

### Phase 3: Polish & Deferred Features (Low Priority)
1. Add Macro Grid v2 (Indexed macros after model refactor)
2. Optimize performance and responsiveness
3. Add comprehensive test coverage

## Files to Modify
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp`
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h`
- Test files for each new track type

## VinxScorza / Modulove Launchpad Improvements (Merged)

The following improvements from the `vinx-modulove-improvements` task have been merged into this workstream:

| # | Segment | Effort | Description |
|---|---------|--------|-------------|
| A | **LP Style / LP Note Style settings** | ~1.5h | `classic`/`blue` color schemes + `classic`/`circuit` note entry styles |
| B | **Circuit Keyboard** | ~2h | Circuit-style keyboard overlay for Note track note layer |
| C | **Generators Mode** | ~4h | Split architecture (Note vs. non-Note), preview/apply workflow with A/B state |
| D | **1-Level Undo/Redo** | ~1h | `Shift + Play` shortcut mirroring hardware `PAGE + S7` undo |
| E | **Track Selection Locking** | ~1.5h | Modal/UI-kind/top-page locking to prevent accidental track/scene switches |
| F | **Performer Mode** | ~3h | Scene mute/solo/fill, track selection, follow mode |
| G | **Enhanced Button Events** | ~1.5h | Double-tap, long-press, improved state tracking |
| H | **Visual Feedback** | ~2h | Octave lines, pattern visualization, requested-pattern dimmed colors, mute status on scene buttons |
| I | **Interaction Improvements** | ~1h | Fill functionality, range editing, run mode selection enhancements |
| J | **Layer Selection Optimization** | ~1h | Better layer mapping visualization and quick access |

**Vinx/Modulove subtotal: ~18.5h**

## Reference Implementation
The `/temp-ref/mebitek-performer/` directory contains:
- Additional track types: Stochastic, Logic, Arp
- Performer mode implementation
- Enhanced note keyboard functionality
- More layer types and visualizations

The `/temp-ref/vinx-performer/` directory contains:
- LP Style / LP Note Style settings
- Generators mode with preview/apply
- Circuit keyboard implementation
- Track selection locking

The `/temp-ref/modulove-performer/` directory contains:
- Enhanced button event handling
- Visual feedback enhancements
- Performer mode with scene control

## Updated Implementation Strategy

### Phase 0: Pre-implementation (Required)
1. Split `LaunchpadController.cpp` into per-track files
2. Add `isEdited()` to DiscreteMapSequence, IndexedSequence, TuesdaySequence
3. Verify all `Project.h` accessors compile
4. Add `selectedLayer` to `_sequence` state struct

### Phase 1: Foundation + NoteTrack Fixes (~3h)
1. Add LP Style and LP Note Style settings (`UserSettings.h`)
2. Fix 6 missing NoteSequence layers, step highlighting, mode selector
3. Implement circuit keyboard for note editing

### Phase 2: Track Implementations (~8.5h)
1. Implement TuesdayTrack step-key grid (~2.5h)
2. Implement DiscreteMapTrack 32-stage editor (~3h)
3. Implement IndexedTrack 48-step grid editor (~3.5h)

### Phase 3: Vinx/Modulove Enhancements (~12h)
1. Performer mode with scene mute/solo/fill (~3h)
2. Generators mode with preview/apply workflow (~4h)
3. Enhanced button events + undo/redo (~2.5h)
4. Visual feedback + layer optimization + track status (~2.5h)

### Phase 4: Polish & Deferred (~3h)
1. Add Macro Grid v2 (Curve + DiscreteMap; **Indexed blocked by `indexed-sequence-macro-refactor`**)
2. Optimize performance and responsiveness
3. Add comprehensive test coverage

**Updated Total: ~26.5h** (track port ~10–13h + vinx/modulove ~18.5h with overlap)

## Dependencies

| Task | Relationship | Notes |
|------|-------------|-------|
| `indexed-sequence-macro-refactor` | **Blocks Phase 4 Macro Grid v2** for IndexedTrack | ~4.5h separate task; can run in parallel with Phases 1-3 |
| `teletype-file-reliability` | Independent | No overlap |
| `vinx-modulove-improvements` | LP items merged here | Non-LP items (microtiming, LFOs) remain separate |

## Status
Active — planning reviewed, corrected, and expanded with vinx/modulove segments. Ready to begin implementation.

## Next Action
Split `LaunchpadController.cpp` into per-track files, then begin Phase 0 pre-flight tasks.