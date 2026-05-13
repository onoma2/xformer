# Task Board
_Updated: 2026-05-14_

## 🔴 resource-optimization — RAM & Flash budget recovery (includes teletype-performer-ecosystem-redesign analysis)
**Status:** active — Phase 1 complete. Phase 2 prep.
**Where I stopped:** Phase 1 implemented (P2+P4+P14+P14b). Measured .data reduction 9,020→6,316 = 2,704 B saved; P2/P4 are internal Teletype cleanup with no current .bss effect because Track container remains NoteTrack-sized.
**Next action:** Plan Phase 2 around P5 (RoutingEngine TrackState union) and P6 (non-per-track routes); keep P7 as future research because it only saves RAM if live Teletype engine count is capped.
**Depends on:** nothing
**Branch:** feat/resource-optimization

---

## 🟡 core-architecture-optimization — Research XFORMER core mechanics after expansion beyond original Note/Curve Performer
**Status:** research complete — restructured into two docs per user direction.
**Where I stopped:** Restructured into `architecture-research-directions.md` (broad architecture: 7 mismatches, ranked directions, keep/change/research-later) and `ram-recovery-experiments.md` (narrow RAM experiments: 5 experiments with baselines, exit criteria, rollback). Key architectural finding: RoutingEngine evolved from sidecar to modulation engine (7,488 B unconditional state, no lifecycle). Key structural finding: model Container Note/Curve-driven (~9.5 KB each), engine Container Teletype-driven (~904 B). Next: run Experiment C/D (measurement-only) then Experiment A (RoutingEngine conditional state).
**Depends on:** resource-optimization (for baseline measurements)
**Branch:** TBD

---

## 🟡 teletype-edit-page — Build TeletypeEditPage as multi-mode config page
**Status:** paused
**Where I stopped:** Design finalized — 4-mode ListPage (INPUT/OUTPUT/CV/TIMING) with 4 new ListModels. LayoutPage stays untouched.
**Next action:** Create `TeletypeInputListModel.h`, first of 4 ListModel files
**Depends on:** resource-optimization (needs RAM headroom)
**Branch:** TBD

---

## 🟡 launchpad-track-port — Extend Launchpad controller to all 5 grid-editable track types + Vinx/Modulove enhancements
**Status:** paused
**Where I stopped:** Comprehensive research done across all forks. Track port plan corrected (MidiCv + Teletype removed). Vinx/Modulove LP improvements identified.
**Next action:** Split LaunchpadController.cpp into per-track files, then begin Phase 1: NoteTrack fixes + Foundation (LP Style / LP Note Style settings)
**Depends on:** indexed-sequence-macro-refactor (Phase 4: Macro Grid v2 for Indexed)
**Branch:** TBD

---

## ⚪ indexed-sequence-macro-refactor — Extract macro logic into IndexedSequence model
**Status:** ready — not started
**Where I stopped:** Identified ~700 lines of macro logic in `IndexedSequenceEditPage.cpp:1605-2304` that must move to `IndexedSequence` model methods. 20 macros across rhythm, waveform, melodic, and duration categories.
**Next action:** Read macro code, catalog functions, design model API (individual methods vs. enum dispatch)
**Depends on:** nothing
**Blocks:** launchpad-track-port Phase 4 (Macro Grid v2 for Indexed)
**Branch:** TBD

---

## 🟡 performer-improvements — Non-Launchpad improvements from VinxScorza, Modulove, and Mebitek
**Status:** blocked
**Where I stopped:** MenuWrapSetting wired (Settings::Version=2, dynamic_cast gate, default=on). Hardware crashes on boot — RAM at 97%, no headroom.
**Next action:** Unblocked by resource-optimization
**Depends on:** resource-optimization (needs RAM headroom)
**Branch:** TBD

---

## 🔵 fractal-track-implementation — Smart Mutation Engine track type (FractalTrack)
**Status:** blocked
**Where I stopped:** Full design doc read. Architecture defined as new TrackMode following TuesdayTrack pattern.
**Next action:** Phase 1: model layer (`FractalSequence.h` + `FractalTrack.h`) + Track integration
**Depends on:** resource-optimization (needs RAM headroom)
**Branch:** TBD

---

## 🟢 usb-keyboard-system — Full USB keyboard pipeline (driver, manager, global shortcuts, Teletype shortcuts)
**Status:** done — all 4 phases merged to master
**Where I stopped:** HID driver fixed (dedup, ring buffer, mouse rejected). KeyboardManager extracted. Global shortcuts (Tab context menu, arrow keys, Alt+letter). Teletype shortcuts (F1-F5, Alt+F1-F5, Alt+/, Alt+arrows). Branches: feat/USB-keyboard4, feat/global-keyboard-refactoring (merged).
**Components:** usb-hid-implementation, usb-keyboard-manager-refactor, performer-keyboard-shortcuts, teletype-keyboard-shortcuts

---

## 🟢 teletype-file-reliability — Fix real Teletype file saving/loading bugs
**Status:** done — all 3 phases complete, merged to master
**Where I stopped:** Phases 0-2 merged. Follow-on on master: script files show project name in slot picker (`8a829819`), USB keyboard text input for NAME fields (`2f4f38dc`).
**Branch:** fix/teletype-files (stale, can delete)
