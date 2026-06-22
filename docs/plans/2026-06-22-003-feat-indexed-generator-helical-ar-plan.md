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

Two coupled pieces: (1) make the Indexed track a client of the existing **Generator framework**
(so it gets the knob param-editing page + preview/apply that Note/Curve already have), and (2) add
the **helical autoregressive generator** as a new generator Mode targeting Indexed — porting the
Nebulae 2 helical map (deterministic coupled fold-feedback → note + variable per-step duration).

Origin: `docs/plans/2026-06-22-indexed-helical-ar-macro-design.md` (validated brainstorm; the AR
algorithm + scope are settled there). One decision was reopened during planning: deliver via the
Generator framework (knobs + preview), not the preset-menu macro path — which requires the bridge below.

---

## Summary

The Generator framework (`Generator` + `SequenceBuilder`, hosted by the type-agnostic `GeneratorPage`
with paramName/editParam + preview/apply) currently reaches only Note and Curve sequences — they
expose the `Layer` surface `SequenceBuilderImpl<T>` needs. Indexed does not, so its only algorithmic
fill today is the preset-menu `populateWithMacro*` methods (no knobs, no preview).

The helical AR wants both **variable per-step duration** (only Indexed has it) and the **knob/preview
UX** (only the framework has it). So this plan bridges Indexed into the framework, then lands the AR
as a Mode on that bridge. The AR map itself (deterministic, mono, coupled fold-feedback, no RNG) is
specified in the origin design doc and implemented as a pure, unit-tested helper.

---

## Problem Frame

- **Why Indexed:** its steps carry a first-class 10-bit `duration` (variable step time), which the
  helical pitch↔duration coupling requires. Note/Curve are fixed-grid (gate length only).
- **Why the framework (not the macro path):** the framework owns the knob param-editing page +
  non-destructive preview/apply (`GeneratorPage`, `generatorSelect`, the builder container). The
  macro path applies directly with preset-menu params. For a 4-knob chaotic generator where scrubbing
  seed/octave-range and auditioning before commit matter, the framework UX is the point.
- **The gap:** `SequenceBuilderImpl<T>` requires `T::Layer` + `layerRange` + `layerDefaultValue` +
  copyability. `NoteSequence`/`CurveSequence` have them; `IndexedSequence` doesn't. `GeneratorPage`
  is already type-agnostic (drives the `Generator`/`SequenceBuilder` base), and the factory guards
  modes by builder type. So the bridge is: make Indexed conform, register a Helical mode guarded to
  the Indexed builder, and wire the Indexed page's launch.

---

## Key Technical Decisions

- **Reuse the framework, don't fork it.** `GeneratorPage` is type-agnostic; the work is making Indexed a `SequenceBuilder` client + a new `Generator` subclass, not new page UI. If `IndexedSequence` can't satisfy the `SequenceBuilderImpl<T>` template cleanly (its bit-packed step model differs from Note's), fall back to a bespoke `IndexedSequenceBuilder` implementing the `SequenceBuilder` base interface — resolved at implementation time.
- **The AR generator writes whole steps**, like `AlgoGenerator` does for Note (grabs `builder.editSequence()` and sets note+duration+gateLength per step), rather than the single-layer `value()/setValue()` path. The coupling needs note and duration produced together in one pass.
- **AR map = origin design doc, verbatim.** Deterministic, mono, `raw = prevDur·heavy + prevPitch·light`, fold mod (octaveRange × scaleSize), scale-quantize → note, `√freq` law → duration, fold-remainder → gate length, feed back. No RNG; seed sets initial state. Implemented as a pure helper for TDD.
- **Mode guarding.** Register `Mode::Helical`; the factory guards it to the Indexed builder (mirroring the Algo→NoteSequenceBuilder guard). `generatorSelect` offers Helical for Indexed; Algo stays Note-only.
- **Scale + loop length come from the track** (existing Indexed `scale()`/`rootNote()` + the apply range), not generator params — per the design doc.

---

## High-Level Technical Design

Launch flow (mirrors the Note path; directional, not implementation spec):

```
IndexedSequenceEditPage  --Generate-->  generatorSelect (Mode picker, Helical shown for Indexed)
   -> builderContainer.create<IndexedSequenceBuilder>(selectedIndexedSequence(), layer)
   -> Generator factory(mode=Helical, builder)  // guarded to Indexed builder
   -> GeneratorPage.show(generator, &stepSelection)   // knobs + preview/apply, type-agnostic
        apply -> builder writes the Indexed sequence
```

The new surface is the builder + the generator + the factory entry + the page's launch hook;
`GeneratorPage` itself is reused.

---

## Implementation Units

### U1. Make IndexedSequence a SequenceBuilder client

**Goal:** Indexed conforms to `SequenceBuilderImpl<T>` (or gets a bespoke builder), yielding an `IndexedSequenceBuilder`.

**Dependencies:** none.

**Files:**
- `src/apps/sequencer/model/IndexedSequence.h` / `.cpp` (add `Layer` enum + `layerRange`/`layerDefaultValue` + per-layer get/set-as-float; confirm copyable)
- `src/apps/sequencer/engine/generators/SequenceBuilder.h` (add `IndexedSequenceBuilder` typedef, or a bespoke builder)

**Approach:** Enumerate Indexed's step layers (note / duration / gateLength / slide) as `Layer`; provide `layerRange`/`layerDefaultValue` + scalar get/set per layer (ints ↔ float). Verify `IndexedSequence` copy semantics suffice for the preview clone (`new T(_original)`). If the template's single-layer `value()` model fights Indexed's packed step, implement a bespoke `IndexedSequenceBuilder : SequenceBuilder` instead.

**Patterns to follow:** `NoteSequence::Layer` + `layerRange`/`layerDefaultValue`; `SequenceBuilderImpl<T>`; the `NoteSequenceBuilder`/`CurveSequenceBuilder` typedefs.

**Test scenarios:**
- A builder over an Indexed sequence round-trips: preview → apply commits; revert restores the original.
- `value/setValue` for each Layer maps int↔float correctly at range bounds.
- Test expectation: builder round-trip is unit-testable if a builder test harness exists; otherwise build + the U5 manual preview/apply exercises it.

**Verification:** `IndexedSequenceBuilder` constructs over an Indexed sequence; preview/apply/revert behave; sim + STM32 build.

---

### U2. Helical AR map — pure helper (test-first)

**Goal:** The deterministic coupled-AR step function as a standalone, unit-tested helper.

**Dependencies:** none.

**Files:**
- `src/apps/sequencer/engine/generators/HelicalAr.h` (create — pure struct/function)
- `src/tests/unit/sequencer/TestHelicalAr.cpp` (create)
- `src/tests/unit/sequencer/CMakeLists.txt` (register)

**Approach:** `helicalArStep(pitch, dur, span, base, lawDir) → {noteIndex, durTicks, gateLen, newPitch, newDur}` per the origin doc: `raw = prevDur·W_DUR + prevPitch·W_PITCH`; `influence = fmod(raw, span)`; `noteIndex = round(influence)`; gate length from the fold remainder mapped to 20–100%; duration = `base × law(√freq)` clamped to tick range; feed back. Borrow helical's constants (17.31 / 0.5 / 16.17) as starting points. Include the convergence guard (nudge off a fixed point).

**Execution note:** Test-first — this is the one cleanly unit-testable unit (mirror `TuringRegister`/`TT2Transpose`).

**Test scenarios:**
- determinism: same seed+params → identical step stream.
- coupling: altering the duration-feedback weight shifts the pitch trajectory.
- octave range: `noteIndex` stays within `[0, octaveRange×scaleSize)`.
- law direction: higher pitch → monotonically longer (or shorter) `durTicks`.
- bounds: `durTicks` within the Indexed tick range; gate length in 20–100%.
- convergence guard: a fixed-point-prone seed does not collapse to one repeated note.

**Verification:** `TestHelicalAr` green; sim + STM32 build with the header.

---

### U3. HelicalGenerator (Generator subclass)

**Goal:** A `Generator` for `Mode::Helical` that runs U2 across the apply range and writes whole Indexed steps, with the 4 knob params.

**Dependencies:** U1, U2.

**Files:**
- `src/apps/sequencer/engine/generators/HelicalGenerator.h` / `.cpp` (create)

**Approach:** Mirror `AlgoGenerator`: grab `editSequence()` from the Indexed builder, seed the AR from `seed`, read the sequence `selectedScale()` for the √freq law + degrees-per-octave, run `helicalArStep` per step writing `setNoteIndex`/`setDuration`/`setGateLength`. Expose params via `paramName`/`printParam`/`editParam`: octave range, base, law direction, seed (+ `randomizeSeed` if the base offers it). Deterministic → `init()`/preview regenerate identically.

**Patterns to follow:** `AlgoGenerator` (param surface, `editSequence()` whole-step writes, `randomizeSeed`).

**Test scenarios:**
- Same params+seed → identical generated Indexed sequence (determinism) — via builder/preview if testable, else build + manual.
- Octave-range and base params visibly change span and step timing.
- Test expectation: map correctness is covered by U2; this unit's wiring is build + manual preview.

**Verification:** Generating with the Helical mode fills the Indexed sequence; params edit and re-preview; reproducible.

---

### U4. Register + guard the Helical mode

**Goal:** `Mode::Helical` exists in the factory, guarded to the Indexed builder.

**Dependencies:** U3.

**Files:**
- `src/apps/sequencer/engine/generators/Generator.h` / `.cpp` (add `Mode::Helical`, `modeName`, factory case + builder-type guard)

**Approach:** Add the enum value + name; in the factory, instantiate `HelicalGenerator` only for an Indexed builder (mirror the `Algo`→`NoteSequenceBuilder` guard at `Generator.cpp:35`). Other modes remain unaffected.

**Test scenarios:**
- factory returns a HelicalGenerator for an Indexed builder + Helical mode; returns null/guarded for a non-Indexed builder.
- Test expectation: small enum/factory logic — covered by build + the U5 launch check.

**Verification:** Factory wiring builds; Helical resolves only for Indexed.

---

### U5. Indexed edit page launch + mode select

**Goal:** Launch the generator from the Indexed edit page; Helical appears in the picker for Indexed.

**Dependencies:** U1, U4.

**Files:**
- `src/apps/sequencer/ui/pages/IndexedSequenceEditPage.cpp` / `.h` (a Generate entry → `generatorSelect` → `create<IndexedSequenceBuilder>` → `generator.show`)
- the `generatorSelect` mode-picker source (offer Helical for Indexed)

**Approach:** Mirror `NoteSequenceEditPage`'s generator launch (`generatorSelect.show(cb) → builderContainer.create<…Builder>(seq, layer) → factory → generator.show(generator, &_stepSelection)`). Range from the existing step selection / active length. Ensure the mode picker lists Helical when the track is Indexed (and not the Note-only modes that don't fit, per the guards).

**Patterns to follow:** `NoteSequenceEditPage.cpp:1163` generator launch block; `getMacroRange` for the apply range.

**Test scenarios:**
- From an Indexed track, Generate → picker shows Helical → preview opens → apply writes the sequence.
- Step-selection range scopes the fill; full-sequence when nothing selected.
- Test expectation: UI flow — build + manual sim.

**Verification:** End-to-end on the sim: Indexed track → Generate → Helical → tweak knobs → preview → apply.

---

### U6. GeneratorPage preview rendering for Indexed

**Goal:** The preview/apply page renders an Indexed preview correctly.

**Dependencies:** U5.

**Files:**
- `src/apps/sequencer/ui/pages/GeneratorPage.cpp` (verify/adjust preview draw for Indexed)

**Approach:** `GeneratorPage` is type-agnostic via the `SequenceBuilder` base, but its preview draw may assume Note-like values. Verify the Indexed preview (notes + variable durations) renders; adjust the draw path only if it hardcodes Note assumptions. Likely small or no-op.

**Test scenarios:**
- Preview shows the generated Indexed steps without visual corruption; apply/revert from the page work.
- Test expectation: build + manual sim.

**Verification:** Preview legible on the sim; apply/revert correct.

---

## Scope Boundaries

In scope: the Indexed↔Generator bridge (U1, U4–U6), the AR map (U2) and its generator (U3), the 4 params, scale/loop-length from the track.

Out of scope / deferred (from the origin doc): Marbles distribution warp, "steps" role-hierarchy, rests/gate on-off, gate-length depth knob, multi-voice/swarm/intone.

### Deferred to Follow-Up Work
- Enabling the *other* generator modes (Euclidean/Random/Init) on Indexed now that it's a builder client — a free side benefit to evaluate separately, not required here.
- A dedicated bit-pattern/timeline visualization of the AR output.

---

## System-Wide Impact

- **Surfaces:** `IndexedSequence` (builder conformance — model-adjacent but additive, no stored layout change), the generators module (new builder typedef, generator, mode), `IndexedSequenceEditPage` (launch), `GeneratorPage` (preview verify). No save-format change (Indexed steps unchanged).
- **Risk concentration:** U1 (template conformance vs Indexed's packed step model) is the load-bearing unknown; the bespoke-builder fallback de-risks it. Everything downstream depends on U1.
- **Affected users:** Indexed-track users gain a generator + (as a deferred bonus) the path to other generator modes.

---

## Risks & Mitigation

- **U1 template mismatch** — Indexed's bit-packed step may not satisfy `SequenceBuilderImpl<T>` cleanly. Mitigation: bespoke `IndexedSequenceBuilder` on the `SequenceBuilder` base; U1 verifies preview/apply/revert before downstream units build on it.
- **AR convergence to a fixed point** — some seed/param combos collapse to one note. Mitigation: convergence-guard nudge in U2, covered by a test scenario.
- **Mode picker leakage** — Helical must not appear for Note/Curve, nor Algo for Indexed. Mitigation: factory guards + picker filtering, checked in U4/U5.
- **base→ticks calibration** — durations must land musical. Mitigation: tune constants in U2/U3; flagged as implementation-time.

---

## Verification (whole plan)

- `TestHelicalAr` green; sim (`make -C build/sim/debug sequencer`) + STM32 release build clean.
- End-to-end on the sim: Indexed track → Generate → Helical mode → edit octave-range/base/law/seed → preview (non-destructive) → apply; reseed regenerates; the sequence breathes (variable step durations) and stays in the track scale.
- No change to saved Indexed projects (step model + serialization untouched).
- Other tracks unaffected: Note/Curve generators still work; Helical not offered there.
