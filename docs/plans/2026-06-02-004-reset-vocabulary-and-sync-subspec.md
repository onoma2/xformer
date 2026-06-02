---
id: reset-vocabulary-and-universal-sync
schema: subspec
title: "sub-spec: disambiguate \"reset\" + universalize Sync as a warm re-anchor inlet"
type: refactor
status: active
scope: now
date: 2026-06-02
parent: routing-mod-matrix-overhaul
related:
  - wallclock-time-architecture
---

# Sub-spec: reset vocabulary + universal Sync inlet

Sub-spec of the routing mod-matrix overhaul
(`2026-05-31-002-routing-mod-matrix-overhaul-plan.md`). **Scope: NOW** — this is
active design intent for the routing surface, not a deferred parking-lot item. It
settles what "Sync" means and how it differs from "Reset", which the routing
overhaul forces (the Indexed → `DiscreteMapSync` borrow can't be resolved without
deciding the concept).

## Problem

"reset" is overloaded across at least five genuinely different operations, on
three different layers, and `reset()` itself clears a different set of state in
every track engine. The routing overhaul exposed two consequences:

- the Indexed track **borrows** DiscreteMap's `DiscreteMapSync` target to drive
  its External sync (an R13 violation), and
- External sync's purpose (drift-correct re-anchor) is conflated with the cold
  full reset its implementation actually calls.

## The vocabulary (the five families)

| Operation | Layer | What it does | Weight |
|---|---|---|---|
| `clear()` / init | Model | Wipe a sequence/track to factory defaults (content gone). | wipe |
| `Engine::reset()` (`Engine.cpp:816`) | Transport · global | Cold-reset every track engine + MIDI-out + modulators. Boot, `Clock::Reset` event. | cold · all |
| `Engine::clockReset()` (`Engine.cpp:320`) | Transport · clock | Zero the master clock tick/phase + routing shaper state. Not sequence content. | clock phase |
| `TrackEngine::reset()` (virtual, `TrackEngine.h:57`) | Per-track | **COLD.** Cursor→start, clear CV/gate, queues, accumulators, seek state, record history, then `changePattern()`. Called by `Engine::reset()` and the routed **Reset** target. | cold |
| `TrackEngine::restart()` (virtual, `TrackEngine.h:58`) | Per-track | **WARM.** Cursor→first step only; keep CV/gate, queues, accumulators; no `changePattern()`. Called on song-slot advance (`Engine.cpp:999`). | warm |
| `resetMeasure` (config) | Model | A setting, not an act: "auto re-anchor every N bars." A schedule that fires a restart. | config |

Smaller resets in the same family: `SequenceState::reset()` (run-mode cursor
only), `Accumulator::reset()` (back to minValue), `Engine::tapTempoReset()`
(averaging buffer — unrelated to playback), `resetSequenceState()`
(Indexed/DiscreteMap inner reset-to-first-step shared by `reset()` and External
sync, e.g. `IndexedTrackEngine.cpp:141`).

## "Full reset" is per-track

The same `reset()` call clears an engine-specific state set — "reset the track"
means a different thing each time:

- **Note** — Re:Rene seek X/Y + divisors, accumulators, record history, gate/CV
  queues, pulse counter (`NoteTrackEngine.cpp:104`).
- **Indexed** — re-*rolls* the runMode advance (RNG picks the next step), step/gate
  timers, slide, steps-remaining (`IndexedTrackEngine.cpp:141`).
- **DiscreteMap** — internal ramp phase → −5V, prev-input crossing, pluck envelope,
  re-samples octave/transpose/rootNote, threshold cache.
- **Curve / Tuesday / Stochastic / PhaseFlux** — each its own phase/shape/seed
  state; no shared definition of "cleared."

## The musical mismatch

The drift-correction "re-anchor the first tick" use case (free-running / phase-
modulated / divided tracks accumulating phase drift, re-aligned to an arbitrary
source **without** slaving tempo) is **`restart()` semantics** — reposition the
playhead, keep CV/queues/accumulators flowing.

But External sync today reaches for the **cold** `resetSequenceState()` /
`reset()`-shaped path, which dumps output and runtime state — an audible jolt,
worst on the continuous tracks (Curve, PhaseFlux, DiscreteMap). So "universalize
Sync" is **not** just porting the existing branch to all tracks; it is binding
Sync to *warm* re-anchor, not cold reset.

## Resolution (NOW)

1. **Adopt a five-name vocabulary** so no label overloads:
   - **Clear / Init** — wipe content to defaults (model).
   - **Clock Reset** — zero the master clock phase (tempo, not notes).
   - **Reset** — cold per-track / global, back-to-top, clears output (transport +
     the routing Reset target).
   - **Restart / Rewind** — warm reposition, cursor→first step, keep continuity.
   - **Sync / Re-anchor** — external first-tick realign for drift; warm, per-track,
     universal.

2. **Universalize Sync as a warm re-anchor inlet** — a per-track input that, on an
   external CV rising edge, re-anchors *that track's* first tick using
   `restart()`-class (warm) semantics, not `reset()`. Conceptual sibling of
   `resetMeasure` (internal periodic re-anchor); every track gets it, exactly as
   every track has `resetMeasure` today.

3. **In the routing matrix:** Sync is a **universal inlet/target** (Tier-1-ish),
   owned by no single track. This dissolves the Indexed → `DiscreteMapSync` borrow
   **by promotion** (nobody borrows; everyone owns Sync), and retires DiscreteMap's
   special ownership of a Sync target. DiscreteMap keeps **Input/Scanner** — its
   genuinely-unique CV-input + scanner-position inlets, which have no universal
   equivalent.

4. **Distinct from Reset:** the routing surface keeps both — **Reset** (cold,
   hard clear on edge) and **Sync** (warm, first-tick re-anchor on edge) — as
   separate targets/inlets. They are not duplicates once Sync is warm.

## Open / to settle at build

- **Per-track warm semantics.** `restart()` exists on every engine, but its exact
  content varies; confirm each engine's `restart()` is the right "first-tick,
  no-jolt" behavior for Sync, especially the continuous tracks (Curve/PhaseFlux/
  DiscreteMap) where a cold clear is audible. Some may need a lighter phase-only
  re-anchor than today's `restart()`.
- **Storage.** Today External sync rides a per-sequence `SyncMode` enum
  (Off/ResetMeasure/External) on DiscreteMap + Indexed only. Universalizing means a
  per-track sync inlet + (optionally) a per-track enable, replacing the borrowed
  `_routedSync` plumbing.
- **UI.** The Indexed sequence page "Sync" chip (Off/Reset/Ext) and the DiscreteMap
  equivalent get reconciled with the universal model; the "Ext" option becomes the
  universal Sync inlet rather than a per-track mode.
- **Drift linkage.** Pairs with `wallclock-time-architecture`
  (`docs/plans/2026-05-29-wallclock-time-architecture-design.md`) — PhaseFlux +
  Stochastic re-deriving phase from discrete tick is exactly the drift Sync
  corrects.

## Out of scope

- Renaming the existing C++ symbols (`reset()`/`restart()`/`clear()`) — the
  vocabulary is for the *product/UI surface and routing targets*, not a code-wide
  rename. Engine `reset()`/`restart()` stay as the warm/cold pair they already are.
