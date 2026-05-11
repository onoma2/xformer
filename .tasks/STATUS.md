# Task Board
_Updated: 2026-05-11_

## 🔴 performer-keyboard-shortcuts — USB keyboard context menu
**Status:** active
**Where I stopped:** Debug build deployed — showMessage("KB:%d", 2000) added to BasePage::keyboard() to verify if KeyboardEvents reach it at all. Three previous approaches tried and failed on hardware: (1) Shift+Alt via shift()&&alt() — USB HID driver only enqueues keycodes, not modifier-only events; (2) CapsLock keycode — keyboards may not report it in boot protocol; (3) Tab/Alt+M via KeyboardEvent path — keycodes reach pages but contextShow() didn't fire; (4) Tab/Alt+M via hardware Key injection — immediate KeyUp closes the menu. TopPage::keyboard() now chains to BasePage::keyboard(). All need hardware testing.
**Next action:** Flash debug build, press any USB key on NoteSequence page, check if "KB:" message appears
**Branch:** feat/global-keyboard
**Files involved:** TopPage.cpp, BasePage.cpp, Event.h, TeletypeScriptViewPage.cpp, TeletypePatternViewPage.cpp

---

## Approach history
- **A1: Shift+Alt via `event.shift() && event.alt()`** — Broken: USB HID driver only enqueues events for new keycodes, not modifier-only changes. CapsLock keycode (0x39) also unreliable across keyboards.
- **A2: CapsLock keycode** — Same root cause as A1.
- **A3: F12 keycode** — Not tested, superseded by A5.
- **A4: Tab & Alt+M via KeyboardEvent → contextShow()** — TopPage::keyboard() wasn't chaining to BasePage. Fixed. But Tab (0x2B) and Alt+letter combos still don't open menu on hardware despite F1-F5 working. Root cause unknown.
- **A5: Tab & Alt+M via KeyboardEvent → hardware Key event injection** — Immediate KeyUp closes the menu. Wrong approach.
- **A6: Tab & Alt+M via KeyboardEvent → contextShow()** — Back to direct call. Same as A4, still untested with TopPage chain fix.
- **A7: Debug build with showMessage("KB:%d")** — Current. Awaiting hardware test.

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