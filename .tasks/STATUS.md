# Task Board
_Updated: 2026-05-11_

## 🟡 performer-keyboard-shortcuts — USB keyboard context menu
**Status:** paused
**Where I stopped:** Plan B v3 WORKS on hardware. QWERTY step mapping (Q-I = S0-S7, A-K = S8-S15) confirmed working with proper press/release. Tab context menu confirmed. Key release detection lives in UsbH::processHidReports() (C++), not the fragile C driver. Keyboard detection, Launchpad, MIDI all stable.
**Next action:** Verify across all track types. Then consider adding remaining keyboard shortcuts (shift modifier, encoder emulation, etc.)
**Branch:** feat/global-keyboard

---

## Approach history
- **A8 (SUCCESS):** Tab context menu via hardware Key injection.
- **A9 (FAILED):** HID driver press/release tracking crashed USB path — reverted.
- **A10 (SUCCESS):** Key release diffing moved to UsbH::processHidReports() in C++ layer. Stable. QWERTY step mapping + Tab context menu both work.

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

## 🔴 teletype-file-reliability — Improve TeletypeTrack file loading/saving reliability
**Status:** active
**Where I stopped:** Identified critical 32-byte stack buffer overflow in FileManager `writeScriptSection`. The user requested to streamline the track binary format (drop backward compatibility) and expand memory bounds.
**Next action:** Apply the agreed plan: expand the write buffer in `FileManager` to 128 bytes and streamline `TeletypeTrack::read/write` to drop legacy I/O mappings.
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

## 🟡 other-forks-improvements — Integrate non-Launchpad improvements from VinxScorza, Modulove, and Mebitek
**Status:** paused
**Where I stopped:** Merged with the vinx-modulove-improvements task. Reorganized task against current XFORMER codebase with feasibility, design decisions, and priority. The implementation plan spans 4 phases (Microtiming, Generator workflows, Advanced Track features).
**Next action:** To be determined after Launchpad improvements
**Branch:** TBD

---

## ⚪ fractal-track-implementation — Smart Mutation Engine track type (FractalTrack)
**Status:** paused
**Where I stopped:** Full design doc read. Architecture defined as new TrackMode following TuesdayTrack pattern.
**Next action:** Phase 1: model layer (`FractalSequence.h` + `FractalTrack.h`) + Track integration
**Branch:** TBD
