# Generator System

The Generator system provides algorithmic tools for creating and modifying Note, Curve, and Indexed sequence data. Accessed via long-press on the **GEN** button in a sequence edit page, generators produce deterministic output and support an A/B preview workflow to audition changes before committing. The available generator types depend on the track: Note and Curve tracks offer the **Algo** generator, while Indexed tracks offer **Helical** in its place.

---

## Contents

1. [Accessing Generators](#1-accessing-generators)
2. [Generator Types](#2-generator-types)
3. [A/B Preview Workflow](#3-ab-preview-workflow)
4. [Common Controls](#4-common-controls)
5. [Random Generator](#5-random-generator)
6. [Algo Generator](#6-algo-generator)
7. [Helical Generator](#7-helical-generator)
8. [Euclidean Generator](#8-euclidean-generator)
9. [Init Layer / Init Steps](#9-init-layer--init-steps)
10. [Layer-Specific Visualization](#10-layer-specific-visualization)
11. [Context Menu](#11-context-menu)

---

## 1. Accessing Generators

From a Note or Curve sequence edit page:

- **Long-press GEN** to open the generator select page
- Choose a generator type from the list
- The generator opens in **ORIGINAL** state — no data is modified yet

Available generator types depend on the track:

| Generator | Note Track | Curve Track | Indexed Track | Description |
|---|---|---|---|---|
| Init Layer | ✅ | ✅ | ✅ | Reset current layer to default values |
| Init Steps | ✅ | ✅ | ✅ | Clear all step data |
| Euclidean | ✅ | ✅ | ✅ | Distribute N gates evenly across M steps |
| Random | ✅ | ✅ | ✅ | Randomize step values per layer |
| Algo | ✅ | ❌ | ❌ | Tuesday algorithm engines on Note sequences |
| Helical | ❌ | ❌ | ✅ | Autoregressive fold-feedback map on Indexed sequences |

Indexed tracks offer **Helical** in the slot where Note/Curve tracks offer **Algo**.

---

## 2. Generator Types

### Init Layer
Resets the current edit layer to its default value across all steps (or selected steps). No A/B workflow — applies immediately.

### Init Steps
Clears all step data (gate, note, slide, etc.) across all steps (or selected steps). No A/B workflow — applies immediately.

### Euclidean
Generates a Euclidean rhythm pattern: distributes N gates as evenly as possible across M steps with an offset. Supports A/B preview.

**Parameters:**
| Param | Function | Description |
|---|---|---|
| Steps | F2+encoder | Total number of steps in the pattern |
| Beats | F3+encoder | Number of gates to distribute |
| Offset | F1+encoder | Rotate the pattern |

### Random
Generates random values for the current edit layer. Seed-driven and deterministic — same seed always produces the same output. Supports A/B preview.

**Parameters:**
| Param | Function | Description |
|---|---|---|
| Seed | F0 (A/B toggle) | 32-bit seed value. Displayed in footer |
| VAR | F1+encoder | Variation 0-100%. 100% = fully random, 0% = unchanged |
| RANGE | F2+encoder | Scale range for generated values |
| BIAS | F3+encoder | Bias offset from center |

**Visualization:** Draws a profile graph showing the generated values for the current layer. The graph updates per layer type (gate blocks, note contour, length bars, slide markers, etc.).

### Algo
**Note track only.** Runs one of 15 Tuesday algorithmic composition engines against the sequence, producing gate, note, slide, gate offset, gate length, retrigger, and gate probability values in a single pass. Each algorithm is a self-contained compositional state machine. Supports A/B preview.

**Parameters:**
| Param | Function | Description |
|---|---|---|
| Seed | F0 (A/B toggle) | 32-bit seed mixed into algorithm RNG |
| ALGO | F1+encoder | Algorithm selection 0-14 |
| FLOW | F2+encoder | Flow parameter 0-16 (seeds main RNG, drives gesture/mode decisions) |
| ORNMT | F3+encoder | Ornament parameter 0-16 (seeds aux RNG, drives subdivisions) |
| POWER | F4+encoder | Gate density 0-16 (higher = more gates) |

**Algorithms:**

| # | Name | Character |
|---|---|---|
| 0 | TEST | Simple test patterns: octave sweeps or scale walk |
| 1 | TRITRANCE | 3-phase polyrhythm with swing and drag |
| 2 | STOMPER | 7-mode gestural state machine with slide transitions |
| 3 | MARKOV | Order-2 Markov chain, 8×8×2 probability matrix |
| 4 | CHIP1 | Chord-arpeggio pattern with chord stabs |
| 5 | CHIP2 | Variable-length ascending/descending arpeggio |
| 6 | WOBBLE | Dual-phasor oscillator with phase-selected notes |
| 7 | SCALEWLK | Scale direction walker with polyrhythm subdivisions |
| 8 | WINDOW | Dual-phase Markov-arpeggio engine, 4 voices |
| 9 | MINIMAL | Burst/silence state machine with density control |
| 10 | GANZ | Triple-phasor engine with 5-tuplet grid and phrase skipping |
| 11 | BLAKE | Breath-phrase engine with 4-note stable motif |
| 12 | APHEX | 3-track polyrhythmic engine (4/4, warped, 5/4) |
| 13 | AUTECHRE | 8-step pattern with rule-based transformation |
| 14 | STEPWAVE | Scale-degree stepping engine with micro-sequencing |

**Mapped layers (applied per-step, all at once):**
- **Gate**: `velocity > 0 && gateRatio > 0`, modulated by POWER cooldown
- **Note**: Scale degree from algorithm output
- **Slide**: Algorithm-specific slide probability (modulated by VAR)
- **GateOffset**: Timing offset from algorithm
- **Length**: Gate length ratio from algorithm
- **Retrigger**: Trill count or polyrhythm subdivision count
- **GateProbability**: Velocity-based probability; accents set to 100%

**Variation blend:** Each layer independently rolls against the VAR parameter — at 100% all generated values are used, at 0% all original values are kept.

### Helical
**Indexed track only.** Runs a deterministic coupled autoregressive (AR) fold-feedback map, ported from Nebulae 2's helical generator. Fills each step's note index, duration, and gate length from the feedback (duration dominates the feedback, ~35:1), reading the resolved scale/root off the Indexed sequence. Indexed tracks offer Helical in place of Algo. Supports A/B preview.

**Parameters:**
| Param | Function | Description |
|---|---|---|
| Seed | F0 (A/B toggle) | Re-roll for new initial conditions (deterministic per seed) |
| Octave Range | F1+encoder | Octave span, 1–4 (default 2) |
| Base | F2+encoder | Feedback base, 24–768 (default 192) |
| Law Dir | F3+encoder | Law/fold direction, ±1 |
| Helicity | F4+encoder | Law depth ×10, 1–20 (default 8 → 0.8) |

---

## 3. A/B Preview Workflow

Generators with A/B support (Random, Euclidean, Algo) follow a two-state workflow:

```
              ┌─────────────────┐
              │    ORIGINAL     │  ← generator opens here
              │ (sequence data) │
              └────────┬────────┘
                       │
            Turn encoder OR
           Press Shift+Step
                       │
                       ▼
              ┌─────────────────┐
              │    PREVIEW      │
              │  (generated)    │
              └────────┬────────┘
                       │
            ┌──────────┴──────────┐
            │                     │
         Press F0            Context menu
            │                     │
            ▼                     ├── REGEN → restart with fresh params
    ┌───────────────┐             ├── CANCEL → discard, return to original
    │   Toggle:     │             └── COMMIT → accept preview
    │ ORIGINAL ◄──► │
    │   PREVIEW     │
    └───────────────┘
```

**Workflow steps:**

1. **Enter**: Generator opens in ORIGINAL state. No data is modified. Footer shows "ORIGINAL" under the A/B label.

2. **Generate preview**: Turn the encoder (with no function key held), or press Shift+Step. The generator creates a preview and switches to PREVIEW state. Footer shows the seed value or "CURRENT."

3. **Toggle A/B**: Press **F0** to switch between ORIGINAL and PREVIEW. The footer label updates to show which state is active.

4. **Adjust parameters**: Hold **F1-F4** and turn the encoder to adjust generator parameters in real time. The preview updates automatically.

5. **Commit or Cancel**:
   - **Context menu → COMMIT**: Accept the preview. The generated data replaces the original sequence. The generator closes.
   - **Context menu → CANCEL**: Discard the preview. The original sequence is restored.
   - **Close without action** (e.g., press Back): Automatically discards the preview and restores the original.

6. **Re-entering**: After committing, reopening the generator shows the committed data as the new ORIGINAL. After canceling, the original data remains unchanged.

---

## 4. Common Controls

### Footer Functions (F0-F4)

| F-key | Random | Euclidean | Algo |
|---|---|---|---|
| F0 | A/B toggle | A/B toggle | A/B toggle |
| F1 | VAR | OFFSET | ALGO |
| F2 | RANGE | STEPS | FLOW |
| F3 | BIAS | BEATS | ORNMT |
| F4 | (context menu) | (context menu) | POWER |

- **A/B toggle (F0)**: Switch between original and preview state. Blocked until first preview is generated.
- **Param edit (F1-F4)**: Hold function key and turn encoder to adjust the parameter.

### Encoder

- **Bare encoder turn** (no function key): Triggers a new preview (Random = reseed, Euclidean = new pattern, Algo = reseed).
- **F1-F4 + encoder**: Adjust the corresponding parameter.

### Step Buttons

- **Shift + Step**: Generates a new preview (re-rolls the seed for Random and Algo, re-randomizes for Euclidean).
- **Step selection**: Works as in the sequence edit page — select steps to constrain generation scope.

### Section Navigation (64-step sequences)

- **Left/Right buttons**: Cycle through sections (banks) of 16 steps.
- **Bank separators**: Vertical lines at 16-step boundaries. Current bank is highlighted.
- **Bank frame**: Horizontal lines framing the active section.
- **Non-active banks**: Dimmed in the visualization.
- **Playhead**: Current playback position cursor.

### Common Context Menu Items

| Generator | Item 0 | Item 1 | Item 2 | Item 3 | Item 4 |
|---|---|---|---|---|---|
| Random | (empty) | SMOOTH N | REGEN | CANCEL | COMMIT |
| Algo | (empty) | VAR N | REGEN | CANCEL | COMMIT |
| Euclidean | NEW RAND | RESEED | REVERT | COMMIT | (empty) |

**Access**: Press **Page**, short-press **GEN**, or **double-tap Page** on the Generator page.

**Item descriptions:**
- **SMOOTH N / VAR N**: Opens a quick-edit dialog for the Smooth/Variation parameter
- **REGEN**: Resets to default parameters and returns to ORIGINAL state
- **CANCEL / REVERT**: Discard preview, restore original sequence, close
- **COMMIT**: Accept preview, write to sequence, close
- **NEW RAND**: Re-randomize all parameters and generate a new preview
- **RESEED**: Re-roll the seed only, keep current parameters, generate new preview

---

## 5. Random Generator

The Random generator operates on a single layer at a time (the currently selected layer). It generates values within the layer's valid range.

### Variation blend

The `VAR` parameter controls how much of the generated data is kept vs. how much original data survives. At 100%, all generated values are used. At 0%, all original values are kept. The blend is applied per-step — some steps may keep original values while others use generated ones.

The algorithm is seed-driven: the same seed with the same VAR/RANGE/BIAS always produces the same output.

---

## 6. Algo Generator

The Algo generator runs one of 15 Tuesday algorithms against the full NoteSequence (all active steps) in a single pass. Unlike the Random generator which operates layer-by-layer, Algo generates gate, note, slide, offset, length, retrigger, and probability in one pass.

### Gate density control (POWER)

The POWER parameter simulates a cooldown-based density gate. Higher values produce more gates per sequence;
lower values create sparser patterns. POWER=16 passes all gates, POWER=0 passes none.

### Variation blend

Like the Random generator, each layer independently rolls against the VAR parameter. A step may get the generated note but keep the original gate, or get the generated gate but keep the original note. This is set in the context menu (shortcut: F4 opens context menu, then item 1 for VAR quick edit).

### Algorithm state

Each algorithm is deterministic: the same ALGO/FLOW/ORNMT/SEED combination always produces the same sequence. The seed is mixed into the RNG seed derivation rather than modifying the FLOW or ORNMT parameters directly.

---

## 7. Helical Generator

**Indexed track only.** A deterministic coupled autoregressive (AR) fold-feedback map, ported from Nebulae 2's helical generator. It fills each step's note index, duration, and gate length from the feedback — duration dominates the feedback (~35:1) — reading the resolved scale and root off the Indexed sequence. On Indexed tracks it occupies the generator slot that Note/Curve tracks give to Algo.

**Parameters:**
| Param | Range | Default | Effect |
|---|---|---|---|
| Seed | 32-bit | — | Re-roll for new initial conditions; same seed + params is repeatable |
| Octave Range | 1–4 | 2 | Octave span of the generated note indices |
| Base | 24–768 | 192 | Feedback base scaling the map output |
| Law Dir | ±1 | +1 | Law/fold direction |
| Helicity | 1–20 | 8 | Law depth ×10 (8 → 0.8): higher = deeper fold |

Re-rolling the seed produces new initial conditions; the same seed with the same parameters always reproduces the same sequence.

---

## 8. Euclidean Generator

Generates a Euclidean rhythm: distributes `Beats` gates evenly across `Steps` positions with an `Offset` rotation.

- **STEPS**: Total positions (max 16 for Euclidean)
- **BEATS**: Number of gates to distribute
- **OFFSET**: Rotation offset for the pattern

The visualization shows filled/empty step cells: filled = gate, empty = rest.

---

## 9. Init Layer / Init Steps

- **Init Layer**: Resets the current edit layer to its default value. The default depends on the layer type (e.g., Gate = off, Note = C, etc.). If steps are selected, only those steps are affected.
- **Init Steps**: Clears all step data (gate, note, slide, layer values). If steps are selected, only those steps are affected. If no steps are selected, the entire active range is cleared.

Both are immediate actions with no preview. A confirmation dialog may appear for destructive operations.

---

## 10. Layer-Specific Visualization

The Random and Algo generators display a profile graph using the current sequence edit layer. The visualization adapts to the layer type:

| Layer | Visualization | Description |
|---|---|---|
| Gate | Gate blocks | Filled/empty rectangles per step |
| Note | Note contour | Stair-step pitch graph (2 rows: value + vertical connector) |
| Slide | Slide markers | Filled rectangles at bottom of each gated step |
| Length | Length bars | Variable-height bars proportional to length value |
| Retrigger / Pulse Count | Repeat stripes | Vertical stripes proportional to repeat count |
| GateOffset | Centered profile | Value centered on baseline (positive = above, negative = below) |
| Probability, Range, Variation, etc. | Offset profile | 0-255 value from top of plot area |
| Accumulator Trigger | Boolean markers | Small dots for enabled steps |
| Gate Mode / Harmony / Inversion / Voicing | Offset profile | Same offset profile (values may clamp to layer range) |

All visualizations include:
- **Bank separators**: Vertical lines at 16-step boundaries
- **Bank frame**: Horizontal lines framing the active section
- **Playhead**: Current playback position cursor
- **Active bank highlight**: Current bank is bright; others are dimmed

---

## 11. Context Menu

The context menu provides additional actions beyond the footer buttons.

**Opening**: Page button, short-press GEN, or double-tap Page on the Generator page.

**Random & Algo menus:**
```
┌─────────────────┐
│                 │  ← empty slot (padding)
│ SMOOTH / VAR N  │  ← opens quick-edit for smooth/variation
│ REGEN           │  ← reset to defaults, return to ORIGINAL
│ CANCEL          │  ← discard preview, restore original
│ COMMIT          │  ← accept preview
└─────────────────┘
```

**Euclidean menu:**
```
┌─────────────────┐
│ NEW RAND        │  ← re-randomize all params, generate preview
│ RESEED          │  ← re-roll seed only, generate preview
│ REVERT          │  ← discard preview, restore original
│ COMMIT          │  ← accept preview
└─────────────────┘
```

---

## Technical Notes

- **RAM**: Generators are heap-allocated when opened and freed on exit. The A/B preview buffer (~9.5 KB for NoteSequence) is also heap-allocated on first preview generation.
- **Determinism**: Random and Algo generators are fully deterministic. Same seed + same parameters = same output, across reboots.
- **Focus**: Track selection is locked while a generator is open. Changing tracks cancels the generator automatically.
