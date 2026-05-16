# Task Board
_Updated: 2026-05-17_

## ⚪ generator-preview-apply — Generator A/B preview, step selection, 64-step context
**Status:** ready — Phase A committed and hardware verified. Phases B-E remaining.
**Where I stopped:** Phase A complete. SequenceBuilder has 3-copy state machine (T* _preview heap-allocated, apply/showOriginal/showPreview). Generator has delegate methods. .data + .bss unchanged at 118,648 (90.5%).
**Next action:** Phase B — Add Variation param, randomizeParams/displayValue to RandomGenerator, widen seed to uint32_t.
**Depends on:** resource-optimization (RAM at 90.5%, ~2.3 KB headroom to 120 KB warning)
**Branch:** feat/generator

## 🟡 resource-optimization — RAM & Flash budget recovery
**Status:** paused — baseline recorded; safe wins exhausted; struct-packing only remaining.
**Where I stopped:** Current build is `.data=6,320`, `.bss=113,640`, `.ccmram_bss=54,096`; MonitorPage shows `Track=9560`, `NoteTrack=9544`, `CurveTrack=9480`, `Model=88072`.
**Next action:** Research Teletype file-load backup transaction semantics; USB/FS DirBuf already dead (FF_USE_LFN=0), remaining safe win ~100-300 B struct packing only; LCD packed Canvas (D-A) confirmed no-go; P13/LCD D-B remain future/last-resort research.
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
**Status:** paused — Phase 0-4 hardware-verified; two persistence contracts are now implemented.
**Where I stopped:** Phase 4 verified: active-clip-only text save/load, S1-S3 rollback transaction, inactive clip untouched, project save/load OK, `.data+bss` reduced by 1,136 B.
**Next action:** Post-Phase-4 research: decide whether further transaction-buffer reduction or `write()` redundancy cleanup is worth doing.
**Depends on:** resource-optimization (for RAM gates), teletype file transaction semantics
**Branch:** refactor/resouce-optimization

---

## 🟡 teletype-cv-source-combiner — Explicit raw/value-domain CV source pipeline for Teletype outputs
**Status:** paused — task and first implementation plan captured; no code started.
**Where I stopped:** Rejected Telex VCA semantics; first implementation uses explicit raw source arbitration with V0 priority law `ENV > GEODE > LFO > CV`.
**Next action:** If resumed, start Phase 1 in `.tasks/teletype-cv-source-combiner/plan.md`: add per-output source cache without behavior change.
**Depends on:** none; Teletype runtime Phase 4 is hardware-verified
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
**Status:** paused — Phase 1 wired and hardware-verified (6/8 items done); generator task extracted to separate task.
**Where I stopped:** Phase 1 items completed: (1) Quick octave change Step+F1-F5, (2) Double-click Page context menus with 2s auto-close, (3) Short clock pulse 1ms floor, (4-6) PerformerPage mute LEDs + pattern labels + MenuWrap. Generator preview/apply extracted to `generator-preview-apply` task.
**Next action:** Remaining Phase 1: Curve undo restoration. Then generator-preview-apply Phase A.
**Depends on:** resource-optimization (RAM headroom available)
**Branch:** TBD

---

## 🟢 global-modulators-v1 — Port Modulove-style global modulators with chaos shapes
**Status:** complete — Phases 1-9 done. Core shapes, output routing, RoutingEngine/CvRoute integration, defaults, rate consistency, documentation. Hardware verified.
**Where I stopped:** Phase 9 (CvRoute modulator inputs) committed. manuals/Modulators.md written. Feature ready for merge to master.
**Next action:** Merge `feat/modulators` to master when user confirms.
**Depends on:** none (RAM gate passed)
**Branch:** feat/modulators

---

## 🟡 route-reordering — Rearranging Routing::Target enum into signal-flow ordering
**Status:** paused — Spec complete; no code started.
**Where I stopped:** Spec written in `reorder-spec.md`. Target ordering by signal-flow agreed. `targetSerialize()` already decouples serialization from enum values.
**Next action:** Update `Routing.h` enum, sentinel values, `isXxxTarget()` checks; then `Routing.cpp` `targetInfos[]` and `targetName()` order.
**Depends on:** nothing (pure refactor, no behavior change)
**Branch:** feat/modulators

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
