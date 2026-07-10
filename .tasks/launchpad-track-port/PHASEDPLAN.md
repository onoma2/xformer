# Launchpad Mini MK1 — Phased Implementation Plan

> Created: 2026-05-17 · Revised: 2026-07-03 (covers the full 11-mode TrackMode enum)  
> Hardware constraint: **Only Launchpad Mini MK1 available for testing.** All phases prioritize MK1-verifiable work. MK2/MK3/Pro/X-specific features are flagged explicitly.

---

## Mini MK1 Capabilities & Limitations

| Feature | Mini MK1 Behavior |
|---------|-------------------|
| Protocol | Base `LaunchpadDevice` — no SysEx init required |
| Grid notes | `note = row * 16 + col` (rows 0–7, cols 0–7) |
| Scene buttons (right col) | `note = col * 16 + 8` |
| Function buttons (top row) | CC 104–111 |
| Colors | Red + Green only (4 brightness levels each) → amber/yellow/orange mixes |
| **No blue** | The "Blue" LP style setting will be invisible/untestable on MK1 |
| RGB | Not supported — any RGB-specific color remapping is MK1-no-op |

**Implication:** All core functionality must work with red/green/amber only. Color styles should default to **Classic** (amber/yellow/green) for MK1 users. Blue style can be implemented for MK2+ owners but cannot be verified on our test hardware.

---

## Track-Mode Coverage (all 11 modes)

| Mode | Grid verdict | Phase |
|------|--------------|-------|
| Note | Ported; regression fixes remain | E1 |
| Curve | Ported | — |
| MidiCv | Not ported — no sequence data (arpeggiator only) | — |
| Tuesday | Port | E2 |
| DiscreteMap | Port | E3 |
| Indexed | Port | E4 |
| Stochastic | Owner decision — steps are generator outputs (seed + knobs); free grid painting fights regeneration | D8 |
| Fractal | Not ported — no per-cell model data; cell content is volatile engine state (`FractalTrackEngine::_trunk`, CCMRAM). The sequence holds only window brackets + branch-pool params (knob territory, not grid content) | — |
| PhaseFlux | Port — 16 model-resident stages, OLED grid editor already exists (`PhaseFluxEditPage::drawGrid`) | E6 |
| TeletypeV2 | Scripts not portable (text VM, same verdict as the removed TeletypeTrack); 4×64 tt-pattern bank could map to a paged grid | D9 |
| TeletypeMini | Same as TeletypeV2 (per-scene programs) | D9 |

**Code facts (verified 2026-07-03):**
- `LaunchpadController.cpp` handles only Note + Curve; all 12 `trackMode()` switches `default: break` (`LaunchpadController.cpp:303-620`). Every other mode gets a blank, no-op sequence grid — no crash path (union access only inside Note/Curve case bodies). Track select, mute, fill, pattern select stay mode-agnostic.
- `Project.h` selected-sequence accessors exist for Note/Curve/Tuesday/DiscreteMap/Indexed/PhaseFlux (`Project.h:506-557`); **none** for Stochastic/Fractal/TT2/TT2Mini.
- No new mode has a model `Layer` enum. PhaseFlux's stored `edited()` flag (`PhaseFluxSequence.h:568`) has zero writers — dead state; E0.1 adds a computed `isEdited()` instead. Stochastic tracks `rhythmValid()`/`melodyValid()` flags.
- Stochastic has no independent last step: `last()` is `size()-1` (`StochasticSequence.h:478`).

---

## Phase Tier Overview

| Tier | What | Effort | Cumulative |
|------|------|--------|------------|
| **Essential** | Pre-flight optimization + core fixes + 6 track types (incl. PhaseFlux) + Performer | ~22h | ~22h |
| **Baseline** | Mebitek parity + long-press + range fills + visual polish | ~10.5h | ~32.5h |
| **Requires User Decisions** | Generators, macros, undo, follow, locking, prob overlay, loop mod, Stochastic grid, TT2 pattern grid | ~19.5h | ~52h |
| **Extra** | Chaos params, RGB remapping, global config, other controllers | ~13–15h | ~65–67h |

**Recommended ship target:** Essential + Baseline (~32.5h).  
**Absolute MVP:** Essential only (~22h).

---

## ESSENTIAL — Ship First

> Criteria: must work on Mini MK1, testable, no user decisions required, unblocks Baseline.

### E0. Pre-Flight (~4.5h)

| # | Task | Detail | Source |
|---|------|--------|--------|
| E0.1 | Add computed `isEdited()` to `DiscreteMapSequence`, `IndexedSequence`, `TuesdaySequence`, `PhaseFluxSequence` | Note's compare-to-defaults pattern (`NoteSequence.cpp:276-282`); safe, no serialization impact. Do NOT reuse PhaseFlux's stored `edited()` flag — it has zero writers (dead state). | Plan |
| E0.2 | Split `LaunchpadController.cpp` into per-track files | `LaunchpadNoteController.cpp`, `LaunchpadCurveController.cpp`, etc. Keep dispatch in main file. Prevents 950 → 2500+ line explosion. | Plan |
| E0.3 | Fill `Project.h` accessor gaps | Exist: Note/Curve/Tuesday/DiscreteMap/Indexed/PhaseFlux (`Project.h:506-557`). Missing: Stochastic/Fractal/TT2/TT2Mini — add only what an approved phase needs (D8/D9). | Plan |
| E0.4 | Add `selectedLayer` to `_sequence` state struct | Required for track types without native Layer enums (Indexed, DiscreteMap, Tuesday, PhaseFlux) | Plan |
| E0.5 | **Add `LedSemantic` enum + `semanticColor()` helper** | Replace boolean `active/current` in `stepColor()` with semantic levels: Off/Background/DataPresent/Active/Playhead. Maps to MK1 red/green/amber consistently. | Kria |
| E0.7 | **Add `_dirtyRows` row-dirty mask to `LaunchpadDevice`** | 8-bit mask **plus compare-on-write in `setLed()`** — the controller clears and repaints every frame (`LaunchpadController.cpp:141-149`), so a plain dirty-on-write mask is always dirty. Skips the 80-compare loop on idle frames. | libavr32 |
| E0.8 | **Extract `VirtualGrid` pagination helper** | Reusable class for >8-step tracks (Indexed 48, DiscreteMap 32, PhaseFlux 16). Centralizes Left/Right page math, view offset, virtual→physical col mapping. | midigrid |
| E0.9 | **Add `Modifier` bitmask enum + `_modifierMask` byte** | Shift/Loop/Prob/Time modifiers as bit flags. Replaces 7 sequential `buttonState()` checks in guards. Enables overlay modes. | Kria |
| E0.10 | **Add `DeviceCapabilities` struct to device classes** | `supportsRgbSysex`, `supportsDoubleBuffer`, `ccEdgeButtons`, `maxBrightnessLevels`. PID checks exist once today (ctor factory dispatch, `LaunchpadController.cpp:103-117`) — land this with its first consumer (B2 LP Style), not before. | midigrid |
| E0.12 | **Hoist `mapColor` table to base class + deduplicate** | Single `static constexpr uint8_t colorMap[16]` instead of 4 copies in derived headers. Saves ~48B flash. Only the MK1 base-class path is hardware-testable; MK2/MK3/Pro changes are compile-only. | OptReview |
| E0.13 | **Replace `std::function` handlers with function pointers** | `SendMidiHandler` / `ButtonHandler` as raw fn ptr + `void *user`. Saves ~64B RAM, removes indirection. Same compile-only caveat as E0.12 for non-MK1 devices. | OptReview |

> **Not in plan — buffer-then-blast syncLeds / TX scratch buffer:** the TX path already batches. `sendMidi` enqueues into a 128-entry ring buffer (`src/platform/stm32/drivers/UsbMidi.h:18-24,96`) that drains asynchronously; there is no bulk-transfer API at the device-class layer, and `syncLeds()` already fills the ring in one pass (`LaunchpadDevice.cpp:52-84`). A scratch buffer would add .bss against the same budget this plan polices, for nothing.

**Pre-flight subtotal: ~6h** (11 items — the earlier ~4.5h/13-item figure was not credible against the 950-line split + VirtualGrid + device-class refactors; the day plan already allocated 3 days)

### E1. NoteTrack Regression Fixes (~1.5h)

| # | Task | Detail | MK1 Test |
|---|------|--------|----------|
| E1.1 | Add 6 missing layers to `noteSequenceLayerMap` | AccumulatorTrigger, PulseCount, GateMode, HarmonyRoleOverride, InversionOverride, VoicingOverride | ✅ |
| E1.2 | Fix `currentStep` for non-Linear modes | Use `currentNoteStep()` when `mode() != Linear` | ✅ |
| E1.3 | Add `NoteSequence::Mode` selector | Show Linear/ReRene/Ikra in RunMode picker area | ✅ |
| E1.4 | Add `noteFirstStep` / `noteLastStep` editing | When `mode() == Ikra`, FirstStep/LastStep buttons edit Ikra bounds instead of global bounds | ✅ |

### E2. TuesdayTrack Step-Key Grid (~2.5h)

| # | Task | Detail | MK1 Test |
|---|------|--------|----------|
| E2.1 | 2×8 step-key parameter grid | Rows 0–1 mirror hardware step buttons 1:1 (Octave±, Transpose±, Root±, Divisor±, Mask±, Loop±) | ✅ |
| E2.2 | Special hold actions | Shift+Step7 = Jam Run (mute gate), Shift+Step15 = Reset engine | ✅ |
| E2.3 | FirstStep/LastStep/RunMode/Fill remapping | FirstStep→Reseed, LastStep→Reset, RunMode→Cycle algorithm, Fill→Jam Run toggle | ✅ |
| E2.4 | QuickEdit overlay (Navigate hold) | Copy/paste clipboard + Randomize on rows 2–3 | ✅ |

### E3. DiscreteMapTrack 32-Stage Editor (~2.5h)

| # | Task | Detail | MK1 Test |
|---|------|--------|----------|
| E3.1 | LayerMapItem array (3 per-stage layers) | Threshold, Direction, Note — the only per-stage data (`DiscreteMapSequence.h:30-89`). GateLength/SlewTime/Pluck are **sequence-wide scalars** (`:161-250`) — not grid layers; they stay on the OLED. | ✅ |
| E3.2 | 4-page navigation (Left/Right) | 32 stages → 4 pages × 8 columns | ✅ |
| E3.3 | Per-param draw functions | Bars for thresholds; color-coded dots for direction; note dots | ✅ |
| E3.4 | Per-param edit handlers | Tap column to set value; row maps to range | ✅ |
| E3.5 | Active stage highlighting | Use `engine.activeStage()` + `rampPhase()` position bar | ✅ |

### E4. IndexedTrack 48-Step Grid Editor (~3.5h)

| # | Task | Detail | MK1 Test |
|---|------|--------|----------|
| E4.1 | LayerMapItem array (4 pseudo-layers) | Pitch, Duration, Gate/Slide, Route | ✅ |
| E4.2 | Dynamic step pagination | 1–48 steps, 8 per page, Up/Down/Left/Right navigate | ✅ |
| E4.3 | Draw functions per view | Pitch dots, duration bars, gate/slide bits, route indicators | ✅ |
| E4.4 | Edit handlers per view | Tap to set pitch/duration/gate; double-tap for slide/group | ✅ |
| E4.5 | `firstStep` as rotation offset | Shows 0..activeLength-1 on grid | ✅ |
| E4.6 | Repurpose LastStep for `activeLength()` | Add/remove steps from sequence | ✅ |

### E5. Basic Performer Mode (~3h)

| # | Task | Detail | MK1 Test |
|---|------|--------|----------|
| E5.1 | Implement stubbed `performerDraw/Button/Enter/Exit` | Currently empty bodies with `// TODO` | ✅ |
| E5.2 | Layer 0: SequenceLength | Two-button tap sets firstStep/lastStep across all 8 tracks simultaneously | ✅ |
| E5.3 | Layer 1: Overview | 8×8 grid = 8 tracks × 8 steps; gates toggle directly | ✅ |
| E5.4 | Track/scene mute + fill | Shift+Scene = mute/solo; Fill = momentary fill | ✅ |

> **Scope:** E5.2 (loop bounds) and E5.3 (gate toggles) act on **Note and Curve tracks only** — the other modes have divergent loop models (Indexed `activeLength`, PhaseFlux `firstStage/lastStage`, Tuesday `loopLength`, Stochastic `last()==size()-1`) and no per-step gate. Rows for non-Note/Curve tracks are display-only (playhead + mute state). Extending per-mode row behavior is an owner decision, out of Essential.

### E6. PhaseFluxTrack 16-Stage Grid Editor (~3h)

| # | Task | Detail | MK1 Test |
|---|------|--------|----------|
| E6.1 | LayerMapItem array (5 pseudo-layers) | PulseCount, BasePitch, GateLength, Skip, Length — all `Stage` bitfields (`PhaseFluxSequence.h:120-308`) | ✅ |
| E6.2 | 2-page stage navigation | 16 stages → 2 pages × 8 columns via VirtualGrid (E0.8) | ✅ |
| E6.3 | Draw functions per layer | Bars for pulseCount/gateLength/length; dots for basePitch; bits for skip. Playhead from `PhaseFluxTrackEngine::activeCell()` | ✅ |
| E6.4 | Edit handlers per layer | Tap column/row to set value; B3.1 pattern indicators read the computed `isEdited()` (E0.1) | ✅ |
| E6.5 | FirstStep/LastStep buttons → stage window | Map to `firstStage()`/`lastStage()` (grid-index loop window, `PhaseFluxSequence.h:447-461`) | ✅ |

> **GateLength sentinels:** the engine treats 0 = drop pulse, 1 = min-floor, ≥2 = percent (`PhaseFluxTrackEngine.cpp:490-493`) — the grid is this field's first edit surface (no OLED page writes it). Row mapping reserves the two bottom rows for 0 and 1; remaining rows step the percent range.  
> **Edit resolution (applies to E3/E4/E6):** wide-range layers (BasePitch ±63, Length 1..127) tap-set coarse 8-level values; fine resolution stays on the OLED encoder. Grid taps quantize, never round-trip-corrupt an existing finer value the tap didn't change.

**Essential subtotal: ~22h** (E0 ~6h + E1–E5 ~13h + E6 ~3h)

---

## BASELINE — Mebitek Parity

> Criteria: features from `temp-ref/mebitek-performer/` that work on MK1 and improve daily usability.

### B1. Circuit Keyboard + LP Note Style Setting (~2h)

| # | Task | Detail | MK1 Test |
|---|------|--------|----------|
| B1.1 | `LaunchpadNoteStyle` user setting | `Classic` (0) vs `Circuit` (1); default = **Classic** for MK1 | ✅ |
| B1.2 | Circuit keyboard layout | Rows 3–4 = chromatic keyboard (semitones + tones); row 6 = octave; row 7 = page | ✅ |
| B1.3 | Scale-aware note input | Bypass-scale support; live step entry (select note → tap step) | ✅ |

### B2. LP Style Setting (~1h)

| # | Task | Detail | MK1 Test |
|---|------|--------|----------|
| B2.1 | `LaunchpadStyleSetting` user setting | `Classic` (0) vs `Blue` (1); default = **Classic** | ✅ (Classic only) |
| B2.2 | Color mapping | Classic = amber/yellow/green; Blue = blue-centric (MK2+ only, MK1 falls back to Classic) | ✅ |

> **Note:** Blue style will not be visually distinct on MK1. Implement it for MK2+ compatibility but default to Classic.

### B3. Visual Feedback Enhancements (~2h)

| # | Task | Detail | MK1 Test |
|---|------|--------|----------|
| B3.1 | Pattern edited indicators | Pattern mode shows edited patterns per track (dim yellow/red) | ✅ |
| B3.2 | Mute status on scene buttons | Scene LEDs reflect track mute state | ✅ |
| B3.3 | Octave lines in note views | Horizontal lines at octave boundaries | ✅ |
| B3.4 | Requested-pattern dimmed colors | Show pending pattern changes before they take effect | ✅ |

### B4. Enhanced Button Events (~1.5h)

| # | Task | Detail | MK1 Test |
|---|------|--------|----------|
| B4.1 | Double-tap detection | Already have `ButtonAction::DoublePress` in enum; wire it up | ✅ |
| B4.2 | Long-press detection | Add hold timing to `_buttonTracker` | ✅ |
| B4.3 | State tracking improvements | Better press/release tracking for modal interactions | ✅ |

### B5. Interaction Improvements (~1h)

| # | Task | Detail | MK1 Test |
|---|------|--------|----------|
| B5.1 | Fill functionality | Momentary fill across all tracks | ✅ |
| B5.2 | Range editing | Select step ranges for bulk operations | ✅ |
| B5.3 | Run mode selection enhancements | Visual run mode picker with current mode highlighted | ✅ |

### B6. Long-Press Detection (~1.5h)

| # | Task | Detail | MK1 Test |
|---|------|--------|----------|
| B6.1 | Add `ButtonAction::LongPress` enum value | Extend existing event system | ✅ |
| B6.2 | Timer-based long-press detection in `update()` | Check held buttons against threshold; dispatch LongPress event | ✅ |
| B6.3 | Assign long-press gestures | Pattern slot = save; step = copy; track button = mute/solo | ✅ |

### B7. Min/Max Range Fills & Visual Polish (~1.5h)

| # | Task | Detail | MK1 Test |
|---|------|--------|----------|
| B7.1 | Range fill visualization | Dim bar from min to max with bright endpoint at current value (DiscreteMap thresholds, Indexed durations) | ✅ |
| B7.2 | Cued pattern pulsing animation | Alternate brightness levels every N frames for pending pattern changes | ✅ |
| B7.3 | Cache `isEdited()` in pattern mode | `uint64_t _patternEditedCache[8]` — invalidate on edit/switch. Eliminates 64 calls/frame. | OptReview |

**Baseline subtotal: ~10.5h** (was ~7.5h)

---

## REQUIRES USER DECISIONS

> These features are valuable but need you to choose behavior before implementation. Each can be deferred without breaking Essential or Baseline.

### D1. Generators Mode (~4h) — **BLOCKED: needs your decision**

**What vinx did:** Split architecture (Note vs non-Note), preview/apply workflow with A/B state, grid of generator shortcuts (Random, Acid, Vandalize, Euclidean, Init, etc.).

**Decision needed:**
- Which generators to include? All vinx generators, or a subset?
- Note-track-only generators, or also Curve/DiscreteMap/Indexed/Tuesday?
- Grid layout: which generator goes where?
- A/B preview workflow: keep vinx's TOP 5=A/B, TOP 6=Reset, TOP 7=Cancel, TOP 8=Apply?
- **Dependency:** Core generator A/B state machine lives in `generator-preview-apply` task (ContentSnapshot A/B foundation — landed). Not a blocker anymore; only the owner decisions above gate this.

**Default if deferred:** Use OLED GeneratorPage only; no Launchpad grid shortcuts.

### D2. Macro Grid (~3h) — **BLOCKED: needs your decision**

**What the plan proposed:** Shift+Fill enters macro mode; grid shows batch operations (Init, Invert, Reverse, Randomize, etc.).

**Decision needed:**
- Which macros per track type? All 12 Curve macros + 16 DiscreteMap macros + the 8 Indexed model macros (`IndexedSequence.h:542-552`, `populateWithMacro*` — model-resident, no blocker)?
- Trigger: Shift+Fill? Or dedicate a function button?
- Grid layout: 4×3 for Curve, 4×4 for DiscreteMap? Category navigation for Indexed?
- Apply range: current visible step range, or full loop?

**Default if deferred:** Use OLED context menus for macros; no Launchpad grid macro mode.

### D3. Undo/Redo Shortcut (~1h) — **BLOCKED: needs your decision**

**What vinx did:** `TOP 7 + TOP 8` = Undo/Redo toggle on step-edit pages.  
**What the plan proposed:** `Shift + Play` shortcut mirroring hardware `PAGE + S7`.

**Decision needed:**
- Shortcut mapping: Shift+Play? TOP 7+8? Something else?
- 1-level undo or multi-level?
- Which pages support it? All sequence pages, or only Note/Curve?
- **Dependency:** the general undo stack is still in `performer-improvements` (TT2 script undo shipped separately and doesn't generalize).

**Default if deferred:** Use hardware `PAGE + S7` only; no Launchpad undo shortcut.

### D4. Pattern Follow Mode (~1h) — **BLOCKED: needs your decision**

**What vinx did:** `FollowMode` button toggles auto-scroll so the 8-step window tracks the playhead. Options: Off, Display, LaunchPad, DispAndLP.

**Decision needed:**
- Auto-scroll behavior: center playhead? left-align? page-jump?
- Which tracks support it? All track types, or only Note/Curve?
- Should Performer overview also follow?

**Default if deferred:** Static grid only; no auto-scroll.

### D5. Track Selection Locking (~1.5h) — **BLOCKED: needs your decision**

**What vinx did:** Lock track/scene switching when modal dialogs or generator selectors are open.

**Decision needed:**
- Lock on: modal dialogs? generator selectors? all "top-level" pages? project/layout/routing pages?
- Auto-jump behavior: when exiting a top-page, auto-select Steps page?

**Default if deferred:** No locking; user must be careful not to switch tracks during sensitive operations.

### D6. Probability Overlay Mode (~1.5h) — **BLOCKED: needs your decision**

**What kria did:** Dedicated **Prob** modifier. Hold it, grid shows probability dots per step. Tap at height to set.

**Decision needed:**
- Trigger: Shift+Layer? Shift+Prob (new button concept)? Long-press Layer?
- Which probabilities to overlay: GateProbability, RetriggerProbability, NoteVariationProbability, all at once?
- Visual encoding: 4 levels (0/25/50/100%) as 4 row heights, or continuous bar?

**Default if deferred:** Use individual probability layers in layer map.

### D7. Loop Modifier — Hold + Two-Tap (~1h) — **BLOCKED: needs your decision**

**What kria did:** Hold Loop mod, tap first step, tap second step → sets loop start/end.

**Decision needed:**
- Trigger: Shift+FirstStep? New dedicated modifier? Long-press FirstStep?
- Should it be temporary (release = restore original loop) or permanent?
- Apply to which tracks: all track types, or only those with firstStep/lastStep?

**Default if deferred:** Use existing FirstStep/LastStep picker buttons.

### D8. Stochastic Grid (~3h) — **BLOCKED: needs your decision**

**What exists:** per-step `StochasticStepContent` (`StochasticTypes.h:96-177`) — rhythm domain (durationIndex, densityRank, burst, rest/legato/slide) + melody domain (degree, octave). But steps are **generator outputs** regenerated from seed + knobs; the OLED page edits them via held-step + encoder (hero-param model), not free painting. Engine playhead: `StochasticTrackEngine::currentStep()` (`.h:49`).

**Decision needed:**
- What should the grid edit? (a) mirror the hero-param model — select loop steps, rows set degree/octave/duration; (b) toggle rows for rest/legato/slide only; (c) monitor-only playhead + gate view.
- Edits are overwritten on reseed — acceptable, or restrict the grid to captured loop steps?
- Includes rest-probability editing (formerly X6 — the Stochastic model has landed, branch blocker gone).
- Pre-req: no `selectedStochasticSequence()` in `Project.h` — add it (E0.3) or route through `Track`.

**Default if deferred:** OLED `StochasticSequenceEditPage` only.

### D9. TT2 Pattern Bank Grid (~3.5h) — **BLOCKED: needs your decision**

**What exists:** TeletypeV2/Mini carry 4 tt-patterns × 64 int16 cells (`TeletypeProgram.h:104`) — one pattern = one 8×8 page, 4 pages total. The existing editor is a scrolling row list with digit entry (`TeletypePatternViewPage`), not a grid. Scripts stay excluded (text VM — same verdict as the removed TeletypeTrack).

**Decision needed:**
- Arbitrary int16 values can't be tap-entered — what gesture? (a) tap toggles 0/nonzero (trigger-bank use); (b) row height = coarse 8-level value; (c) grid selects cell, hardware encoder sets value.
- Playhead: per-pattern `idx` is model-resident (`TeletypeProgram.h:46`, inside the serialized program) — readable directly, no engine accessor needed. The real check: the VM mutates `idx` during the engine tick, so grid reads/writes need the same lock or ordering the OLED pattern page uses.
- TT2Mini stores programs per scene — which scene's bank does the grid show?

**Default if deferred:** OLED row-list editor only.

**Requires User Decisions subtotal: ~19.5h (if all approved)** (D1–D7 sum to 13h as listed — 4+3+1+1+1.5+1.5+1 — plus D8 ~3h + D9 ~3.5h)

---

## EXTRA — Nice-to-Have / Higher Risk

| # | Task | Effort | MK1 Testable | Risk |
|---|------|--------|--------------|------|
| X1 | CurveTrack chaos/wavefolder param sub-mode | ~2h | ✅ | Low value — Curve already has 7 layers |
| X2 | TargetState Python bindings + MIDI capture for testing | ~2h | N/A | Infra only, no user-facing value |
| X3 | RGB color remapping for MK2/MK3/Pro/X | ~1.5h | ❌ **No** | Cannot test on MK1; code-only |
| X4 | Smart cycling on pattern follow | ~0.5h | ✅ | Low value |
| X5 | Launch Control XL / BeatStep Pro support | ~4h | ❌ **No** | Different hardware; out of scope for this task |
| X6 | Rest probability editing (Stochastic track) | — | — | **Folded into D8** — Stochastic model landed; grid scope is one owner decision, not two |
| X7 | LED double-buffering for MK2+/Pro | ~1h | ❌ **No** | Visual polish only; MK1 unsupported |
| X8 | **Global config page** (scale, root, divisor, swing, routing) | ~2–4h | ✅ | **Future research** — deferred until user scopes which params to include |

**Extra subtotal: ~13–15h** (X1–X5 + X7 = 11h; X8 scoped 2–4h)

---

## Recommended Implementation Order

```
Week 1: Pre-Flight Optimization
  Day 1:  E0.1–E0.4  isEdited(), file split, accessors, selectedLayer
  Day 2:  E0.5 LedSemantic, E0.7 _dirtyRows + compare-on-write, E0.12 mapColor dedup, E0.13 fn ptrs
  Day 3:  E0.8 VirtualGrid, E0.9 Modifier mask (E0.10 DeviceCapabilities lands later, with B2)

Week 2: Essential Track Porting
  Day 4:  E1 NoteTrack fixes (layers, highlighting, mode selector, Ikra)
  Day 5:  E2 TuesdayTrack step-key grid
  Day 6:  E3 DiscreteMapTrack 32-stage editor
  Day 7:  E4 IndexedTrack 48-step grid
  Day 8:  E6 PhaseFluxTrack 16-stage grid
  Day 9:  E5 Performer mode + integration test on MK1

Week 3: Baseline
  Day 10: B1 Circuit keyboard + B2 LP Style/Note Style settings
  Day 11: B3 Visual feedback + B4 Enhanced button events
  Day 12: B5 Interaction improvements + B6 Long-press detection
  Day 13: B7 Range fills + isEdited cache + bug fixes
  Day 14: MK1 testing / polish
  Day 15: Ship Essential + Baseline

Post-ship (user decides):
  D1–D9: Generators, macros, undo, follow, locking, prob overlay, loop mod, Stochastic grid, TT2 pattern grid
  X1–X8: Extra features as time permits
```

---

## Reference Mapping

| Feature | Baseline (mebitek) | Improvements (vinx) | Our Plan |
|---------|-------------------|---------------------|----------|
| Track types | Note, Curve, Stochastic, Logic, Arp | Same | **Note, Curve, Tuesday, DiscreteMap, Indexed, PhaseFlux** (+ Stochastic/TT2 pending D8/D9) |
| Performer mode | ✅ SequenceLength + Overview | ✅ + follow mode | **E5** |
| Circuit keyboard | ✅ | ✅ + scale-aware | **B1** |
| LP settings | ✅ Classic/Blue + Classic/Circuit | Same, Blue default | **B2 — Classic default for MK1** |
| Generators | ❌ | ✅ | **D1 — deferred, user decides** |
| Undo/redo | ❌ | ✅ TOP 7+8 | **D3 — deferred, user decides** |
| Track locking | ❌ | ✅ | **D5 — deferred, user decides** |
| Visual feedback | ✅ | ✅ + rest probability | **B3** |
| Button events | ✅ double-tap | ✅ + long-press | **B4** |
| Macro grid | ❌ | ❌ | **D2 — deferred, user decides** |
| Row-dirty LED tracking | ❌ | ❌ | **E0.7 — optimization pre-flight** |
| Semantic color levels | ❌ | ❌ | **E0.5 — optimization pre-flight** |
| VirtualGrid pagination | ❌ | ❌ | **E0.8 — optimization pre-flight** |
| Modifier overlay system | ❌ | ❌ | **E0.9 — enables D6, D7** |
| Long-press detection | ❌ | ❌ | **B6 — Baseline** |
| Probability overlay | ❌ | ❌ | **D6 — deferred, user decides** |
| Loop modifier (2-tap) | ❌ | ❌ | **D7 — deferred, user decides** |
| Range fills | ❌ | ❌ | **B7 — Baseline** |
| Global config page | ❌ | ❌ | **X8 — Extra, future research** |

---

## Dependency Graph

```
E0 Pre-flight (Optimization Infrastructure)
├── E0.1–E0.4   isEdited(), file split, accessors, selectedLayer
├── E0.5        LedSemantic color system ──> used by ALL track draw functions
├── E0.7        _dirtyRows mask + compare-on-write setLed ──> used by ALL modes
├── E0.8        VirtualGrid helper ──> E3 DiscreteMap (32), E4 Indexed (48), E6 PhaseFlux (16)
├── E0.9        Modifier bitmask ──> B4 button events, D6 prob overlay, D7 loop mod
├── E0.10       DeviceCapabilities ──> lands with B2 LP Style; also X7 double-buffer
├── E0.12       mapColor dedup ──> all device classes
└── E0.13       std::function→fn ptr ──> all device classes

E1 NoteTrack fixes
E2 TuesdayTrack step keys
E3 DiscreteMapTrack 32-stage editor ──> depends on E0.8 VirtualGrid
E4 IndexedTrack 48-step grid ──> depends on E0.8 VirtualGrid
E6 PhaseFluxTrack 16-stage grid ──> depends on E0.8 VirtualGrid
E5 Performer mode
    └── B3 Visual feedback (mute status, pattern viz)

B1 Circuit keyboard ──> B2 LP Note Style setting
B4 Enhanced button events ──> B5 Interaction improvements
B4 ──> B6 Long-press detection
B7 Range fills ──> uses E0.5 LedSemantic

generator-preview-apply task (done — ContentSnapshot A/B foundation landed)
└── D1 Generators Mode (owner decisions only)

performer-improvements task
└── D3 Undo/Redo (needs the general undo stack)

D2 Macro Grid ──> Indexed model macros exist (IndexedSequence.h:542-552); owner decisions only

D6 Probability overlay ──> depends on E0.9 Modifier system
D7 Loop modifier ──> depends on E0.9 Modifier system
D8 Stochastic grid ──> owner decision on grid scope; needs selectedStochasticSequence() (E0.3)
D9 TT2 pattern bank grid ──> owner decision on value gesture; needs runtime pattern-idx accessor for playhead
```

---

## Acceptance Criteria per Tier

### Essential Acceptance
- [ ] All 6 track types (Note, Curve, Tuesday, DiscreteMap, Indexed, PhaseFlux) have functional grid editing on Mini MK1
- [ ] PhaseFlux: all 16 stages editable across 2 pages; playhead tracks `activeCell()`
- [ ] NoteTrack all 19 layers reachable and correctly highlighted
- [ ] Performer mode: SequenceLength and Overview layers functional
- [ ] No regressions in existing Note/Curve editing
- [ ] `LedSemantic` color system used consistently across all track types
- [ ] `_dirtyRows` mask + compare-on-write `setLed()` skip idle-frame `syncLeds()` iteration
- [ ] `VirtualGrid` helper used for Indexed (48), DiscreteMap (32), and PhaseFlux (16) pagination
- [ ] `Modifier` bitmask replaces sequential `buttonState()` guard checks
- [ ] `mapColor` deduplicated to single table in base class
- [ ] `std::function` handlers converted to function pointers
- [ ] Build passes STM32 release (`make sequencer` in `build/stm32/release`)
- [ ] RAM check: `.data + .bss` stays below 120 KB; `.ccmram_bss` stays below 56 KB
- [ ] Flash check: the E0 refactors in isolation are net flat or negative; the tier as a whole (five grid editors) stays within app-partition headroom

### Baseline Acceptance
- [ ] Circuit keyboard works for Note track note entry
- [ ] LP Style / LP Note Style settings persist and affect colors
- [ ] Pattern edited indicators visible in Pattern mode with `isEdited()` cache
- [ ] Mute status visible on scene buttons
- [ ] Double-tap and long-press functional where assigned
- [ ] Range fills show min/max context (DiscreteMap thresholds, Indexed durations)
- [ ] Cued pattern pulsing animation visible

### User Decision Features
- [ ] Each feature has explicit user approval before implementation begins
- [ ] Each approved feature has its own mini-spec approved by user

### Extra
- [ ] No Extra feature blocks Essential or Baseline work
- [ ] RGB/MK2+ code is compile-gated or runtime-detected, not MK1-breaking
