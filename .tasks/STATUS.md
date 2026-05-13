# Task Board
_Updated: 2026-05-13_

## 🟡 teletype-performer-ecosystem-redesign — Map full architecture from script input to CV/gate output
**Status:** paused
**Where I stopped:** 6-layer pipeline mapped (UI→Model→VM→Bridge→Engine→Output). Static/dynamic ownership documented. 2-stage I/O routing + 3-stage CV pipeline identified.
**Next action:** Design boundary decisions — what goes on TeletypeEditPage vs LayoutPage vs ScriptViewPage
**Branch:** TBD

---

## 🟡 teletype-edit-page — Build TeletypeEditPage as multi-mode config page
**Status:** paused
**Where I stopped:** Design finalized — 4-mode ListPage (INPUT/OUTPUT/CV/TIMING) with 4 new ListModels. LayoutPage stays untouched.
**Next action:** Create `TeletypeInputListModel.h`, first of 4 ListModel files
**Branch:** TBD

---

## 🔵 resource-optimization — RAM & Flash budget recovery
**Status:** ready — research complete
**Where I stopped:** Measured XFORMER RAM at 97% (127KB/128KB) vs Vinx baseline at 86% (113KB/128KB). Flash 94 KB larger. 14 KB RAM gap mostly from XFORMER-unique track types (Teletype, Tuesday, Discrete, Indexed). No headroom for features — blocks performer-improvements and all future feature work.
**Next action:** Identify and trim large .bss consumers from `sequencer.list`
**Branch:** TBD

## 🟢 performer-keyboard-shortcuts — USB keyboard: context menu, refactoring, teletype nav
**Status:** done — KeyboardManager refactor merged via `feat/global-keyboard-refactoring`
**Where I stopped:** Keyboard dispatch extracted into KeyboardManager (Phases 1-6, merged). QWERTY step mapping (Q-I=S0-S7, A-K=S8-S15), Tab context menu, key release in UsbH::processHidReports(). Teletype follow-on: Up/Down script line nav with Shift recall, A-Z text input for NAME fields.
**Branch:** feat/global-keyboard-refactoring (merged)

---

## 🟢 usb-hid-implementation — USB keyboard end-to-end, mouse rejected
**Status:** done — merged
**Where I stopped:** All hardware tested. Bug fixed: removed UsbH.cpp debug callback that overwrote "KEYBOARD CONNECTED" LCD message with "HID 0 t=3". Also removed DBG() prints from connect/disconnect handlers. Branch already fully merged.
**Branch:** feat/USB-keyboard4 (stale, can delete)

## 🟢 teletype-keyboard-shortcuts — Hardware shortcut keys implemented
**Status:** done
**Where I stopped:** All 6 shortcuts implemented. F1-F4 run scripts 0-3, F5 runs metro. Alt+F1-F4 jump to edit scripts 0-3, Alt+F5 jump to metro. Alt+/ toggles line comment. Alt+Left/Right jumps to first/last pattern.
**Branch:** feat/USB-keyboard4 (merged)

---

## 🟢 teletype-file-reliability — Fix real Teletype file saving/loading bugs
**Status:** done — all 3 phases complete, merged to master
**Where I stopped:** Phases 0-2 merged. Follow-on on master: script files show project name in slot picker (`8a829819`), USB keyboard text input for NAME fields (`2f4f38dc`).
**Governing spec:** `docs/superpowers/specs/2026-05-12-teletype-saving-reality-check.md`
**Branch:** fix/teletype-files (stale, can delete)

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

## 🟡 performer-improvements — Non-Launchpad improvements from VinxScorza, Modulove, and Mebitek
**Status:** blocked — blocked by resource-optimization
**Where I stopped:** Landed master: wrap list selection without settings (`10efe3c4`). MenuWrapSetting wired (Settings::Version=2, dynamic_cast gate, default=on). Tested firmware builds but hardware crashes on boot — RAM at 97% (127KB/128KB), no headroom.
**Next action:** Unblocked by resource-optimization
**Branch:** TBD

---

## 🔵 fractal-track-implementation — Smart Mutation Engine track type (FractalTrack)
**Status:** blocked
**Where I stopped:** Full design doc read. Architecture defined as new TrackMode following TuesdayTrack pattern.
**Next action:** Phase 1: model layer (`FractalSequence.h` + `FractalTrack.h`) + Track integration
**Branch:** TBD
