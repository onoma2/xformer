# Launchpad Support - TuesdayTrack Interface Research

## Task Summary
Research TuesdayTrack interface and implementation peculiarities to inform Launchpad support.

## Track Overview
TuesdayTrack is a generative algorithmic sequencer track inspired by TINRS Tuesday.

## Key Properties
- **Algorithmic**: Non-sequential, parameter-driven generation
- **Key Parameters**: Algorithm, flow, ornament, power, start, loop length, glide, trill
- **Playback**: Deterministic generation based on seed and parameters

## Research Areas

### 1. Data Structure Analysis
- Examine TuesdayTrack class definition
- Understand algorithm selection and parameter storage
- Identify how sequences are generated
- Look for existing visualization methods

### 2. Layer Identification
- Determine what layers should be available for editing
- Algorithm selection
- Flow parameters
- Ornament parameters
- Power settings
- Loop configuration
- Performance controls

### 3. Value Ranges
- Determine valid ranges for each parameter
- Algorithm selection (16 algorithms)
- Flow (0-100%)
- Ornament (0-100%)
- Power (0-100%)
- Loop length (1-64 steps)

### 4. Visualization Requirements
- Algorithm selection interface
- Parameter adjustment controls
- Generated sequence visualization
- Real-time performance indicators

### 5. Editing Patterns
- How to select algorithms
- How to adjust flow, ornament, power
- Loop configuration
- Performance controls (start, stop, reset)

## Files to Examine
- `/src/apps/sequencer/model/TuesdayTrack.h`
- `/src/apps/sequencer/model/TuesdayTrack.cpp`
- `/src/apps/sequencer/engine/TuesdayTrackEngine.h`
- `/src/apps/sequencer/engine/TuesdayTrackEngine.cpp`
- `/src/apps/sequencer/ui/page/TuesdayTrackPage.h`
- `/src/apps/sequencer/ui/page/TuesdayTrackPage.cpp`

## Expected Output
- Layer mapping definition
- Parameter control mapping
- Visualization approach
- Editing interaction patterns

## Status
Pending

## Priority
Medium