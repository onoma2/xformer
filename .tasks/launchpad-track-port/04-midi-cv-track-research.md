# Launchpad Support - MidiCvTrack Interface Research

> **STATUS: REMOVED FROM PLAN** — MidiCvTrack has no step sequence, no per-step data, and no `isActiveSequence()`. The only grid-editable content would be arpeggiator parameters (5–6 values already editable via LCD). Not worth the effort. This document is kept for historical reference only.

## Task Summary
Research MidiCvTrack interface and implementation peculiarities to inform Launchpad support.

## Track Overview
MidiCvTrack is a track type that handles MIDI to CV conversion with configurable voice assignment and routing.

## Research Areas

### 1. Data Structure Analysis
- Examine MidiCvTrack class definition and structure
- Understand how MIDI messages are received and processed
- Look at CV output generation and routing
- Identify how voice assignment works

### 2. Layer Identification
- Determine what layers should be available for editing
- MIDI input configuration
- Voice assignment and polyphony
- CV output routing
- Performance parameters

### 3. Value Ranges
- Determine valid ranges for each parameter
- MIDI channel and port selection
- Voice count and assignment
- CV output ranges and offsets
- Performance parameter ranges

### 4. Visualization Requirements
- Steps per page (Launchpad grid is 8x8)
- Pagination/navigation system
- MIDI input visualization
- Voice assignment display
- CV output monitoring

### 5. Editing Patterns
- How to select MIDI input source
- How to configure voice assignment
- How to adjust CV output parameters
- Navigation between pages
- Batch editing operations

## Files to Examine
- `/src/apps/sequencer/model/MidiCvTrack.h`
- `/src/apps/sequencer/model/MidiCvTrack.cpp`
- `/src/apps/sequencer/engine/MidiCvTrackEngine.h`
- `/src/apps/sequencer/engine/MidiCvTrackEngine.cpp`
- `/src/apps/sequencer/ui/pages/MidiCvTrackPage.h`
- `/src/apps/sequencer/ui/pages/MidiCvTrackPage.cpp`

## Expected Output
- Layer mapping definition
- Range mapping tables
- Visualization approach
- Editing interaction patterns

## Status
Discarded - Not Needed

## Priority
Low