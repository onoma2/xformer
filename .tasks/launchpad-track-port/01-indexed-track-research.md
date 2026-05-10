# Launchpad Support - IndexedTrack Interface Research

## Task Summary
Research IndexedTrack interface and implementation peculiarities to inform Launchpad support.

## Track Overview
IndexedTrack is a duration-based sequencer track inspired by Orthogonal Devices ER-101/102.

## Key Properties
- **Step Count**: 1-48 steps
- **Step Structure**: Each step contains note index, duration, gate length, slide
- **Sequence Properties**: Active length, loop, run mode, first step
- **Playback**: Non-linear playback with indexing

## Research Areas

### 1. Data Structure Analysis
- Examine IndexedTrack class definition
- Understand step structure and data storage
- Identify how sequences are stored and accessed
- Look for existing visualization methods

### 2. Layer Identification
- Determine what layers should be available for editing
- Note index
- Duration
- Gate length
- Slide
- Any other relevant properties

### 3. Value Ranges
- Determine valid ranges for each parameter
- Note index range
- Duration range
- Gate length range
- Slide behavior

### 4. Visualization Requirements
- Steps per page (Launchpad grid is 8x8)
- Pagination/navigation system
- Step visualization (dots, bars, colors)
- Duration representation

### 5. Editing Patterns
- How to edit note indexes
- How to adjust durations
- Gate length and slide toggling
- Range selection and manipulation

## Files to Examine
- `/src/apps/sequencer/model/IndexedTrack.h`
- `/src/apps/sequencer/model/IndexedTrack.cpp`
- `/src/apps/sequencer/engine/IndexedTrackEngine.h`
- `/src/apps/sequencer/engine/IndexedTrackEngine.cpp`
- `/src/apps/sequencer/ui/page/IndexedTrackPage.h`
- `/src/apps/sequencer/ui/page/IndexedTrackPage.cpp`

## Expected Output
- Layer mapping definition
- Range mapping tables
- Visualization approach
- Editing interaction patterns

## Status
Pending

## Priority
High