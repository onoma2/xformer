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
`GeneratorSelectPage`) only reaches Note/Curve today because of five hardcodings, all verified:

- `SequenceBuilderImpl<T>` assumes `T` has `firstStep()/lastStep()/setFirstStep/setLastStep/step().layerValue()/setLayerValue()/steps()` and treats `firstStep()` as the generate range start.
- `GeneratorPage::show` + member are `StepSelection<CONFIG_STEP_COUNT>` (64) — a fixed width.
- `GeneratorPage::currentStep()`/LEDs `switch` on Note/Curve, the value-preview hardcodes 64 steps, and section/bank nav is `% 4`.
- `GeneratorSelectPage` casts `selectedRow()` straight to `Mode` over all modes.
- The generic generators write a fixed 64 — `EuclideanGenerator` loops `CONFIG_STEP_COUNT`, `RandomGenerator` loops a fixed-64 pattern — overflowing a shorter (48-step Indexed) builder.

The rework removes those five assumptions (bespoke builder for the non-conforming track, selection view, page rendering by builder length + track
branch, picker row→Mode mapping, length-bounding the generic generators), then onboards Indexed as the first client
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
- **Resolved scale/root ride on the builder, sequence-override-first.** `execute(mode, builder)` has no context arg, `Params` are private statics, and `AlgoGenerator`'s ctor calls `update()` immediately — so context must exist at construction. The `IndexedSequenceBuilder` carries the **resolved** scale + root, computed at launch exactly as the Indexed UI does (`IndexedSequenceEditPage.cpp:388`): scale = `sequence.selectedScale(_project.selectedScale())`, root = `sequence.rootNote() < 0 ? _project.rootNote() : sequence.rootNote()`. Passing the raw project scale would ignore a per-sequence scale/root override. `HelicalGenerator` reads them; `execute` signature unchanged.
- **Generic generators must write `builder.length()`, not 64.** `EuclideanGenerator`/`RandomGenerator` hardcode the write count; on a 48-step Indexed builder that overflows. Length-bounding them is part of making the framework track-agnostic (U8).
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
- `src/apps/sequencer/engine/generators/Generator.h` (add a public `length()` accessor)
- `src/apps/sequencer/ui/pages/GeneratorPage.cpp` (`currentStep()`, LEDs, value-preview, section nav)

**Approach:** `GeneratorPage` holds `const Generator&` and `_builder` is protected, so add a public `int Generator::length() const { return _builder.length(); }`. Then: (a) drive the value-preview off `generator.length()` instead of `CONFIG_STEP_COUNT`; (b) make **section/bank navigation length-derived** — replace the hardcoded `_section = (_section + 3) % 4` with a bank count of `ceil(length / StepCount)` (StepCount=16), clamping/wrapping `_section` to it, and reset/clamp `_section` in `show`/`init` when a generator with fewer banks opens (else `stepOffset()` reaches 48 → unchecked `StepSelection::flip` at index ≥48 on the 48-wide Indexed selection); (c) per-track `currentStep()`/LED handling. Keep Note/Curve branches byte-for-byte (64 → 4 banks naturally). Verified against Indexed once U4/U5 land.

**Test scenarios:**
- Note/Curve: preview, LEDs, playhead, and 4-bank Left/Right nav unchanged.
- Indexed (48): exactly 3 banks; Left/Right wrap 0..2 (never bank 3 → no selection index ≥48); preview length-driven, no overrun; opening the Indexed generator after a Note one clamps `_section` back in range.
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

**Approach:** Implement every `SequenceBuilder` virtual against Indexed. Member shape mirrors `SequenceBuilderImpl`: **`IndexedSequence &_edit` is a live reference to the track's sequence** (so `apply()` commits to the model), `IndexedSequence _original` is a copy (revert), `IndexedSequence *_preview` a heap copy — a copy for `_edit` would pass local tests but never reach the track. Range = active length / passed range (**not** rotation `firstStep`); `editSequence()` returns `_edit` for whole-step writers; `value()/setValue()` map a chosen layer (single-layer modes); `capacity()` returns `IndexedSequence::MaxSteps` (48) for U8. Carry the **resolved** scale (`const Scale&`) + root with accessors, set at construction (U5) via sequence-override-first resolution — not the raw project scale. Verify copy semantics for the preview clone.

**Execution note:** Characterization-first — pin preview/apply/revert before tenants depend on it.

**Test scenarios:**
- preview→apply mutates the **real model sequence** (assert via the project's Indexed sequence, not the builder's local state — `_edit` must be a live reference); revert restores; showOriginal/showPreview toggle.
- range = active/passed range; rotation `firstStep` untouched after apply.
- `value/setValue` round-trip a layer at range bounds.
- Test expectation: builder unit test if feasible; else build + U5 manual (apply must visibly change the played sequence).

**Verification:** Builder drives preview/apply/revert; rotation intact; builds.

---

### U5. Indexed launch + existing modes (the agnostic-ness proof)

**Goal:** Launch generators from the Indexed page; offer Init/Euclidean/Random (+ Helical after U7).

**Dependencies:** U1, U2, U3, U4, U8.

**Files:**
- `src/apps/sequencer/ui/pages/IndexedSequenceEditPage.cpp` / `.h` (Generate entry → picker → build → show)

**Approach:** Mirror `NoteSequenceEditPage.cpp:1163`: construct `IndexedSequenceBuilder(sequence, range, sequence.selectedScale(_project.selectedScale()), sequence.rootNote() < 0 ? _project.rootNote() : sequence.rootNote())` (resolved scale/root, sequence-override-first), pass the Indexed visible-`Mode[]` (Init/Euclidean/Random; Helical lists once U7 registers it) to the picker, run `Generator::execute`, show `GeneratorPage` with the Indexed selection as the view. Range from the Indexed step selection / active length; layer = the currently-edited Indexed layer for single-layer modes.

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

**Approach:** Mirror `AlgoGenerator`: take the builder, `editSequence()`, seed from `seed`, read the **resolved** scale/root (sequence-override-first, set at launch) from the `IndexedSequenceBuilder` (cast), run `helicalArStep` per step writing `setNoteIndex`/`setDuration`/`setGateLength`. `Params` = octave range, base, law direction, seed (+ `randomizeSeed`) via `paramName`/`printParam`/`editParam`. Add to the factory container + a `case Mode::Helical:` guarded by `dynamic_cast<IndexedSequenceBuilder*>(&builder)`; add Helical to the Indexed visible-`Mode[]` (U5). Grep for other `int(Mode::Last)`/`Mode(...)` casts when adding the enum.

**Test scenarios:**
- same params+seed → identical generated Indexed sequence; octave-range/base change span/timing.
- Helical guarded out for non-Indexed builders.
- Test expectation: map correctness from U6; wiring is build + manual preview.

**Verification:** Indexed → Generate → Helical → tweak knobs → preview → apply; reseed regenerates; reproducible; not offered on Note/Curve. With an Indexed sequence whose own scale/root differ from the project, the generated pitches follow the **sequence's** scale/root (override honored), not the project's.

---

### U8. Length-bound the generic generator write loops

**Goal:** `EuclideanGenerator`/`RandomGenerator` (and any generic mode) write `builder.length()` steps, not a hardcoded 64 — so they're safe on any-width builder.

**Dependencies:** none.

**Files:**
- `src/apps/sequencer/engine/generators/SequenceBuilder.h` (add a `capacity()` accessor; Note/Curve impls return `CONFIG_STEP_COUNT`)
- `src/apps/sequencer/engine/generators/EuclideanGenerator.h` / `.cpp`, `RandomGenerator.h` / `.cpp` (clamp params + write loops to the builder; audit `InitLayer`/`InitSteps`)

**Approach:** Two coupled fixes, both off the builder so any-width works:
1. **Write loops** use `_builder.length()` instead of `CONFIG_STEP_COUNT` / fixed-64.
2. **Length/capacity-bearing params** clamp to `_builder.capacity()` instead of `CONFIG_STEP_COUNT`: Euclidean's `setSteps`/`setBeats`/`setOffset` clamp to capacity, and `update()`'s `setLength` is bounded by it; Random's pattern is bounded to capacity. Add `int capacity() const` to the `SequenceBuilder` base — Note/Curve return `CONFIG_STEP_COUNT` (64), the Indexed builder returns `IndexedSequence::MaxSteps` (48, provided in U4). Without this, `Steps=64` on Indexed either overflows (if `setLength(64)` is accepted) or silently truncates (UI says 64, output 48). For Note/Curve `capacity()==64` → behavior-neutral.

**Execution note:** Behavior-neutral for Note/Curve (capacity stays 64); treat any output difference as an intentional length-respect fix and confirm with a Note check.

**Test scenarios:**
- Indexed (48): editing Euclidean `Steps` above 48 clamps to 48; no `setLength`/`setValue` beyond index 47 (no overflow, no `StepSelection` index ≥48); Random likewise.
- Note (64): Euclidean params still reach 64; Euclidean/Random output identical to before.
- Test expectation: build + manual sim, both track types.

**Verification:** On Indexed, params clamp to 48 and no write/setLength exceeds 48; Note/Curve params reach 64 and output is unchanged.

---

## Scope Boundaries

In scope: the generic de-hardcodings (U1–U3 + U8 generator write/param bounds), Indexed as first client (U4–U5) with the existing modes,
helical AR as the first new Mode (U6–U7).

Out of scope / deferred:
- **Other tracks as clients (PhaseFlux, Stochastic, …)** — the framework is built and proven track-agnostic here; onboarding another track is the documented recipe (its builder + selection view), a separate plan.
- From the origin doc: Marbles warp, "steps" role-hierarchy, rests/gate on-off, gate-length depth knob, multi-voice/swarm/intone.

---

## System-Wide Impact

- **Shared surfaces (regression risk, Note/Curve):** selection view (U1), picker (U2), `GeneratorPage` rendering (U3), **and shared generator *behavior* — `EuclideanGenerator`/`RandomGenerator` param + write bounds (U8)**. All changes behavior-neutral for Note/Curve, with explicit regression checks.
- **New per-client code:** Indexed builder + launch.
- **New tenant:** helical map + generator + factory entry.
- **No save-format change:** Indexed storage/serialization untouched; rotation semantics preserved.
- **Users:** Indexed gains generators (existing modes + Helical); Note/Curve unchanged; future tracks gain a cheap on-ramp.

---

## Risks & Mitigation

- **Shared generator UI + behavior regression (highest).** U1–U3 (shared page/picker/selection) **and U8 (`EuclideanGenerator`/`RandomGenerator` param + write bounds)** touch Note/Curve. Mitigation: keep their branches byte-for-byte / capacity==64 for Note/Curve; land U1–U3 + U8 as behavior-neutral refactors with a Note-generator regression check (params still reach 64, output unchanged) before any Indexed code.
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
