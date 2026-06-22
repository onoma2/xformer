---
id: accumulator-ops-unify
schema: plan
title: "refactor: unify AccumulatorOps direction convention + route NoteTrack through it"
type: refactor
status: completed
date: 2026-06-22
depth: standard
---

# Unify AccumulatorOps, dedup NoteTrack's drift math

The accumulator boundary/order math exists in two copies: NoteTrack's stateful `Accumulator`
class (`model/Accumulator.cpp`) and the stateless `AccumulatorOps` free functions
(`engine/AccumulatorOps.cpp`) that PhaseFlux calls. The free functions were extracted *from*
NoteTrack (see `docs/accumulator-v2-spec.md` §13.9), so today the same Wrap/Pendulum/Hold/Random
logic is maintained in two places. This unifies the helper on one calling convention and routes
NoteTrack through it, leaving a single implementation.

Behavior-preserving: musical behavior on both tracks must be identical before/after, with one
documented exception (Wrap overshoot — see Key Decisions). This is the deferred "NoteTrack
migration" from `docs/accumulator-v2-spec.md` §13.6/§13.7, scoped to the math layer + the helper
unification only.

---

## Summary

`AccumulatorOps` is internally inconsistent: `tickWrap`/`tickHold` take an explicit `direction`
(+1/-1/0), but `tickPendulum` takes a *signed* step and `tickRandom` takes no direction at all.
That split is the migration hazard — NoteTrack speaks `Direction{Up,Down,Freeze}` + an unsigned
magnitude, and the mismatch is exactly where a naive delegation could change the bounce or the
freeze case. The fix: make all five ops speak one convention — explicit `direction` (+1/-1/**0 =
freeze, a first-class no-op**) plus an **unsigned magnitude** — then have both callers feed it.

After unification, NoteTrack's `tickWith{Wrap,Pendulum,Hold,Random}` bodies become thin wrappers
that call the ops; NoteTrack keeps its class, state, delayed-first-tick, trigger modes, per-step
overrides, scope, and serialization untouched.

---

## Problem Frame

- **Goal:** one implementation of the order/boundary math, shared by both tracks; future fixes land
  once and both inherit them.
- **Why now:** the two copies can silently diverge (they already differ on Wrap overshoot). The
  spec deferred this migration until PhaseFlux shipped — it has.
- **Hard constraint:** behavior-preserving. The faithfulness gate is bit-identical output vs today's
  NoteTrack on identical input (pure integer math, so exact is achievable) — except the Wrap
  overshoot case below, which is a latent-bug fix.

---

## Key Technical Decisions

- **Wrap overshoot is a behavior change (lead decision — contradicts strict "bit-identical").**
  NoteTrack's `tickWithWrap` (`Accumulator.cpp:58-70`) uses a single-subtract; when `step > range`
  it leaves the counter **outside** `[min,max]` (a latent bug). The helper's `tickWrap`
  (`AccumulatorOps.cpp:7-19`) uses positive-remainder modulo and wraps correctly. They are
  bit-identical for `step ≤ range` (the normal case) and diverge only on `step > range`.
  **Decision: adopt the helper's modulo.** Deduping requires one implementation, and the divergent
  case is NoteTrack producing a wrong (out-of-range) value. Faithfulness is asserted bit-identical
  for `step ≤ range`; the `step > range` case is documented as an intentional fix. (If preserving the
  old buggy value were required, dedup would be impossible — flag, don't silently choose.)
- **Pendulum stays bit-identical via the Freeze guard.** NoteTrack's `tickWithPendulum`
  (`Accumulator.cpp:72-86`) ignores Up/Down (only `Freeze` early-returns) and travels by
  `_pendulumDirection` (seeded +1 at `reset()`); its hardcoded `±1` flips equal the helper's
  negate-flip for positive magnitude. So delegation is exact **as long as the wrapper keeps the
  `Freeze` (direction 0) guard** and passes the unsigned magnitude + caller-owned `pendulumDir`.
- **Freeze becomes a first-class op no-op.** Every op early-returns on `direction == 0`. Removes the
  per-caller "guard before calling" burden; for pendulum/random this means adding a `direction`
  parameter they don't currently take.
- **Random is range-checked, not value-checked.** Both impls use a function-local `static Random`;
  delegation changes which static instance drives it, so exact values differ — but Random is
  non-deterministic by design. Faithfulness for Random asserts the result stays in `[min,max]` and
  honors Freeze, not specific values.
- **Magnitude is unsigned; direction carries sign.** Matches NoteTrack's existing storage
  (`_stepValue` is already a positive magnitude; the `Direction` enum / per-step override carries
  sign — `NoteTrackEngine.cpp` flips direction for negative overrides). PhaseFlux already decomposes
  its signed step into direction+magnitude in `advanceCounter`, so its call site barely changes.

---

## Implementation Units

### U1. Unify AccumulatorOps + update the PhaseFlux call site

**Goal:** all five ops share the `(direction ∈ {+1,-1,0}, magnitude ≥ 0)` convention with
`direction == 0` as a no-op; PhaseFlux keeps compiling and behaving identically.

**Dependencies:** none.

**Files:**
- `src/apps/sequencer/engine/AccumulatorOps.h` / `.cpp`
- `src/apps/sequencer/engine/PhaseFluxTrackEngine.cpp` (the `advanceCounter` wrapper, ~103-142, and
  call sites ~451-453 / 458-460 / 743 / 750 — only if the signature change ripples)
- `src/tests/unit/sequencer/TestAccumulatorOps.cpp`

**Approach:** Rename the `step` parameter to an unsigned `magnitude` and add an explicit `direction`
to `tickPendulum` and `tickRandom` (they lack it today). Every op early-returns when
`direction == 0`. `tickWrap`/`tickHold` already branch on direction — keep their bodies; for
pendulum, `direction` is the freeze gate only (travel stays governed by the caller-owned
`pendulumDir`), so non-zero direction proceeds and `pendulumDir` is seeded/owned by the caller.
Keep `tickWrap`'s modulo and `tickRTZ` as-is. Update PhaseFlux's `advanceCounter` to pass
`(direction, magnitude)` derived from its signed step (it already computes both) instead of a signed
step into pendulum; this is mechanical and behavior-preserving. **No PhaseFlux user-facing / model /
UI change** — the signed-step UX and `AccumulatorConfig` are untouched.

**Execution note:** Test-first — extend `TestAccumulatorOps.cpp` for the new direction/freeze
contract before changing signatures.

**Patterns to follow:** existing `TestAccumulatorOps.cpp` structure; PhaseFlux's existing
sign→direction/magnitude decomposition in `advanceCounter`.

**Test scenarios:**
- `direction == 0` is a no-op for every op (wrap/pendulum/hold/random/rtz): counter unchanged.
- Wrap: ascend overflow wraps to min side; descend underflow wraps to max side; `step ≤ range`
  single overshoot lands identically to the pre-unification result; `step > range` wraps within
  `[min,max]` (the corrected case).
- Pendulum: reflects at max and flips `pendulumDir`; reflects at min and flips back; down-drift
  (caller seeds `pendulumDir = -1`) bounces symmetrically; clamps to exact min/max on hit.
- Hold: pins at the active-direction limit; opposite-direction limit untouched.
- Random: stays within `[min,max]` across many calls; `min == max` returns that value.
- RTZ: snaps to 0 only when out of range; in-range counter untouched.
- Test expectation: PhaseFlux engine tests (`TestPhaseFluxTrackEngine.cpp`) stay green — the call
  site change is behavior-preserving.

**Verification:** `TestAccumulatorOps` green incl. the freeze no-op + down-drift pendulum;
`TestPhaseFluxTrackEngine` unchanged; STM32 release build clean.

---

### U2. Route NoteTrack's Accumulator through the unified ops

**Goal:** replace NoteTrack's duplicated boundary math with calls to `AccumulatorOps`, preserving
all surrounding behavior.

**Dependencies:** U1.

**Files:**
- `src/apps/sequencer/model/Accumulator.cpp` (the `tickWith*` bodies, `58-118`)
- `src/apps/sequencer/model/Accumulator.h` (only if a member needs to be passed by non-const ref)
- `src/tests/unit/sequencer/TestAccumulator.cpp`

**Approach:** Replace the bodies of `tickWithWrap` / `tickWithPendulum` / `tickWithHold` /
`tickWithRandom` with calls to the matching `AccumulatorOps::tick*`, mapping
`Direction{Up→+1, Down→-1, Freeze→0}` and passing `_stepValue` as the unsigned magnitude. Pass
`_currentValue` / `_pendulumDirection` by reference (resolve the existing `const_cast`-on-`this`
pattern as needed — likely make the tick helpers non-const, or hold the refs locally). Keep the
`tick()` dispatch (`36-49`), the delayed-first-tick `_hasStarted` skip (`30-34`), `reset()`
(`52-56`), and serialization (`120-161`) exactly as-is — the ops absorb only the innermost
per-order math. The `Freeze` guard moves into the unified ops via `direction == 0` (no separate
guard needed once mapping is in place).

**Execution note:** Characterization-first — before refactoring, capture golden output sequences
from the *current* `Accumulator` across the matrix (each Order × Up/Down/Freeze × representative
bounds × `step ≤ range` and `step > range`). The refactor must reproduce them exactly, except the
documented Wrap `step > range` case (Key Decisions), which the golden test asserts as the corrected
value.

**Patterns to follow:** PhaseFlux's `advanceCounter` (how it maps to the ops); existing
`TestAccumulator.cpp` for the class-level harness.

**Test scenarios:**
- Faithfulness (the behavior-preservation gate): for each Order, the delegated `Accumulator`
  produces the same `_currentValue` stream as the captured golden for `step ≤ range`.
- Wrap `step > range`: delegated value is the modulo-correct in-range value (documented change), not
  the old out-of-range value.
- Pendulum: Up and Down both travel by `_pendulumDirection` from +1 and bounce identically to today;
  `Freeze` holds the value (delayed-start and reset semantics intact).
- Freeze on every order: no advance.
- Delayed first tick: the first `tick()` after construction/reset does not move the counter.
- `reset()` restores `_currentValue = _minValue`, `_pendulumDirection = 1`, `_hasStarted = false`.
- Serialization roundtrip unchanged (write→read preserves all fields).
- Test expectation: `TestAccumulator` plus the new faithfulness/golden cases green; no change to
  `NoteTrackEngine` behavior (its per-step override / trigger-mode / spread-copy code is untouched).

**Verification:** `TestAccumulator` + faithfulness golden green; NoteTrack accumulator plays
identically (spot-check a Wrap and a Pendulum sequence in sim); STM32 release build clean; the five
`tickWith*` bodies now delegate (no duplicated boundary math remains).

---

## Scope Boundaries

In scope: unify the five `AccumulatorOps` on `(direction, magnitude)` with freeze as `0`; route
NoteTrack's four tick methods through them; update PhaseFlux's internal call site to match.

Out of scope:
- Any PhaseFlux **user-facing / feature / UI / model** change — signed-step UX stays, no `Direction`
  control, no `AccumulatorConfig` field added.
- Restructuring NoteTrack to PhaseFlux's engine-array / `AccumulatorConfig` model — NoteTrack keeps
  its stateful copyable `Accumulator` object, whole-track scope, per-step step/direction overrides
  (`NoteTrackEngine.cpp:569-590/636-655/687-705`), the three trigger modes (Step/Gate/Retrigger),
  the spread-mode copy-and-tick (`.cpp:744-748`), and its own serialization.
- Adding `RTZ` to NoteTrack's `Order` enum (the op exists; not wired unless trivial).
- Per-step override commands, `Random` drunken-walk, and the other `accumulator-v2-spec.md` §13.7
  deferrals.

---

## System-Wide Impact

- **Shared math (both tracks):** `AccumulatorOps` signatures change. PhaseFlux (the existing caller)
  and NoteTrack (the new caller) both move to the unified convention; both guarded by their engine
  tests.
- **No serialization change:** NoteTrack's `Accumulator` read/write and PhaseFlux's
  `AccumulatorConfig` are untouched — no ProjectVersion bump.
- **One user-reachable behavior change:** Wrap with `step > range` now wraps correctly instead of
  going out of range (latent-bug fix). All other behavior identical.

---

## Risks & Mitigation

- **Pendulum divergence (highest watch, low residual risk).** The signed-step vs explicit-direction
  mismatch is the reason for the unification. Mitigation: the `Freeze`-guard-preserved + positive-
  magnitude analysis shows bit-identical; the faithfulness golden (U2) + the down-drift pendulum op
  test (U1) are the proof. If any reachable input diverges, reconcile in the op (the op is the
  canonical extraction) — do not fork.
- **`const_cast`-on-`this` removal.** NoteTrack's tick helpers are `const` and mutate via
  `const_cast`. Passing members by ref to the ops means making the helpers non-const or taking local
  refs. Mitigation: keep the change local to `Accumulator`; `tick()` is already called from non-const
  contexts in the engine — verify the call chain compiles.
- **Random rng instance shift.** Delegation moves Random to the helper's static rng. Mitigation:
  faithfulness for Random is range-only (non-deterministic by design); documented in Key Decisions.

---

## Verification (whole plan)

- `TestAccumulatorOps` (unified contract incl. freeze no-op, down-drift pendulum, wrap overshoot
  corrected) + `TestAccumulator` (faithfulness golden, behavior preserved) green.
- `TestPhaseFluxTrackEngine` unchanged and green.
- STM32 release build clean (`make -C build/stm32/release sequencer`).
- The five `Accumulator::tickWith*` bodies contain no boundary math of their own — only mapped calls
  into `AccumulatorOps`. One implementation remains.
- Manual sim spot-check: a NoteTrack Wrap accumulator and a Pendulum accumulator drift identically
  to before.
