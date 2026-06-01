---
id: routed-value-storage-modulation
schema: subspec
title: "sub-spec: routed value storage — drop per-pattern duplication, decide replace-vs-modulate"
type: refactor
status: draft
date: 2026-06-01
parent: routing-mod-matrix-overhaul
---

# Sub-spec: routed value storage (Routable redesign)

Sub-spec of the routing mod-matrix overhaul
(`2026-05-31-002-routing-mod-matrix-overhaul-plan.md`). Scopes one decision the
overhaul forces but the parent plan does not settle: **where a routed value
lives, and whether routing replaces or modulates the base.**

---

## Problem (confirmed in code)

A routable param is a `Routable<T>` — a union of `{ base, routed }` (Routing.h
`template Routable`). For **sequence-level** params it lives inside each
`Sequence`, and a track owns `CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT` = 17
sequences (Config.h:51-52; `NoteTrack::NoteSequenceArray`).

NoteSequence carries 7 such fields (`_scale,_rootNote,_divisor,_clockMultiplier,
_runMode,_firstStep,_lastStep`, NoteSequence.h:794-805); CurveSequence carries 12.

Two redundancies result:

1. **Storage.** The `routed` half exists ×17 per track per param. A routed value
   is logically *one per track* — `Routing::writeTarget` loops all 17 patterns
   (`for patternIndex < CONFIG_PATTERN_COUNT: sequence(p).writeRouted(...)`,
   Routing.cpp) writing the **same** value, so the 17 routed slots are forced
   equal. Order ~1–2 KB of routed slots that are identical copies when active,
   multiplied through the 8-track container.

2. **CPU.** The per-pattern copy loop runs on every routing recompute, writing
   one value into 17 places.

3. **Semantics.** `get(isRouted())` returns base **or** routed — mutually
   exclusive. A live route **replaces** base; the per-pattern base is dead while
   routed. So routing a per-pattern param flattens all 17 patterns to one value,
   which makes the per-pattern storage of that param pointless precisely when it
   is being driven.

Root cause: the `routed` slot is **transient modulation state stored as
per-pattern model data.** Base is saved project data and legitimately differs per
pattern (pattern 1 C-major, pattern 2 D-minor); the routed value is a single live
per-track route output that has no reason to be per-pattern.

### Forward-designed base-only fields (Stochastic, PhaseFlux)

Not every routed target has a `routed` slot, and that is **deliberate**.
**Stochastic Scale/RootNote/Divisor and PhaseFlux Scale/RootNote are plain base
fields** (`int8_t _scale` etc., StochasticSequence.h:28-42, PhaseFluxSequence.h:341-353)
— intentionally left out of the `Routable` convention because these types were
designed anticipating this overhaul: they are *already* in the target shape
(base-only), waiting for the per-track override table rather than carrying the
redundant per-pattern `routed` half.

Consequence for now: `writeTarget` routes them by writing **base** on all patterns
(`setScale(intValue)` with no routed flag). That is the **interim** — the override
table is exactly what lets these route without touching base. (Open: until the
override lands, a route active at save time writes the value into the serialized
base; flag whether that interim needs a guard, or whether these targets simply do
not go live until the table exists.)

This is why the migration inventory below classifies by **current storage**, built
from `writeTarget` + every `writeRouted` — a `Routable<>` scan would miss the
base-only fields entirely.

---

## Proposed shape

- `Sequence` keeps **base only** (`int8_t _scale` instead of
  `Routable<int8_t> _scale`).
- The **routed value lives once per `(track, paramKey)`** in engine RAM —
  transient, CCMRAM, not serialized (it is modulation, not project data).
- Read combines base + the live route value at access time.
- The per-pattern copy loop is deleted; the ×17 routed storage is dropped.

This is independent of the replace-vs-modulate choice below: **both** combine
rules store the 17 bases (already present) + one per-track value, and both kill
the loop and the duplication.

---

## Combine semantics — replace vs modulate (owner decision)

**Does routing replace the base, or modulate around it?** Per-param, via a
`RouteParam::Flag` (`Modulate` vs `Absolute`):

- **Absolute (today's only mode).** read = `routeActive ? routeValue : base`. The
  route owns the value; base ignored while routed. Window bounds the absolute value.
- **Modulate.** read = `clamp(base_p + offset, hardMin, hardMax)`. Base stays the
  per-pattern origin; the route shifts around it. Per-pattern base stays meaningful
  while routed.

The existing ranges already sort this: several params are **already bipolar**
(designed as modulators, implemented as replace), others are absolute indices.

### Offset-space contract (resolves review finding 1)

Modulate is **not** "reinterpret the current window as an offset" — that would turn
Divisor's `6..24` window into "+6..+24 ticks always" (a regression). Each Modulate
param declares an explicit offset space: **neutral = 0**, a **signed offset window**,
and a **final absolute clamp** after `base + offset`. Source polarity is bipolar
(0.5 source = no change). Values from `targetInfos` (min..max, default minDef..maxDef):

| Param | Storage | Today min..max (def) | Combine | Neutral | Offset window | Final clamp |
|---|---|---|---|---|---|---|
| Octave | Track Routable | -10..10 (-1..1) | **Modulate** | 0 | ± window | valid octave |
| Transpose | Track Routable | -60..60 (-12..12) | **Modulate** | 0 | ± | pitch range |
| Offset (Curve) | Routable | -500..500 (-100..100) | **Modulate** | 0 | ± | value range |
| Rotate | Routable | -64..64 (0..64) | **Modulate** | 0 | ± | step count |
| Gate/Retrig/Length/Note/Shape Bias | Routable | -8..8 | **Modulate** | 0 | ± | bias range |
| SlideTime | Routable | 0..100 | Absolute | — | — | — |
| Divisor | Seq Routable | 1..768 (6..24) | Absolute | — | — | — |
| ClockMult | Seq Routable | 50..150 | Absolute (mult, not additive) | — | — | — |
| Scale / RootNote | Routable (Note/Curve) / base-only (Stoch, PF) | 0..23 / 0..11 | Absolute (index) | — | — | — |
| RunMode | Seq Routable | 0..5 enum | Absolute | — | — | — |
| FirstStep / LastStep | Seq Routable | 0..63 | Absolute | — | — | — |
| Mute / Pattern / Fill | PlayState | — | Absolute | — | — | — |

### Modulate window representation (resolves review finding 2 — neutral is enforceable)

A free `min..max` window cannot guarantee neutral: a stored window of `+1..+3`
makes source 0.5 produce `+2`, i.e. constant drift, not "no change." So Modulate
does **not** reuse the absolute `min/max` pair. A Modulate param stores a **single
signed depth magnitude `d`**; the offset is

```
offset = d * (source_centered)        // source_centered ∈ [-1, +1], 0.5 source → 0
read   = clamp(base + offset, hardMin, hardMax)
```

Neutral is **structural**, not a user responsibility: the window is symmetric ±d by
construction, source-center is always 0, and `d`'s sign sets polarity. Absolute
params keep the existing `min/max` pair unchanged. The UI for a Modulate route edits
one depth value, not a min..max span — so an asymmetric "+1..+3" window is
unrepresentable for a Modulate param. (`biasPct`/`depthPct` and the shaper still
apply to the source before the ±d map; they shift the source within its bipolar
range, they do not break neutral since 0.5-centered source still maps to 0.)

The bipolar absolute ranges (Octave -10..10, Transpose -60..60, biases -8..8) inform
`d`'s default magnitude (e.g. half-range), but Modulate's stored form is `d`, not the
pair. **Owner decision is the column assignment** (which params modulate); the
representation and per-param flag are settled. Clean-break format (parent R9) means
no legacy routes to migrate, so replace→modulate is free at load.

Tests required: source 0.5 → exactly base (every Modulate param), full +d and −d at
base hard min/max (clamp holds), negative `d` inverts polarity, and that an
asymmetric window cannot be constructed for a Modulate param.

## Routed-value ownership — DECIDED: model-owned transient override table (resolves finding 2)

The live routed value lives in a **per-track override table in the Model, not
serialized** — chosen over an engine routed-value service because it preserves the
current layering (UI reads model getters only; no model→engine call) and matches
where routed data lives today (the `Routable.routed` slot is already model RAM the
engine writes).

- **Owner:** the Model. One transient (non-serialized) per-track table keyed by
  `paramKey`, sized to the routable-param count. Replaces the per-pattern `routed`
  half and the Stochastic base-mutation.
- **Writer:** the engine apply path (single-pass recompute), exactly where
  `writeTarget` writes today. Values are word-sized scalars → same write/read
  concurrency as today's `Routable.routed`; no new lock. Structural changes already
  occur under the engine suspend/lock.
- **Active-route gating:** the existing active-route bit (`isRouted`, currently a
  static set) selects override-vs-base at read; unchanged in kind.
- **Invalidation:** cleared on **track-mode change** (the `updateTrackSetups`
  rebuild — the new type has different params) and on **route deactivation/reconfig**
  (clear the param's override + active bit, mirroring the trigger `gateMask` re-arm).
- **Inactive default:** no active route → getter returns base. No stale read because
  the active bit gates it.

This keeps the model the single read API for UI, file, and clipboard paths.

## Migration inventory (replaces blast-radius; resolves finding 3)

**Storage kind is per `(param × track type)`, not per Target** — the flat enum
cannot be classified once. `Scale` is sequence-`Routable` in Note/Curve but a plain
base field in Stochastic/PhaseFlux; `Octave` is track-`Routable` in Note but
absent in MidiCv. So the inventory is **per type**, built from `writeTarget` + every
`writeRouted` + the `Routable<>`/plain field declarations. This per-type-divergence
is itself the argument for per-type tables.

### Storage kinds (the axis)

| Kind | Storage today | This spec |
|---|---|---|
| **S** Sequence-level `Routable<T>` | `{base,routed}` ×17 patterns | drop routed half + copy loop; base→plain; route via override (**the dedup win**) |
| **T** Track-level `Routable<T>` | `{base,routed}` ×1 | base→plain + override; uniform read path (not a RAM win) |
| **P** Project-global `Routable<T>` | `{base,routed}` ×1 (Project / CvRoute) | base→plain + override (scalar) |
| **B** Plain base field, no routed slot | base only | add override; stop the interim base-write; no field surgery |
| **X** PlayState transport | live `TrackState`, no base/routed | unchanged — action-like, writes live transport directly |
| **I** Inlet | engine per-track CV bus | parent overhaul inlet path (out of scope) |
| **G** Engine action / trigger | none | done — `RouteState.gateMask` |
| **R** Engine rail (Bus) | engine-owned CV | engine `setBusCv` (out of scope) |

In scope for the override table: **S, T, P, B**. Untouched: X, I, G, R.

### Per-type / global enumeration (every routed Target, exactly once per type)

**Global (type-independent):**
- **P** — Tempo, Swing, CvRouteScan, CvRouteRoute (`_tempo`,`_swing` Project; `_scan`,`_route` CvRoute)
- **X** — Mute, Fill, FillAmount, Pattern (PlayState `TrackState`)
- **G** — Play, PlayToggle, Record, RecordToggle, TapTempo, Reset; **Run** (level via `_runGate` `Routable`, treat as T-stored gate)
- **R** — BusCv1, BusCv2, BusCv3, BusCv4
- **T** (universal Track base) — CvOutputRotate, GateOutputRotate (`Track.h`)

**Per track type** (S = sequence Routable, T = track Routable, B = base-only, I = inlet):

| Type | S (sequence Routable) | T (track Routable) | B (base-only) | I (inlet) |
|---|---|---|---|---|
| Note | Scale,RootNote,Divisor,ClockMult,RunMode,FirstStep,LastStep | SlideTime,Octave,Transpose,Rotate,Gate/Retrig/Length/Note ProbBias | — | — |
| Curve | Divisor,ClockMult,RunMode,FirstStep,LastStep,WavefolderFold,WavefolderGain,DjFilter,ChaosAmount,ChaosRate,ChaosParam1,ChaosParam2 | SlideTime,Offset,Rotate,Shape/Gate ProbBias,CurveRate | — | — |
| MidiCv | — | SlideTime,Transpose | — | — |
| Tuesday | Algorithm,Flow,Ornament,Power,Glide,Trill,StepTrill,Octave,Transpose,Divisor,ClockMult,Rotate,GateLength,GateOffset | — | — | — |
| DiscreteMap | ClockMult,SlewTime | Octave,Transpose,SlideTime,Offset | RangeHigh,RangeLow (`float`) | Input,Scanner,Sync |
| Indexed | ClockMult,RunMode,RootNote,FirstStep | Octave,Transpose,SlideTime | — | A, B (+borrows DiscreteMapSync) |
| Stochastic | ClockMult,MaskRhythm,GateLength,Tilt,Burst,Feel,Rotate,Complexity,Contour,NoteDuration,Variation,Rest,Slide,Sleep,PatienceRhythm,Mutate,Jump | Octave,Transpose,SlideTime | **Scale,RootNote,Divisor** | — |
| PhaseFlux | Divisor,ClockMult (`Routable`, but `writeRouted` is empty → dormant) | Octave,Transpose,SlideTime | **Scale,RootNote** | — |

Notes that drove review finding 3:
- Stochastic's S set is large (17 Routable seq params); only Scale/RootNote/Divisor are B. A Routable-scan would have mis-bucketed all of these.
- PhaseFlux Divisor/ClockMult are `Routable` but `PhaseFluxSequence::writeRouted` is **empty** — PF routing is not dispatched today (forward-designed, dormant). The override migration wires it; until then PF routes are no-ops, not base-corrupting.
- Indexed borrowing `DiscreteMapSync` is the cross-type drift the parent plan removes (R13).

For each S/T/P/B entry the implementation records, before changing storage:
active-route read path, edit guard, serialization, and owner. Fits the parent
overhaul's clean-break window; reshapes what `Routable` *is* rather than adding a unit.
