# Task Board
_Updated: 2026-05-25 (Stochastic, fourth session pass — Loop page chip cleanup + Live page constellation redesign: Loop page bottom-band MaskR/TiltR chips (which overflowed the footer separator at y=53) replaced with thin vertical bars in the left/right margins; MaskM/TiltM moved to the same side-bar scheme on the right; top tiny readout centered with 5 chips PR/PM/MU/JU/SL (M→MU, J→JU, S→SL); held-step labels for the four mask/tilt knobs expanded to full words "MASK RHYTHM" / "TILT RHYTHM" / "MASK MELODY" / "TILT MELODY"; stale LAST slot removed and Size/Rotate shifted one index lower (case 9 = Size, case 10 = Rotate, slot 11 freed). **Live page constellation:** comet-trail walker (pitch guide rails, slide ribbons, legato rails, age-based stride math) replaced with 12 fixed rings in a seed-hashed XY field — each event keyed by a monotonic `serial` baked into the engine's history struct, so age fade happens in place while position stays put. Bell haze (Bias center / Spread width) preserved. Contour skews via per-event serial bits. Variation wobbles ring radius. Rest culls a slot to a faint dot. Burst petals draw around the bright ring (newest event). Repeat (>50) double-strokes the outline. NewR/NewM reshuffles the seed → next event lands at a fresh XY. Chip strip reduced from 16 single-letter cells to the 11 important knobs (D/V/R/BU/BC/BR on top, E/S/O/X/I bottom), grid-locked with `%-2s%3d` formatting so digit changes never shift positions. UI-preview workflow rule added to CLAUDE.md + AGENTS.md: any OLED-page change must be rendered via `ui-preview/` and `open`'d before any chat description. Previous-session highlights (still landed): Live capture on NewR/NewM (mode-aware, auto-switches to Loop); Pitch+Duration quick-edit at slots 13/14/15; Mutate unipolar-destructive; Duration/Variation/Rest three-zone law; Burst 4-state + Fit-curve flams; Patience log2 curve; ContentSnapshot; Shift+NewR/NewM undo; pitch 5-step law.)_

**Naming convention from 2026-05-24 forward:** older entries below use prior names that are consistent with the code at the time they were written. Current canonical: step (idea, int), `StochasticStepContent` (stored), `RuntimeStep` (runtime), `StepCache` (container), `Gate` / `Cv` (output).

## 🟢 generator-preview-apply — Generator A/B preview, step selection, 64-step context, Tuesday AlgoGenerator
**Status:** done — Phases A-F complete and hardware verified.
**Where I stopped:** All phases committed. SequenceBuilder 3-copy state machine, RandomGenerator enhancements, GeneratorPage A/B workflow, 64-step bank visualization, context menu expansion, and Tuesday AlgoGenerator (15 algorithms via TuesdayAlgoCore). RAM: `.data + .bss = 118,884` (92.9%).
**Next action:** Merge `feat/generator` to master when user confirms.
**Depends on:** nothing
**Branch:** feat/generator

---

## 🟡 stochastic-track-port — Loop chip cleanup + Live constellation redesign landed
**Status:** Pitch generator runs the five-step sequential law in `PITCH-LAW-FINAL.md` (class roll w/ recency, class repeat check, Range candidate set bipolar 0..100, Bias/Spread beta-distribution, Contour direction nudge). Duration picker runs a three-zone Variation blend (home / ticket / explore overlapping triangles) with three-state duration tickets (-1/0/1..100). Rest dice are independent per-cell in Loop mode (salted RNG), making Rest scrubbable without rerolling durations. Burst is a 4-state pitch×timing matrix (HoldFit/HoldOver/RollFit/RollOver) — Fit mode packs cluster cells into one prev_dur via BurstRate curve (r ∈ 0.4..2.5, log-symmetric, knob<50 accel, knob>50 decel) for trap-style flams; Overflow keeps the existing uniform-denom math. Cluster cells now floor at 12 ticks (`kMinClusterCellTicks`) so the gate-clamp leaves a discrete retrigger gap; auto-reduces BurstCount if prev_dur can't house density. `childCount`→`burstTails` rename + 3-bit field semantic = total-cells-minus-one. Mutate is unipolar 0..100, destructive-only (permute branch dropped). Patience knob mapped on a log2 curve (1..128 across the knob with ~2× per 14 units). Steps Sieve removed from generator; pitch-centrality is now a trigger-time MaskM/TiltM filter (LoopM-only). MaskR/TiltR renamed; TiltR unipolar 0..100 center 50 (low-pass = longs survive). Range bipolar 0..100, default 50. **Performance gestures (current):** mode-toggle buttons (LoopR/LiveR, LoopM/LiveM) are plain toggles with no side effects. NewR/NewM in **Live mode** captures the live shadow as loop content and auto-switches to Loop (engine `_capturedFromLive*` flag flips the cache to read events[] verbatim instead of seed-rolling). NewR/NewM in **Loop mode** renews (double-press guard), capturing an undo snapshot before fire. Shift+NewR/NewM = undo last Loop renew (footer labels swap to UndoR/UndoM). PAGE+step15 on the Live page randomizes the 16 Live-row knobs. Pitch and Duration ticket pages share the same quick-edit column at visual steps 13/14/15 (Init/Even/Random). **Live page step layout reshuffled** for semantic grouping: rhythm/articulation top-row (1-6 red), burst top-right (7-8 orange), pitch shape bottom-row (9-14 green), Repeat standalone at 15 (red), BurstRate corner at 16 (orange). Tier 4 ContentSnapshot (~92 B, seeds+knobs+tickets, no events) added as foundation for A/B preview / Lock.
**Where I stopped:** All 7 stochastic + queue tests green on sim release; STM32 release builds clean. 21 generator test cases + 34 cache parity test cases including Fit cluster curve, accel/decel asymmetry, rest dice independence, ticket states, anti-collapse contract, mutate window targeting, content snapshot roundtrip. `finalize.md` captures the closing punch list (real bug at the top: `StochasticFeel` routing no-ops; inert Lock toggle; performance-track parity gaps vs NoteTrack; dead includes; wire-format waste; doc drift; test coverage gaps).
**Next action:** Hardware-flash and audibly verify. Then work the **`finalize.md` punch list** (last gate before marking this task done): real bug at the top (StochasticFeel routing no-ops), inert Lock toggle, performance-track parity gaps vs NoteTrack (MIDI input path, sequenceProgress, linkData, fillMuted, Launchpad/Generator/Python integration, missing routing targets — Patience M / Range / MaskM / TiltM), dead includes, wire-format waste, cross-page Random-slot inconsistency, doc drift, test gaps. Parking-lot design items (default Spread bump, PITCH-LAW-FINAL.md sync, Range<50 up-jump clamp, soft-cap ticket curve, beta calibration, A/B preview page wiring, PHASE16 P5 flat-cell) survive after finalize.
**Branch:** feat/stochastic-seed-log
**Reference:** `.tasks/stochastic-track-port/finalize.md` (closing punch list — must clear before task complete). `.tasks/stochastic-track-port/PITCH-LAW-FINAL.md` (5-step pitch law, knob ownership, anti-collapse safeguards). `.tasks/stochastic-track-port/LOCK-DESIGN-DEFERRED.md` (three Lock candidates: tape replay, RNG snapshot, A/B preview — A/B paradigm sketched). `.tasks/stochastic-track-port/PHASE16-FLAT-CELL.md` (flat-cell rhythm rewrite, separate track, P1-P8 unwired). `.tasks/stochastic-track-port/PHASE15-PITCH-MATH-REVIEW.md` (original review, mostly superseded by PITCH-LAW-FINAL.md). `.tasks/stochastic-track-port/PHASE17-SEED-DELTA.md` (structural cleanup parking lot). `.scratch/stochastic-knob-ownership.html` (current knob inventory).

## ⚪ stability-fixes — Narrow crash/corruption fixes from adversarial reviews
**Status:** ready — Modulator/Generator fixes captured; Scope/CV Router/Bus recommendations added.
**Where I stopped:** Audits found four initial stability fixes plus CV Router physical-output staleness, bus last-writer ambiguity, bus shaper stale-state risk, and CvRoute getter hardening.
**Next action:** Fix CV Router output ordering first, then implement the Modulator/Generator crash/corruption fixes and add bus writer observability/priority.
**Depends on:** nothing
**Branch:** TBD
**Reference:** `.tasks/stability-fixes.md`

## ⚪ phaseflux — New TrackMode: 4×4 grid sequencer, stateless-ramp engine, scale-degree pitch, per-stage curves + transforms
**Status:** ready — design fully locked in `docs/phaseflux-spec.md` (18 sections, all 66 audit questions resolved, 3-reviewer audit clean). UI rendered + verified in `ui-preview/phaseflux-preview.png`.
**Where I stopped:** Spec lock complete. No open product decisions before implementation. Implementer-resolved items captured in spec §16 punch list + this task's Open questions.
**Next action:** Phase A — math & storage foundations. Add `Track::TrackMode::PhaseFlux` enum + `Version_PhaseFlux_Pending = 36` placeholder. Create `PhaseFluxTrack` / `PhaseFluxSequence` / Stage record model files with bit-packed 3×uint32_t records. Land serialization round-trip test (retro lesson #4 gate) before any engine code.
**Depends on:** nothing
**Branch:** TBD (suggest `feat/phaseflux`)
**Reference:** `.tasks/phaseflux/task.md` (deep context). `docs/phaseflux-spec.md` (locked spec — single source of truth). `docs/spec-template.md` (process lessons extracted). `ui-preview/pages_phaseflux.py` + `ui-preview/phaseflux-preview.png` (UI render).

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
