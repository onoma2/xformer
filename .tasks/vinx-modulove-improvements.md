# VinxScorza and Modulove Performer Improvements for XFORMER

## Overview

This document outlines the comprehensive set of improvements from the VinxScorza and Modulove performer forks that should be integrated into the XFORMER project. These improvements primarily focus on enhancing the Launchpad controller functionality and adding new features that XFORMER currently lacks.

## Reference Files

**VinxScorza Performer (temp-ref/vinx-performer/):**
- `/temp-ref/vinx-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Main controller header
- `/temp-ref/vinx-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Main controller implementation
- `/temp-ref/vinx-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadControllerGeneratorsMode.cpp` - Generators mode logic
- `/temp-ref/vinx-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadControllerTrackSceneRouting.cpp` - Track/scene routing and locking
- `/temp-ref/vinx-performer/src/apps/sequencer/ui/page/NoteSequenceEditPage.h` - LaunchpadGenerator enum
- `/temp-ref/vinx-performer/src/apps/sequencer/ui/page/GeneratorPage.h` - Generator page integration
- `/temp-ref/vinx-performer/src/apps/sequencer/model/UserSettings.h` - Style and note style settings

**Modulove Performer (temp-ref/modulove-performer/):**
- `/temp-ref/modulove-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Main controller header
- `/temp-ref/modulove-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Main controller implementation
- `/temp-ref/modulove-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadDevice.h` - Base device class
- `/temp-ref/modulove-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadMk2Device.cpp/h` - Mk2 device implementation
- `/temp-ref/modulove-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadMk3Device.cpp/h` - Mk3 device implementation
- `/temp-ref/modulove-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadProDevice.cpp/h` - Pro device implementation
- `/temp-ref/modulove-performer/src/apps/sequencer/ui/controllers/launchpad/LaunchpadProMk3Device.cpp/h` - Pro Mk3 device implementation

## VinxScorza Performer Improvements

### 1. Launchpad as a First-Class Layer

**Features to Implement:**
- `LP Style` machine setting (classic/blue color schemes)
- `LP Note Style` machine setting (classic/circuit note editor styles)
- Default to `Blue + Circuit` for modern look and workflow
- Preserve compatibility with classic modes

**Implementation Files:**
- `/src/apps/sequencer/model/UserSettings.h` - Add style settings
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Add style management
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Implement style switching

### 2. Generators Mode (Split Architecture)

**Features to Implement:**
- **Note Track Full Map**: Random, Acid Layer, Acid Phrase, Vandalize, Wreck, Euclidean, Init Layer, Init Steps
- **Non-Note Track Subset**: Random, Entropy, Euclidean, Init Layer, Init Steps (for Curve/Stochastic/Logic/Arp tracks)
- `LaunchpadGenerator` enum in NoteSequenceEditPage
- Generator mode integration with all edit pages
- Preview/Apply workflow with A/B preview, regenerate, cancel, apply

**Implementation Files:**
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadControllerGeneratorsMode.cpp` - New file for generators mode logic
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Add generators mode methods
- `/src/apps/sequencer/ui/page/NoteSequenceEditPage.h` - Add LaunchpadGenerator enum
- `/src/apps/sequencer/ui/page/GeneratorPage.h` - Add Launchpad-specific methods

### 3. Circuit Keyboard for Note Editing

**Features to Implement:**
- Circuit-style keyboard overlay for note editing (LP Note Style = Circuit)
- Keyboard on Note track note layer
- Keyboard on Stochastic track note probability layer
- Keyboard on Arp track note layer
- Classic mode fallback (LP Note Style = Classic)

**Implementation Files:**
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Implement circuit keyboard
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Add circuit keyboard methods
- `/src/apps/sequencer/model/UserSettings.h` - Add note style setting

### 4. 1-Level Undo/Redo

**Features to Implement:**
- Launchpad shortcut: TOP 7 + TOP 8 (Shift + Play) for undo/redo
- Mirror machine's PAGE + S7 undo functionality
- Works on Note/Curve/Logic step-edit pages and generators preview pages

**Implementation Files:**
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Implement undo/redo
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Add undo/redo methods

### 5. Track Selection Locking

**Features to Implement:**
- Modal locking: Prevents track/scene switching while in modal pages
- UI kind locking: Locks track selection when in generator UI pages
- Top page locking: Locks track selection when specific top pages are active

**Implementation Files:**
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadControllerTrackSceneRouting.cpp` - New file for routing and locking
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Add locking methods

### 6. Performer Mode Support

**Features to Implement:**
- Performer mode with scene mute/solo/fill
- Track selection in performer mode
- Layer selection for different views
- Follow mode functionality

**Implementation Files:**
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Implement performer mode
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Add performer mode methods

### 7. Launchpad Style and Color Customization

**Features to Implement:**
- LP Style: Classic (original colors) or Blue (new blue color scheme)
- LP Note Style: Classic (layer-based) or Circuit (keyboard-based)
- Persistent settings stored in flash when saved from System page

**Implementation Files:**
- `/src/apps/sequencer/model/UserSettings.h` - Define settings
- `/src/apps/sequencer/ui/page/SystemPage.h` - Add UI for settings
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Implement style switching

## Modulove Performer Improvements

### 1. Enhanced Button Event Handling

**Features to Implement:**
- Double-tap for gate toggling on all track types
- Long-press functionality for additional actions
- Improved button state tracking reliability

**Implementation Files:**
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Enhance button event handling
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Add button event methods

### 2. Visual Feedback Enhancements

**Features to Implement:**
- Octave line visualization for note editing
- Improved pattern visualization with dimmed colors for edited patterns
- Show requested patterns (latched/synced) with distinct colors

**Implementation Files:**
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Enhance visualization
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Add visualization methods

### 3. Interaction Improvements

**Features to Implement:**
- Fill functionality on Launchpad
- Range editing (first/last step selection)
- Run mode selection directly from Launchpad

**Implementation Files:**
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Implement interaction features
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Add interaction methods

### 4. Layer Selection Optimization

**Features to Implement:**
- Improve layer mapping visualization
- Add quick access to all available sequence layers

**Implementation Files:**
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Optimize layer selection
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Add layer selection methods

### 5. Track Status Visualization

**Features to Implement:**
- Show track mute status on scene buttons
- Indicate active tracks with clear LED feedback

**Implementation Files:**
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.cpp` - Enhance track status visualization
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h` - Add track status methods

## Implementation Plan

### Phase 1: Foundation (Weeks 1-2)
1. Add LP Style and LP Note Style settings
2. Implement circuit keyboard for note editing
3. Add track selection locking
4. Implement performer mode

### Phase 2: Generators Mode (Weeks 3-4)
1. Add LaunchpadGenerator enum and split architecture
2. Implement generators mode UI
3. Add preview/apply workflow
4. Integrate with all track types

### Phase 3: Enhancements (Weeks 5-6)
1. Add 1-level undo/redo
2. Implement double-tap and long-press functionality
3. Enhance visual feedback
4. Add fill, range editing, and run mode selection

### Phase 4: Optimization (Weeks 7-8)
1. Improve layer selection
2. Enhance track status visualization
3. Optimize performance
4. Test and bug fix

## Success Criteria

- All VinxScorza and Modulove improvements integrated
- Launchpad as a first-class layer with style settings
- Full generators mode with circuit keyboard
- Performer mode with scene control
- Enhanced button event handling
- Improved visual feedback and interaction
- Compatibility with all Launchpad models

## Files to Create/Modify

**New Files:**
- `LaunchpadControllerGeneratorsMode.cpp` - Generators mode logic
- `LaunchpadControllerTrackSceneRouting.cpp` - Track/scene routing and locking

**Modified Files:**
- `/src/apps/sequencer/model/UserSettings.h` - Add style settings
- `/src/apps/sequencer/ui/controllers/launchpad/LaunchpadController.h/cpp` - Enhance controller
- `/src/apps/sequencer/ui/page/NoteSequenceEditPage.h` - Add generator enum
- `/src/apps/sequencer/ui/page/GeneratorPage.h` - Add Launchpad methods
- `/src/apps/sequencer/ui/page/SystemPage.h` - Add style settings UI

This comprehensive implementation will transform the Launchpad from a simple step controller to a first-class interface with comprehensive generator access, note editing, and workflow improvements.