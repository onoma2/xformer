---
id: track-agnostic-generators
schema: plan
title: "feat: track-agnostic generator framework (Indexed first client + helical AR)"
type: feat
status: active
date: 2026-06-22
depth: deep
---

# Track-agnostic generator framework

De-Note/Curve-hardcode the Generator framework so any track that supplies a builder + a selection
view can host generators. Prove and pay it off by making **Indexed** the first new client — which
immediately gives Indexed the existing modes (Euclidean / Random / Init) — and add the **helical
autoregressive generator** as the first new Mode (Nebulae 2 helical map: deterministic coupled
fold-feedback → note + variable per-step duration).

Origin: `docs/plans/2026-06-22-indexed-helical-ar-macro-design.md` (the AR algorithm + its scope are
settled there). This plan was re-scoped from "helical AR on Indexed" to "track-agnostic generators"
after four review rounds showed the bulk of the work is generic framework de-hardcoding, not the AR.

---

## Summary

The Generator framework (`Generator` + `SequenceBuilder`, hosted by `GeneratorPage`, picked via
`GeneratorSelectPage`) only reaches Note/Curve today because of four hardcodings, all verified:

- `SequenceBuilderImpl<T>` assumes `T` has `firstStep()/lastStep()/setFirstStep/setLastStep/step().layerValue()/setLayerValue()/steps()` and treats `firstStep()` as the generate range start.
- `GeneratorPage::show` + member are `StepSelection<CONFIG_STEP_COUNT>` (64) — a fixed width.
- `GeneratorPage::currentStep()`/LEDs `switch` on Note/Curve, and the value-preview hardcodes 64 steps.
- `GeneratorSelectPage` casts `selectedRow()` straight to `Mode` over all modes.

The rework removes those four assumptions (selection view, page rendering by builder length + track
branch, picker row→Mode mapping, per-builder guarding), then onboards Indexed as the first client
with a **bespoke builder** (the template can't be used — Indexed's `firstStep()` is rotation). The
existing modes light up on Indexed as the agnostic-ness proof; helical AR (a pure, unit-tested map
+ a generator) is the first net-new Mode. Other tracks (PhaseFlux, Stochastic) become a documented
follow-up: add their builder + selection view, nothing else.

---

## Problem Frame

- **Goal shift:** the durable asset is a track-agnostic generator framework. Helical AR and "Indexed gains generators" are the first tenants that justify and validate it.
- **Why Indexed first:** it's the track whose steps carry variable per-step `duration` (the helical coupling needs it) and it currently has *no* generator access at all — so onboarding it both delivers value (Euclidean/Random/Init + Helical) and exercises every generic seam.
- **The framework's reusable contract (the point of the rework):** a track becomes a generator client by providing (1) a `SequenceBuilder` implementation over its sequence and (2) a width-agnostic selection view. The page, picker, and factory then work unchanged. This recipe is what future tracks reuse.

---

## Key Technical Decisions

- **Bespoke builders per non-conforming track; keep the template for Note/Curve.** `SequenceBuilderImpl<T>` stays for Note/Curve. Indexed gets a hand-written `IndexedSequenceBuilder` implementing the `SequenceBuilder` base against its real API; **do not** add Note-style `firstStep/lastStep` to `IndexedSequence` (its `firstStep()` is rotation). Generate range is passed in / from active length.
- **Selection is type-erased to a view interface** covering `GeneratorPage`'s real contract: `selected(int)`, `clear()`, `keyDown(KeyEvent&, int stepOffset)`, `keyUp(KeyEvent&, int stepOffset)`, `any()/firstSetIndex()/lastSetIndex()`. `StepSelection<N>` implements it for any `N`; the page holds the interface. Note/Curve callers pass their existing selection as the view (behavior-neutral).
- **`GeneratorPage` renders by builder length + a track branch, not 64/Note.** `currentStep()` and LEDs gain per-track handling; the value-preview uses the builder's `length()`. Note/Curve branches stay byte-for-byte.
- **Picker uses an explicit visible-`Mode[]` + `modeForRow()`,** set per launching track. Note/Curve pass their existing set (unchanged); Indexed passes its set. Per-mode guarding in the factory via `dynamic_cast` to the required builder type (e.g. `Algo`→`NoteSequenceBuilder`, `Helical`→`IndexedSequenceBuilder`).
- **Project scale/root ride on the builder.** `execute(mode, builder)` has no context arg, `Params` are private statics, and `AlgoGenerator`'s ctor calls `update()` immediately — so context must exist at construction. The `IndexedSequenceBuilder` carries the resolved project scale + root (set at launch); `HelicalGenerator` reads them. `execute` signature unchanged.
- **Build registration is explicit.** New sources go in the manual list in `src/apps/sequencer/CMakeLists.txt`; new generators go in the `Container<…>` + a static `Params` in `Generator.cpp`.
- **AR map = origin doc, verbatim** (fold-feedback note, √freq duration, fold-remainder gate, seed init, no RNG, convergence guard).

---

## High-Level Technical Design

The framework after the rework, and the launch flow:

```
Track edit page --Generate--> GeneratorSelectPage(visibleModes for this track)   [U2]
   -> create <Track>SequenceBuilder(sequence, range[, scale/root])               [Note/Curve: existing; Indexed: U4]
   -> Generator::execute(mode, builder)  // per-mode dynamic_cast guard           [U7 adds Helical]
   -> GeneratorPage.show(generator, selectionView)                               [U1 selection view, U3 render-by-length]
        apply -> builder writes the sequence
```

Generic seams (touch Note/Curve — must stay behavior-neutral): selection view (U1), picker mapping
(U2), page rendering (U3). New per-client code: Indexed builder (U4) + launch (U5). New tenant:
helical map (U6) + generator (U7).

---

## Implementation Units

### U1. Type-erase the step selection

**Goal:** `GeneratorPage` accepts any-width selection via an interface.

**Dependencies:** none.

**Files:**
- `src/apps/sequencer/ui/StepSelection.h` (add the view interface; `StepSelection<N>` implements it)
- `src/apps/sequencer/ui/pages/GeneratorPage.h` / `.cpp` (hold/accept the view; rewrite `selected()[i]` → `selected(i)`)
- `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp`, `CurveSequenceEditPage.cpp` (pass their selection as the view)

**Approach:** Define a `StepSelectionView` (or similar) with `selected(int)`, `clear()`, `keyDown(KeyEvent&, int)`, `keyUp(KeyEvent&, int)`, `any()`, `firstSetIndex()`, `lastSetIndex()`. `StepSelection<N>` already has these (or trivially exposes them) → it implements the view. `GeneratorPage` stores `StepSelectionView*`. Rewrite the LED reads from `_stepSelection->selected()[i]` to `selected(i)`.

**Execution note:** Behavior-neutral refactor — diff Note/Curve to confirm only the selection-pass changed.

**Test scenarios:**
- Note generator: step keys still toggle selection inside the generator page; LEDs and clear behave identically.
- Curve generator: same.
- Test expectation: build + manual sim (Note generator), since the page has no unit harness.

**Verification:** Note/Curve generator pages behave identically; sim + STM32 build.

---

### U2. Picker row↔Mode mapping + per-track visible set

**Goal:** The picker offers a per-track mode subset via explicit mapping.

**Dependencies:** none.

**Files:**
- `src/apps/sequencer/ui/model/GeneratorSelectListModel.h` (visible-`Mode[]` + `modeForRow()`)
- `src/apps/sequencer/ui/pages/GeneratorSelectPage.cpp` / `.h` (accept the visible set; `closeWithResult` returns `modeForRow(selectedRow())`)
- launching pages pass their set (Note/Curve: existing modes; Indexed in U5)

**Approach:** Replace `rows()=int(Mode::Last)` + `Mode(selectedRow())` with a configurable visible-`Mode[]` + `modeForRow()`. Note/Curve pass their current full set (no behavior change).

**Test scenarios:**
- Note picker: same modes, same selection→Mode results as before.
- A filtered set returns the correct `Mode` (no ordinal drift).
- Test expectation: build + manual sim.

**Verification:** Note picker unchanged; filtered sets map correctly.

---

### U3. GeneratorPage track-agnostic rendering

**Goal:** Page renders/tracks by builder length + a per-track branch, not Note/64.

**Dependencies:** U1.

**Files:**
- `src/apps/sequencer/ui/pages/GeneratorPage.cpp` (`currentStep()`, LEDs, value-preview)

**Approach:** Drive the value-preview off the builder's `length()` instead of `CONFIG_STEP_COUNT`; add per-track `currentStep()`/LED handling so a new track can supply its playhead. Keep Note/Curve branches byte-for-byte. (Verified against Indexed once U4/U5 land.)

**Test scenarios:**
- Note/Curve: preview, LEDs, playhead unchanged.
- A shorter-than-64 sequence previews without overrun (verified via Indexed in U5).
- Test expectation: build + manual sim; Note regression check.

**Verification:** Note/Curve visuals unchanged; preview length-driven.

---

### U4. IndexedSequenceBuilder (first new client)

**Goal:** A bespoke `SequenceBuilder` for Indexed, carrying project scale/root.

**Dependencies:** none.

**Files:**
- `src/apps/sequencer/engine/generators/IndexedSequenceBuilder.h` / `.cpp` (create)
- `src/apps/sequencer/CMakeLists.txt` (add the source)
- possibly `src/apps/sequencer/model/IndexedSequence.h` (additive accessors only; not firstStep/lastStep)

**Approach:** Implement every `SequenceBuilder` virtual against Indexed: `_edit`/`_original`/`_preview` Indexed copies; range = active length / passed range (**not** rotation `firstStep`); `editSequence()` for whole-step writers; `value()/setValue()` map a chosen layer (so single-layer modes work). Carry resolved project scale (`const Scale&`) + root with accessors, set at construction (U5). Verify copy semantics for the preview clone.

**Execution note:** Characterization-first — pin preview/apply/revert before tenants depend on it.

**Test scenarios:**
- preview→apply commits; revert restores; showOriginal/showPreview toggle.
- range = active/passed range; rotation `firstStep` untouched after apply.
- `value/setValue` round-trip a layer at range bounds.
- Test expectation: builder unit test if feasible; else build + U5 manual.

**Verification:** Builder drives preview/apply/revert; rotation intact; builds.

---

### U5. Indexed launch + existing modes (the agnostic-ness proof)

**Goal:** Launch generators from the Indexed page; offer Init/Euclidean/Random (+ Helical after U7).

**Dependencies:** U1, U2, U3, U4.

**Files:**
- `src/apps/sequencer/ui/pages/IndexedSequenceEditPage.cpp` / `.h` (Generate entry → picker → build → show)

**Approach:** Mirror `NoteSequenceEditPage.cpp:1163`: construct `IndexedSequenceBuilder(sequence, range, _project.selectedScale(), root)`, pass the Indexed visible-`Mode[]` (Init/Euclidean/Random; Helical lists once U7 registers it) to the picker, run `Generator::execute`, show `GeneratorPage` with the Indexed selection as the view. Range from the Indexed step selection / active length; layer = the currently-edited Indexed layer for single-layer modes.

**Test scenarios:**
- Indexed: Generate → Euclidean fills a gate pattern on the chosen layer; Random fills; preview/apply/revert work.
- Selection scopes the fill; full-sequence when nothing selected.
- Test expectation: build + manual sim (this is the proof the framework is now track-agnostic).

**Verification:** Indexed runs the existing generic modes end-to-end via the shared page/picker.

---

### U6. Helical AR map — pure helper (test-first)

**Goal:** The deterministic coupled-AR step function as a standalone, unit-tested helper.

**Dependencies:** none.

**Files:**
- `src/apps/sequencer/engine/generators/HelicalAr.h` (create)
- `src/tests/unit/sequencer/TestHelicalAr.cpp` (create)
- `src/tests/unit/sequencer/CMakeLists.txt` (register)

**Approach:** `helicalArStep(pitch, dur, span, base, lawDir) → {noteIndex, durTicks, gateLen, newPitch, newDur}` per the origin doc: `raw = prevDur·W_DUR + prevPitch·W_PITCH`; `influence = fmod(raw, span)`; `noteIndex = round(influence)`; gate from the fold remainder → 20–100%; duration = `base × law(√freq)` clamped; feed back. Constants 17.31/0.5/16.17. Include the convergence-guard nudge.

**Execution note:** Test-first (mirror `TuringRegister`/`TT2Transpose`).

**Test scenarios:**
- determinism: same seed+params → identical step stream.
- coupling: changing the duration-feedback weight shifts the pitch trajectory.
- octave range: `noteIndex` in `[0, octaveRange×scaleSize)`.
- law direction: higher pitch → monotonically longer/shorter `durTicks`.
- bounds: `durTicks` in the Indexed tick range; gate length 20–100%.
- convergence guard: a fixed-point-prone seed doesn't collapse to one note.

**Verification:** `TestHelicalAr` green; builds.

---

### U7. HelicalGenerator + register Mode::Helical

**Goal:** The AR generator (first net-new Mode), guarded to Indexed.

**Dependencies:** U4, U6.

**Files:**
- `src/apps/sequencer/engine/generators/HelicalGenerator.h` / `.cpp` (create)
- `src/apps/sequencer/CMakeLists.txt` (add the source)
- `src/apps/sequencer/engine/generators/Generator.cpp` / `.h` (`Mode::Helical`, `modeName`, add to `Container<…>`, static `helicalParams`, guarded `execute` case)

**Approach:** Mirror `AlgoGenerator`: take the builder, `editSequence()`, seed from `seed`, read project scale/root from the `IndexedSequenceBuilder` (cast), run `helicalArStep` per step writing `setNoteIndex`/`setDuration`/`setGateLength`. `Params` = octave range, base, law direction, seed (+ `randomizeSeed`) via `paramName`/`printParam`/`editParam`. Add to the factory container + a `case Mode::Helical:` guarded by `dynamic_cast<IndexedSequenceBuilder*>(&builder)`; add Helical to the Indexed visible-`Mode[]` (U5). Grep for other `int(Mode::Last)`/`Mode(...)` casts when adding the enum.

**Test scenarios:**
- same params+seed → identical generated Indexed sequence; octave-range/base change span/timing.
- Helical guarded out for non-Indexed builders.
- Test expectation: map correctness from U6; wiring is build + manual preview.

**Verification:** Indexed → Generate → Helical → tweak knobs → preview → apply; reseed regenerates; reproducible; not offered on Note/Curve.

---

## Scope Boundaries

In scope: the four de-hardcodings (U1–U3), Indexed as first client (U4–U5) with the existing modes,
helical AR as the first new Mode (U6–U7).

Out of scope / deferred:
- **Other tracks as clients (PhaseFlux, Stochastic, …)** — the framework is built and proven track-agnostic here; onboarding another track is the documented recipe (its builder + selection view), a separate plan.
- From the origin doc: Marbles warp, "steps" role-hierarchy, rests/gate on-off, gate-length depth knob, multi-voice/swarm/intone.

---

## System-Wide Impact

- **Shared-UI surfaces (regression risk, Note/Curve):** selection view (U1), picker (U2), `GeneratorPage` rendering (U3). All changes behavior-neutral for Note/Curve, with explicit regression checks.
- **New per-client code:** Indexed builder + launch.
- **New tenant:** helical map + generator + factory entry.
- **No save-format change:** Indexed storage/serialization untouched; rotation semantics preserved.
- **Users:** Indexed gains generators (existing modes + Helical); Note/Curve unchanged; future tracks gain a cheap on-ramp.

---

## Risks & Mitigation

- **Shared generator UI regression (highest).** U1–U3 touch Note/Curve. Mitigation: keep their branches byte-for-byte; land U1–U3 as behavior-neutral refactors with a Note-generator regression check before any Indexed code.
- **Bespoke builder correctness.** Must honor preview/apply/revert and never touch rotation `firstStep`. Mitigation: characterization-first contract (U4).
- **Single-layer modes on Indexed** (Euclidean/Random) need a sensible target layer + range. Mitigation: launch passes the current Indexed layer; verify Euclidean/Random output in U5.
- **Enum/ordinal assumptions.** Adding `Mode::Helical` shifts `Mode::Last`; the picker no longer casts ordinals (U2), but grep other casts (U7).
- **AR convergence / base→ticks calibration.** Convergence-guard nudge + constant tuning (U6/U7), implementation-time.

---

## Verification (whole plan)

- `TestHelicalAr` green; sim + STM32 release build clean.
- **Framework agnostic-ness:** Indexed runs Euclidean/Random/Init via the shared page/picker (U5), and a Note (and Curve) generator still launches/previews/applies identically (U1–U3 regression).
- **Helical:** Indexed → Generate → Helical → edit octave-range/base/law/seed → non-destructive preview → apply; reseed regenerates; sequence breathes in the track scale; not offered on Note/Curve.
- No change to saved Indexed projects; rotation intact.
