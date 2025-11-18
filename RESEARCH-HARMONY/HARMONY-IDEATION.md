# HARMONY-IDEATION.md - Westlicht Performer Harmony Feature Specification

## Current Status: Requirements Defined - Planning Phase

**Date**: November 19, 2025  
**Feature**: Harmonic Sequencing (Harmonaig-style)  
**Status**: ✅ Requirements Collected - Ready for Implementation Planning  
**Priority**: High - Core Feature Enhancement

## Overview

This document captures the requirements for implementing harmonic sequencing features in the Westlicht Performer, based on Instruo Harmonaig functionality but simplified for initial implementation.

## Core Architecture

### HarmonyRoot Tracks
- **Limited Positions**: Only Track 1 and Track 5 can be designated as HarmonyRoot tracks
- **Scale Override**: HarmonyRoot tracks use their own scale setting which overrides the project scale for the entire harmony system
- **Function**: Provide root notes for harmonic generation

### Follower Tracks
- **Positional Limitation**: Only tracks immediately below HarmonyRoots can follow:
  - Track 1 (HarmonyRoot) → Tracks 2, 3, 4 as followers (Harm3, Harm5, Harm7)
  - Track 5 (HarmonyRoot) → Tracks 6, 7, 8 as followers (Harm3, Harm5, Harm7)
- **Function**: Output 3rd, 5th, or 7th harmonized notes based on HarmonyRoot

## Feature Requirements

### 1. Global Harmony Page (HARM)
- Accessible from TopPage navigation
- Contains all global harmony parameters:
  - Scale selection (all 14 modes: 7 Ionian + 7 Harmonic Minor)
  - Chord quality selection (diatonic auto-mode)
  - Unison mode toggle
  - Global voicing options
  - Global inversion options

### 2. Track Configuration
- Option to designate Track 1 and 5 as HarmonyRoot
- Follower assignment options for tracks 2-4 and 6-8
- Each follower track can be set to output: Harm3, Harm5, or Harm7

### 3. Step-Level Controls (F3 NOTE Button Layers)
- **Voicing Layer**: Accessible via F3 NOTE button cycling, allows per-step voicing control (Close/Open for initial implementation)
- **Inversion Layer**: Accessible via F3 NOTE button cycling, allows per-step inversion control (Root/1st for initial implementation)
- Integration with existing layer cycling: Note → NoteVariationRange → NoteVariationProbability → Voicing → Inversion → Note

### 4. Accumulator Integration
- Accumulator should influence HarmonyRoot steps
- Harmonic changes propagate to followers accordingly

## Simplified Implementation Scope

### Initial Feature Set
- **Voicing**: Close and Open only (for UI testing)
- **Inversion**: Root and 1st only (for UI testing)
- **Chord Quality**: Diatonic auto-mode only (based on scale degree)
- **Unison Mode**: Toggle available on HARM page

### Full Scale Support
- Implement all 14 modes (7 Ionian + 7 Harmonic Minor)
- Leave option for future scale expansion
- Maintain complete lookup tables for all modes

## User Interface Flow

### Navigation
```
TopPage → Sequence Key → HARM page (global settings)
TopPage → Track Key → Track config (HarmonyRoot/follower assignment)
NoteStep UI → F3 NOTE button cycling → Voicing/Inversion layers
```

### Visual Feedback
- Clear indication of HarmonyRoot tracks in UI
- Visual connection indicators between roots and followers
- Chord symbol display (e.g., "Cmaj7")

## Technical Considerations

### Data Model
- Extend NoteSequence with harmony-specific properties
- Add harmony role properties (HarmonyRoot, Harm3, Harm5, Harm7)
- Store per-step voicing and inversion data

### Engine Integration
- Modify NoteTrackEngine to handle harmony logic
- Master/follower relationship management
- Real-time harmonic calculation

## Next Steps

1. [ ] Create HarmonyEngine class with lookup tables
2. [ ] Extend NoteSequence model with harmony properties
3. [ ] Modify NoteTrackEngine for harmony processing
4. [ ] Create HARM global page
5. [ ] Create track configuration UI
6. [ ] Add per-step harmony controls (Voicing/Inversion layers)
7. [ ] Test accumulator integration
8. [ ] UI testing and refinement

## Dependencies

- Existing Performer architecture (NoteTrack, NoteSequence, etc.)
- Accumulator feature (already implemented)
- Existing CV routing system
- Project serialization system

## Constraints

- Only Track 1 and 5 can be HarmonyRoots
- Only 3 followers per HarmonyRoot (immediately following tracks)
- HarmonyRoot scale overrides project scale
- Voicing and Inversion as F3 NOTE button layers
- UI-first approach, CV control secondary

---

**Document Maintainer**: Onoma2  
**Last Updated**: November 19, 2025