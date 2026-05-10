# Launchpad Track Port — Full Implementation Plan

Status: Planning
Priority: Medium

## Overview

Rebuild LaunchpadController.cpp to support all 5 grid-editable track types of PER|FORMER/XFORMER (Note, Curve, Tuesday, DiscreteMap, Indexed). MidiCvTrack and TeletypeTrack are out of scope. Each track type differs radically in data model, requiring customized grid layouts per type. The dispatch architecture (switch on `trackMode()`) stays the same; we add cases for each new type.

## Key Files

| File | Lines | Role |
|------|-------|------|
| `src/.../ui/controllers/launchpad/LaunchpadController.h` | 194 | Declarations |
| `src/.../ui/controllers/launchpad/LaunchpadController.cpp` | 974 | All logic (will grow to ~2000+) |
| `src/.../model/NoteSequence.h` | 824 | Note sequence (19 layers, ReRene/Ikra modes) |
| `src/.../model/CurveSequence.h` | ~600 | Curve sequence (7 layers, chaos params) |
| `src/.../model/IndexedSequence.h` | ~610 | Indexed sequence (48 steps, no layers) |
| `src/.../model/DiscreteMapSequence.h` | ~365 | DiscreteMap (32 stages, no layers) |
| `src/.../model/TuesdaySequence.h` | ~540 | Tuesday (algorithm params, no steps) |

| `src/.../model/Project.h` | ~600 | Track/sequence accessors |

## Dispatch Points (Every Function)

Every function in LaunchpadController.cpp uses `switch(_project.selectedTrack().trackMode())`. All 5 supported track types need cases in every switch:

**Sequence mode (11 dispatch points):**

| Function | Lines | Purpose |
|----------|-------|---------|
| `sequenceUpdateNavigation` | 302-333 | Set navigation bounds for current layer |
| `sequenceSetLayer` | 336-363 | Map grid button press to layer select |
| `sequenceSetFirstStep` | 365-377 | Set sequence first step |
| `sequenceSetLastStep` | 380-392 | Set sequence last step |
| `sequenceSetRunMode` | 395-408 | Set run mode |
| `sequenceToggleStep` | 410-419 | Double-press toggle gate |
| `sequenceEditStep` | 438-450 | Edit step data |
| `sequenceDrawLayer` | 488-509 | Draw layer selector grid |
| `sequenceDrawStepRange` | 511-528 | Draw first/last step range |
| `sequenceDrawRunMode` | 530-545 | Draw run mode picker |
| `sequenceDrawSequence` | 547-560 | Draw main sequence grid |

**Pattern mode (1 dispatch point):**
| `patternDraw` | 643-652 | Show edited pattern indicators |

**Per-track handler functions (5 existing):**
| Function | Lines | Purpose |
|----------|-------|---------|
| `sequenceToggleNoteStep` | 422-436 | Note-specific toggle |
| `sequenceEditNoteStep` | 453-471 | Note-specific edit |
| `sequenceEditCurveStep` | 474-486 | Curve-specific edit |
| `sequenceDrawNoteSequence` | 562-583 | Note-specific draw |
| `sequenceDrawCurveSequence` | 585-606 | Curve-specific draw |

---

## Track-by-Track Implementation Plan

### Phase 1: Fix NoteTrack Regressions (HIGH PRIORITY)

**What's broken:** 6 new layers unreachable; step highlighting uses linear step in ReRene/Ikra modes; new mode/divisorY/noteStep fields not exposed.

#### 1a. Add 6 missing layers to noteSequenceLayerMap

In `LaunchpadController.cpp`, extend the array. Column 4 has Condition at row 0, rows 1-6 are free:

```cpp
[int(NoteSequence::Layer::AccumulatorTrigger)]       =  { 1, 4 },
[int(NoteSequence::Layer::PulseCount)]               =  { 2, 4 },
[int(NoteSequence::Layer::GateMode)]                 =  { 3, 4 },
[int(NoteSequence::Layer::HarmonyRoleOverride)]      =  { 4, 4 },
[int(NoteSequence::Layer::InversionOverride)]        =  { 5, 4 },
[int(NoteSequence::Layer::VoicingOverride)]          =  { 6, 4 },  // stub
```

`layerValue()`/`setLayerValue()` already handle all 19 layers. No additional code needed — the existing `drawNoteSequenceBars()` just works.

#### 1b. Fix current step for non-Linear modes

In `sequenceDrawNoteSequence`, replace:
```cpp
int currentStep = trackEngine.isActiveSequence(sequence) ? trackEngine.currentStep() : -1;
```
with:
```cpp
int currentStep = trackEngine.isActiveSequence(sequence) ?
    (sequence.mode() == NoteSequence::Mode::Linear
        ? trackEngine.currentStep()
        : trackEngine.currentNoteStep())
    : -1;
```

#### 1c. Add Mode selector to sequence UI

Use the **RunMode button area** to expose `NoteSequence::Mode` (Linear/ReRene/Ikra) alongside `Types::RunMode`. When in Note track:

- **RunMode button** cycles between old behavior (RunMode enum) and Mode enum.
- Or: add a new `Mode` button concept. Simpler: when `RunMode` button is held, show rows 0-1 for RunMode (Forward/Backward/...) and rows 2-3 for NoteSequence Mode (Linear/ReRene/Ikra).

Alternative: repurpose the `Fill` button in Note mode to show the Mode selector. Or add Mode to `sequenceSequenceRunMode` — `RunMode` button press while holding `Shift` shows Mode instead.

**Recommendation:** Show Mode at the top of the RunMode picker (rows 0-2 = Linear/ReRene/Ikra, rows 3-N = standard RunMode values). Grid columns represent the selected value.

#### 1d. Add noteFirstStep / noteLastStep (Ikra loop bounds)

When `sequence.mode() == Ikra`, the `FirstStep`/`LastStep` buttons should edit `noteFirstStep()`/`noteLastStep()` instead of `firstStep()`/`lastStep()`. Modify `sequenceSetFirstStep`/`sequenceSetLastStep` in the Note case to check `mode() == Ikra`.

---

### Phase 2: CurveTrack — Add Chaos & Wavefolder Params (LOW PRIORITY)

Curve's Layer enum is clean (7 layers, no regressions). But CurveSequence has ~20 additional parameters (chaos, wavefolder, dj filter, xFade, etc.) with no Launchpad UI.

**Approach:** Add a **"params" sub-mode** toggled via a dedicated button combo. Or use the `RunMode` button to switch between editing layers (default) and editing sequence-level params. In param mode:
- Row 0-3: chaos params (amount, algo, range, rate)
- Row 4-5: wavefolder (fold, gain)
- Row 6: djFilter, xFade

Each param rendered as a bar or dot column on the grid.

**Not blocking** — Curve already has full layer editing. This is a nice-to-have expansion.

---

### Phase 2.5: Macro Grid — Batch Step Operations (HIGH PRIORITY)

Macros are batch operations modifying multiple steps at once. Currently only accessible via LCD context menus requiring multi-key combos. The Launchpad 8×8 grid is ideal — dedicate a sub-mode to it.

#### Existing Macros by Track Type

**CurveSequence macros** (12 — `CurveSequence.h:587-602`, `.cpp:400-560`):

| # | Macro | Function | Visual Effect |
|---|-------|----------|---------------|
| 0 | **Init** | `populateWithMacroInit(first, last)` | Flat: all Min=0, Max=255 |
| 1 | **Fm** | `populateWithMacroFm(first, last)` | Chirp: triangle×t² sweep, accelerating |
| 2 | **Damp** | `populateWithMacroDamp(first, last)` | Ringing: sin(4×2πt)×(1-t) decay |
| 3 | **Bounce** | `populateWithMacroBounce(first, last)` | Bounce: \|sin(4πt)\|×(1-t) decay |
| 4 | **Raster** | `populateWithRasterizedShape(first, last)` | Rasterize source shape at N phase intervals |
| 5 | **Invert** | `transformInvert(first, last)` | Swap Min↔Max per step |
| 6 | **Reverse** | `transformReverse(first, last)` | Reverse step order |
| 7 | **Mirror** | `transformMirror(first, last)` | Mirror around range center |
| 8 | **Align** | `transformAlign(first, last)` | Align all to same shape |
| 9 | **Smooth** | `transformSmooth(first, last)` | Smooth min/max transitions |
| 10 | **Walk** | `transformWalk(first, last)` | Smooth walk endpoints |
| 11 | **Random** | `transformRandom(first, last)` | Randomize min/max |

**DiscreteMap macros** (16 — `DiscreteMapSequencePage.h:59-69`, `DiscreteMapSequence.h:334-341`):

| # | Macro | Type | Effect |
|---|-------|------|--------|
| 0-7 | **Full/Inv/Pos/Neg/Top/Btm/Ass/BAss** | RangeMacro | Set voltage range low/high |
| 8 | **ClearAll** | Clear | Reset all 32 stages |
| 9 | **ClearThresh** | Clear | Reset thresholds to 0 |
| 10 | **ClearNotes** | Clear | Reset note indices to 0 |
| 11 | **ClearDirs** | Clear | Reset directions to Off |
| 12 | **RandomAll** | Randomize | Randomize thresholds+notes+dirs |
| 13 | **RandomThresh** | Randomize | Randomize thresholds |
| 14 | **RandomNotes** | Randomize | Randomize note indices |
| 15 | **RandomDirs** | Randomize | Randomize directions |

**IndexedSequence macros** (18 — `IndexedSequenceEditPage.cpp:1605+`):

| # | Macro | Category | Effect |
|---|-------|----------|--------|
| 0 | **3/9** | Rhythm | Euclidean 3 or 9 hits |
| 1 | **5/20** | Rhythm | Clave 5 or 20 |
| 2 | **7/28** | Rhythm | Tuplet 7 or 28 |
| 3 | **3-5** | Rhythm | Polyrhythm 3 against 5 |
| 4 | **5-7** | Rhythm | Polyrhythm 5 against 7 |
| 5 | **M-TALA** | Rhythm | Indian tala (Khand/Tihai/Dhamar) |
| 6 | **TRI** | Waveform | Triangle wave durations |
| 7 | **2TRI** | Waveform | 2-cycle triangle |
| 8 | **3TRI** | Waveform | 3-cycle triangle |
| 9 | **2SAW** | Waveform | 2-cycle sawtooth |
| 10 | **3SAW** | Waveform | 3-cycle sawtooth |
| 11 | **SCALE** | Melodic | Fill notes from scale |
| 12 | **ARP** | Melodic | Arpeggio pattern |
| 13 | **CHORD** | Melodic | Chord voicings |
| 14 | **MODAL** | Melodic | Modal pattern |
| 15 | **M-MEL** | Melodic | Melodic pattern |
| 16 | **D-LOG** | Duration | Logarithmic duration shaping |
| 17 | **D-EXP** | Duration | Exponential duration shaping |
| 18 | **Q-MEAS** | Duration | Quantize durations to measure |
| 19 | **REV** | Duration | Reverse step order |
| 20 | **MIRR** | Duration | Mirror duration values |

**TuesdayTrack** has no macros (algorithm parameters only — edited directly as parameters).

**TuesdayTrack** has no macros (algorithm parameters only — edited directly as parameters).

#### Macro Sub-Mode Design

**Trigger:** Hold **Shift+Fill** → enter Macro mode. Release both → exit.

**While in Macro mode:**
- Grid shows available macros for the current track type
- Tap a grid cell to apply that macro to the *current visible step range* (or full loop if Navigate is held)
- Scene LEDs flash to indicate macro apply
- Left/Right page through macro categories (for Indexed)

**Range semantics per macro:**
- Curve transforms/macros: apply from `sequence.firstStep()` to `sequence.lastStep()`
- DiscreteMap clear/randomize: operate on all 32 stages
- DiscreteMap RangeMacro: sets global range (not per-stage)
- Indexed macros: apply to `0..activeLength()-1`

#### Grid Layouts

**CurveSequence — 4×3 grid of 12 macro buttons:**

```
Rows 0-2, Cols 0-3:
  Init   Fm    Damp  Bounce
  Raster Inv   Rev   Mirror
  Align  Smooth Walk  Random

Cols 4-7: empty (or show range preview bars)
```

Color: Green = populate macros, Yellow = transform macros.

**DiscreteMap — 4×4 grid (16 macros):**

```
Rows 0-3, Cols 0-1 (RangeMacro):  Rows 4-6, Cols 0-3 (Clear/Random):
  Full   Inv                         ClrAll  ClrThr  ClrNote  ClrDir
  Pos    Neg                         RndAll  RndThr  RndNote  RndDir
  Top    Btm
  Ass    BAss
```

**IndexedSequence — 4 pages × up to 7 macros each:**

Navigate with **Up/Down** to switch category pages:
- Page 0 (Rhythm): 3/9, 5/20, 7/28, 3-5, 5-7, M-TALA
- Page 1 (Waveform): TRI, 2TRI, 3TRI, 2SAW, 3SAW
- Page 2 (Melodic): SCALE, ARP, CHORD, MODAL, M-MEL
- Page 3 (Duration): D-LOG, D-EXP, Q-MEAS, REV, MIRR

#### Implementation

New struct member:
```cpp
bool _macroMode = false;
```

Modified `sequenceDraw()` — at the top, check:
```cpp
if (buttonState<Shift>() && buttonState<Fill>()) {
    _macroMode = true;
    sequenceDrawMacros();
    return;
}
_macroMode = false;
```

Modified `sequenceButton()`:
```cpp
if (_macroMode) {
    if (button.isGrid()) applyMacro(button.row, button.col);
    return;
}
```

New functions:
```cpp
void sequenceDrawMacros();   // dispatches to per-track macro grid layout
void sequenceDrawCurveMacros();
void sequenceDrawDiscreteMapMacros();
void sequenceDrawIndexedMacros();
void applyMacro(int row, int col);
void applyCurveMacro(int row, int col);
void applyDiscreteMapMacro(int row, int col);
void applyIndexedMacro(int row, int col);
```

#### Effort: ~2h

- 8 new function declarations in .h
- ~200 lines of macro grid drawing + dispatch code
- No model changes needed — all `populateWithMacro*()` / `transform*()` / `clear*()` / `randomize*()` already exist

---

**Characteristics:**

| Property | Value |
|----------|-------|
| Steps | 1-48 (variable via `activeLength()`) |
| Per-step fields | noteIndex(-63..63), duration(0-1023), gateLength(0-1023), slide(bool), groupMask(0-15) |
| Layers | **None** |
| Sequence props | divisor, clockMultiplier, loop, runMode, scale, rootNote, firstStep, resetMeasure, routeA/B, routeCombineMode |
| Engine API | `isActiveSequence()`, `currentStep()`, `effectiveStepDuration()`, `effectiveGateTicks()` |
| `isEdited()` | **Must add** — follow mebitek pattern (check if any step != default cleared state) |

#### 3a. LayerMapItem array — 4 views as pseudo-layers

Following mebitek's pattern, give IndexedTrack a `LayerMapItem[]` where each "layer" = a visualization mode:

```cpp
static const LayerMapItem indexedSequenceLayerMap[] = {
    [0] = { 0, 0 },  // Pitch — noteIndex as dots (piano roll)
    [1] = { 1, 0 },  // Duration — duration bars + gateLength overlay
    [2] = { 2, 0 },  // Gate/Slide — gateLength, slide toggle, groupMask color
    [3] = { 3, 0 },  // Route — routeA/B + combine mode indicator
};
```

Tap **Layer** button → grid lights up 4 positions → tap to select view. `_sequence.selectedLayer` stores 0-3.

Same dispatch pattern as Note/Curve: `sequenceDrawSequence()` switches on `_sequence.selectedLayer` instead of enum, routes to per-view draw functions.

#### 3b. Step range → dynamic, not fixed

Steps are 1-48. The grid shows 8 steps at a time, navigated with Left/Right/Up/Down buttons. `sequenceUpdateNavigation` sets bounds based on `activeLength()`.

#### 3c. FirstStep → rotation offset

`firstStep()` is the rotation offset (0..activeLength-1). The FirstStep button shows 0..activeLength-1 on grid.

#### 3d. No LastStep

IndexedSequence has no `lastStep()`. The LastStep button is unused (or could be repurposed for `activeLength()` editing — add/remove steps from the sequence).

#### 3e. RunMode

IndexedSequence has `runMode()` (Routable) — same Types::RunMode as Note/Curve. Reuse identical RunMode picker.

#### 3f. Draw functions needed

```
sequenceDrawIndexedSequence()     — main draw, dispatches by edit mode
sequenceDrawIndexedPitch()        — noteIndex as dot-per-step (piano roll)
sequenceDrawIndexedTiming()       — duration bars + gateLength dots
sequenceDrawIndexedGateSlide()    — slide on/off + groupMask color
sequenceEditIndexedStep()         — edit handler, dispatches by edit mode
sequenceEditIndexedPitch()        — tap grid to set noteIndex
sequenceEditIndexedTiming()       — tap grid to set duration (row = value/8th)
sequenceEditIndexedGateSlide()    — tap to toggle slide, double-press to toggle group
sequenceToggleIndexedStep()       — toggle gateLength or slide
```

#### 3g. Pattern page

IndexedSequence has no `isEdited()`. Skip edited pattern indicator, or add a simple `isEdited()` method (check if any step `!=` default cleared state).

#### 3h. Declarations to add (LaunchpadController.h)

```cpp
void sequenceDrawIndexedSequence();
void sequenceEditIndexedStep(int row, int col);
void sequenceToggleIndexedStep(int row, int col);

// Internal
void sequenceDrawIndexedPitch();
void sequenceDrawIndexedTiming();
void sequenceDrawIndexedGateSlide();
void sequenceEditIndexedPitch(int row, int col);
void sequenceEditIndexedTiming(int row, int col);
void sequenceEditIndexedGateSlide(int row, int col);
```

---

### Phase 4: DiscreteMapTrack (MEDIUM PRIORITY)

**Characteristics:**

| Property | Value |
|----------|-------|
| Stages | 32 (fixed) |
| Per-stage fields | threshold(-100..100), direction(Rise/Fall/Off/Both), noteIndex(-63..63) |
| Layers | **None** |
| Sequence props | clockSource(Internal/InternalTriangle/External), syncMode, divisor, clockMultiplier, loop, gateLength(0-100), thresholdMode(Position/Length), scale, rootNote, slewTime(0-100), pluck(-100..100), octave, transpose, offset |
| Engine API | `isActiveSequence()`, `activeStage()` (-1=none), `currentInput()`, `rampPhase()` |
| `isEdited()` | **No** |

#### 4a. Grid layout

32 stages → 4 pages × 8 columns (Left/Right to page through). Each column = one stage.

Row = value of the currently selected editing parameter.

#### 4b. Layer button → parameter selector

Tap Layer button, grid shows available editing params:

| Row | Col 0-1 |
|-----|---------|
| 0 | Threshold |
| 1 | Direction |
| 2 | Note |
| 3 | Gate Length |
| 4 | Slew Time |
| 5 | Pluck |
| 6 | — |
| 7 | — |

Tap to select which parameter to visualize/edit on the step grid.

#### 4c. Draw functions

```
sequenceDrawDiscreteMapSequence()     — dispatches by selected param
sequenceDrawDiscreteMapThreshold()    — bars showing threshold value per stage
sequenceDrawDiscreteMapDirection()    — color-coded: green=Rise, red=Fall, off=none, yellow=Both
sequenceDrawDiscreteMapNote()         — dots showing noteIndex per stage
sequenceDrawDiscreteMapGateLength()   — bar height = gateLength%
sequenceDrawDiscreteMapSlewTime()     — bar height = slewTime%
sequenceDrawDiscreteMapPluck()        — bar showing bipolar pluck value
```

#### 4d. Edit handler

```
sequenceEditDiscreteMapStep()         — dispatches by selected param
```

Tap a column to set the value of the currently selected param for that stage. Row maps to value range.

#### 4e. Current stage highlighting

Use `engine.activeStage()` — returns stage index, or -1 if no stage active. Highlight the active stage's column. Also show `rampPhase()` as a position bar on the grid.

#### 4f. FirstStep/LastStep/LoadMode

DiscreteMapSequence has no firstStep/lastStep/runMode. These buttons are either:
- Repurposed for discrete-map-specific controls (clockSource, thresholdMode)
- Or skip them entirely (buttons do nothing in DiscreteMap mode)

#### 4g. Declarations (LaunchpadController.h)

```cpp
void sequenceDrawDiscreteMapSequence();
void sequenceEditDiscreteMapStep(int row, int col);

// Per-param draw/edit
void sequenceDrawDiscreteMapThreshold();
void sequenceDrawDiscreteMapDirection();
void sequenceDrawDiscreteMapNote();
void sequenceDrawDiscreteMapGateLength();
void sequenceDrawDiscreteMapSlewTime();
void sequenceDrawDiscreteMapPluck();
void sequenceEditDiscreteMapThreshold(int row, int col);
void sequenceEditDiscreteMapDirection(int row, int col);
void sequenceEditDiscreteMapNote(int row, int col);
void sequenceEditDiscreteMapGateLength(int row, int col);
void sequenceEditDiscreteMapSlewTime(int row, int col);
void sequenceEditDiscreteMapPluck(int row, int col);
```

---

### Phase 5: TuesdayTrack — Step Key Performance Grid (HIGH PRIORITY)

**Characteristics:**

| Property | Value |
|----------|-------|
| Steps | Algorithmically generated (no step array) |
| Per-step data | **None** — steps are computed by algorithm |
| Layers | **None** |
| Physical step keys | **16 buttons** already mapped as a 2×8 grid of parameter shortcuts on the hardware |
| Engine API | `reseed()`, `reset()`, `restart()`, `currentStep()` |
| `isEdited()` | **No** |

#### 5a. Existing Step Button Map (Physical Hardware → Launchpad 1:1)

The hardware's 16 step buttons are ALREADY a 2×8 grid. It maps perfectly to Launchpad rows 0-1:

**Row 0 (top) = hardware steps 0-7 (increment):**

| Col | Step | Param | Action |
|-----|------|-------|--------|
| 0 | 0 | **Octave** | +1 (`sequence.editOctave(1, false)`) |
| 1 | 1 | **Transpose** | +1 (`sequence.editTranspose(1, false)`) |
| 2 | 2 | **Root Note** | +1 (`sequence.editRootNote(1, false)`) |
| 3 | 3 | **Divisor** | Next straight divisor |
| 4 | 4 | **Divisor T** | Next triplet divisor |
| 5 | 5 | **Divisor** | /2 (halve clock) |
| 6 | 6 | **Mask Param** | +1 (`sequence.editMaskParameter(1, false)`) |
| 7 | 7 | **Loop Length** | +1 (`sequence.editLoopLength(1, false)`) |

**Row 1 (bottom) = hardware steps 8-15 (decrement):**

| Col | Step | Param | Action |
|-----|------|-------|--------|
| 0 | 8 | **Octave** | -1 (`sequence.editOctave(-1, false)`) |
| 1 | 9 | **Transpose** | -1 (`sequence.editTranspose(-1, false)`) |
| 2 | 10 | **Root Note** | -1 (`sequence.editRootNote(-1, false)`) |
| 3 | 11 | **Divisor** | Previous straight divisor |
| 4 | 12 | **Divisor T** | Previous triplet divisor |
| 5 | 13 | **Divisor** | ×2 (double clock) |
| 6 | 14 | **Mask Param** | -1 (`sequence.editMaskParameter(-1, false)`) |
| 7 | 15 | **Loop Length** | -1 (`sequence.editLoopLength(-1, false)`) |

**Special hold actions:**

| Trigger | Action |
|----------|--------|
| Shift+Step7 (HOLD) | **Jam Run** — mute gate output while held (saves/restores `runGate()`) |
| Shift+Step15 (PRESS) | **Reset** — `engine.reset()`, shows "RESET" message |
| Shift+F5 | **Reseed** — `engine.reseed()` RNG |

**QuickEdit (Page+Step) actions (secondary overlay):**

| Page+Step | Action |
|-----------|--------|
| 8 | Copy params to clipboard 1 |
| 9 | Copy to clipboard 2 |
| 10 | Copy to clipboard 3 |
| 11 | Paste from clipboard 1 |
| 12 | Paste from clipboard 2 |
| 13 | Paste from clipboard 3 |
| 14 | **Randomize** all sequence params |

**Context menu actions:**

| Action | Method |
|--------|--------|
| INIT | `sequence.clear()` |
| RESEED | `engine.reseed()` |
| RAND | `randomizeSequence()` |
| COPY | `copySequenceParams(0)` |
| PASTE | `pasteSequenceParams(0)` |

#### 5b. Launchpad Grid Layout

**Default view (Rows 0-1):** Step-key parameter grid — identical 1:1 mapping.

Rows 2-7 are freed up for secondary displays:
- **Rows 2-3:** Full parameter editor (same as the parameter pages from the original plan). Navigate with Up/Down to switch between Core/Timing/Output pages.
- **Rows 4-7:** Algorithm overview — show current algorithm name and live step position. Could show a visual representation of the generated pattern.

**Button remapping:**

| Button | Action |
|--------|--------|
| **Navigate** | Enter grid navigation (scroll between param edit / parameter editor views) |
| **Layer** | Select current edit target (switch which row is primary) |
| **FirstStep** | Reseed RNG |
| **LastStep** | Reset engine |
| **RunMode** | Cycle through algorithms |
| **Fill** | JAM RUN toggle (momentary mute gate) |
| **Shift+Fill** | Enter Macro Grid (Init/Reseed/Rand/Copy/Paste) |

#### 5c. Declarations (LaunchpadController.h)

```cpp
void sequenceDrawTuesdayStepKeys();
void sequenceDrawTuesdayParameters();  // row 2-3 parameter editor
void sequenceDrawTuesdayAlgorithm();   // row 4-7 algorithm overview
void sequenceEditTuesdayStepKey(int row, int col);
void sequenceEditTuesdayParameter(int paramIndex, int value);
void sequenceHoldTuesdayAction(int row, int col, bool held);  // Jam Run hold/release
```

#### 5d. Effort: ~2h

- 2×8 step-key grid direct mapping + ~2 hours of integration
- Parameter editor on rows 2-7
- Special hold actions (Jam Run, Reset)
- QuickEdit clipboard overlay via Page toggle

---

---

## Summary: Function Count Per Track Type

| Track | New draw functions | New edit functions | Handler in .h | Switches modified |
|-------|:-----------------:|:-----------------:|:-------------:|:-----------------:|
| Note (fix) | 0 | 0 | 0 | 3 (currentStep, firstStep/lastStep, layerMap) |
| Curve (expand) | 1-2 | 0 | 1-2 | 0 (add param sub-mode internally) |
| **Macro Grid** | 4 | 1 | 5 | 1 (Shift+Fill check in sequenceDraw) |
| Indexed | 4 | 4 | 4 | 11 |
| DiscreteMap | 7 | 7 | 7 | 11 |
| Tuesday | 3 | 3 | 4 | 11 |

**Total new declarations:** ~31 methods in LaunchpadController.h
**Total new code:** ~1000–1400 lines in LaunchpadController.cpp (current: 974 lines, estimated final: ~1800–2200 lines)

---

## Implementation Order (Phased)

| Phase | Track | Effort | Value |
|=======|=======|========|=======|
| 1 | NoteTrack fixes | ~1.5h | Critical — fixes broken layers, step highlighting |
| 2 | **Macro Grid** | ~3h | **High** — batch ops: Curve (12), DiscreteMap (16) |
| 3 | CurveTrack chaos params | — | **DEFERRED** — low value, revisit later |
| 4 | IndexedTrack full | ~3.5h | High — pitch/timing/gate-slide grid editing, 48 steps |
| 5 | DiscreteMapTrack full | ~3h | High — 32-stage grid editor, 7 param views |
| 6 | TuesdayTrack step keys | ~2.5h | **High** — 2×8 step-key grid (1:1 HW map) + parameter editor |

**Total: ~10–13h**

### Incremental Deliverables

| Milestone | When | What works |
|-----------|------|------------|
| M1 | After Phase 1 | Note track fully usable (all 19 layers, correct highlighting, modes) |
| M2 | After Phase 2 | All macros available on Launchpad (populate/transform/clear/randomize) |
| M3 | After Phase 5 | Indexed + DiscreteMap have full grid editing |
| M4 | After Phase 6 | **Tuesday step-key grid** — full 1:1 HW parity |
| M5 | After Phase 5 | **All 5 track types** fully supported on Launchpad grid |

---

## Re-examination: mebitek-performer Patterns (Round 2)

Re-examined `temp-ref/mebitek-performer/controllers/launchpad/LaunchpadController.cpp` (3240 lines) with fresh context. Key findings:

### What mebitek does BETTER — adopt these

| Finding | Detail | Action |
|---------|--------|--------|
| **No sub-mode selector needed** | All 5 track types dispatch through the same Layer button. Each has its own `LayerMapItem[]` array. No "edit mode" concept — the Layer IS the edit mode. | Drop our sub-mode idea for Indexed/DiscreteMap. Instead, give them their own `LayerMapItem[]` arrays where each "layer" is a parameter view. |
| **All tracks have `isEdited()`** | Note, Curve, Stochastic, Logic all implement `isEdited()` for pattern page indicators. | Add `isEdited()` to DiscreteMapSequence, IndexedSequence, TuesdaySequence. |
| **Navigation state is minimal** | Only stores `Navigation` (col, row, left, right, bottom, top) per mode. No per-track cache. All selection state is in Project. | Keep our `_sequence.navigation` model. No additional state needed. |
| **Performer mode paints all tracks** | Two-button tap sets firstStep/lastStep across all 8 tracks simultaneously. Release restores originals. Layer 1 shows 8-track step overview. | Worth porting whole performer mode after phases 5-8. ~3h extra. |
| **User settings control Launchpad** | `LaunchpadNoteStyle` (classic/circuit), `LaunchpadStyleSetting` (classic/blue), `PatternChange` (immediate/sync). Default note style = circuit. | Add `LaunchpadNoteStyle` setting. Circuit keyboard as default for Note track. |

### What mebitek does NOT have — we're ahead

| Missing in mebitek | Our plan |
|--------------------|----------|
| No Macro Grid | We have 48 macros for Curve (12), DiscreteMap (16), Indexed (20) |
| No chaos/wavefolder params | We expose them |
| No Tuesday-style algorithmic params | We have full Tuesday step-key grid |
| No DiscreteMap stage editing | We have 7-param stage grid |

| No ReRene/Ikra modes | We fix NoteTrack for these |
| No CALL_MODE_FUNCTION for Macro | We add macro sub-mode via Shift+Fill |

### Drawing primitives — same as ours, nothing new to borrow

mebitek's `drawBar()`, `drawBarH()`, `stepColor()`, and the Bits/Bars/Dots/Notes dispatch pattern are **identical** to our existing code. No primitives to port — ours already work.

### Revised approach for IndexedTrack and DiscreteMapTrack

Instead of a "edit mode selector" repurposing the Layer button, follow mebitek's pattern:

**Give each track type its own `LayerMapItem[]` array** where each entry maps a parameter-view to a grid position:

```cpp
// IndexedTrack: each "layer" = a visualization mode
static const LayerMapItem indexedSequenceLayerMap[] = {
    [0] = { 0, 0 },  // Pitch view
    [1] = { 1, 0 },  // Duration view
    [2] = { 2, 0 },  // Gate/Slide view
    [3] = { 3, 0 },  // Groups view
};

// DiscreteMap: each "layer" = a parameter view
static const LayerMapItem discreteMapSequenceLayerMap[] = {
    [0] = { 0, 0 },  // Threshold view
    [1] = { 1, 0 },  // Direction view
    [2] = { 2, 0 },  // Note view
    [3] = { 3, 0 },  // Gate Length view
    [4] = { 4, 0 },  // Slew Time view
    [5] = { 5, 0 },  // Pluck view
};
```

The Layer button behavior stays consistent: tap Layer → grid shows available views → tap to select. The drawing code dispatches on the "selected layer" (which is really a view index). This eliminates the need for an extra `_editMode` state variable.

**New requirement:** Add `_selectedIndexedSequenceLayer` and `_selectedDiscreteMapSequenceLayer` to LaunchpadController state (since Project.h doesn't track these — the tracks don't have real Layer enums). Or better: use a simple `int` in `_sequence` state struct.

### Updated state additions

```cpp
struct {
    Navigation navigation = { 0, 0, 0, 7, 0, 0 };
    int selectedLayer = 0;  // used by tracks without native Layer enums
} _sequence;
```

---

## Feasibility / Priority / Risk Review

### Overall: CONDITIONAL GO — with modifications

The plan is architecturally sound, but several phases are mis-scoped, some method references are incorrect, and IndexedSequence macros don't exist in the model. **Cut Phase 2 (Curve chaos) as low-value, defer Indexed macros, and split the monolithic controller file.**

### Per-Phase Matrix

| Phase | Track | Feasibility | Priority | Risk | Effort (Plan) | Effort (Corrected) |
|-------|-------|:-----------:|:--------:|:----:|:-------------:|:------------------:|
| 1 | Note fixes | High | **Critical** | Low | ~1h | **~1.5h** |
| 2 | Curve chaos | Medium | Low | Medium | ~1h | **DEFER** |
| 2.5 | Macro Grid | High (Curve/DMap) / Low (Indexed) | **High** | High | ~2h | **~3h** (Curve+DMap only) |
| 3 | Indexed | High | **High** | Low-Med | ~3h | **~3.5h** (grid only, no macros) |
| 4 | DiscreteMap | High | **High** | Low | ~3h | ~3h |
| 5 | Tuesday | High | **High** | Low-Med | ~2h | **~2.5h** |

**Corrected total: ~10–13h** (plan says 17h — close, but only if Indexed macros deferred and Teletype removed)

### Reordered Phases (recommended)

| New Order | Phase | Rationale |
|-----------|-------|-----------|
| 1 | Note fixes | Critical regression fixes first. |
| 2 | Tuesday step keys | High value, low complexity. Good early win. |
| 3 | DiscreteMap full | High value, mechanical — builds confidence. |
| 4 | Indexed grid only | Skip macros. Get the 48-step grid working first. |
| 5 | Curve chaos | **Deferred.** Low value, add later if requested. |
| 7 | Macro Grid v2 | **New phase:** Refactor Indexed macros into model, then add to Launchpad. |

### Top Risks and Mitigations

| # | Risk | Severity | Mitigation |
|---|------|:--------:|------------|
| 1 | **Indexed macros don't exist in the model.** They're ~700 lines of UI code in `IndexedSequenceEditPage.cpp` (2,304 lines total; macros start at line ~1605). Plan claims "no model changes needed" — this is **false**. | 🔴 High | **Defer Indexed macros.** Ship basic Indexed grid editing first. Create follow-up task to refactor macros into `IndexedSequence` model methods. Do NOT duplicate macro logic into LaunchpadController. |
| 2 | **LaunchpadController.cpp will explode from 974 → ~2000 lines.** A 2000-line file with 14 dispatch points × 5 track types is unmaintainable and slows compilation. | 🟡 Medium | **Split the file BEFORE starting.** Create per-track files: `LaunchpadNoteController.cpp`, `LaunchpadCurveController.cpp`, etc. Keep dispatch in main file, move per-track logic out. |
| 3 | **Incorrect method names in plan.** `transformSmooth`, `transformWalk`, `transformRandom` listed but only `transformSmoothWalk` exists. `clearNotes()` **does exist** but `clearDirs()` does not. | 🟡 Medium | Audit all macro method names against headers before coding. Use `clearNotes()` directly; for `clearDirs()` use `randomizeDirections()` or add trivial method. |

### API Verification: Confirmed vs Missing

**✅ Confirmed to exist:**
- `NoteTrackEngine::currentNoteStep()`, `NoteSequence::mode()`, `noteFirstStep()`, `noteLastStep()`, `divisorY*()`
- `DiscreteMapTrackEngine::activeStage()`, `currentInput()`, `rampPhase()`
- `TuesdayTrackEngine::reseed()`, `reset()`, `restart()`
- `TuesdaySequence::editOctave()`, `editTranspose()`, `editRootNote()`, `editMaskParameter()`, `editLoopLength()`
- `CurveSequence::populateWithMacroInit/Fm/Damp/Bounce`, `populateWithRasterizedShape`
- `CurveSequence::transformInvert/Reverse/Mirror/Align/SmoothWalk`
- `DiscreteMapSequence::clear()`, `clearThresholds()`, `randomize()`, `randomizeThresholds()`, `randomizeNotes()`, `randomizeDirections()`
- All `Project.h` `selectedXxxSequence()` accessors

**❌ Do NOT exist (plan errors):**

| Plan Reference | Actual Status | Fix |
|----------------|---------------|-----|
| `transformSmooth` | ❌ Only `transformSmoothWalk` exists | Use `transformSmoothWalk` |
| `transformWalk` | ❌ Does not exist | Use `transformSmoothWalk` |
| `transformRandom` | ❌ Does not exist | Use `populateWithRandomMinMax` or skip |
| `clearNotes()` (DiscreteMap) | ✅ **EXISTS** (DiscreteMapSequence.h:337) | Use directly — plan was wrong |
| `clearDirs()` (DiscreteMap) | ❌ Does not exist | Use `randomizeDirections()` or add trivial method |
| Indexed macros (20 listed) | ❌ None exist in model. Only in UI page. | Refactor into model first, or defer |

**⚠️ `isEdited()` status:**
- `NoteSequence::isEdited()` — ✅ exists
- `CurveSequence::isEdited()` — ✅ exists
- `DiscreteMapSequence::isEdited()` — ❌ **missing** (~15 min to add)
- `IndexedSequence::isEdited()` — ❌ **missing** (~15 min to add)
- `TuesdaySequence::isEdited()` — ❌ **missing** (~15 min to add)

Adding `isEdited()` is safe — it's computed, no backing field, doesn't affect serialization.

### Resource Impact

| Resource | Current | Impact | Verdict |
|----------|---------|--------|---------|
| **Flash (.text)** | ~413 KB / 1 MB | +~30–50 KB for ~1600 lines | ✅ Safe. Reaches ~460 KB (46%). |
| **RAM (CCM)** | ~21 KB free | +~16 bytes (new state vars) | ✅ Negligible. |

### Pre-Implementation Tasks (30 min)

1. **Add `isEdited()`** to `DiscreteMapSequence`, `IndexedSequence`, `TuesdaySequence`.
2. **Fix plan method names:** `transformSmoothWalk` (not `transformSmooth`/`transformWalk`), remove `transformRandom`, fix DiscreteMap clear macro names.
3. **Split `LaunchpadController.cpp`** into per-track files before writing new code.

---

## Track Summary

| Track | Grid Type | # Cells | Approach |
|-------|-----------|:-------:|----------|
| **Note** | Step grid + Layer selector | 19 layers | Fix regressions: 6 missing layers, mode highlighting, Ikra/ReRene |
| **Curve** | Step grid + Layer selector + Macros | 7 layers | 12 macros via Shift+Fill; chaos params deferred |
| **Indexed** | 4-view grid + Macros (deferred) | 48 steps | Pitch/Duration/GateSlide/Route views; rotation offset |
| **DiscreteMap** | 6-view stage grid + Macros + Range | 32 stages | Threshold/Direction/Note/GateLen/Slew/Pluck views; 16 macros |
| **Tuesday** | **2×8 step-key grid** | 16 steps | 1:1 HW parity — Octave, Transpose, Root, Divisor, Mask, Loop shortcuts + Jam Run hold + Reseed |


---

## Codebase Verification (2026-05-04)

All API references audited against source. Corrections applied inline above.

### Verified ✅

| API | File:Line |
|-----|-----------|
| `NoteTrackEngine::currentNoteStep()` | NoteTrackEngine.h:68 |
| `NoteSequence::mode()` (Linear/ReRene/Ikra) | NoteSequence.h:63-66, 588 |
| `NoteSequence::noteFirstStep()` | NoteSequence.h:662 |
| `NoteSequence::noteLastStep()` | NoteSequence.h:680 |
| `DiscreteMapTrackEngine::activeStage()/currentInput()/rampPhase()` | DiscreteMapTrackEngine.h:52-54 |
| `TuesdayTrackEngine::reseed()/reset()/restart()` | TuesdayTrackEngine.h |
| `TuesdaySequence::editOctave/editTranspose/editRootNote/editMaskParameter/editLoopLength` | TuesdaySequence.h |
| All 4 `CurveSequence::populateWith*` macros | CurveSequence.h:588-592 |
| All 6 `CurveSequence::transform*` methods | CurveSequence.h:595-599 |
| `DiscreteMapSequence::clearNotes()` | DiscreteMapSequence.h:337 |
| `DiscreteMapSequence::clearThresholds()/randomize()/randomizeThresholds()/randomizeNotes()/randomizeDirections()` | DiscreteMapSequence.h:334-341 |

### Corrected ❌→✅

| Claim | Reality |
|-------|---------|
| `clearNotes()` doesn't exist | **EXISTS** — DiscreteMapSequence.h:337 |
| Indexed macros "500+ lines" | ~700 lines of UI code (IndexedSequenceEditPage.cpp lines 1605-2304) |
| `transformSmooth` / `transformWalk` | Only `transformSmoothWalk` exists |
| `transformRandom` | Only `populateWithRandomMinMax` exists |
| `clearDirs()` | Does not exist — use `randomizeDirections()` or add trivial method |

### Resource Verification

| Resource | Claim | Verified |
|----------|------|----------|
| Flash | 1 MB total, ~413 KB used | ROM=1024K (stm32f405.ld); .text needs build map to verify |
| SRAM | 128 KB total | ✅ stm32f405.ld:7 |
| CCM RAM | 64 KB total | ✅ stm32f405.ld:8 |
| LaunchpadController.cpp | 974 lines | ✅ 974 lines, 33,081 bytes |
| Model ~93 KB | Cannot verify without build — plausible given data layout | |
| Ui framebuffer | AGENTS.md says 8 KB | ⚠️ **CONFIG_LCD_WIDTH=256**, so framebuffer = 16 KB (256×64), not 128×64 |
| LCD resolution | AGENTS.md says 128×64 | ⚠️ **256×64** per SystemConfig.h |

---

## TDD Strategy

### Existing Infrastructure

| Component | Location | Pattern | CI |
|-----------|----------|---------|-----|
| C++ unit tests | `src/tests/unit/sequencer/` (43+ files) | `UNIT_TEST` + `CASE` + `expect/expectEqual` | CTest on Linux/macOS |
| C++ integration tests | `src/tests/integration/drivers/` (11 files) | `IntegrationTest` base + `Simulator` + SDL | On-device only, not in CI |
| Python UI tests | `src/apps/sequencer/tests/` | `UiTest` + `Controller` fluent API + pybind11 | Not in CI |
| Test framework headers | `src/test/UnitTest.h`, `IntegrationTest.h` | Custom, no external deps | |
| pybind11 module | `src/apps/sequencer/python/` | `Environment` → `Simulator` + `SequencerApp` | |

**C++ unit test pattern** (most common):
```cpp
UNIT_TEST("TestName") {
    CASE("description") {
        NoteSequence seq;
        seq.clear();
        expectEqual(int(seq.step(0).gate), 0, "default gate");
    }
}
```
Built via `register_sequencer_test()`, links against `core` + `sequencer_shared`, run by CTest.

**Python test pattern**:
```python
class MyTest(tf.UiTest):
    def test_something(self):
        c = self.controller
        c.press("step1").wait()
        p = self.env.sequencer.model.project
        self.assertTrue(p.track(0).noteTrack.sequences[0].step(0).gate)
```
Full model access via pybind11. **Cannot verify LEDs/gates/DAC** (TargetState not Python-bound).

### LaunchpadController Is NOT Unit-Testable (Current State)

Blocking dependencies:
1. `LaunchpadDevice` has no virtual interface — cannot inject mock
2. `Controller::sendMidi()` routes through `ControllerManager` → `MidiPort`
3. Constructor auto-selects device variant and sends SysEx MIDI
4. LED writes go to `_device->setLed()` then `syncLeds()` sends MIDI
5. Button state read from `_device->buttonState(row, col)`

### Recommended TDD Approach: Pragmatic 3-Layer

**Layer 1 — Extract pure-logic helpers** (before implementation, ~1h):

Move to `LaunchpadLogic.h/.cpp`:
- `Button` struct methods (`isGrid()`, `isFunction()`, `gridIndex()`)
- `Navigation` calculations
- `RangeMap::map()`/`unmap()`
- `stepColor()` and LED-value computation

These are pure functions — immediately unit-testable with existing `UNIT_TEST` framework.

**Layer 2 — C++ model/engine unit tests** (during implementation, ~1h):

For each phase, add `TestLaunchpadLogic.cpp`:
- Note: layer math, step highlighting, `noteFirstStep`/`noteLastStep` in Ikra mode
- DiscreteMap: stage navigation, `isEdited()` verification
- Indexed: step access patterns, `isEdited()` verification
- Tuesday: edit parameter clamping

Construct `Model`/`Project`/`Engine` on stack (established pattern), configure track mode, assert behavior.

**Layer 3 — Python integration tests** (after TargetState bindings, ~1h per phase):

Requires binding `TargetState`, `LedState`, `GateOutputState`, `DacState` to pybind11 (~1.5h infra). After that:
- Simulate Launchpad MIDI input → verify LED output via MIDI capture
- Simulate encoder rotations → verify model state
- Screenshot regression tests for LCD pages

### Phase-Specific Test Plan

| Phase | C++ Unit Tests | Python Integration Tests |
|-------|---------------|--------------------------|
| 1 (Note fixes) | `TestNoteSequence` — layer math, step highlight, Ikra bounds | Press Note layer buttons → verify correct layer selected in model |
| 2 (Tuesday step keys) | `TestTuesdaySequence` — parameter clamping, boundary values | Press step keys → verify correct parameter edits via model |
| 3 (DiscreteMap) | `TestDiscreteMapSequence` — stage threshold/note/direction round-trip | Navigate stages → verify stage data visible in model |
| 4 (Indexed) | `TestIndexedSequence` — `isEdited()`, step access, layer switching | Navigate 48 steps → verify pitch/duration/gate display |

| 7 (Macro Grid) | Verify macro invocation maps to correct model method | Press macro buttons → verify Curve/DMap sequence transforms |

### TDD Infrastructure Tasks (adds ~3h total)

| # | Task | Effort | Blocks |
|---|------|--------|--------|
| T1 | Bind `TargetState`/`LedState`/`GateOutputState`/`DacState` to pybind11 | ~1.5h | All Python verification |
| T2 | Add MIDI output capture ring buffer accessible from Python | ~0.5h | Launchpad LED feedback tests |
| T3 | Extract pure-logic helpers from `LaunchpadController.cpp` to `LaunchpadLogic.h/.cpp` | ~1h | C++ unit tests for Button/Navigation/stepColor |
