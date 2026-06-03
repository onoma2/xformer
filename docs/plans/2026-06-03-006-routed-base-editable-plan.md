---
id: routed-base-editable
schema: plan
title: "plan: editable base under an active route (base-anchored model)"
type: feature
status: done
scope: next
date: 2026-06-03
parent: routing-sliced-cutover
related:
  - routing-sliced-cutover
  - routed-value-storage-modulation
---

# Plan: editable base under an active route

_Status (2026-06-03): DONE — implemented, TDD + Codex-gated, sim + STM32 green.
All migrated edits operate on base under a route (no gate, no lurch). Beyond the plan's
relative/absolute editors, the First/Last setters + `offsetFirstAndLastStep` clamp in
base-domain, and the structural step ops `duplicateSteps`/`shiftSteps` were also moved to the
base loop (Codex flagged `duplicateSteps`; `shiftSteps` folded in as the same-shape sibling).
Readout stays the effective value + static arrow glyph, as decided._

## Why

The apply-fork cutover (plan 005) made migrated Note/PhaseFlux params **base-anchored**:
`out = clamp(base + override-delta)`. Slice 4 kept the old edit-gate — while a route
targets a param, the UI locks it (you must delete the route to change the value).

That lock made sense in the **old** model: the routed value *replaced* base, so editing
base changed nothing visible. In the base-anchored model it is the opposite — editing
base **moves the modulation center**, a useful live gesture. The lock is now friction with
no purpose. Hardware finding (2026-06-03): routing RootNote on a Note track works, but the
base can only be changed by removing the route.

**Decision:** unlock base editing for migrated params. Drop the `routeOverridden` edit-gate
and make relative edits operate on **base**, not on the override-aware getter.

## The lurch (why "drop the gate" is not enough)

Relative editors read the current value then add the knob delta:

    setScale(scale() + value)        // scale() == base + override

`scale()` returns `base + override` (the moving modulated value). If we just drop the gate,
each edit sets `base = base + override + value` — base permanently absorbs whatever the
override happened to be at that instant, so it **lurches by the live modulation amount**
every edit. Wrong.

Fix: relative editors must compute from **base**:

    setScale(int(_scale.base) + value)

When unrouted, `override` is absent so `routedValueInt` returns base **unmodified** (incl.
`Scale = -1` Default, despite `lo=0`) — so `scale() == _scale.base`, identical to today. When
routed, this edits the center cleanly with no lurch. Same substitution for every relative
editor (`getter()` -> the param's `.base`).

Absolute setters (set to a chosen value, not `current + delta`) have no lurch — they only
need the gate dropped.

## Surface

Two classes of edit path; 33 gate sites today.

### A. Relative editors — drop gate **and** edit from base

- **NoteSequence** (7): `editScale`, `editRootNote`, `editDivisor`, `editClockMultiplier`,
  `editRunMode`, `editFirstStep`, `editLastStep`. Each currently
  `if (!routeOverridden(...)) setX(getter()±value)`. Replace the inner expression's
  `getter()` with the param base:
  - `_scale.base`, `_rootNote.base`, `_divisor.base`, `_clockMultiplier.base`,
    `int(_runMode.base)`, `_firstStep.base`, `_lastStep.base`.
  - Keep the helper-call shapes: `ModelUtils::adjustedByDivisor(_divisor.base, value, shift)`,
    `ModelUtils::adjustedEnum(Types::RunMode(int(_runMode.base)), value)`.
  - `editFirstStep`/`editLastStep`: **special — cross-endpoint clamp hazard.** The
    non-shift edit moves to `_firstStep.base`/`_lastStep.base`, but that alone is not enough:
    `setFirstStep(v)` clamps `v` against `lastStep()` and `setLastStep(v)` against
    `firstStep()` — both override-aware, so the *bound itself* is the moving routed value, and
    a base edit gets clamped by the live modulated window. Fix: the two setters must clamp the
    base against the **peer's base** (`_lastStep.base` / `_firstStep.base`), not the getter.
    These setters back only Note (migrated) and bulk/init, so the change is contained. The
    `lastStep()` read guard (`max(firstStep(), …)`) stays — playback ordering self-corrects at
    read time even if stored base order is transiently invalid.
  - `offsetFirstAndLastStep` (the `shift` branch) is **not** left unchanged: it computes the
    allowed offset from `firstStep()`/`lastStep()` (override-aware), so shift-edits would clamp
    by the live routed window. Decision for the base-center model: compute the offset from
    `_firstStep.base`/`_lastStep.base` so shift-offset edits the **base** loop.
- **NoteTrack** (8): `editSlideTime` (`adjustedByStep(_slideTime.base, …)`), `editOctave`,
  `editTranspose`, `editRotate`, `editGate/Retrigger/Length/NoteProbabilityBias` —
  `getter()±value` -> `_x.base±value`.
- **PhaseFluxSequence** (4): `editScale`, `editRootNote`, `editDivisor`,
  `editClockMultiplier` — same substitution (`_scale`/`_rootNote` are plain int fields;
  `_divisor.base`/`_clockMultiplier.base`).
- **PhaseFluxTrackListModel** (3): `SlideTime`/`Octave`/`Transpose` use
  `ModelUtils::adjusted(_track->slideTime(), …)` — relative, so they lurch too. The list
  model has only the getter, not base. **Resolve by adding `editSlideTime/editOctave/
  editTranspose` to PhaseFluxTrack** (edit-from-base, mirroring NoteTrack) and have the list
  model call those — symmetric with how NoteTrack track params are edited.

### B. Absolute setters — drop gate only (no lurch)

- **NoteSequenceListModel::setIndexedValue** (7 migrated cases): sets to the chosen `index`
  (absolute). Remove the `if (!routeOverridden(...))` guard.
- **LaunchpadController** `sequenceSetFirstStep/LastStep/RunMode` (Note, 3): sets to the
  chosen step/mode (absolute). Remove the guard.

These still route First/Last through `setFirstStep`/`setLastStep`, so they inherit the
cross-endpoint clamp hazard above — once those setters clamp in base-domain (section A) the
absolute paths are correct with no further change.

## What does NOT change

- **Getters** stay `clamp(base + override)` — reads are untouched; the value still modulates.
- **Override write path / apply fork / RouteApply / RouteShaper** — untouched.
- **Routing arrow marker** (`printRouted`, still on `isRouted`) — stays. The arrow now means
  "a route targets this," and you can still dial its center. That is the intended new
  semantics, not a contradiction.
- **`routeOverridden` accessor** stays (still used by reads/tests); only the edit *gates*
  that call it are removed.

## Readout (decided 2026-06-03)

The routed param's readout shows the **effective value** (`base + override`, the live
modulated value) — *not* base. The routing arrow stays a static "this is routed" glyph
(`printRouted`, already the behavior). **No readout/arrow change.**

This is coherent with edit-from-base: an edit shifts base, and since the override is
independent, the effective readout shifts by the edit amount (on top of the live modulation
wiggle). So turning the knob visibly moves the value the expected direction; the user is
dialing the center while watching the modulated result. The owner chose this explicitly over
showing base — do not reopen.

## Tests (TDD)

Extend `TestRouteGetterMigration`:
- **edit under active override moves base by the edit amount, not by the override.** Set
  base 5, write override +7 (so getter reads 12), `editScale(+2)`, clear override -> base is
  **7** (5+2), not 14 (5+7+2). Proves edit-from-base, no lurch.
- **getter after edit = newBase + override.** Same setup, before clearing: getter == 7+7.
- **unrouted edit unchanged.** No override, `editScale(+2)` on base 5 -> 7 (parity with today).
- Repeat for one bipolar param (NoteTrack Transpose via a base accessor or a direct base
  check) and one PhaseFlux param.
- **First/Last base-domain clamp under route.** Base first=0/last=15, write a LastStep
  override that drives `lastStep()` to e.g. 31, then `editFirstStep(+20)` -> `_firstStep.base`
  clamps against `_lastStep.base` (15), landing at 15, **not** at 31 (the routed window). Same
  shape for the absolute `setIndexedValue(FirstStep, …)` path and `offsetFirstAndLastStep`.

Absolute-setter paths (list model / Launchpad) are UI glue over `setX`; covered by the
model-level base behavior, not separately unit-tested (no harness).

## Build order

1. Relative editors edit-from-base + gate removed — NoteSequence, NoteTrack,
   PhaseFluxSequence. **Including the First/Last setter base-domain clamp** (clamp against
   the peer's `.base`, not the getter) and `offsetFirstAndLastStep` base-domain math. Tests
   first (lead with the lurch + the First/Last cross-endpoint clamp cases).
2. PhaseFluxTrack `editSlideTime/editOctave/editTranspose` (edit-from-base); rewire
   PhaseFluxTrackListModel to call them, gate removed.
3. Absolute setters: drop the gate in NoteSequenceListModel::setIndexedValue + Launchpad.
4. sim + STM32 green; routing+Note/PhaseFlux tests pass; Codex gate; hardware re-check
   (edit RootNote center while modulating, confirm no lurch, base survives route delete).

## Risk

Most editors are uniform mechanical substitution (`getter()` -> `.base`); the TDD lurch test
on a representative param catches a missed one. **Riskiest step (Codex-confirmed):
`editFirstStep`/`editLastStep` + their indexed and Launchpad paths** — not uniform, because
the setters' clamp and the shift helper already depend on override-aware *peer* reads.
Getting only the edit operand to base while leaving the clamp bound on the getter silently
re-introduces the routed-window clamp. The First/Last base-domain clamp test is the gate.

## Reversal note

This reverses plan 005 slice 4's "keep the edit-gate, switch its condition." Justification:
the base-anchored model makes base a live, meaningful parameter under routing — which the old
model could not offer — so locking it is friction the new model does not need. New
information = the hardware finding that base is only editable by deleting the route.
