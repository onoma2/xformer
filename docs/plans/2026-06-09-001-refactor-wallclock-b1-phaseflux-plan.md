---
id: refactor-wallclock-b1-phaseflux-plan
schema: plan
title: "refactor: wallclock B1 (MidiCv → WallClock) + A (PhaseFlux phase → tickPosition)"
type: refactor
status: completed
date: 2026-06-09
resolved: 2026-06-09
resolution: closed-as-investigated
depth: standard
origin: TASK.md
---

> **Resolution (2026-06-09): closed as investigated — no code shipped.** Both units'
> premises failed on contact with the code. **U1 (B1):** `os::ticks()` is load-bearing —
> the retrigger gate pulse is quantized to the ~1 ms os-tick via wrap arithmetic; µs would
> collapse it. Won't migrate. **U2 (A):** PhaseFlux already samples its curve every PPQN
> tick (~2.6 ms zero-order-hold staircase @120 BPM, sub-audible); `tickPosition` migration
> buys ~nothing and is entangled in `deriveTickPosition`; the cheap path for smoothness, if
> ever wanted, is `update(dt)` slew. **U3:** Stochastic is event-triggered, no continuous
> phase. Full reasoning + cadence data in `TASK.md`. B2 (slave-clock guard, hardware-gated)
> remains the only actionable wallclock item.

# refactor: wallclock B1 + A (PhaseFlux phase → tickPosition)

Two independent timing cleanups from `TASK.md`, each shippable as its own commit.
**B2 (slave-clock guard) is explicitly out of scope** — it carries a hardware-audition
gate and is left for a separate effort.

Guiding constraint (from `TASK.md`, validated against 4MS Catalyst `clock.hh`):
**32-bit wrap-safe deltas, no 64-bit, two clocks kept separate** (wall-time vs musical
tick). No `ProjectVersion` bump — no serialized layout changes here.

---

## Problem Frame

xformer has two correct time bases that a few sites don't yet use consistently:

- **Wall time** — `WallClock` (32-bit µs, `engine/WallClock.h`), already adopted for the
  modulator `dt` and the MIDI CC rate-limit. One behavioral site still reads raw
  `os::ticks()` directly: MidiCv voice scheduling.
- **Musical tick** — `Clock::tickPosition()` (continuous float), used by six track
  engines for sub-tick-smooth phase. PhaseFlux and Stochastic instead derive phase from
  the **integer** tick passed to `tick(uint32_t)`, so their continuous phase is quantized
  to whole ticks.

Neither is a bug today; both are consistency/quality gaps. B1 unifies the last raw
wall-time read. A gives PhaseFlux's per-stage curves the same sub-tick smoothness the
other engines already have.

---

## Scope Boundaries

In scope:
- B1 — MidiCv voice scheduling onto `WallClock`.
- A — PhaseFlux **continuous phase fraction** onto `Clock::tickPosition()`; discrete
  event scheduling stays integer-tick.
- A — a bounded investigation of whether Stochastic has any continuous-phase consumer
  worth migrating (decision recorded, not necessarily code).

### Deferred to Follow-Up Work
- **B2** — slave-clock 0.5×–2× outlier latch (needs a jittery-clock hardware audition;
  also folds in the parked `performer-improvements` slave-clock filter).
- **Stochastic phase migration** — only if the A3 investigation finds a real continuous
  consumer; otherwise closed as "stays integer-tick by design."

### Out of scope (non-goals)
- Any 64-bit time service or software counter-extension.
- Unifying wall-time and musical tick into one counter (Catalyst can; xformer can't —
  Free(Hz) modulators are tempo-independent).
- Touching the six engines already on `tickPosition()`.

---

## Key Technical Decisions

- **B1 reuses the existing `WallClock`**, not a new type. The voice already stores a
  `uint32_t` timestamp; this is a like-for-like source swap (`os::ticks()` →
  `WallClock::now()`) keeping the same wrap-safe delta comparison.
- **A is surgical, not a blanket swap.** In `PhaseFluxTrackEngine::tick`, only the
  continuous phase fraction (currently `relativeTick % cycleTicks / cycleTicks`) reads
  `tickPosition()`. Discrete logic — reset at `relativeTick % resetDivisor == 0`, cycle
  boundary detection, schedule rebuild — stays on the integer `tick`, or triggers smear.
- **Stochastic is event-triggered by nature** (it computes trigger steps at integer-tick
  boundaries from a LUT). It is not assumed to have a continuous phase to migrate; A3
  decides this from the code before any Stochastic change is scoped.
- **Reference pattern**: mirror how `CurveTrackEngine` reads `_engine.clock().tickPosition()`
  for its smooth phase (`CurveTrackEngine.cpp:99,133,190`).

---

## Implementation Units

### U1. B1 — MidiCv voice scheduling onto WallClock
**Goal:** Replace the two raw `os::ticks()` reads in MidiCv voice scheduling with
`WallClock`, so all musical wall-time shares one wrap-safe source.
**Dependencies:** none.
**Files:**
- `src/apps/sequencer/engine/MidiCvTrackEngine.cpp` (lines ~113 delay check, ~215 voice stamp)
- `src/apps/sequencer/engine/MidiCvTrackEngine.h` (if a `WallClock` member/handle is needed)
**Approach:** The voice stores a `uint32_t ticks` stamp and tests
`(voice.ticks - os::ticks()) >= delay`. Swap the source to `WallClock::now()` (µs),
ensuring `delay` is expressed in the same µs unit. Keep the unsigned wrap-safe
subtraction. Confirm whether `delay` is currently in `os::ticks()` units and convert if
the unit changes. Do **not** introduce a `WallTimer` unless it reads cleaner than the
existing delta — a plain `WallClock().now()` swap is sufficient and minimal.
**Patterns to follow:** `MidiOutputEngine.cpp:30` (`WallClock().now()`, wrap-safe delta).
**Test scenarios:**
- Behavioral parity: a held MidiCv note's on/off and any retrigger delay timing is
  unchanged vs the `os::ticks()` baseline (same audible/measured timing at a given tempo).
- Edge: the delay comparison still behaves correctly across a 32-bit µs wrap (delta < wrap
  window) — reason about it; `delay` intervals are ms so the wrap is never straddled.
- `Test expectation: no new unit test` — this is voice-scheduling timing with no isolated
  seam; verification is behavioral in sim + on hardware. Note the reasoning in the commit.
**Verification:** sim builds and runs; STM32 release builds; MidiCv note timing audibly
unchanged at multiple tempos; the only `os::ticks()` references left in the engine are the
non-timing ones (uptime stat, RNG seeds, `UpdateReducer`).

### U2. A1 — PhaseFlux continuous phase onto Clock::tickPosition()
**Goal:** Give PhaseFlux's continuous phase fraction sub-tick smoothness by reading
`Clock::tickPosition()`, matching the six engines already on it; keep all discrete event
logic on the integer tick.
**Dependencies:** none (independent of U1).
**Files:**
- `src/apps/sequencer/engine/PhaseFluxTrackEngine.cpp` (`tick(uint32_t)` ~555+, the
  `cyclePhase`/`relativeTick % cycleTicks` fraction ~598,643)
- `src/apps/sequencer/engine/PhaseFluxTrackEngine.h` (only if a cached phase field changes)
**Approach:** Identify the exact reads where `relativeTick` is converted to a 0..1 cycle
fraction for curve sampling, and source that fraction from `tickPosition()` (offset by the
same `_resetTickOffset`, scaled by `cycleTicks`) instead of the integer modulo. Leave reset
detection (`relativeTick % resetDivisor == 0`), schedule rebuild, and cycle-boundary
counting on the integer `tick` — those are discrete events. The continuous fraction and the
integer event index must stay consistent at tick boundaries (at integer ticks,
`tickPosition()` fraction must equal the integer-derived fraction).
**Execution note:** Characterize the current integer-phase output at a few tick positions
before changing it, so parity at integer boundaries is provable.
**Patterns to follow:** `CurveTrackEngine.cpp:99,133,190` (continuous `tickPosition()` phase).
**Test scenarios:**
- Parity at boundaries: at every integer tick, the new continuous fraction equals the old
  `relativeTick % cycleTicks / cycleTicks` (no drift introduced at the points that already
  worked).
- Sub-tick smoothness: between two integer ticks, the fraction advances monotonically
  (the gain this unit exists for) rather than stepping.
- Reset behavior unchanged: a reset at `relativeTick % resetDivisor == 0` still zeroes the
  phase at the same tick as before (events did not move).
- Edge: `cycleTicks == 0` / degenerate divisor path still guards against divide-by-zero
  exactly as today.
- Edge: warped/shifted cycle path (the `warped * cycleTicks` branch ~600) still maps phase
  consistently when fed continuous input.
**Verification:** sim builds and runs; STM32 release builds; PhaseFlux curve output is
smooth across a cycle with no audible discontinuity or trigger-timing shift vs baseline at
multiple tempos and divisors.

### U3. A3 — Stochastic continuous-phase investigation (decision unit)
**Goal:** Determine from the code whether `StochasticTrackEngine` has any continuous-phase
consumer that would benefit from `tickPosition()`, and record the decision. Do not migrate
blindly.
**Dependencies:** none (can run before or alongside U2).
**Files:**
- `src/apps/sequencer/engine/StochasticTrackEngine.cpp` (`tick(uint32_t)` ~204,
  `triggerStep` ~261, divisor math ~836–851) — read only, to classify.
- Plan/decision recorded in `TASK.md` (update the wallclock section) — not necessarily code.
**Approach:** Trace every use of the integer `tick`/`relativeTick` in Stochastic. Classify
each as discrete event scheduling (stays integer) vs continuous phase (candidate for
`tickPosition()`). Stochastic is expected to be entirely event-triggered (LUT-driven trigger
steps), in which case the conclusion is "stays integer-tick by design" and Stochastic is
dropped from A. If a genuine continuous consumer exists (e.g., a smoothly-sampled curve or
ramp over the step duration), scope it as a follow-up unit mirroring U2.
**Test scenarios:** `Test expectation: none — investigation/decision unit.` Outcome is a
documented classification, not behavior change.
**Verification:** a written decision in `TASK.md`: either "Stochastic stays integer-tick
(no continuous-phase consumer)" with the traced reasoning, or a scoped follow-up unit for
the specific consumer found.

---

## System-Wide Impact

- **Engine timing only.** No model/serialization changes, no UI, no `ProjectVersion` bump.
- B1 touches MIDI-CV output timing; A1 touches PhaseFlux phase sampling. They share no
  files or state and can land in either order.
- Risk concentrates in A1's boundary parity (continuous vs integer at tick edges) and in
  B1's `delay` unit conversion if it was in `os::ticks()` rather than µs.

---

## Deferred Implementation Notes

- B1: exact unit of `voice` `delay` (os-ticks vs µs) is confirmed when touching the code;
  convert if the source unit changes.
- A1: the precise set of `tickPosition()` reads vs integer reads is finalized against the
  live `tick()` body; the plan names the phase-fraction sites but the implementer confirms
  each branch (normal, shifted, warped, reset) at edit time.
- A3 may convert into a real migration unit or close Stochastic out — that fork resolves
  during the investigation, not now.

---

## Verification Strategy

Per unit: sim debug build + run, STM32 release build (flash well under the 983040 ceiling;
this work is ~neutral on size). A1 gets an explicit integer-boundary parity check before
and after. B1 and A1 each ship as their own commit; A3 is a decision recorded in `TASK.md`.
