# Task Board
_Updated: 2026-05-11_

## 🔴 performer-keyboard-shortcuts — Full Performer UI keyboard shortcuts
**Status:** active
**Where I stopped:** Shift+Alt and Caps Lock USB keyboard context menu triggers DO NOT WORK on hardware. Root cause: USB HID path (KeyboardEvent via BasePage::keyboard()) only fires for USB keyboards connected to STM32 USB host — but the Alt keycode (0xE2) may not arrive correctly, or modifier-only keypresses generate no HID report at all. Hardware Shift+Page buttons work through a completely different path (Key::isContextMenu via button matrix). Need alternative approach.
**Next action:** Decide on working approach for USB keyboard context menu trigger — options: (1) dedicated single key like Tab/Menu, (2) track modifier state across KeyboardEvents in Ui and fire contextShow when both held, (3) remove the non-working code
**Branch:** feat/global-keyboard
**Files involved:** BasePage.cpp, Event.h, Key.h, Ui.cpp (handleKeyboard), KeyPressEventTracker.h

---

## 🟢 usb-hid-implementation — USB keyboard end-to-end, mouse rejected
**Status:** done
**Where I stopped:** All hardware tested. Bug fixed: removed UsbH.cpp debug callback that overwrote "KEYBOARD CONNECTED" LCD message with "HID 0 t=3". Also removed DBG() prints from connect/disconnect handlers.
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

## 🟡 launchpad-track-port — Extend Launchpad controller to all 5 track types + Vinx/Modulove enhancements
**Status:** paused
**Where I stopped:** Comprehensive research done across all forks (mebitek, vinx, modulove, cavian, extracadian). Track port plan corrected (MidiCv + Teletype removed). Vinx/Modulove LP improvements identified for merge.
**Next action:** Split LaunchpadController.cpp into per-track files, then begin Phase 1: NoteTrack fixes + Foundation (LP Style / LP Note Style settings)
**Branch:** TBD

---

## ⚪ indexed-sequence-macro-refactor — Extract macro logic into IndexedSequence model
**Status:** ready — not started
**Where I stopped:** Identified ~700 lines of macro logic in `IndexedSequenceEditPage.cpp:1605-2304` that must move to `IndexedSequence` model methods. 20 macros across rhythm, waveform, melodic, and duration categories. Blocks Launchpad Macro Grid v2 for Indexed.
**Next action:** Read macro code, catalog functions, design model API (individual methods vs. enum dispatch)
**Branch:** TBD

---

## 🟡 vinx-modulove-improvements — Integrate VinxScorza and Modulove performer improvements
**Status:** paused
**Where I stopped:** All Launchpad-specific improvements from this task have been merged into `launchpad-track-port`. Remaining non-Launchpad items (microtiming, LFO modulators, 16-track dual bank research, chaos generators) stay here or move to `other-forks-improvements`.
**Next action:** Continue non-Launchpad Vinx/Modulove work (microtiming, enhanced performer page) after LP track port completes
**Branch:** TBD

---

## 🟡 other-forks-improvements — Integrate non-Launchpad improvements from VinxScorza, Modulove, and Mebitek
**Status:** paused - Implementation Plan Updated with Feasibility Analysis
**Where I stopped:** Reorganized task against current XFORMER codebase with feasibility, design decisions, and priority
**Next action:** To be determined after Launchpad improvements
**Branch:** TBD

---

## ⚪ fractal-track-implementation — Smart Mutation Engine track type (FractalTrack)
**Status:** paused
**Where I stopped:** Full design doc read. Architecture defined as new TrackMode following TuesdayTrack pattern.
**Next action:** Phase 1: model layer (`FractalSequence.h` + `FractalTrack.h`) + Track integration
**Branch:** TBD