# Performer Improvements — Non-Launchpad Fork Integrations

## Overview

This task documents all non-Launchpad-related improvements from the VinxScorza, Modulove, and Mebitek performer forks that should be integrated into the XFORMER project, reorganized against current XFORMER codebase with feasibility analysis and design decisions.

Launchpad-specific improvements (LP Style, Circuit Keyboard, Performer Mode, etc.) live in `launchpad-track-port`.

## Current XFORMER Codebase Analysis

### Track Types (7 types, 8 tracks total)
| Track | XFORMER Status | In Vinx? | In Modulove? | In Mebitek? |
|-------|---------------|----------|-------------|------------|
| Note Circus | Current | Yes | Yes | Yes |
| Curve Studio | Current | Yes | Yes | Yes |
| MIDI/CV | Current | Yes | Yes | Yes |
| Algo(Tuesday) | XFORMER-only | No | No | No |
| Discrete | XFORMER-only | No | No | No |
| Indexed | XFORMER-only | No | No | No |
| T9type (Teletype) | XFORMER-only | No | No | No |

### Key Architecture Differences
- **8 tracks** (CONFIG_TRACK_COUNT=8) vs Modulove's 16-track dual bank
- **No built-in LFO modulators** — no ModulatorEngine exists in codebase
- **No microtiming recording** — no microtiming fields in NoteSequence::Step (NO-GO: kept for reference)
- **Unique track types** — Tuesday, Discrete, Indexed, Teletype have no counterpart in other forks
- **No Logic/Stochastic/Arp tracks** — XFORMER does not have these track types
- **Unique Harmony system** — NoteSequence has HarmonyRole, InversionOverride, VoicingOverride not found in other forks

---

## VinxScorza Improvements

### 1. Core Sequencing Stabilization
**Source:** `temp-ref/vinx-performer/src/apps/sequencer/engine/`

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| Step engine stabilization (all tracks) | High | Extend existing NoteTrackEngine, CurveTrackEngine |
| Crash path fixes | High | Add to all 7 track types |
| Non-zero subrange shifting | High | Enhance existing step selection in NoteSequence |
| Curve Gate Offset/Gate Length | Already present | No action needed |
| Curve undo restoration | High | Add undo state to CurveTrackEngine |
| Scale/MIDI capture consistency | High | Improve existing StepRecorder |
| Chaos → Vandalize + Wreck with A/B safe | Medium | Requires generator page refactor |
| Random gen preview/apply | Medium | Extend RandomizePage with A/B preview |

**Files to modify:**
- `src/apps/sequencer/engine/NoteTrackEngine.h`
- `src/apps/sequencer/engine/CurveTrackEngine.h`
- `src/apps/sequencer/engine/StepRecorder.h`
- `src/apps/sequencer/ui/page/RandomizePage.h`
- `src/apps/sequencer/ui/page/GeneratorPage.h`

### 2. Generator Preview/Apply Workflow
**Source:** `temp-ref/vinx-performer/src/apps/sequencer/ui/page/GeneratorPage.h`

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| Preview/apply flow for all generators | Medium | Add A/B preview state to GeneratorPage base class |
| ORIGINAL state entry with explicit first reroll | Medium | Add state machine to generator workflow |
| Safer commit/cancel flow | Medium | Extend context menu commit logic |
| Uniform footer/context layouts | Low | Cosmetic — lower priority |

**Relation to `launchpad-track-port`:** The core A/B preview state machine and GeneratorPage refactor live here. The Launchpad grid trigger for generators (Generators Mode) lives in `launchpad-track-port`.

**Files to modify:**
- `src/apps/sequencer/ui/page/GeneratorPage.h`
- `src/apps/sequencer/ui/page/GeneratorPage.cpp`

### 3. UI & Display Improvements
**Source:** `temp-ref/vinx-performer/src/apps/sequencer/ui/`

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| 64-step context + 16-step bank visualization | Medium | Enhance step multiplex rendering in SequencerPage |
| Layer graphics 30fps skip unchanged frames | Medium | Add dirty-rect tracking to Canvas |
| Menu wrap | High | Simple modulo in UI navigation code |
| Screensaver/wake refinement | High | Extend existing screensaver code |

**Files to modify:**
- `src/apps/sequencer/ui/page/SequencerPage.h`
- `src/apps/sequencer/ui/page/SequencerPage.cpp`
- `src/apps/sequencer/ui/Canvas.h`

### 4. System & Defaults
**Source:** `temp-ref/vinx-performer/src/apps/sequencer/model/UserSettings.h`

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| Harden system save flow | High | Add checksum + atomic write to FileManager |
| Chaos defaults persistence | High | Add chaos defaults to UserSettings |
| Save prompts for unsaved changes | High | Add dirty-flag check on project switch |
| Memory optimization | Medium | Already have AGENTS.md analysis on this |

---

## Modulove Improvements

### 1. 8 LFO Modulators
**Source:** `temp-ref/modulove-performer/src/apps/sequencer/model/Modulator.h`

**Status:** Extracted to dedicated task `.tasks/global-modulators-v1/`. Keep this
section as reference only; implementation planning lives in the new task.

| Requirement | Feasibility | Effort |
|-------------|------------|--------|
| New Modulator data model | High | 1 file |
| New ModulatorEngine | Medium | shares engine tick |
| Waveform preview (oscilloscope) | Medium | needs Canvas rendering |
| Quick-map popup for routing | Medium | extends routing UI |
| Triplet/dotted note divisions | High | simple divisor addition |

**Files to create:**
- `src/apps/sequencer/model/Modulator.h`
- `src/apps/sequencer/engine/ModulatorEngine.h`
- `src/apps/sequencer/ui/page/ModulatorPage.h`

### 2. Microtiming Recording — 🔴 NO-GO (reference only)
**Source:** `temp-ref/modulove-performer/src/apps/sequencer/model/NoteSequence.h`

| Requirement | Feasibility | Effort |
|-------------|------------|--------|
| 7-bit gate offset (-63 to +63) | High | Extend NoteSequence::Step bitfield |
| Capture timing during live record | High | Extend StepRecorder |
| Timing quantize (0-100%) | High | Add quantize logic in engine |
| Bidirectional timing (early/late) | High | Simple signed arithmetic |

**Files to modify (kept for reference):**
- `src/apps/sequencer/model/NoteSequence.h` — add offset field (7 bits)
- `src/apps/sequencer/engine/StepRecorder.h` — capture timing
- `src/apps/sequencer/engine/NoteTrackEngine.h` — apply offset during playback

**Decision:** Not implementing. Changes bitfield layout and serialization format with insufficient payoff for XFORMER's use case.

### 3. Enhanced Performer Page
**Source:** `temp-ref/modulove-performer/src/apps/sequencer/ui/page/PerformerPage.h`

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| Green/Red/Yellow LED coding | High | Extend LedPainter |
| Dimmed pattern numbers for muted | High | Add alpha/grey in canvas |
| All-tracks view for 8 tracks | High | Already have 8, just enhance |
| Consistent LED across banks | N/A | No bank system in XFORMER |

**Relation to `launchpad-track-port`:** This is about on-device OLED/LED rendering (PerformerPage). The Launchpad-based Performer Mode (scene mute/solo/fill via Launchpad grid) lives in `launchpad-track-port`. They are independent.

---

## Mebitek Improvements

### 1. Track Features
**Source:** `temp-ref/mebitek-performer/src/apps/sequencer/`

| Improvement | Feasibility | Notes |
|-------------|------------|-------|
| Arpeggiator Track | Low | XFORMER has Tuesday instead — no Arp track type |
| Stochastic Track enhancements | N/A | XFORMER has no Stochastic track |
| Logic Track per-step operators | N/A | XFORMER has no Logic track |
| Curve CV controllable min/max | Already present | No action needed |

### 2. Performance Features

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| Quick octave change (Step+F1-F5) | High | Add page-level shortcut handler |
| Quick gate accent on Launchpad | High | Lives in `launchpad-track-port` (LP work) |
| Steps to stop feature | High | Add to Project properties + ClockEngine |

**Files to modify:**
- `src/apps/sequencer/model/Project.h` — add stepsToStop field
- `src/apps/sequencer/engine/ClockEngine.h` — check stepsToStop

### 3. UI Shortcuts

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| Submenu shortcuts (double-click F1-F5) | High | Add double-click detection to key handler |
| Context menu via double-click page | High | Add timer-based key event detection |

### 4. Technical Improvements

| Improvement | Feasibility | Design Decision |
|-------------|------------|----------------|
| Undo function | Partially present | Enhance existing undo system |
| Curve backward playback — 🔴 NO-GO | High | Add run mode to CurveTrackEngine (reference only) |
| Bypass voltage table per step | Already present | NoteSequence already has this |
| Prevent short clock pulses | High | Add min pulse width to ClockEngine |

**Relation to `launchpad-track-port`:** The core undo/redo state management lives here. The Launchpad Shift+Play undo shortcut lives in `launchpad-track-port`.

---

## Implementation Plan (Prioritized)

### Phase 1: High Priority
1. **Quick octave change** (Step+F1-F5) — **DONE** — Page shortcut handler
2. **Submenu shortcuts** (double-click F1-F5) — **DONE** — Double-click Page opens context menu with 2s auto-close
3. **Context menu via double-click page** — **DONE** (merged with #2)
4. **Menu wrap** — **DONE** (`10efe3c4`)
5. **Enhanced Performer Page** — **DONE** — Mute LEDs wired from PlayState, track labels dimmed when muted, pattern numbers shown instead of T1-T8
6. **Prevent short clock pulses** — **DONE** — 1ms floor via `tickPeriodUs()` minimum in `Clock.cpp`
7. **Generator preview/apply + step selection + 64-step context** — Unified task: (a) SequenceBuilder 3-copy state machine (original/edit/preview) + commit/cancel flow, (b) GeneratorPage A/B workflow + StepSelection + section navigation + bank separators, (c) RandomGenerator wired through as first consumer. Subsumes former "Random generator preview/apply" (Phase 1 #7), "Generator preview/apply workflow" (Phase 2 #1), and "64-step context visualization" (Phase 3 item).
8. **Curve undo restoration** — CurveTrackEngine undo state

### Phase 2: Medium Priority
1. **Screensaver/wake refinement** — Existing screensaver improvements

### Phase 3: Low Priority
1. **Chaos generators (Vandalize + Wreck)** — Second consumer of generator preview/apply infrastructure. Chaos (Note-only, 14-target blend) and Entropy (template-generic across NoteSequence/CurveSequence).
2. **LFO modulators** — **DONE** (extracted to `global-modulators-v1`)

### Phase 4: Future
1. **Arpeggiator track** — Evaluate vs Tuesday approach

### Items Not in Phases (System & Defaults)
- Harden system save flow (checksum + atomic write)
- Chaos defaults persistence
- Save prompts for unsaved changes
- Memory optimization
- Step engine stabilization (all tracks)
- Crash path fixes
- Non-zero subrange shifting
- Scale/MIDI capture consistency
- 30fps dirty-rect tracking

## Success Criteria

- **Phase 1**: Performer page enhanced, shortcuts functional
- **Phase 2**: Undo restored, generators have preview/apply, UI responsiveness improved
- **Phase 3**: Advanced generators working, LFO modulators operational (if done)
- **Phase 4**: Research outputs only — no commitment

## Phase 1 — Implementation Plans

### Quick octave change (Step+F1-F5)

**Files:**
- `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` — add in `keyPress()`: hold step + F1-F5 sets note to octave 1-5
- `src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp` — optional same pattern
- `src/apps/sequencer/ui/pages/IndexedSequenceEditPage.cpp` — optional same pattern

**Change:** In `NoteSequenceEditPage::keyPress()`, after existing handling: if `key.isFunction()` and any step is held, compute `octave = key.function() + 1`, then for each held step: `sequence.step(stepIndex).setNote(scale.notesPerOctave() * octave)`.

**Reference files:**
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` — full impl in `keyPress()`
- `src/apps/sequencer/model/NoteSequence.h` — `Step::setNote()`
- `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp:446` — existing `keyPress()` addition point

---

### Submenu shortcuts (double-click F1-F5)

**Files:**
- `src/apps/sequencer/ui/pages/ListPage.cpp` — base list key handler for double-click
- `src/apps/sequencer/ui/pages/PatternPage.cpp` — routing on double-click
- `src/apps/sequencer/ui/pages/ProjectPage.cpp` — routing on double-click
- `src/apps/sequencer/ui/pages/TrackPage.cpp` — routing on double-click

**Change:** Add timer-based double-click detection in page `keyDown()`. F1-F5 double-click within ~300ms navigates directly to sub-page instead of single-click action.

**Reference files:**
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/PatternPage.cpp` — has double-click routing
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/ListPage.cpp` — base list rework
- `src/apps/sequencer/ui/pages/ListPage.cpp` — current baseline

---

### Context menu via double-click page

**Files:**
- `src/apps/sequencer/ui/pages/ContextMenuPage.cpp` — context menu page
- `src/apps/sequencer/ui/PageManager.cpp` — timer-based key event detection

**Change:** Detect double-click anywhere to dismiss or trigger context menu. Uses same timer infra as submenu shortcuts.

**Reference files:**
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/ContextMenuPage.cpp` — double-click dismiss
- `src/apps/sequencer/ui/PageManager.h` — event dispatch

---

### Prevent short clock pulses

**File:** `src/apps/sequencer/engine/Clock.cpp:429`

**Change:** Raise minimum clock pulse from 1 tick to a real-time-based floor (~1ms). Replace `std::max(uint32_t(1), ...)` with time-based minimum guaranteeing ≥1000µs pulse width regardless of BPM.

**Reference files:**
- `temp-ref/mebitek-performer/src/apps/sequencer/engine/Clock.cpp:422` — tick-based fix `uint32_t(10)` (reverted — too aggressive at low BPM)
- `temp-ref/modulove-performer/src/apps/sequencer/engine/Clock.cpp` — `_lastTickUs` + `tickPeriodUs()` for microsecond timing

---

### Generator preview/apply + step selection + 64-step context

Unified task combining former "Random generator preview/apply" (Phase 1 #7), "Generator preview/apply workflow" (Phase 2 #1), and "64-step context visualization" (Phase 3).

#### Vinx reference architecture

The Vinx fork has a complete 3-copy state machine with A/B preview, step selection, bank navigation, and chaos/entropy generators. Key files:

- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/SequenceBuilder.h` — Base class with `revert()`, `apply()`, `showOriginal()`, `showPreview()`, `showingPreview()` virtuals. `SequenceBuilderImpl<T>` has `_edit` (ref), `_original` (copy), `_preview` (copy), `_showingPreview` flag. `apply()` copies preview → edit → original. `showOriginal()` swaps edit ← original. `showPreview()` swaps edit ← preview. Also: `clearSteps(selected)` with selection mask, `clearLayer(selected)` with selection mask, `applyEntropy(seed, amount, selected, targetMask)` for entropy system. `AcidSequenceBuilder` — phrase/layer mode with `ApplyMode` enum, `displayValue()`, target step collection. `ChaosSequenceBuilder` — multi-track chaos with Scope (Sequence/Pattern), per-track backup vectors.
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/Generator.h` — `Mode` enum: `InitLayer, InitSteps, Euclidean, Random, Acid, Chaos, ChaosEntropy`. Delegate methods: `revert()`, `apply()`, `showOriginal()`, `showPreview()`, `showingPreview()`, `randomizeParams()`.
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/RandomGenerator.h` — `Param` enum: Seed, Smooth, Bias, Scale, **Variation**. `randomizeParams()`, `randomizeSeed()`, `randomizeContextParams()`. `displayValue(int index)` for graph drawing. 32-bit seed (vs XFORMER's 16-bit).
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/ChaosGenerator.h` — 14-target enum `Target`, `blendRandomValue()`, `blendRandomBool()`, `TargetScope` (Sequence/Pattern), per-target mask bitfield, seed parameter.
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/ChaosEntropyGenerator.h` — Thin wrapper delegating to `SequenceBuilder::applyEntropy()`. 14-target enum mapping to `EntropyTargets`.
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/EntropyTargets.h` — 14-target enum `EntropyTarget` with `DefaultEntropyTargetMask`. Template `entropy_detail::applyTarget<T>()` maps targets to sequence-specific layers. Specializations for `CurveSequence`, `StochasticSequence`, `LogicSequence`, `ArpSequence`. `entropy_detail::blendValue()` lerps original → random with amount%.
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/GeneratorPage.h` — `StepSelection<CONFIG_STEP_COUNT>* _stepSelection`, `_section = 0`, `_previewArmed`, `_applied`, `_boundTrackIndex`, `_boundTrackMode`. Methods: `togglePreview()`, `invalidateChaosPreview()`, `triggerChaosPreview()`, `launchpadRandomize()`, `showPreviewStateMessage()`, `boundTrackContextValid()`, `ensureBoundTrackContext()`.
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/GeneratorPage.cpp` (2093 lines) — Full A/B workflow: `enter()` starts in ORIGINAL state, `keyPress()` on step+Shift triggers reroll, F0 toggles A/B via `togglePreview()`, context menu has NEW RAND / SMOOTH / RESEED / CANCEL / APPLY. `commit()` calls `apply()`, `revert()` calls `revert()`. Bank separators drawn in `drawRandomGenerator()` via `drawBankSeparators()` and `drawBankFrame()`. Step highlight uses `_stepSelection` for LED and draw. `boundTrackContextValid()` guard cancels if track switches.

#### XFORMER current state

- `SequenceBuilder.h` — 2-copy model (`_edit` ref + `_original` copy). No `_preview`, no `apply()`, no `showOriginal()`/`showPreview()`, no `showingPreview()`. `clearSteps()` without selection mask. No `applyEntropy()`. Only `NoteSequenceBuilder` and `CurveSequenceBuilder`.
- `Generator.h` — `Mode` enum: `InitLayer, Euclidean, Random, Last`. No `apply()`, `showOriginal()`, `showPreview()`, `showingPreview()`, `randomizeParams()`.
- `RandomGenerator.h` — `Param` enum: Seed, Smooth, Bias, Scale. No Variation param, no `randomizeParams()`, no `randomizeSeed()`, no `randomizeContextParams()`, no `displayValue()`. 16-bit seed.
- `GeneratorPage.h` — No `_stepSelection`, no `_section`, no `_previewArmed`, no `_applied`. No `togglePreview()`. Simple `enter()`/`exit()` with no state machine. Just `_valueRange` and `_generator`.
- `GeneratorPage.cpp` (270 lines) — No A/B workflow. `commit()` just calls `close()`. `revert()` calls `_generator->revert()` then `close()`. No bank separators, no step selection, no section navigation.

#### XFORMER sequence types for entropy (not in Vinx)

- `NoteSequence` — has full `layerValue/setLayerValue/layerRange` API with 17+ layers. XFORMER-unique layers: AccumulatorTrigger, PulseCount, GateMode, HarmonyRoleOverride, InversionOverride, VoicingOverride.
- `CurveSequence` — 7 layers: Shape, ShapeVariation, Min, Max, Gate, GateOffset, GateProbability (GateLength in XFORMER maps to EventLength).
- `IndexedSequence` — No Layer enum. Needs custom `applyTarget<IndexedSequence>` for duration, swing, repeats, condition.
- `TuesdaySequence` — No Layer enum. Uses `Routable<T>`. Significant abstraction needed.
- `DiscreteMapSequence` — Stage-based, not step-based. Custom handler needed.

#### Implementation plan (phased)

**Phase A: SequenceBuilder 3-copy state machine** (model layer)

1. Add `_preview` copy and `_showingPreview` bool to `SequenceBuilderImpl<T>`
2. Add virtual methods to `SequenceBuilder` base: `apply()`, `showOriginal()`, `showPreview()`, `showingPreview()`
3. Update `clearSteps()` and `clearLayer()` to accept `std::bitset<CONFIG_STEP_COUNT>` selection mask (XFORMER doesn't use selected-step clearing yet, but Vinx does)
4. Add `applyEntropy()` virtual with default no-op for future entropy consumer
5. Implementation: `apply()` = `_edit = _preview; _original = _preview; _showingPreview = true;`, `showOriginal()` = `_edit = _original; _showingPreview = false;`, `showPreview()` = `_edit = _preview; _showingPreview = true;`, `revert()` = `_edit = _original; _preview = _original; _showingPreview = false;`

**Phase B: Generator base class** (engine layer)

1. Add `Mode::InitSteps` (clear all steps, already hinted in Vinx)
2. Add delegate methods: `apply()`, `showOriginal()`, `showPreview()`, `showingPreview()`, `randomizeParams()` (with default no-op)
3. Add `RandomGenerator::Param::Variation` (0-100 blend amount)
4. Add `RandomGenerator::randomizeParams()` (re-randomize seed + context params)
5. Add `RandomGenerator::randomizeSeed()` (re-roll seed only)
6. Add `RandomGenerator::displayValue(int index)` for graph drawing
7. Widen seed to 32-bit (Vinx uses uint32_t)

**Phase C: GeneratorPage A/B workflow + step selection** (UI layer)

1. Add `_section` member with `stepOffset()` (already in Vinx: `_section * StepCount`)
2. Add `_previewArmed` and `_applied` state flags
3. Add `_stepSelection` pointer (already exists in edit pages, port to GeneratorPage)
4. Add `togglePreview()`, `commit()`, `revert()` state machine:
   - Enter: `_generator->revert(); _generator->showOriginal();` (start in ORIGINAL)
   - Encoder re-roll: `_generator->showPreview()` (show generated content)
   - F0 or `togglePreview()`: flip between A/B states
   - APPLY context: `_generator->apply(); close();`
   - CANCEL context: `_generator->revert(); close();`
   - Exit without apply: `_generator->revert();`
5. Add `boundTrackContextValid()` / `ensureBoundTrackContext()` guard (cancel if track switches)
6. Add section navigation: Left/Right buttons cycle `_section` 0-3, `stepOffset() = _section * 16`

**Phase D: 64-step visualization** (UI layer)

1. Add `drawBankSeparators()` — vertical lines at 16-step boundaries, brighter for current bank
2. Add `drawBankFrame()` — horizontal lines framing current bank section
3. Port `stepInCurrentBank()` check for dimming non-active banks
4. Port `drawRandomGenerator()` graph with bank-aware rendering (current bank bright, other banks dim)
5. Step selection LED feedback: `LedPainter::drawSelectedSequenceSection(leds, _section)` for bank indicator

**Phase E: Context menu expansion**

1. Replace current 3-item menu (INIT / RANDOM / CLOSE) with Vinx-style 5-item menu: NEW RAND / SMOOTH X / RESETGEN / CANCEL / APPLY
2. Double-click context menu already wired (from Phase 1 submenu shortcuts)
3. Add `randomizeSeed()` context action for re-rolling seed without changing other params

**Porting strategy:** Port Vinx code as directly as possible. The Vinx `SequenceBuilderImpl<T>` template is almost identical to XFORMER's — the delta is `_preview` copy + virtual methods. `Generator` base class just needs the delegate additions. `RandomGenerator` needs Variation param + `randomizeParams()` + `displayValue()`. `EntropyTargets.h` and `entropy_detail::applyTarget<T>()` templates port directly with XFORMER-specific layer mappings. `ChaosGenerator` and `ChaosEntropyGenerator` are Phase 3 consumers.

**Reference files:**
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/SequenceBuilder.h` (full — 3-copy state machine, AcidSequenceBuilder, ChaosSequenceBuilder)
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/Generator.h` (delegates + mode enum)
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/RandomGenerator.h` (Variation param, randomizeParams, displayValue)
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/EntropyTargets.h` (target enum + blendValue)
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/ChaosGenerator.h` (14-target chaos — Phase 3 consumer)
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/ChaosEntropyGenerator.h` (thin entropy wrapper — Phase 3 consumer)
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/GeneratorPage.h` (state machine + step selection)
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/GeneratorPage.cpp` (full workflow 2093 lines)

---

### Curve undo restoration

**Files:**
- `src/apps/sequencer/engine/CurveTrackEngine.h` — add `UndoState` struct + `saveUndoState()`/`restoreUndoState()` methods
- `src/apps/sequencer/engine/CurveTrackEngine.cpp` — implement save/restore of 18 runtime signal state fields
- `src/apps/sequencer/engine/Engine.cpp` or `model/PlayState.cpp` — hook into snapshot lifecycle

**Change:** Current snapshot saves pattern data only. When reverted, DJ filter, feedback, chaos oscillators, gate slope state machine, and CV interpolation retain stale values → output glitches. Save runtime state (lpfState, feedbackState, prevCvOutput, wasRising/falling, gateTimer, chaosValue, chaosPhase, latoocarfian, lorenz, freePhase, etc.) before pattern switch, restore after pattern revert.

**Reference files:** None — original implementation. All forks (Vinx, Mebitek, Modulove) share same base without undo.

---

## Enhanced Performer Page — Implementation Plan

### Item 1: Wire mute LED display

**File:** `src/apps/sequencer/ui/pages/PerformerPage.cpp` — `updateLeds()`

**Change:** Replace `LedPainter::drawMutes(leds, 0, 0)` with computed mute bitmasks from `playState.trackState()`. Step keys 1-8 show green for actively muted, red for pending mute changes (flashing). `drawMutes()` already handles LED mapping and color semantics.

**Reference files:**
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/PerformerPage.cpp` — baseline (same `drawMutes(leds, 0, 0)` bug)
- `src/apps/sequencer/ui/LedPainter.cpp` — `drawMutes()` impl
- `src/apps/sequencer/model/PlayState.h` — `TrackState::mute()`, `hasMuteRequest()`, `requestedMute()`

### Item 2: Pattern numbers with mute dimming

**File:** `src/apps/sequencer/ui/pages/PerformerPage.cpp` — `draw()`

**Change:** After fill bar rendering, add pattern number centered in each track column. Show as `P1-P16` (internal 0-indexed, display +1). Muted tracks render in `Color::Low` (dim), active in `Color::Medium`. Layout: y=52, below fill bar, within 64px screen.

**Reference files:**
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/PatternPage.cpp:119` — pattern display convention (`pattern() + 1`)
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp:330` — same `P%d` convention
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/painters/WindowPainter.cpp:94` — header pattern display (`playPattern + 1`)
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/PerformerPage.cpp` — baseline for draw layout

---

## Phase 2+ — Reference Files

### Screensaver/wake refinement (Phase 2)

**Reference files:**
- `temp-ref/vinx-performer/src/apps/sequencer/ui/Screensaver.cpp` — `_canvas.screensaver()` active (ours commented out), `_wakeKey` tracking
- `temp-ref/vinx-performer/src/apps/sequencer/ui/Screensaver.h:35-36` — `_wakeKey` member
- `temp-ref/vinx-performer/src/apps/sequencer/ui/Ui.cpp` — `screensaver.on()` no gate arg, has `screensaver.off()`
- `temp-ref/vinx-performer/src/apps/sequencer/model/UserSettings.h` — `DimSequenceSetting` as uint8_t (3-state: off/dim/dim+)
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/Screensaver.cpp` — also has active `_canvas.screensaver()`

### Chaos generators (Vandalize + Wreck) (Phase 3)

**User-facing behavior:**
- **14 targets** on NoteSequence steps: Gate, GateOffset, GateProbability, Retrigger, Length, LengthVariationRange, LengthVariationProbability, RetriggerProbability, Note, Slide, NoteVariationRange, NoteVariationProbability, BypassScale, Condition
- **2 params**: Seed (32-bit hex), Amount (0-100% blend)
- **2 scopes**: Sequence (single track), Pattern (all note tracks simultaneously)
- **Amount** = blend % between original and fully random. 50% = half-randomized. Deterministic — same seed + amount = same result
- **UI**: 16-cell target grid on GeneratorPage. Per-cell enable/disable toggle + All On/All Off
- "Vandalize" = per-step target-scoped randomization on Note sequences (ChaosGenerator)

**Internal architecture:**
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/ChaosGenerator.h/.cpp` — 14-target enum, `blendRandomValue()` lerps original→random, `blendRandomBool()` probability-based flip
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/SequenceBuilder.h:623-842` — `ChaosSequenceBuilder` tracks N note tracks, backs up original steps, applies chaos via `_params.seed` + per-track deterministic RNG
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/GeneratorPage.cpp` — `drawChaosGenerator()` renders 16-cell target grid

### ChaosEntropy generator — Wreck (Phase 3)

**User-facing behavior:**
- **14 targets** generalized across sequence types: Gate, GateOffset, GateProbability, Retrigger, RetriggerProbability, EventLength, EventLengthVariationRange, EventLengthVariationProbability, PrimaryValue, PrimaryValueVariationRange, PrimaryValueVariationProbability, Register, Motion, LogicRepeatRules
- Targets have type-specific meanings (e.g. Gate→Curve gate probability, Register→Logic register, Motion→Stochastic motion)
- Same seed/amount UI as Chaos, but operates on **Note, Curve, Logic, Stochastic, Arp** sequences via template generics
- "Wreck" = cross-type entropy applied via `SequenceBuilder::applyEntropy()` (template method)

**Internal architecture:**
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/EntropyTargets.h` — 14-target enum + `DefaultEntropyTargetMask`
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/ChaosEntropyGenerator.h/.cpp` — thin wrapper, delegates to `SequenceBuilder::applyEntropy()`
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/SequenceBuilder.h:369-390` — `applyEntropy()` with `entropy_detail::applyTarget<T>()` template per layer

**Key distinction:** Chaos = NoteSequence-specific with standalone ChaosSequenceBuilder. Entropy = template-generic across all sequence types. TASK.md decision (Chaos & Entropy section): adopt Entropy approach.

**XFORMER applicability — sequence types compatible with template-generic Entropy:**
- `NoteSequence` ✅ — 17 layers including XFORMER-unique: AccumulatorTrigger, PulseCount, GateMode, HarmonyRoleOverride, InversionOverride, VoicingOverride. Full `layerValue/setLayerValue/layerRange` API.
- `CurveSequence` ✅ — 7 layers: Shape, ShapeVariation, Min, Max, Gate, GateOffset, GateProbability. Full layer API.

**Sequence types requiring custom entropy handling:**
- `IndexedSequence` ⚠️ — Step data exists but no Layer enum. Custom `applyTarget<IndexedSequence>` needed (duration, swing, repeats, condition).
- `TuesdaySequence` ⚠️ — No Layer enum, uses `Routable<T>`. Significant new layer abstraction needed.
- `DiscreteMapSequence` ⚠️ — Stage-based, not step-based. Custom entropy handler needed.
- `MidiCvTrack` ❌ — No sequencer steps. Not applicable.
- `TeletypeTrack` ❌ — Script-based. Not applicable.

**Implementation plan:**
1. Add `applyTarget<NoteSequence>` — maps 14 EntropyTargets to NoteSequence layers (including XFORMER-only layers). Straightforward, same pattern as Vinx.
2. Add `applyTarget<CurveSequence>` — maps to Curve layer enums. Straightforward.
3. Defer Indexed/Tuesday/Discrete entropy until SequenceBuilderImpl exists for those types.

### LFO modulators (Phase 3)

**User-facing behavior:**
- **8 modulators** (`CONFIG_MODULATOR_COUNT`), each assignable to MIDI CC or CV output
- **7 waveform shapes**: Sine (parabolic), Triangle, Saw Up, Saw Down, Square, Random (slew-smoothed), ADSR
- **3 LFO modes**: Free (free-running), Sync (phase reset on gate), Retrigger (soft restart from phase offset)
- **Random sub-modes**: Clocked (new value per cycle), Triggered (new value on gate rising edge)
- **Rate**: 22 musical divisions (1/64T to 16 bars, including triplets + dotted)
- **Per-modulator params**: Depth (0-127), Offset (-64/+63), Phase (0-360°), Smooth (0-5000ms slew for random)
- **ADSR**: Attack/Decay/Release (0-2000ms), Sustain (0-127), Amplitude (0-127). State machine: Idle→Attack→Decay→Sustain→Release
- **Routing**: Target MIDI output 1-16 + CC# 0-127, or CV output 1-8. Each modulator listens to a specific track's gate for Sync/Retrigger
- **UI**: Split-screen waveform preview (left) + parameter display (right). Routing overlay via Shift+Page. Waveform cache for smooth preview rendering
- **Split-screen layout**: Left half (4, 15, 116×34px): border box + centerline + waveform graph. Right half (128, 16, 128px): parameter name (Font::Tiny, Medium) + value (Font::Small, Bright)
- **Waveform rendering**: Sine/Triangle/Saw/Square drawn via `WaveformGenerator::generate()` (same algorithm as engine — WYSIWYG), cached in `_waveformCache[112]` int8 array, invalidated on param change. Random: horizontal line at mapped `currentValue()`. ADSR: envelope line segments (Attack→Decay→Sustain→Release) with amplitude-scaled peak + live playhead synced to engine's `adsrState()`/`adsrTimer()`
- **Dynamic footer**: LFO = SHAPE/RATE/DEPTH/OFSET/PHASE. Random = SHAPE/MODE/DEPTH/OFSET/SLEW. ADSR page 1 = SHAPE/ATTACK/DECAY/SUSTAIN/RELEAS, page 2 = AMPLIT/DEPTH/OFSET. Routing overlay = MODE/GATE/TARGET/EVENT/CC NUM (EVENT+CC dim for CV routing)
- **`drawPagination()` dots** for ADSR multi-page
- **Routing overlay**: replaces waveform view with routing config (Shift+Page toggle). Function labels change to MODE/GATE/TARGET/EVENT/CC NUM. Available array dims irrelevant options for CV target

**Internal architecture:**
- `temp-ref/modulove-performer/src/apps/sequencer/model/Modulator.h` (462 lines) — full model: Shape/Mode/RandomMode enums, all properties with edit/print/serialization
- `temp-ref/modulove-performer/src/apps/sequencer/engine/ModulatorEngine.h` (283 lines) — phase accumulation at tick rate (16-bit accumulator, 0-65535), waveform generation via delegate, ADSR state machine, per-modulator RNG (`Random _rng[8]`), tempo-aware slew calculation
- `temp-ref/modulove-performer/src/apps/sequencer/engine/WaveformGenerator.h` (99 lines) — shared static waveform sample gen (used by both engine + UI for WYSIWYG)
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/ModulatorPage.h/.cpp` (96+956 lines) — waveform preview, param editing, routing overlay popup, context menu, pagination
- Output: 0-127 (MIDI CC compatible). Formula: `(waveform * depth / 127 + offset + 64)` clamped. Phase increment: `65536 / (rate * 2)` per tick

**ModulatorEngine vs XFORMER CurveTrackEngine — complementary systems:**

| Aspect | Modulove ModulatorEngine | XFORMER CurveTrackEngine |
|--------|--------------------------|--------------------------|
| Purpose | Standalone modulation source for entire project | Dedicated track type on its own CV/gate channel |
| Count | 8 modulators, all active simultaneously | 1 per track slot (max 8) |
| Routing | Routes to any MIDI CC (1-16 + CC#) or CV output (1-8) | Fixed to its own CV/gate channel |
| Output | 0-127 (MIDI CC), `clamp(value+64, 0, 127)` | Float CV (-5V to +5V) on DAC |
| Waveforms | Standard LFO: Sine, Triangle, SawUp, SawDown, Square | Curve shapes via `Curve::function()` — bezier-like shape table |
| Random | Clocked/Triggered S&H with smooth slew (0-5000ms) | Chaos maps (Latoocarfian, Lorenz) — not S&H |
| ADSR | **Full**: Attack→Decay→Sustain→Release with amplitude | **None**: gate logic (peak/trough) but no envelope |
| Phase | Phase offset 0-360°, 16-bit accumulator per modulator | Global phase offset (step scanning, not free-running) |
| Rate | 22 musical divisions (1/64T to 16 bars) | Step-based divisor + clock multiplier |
| State | Stateless per-modulator (phase + RNG + ADSR timer) | Stateful: sequence, DJ filter, chaos maps, gate logic, 1kHz interpolation |
| Engine call | Central loop driving all 8 per tick | Inline in track engine update cycle |

**Key takeaway:** Complementary, not competing. CurveTrack is a sequence-driven CV track playing step sequences through curve shapes with chaos processing. ModulatorEngine is a free-running LFO/ADSR/random source routable anywhere. XFORMER has nothing like Modulove's free-running LFO system with ADSR and flexible routing.

---

## Untracked Discoveries — Reference Files

### High Value

#### Record Delay

**Source:** Mebitek

**Files:**
- `temp-ref/mebitek-performer/src/apps/sequencer/model/Project.h:433-453` — `_recordDelay` field
- `temp-ref/mebitek-performer/src/apps/sequencer/engine/NoteTrackEngine.cpp:141-143,169-171` — suppresses recording for initial steps

**What it does:** Delays recording start by N steps — avoids capturing the first steps during live recording.

---

#### GeneratorPage step selection + section navigation

**Source:** Mebitek

**Files:**
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/GeneratorPage.h` — `StepSelection<CONFIG_STEP_COUNT>*`, `_section` member
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/GeneratorPage.cpp` — keyDown/keyUp delegating to StepSelection, `stepOffset()` navigation, multi-track-type LED viz
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/GeneratorPage.cpp:1092-1162` — bank separators + bank frame for step visualization

**What it does:** Select which steps a generator targets via step buttons, navigate all 64 steps across 4 banks, LED feedback showing current playback + selected steps.

---

#### Function button latching in GeneratorPage

**Source:** Modulove

**Files:**
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/GeneratorPage.h:43` — `int _activeFunction = -1` member
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/GeneratorPage.cpp` — `keyPress()` toggles `_activeFunction`, `encoder()` checks it

**What it does:** Tap F0-F4 to latch a parameter, turn encoder to adjust without holding the button. Tap again releases. ~15 lines of changes, no architectural impact.

---

#### Bound track context validation in GeneratorPage

**Source:** Vinx

**Files:**
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/GeneratorPage.cpp` — `boundTrackContextValid()` + `ensureBoundTrackContext()` methods

**What it does:** If user switches tracks while GeneratorPage is open, auto-reverts and shows "GEN CANCELED". Prevents applying generator to wrong track.

---

#### Bidirectional gate offset

**Source:** Mebitek

**Files:**
- `temp-ref/mebitek-performer/src/apps/sequencer/engine/NoteTrackEngine.cpp` — negative `gateOffset` fires next step early
- `temp-ref/mebitek-performer/src/apps/sequencer/engine/SequenceState.h` — `nextStep()`, `calculateNextStepAligned()`, `calculateNextStepFree()` for pre-calculation

**What it does:** Negative gate offset pre-calculates and fires the next step before the grid boundary — anticipatory note feel.

---

#### Per-track curve CV input + multi-track recording

**Source:** Vinx / Mebitek

**Files:**
- `temp-ref/vinx-performer/src/apps/sequencer/model/CurveTrack.h` — `curveCvInput()`/`setCurveCvInput()` per-track
- `temp-ref/vinx-performer/src/apps/sequencer/model/Project.h:476-489` — `_useMultiCv` flag
- `temp-ref/vinx-performer/src/apps/sequencer/engine/CurveTrackEngine.cpp` — multi-track recording logic

**What it does:** Each CurveTrack selects its own CV input source (Vinx stores per-track, XFORMER uses project-global). Multi-track recording enables simultaneous CV recording on all tracks.

---

### Medium Value

#### AcidGenerator

**Source:** Vinx

**Files:**
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/AcidGenerator.h/.cpp` — bassline-style generator with density, slide, range, variation
- `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/Generator.h` — `Mode::Acid` enum entry

**What it does:** New generator mode for bassline-style sequences. Two sub-modes (Phrase + Layer). Complements existing Random/Euclidean generators.

---

#### WreckPatternWarningPage

**Source:** Vinx

**Files:**
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/WreckPatternWarningPage.h/.cpp` — confirmation dialog

**What it does:** Confirmation dialog before destructive pattern-scope chaos operations. Low effort safety UI.

---

#### PatternChange sync mode

**Source:** Vinx / Mebitek

**Files:**
- `temp-ref/vinx-performer/src/apps/sequencer/model/UserSettings.h` — `PatternChange` Setting
- `temp-ref/mebitek-performer/src/apps/sequencer/model/UserSettings.h` — same

**What it does:** UserSetting toggling between immediate and grid-synced pattern changes. Affects `PlayState::selectTrackPattern()` behavior.

---

#### Shift+step quick re-roll in GeneratorPage

**Source:** Mebitek

**Files:**
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/GeneratorPage.cpp` — `key.isStep() && key.shiftModifier()` calls `_generator->update()`

**What it does:** Shift+step in GeneratorPage triggers immediate re-roll without going through the full preview workflow.

---

### Nice-to-Have

#### ProceduralIcons.h

**Source:** Modulove

**Files:**
- `temp-ref/modulove-performer/src/apps/sequencer/ui/ProceduralIcons.h` (438 lines) — procedural icon drawing functions for playback controls, navigation, modulation, etc.

**What it does:** Single-header icon library. Zero-dependency, trivially mergable. Could enhance UI polish across pages.

---

#### KeyboardPage (on-screen piano)

**Source:** Modulove

**Files:**
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/KeyboardPage.h` (49 lines) — class with KeyState[16], _octave, _rootOffset, _frameOffset, track bank switching
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/KeyboardPage.cpp` (451 lines) — full implementation

**What it does:** Maps 16 step buttons to piano keyboard layout. S9-S16 = 8 white keys. S2,S3,S5,S6,S7 = 5 black keys. Encoder scrolls root note by semitone (0-11). Left/Right buttons also scroll. F4/F5 for octave shift (0-9). Track select buttons with double-tap for bank switching (Modulove 16-track). Plays note via MIDI output engine (CV + gate). Shows persistent last-note display + navigation arrows on OLED.

---

#### ChaosDefaultsPage + ChaosDefaultsSelectPage

**Source:** Vinx

**Files:**
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/ChaosDefaultsPage.h/.cpp` — configure default chaos/entropy target masks
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/ChaosDefaultsSelectPage.h/.cpp` — select mode (Sequence/Pattern/Entropy)
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/ChaosScopeSelectPage.h/.cpp` — select chaos scope

**What it does:** UI pages for configuring chaos/entropy target mask persistence. Required for chaos defaults (which is already notionally tracked in Settings Version Impact).

---

#### Per-track CV min/max range for CurveTrack

**Source:** Mebitek

**Files:**
- `temp-ref/mebitek-performer/src/apps/sequencer/model/CurveTrack.h` — per-track `min()`/`max()` CV range fields
- `temp-ref/mebitek-performer/src/apps/sequencer/engine/CurveTrackEngine.cpp` — `float(_curveTrack.min()) / CurveSequence::Min::Max` scaling

**What it does:** Each curve track can scale its CV output to a per-track min/max voltage range, rather than using a global range.

---

#### WindowPainter extras (`drawPagination`, `available[]` overloads)

**Source:** Modulove

**Files:**
- `temp-ref/modulove-performer/src/apps/sequencer/ui/painters/WindowPainter.h/.cpp` — `drawPagination()` + function key `available[]` overloads

**What it does:** Page indicator dots for multi-page views; visually dim unavailable function buttons.

---

## No-Go Items (Kept for Reference)

These were researched and documented but explicitly not implementing. Code reference preserved for future evaluation.

| Item | Source | Rationale |
|------|--------|-----------|
| **Microtiming Recording** | Modulove | Changes NoteSequence::Step bitfield layout + serialization. Insufficient payoff for XFORMER's composition-focused use case. |
| **Curve Backward Playback** | Mebitek | Curve forward playback already covers core use. Backward adds engine complexity with marginal musical value. |
| **Steps to stop** | Mebitek | Not applicable to XFORMER's composition workflow. |

---

## Cross-Fork UI Comparison

### NoteSequencePage — UI differences

- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/NoteSequencePage.cpp` — save/load sequence to SD file slots (5 methods), text input naming via encoder on row 0, cancelable edit with snapshot/commit/rollback for Scale/RootNote rows, scale preview in engine, footer shows "CANCEL" on editable rows
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/NoteSequencePage.cpp` — save/load + naming (same)
- XFORMER has Accumulator value in header; neither fork does

### NoteSequenceEditPage — UI differences

- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` — encoder cycles layers when nothing selected, in-memory undo buffer `_inMemorySequence`, note chord entry via F0-F4, `tieNotes()` via Shift+F2, pattern follow toggle via Step15, BypassScale/StageRepeats/StageRepeatsMode layers, `_keyPressEventTracker` for double-tap detection
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` — same as vinx except section clamped (not wrapped), `clearStepsSelected()` clears only selected steps
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` — bank color inversion for tracks 9-16, custom double-tap with `_lastTappedStep`
- XFORMER has: PulseCount, GateMode, AccumulatorTrigger, HarmonyRoleOverride, InversionOverride, VoicingOverride layers; Accumulator display + Ikra playhead; keyboard() F-key handler; DivX quick edit

### CurveSequenceEditPage — UI differences

- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp` — 4-function footer (no PHASE), pattern follow in header, GateOffset+GateLength layers, direction-aware playback cursor, encoder cycles all layers when nothing selected, `_inMemorySequence` undo, Launchpad generator overlay, `openLaunchpadGenerator()`, section wraps via `sequence.section()` modulo
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp` — same as vinx but no GateOffset/GateLength, section clamped (not wrapped)
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp` — same layers as XFORMER baseline, no pattern follow, no direction-aware cursor, section managed via `_section` member
- XFORMER has: 5-function footer (including PHASE), Wavefolder/Chaos sub-editing modes, GlobalPhase mode, gate event messages (PEAK/TROUGH/ZERO UP/DOWN), 7 advanced gate modes, gate length as % of beat, gate presets context menu (Page+Step14), LFO/Macro/Transform context menus (Page+Step4/5/6), global phase cursor, SettingsClipboard, double-press for Peak+Trough

### PatternPage — UI differences

- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/PatternPage.cpp` — PatternChange user setting (sync vs immediate), pattern chaining via 2 step keys, double-click context menu, KeyPressEventTracker
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/PatternPage.cpp` — same as vinx
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/PatternPage.cpp` — 16-track bank switching (T1-8/T9-16), per-track pattern change via held track + encoder, Left/Right arrows switch bank

### TrackPage — UI differences

- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/TrackPage.cpp` — track naming via encoder on row 0, Logic track auto-routing (encoder on rows 14-15), double-click context menu, Stochastic/Logic/Arp track modes
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/TrackPage.cpp` — same as vinx
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/TrackPage.cpp` — conditional compilation for Curve/MidiCv tracks
- XFORMER has: Tuesday/DiscreteMap/Indexed/Teletype tracks, Teletype TI preset cycling/F5 sync outs, Tuesday reseed, RESEED context action

### OverviewPage — UI differences

- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp` (1357 lines) — full interactive step selection, step detail popup, quick edit via Page+steps, double-click step toggles gate, double-click track enters sequence edit, 7-layer curve editing via encoder, note transposition with selection, track names (not "T1"-"T8"), inverted pattern numbers, "F"(follow)/"L"(loop)/"K"(keyboard) indicators, per-track LED matrix control, section navigation (wrap-based)
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp` (1299 lines) — same as vinx but 4-layer curve editing, clamp-based section navigation
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp` (194 lines) — display-only, 16-track bank switching, conditional curve/midicv compilation
- XFORMER (`src/apps/sequencer/ui/pages/OverviewPage.cpp`, 433 lines) — display-only, renders Tuesday/DiscreteMap/Indexed/Teletype tracks which no fork can

### ListPage — UI differences

- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/ListPage.cpp` — configurable MenuWrap via `MenuWrapSetting`/`SettingMenuWrap` from UserSettings
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/ListPage.cpp` — dynamic column widths (text-width-aware, shrink-to-fit with minGap=8px)
- XFORMER has: `keyboard()` handler for PC keys (Up/Down/Left/Right/Enter) — no fork has this

---

### Graphical rendering comparisons

#### PerformerPage — 8-column track grid
- All forks share the same layout: 8 columns × 32px, activity rect (16×16), mute fill box, progress bar, fill amount bar, track number "T1"-"T8"
- XFORMER has `keyboard()` handler (F1-F5 routed); vinx/mebitek have `isKeySelected()` + tempo encoder editing instead
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/PerformerPage.cpp` — baseline draw, `drawMutes(leds, 0, 0)` bug
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/PerformerPage.cpp` — same
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/PerformerPage.cpp` — 16-track bank aware

#### NoteSequenceEditPage — step grid rendering
- `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp:93-156` — XFORMER: loop dots between first/last step, colored gate rects, Ikra playhead vline (optional), accumulator trigger dot (top-right of step), per-layer viz: probability bars, offset bars, retrigger bars, length bars, note names at y+20/y+27, slide bars, condition text, harmony/inversion/voicing abbreviations, pulse count text, gate mode text
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` — no loop dots, uses `SequencePainter::drawStepIndex()` for condition-colored step index, uses `SequencePainter::drawGateBody()` showing gate offset/length/retrigger/slide within the gate rect (richer gate viz than XFORMER's simple fillRect). No Ikra, accumulator, harmony/voicing/inversion. BypassScale + StageRepeats text rendering
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` — has loop dots, plain text step index (no drawStepIndex), simple fillRect gate (no drawGateBody), BypassScale + StageRepeats text
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` — same baseline as XFORMER but note name at y+28 (1px offset), no Ikra/accumulator

#### CurveSequenceEditPage — curve shape rendering
- `src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp` — XFORMER: draws actual curve shapes per step (line graph via `Curve::function()`), shape variation overlay, min/max range lines, gate patterns, probability bars. Wavefolder mode: fold/gain/filter/xfade bar graphs with bipolar center line for DJ FILTER. Chaos mode: amt/hz/heat/damp bar graphs with algo name (Latoocarfian split across 2 lines) + range text. Global phase cursor vertical line
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp` — draws curve shapes, min/max, gate. Direction-aware cursor (forward vs reverse). Launchpad generator overlay (2×6 grid) replaces step grid entirely when active. No wavefolder/chaos/global phase viz
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp` — same as vinx but simpler gate (2-state). No launchpad overlay
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp` — same baseline shapes/min/max/gate. No direction-aware cursor, no wavefolder/chaos

#### GeneratorPage — step pattern grid + target grids
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/GeneratorPage.cpp:1092-1162` — bank separators (vertical lines at 16-step boundaries), bank frame highlight around active bank, dimmed steps for non-active banks. 2-row layout for >16 steps with row gap. `drawChaosGenerator()` renders 16-cell chaos target grid (4×4 with All On/Off cells at positions 11/15, filled background for enabled). `drawChaosEntropyGenerator()` renders 14 entropy targets in same 4×4 layout with 2 action cells. `drawAcidGenerator()` with bank indicators + separators
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/GeneratorPage.cpp` — 2-row layout for >16 steps, larger step boxes (12px vs 8px height). StepSelection integration shows selected steps via highlight
- `src/apps/sequencer/ui/pages/GeneratorPage.cpp` — single row, 8px step boxes, fixed 16-step view, no bank viz or target grids

#### PatternPage — pattern grid squares
- All forks: same 8-track × 16-pattern 2×8 grid with 3px squares. Pattern number "P1"-"P16" (or "S" during snapshot) below
- XFORMER/modulove: pattern text at y+10. `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/PatternPage.cpp` — 16-track bank aware
- Vinx/mebitek: pattern text at y+7 (3px offset). `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/PatternPage.cpp` — PatternChange label swaps to "IMMEDIATE", pattern chaining via 2 held step keys

#### OverviewPage — per-track step grids
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp` (1357 lines) — draws interactive 16-step grid per track, step detail popup frame (128×32 with vertical divider), track names (not "T1"-"T8"), inverted pattern number box (fillRect + Sub blend), "F"(follow)/"L"(loop)/"K"(keyboard) indicator text. Curve rendering with phased cursor. 12-step grid for stochastic/arp tracks with scale indicators. Gate output visual for logic tracks. Per-track LED matrix control
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp` (1299 lines) — same as vinx but 4-layer curve editing viz
- `src/apps/sequencer/ui/pages/OverviewPage.cpp` (433 lines) — display-only: track labels + pattern number + gate indicator + CV indicator columns at x=64/192. Tuesday: gate square + step counter + algo name. DiscreteMap: threshold markers + input cursor. Indexed: timeline bars with gate fill. Teletype: TI1-4/TR1-4/CV1-4 cell indicators
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp` (194 lines) — display-only, 16-track bank switching, conditional curve/midicv compilation

#### New track types — graphical rendering across forks

**NoteTrack** — all forks draw a 16-step grid (6×6 filled squares per step)
- `src/apps/sequencer/ui/pages/OverviewPage.cpp:15-40` — XFORMER: step offset = `max(0, currentStep)/16 * 16` (auto-scrolling). Gate fill: bright/medium. 8px steps starting at x=64
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp:66-90` — Vinx: step offset from `sequence.section()` (configurable), auto-follow via `patternFollow` flag. 8px steps starting at x=76 (12px offset from XFORMER). Same gate color pattern
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp:66-90` — Mebitek: same as vinx (section-based offset + pattern follow)
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp:7-56` — Modulove: step offset from `_section` member. 8px steps starting at x=64. Same gate logic

**CurveTrack** — all forks draw curve line graphs per step
- `src/apps/sequencer/ui/pages/OverviewPage.cpp:64-102` — XFORMER: `drawCurve()` with step-based function eval, BlendMode::Add, min/max range lines, shape variation. Two cursor modes: normal (bright vline) and globalPhase (medium vline at `phasedStep` position)
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp:208-239` — Vinx: same `drawCurve()` function, section-based offset + pattern follow, simple cursor vline (no globalPhase). x=76 vs XFORMER's x=64
- `temp-ref/mebitek-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp:208-239` — Mebitek: same as vinx
- `temp-ref/modulove-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp:57-103` — Modulove: same `drawCurve()`, `_section`-based offset. No globalPhase cursor

**XFORMER-only track type renderings** — no fork has equivalents:

- `src/apps/sequencer/ui/pages/OverviewPage.cpp:104-158` — **TuesdayTrack**: gate square (blinking when active, 6×6 at x=64), step counter text ("step/loop" or "step"), algorithm name right-aligned at x=192 (13 algo names: Test/TriTr/Stomp/Marko/Chip1/Chip2/Wobbl/SclWk/Wndow/Minml/Ganz/Blake/Aphex/Autec/StpWv)
- `src/apps/sequencer/ui/pages/OverviewPage.cpp:160-224` — **IndexedTrack**: timeline bars across 128px width. Each step drawn as a proportional-width rectangle (step duration → pixel width). Gate fill as filled portion from bottom. Active step highlighted. Bresenham-like error accumulation for pixel distribution
- `src/apps/sequencer/ui/pages/OverviewPage.cpp:226-277` — **DiscreteMapTrack**: 128px baseline at y+6. Per-stage threshold markers (vlines growing upward from baseline, active=5px/normal=3px height). Input cursor as full-height vline tracking current CV input normalized to range
- `src/apps/sequencer/ui/pages/OverviewPage.cpp:279-322` — **TeletypeTrack**: 10 cells at 8px intervals. Groups: TI1-4 (assigned color + activity), TR1-4 (activity), CV1-4 (bipolar magnitude: bright >2048, medium >512, low). 1px gaps between groups. Assigned+active = bright, assigned only = medium, unassigned = low

**Fork-only track type renderings** — XFORMER has no equivalents:

- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp:92-122` / `temp-ref/mebitek-performer/...` — **LogicTrack**: 16-step grid (same layout as NoteTrack). When current step AND `gateOutput(stepIndex)`, draws a smaller inner square (3×3 at center) instead of full 6×6 fill — visual distinction for logic gate output
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp:146-175` / `temp-ref/mebitek-performer/...` — **StochasticTrack**: 12-step grid (not 16). Scale indicators: if `scale.isNotePresent(step.note())` draws a Sub-blend dimple in the center of the gate square. 12px extra left margin (x=76+16 vs x=76)
- `temp-ref/vinx-performer/src/apps/sequencer/ui/pages/OverviewPage.cpp:177-207` / `temp-ref/mebitek-performer/...` — **ArpTrack**: 12-step grid. Same scale indicator pattern as stochastic. Same 12px left margin offset
- **Modulove**: Only NoteTrack + CurveTrack rendered. Tuesday/DiscreteMap/Indexed/Teletype/Logic/Stochastic/Arp all absent. MidiCv track is empty

---

## Status
Active — Phase 1 items 1-3, 5-6 complete and hardware-verified.

## Priority
High

## Dependencies

| Task | Relationship |
|------|-------------|
| `launchpad-track-port` | **Independent.** Shared features (Generators, Undo, Performer Page) have core logic here, triggers there. Can implement in either order. |

## Current Codebase Reference Files

- `src/apps/sequencer/Config.h` — CONFIG_TRACK_COUNT=8, CONFIG_PPQN=192
- `src/apps/sequencer/model/Track.h` — 7 track types
- `src/apps/sequencer/model/Project.h` — project structure, tracks array
- `src/apps/sequencer/model/NoteSequence.h` — step bitfields, harmony system
- `src/apps/sequencer/model/CurveSequence.h` — curve sequence layers
- `src/apps/sequencer/engine/NoteTrackEngine.h` — note track engine
- `src/apps/sequencer/engine/CurveTrackEngine.h` — curve track engine
- `src/apps/sequencer/engine/StepRecorder.h` — step recording with timing
- `src/apps/sequencer/engine/ClockEngine.h` — clock/reset/stop logic
- `src/apps/sequencer/ui/page/PerformerPage.h` — performer page
- `src/apps/sequencer/ui/page/GeneratorPage.h` — generator page base

---

## Chaos & Entropy — Vinx vs XFORMER Analysis

### What XFORMER Already Has
- **RandomGenerator** (`src/apps/sequencer/engine/generators/RandomGenerator.h/.cpp`) — euclidean + random generators only; `Generator::Mode` enum: `InitLayer, Euclidean, Random, Last`
- **SequenceBuilder** (`src/apps/sequencer/engine/generators/SequenceBuilder.h`) — template-based, no `apply()`, `showOriginal()`, `showPreview()`, `showingPreview()`, `applyEntropy()`, or `_preview` state; only simple original→edit model
- **EntropyTargets.h** — does **not** exist in XFORMER
- **GeneratorPage** — no preview/apply A/B workflow, no chaos/entropy target mask UI
- **UserSettings** — only Brightness, Screensaver, WakeMode, DimSequence; Settings::Version = 1

### What Vinx Adds

| Feature | Vinx Implementation | XFORMER Gap |
|---------|-------------------|-------------|
| **ChaosGenerator** | `ChaosGenerator.h/.cpp` — seed-based blend randomization of 14 targets across Note sequence layers | No generator with blend/randomize semantics exists; RandomGenerator is pattern-based, not blend-based |
| **EntropyTargets** | `EntropyTargets.h` — 14-target enum (Gate, GateOffset, GateProbability, Retrigger, etc.) with `targetMask` bitfield | Missing entirely — no target mask abstraction |
| **SequenceBuilder A/B preview** | `revert()`, `apply()`, `showOriginal()`, `showPreview()`, `showingPreview()`, plus `_preview` copy | XFORMER's SequenceBuilder has only `_edit` + `_original`, no preview/apply state machine |
| **ChaosSequenceBuilder** | Pattern-scoped (single track or all tracks) with per-track backup/restore | No equivalent — XFORMER has no cross-track generator application |
| **Generator page preview/apply** | `GeneratorPage` A/B state: ORIGINAL → preview → commit/cancel flow | XFORMER's GeneratorPage has no preview/commit — just immediate application |
| **WreckPatternWarningPage** | Confirmation dialog for destructive pattern-scope chaos | No equivalent |
| **ChaosSeqLayersSetting** | Persists chaos target mask per-sequence (uint16_t, addedInVersion=3) | No persistence of target masks |
| **ChaosPatLayersSetting** | Persists chaos target mask per-pattern (uint16_t, addedInVersion=3) | No persistence of target masks |
| **EntropyLayersSetting** | Persists entropy target mask (uint16_t, addedInVersion=5) | No persistence of target masks |
| **MenuWrapSetting** | Menu wrap toggle (uint8_t, addedInVersion=4, default=on) | Not present |
| **PatternChange** | Immediate/sync pattern change toggle | Not present |
| **SyncSong** | Song sync toggle | Not present |
| **LPStyle/LPNoteStyle** | Launchpad style settings | Belongs in launchpad-track-port |

### Chaos vs Entropy — Key Distinction

**Chaos** (VinxScorzia): Operates on `NoteSequence::Step` fields directly via `ChaosGenerator` + `ChaosSequenceBuilder`. Has its own target enum (`ChaosGenerator::Target`, 14 targets). Supports **per-pattern** scope (all note tracks at once). Uses `blendRandomValue()` with amount% to interpolate between original and random.

**Entropy**: Operates generically on any sequence type's layers via `SequenceBuilderImpl::applyEntropy()`. Uses `EntropyTarget` enum (14 targets). Works per-layer with template specialization for `NoteSequence`, `CurveSequence`, `StochasticSequence`, `LogicSequence`, `ArpSequence`. This is the **unified, generalized** approach.

**Decision**: XFORMER should adopt the **Entropy** approach (template-based, multi-sequence-type) rather than the Chaos approach (Note-only, separate builder). Entropy is already partially ported in the Vinx `SequenceBuilder.h` — it has `applyEntropy()` and the full `entropy_detail` template system — but XFORMER's `SequenceBuilder` lacks these.

### Settings Version Impact

Adding chaos/entropy persistence requires bumping `Settings::Version`. The Vinx fork shows the version progression:

| Version | Added |
|---------|-------|
| 1 | Brightness, Screensaver, WakeMode, DimSequence |
| 2 | (not documented — likely PatternChange, SyncSong, LPStyle, LPNoteStyle) |
| 3 | ChaosSeqLayers, ChaosPatLayers |
| 4 | MenuWrap |
| 5 | EntropyLayers |

For XFORMER (currently Version=1), adding MenuWrap requires Version=2 at minimum. Adding chaos/entropy layer persistence would require Version=3+. These can be staggered — each new setting uses `addedInVersion` guards in `UserSettings::read()` so old settings files load safely with defaults for missing fields.
