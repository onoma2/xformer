---
id: align-vs-reset-routing
schema: subspec
title: "sub-spec: routing re-anchor = restart/align (routable); reset = lifecycle (non-routable)"
type: refactor
status: active
scope: now
date: 2026-06-02
parent: routing-mod-matrix-overhaul
related:
  - wallclock-time-architecture
supersedes: "earlier draft of this file (\"universalize Sync as a warm reset inlet\")"
---

# Sub-spec: align vs reset in routing

Sub-spec of the routing mod-matrix overhaul
(`2026-05-31-002-routing-mod-matrix-overhaul-plan.md`). Settles what the routing
matrix's re-anchor target means, which the overhaul forces (the Indexed →
`DiscreteMapSync` borrow can't be resolved without it).

## The frame (owner)

Two distinct operations, by their trigger and lifetime — not by "warm vs cold":

- **restart = pointer / time-base alignment.** Snap a track's playhead/phase to a
  reference. A *musical* gesture: re-align drifted/modulated/divided tracks to an
  arbitrary source without slaving tempo. **This is the routable one.**
- **reset = lifecycle.** Play/stop, project load, transport start, panic. A full
  re-init of internal state. **Not a routing target** — it belongs to transport
  and project lifecycle, not the modulation matrix.

The earlier draft of this file framed it as "warm the Reset target" — muddier.
The clean rule: **routing only ever triggers `restart` (align); `reset` is
lifecycle.**

## Original-Performer grounding

- `resetMeasure` calling the cold `reset()` is **upstream behavior, inherited**
  (`temp-ref/original-performer/.../NoteTrackEngine.cpp:127`), not a fork bug.
- Upstream `restart()` is genuinely minimal — three lines (free-tick, sequence
  state, current step). That *is* the "wrap, keep continuity" alignment we want;
  it already exists, it was just never the thing `resetMeasure` called.
- `SyncMode` / External sync / `_routedSync` are **entirely fork constructs**
  (DiscreteMap/Indexed) layered on top; upstream has none of it.

## NOW — routing scope (this overhaul)

Decisions that shape the registry / U7 cutover. No per-track engine surgery.

1. **The routable re-anchor is an `Align` target that binds `restart()`**, never
   `reset()`. `reset()` is not exposed as a routing target.
2. **External sync folds into Align** — "external edge → restart" *is* Align. So
   `SyncMode::External` as a separate per-track feature retires, and the Indexed →
   `DiscreteMapSync` **borrow dies**.
3. **Drop the `DiscreteMapSync` inlet row** from the DiscreteMap param table
   (`ParamTableDiscreteMap`). Input/Scanner stay (genuinely-unique CV inlets).
   `ParamKey::DiscreteMapSync` (104) is **retired, not reused** (append-only, F6).
   The model field `_routedSync` + `setRoutedSync` stay until the deferred engine
   task removes External sync.
4. **Align is universal** — every track can be aligned; it is owned by no single
   track (no borrow because nobody borrows). Its apply binding (→ each engine's
   `restart()`) is wired at the U7 cutover alongside the other target bindings.

## DEFER — engine semantics (separate task, pairs with wallclock/drift)

What `reset()` vs `restart()` actually touch, per track — out of scope for routing:

- **Trim each track's `restart()` to minimal pointer/time-base alignment.** The
  fork's drifted heavy (Note clears Re:Rene state, Curve wipes LPF/chaos, Indexed
  dumps CV, Stochastic re-seeds its RNG) vs upstream's 3-line restart. Decide the
  canonical "align = reposition, keep continuity" content per engine, especially
  the CV/DSP tracks where a clobber is audible (blip, dropped note, reset
  evolution).
- **Repoint `resetMeasure`** (and any internal periodic re-anchor) from `reset()`
  → `restart()` — the bar-aligned *wrap* it should have been.
- **Tuesday** `restart()` currently calls `reset()` → give it a real minimal
  restart.
- **Stochastic** RNG re-seed on restart → decide (deterministic replay vs preserve
  generative position).
- **Define `reset()` canonically as lifecycle** (play/stop/load/transport) and stop
  other paths from invoking it.
- Retire `SyncMode::External` + `_routedSync` in both DiscreteMap and Indexed once
  Align covers it.

## Out of scope

- Renaming the C++ `reset()`/`restart()` symbols — they already encode the
  cold/light pair; the work is *what they do* and *who calls them*, not their names.
