---
id: indexed-generator-helical-ar
schema: plan
title: "feat: Indexed generator framework + helical AR generator"
type: feat
status: active
date: 2026-06-22
depth: deep
---

# Indexed generator framework + helical AR generator

Two coupled pieces: (1) make the Indexed track a client of the existing **Generator framework** — a
bigger, more invasive bridge than first assumed, because the shared generator UI (`GeneratorPage`,
the `StepSelection` width, the mode picker) is Note/Curve-shaped — and (2) add the **helical
autoregressive generator** as a new Mode targeting Indexed (the Nebulae 2 helical map: deterministic
coupled fold-feedback → note + variable per-step duration).

Origin: `docs/plans/2026-06-22-indexed-helical-ar-macro-design.md` (validated brainstorm; the AR
algorithm + scope are settled there). Planning reopened the delivery decision: framework (knobs +
preview), not the preset-menu macro path — which forces the bridge below.

> **Cost note (post-review):** the bridge is materially larger than a "make Indexed a builder client
> and reuse the page" change. `SequenceBuilderImpl<T>` cannot be used for Indexed (its `firstStep()`
> is a rotation offset, not an edit-range start), and `GeneratorPage`/`StepSelection`/the picker each
> hardcode Note/Curve assumptions. Reusing them means **modifying shared generator UI that Note and
> Curve depend on** — regression surface, not just additive. See Risks.

---

## Summary

The Generator framework (`Generator` + `SequenceBuilder`, hosted by `GeneratorPage` with
paramName/editParam + preview/apply) reaches only Note and Curve today. Three concrete couplings block
Indexed:

- `SequenceBuilderImpl<T>` calls `T::firstStep()/lastStep()/setFirstStep/setLastStep/step().layerValue()/setLayerValue()/steps()` and treats `firstStep()` as the generate range start. Indexed has none of that, and its `firstStep()` means rotation.
- `GeneratorPage::show` and its member are `StepSelection<CONFIG_STEP_COUNT>` (64); Indexed owns `StepSelection<IndexedSequence::MaxSteps>` (48) — incompatible template types.
- `GeneratorPage::currentStep()`/LED draw `switch` on Note/Curve, and the value-preview hardcodes 64 steps; the mode picker casts row→Mode directly.

So Indexed gets a **bespoke builder** (not the template) plus targeted genericization of the shared
page/selection/picker, then the AR lands as a Mode. The AR map itself (deterministic, mono, coupled,
no RNG) is per the origin doc and built as a pure, unit-tested helper.

---

## Problem Frame

- **Why Indexed:** its steps carry a first-class 10-bit `duration` (variable step time) — the helical pitch↔duration coupling needs it. Note/Curve are fixed-grid.
- **Why the framework, not the macro path:** the framework owns the knob param page + non-destructive preview/apply; the macro path is preset-menu, apply-direct. The user chose the framework path knowing it's the bigger build.
- **The real gap (verified):** the framework's *engine* side (`SequenceBuilder`) and its *UI* side (`GeneratorPage`, `StepSelection`, `GeneratorSelectPage`) are both Note/Curve-shaped. The bridge must supply a bespoke Indexed builder AND genericize the shared UI without regressing Note/Curve.

---

## Key Technical Decisions

- **Bespoke `IndexedSequenceBuilder`, mandatory.** Implement the `SequenceBuilder` *base interface* directly against Indexed's real API (its step `noteIndex`/`duration`/`gateLength` accessors, its active-length range, its own preview clone). Do **not** add Note-style `firstStep`/`lastStep` to `IndexedSequence` — its `firstStep()` is rotation and must not be repurposed. The generate range comes from the apply selection / active length, passed in, not from `firstStep`.
- **AR generator writes whole steps** via `builder.editSequence()` (like `AlgoGenerator`), not the single-layer `value()/setValue()` path — the coupling needs note+duration produced together.
- **`GeneratorPage` is not type-agnostic; genericize the Note/Curve-specific points.** `currentStep()`, the LED draw, and the value-preview need an Indexed branch (and the preview must use the builder's `length()`, not `CONFIG_STEP_COUNT`). This is a change to shared UI — guard Note/Curve behavior.
- **`StepSelection` width mismatch → type-erase the selection.** Introduce a small selection-view interface (`any()`/`firstSetIndex()`/`lastSetIndex()`/count) that both `StepSelection<64>` and `<48>` satisfy; `GeneratorPage::show` takes the interface. (Alternatives: template `show` on width, or an adapter — type-erasure keeps the page concrete and is least churny for the Note callers.)
- **Mode picker needs explicit row↔Mode mapping.** Replace the `rows()=int(Mode::Last)` + `Mode(selectedRow())` cast with a per-context visible-`Mode[]` list + `modeForRow()`; the picker is given the set valid for the current track. Helical shown for Indexed; Algo stays Note-only.
- **AR map = origin doc, verbatim** (fold-feedback note, √freq duration, fold-remainder gate, seed init, no RNG, convergence guard). Scale + loop length from the track.

---

## High-Level Technical Design

Launch flow (mirrors the Note path; the new/changed surface is annotated):

```
IndexedSequenceEditPage --Generate--> GeneratorSelectPage (visible-Mode[] for Indexed)   [U7 change]
   -> create IndexedSequenceBuilder(selectedIndexedSequence(), range, layer)             [U1 new]
   -> Generator factory(mode=Helical, builder)   // guard to Indexed builder             [U4 new]
   -> GeneratorPage.show(generator, selectionView)   // selection type-erased,            [U5/U6 change]
        Indexed branches in currentStep/LEDs/preview
        apply -> builder writes the Indexed sequence
```

Shared, Note/Curve-touching changes: `GeneratorPage` (U6), the selection-view interface (U5),
`GeneratorSelectPage`/model (U7). Indexed-only new code: the builder (U1), the AR map (U2), the
generator (U3), the factory entry (U4), the Indexed-page launch (U5).

---

## Implementation Units

### U1. Bespoke IndexedSequenceBuilder

**Goal:** A hand-written `SequenceBuilder` for Indexed — the template can't be used.

**Dependencies:** none.

**Files:**
- `src/apps/sequencer/engine/generators/IndexedSequenceBuilder.h` / `.cpp` (create — implements the `SequenceBuilder` base)
- possibly `src/apps/sequencer/model/IndexedSequence.h` (only additive read/write helpers if missing; **not** firstStep/lastStep semantics)

**Approach:** Implement every `SequenceBuilder` virtual (`revert/apply/showOriginal/showPreview/showingPreview/updatePreview/originalLength/originalValue/length/setLength/value/setValue/clearSteps/copyStep/clearLayer`) against Indexed: keep `_edit`/`_original`/`_preview` Indexed copies; range = active length (or the passed apply range), **not** `firstStep()`. Expose `editSequence()` for whole-step writers. `value()/setValue()` map a chosen step field (noteIndex) ↔ float for the generic preview; the AR generator uses `editSequence()` directly. Verify Indexed's copy semantics suffice for the preview clone.

**Patterns to follow:** `SequenceBuilderImpl<T>` (the contract to satisfy) and `AlgoGenerator`'s `editSequence()` usage — but re-implemented, not instantiated, for Indexed.

**Execution note:** Characterization-first — pin the preview/apply/revert contract before the AR depends on it.

**Test scenarios:**
- preview → apply commits to the Indexed sequence; revert restores the original; `showOriginal`/`showPreview` toggle correctly.
- range is the active length / passed range; rotation `firstStep` is untouched after apply.
- `value/setValue` round-trip the chosen field at range bounds.
- Test expectation: builder round-trip via a small unit test if feasible; otherwise build + the U5/U6 manual preview exercises it.

**Verification:** Builder drives preview/apply/revert over an Indexed sequence; Indexed rotation behavior unchanged; sim + STM32 build.

---

### U2. Helical AR map — pure helper (test-first)

**Goal:** The deterministic coupled-AR step function as a standalone, unit-tested helper.

**Dependencies:** none.

**Files:**
- `src/apps/sequencer/engine/generators/HelicalAr.h` (create)
- `src/tests/unit/sequencer/TestHelicalAr.cpp` (create)
- `src/tests/unit/sequencer/CMakeLists.txt` (register)

**Approach:** `helicalArStep(pitch, dur, span, base, lawDir) → {noteIndex, durTicks, gateLen, newPitch, newDur}` per the origin doc: `raw = prevDur·W_DUR + prevPitch·W_PITCH`; `influence = fmod(raw, span)`; `noteIndex = round(influence)`; gate length from the fold remainder → 20–100%; duration = `base × law(√freq)` clamped; feed back. Borrow helical's constants (17.31 / 0.5 / 16.17). Include the convergence-guard nudge.

**Execution note:** Test-first (mirror `TuringRegister`/`TT2Transpose`).

**Test scenarios:**
- determinism: same seed+params → identical step stream.
- coupling: changing the duration-feedback weight shifts the pitch trajectory.
- octave range: `noteIndex` in `[0, octaveRange×scaleSize)`.
- law direction: higher pitch → monotonically longer (or shorter) `durTicks`.
- bounds: `durTicks` in the Indexed tick range; gate length in 20–100%.
- convergence guard: a fixed-point-prone seed doesn't collapse to one note.

**Verification:** `TestHelicalAr` green; sim + STM32 build.

---

### U3. HelicalGenerator (Generator subclass)

**Goal:** A `Generator` for `Mode::Helical` running U2 across the range, writing whole Indexed steps, with the 4 knob params.

**Dependencies:** U1, U2.

**Files:**
- `src/apps/sequencer/engine/generators/HelicalGenerator.h` / `.cpp` (create)

**Approach:** Mirror `AlgoGenerator`: take the Indexed builder, `editSequence()`, seed the AR from `seed`, read `selectedScale()` for the √freq law + degrees-per-octave, run `helicalArStep` per step writing `setNoteIndex`/`setDuration`/`setGateLength`. Params via `paramName`/`printParam`/`editParam`: octave range, base, law direction, seed (+ `randomizeSeed`). Deterministic preview/regenerate.

**Patterns to follow:** `AlgoGenerator`.

**Test scenarios:**
- same params+seed → identical generated Indexed sequence (determinism).
- octave-range/base params change span and step timing.
- Test expectation: map correctness from U2; wiring is build + manual preview.

**Verification:** Generating with Helical fills the Indexed sequence; params edit and re-preview; reproducible.

---

### U4. Register + guard the Helical mode

**Goal:** `Mode::Helical` in the factory, guarded to the Indexed builder.

**Dependencies:** U3.

**Files:**
- `src/apps/sequencer/engine/generators/Generator.h` / `.cpp` (enum value, `modeName`, factory case + builder-type guard)

**Approach:** Add the enum + name; factory instantiates `HelicalGenerator` only for the Indexed builder (mirror the `Algo`→`NoteSequenceBuilder` guard at `Generator.cpp:35`). Adding an enum value shifts `Mode::Last`; confirm no code assumes a fixed mode count beyond the picker (handled in U7).

**Test scenarios:**
- factory returns HelicalGenerator for Indexed builder + Helical; guarded/null otherwise.
- Test expectation: build + U5 launch check.

**Verification:** Factory builds; Helical resolves only for Indexed.

---

### U5. Indexed launch + type-erased selection

**Goal:** Launch the generator from the Indexed page; resolve the `StepSelection` width mismatch.

**Dependencies:** U1, U4, U7.

**Files:**
- `src/apps/sequencer/ui/StepSelection.h` (add a width-agnostic selection-view interface the templates expose)
- `src/apps/sequencer/ui/pages/GeneratorPage.h` / `.cpp` (`show` takes the view, not `StepSelection<CONFIG_STEP_COUNT>*`)
- `src/apps/sequencer/ui/pages/IndexedSequenceEditPage.cpp` / `.h` (Generate entry → select → build → show)
- `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp`, `CurveSequenceEditPage.cpp` (pass the view — the regression touch)

**Approach:** Introduce a small selection-view interface (`any()`/`firstSetIndex()`/`lastSetIndex()`/count) implemented by `StepSelection<N>` for any `N`; change `GeneratorPage` to hold/accept it. Update the Note/Curve callers to pass the view (behavior-neutral for them). Wire the Indexed page's Generate launch mirroring `NoteSequenceEditPage.cpp:1163`, range from its step selection / active length.

**Execution note:** Behavior-neutral for Note/Curve — diff those callers to confirm only the selection-pass changed.

**Test scenarios:**
- Indexed: Generate → picker → preview → apply writes the sequence; selection range scopes the fill.
- Note/Curve regression: their generator launch + range still behave identically.
- Test expectation: build + manual sim (both Indexed and a Note generator).

**Verification:** Indexed end-to-end works; Note/Curve generators unaffected; sim + STM32 build.

---

### U6. GeneratorPage Indexed support (currentStep / LEDs / preview)

**Goal:** The shared generator page renders and tracks an Indexed sequence correctly.

**Dependencies:** U1, U5.

**Files:**
- `src/apps/sequencer/ui/pages/GeneratorPage.cpp` (`currentStep()`, `updateLeds`, `drawValueGenerator` / preview)

**Approach:** Add a `Track::TrackMode::Indexed` branch to `currentStep()` (Indexed playhead via its engine) and to the LED draw. Make the value-preview use the builder's `length()` instead of the hardcoded `CONFIG_STEP_COUNT`, and handle Indexed's step count (≤48) + variable durations in the plot. Keep Note/Curve branches byte-for-byte.

**Test scenarios:**
- Indexed preview renders notes + variable durations without overrun (length ≤ 48, not 64); playhead/current-step tracks the Indexed engine; LEDs correct.
- Note/Curve preview/LEDs/playhead unchanged.
- Test expectation: build + manual sim, both track types.

**Verification:** Indexed preview legible + correct playhead; Note/Curve visuals unchanged.

---

### U7. Mode picker row↔Mode mapping + per-track filtering

**Goal:** The picker offers the modes valid for the current track via an explicit mapping.

**Dependencies:** none (precedes U5's launch).

**Files:**
- `src/apps/sequencer/ui/model/GeneratorSelectListModel.h` (visible-`Mode[]` + `modeForRow()`)
- `src/apps/sequencer/ui/pages/GeneratorSelectPage.cpp` (`closeWithResult` returns `modeForRow(selectedRow())`; accept the visible set)

**Approach:** Replace `rows()=int(Mode::Last)` + `Mode(selectedRow())` with a configurable visible-`Mode[]` (set by the launching page per track) and `modeForRow()` so a filtered list returns correct modes. Note/Curve pages pass their existing set (no behavior change); the Indexed page passes the Indexed-valid set (incl. Helical).

**Test scenarios:**
- Indexed picker lists the Indexed-valid modes incl. Helical; selecting any returns the right `Mode` (not a shifted ordinal).
- Note picker unchanged: same modes, same results.
- Test expectation: build + manual sim.

**Verification:** Correct modes per track; no ordinal drift; Note/Curve picker identical.

---

## Scope Boundaries

In scope: bespoke Indexed builder (U1); AR map + generator (U2/U3); factory mode (U4); the shared-UI genericization needed to host Indexed (U5–U7).

Out of scope / deferred (origin doc): Marbles warp, "steps" role-hierarchy, rests/gate on-off, gate-length depth knob, multi-voice/swarm/intone.

### Deferred to Follow-Up Work
- Enabling the *other* modes (Euclidean/Random/Init) on Indexed now that it has a builder — a separate evaluation; this plan only guarantees Helical.
- AR output visualization beyond the standard preview.

---

## System-Wide Impact

- **Shared-UI surfaces touched (regression risk):** `GeneratorPage` (Indexed branches), `StepSelection` (selection-view interface), `GeneratorSelectPage`/model (row↔Mode). These are used by Note and Curve generators — changes must be behavior-neutral for them.
- **Indexed-only new code:** builder, AR map, generator, factory entry, page launch.
- **No save-format change:** `IndexedSequence` storage + serialization untouched (only additive accessors if needed; rotation `firstStep` semantics preserved).
- **Affected users:** Indexed users gain a generator with knobs + preview; Note/Curve users should see zero change.

---

## Risks & Mitigation

- **Shared generator UI regression (highest).** U5–U7 modify `GeneratorPage`/`StepSelection`/the picker that Note/Curve rely on. Mitigation: keep Note/Curve branches byte-for-byte; explicit Note-generator regression check in U5/U6/U7 verification; land the selection-view + picker mapping as behavior-neutral refactors first, Indexed branches second.
- **U1 builder correctness.** Hand-written builder must honor preview/apply/revert exactly and never touch rotation `firstStep`. Mitigation: characterization-first contract test before downstream units.
- **`Mode::Last` / ordinal assumptions.** Adding a mode shifts the enum; any raw-ordinal use (beyond the picker, now fixed in U7) breaks silently. Mitigation: grep for `Mode(` casts and `int(Mode::Last)` during U4/U7.
- **AR convergence to a fixed point.** Mitigation: convergence-guard nudge in U2, covered by a test.
- **base→ticks calibration.** Tune in U2/U3; implementation-time.

---

## Verification (whole plan)

- `TestHelicalAr` green; sim + STM32 release build clean.
- Indexed end-to-end on the sim: Generate → Helical → edit octave-range/base/law/seed → non-destructive preview → apply; reseed regenerates; sequence breathes (variable durations) in the track scale.
- **Regression:** a Note (and Curve) generator still launches, previews, and applies identically; the mode picker shows the same modes/results for Note/Curve.
- No change to saved Indexed projects; rotation behavior intact.
