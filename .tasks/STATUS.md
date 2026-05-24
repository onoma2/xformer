# Task Board
_Updated: 2026-05-24 (Stochastic: pitch generator rewritten as 5-step sequential law — class roll w/ recency, class repeat check w/ Complexity, Range-built candidate set bipolar 0..100, Bias/Spread beta-distribution, Contour direction nudge; ticket 0-flip fixed so 0=default-flat with stable meaning; Steps→MaskM/TiltM pitch-mask trigger-time filter, LoopM-only; MaskR/TiltR rename + TiltR knob made unipolar 0..100 center 50 with low-pass=longs survive; save/load EOF regression fixed; PITCH-LAW-FINAL.md captures final design; 15 generator test cases including anti-collapse contract.)_

**Naming convention from 2026-05-24 forward:** older entries below use prior names that are consistent with the code at the time they were written. Current canonical: step (idea, int), `StochasticStepContent` (stored), `RuntimeStep` (runtime), `StepCache` (container), `Gate` / `Cv` (output).

## 🟢 generator-preview-apply — Generator A/B preview, step selection, 64-step context, Tuesday AlgoGenerator
**Status:** done — Phases A-F complete and hardware verified.
**Where I stopped:** All phases committed. SequenceBuilder 3-copy state machine, RandomGenerator enhancements, GeneratorPage A/B workflow, 64-step bank visualization, context menu expansion, and Tuesday AlgoGenerator (15 algorithms via TuesdayAlgoCore). RAM: `.data + .bss = 118,884` (92.9%).
**Next action:** Merge `feat/generator` to master when user confirms.
**Depends on:** nothing
**Branch:** feat/generator

---

## 🟡 stochastic-track-port — Pitch law rewrite + mask/tilt restructure landed
**Status:** Pitch generator rewritten as the five-step sequential law in `PITCH-LAW-FINAL.md` — class roll (tickets × recency penalty), class repeat check (Complexity), Range-built candidate set (bipolar 0..100 centered at 50), Bias/Spread beta-distribution region roll, Contour direction nudge. Each knob owns one step; no multiplicative tangle. Tickets stay blind to motion. Three explicit ticket states honored: `-1` excludes, `0` is default-flat (weight 1), `1..100` weighted emphasis. Anti-collapse promise verified by test: heavy ticket=100 with all others=0 still produces varied output. Steps Sieve removed from generator entirely — pitch-centrality moved to trigger-time MaskM/TiltM filter (LoopM-only, knobs inert in LiveM). MaskR/TiltR renamed from Mask/Tilt; TiltR knob made unipolar 0..100 centered at 50 (half the encoder travel); low-pass = long notes survive, high-pass = short notes survive. MaskM/TiltM exposed at LOOP page top row S6/S7 (inverse-colored chips), kept off the LIVE page entirely. Range field semantics changed from 1..4 octaves to 0..100 bipolar knob, default 50. Save/load EOF regression fixed (_tiltMelody write was missing). Cache cells remain slot-keyed; anchor melody keyed per slot by melodySeed; cycle-end hooks bundled and lockstep across natural wrap + preempted reset.
**Where I stopped:** All 7 stochastic + queue tests green on sim release; STM32 release builds clean. 15 cases in the generator test cover class distribution, recency cap, Complexity repeat-suppression, Range bipolar bands, Bias/Spread edge favoring, Contour direction nudge both axes, heavy-ticket anti-collapse, and ticket-state contract. PITCH-LAW-FINAL.md captures the design; knob inventory HTML updated for MaskM/TiltM/MaskR/TiltR.
**Next action:** Hardware-flash and verify audibly. Then optional follow-ups (parking lot): default `_marblesSpread` 50 → ~70..80 for transparent uniform-at-default; PITCH-LAW-FINAL.md doc sync (Range>50 anchored at oct 0, not "above currentOctave"); Range<50 up-jump clamp; soft-cap ticket curve; Complexity-to-reject curve shape; beta-shape calibration.
**Branch:** feat/stochastic-seed-log
**Reference:** `.tasks/stochastic-track-port/PITCH-LAW-FINAL.md` (final pitch-law design — 5-step sequential decisions, knob ownership, anti-collapse safeguards, defaults). `.tasks/stochastic-track-port/PHASE17-SEED-DELTA.md` (structural cleanup parking lot). `.tasks/stochastic-track-port/PHASE15-PITCH-MATH-REVIEW.md` (original pitch review, mostly superseded by PITCH-LAW-FINAL.md). `.tasks/stochastic-track-port/PHASE16-FLAT-CELL.md` (flat-cell rhythm rewrite, separate track). `.scratch/stochastic-knob-ownership.html` (current knob inventory).

## ⚪ stability-fixes — Narrow crash/corruption fixes from adversarial reviews
**Status:** ready — Modulator/Generator fixes captured; Scope/CV Router/Bus recommendations added.
**Where I stopped:** Audits found four initial stability fixes plus CV Router physical-output staleness, bus last-writer ambiguity, bus shaper stale-state risk, and CvRoute getter hardening.
**Next action:** Fix CV Router output ordering first, then implement the Modulator/Generator crash/corruption fixes and add bus writer observability/priority.
**Depends on:** nothing
**Branch:** TBD
**Reference:** `.tasks/stability-fixes.md`

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

## 🟢 usb-keyboard-function-keys-extraction — Extract duplicated F1-F5 pressFunctionButton dispatch from 17 page keyboard() overrides into BasePage helper
**Status:** done — all phases complete. 18 files refactored (original 17 + GeneratorPage). Build clean.
**Where I stopped:** `BasePage::handleFunctionKeys()` added. Pattern A (11 files), B (5 files), C (2 files) refactored. STM32 release build verified: `.data=6,332`, `.bss=112,552`, `.ccmram_bss=54,804` (net SRAM -1,076 B vs baseline — within linker noise for pure code move).
**Next action:** Merge to master when user confirms.
**Depends on:** nothing (structural refactor, no RAM or behavior change)
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

## 🟡 usb-mouse-to-route — USB mouse axes/buttons as Routing::Source CV inputs
**Status:** paused — design captured; no code started. Gate identified before any feature work. Worktree at `.worktrees/usb-mouse-to-route/`.
**Where I stopped:** Walked the USB-keyboard git history (`6b0e407e` driver lift → `41ade375`/`80dd6d9c`/`b32cf6cb` failed-then-reverted in-driver OLED diagnostics → `82e64080` Engine pump → KeyboardManager extraction → `0d2b0015` mouse rejection). Established the OLED diagnostic rule: nothing inside `usbh_driver_hid.c` hooks or `hid_in_message_handler` may take a FreeRTOS mutex; only `UsbH::process()` post-poll may touch OLED. Mapped source-enum / RoutingEngine plug-in points.
**Next action:** Step 1 in TASK.md — reproduce the mouse-rejection crash on hardware with the rejection at `usbh_driver_hid.c:218-224` removed, observe whether keyboard/Launchpad survive, to establish the failure boundary before designing a fix.
**Depends on:** none (gated on libusbhost mouse-enumeration root cause investigation, which is step 1 of this task itself)
**Branch:** feat/usb-mouse-to-route (from master @ 11b5dae0)
**Reference:** `.tasks/usb-mouse-to-route/TASK.md`

---

## 🔵 fractal-track-implementation — Parent-material command/rule track (FractalTrack)
**Status:** blocked — design re-anchored 2026-05-22 to parent material + volatile trunk + model-owned command rules; awaits stochastic completion and RAM budget.
**Where I stopped:** Current best contract is now parent motif → section-sampled engine trunk → model rule sequence → output. The captured unit is a Fractal section, not a Performer step or parent event; parent provenance is opaque. Capture is a model rule family (record extent, Replace/Latch/Once, PunchIn, recordTrigger, lock), not hidden engine plumbing or a separate manual recorder phase. Trunk cell contract stores 11-bit CV + 4-bit gate length + valid; trunk is volatile engine state, never serialized.
**Next action:** Finish Phase 0 by validating TASK/DICTIONARY/HTML agreement after the command-rule update, then resume Phase 1 model work with STM32 sizeof probe when RAM budget opens.
**Depends on:** resource-optimization (needs RAM headroom), stochastic-track-port (higher priority, will consume first available RAM)
**Branch:** TBD
**Reference:** `docs/fractal-track-options-comparison.html` (canonical architectural ref), `.tasks/fractal-track-implementation/TASK.md`, `.tasks/fractal-track-implementation/DICTIONARY.md`, `docs/superpowers/specs/2026-05-17-fractal-track-design.md` (historical spec; superseded by HTML for MVP shape), `docs/superpowers/specs/2026-05-17-fractal-advanced-research.md`

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
