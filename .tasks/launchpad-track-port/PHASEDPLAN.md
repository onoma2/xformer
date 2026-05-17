# Launchpad Mini MK1 — Phased Implementation Plan

> Created: 2026-05-17  
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

## Phase Tier Overview

| Tier | What | Effort | Cumulative |
|------|------|--------|------------|
| **Essential** | Pre-flight optimization + core fixes + all 5 track types + Performer | ~18h | ~18h |
| **Baseline** | Mebitek parity + long-press + range fills + visual polish | ~10.5h | ~28.5h |
| **Requires User Decisions** | Generators, macros, undo, follow, locking, prob overlay, loop mod | ~10.5h | ~39h |
| **Extra** | Chaos params, RGB remapping, global config, other controllers | ~13h | ~52h |

**Recommended ship target:** Essential + Baseline (~28.5h).  
**Absolute MVP:** Essential only (~18h).

---

## ESSENTIAL — Ship First

> Criteria: must work on Mini MK1, testable, no user decisions required, unblocks Baseline.

### E0. Pre-Flight (~4.5h)

| # | Task | Detail | Source |
|---|------|--------|--------|
| E0.1 | Add `isEdited()` to `DiscreteMapSequence`, `IndexedSequence`, `TuesdaySequence` | ~15 min each; safe (computed, no serialization impact) | Plan |
| E0.2 | Split `LaunchpadController.cpp` into per-track files | `LaunchpadNoteController.cpp`, `LaunchpadCurveController.cpp`, etc. Keep dispatch in main file. Prevents 974 → 2200 line explosion. | Plan |
| E0.3 | Verify all `Project.h` accessors compile | Ensure `selectedXxxSequence()` methods exist for all track types | Plan |
| E0.4 | Add `selectedLayer` to `_sequence` state struct | Required for track types without native Layer enums (Indexed, DiscreteMap, Tuesday) | Plan |
| E0.5 | **Add `LedSemantic` enum + `semanticColor()` helper** | Replace boolean `active/current` in `stepColor()` with semantic levels: Off/Background/DataPresent/Active/Playhead. Maps to MK1 red/green/amber consistently. | Kria |
| E0.6 | **Refactor `syncLeds()` to buffer-then-blast MIDI** | Accumulate all changed LED MIDI bytes into a single buffer, send one bulk transfer. Cuts USB IRQ overhead vs one message per LED. | midigrid |
| E0.7 | **Add `_dirtyRows` row-dirty mask to `LaunchpadDevice`** | 8-bit mask: skip `syncLeds()` entirely when no rows changed. Eliminates 80-iteration loop on idle frames. | libavr32 |
| E0.8 | **Extract `VirtualGrid` pagination helper** | Reusable class for >8-step tracks (Indexed 48, DiscreteMap 32). Centralizes Left/Right page math, view offset, virtual→physical col mapping. | midigrid |
| E0.9 | **Add `Modifier` bitmask enum + `_modifierMask` byte** | Shift/Loop/Prob/Time modifiers as bit flags. Replaces 7 sequential `buttonState()` checks in guards. Enables overlay modes. | Kria |
| E0.10 | **Add `DeviceCapabilities` struct to device classes** | `supportsRgbSysex`, `supportsDoubleBuffer`, `ccEdgeButtons`, `maxBrightnessLevels`. Replaces hard-coded PID checks. | midigrid |
| E0.11 | **Add shared TX scratch buffer to `LaunchpadDevice`** | Static `uint8_t _txScratch[256]` reused per sync. Reduces stack pressure. | libavr32 |
| E0.12 | **Hoist `mapColor` table to base class + deduplicate** | Single `static constexpr uint8_t colorMap[16]` instead of 4 copies in derived headers. Saves ~48B flash. | OptReview |
| E0.13 | **Replace `std::function` handlers with function pointers** | `SendMidiHandler` / `ButtonHandler` as raw fn ptr + `void *user`. Saves ~64B RAM, removes indirection. | OptReview |

**Pre-flight subtotal: ~4.5h** (was ~1h)

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

### E3. DiscreteMapTrack 32-Stage Editor (~3h)

| # | Task | Detail | MK1 Test |
|---|------|--------|----------|
| E3.1 | LayerMapItem array (6 pseudo-layers) | Threshold, Direction, Note, GateLength, SlewTime, Pluck | ✅ |
| E3.2 | 4-page navigation (Left/Right) | 32 stages → 4 pages × 8 columns | ✅ |
| E3.3 | Per-param draw functions | Bars for thresholds/gate/slew/pluck; color-coded dots for direction; note dots | ✅ |
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

**Essential subtotal: ~18h** (was ~14.5h; +3.5h optimization pre-flight)

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
- **Dependency:** Core generator A/B state machine lives in `performer-improvements` task. Cannot implement Launchpad trigger until that core is ready.

**Default if deferred:** Use OLED GeneratorPage only; no Launchpad grid shortcuts.

### D2. Macro Grid (~3h) — **BLOCKED: needs your decision**

**What the plan proposed:** Shift+Fill enters macro mode; grid shows batch operations (Init, Invert, Reverse, Randomize, etc.).

**Decision needed:**
- Which macros per track type? All 12 Curve macros + 16 DiscreteMap macros? Indexed macros don't exist in model yet (blocked by `indexed-sequence-macro-refactor`).
- Trigger: Shift+Fill? Or dedicate a function button?
- Grid layout: 4×3 for Curve, 4×4 for DiscreteMap? How to navigate categories for Indexed (if ever)?
- Apply range: current visible step range, or full loop?

**Default if deferred:** Use OLED context menus for macros; no Launchpad grid macro mode.

### D3. Undo/Redo Shortcut (~1h) — **BLOCKED: needs your decision**

**What vinx did:** `TOP 7 + TOP 8` = Undo/Redo toggle on step-edit pages.  
**What the plan proposed:** `Shift + Play` shortcut mirroring hardware `PAGE + S7`.

**Decision needed:**
- Shortcut mapping: Shift+Play? TOP 7+8? Something else?
- 1-level undo or multi-level?
- Which pages support it? All sequence pages, or only Note/Curve?
- **Dependency:** Core undo stack lives in `performer-improvements` task.

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

**Requires User Decisions subtotal: ~10.5h (if all approved)** (was ~8h)

---

## EXTRA — Nice-to-Have / Higher Risk

| # | Task | Effort | MK1 Testable | Risk |
|---|------|--------|--------------|------|
| X1 | CurveTrack chaos/wavefolder param sub-mode | ~2h | ✅ | Low value — Curve already has 7 layers |
| X2 | TargetState Python bindings + MIDI capture for testing | ~2h | N/A | Infra only, no user-facing value |
| X3 | RGB color remapping for MK2/MK3/Pro/X | ~1.5h | ❌ **No** | Cannot test on MK1; code-only |
| X4 | Smart cycling on pattern follow | ~0.5h | ✅ | Low value |
| X5 | Launch Control XL / BeatStep Pro support | ~4h | ❌ **No** | Different hardware; out of scope for this task |
| X6 | Rest probability editing (Stochastic track) | ~1h | ✅ | **Blocked** — requires Stochastic track model to land first (`feat/stochastic` branch, in progress) |
| X7 | LED double-buffering for MK2+/Pro | ~1h | ❌ **No** | Visual polish only; MK1 unsupported |
| X8 | **Global config page** (scale, root, divisor, swing, routing) | ~2–4h | ✅ | **Future research** — deferred until user scopes which params to include |

**Extra subtotal: ~13h** (was ~11h)

---

## Recommended Implementation Order

```
Week 1: Pre-Flight Optimization
  Day 1:  E0.1–E0.4  isEdited(), file split, accessors, selectedLayer
  Day 2:  E0.11–E0.13 TX scratch buffer → then E0.6 buffer-then-blast syncLeds, E0.7 _dirtyRows mask
  Day 3:  E0.5 LedSemantic, E0.8 VirtualGrid, E0.9 Modifier mask, E0.10 DeviceCapabilities, E0.12 mapColor dedup

Week 2: Essential Track Porting
  Day 4:  E1 NoteTrack fixes (layers, highlighting, mode selector, Ikra)
  Day 5:  E2 TuesdayTrack step-key grid
  Day 6:  E3 DiscreteMapTrack 32-stage editor
  Day 7:  E4 IndexedTrack 48-step grid
  Day 8:  E5 Performer mode + integration test on MK1

Week 3: Baseline
  Day 9:  B1 Circuit keyboard + B2 LP Style/Note Style settings
  Day 10: B3 Visual feedback + B4 Enhanced button events
  Day 11: B5 Interaction improvements + B6 Long-press detection
  Day 12: B7 Range fills + isEdited cache + bug fixes
  Day 13: MK1 testing / polish
  Day 14: Ship Essential + Baseline

Post-ship (user decides):
  D1–D7: Generators, macros, undo, follow, locking, prob overlay, loop mod
  X1–X7: Extra features as time permits
```

---

## Reference Mapping

| Feature | Baseline (mebitek) | Improvements (vinx) | Our Plan |
|---------|-------------------|---------------------|----------|
| Track types | Note, Curve, Stochastic, Logic, Arp | Same | **Note, Curve, Tuesday, DiscreteMap, Indexed** |
| Performer mode | ✅ SequenceLength + Overview | ✅ + follow mode | **E5** |
| Circuit keyboard | ✅ | ✅ + scale-aware | **B1** |
| LP settings | ✅ Classic/Blue + Classic/Circuit | Same, Blue default | **B2 — Classic default for MK1** |
| Generators | ❌ | ✅ | **D1 — deferred, user decides** |
| Undo/redo | ❌ | ✅ TOP 7+8 | **D3 — deferred, user decides** |
| Track locking | ❌ | ✅ | **D5 — deferred, user decides** |
| Visual feedback | ✅ | ✅ + rest probability | **B3** |
| Button events | ✅ double-tap | ✅ + long-press | **B4** |
| Macro grid | ❌ | ❌ | **D2 — deferred, user decides** |
| Buffer-then-blast MIDI | ❌ | ❌ | **E0.6 — optimization pre-flight** |
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
├── E0.11       TX scratch buffer ──> E0.6 syncLeds
├── E0.6        buffer-then-blast syncLeds ──> depends on E0.11; used by ALL modes
├── E0.7        _dirtyRows mask ──> used by ALL modes
├── E0.8        VirtualGrid helper ──> E3 DiscreteMap (32), E4 Indexed (48)
├── E0.9        Modifier bitmask ──> B4 button events, D6 prob overlay, D7 loop mod
├── E0.10       DeviceCapabilities ──> B2 LP Style, X7 double-buffer
├── E0.12       mapColor dedup ──> all device classes
└── E0.13       std::function→fn ptr ──> all device classes

E1 NoteTrack fixes
E2 TuesdayTrack step keys
E3 DiscreteMapTrack 32-stage editor ──> depends on E0.8 VirtualGrid
E4 IndexedTrack 48-step grid ──> depends on E0.8 VirtualGrid
E5 Performer mode
    └── B3 Visual feedback (mute status, pattern viz)

B1 Circuit keyboard ──> B2 LP Note Style setting
B4 Enhanced button events ──> B5 Interaction improvements
B4 ──> B6 Long-press detection
B7 Range fills ──> uses E0.5 LedSemantic

performer-improvements task
├── D1 Generators Mode (needs core A/B state machine)
└── D3 Undo/Redo (needs core undo stack)

indexed-sequence-macro-refactor task
└── D2 Macro Grid for IndexedTrack (needs model macros)

D6 Probability overlay ──> depends on E0.9 Modifier system
D7 Loop modifier ──> depends on E0.9 Modifier system
```

---

## Acceptance Criteria per Tier

### Essential Acceptance
- [ ] All 5 track types (Note, Curve, Tuesday, DiscreteMap, Indexed) have functional grid editing on Mini MK1
- [ ] NoteTrack all 19 layers reachable and correctly highlighted
- [ ] Performer mode: SequenceLength and Overview layers functional
- [ ] No regressions in existing Note/Curve editing
- [ ] `LedSemantic` color system used consistently across all track types
- [ ] `syncLeds()` uses buffer-then-blast (single USB transfer per frame)
- [ ] `_dirtyRows` mask eliminates idle-frame `syncLeds()` iteration
- [ ] `VirtualGrid` helper used for Indexed (48) and DiscreteMap (32) pagination
- [ ] `Modifier` bitmask replaces sequential `buttonState()` guard checks
- [ ] `mapColor` deduplicated to single table in base class
- [ ] `std::function` handlers converted to function pointers
- [ ] Build passes STM32 release (`make sequencer` in `build/stm32/release`)
- [ ] RAM check: `.data + .bss` stays below 120 KB; `.ccmram_bss` stays below 56 KB
- [ ] Flash check: net flash change flat or negative after optimizations

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
