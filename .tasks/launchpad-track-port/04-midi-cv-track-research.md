# Launchpad Support - MidiCvTrack Interface Research

## Task Summary
Research MidiCvTrack interface and implementation peculiarities to inform Launchpad support.

## Track Overview
MidiCvTrack provides direct MIDI to CV conversion with voice management.

## Key Properties
- **Voice Management**: Multiple voices with voice config
- **MIDI Input**: Note input, pitch bend, modulation
- **Arpeggiator**: Built-in arpeggiator with patterns
- **Output**: CV and gate outputs for each voice

## Research Areas

### 1. Data Structure Analysis
- Examine MidiCvTrack class definition
- Understand voice configuration and management
- Identify how MIDI input is processed
- Look for existing visualization methods

### 2. Layer Identification
- Determine what layers should be available for editing
- Voice configuration
- Note priority
- Pitch bend range
- Arpeggiator settings
- MIDI channel and port selection

### 3. Value Ranges
- Determine valid ranges for each parameter
- Voice count (1-8 voices)
- Note priority (last, lowest, highest)
- Pitch bend range (0-24 semitones)
- Arpeggiator patterns (various styles)

### 4. Visualization Requirements
- MIDI note grid
- Active note indicators
- Voice status display
- Arpeggiator pattern visualization

### 5. Editing Patterns
- Voice configuration (add/remove voices)
- MIDI channel/port selection
- Arpeggiator settings
- Note priority adjustment

## Files to Examine
- `/src/apps/sequencer/model/MidiCvTrack.h`
- `/src/apps/sequencer/model/MidiCvTrack.cpp`
- `/src/apps/sequencer/engine/MidiCvTrackEngine.h`
- `/src/apps/sequencer/engine/MidiCvTrackEngine.cpp`
- `/src/apps/sequencer/ui/page/MidiCvTrackPage.h`
- `/src/apps/sequencer/ui/page/MidiCvTrackPage.cpp`

## Expected Output
- Layer mapping definition
- Voice management interface
- Visualization approach
- Editing interaction patterns

## Status
Pending

## Priority
Medium