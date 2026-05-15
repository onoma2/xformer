# Task Board
_Updated: 2026-05-15_

## 🔴 resource-optimization — RAM & Flash budget recovery
**Status:** active — P1/P5/P15 hardware/build-verified; model storage is solved for now.
**Where I stopped:** Current build is `.data=6,320`, `.bss=113,640`, `.ccmram_bss=54,096`; MonitorPage shows `Track=9560`, `NoteTrack=9544`, `CurveTrack=9480`, `Model=88072`.
**Next action:** Research Teletype file-load backup transaction semantics; USB/FS audit found only ~700-1,000 B safe SRAM, and P13/LCD D-B remain future/last-resort research.
**Depends on:** nothing
**Branch:** refactor/resouce-optimization

---

## 🟡 sim-project-io — Desktop editor foundation: simulator load/save HW projects via FileManager
**Status:** paused
**Where I stopped:** Architecture mapped. Two I/O paths identified (direct std::fstream vs FileManager→FatFs→sdcard.iso). Full plan written at `.tasks/sim-project-io/PLAN.md`.
**Next action:** Step 1 — generate `sdcard.iso` FAT disk image for the virtual SD card
**Depends on:** nothing
**Branch:** TBD

---

## 🟡 core-architecture-optimization — Research XFORMER core mechanics after expansion beyond original Note/Curve Performer
**Status:** paused/reference — research and decision artifacts only, not an implementation queue.
**Where I stopped:** Source probes and decision tables now say RoutingEngine compaction is the only current architecture-adjacent RAM implementation; model/container pool replacement is no-go unless product semantics change.
**Next action:** Keep docs current while implementing `resource-optimization`; use `model-pool-decision-table.md` and `routingengine-refactor-phased-plan.md` as decision references.
**Depends on:** resource-optimization (for baseline measurements)
**Branch:** refactor/resouce-optimization

---

## 🟡 teletype-runtime-architecture — Clarify Teletype VM/slot/runtime ownership before global runtime work
**Status:** active — Phase 0-3 hardware-verified and committed. Phase 4 code-complete, awaiting hardware verification.
**Where I stopped:** Phase 4 code-complete: active-clip-only text save/load, S1-S3 rollback transaction, `.data+bss` reduced by 1,136 B. Text format changed: `#S4`/`#M` (no slot sub-headers), no `SLOT` markers.
**Next action:** Phase 4 hardware verification: text save/load roundtrip, inactive clip untouched, project save/load, IO config, error rollback.
**Depends on:** resource-optimization (for RAM gates), teletype file transaction semantics
**Branch:** refactor/resouce-optimization

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

---

## 🟡 orca-mvp — Port Orca esolang to Performer as new track type
**Status:** paused
**Where I stopped:** Research phase complete. All reference codebases cloned to `temp-ref/` (C, Lua, Arduino C++). RAM container gates identified: `NoteTrack=9544 B`, `TeletypeTrackEngine=912 B`. Next step is designing fixed-grid `OrcaTrack` model that fits under the container gate.
**Next action:** Prototype `OrcaTrack` model with fixed grid size (e.g., 16×16 or 32×16) and measure `sizeof(OrcaTrack)` in STM32 release build to verify it stays ≤ `NoteTrack`.
**Depends on:** resource-optimization (needs RAM headroom), teletype-runtime-architecture (pattern slot pattern to follow)
**Branch:** TBD
