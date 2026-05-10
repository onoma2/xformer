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

## 🟡 performer-keyboard-shortcuts — Full Performer UI keyboard shortcuts
**Status:** paused
**Where I stopped:** Full analysis complete:
- **Architecture research**: All ~50 page types inventoried, event flow understood (top-to-bottom, clean slate — zero non-Teletype pages override `keyboard()`)
- **Workflow analysis**: Full 22-step hardware button sequence documented for typical Note track setup (Layout → Sequence → Step Edit → Routing → Play). Core friction = encoder scroll+click repetition on list pages + no batch step operations
- **Feasibility**: No non-Teletype page has a `keyboard()` override, meaning all pages are clean slate with zero conflicts. `ListModel::edit()` only supports relative (+/-1) edits, but calling model methods directly from page keyboard handlers is trivial
- **3 approaches proposed**: (A) Form mode — Tab through list fields, type values directly. Very high feasibility, ~20 lines per page. (C) Step type-in command line — `5N E4` syntax on NoteSequenceEditPage. High feasibility, modeled on Teletype's `_editBuffer`. (B) Command palette — `Ctrl+K` overlay search. Moderate feasibility, ~200 lines.
- **Recommendation**: Phase A (form mode) + Phase C (step type-in) first, then command palette later
**Next action:** Implement Phase 1a: Universal shortcuts (Space=play, Esc=back, 1-8=track select) + TopPage Alt+letter nav
**Branch:** feat/USB-keyboard4
**Files involved:** Page.h/cpp, PageManager.h/cpp, TopPage.cpp, all page .cpp files with F1-F5

---

## 🟡 teletype-file-reliability — Improve TeletypeTrack file loading/saving reliability
**Status:** paused
**Where I stopped:** Complete analysis of file loading/saving across track types, identified key risks and improvement opportunities for TeletypeTrack
**Next action:** Begin implementing fixes for serialization, error handling, and parsing robustness
**Branch:** TBD

---

## 🟡 launchpad-track-port — Extend Launchpad controller to all 5 track types + Vinx/Modulove enhancements
**Status:** active - Planning complete, feasibility reviewed
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
**Status:** active - Launchpad items merged into `launchpad-track-port`
**Where I stopped:** All Launchpad-specific improvements from this task have been merged into `launchpad-track-port`. Remaining non-Launchpad items (microtiming, LFO modulators, 16-track dual bank research, chaos generators) stay here or move to `other-forks-improvements`.
**Next action:** Continue non-Launchpad Vinx/Modulove work (microtiming, enhanced performer page) after LP track port completes
**Branch:** TBD

---

## 🟡 other-forks-improvements — Integrate non-Launchpad improvements from VinxScorza, Modulove, and Mebitek
**Status:** paused - Implementation Plan Updated with Feasibility Analysis
**Where I stopped:** Reorganized task against current XFORMER codebase with feasibility, design decisions, and priority:
- **Current XFORMER Analysis**: 7 track types (4 XFORMER-unique), 8 tracks, no LFOs, no microtiming, unique Harmony system
- **High Feasibility (weeks 1-4)**: Microtiming recording, enhanced performer page, quick octave change, steps to stop, menu wrap
- **Medium Feasibility (weeks 5-10)**: Generator preview/apply, curve undo, submenu shortcuts, backward playback
- **Low Feasibility (weeks 11-16)**: Chaos generators, random gen improvements, LFO modulators, 64-step visualization
- **Not Feasible Now**: 16-track dual bank (architecture too invasive), Arpeggiator track (no track type)
- **Already Present**: Curve gate offset/length, bypass voltage table
**Next action:** To be determined after Launchpad improvements
**Branch:** TBD

---

## ⚪ fractal-track-implementation — Smart Mutation Engine track type (FractalTrack)
**Status:** paused
**Where I stopped:** Full design doc read (`doc/fractal-track-research.md`, ~12K words). Architecture defined as new TrackMode following TuesdayTrack pattern. Parent-child inheritance from NoteTrack/Curve/Indexed/Tuesday/DiscreteMap via FractalSourceInterface. MutationHistory (16-record buffer) + SelectionPressure for learning. Extended rules (Markov, L-System, CA, Fibonacci, Perlin, echo, symmetry, tension). Bloom v2 features (ratchet, slew, trill, 8-slot branches).
**Next action:** Phase 1: model layer (`FractalSequence.h` + `FractalTrack.h`) + Track integration
**Branch:** TBD
