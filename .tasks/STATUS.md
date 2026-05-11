# Task Board
_Updated: 2026-05-11_

## 🔴 performer-keyboard-shortcuts — USB keyboard context menu
**Status:** active
**Where I stopped:** 8 approaches tried, ALL failed on hardware. Tab keycode 43 confirmed reaching BasePage::keyboard(). Latest A8: hardware Key injection without KeyUp — debug messages gone, no menu. Fundamental issue: KeyboardEvent path is a separate dispatch from hardware KeyEvent path, and contextShow() or Key injection from within keyboard() do not produce visible results. Need fundamentally different approach — possibly route KeyboardEvent through PageManager::dispatchEvent as a KeyEvent.
**Next action:** Investigate converting KeyboardEvent to KeyPressEvent and dispatching via PageManager instead of calling page handlers directly
**Branch:** feat/global-keyboard

---

## Approach history
- **A1: Shift+Alt via `event.shift() && event.alt()`** — Broken: USB HID driver enqueues keycodes only, not modifier-only events.
- **A2: CapsLock keycode (0x39)** — Same root cause as A1. Keyboards may not report it in boot protocol.
- **A3: F12 keycode** — Not tested, superseded.
- **A4: Tab & Alt+M via KeyboardEvent → contextShow()** — TopPage::keyboard() wasn't chaining. Fixed. Still no menu.
- **A5: Tab & Alt+M via KeyboardEvent → hardware Key injection (with KeyUp)** — Immediate KeyUp closes the menu.
- **A6: Tab & Alt+M via KeyboardEvent → contextShow() (retry with TopPage chain)** — No menu.
- **A7: Debug build showMessage("KB:")** — Confirmed Tab keycode 43 reaches BasePage::keyboard() on NoteSequencePage. contextShow() called but no menu.
- **A8: Tab via KeyboardEvent → hardware Key injection (no KeyUp)** — Debug messages gone, no menu. Broken.

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