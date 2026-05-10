# Task Board
_Updated: 2026-05-10_

## 🟢 usb-hid-implementation — USB keyboard end-to-end, mouse rejected
**Status:** done
**Where I stopped:** All hardware tested — USB keyboard works on both TeletypeScriptView and TeletypePatternView pages. Mice rejected at driver level. Launchpad still works. Engine integration, human-readable messages, uppercase conversion, shortcut mapping all complete.
**Next action:** Squash branch for merge
**Branch:** feat/USB-keyboard4

## 🟢 teletype-keyboard-shortcuts — Hardware shortcut keys implemented
**Status:** done
**Where I stopped:** All 6 shortcuts implemented and build passes. F1-F4 run scripts 0-3, F5 runs metro. Alt+F1-F4 jump to edit scripts 0-3, Alt+F5 jump to metro. Alt+/ toggles line comment. Alt+Left/Right jumps to first/last pattern.
**Next action:** Test on hardware
**Branch:** feat/USB-keyboard4

---

## 🟡 teletype-file-reliability — Improve TeletypeTrack file loading/saving reliability
**Status:** paused
**Where I stopped:** Complete analysis of file loading/saving across track types, identified key risks and improvement opportunities for TeletypeTrack
**Next action:** Begin implementing fixes for serialization, error handling, and parsing robustness
**Branch:** TBD

---

## 🟡 launchpad-track-port — Extend Launchpad controller to all 6 track types
**Status:** active - Phase 1: Track Interface Research (Complete)
**Where I stopped:** Completed comprehensive research of other performer forks
**Next action:** Begin Phase 2: NoteTrack & CurveTrack Improvements with layer map updates
**Branch:** TBD

---

## 🟡 vinx-modulove-improvements — Integrate VinxScorza and Modulove performer improvements
**Status:** active - Implementation Plan Complete
**Where I stopped:** Created comprehensive implementation plan for VinxScorza and Modulove performer improvements
- **Launchpad as First-Class Layer**: LP Style and LP Note Style settings with blue and circuit note editor styles
- **Generators Mode**: Split architecture for note/non-note track generators with preview/apply workflow
- **Circuit Keyboard**: Innovative note editing interface for intuitive operation
- **1-Level Undo/Redo**: Launchpad shortcut for undo/redo functionality
- **Track Selection Locking**: Modal, UI kind, and top page locking for modal contexts
- **Performer Mode**: Scene mute/solo/fill with track selection and follow mode
- **Enhanced Button Event Handling**: Double-tap, long-press, and improved state tracking
- **Visual Feedback Enhancements**: Octave lines, pattern visualization, requested patterns display
- **Interaction Improvements**: Fill functionality, range editing, run mode selection
- **Layer Selection Optimization**: Improved layer mapping visualization and quick access
- **Track Status Visualization**: Track mute status and active track indicators
**Next action:** Begin Phase 1: Foundation with style settings and circuit keyboard
**Branch:** TBD

---

## 🟡 other-forks-improvements — Integrate non-Launchpad improvements from VinxScorza, Modulove, and Mebitek
**Status:** paused - Implementation Plan Complete
**Where I stopped:** Created comprehensive implementation plan for all non-Launchpad improvements from other performer forks
- **Core Sequencing**: Step engine stabilization, crash path fixes, chaos generators evolution
- **16-Track Support**: Dual bank system with 2 banks of 8 tracks, extended MIDI routing
- **8 LFO Modulators**: Multiple waveform shapes, waveform preview, quick-map popup, musical divisions
- **Microtiming Recording**: 7-bit resolution (-63 to +63), bidirectional timing, timing quantize
- **UI/UX Enhancements**: 64-step context visualization, menu wrap, screensaver/wake refinement
- **Technical Improvements**: Toolchain update, memory optimization, GitHub Actions CI
- **Generators**: Preview/apply workflow, generator families reorganization, uniform layouts
**Next action:** To be determined after Launchpad improvements
**Branch:** TBD
