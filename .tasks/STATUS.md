# Task Board
_Updated: 2026-05-29 (PhaseFlux polish round landed: mask+tilt orthogonal union replaces dead-zone formula in both engines; CycleLength: Adaptive | Fixed sequence field (skip mutes vs shrinks); Accum Mode (Stage/Sequence per accumulator) exposed in sequence list + ACCUM topic F2 chip "M:Sta"/"M:Seq"; ACCUM.NT/.PT top-label suffix dropped (single source of truth lives in the chip); root-caused 3-5min hw reboot to DBG_PFX_ENABLE committed enabled → printf-flooded heap on STM32, define disabled. STM32 image -704 bytes from debug strip. Sim + STM32 release builds clean.) (PhaseFlux feature surface complete — entering slow-polish phase pending hardware audition. Merged into parent `feat/stochastic-seed-log` (sibling `feat/tt2-v2-native` will need rebase/merge after).)_

**Naming convention from 2026-05-24 forward:** older entries below use prior names that are consistent with the code at the time they were written. Current canonical: step (idea, int), `StochasticStepContent` (stored), `RuntimeStep` (runtime), `StepCache` (container), `Gate` / `Cv` (output).

## 🟡 routing-matrix — unified scope-addressed mod matrix (name-agnostic engine)
**Status:** active — design locked & Codex-clean; param registry curation in progress. Plan `docs/plans/2026-05-31-002-routing-mod-matrix-overhaul-plan.md` + sub-spec `2026-06-01-003-routed-value-storage-subspec.md`. Engine delivers a normalized value into `(scope, paramKey)`; per-type param tables; no flat `Target` enum. **Unified combine:** one signed per-track `d`, base-anchored, no `min/max` window — `out = clamp(base + d·u·range)`, combine flag picks centered (Modulate, neutral-at-center) vs raw (Absolute, sweep-from-base) source; `d` subsumes bias+depth+window. Routed value is a model-owned transient override (delta; read = `clamp(base+delta)`), killing the per-pattern `Routable` duplication + ×17 copy loop. `scaleSource = Source` only (R6). Clean-break format (R9).
**Where I stopped:** On `refactor/routing-matrix`: U1 `RouteParam` (+fail-closed guard), trigger unification. **Shared `ParamKey` registry** (`model/RouteParamKey.h`, append-only authority, F6); **U2 Global + U3 Note + U4 Curve + U4 Indexed tables on it.** Curve validates "one key, many types"; **Indexed adds the first-class INLET row-kind** (A/B fill `_routedIndexedA/B` −100..100→−1..1, flagged `Inlet`; `setRoutedIndexedA/B` added; legacy DiscreteMapSync borrow dropped per R13; inlet keys 100/101). All hooks mirror `writeTarget` incl. parity quirks (CurveRate /100, Wavefolder/DjFilter truncate→int16_t; Indexed Divisor/Scale/RootNote base-write). `TestParamRegistry` guards per-table key uniqueness; parity green; sim + STM32 release clean; nothing wired live. **Param triage RESOLVED.** Codex U7 watch-item: `uint8_t` key width settled at cutover (~130–160 keys fit). Full snapshot in TASK.md.
**Next action:** Remaining U4 tables: Tuesday, DiscreteMap (In/Scanner/Sync inlets + SlewTime≡SlideTime), Stochastic (defect-fixes), PhaseFlux (A/B inlets + nudges + Phase, defect-fix), MidiCv → U5 scaleSource, U6 groups, U6b override+combine (range-class), U7 cutover.
**Depends on:** spine refactor (landed on dev: WallClock, slave-guard, single-pass engine recompute, linking removed).
**Branch:** refactor/routing-matrix
**Reference:** `.tasks/routing-matrix/TASK.md` (decision log). `.scratch/param-groups.html` (grouped triage canvas). `.scratch/param-census.html` (full per-type surface). `.tasks/core-architecture-optimization/routing-five-sources-map.md` (the smear this replaces).

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
**Reference:** `.tasks/stochastic-track-port/finalize.md` (closing punch list — must clear before task complete). `.tasks/stochastic-track-port/LIVE-CONSTELLATION.md` (Live page metaphor + per-event serial XY contract). `.tasks/stochastic-track-port/PITCH-LAW-FINAL.md` (5-step pitch law, knob ownership, anti-collapse safeguards). `.tasks/stochastic-track-port/LOCK-DESIGN-DEFERRED.md` (three Lock candidates: tape replay, RNG snapshot, A/B preview — A/B paradigm sketched). `.tasks/stochastic-track-port/PHASE16-FLAT-CELL.md` (flat-cell rhythm rewrite, separate track, P1-P8 unwired). `.tasks/stochastic-track-port/PHASE15-PITCH-MATH-REVIEW.md` (original review, mostly superseded by PITCH-LAW-FINAL.md). `.tasks/stochastic-track-port/PHASE17-SEED-DELTA.md` (structural cleanup parking lot). `.scratch/stochastic-knob-ownership.html` (current knob inventory).

## 🟡 teletype-v2-dialect — Performer-native Teletype++ dialect and Phase 1 data-model plan
**Status:** active — Phase 2 dialect landed on `feat/tt2-v2-native`: parser/lowering/evaluator/script-runner, native SCRIPT calls, EVERY/SKIP/PROB mods, narrow L loop with Teletype-correct I semantics. Six TT2 unit tests green; sim + STM32 release build clean (rebased onto dev tip).
**Where I stopped:** `docs/teletype_v2.md` defines the keep/drop/redesign contract; `docs/teletype_v2_phase1_plan.md` outlines native program/runtime/output state. TT2TrackEngine/TeletypeInterpreter/TeletypeProgram/TeletypeRuntime/TeletypeOutputState in place.
**Next action:** Implement narrow `DEL n: body` scheduling with `TT2DelayQueue`; keep `DEL.CLR`, `$`/function-return ops, `BREAK`, `KILL` out of the first slice.
**Depends on:** RAM gates; Teletype runtime architecture lessons.
**Branch:** feat/tt2-v2-native (Phase 2 dialect work — parser/lowering/evaluator/native SCRIPT/EVERY/SKIP/PROB/loop — committed there, rebased onto dev tip)
**Subtasks (Teletype family under this dialect umbrella):**
- `teletype-performer-ecosystem-redesign` — 6-layer pipeline architecture overview (umbrella design context)
- `teletype-runtime-architecture` — VM/slot/runtime ownership; Phase 0-4 hardware-verified 🟡
- `teletype-cv-source-combiner` — raw/value-domain CV source pipeline for outputs 🟡
- `teletype-edit-page` — multi-mode (INPUT/OUTPUT/CV/TIMING) config page 🟡
- `teletype-keyboard-shortcuts` — F1-F5 + Alt shortcuts 🟢 (shipped via usb-keyboard-system)
- `teletype-file-reliability` — save/load bug fixes 🟢 (merged to master)
**Reference:** `.tasks/teletype-v2-dialect/TASK.md`

## 🟡 phaseflux — New TrackMode: 4×4 grid sequencer, stateless-ramp engine, scale-degree pitch, per-stage curves + transforms
**Status:** Feature surface effectively complete; entering slow-polish phase pending hardware audition. Engine + scope + macro topic + nudges (WarpN/RespN/PulsN/LenN/GWarp) + Snap + Zero + project beat-grid dents + Window/Repeat in scope + traversal-pattern picker (Snake + 7 René mk2 patterns) + per-stage divisor as Stochastic-style fraction-vs-seqDivisor (Whole-note default) + MaskM/TiltM pitch centrality filter + clipboard + context menu (INIT[Stage/Topic/Sequence/Track] / COPY / PASTE / DUP) + shake (2 variants on PAGE+S15/S16) + USB keyboard baseline (F1-F5 + Tab + arrows) + multi-cell StepSelection (held + Persist via Shift) + double-click step = skip toggle. Spec/template/policy docs cleaned up — ProjectVersion bump retired, sequenceShift folded into globalPhase, modes A/B confirmed shipped.
**Where I stopped:** Last commit `198b46c6` — multi-cell selection. All shipped behavior verified in sim build (build/sim/debug + build/stm32/release both clean). Merged into parent `feat/stochastic-seed-log` (sibling `feat/tt2-v2-native` to be reconciled separately).
**Next action:** **Slow polishing after testing.** Hardware audition full feature set against musical expectations; collect per-feature feedback. Likely follow-ups discovered during play: visual tweaks on scopes (dent visibility, playhead style, label collisions), tuning of randomize ranges, refinement of context-menu wording, adjusting the StepSelection Persist→Immediate transition if friction hits, potentially adding pitchPhaseWarp for Global mode pitch-decoupled evolution. Patterns 5+6 (René spirals) may need correction against the source image. Other deferred: wire `+Lim` / `-Lim` editing on `PhaseFluxListPage`; decide on cycleSpread (§14.1 Q9), pitchPhaseWarp, hysteresis for Always mode (§14.1 Q12). Don't add new big features without audition signal.
**Depends on:** nothing
**Branch:** feat/phaseflux
**Reference:** `.tasks/phaseflux/task.md` (decision log). `docs/phaseflux-spec.md` (locked spec — single source of truth). `docs/spec-template.md` §17.2-17.5 (UI infrastructure baseline ported from this work). `ui-preview/macro/` + `ui-preview/tempo-scope/` (mockup renders). Macro mockup at `ui-preview/pages_phaseflux_macro.py`.

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

## 🟢 indexed-sequence-macro-refactor — Extract macro logic into IndexedSequence model
**Status:** done — macro logic lives in the model; EditPage only dispatches.
**Where I stopped:** Eight `populateWithMacro*` methods (Rhythm/Triangle/Sawtooth/Scale/Arpeggio/Chord/Modal/RandomMelody) implemented in `IndexedSequence.cpp`; `IndexedSequenceEditPage.cpp` macro menus now call `sequence.populateWithMacro*(...)` instead of inlining the math.
**Next action:** none — launchpad-track-port Phase 4 (Macro Grid v2) dependency is now cleared.
**Depends on:** nothing
**Branch:** (folded into feature history)

---

## 🟡 stability-fixes — Narrow crash/corruption fixes from adversarial reviews
**Status:** in progress — 3 of the audit fixes landed on `fix/stability` (TDD); rest deferred.
**Where I stopped:** DONE: CvRoute lane guards, ADSR zero-tick clamp, Modulator read sanitize (3 new unit tests green, STM32 release builds). REMAINING: CV Router output ordering (critical — needs hardware audition), GeneratorPage type guards + Generator relative-index OOB (verify on `feat/generator` first), bus writer arbitration + bus shaper stale state (design-ish hardening). SortedQueue overflow + SANITIZE_TRACK_MODE already in main; slave-clock filter owned by wallclock-time-architecture.
**Next action:** Hardware-audition the CV Router output-ordering fix next session; reconcile the two generator items against `feat/generator`.
**Depends on:** nothing. (GeneratorPage + Generator-OOB overlap `feat/generator` — verify there before fixing.)
**Branch:** fix/stability (off dev)
**Reference:** `.tasks/stability-fixes.md`

---

## 🟡 performer-improvements — Non-Launchpad fork-feature integrations (VinxScorza, Modulove, Mebitek, performer-nx)
**Status:** paused — Phase 1 wired and hardware-verified (6/8 items done); generator task extracted; stability re-extracted to its own task. Fork-feature integration only now.
**Where I stopped:** Phase 1 items completed: (1) Quick octave change Step+F1-F5, (2) Double-click Page context menus with 2s auto-close, (3) Short clock pulse 1ms floor, (4-6) PerformerPage mute LEDs + pattern labels + MenuWrap. Generator preview/apply extracted to `generator-preview-apply`. Crash/corruption stability fixes re-extracted to `stability-fixes` (different concern). Still holds: performer-nx fork survey + Turing-as-Tuesday-algo actionable, and the deferred macro-overload open question.
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

## 🔵 route-reordering — Rearranging Routing::Target enum into signal-flow ordering
**Status:** blocked — gated on the routing direction decision. Reordering the `Target` enum now is throwaway work if the five-sources collapse rewrites/replaces it. Spec also stale (62-target snapshot, pre-Stochastic/PhaseFlux).
**Where I stopped:** Spec written in `reorder-spec.md` (signal-flow order). `targetSerialize()` already decouples serialization from enum values, so reordering is behavior-safe — but premature.
**Next action:** Wait for the routing direction decision (`core-architecture-optimization` → `routing-five-sources-map.md`). If the collapse happens, fold ordering into it. If "consistency pass only" wins, do this then.
**Blocked-by:** routing direction decision (core-architecture-optimization routing-five-sources-map)
**Branch:** TBD

---

## 🟡 usb-mouse-to-route — USB mouse axes/buttons as Routing::Source CV inputs
**Status:** paused — design captured; no code started. Gate identified before any feature work. Worktree at `.worktrees/usb-mouse-to-route/`.
**Where I stopped:** Walked the USB-keyboard git history (`6b0e407e` driver lift → `41ade375`/`80dd6d9c`/`b32cf6cb` failed-then-reverted in-driver OLED diagnostics → `82e64080` Engine pump → KeyboardManager extraction → `0d2b0015` mouse rejection). Established the OLED diagnostic rule: nothing inside `usbh_driver_hid.c` hooks or `hid_in_message_handler` may take a FreeRTOS mutex; only `UsbH::process()` post-poll may touch OLED. Mapped source-enum / RoutingEngine plug-in points.
**Next action:** Step 1 in TASK.md — reproduce the mouse-rejection crash on hardware with the rejection at `usbh_driver_hid.c:218-224` removed, observe whether keyboard/Launchpad survive, to establish the failure boundary before designing a fix.
**Depends on:** none (gated on libusbhost mouse-enumeration root cause investigation, which is step 1 of this task itself)
**Branch:** feat/usb-mouse-to-route (from master @ 11b5dae0)
**Reference:** `.tasks/usb-mouse-to-route/TASK.md`. Precursor: `hid-keyboard-layering` gives mouse decode a home in `hid/` so it doesn't land back in UI.

---

## ⚪ hid-keyboard-layering — Extract USB-keyboard HID decode out of UI into `hid/` module
**Status:** ready — design validated, no code started.
**Where I stopped:** `ui/KeyboardManager` straddles three layers (HID decode / event buffering / UI dispatch). Decision: sibling decode headers in a new neutral `apps/sequencer/hid/` module (no `HidDevice` base — keyboard/mouse share little decode). `hid/Keyboard.h` gets `keycodeToAscii`/`keycodeToStep` + shared `Report` struct; `hid/Mouse.h` ships as a documented stub (decode deferred to usb-mouse-to-route, which is gated on the libusbhost crash). All keyboard decode callers are internal to KeyboardManager.cpp — self-contained. Engine handler signature untouched this pass.
**Next action:** Create `hid/Keyboard.{h,cpp}` (move decode verbatim), retype ring buffer to `Hid::Keyboard::Report`, stub `hid/Mouse.h`, add `TestHidKeyboard.cpp`, wire CMake. Verify sim + STM32 release (pure code move, RAM/flash-neutral).
**Depends on:** nothing (pure relocation, no behavior change). Unblocks the `hid/` home for usb-mouse-to-route.
**Branch:** TBD
**Reference:** `docs/plans/2026-05-29-hid-keyboard-layering-design.md` (validated design). Prior `usb-keyboard-manager-refactor` (🟢 done) moved decode Ui→KeyboardManager; this moves it KeyboardManager→`hid/`.

---

## ⚪ wallclock-time-architecture — Unify time handling: WallClock service (B) + PhaseFlux/Stochastic onto tickPosition (A)
**Status:** ready — design validated against ER-101 reference, no code started.
**Where I stopped:** Two gaps. (A) PhaseFlux + Stochastic re-derive phase from discrete integer tick while six other engines read the continuous `Clock::tickPosition()`. (B) Real wall-time is scattered across 3 `os::ticks()` behavioral sites (modulator `dt` `Engine.cpp:72`, MidiOutput CC rate-limit, MidiCv voice) with no service, and `os::ticks()` is 32-bit (wraps). ER-101's lesson (`OTHERS/ortagonal/er-101/wallclock.h`): single 64-bit free-running reference + `walltimer` with drift-free `end_time += delay` restart. NOT importing ER-101's synthetic-clock/tick-adjustment — xformer transport already centralizes cross-track sync.
**Next action:** B first (substrate): add a 64-bit µs `WallClock` + `WallTimer` value type, migrate the 3 scattered sites, fold the slave-period outlier guard (ER-101 0.5×–2× latch) in — this absorbs the Phase 0 slave-clock item. A is parallel/independent: PhaseFlux + Stochastic read `tickPosition()`. Verify sim + STM32 + timing parity; hardware-check slave guard against a jittery clock.
**Depends on:** nothing. Overlaps performer-improvements Phase 0 (slave-clock filter — same fix, this is its proper home).
**Branch:** TBD
**Reference:** `docs/plans/2026-05-29-wallclock-time-architecture-design.md` (validated design). ER-101 source at `OTHERS/ortagonal/er-101/` (`wallclock.h`, `res-er-clock.md`).

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
