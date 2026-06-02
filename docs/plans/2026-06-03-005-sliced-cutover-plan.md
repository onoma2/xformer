---
id: routing-sliced-cutover
schema: plan
title: "plan: apply-fork slice — validate the override path on Note + PhaseFlux, no storage flip"
type: refactor
status: active
scope: now
date: 2026-06-03
revised: 2026-06-03 (v3 — apply-level fork after two adversarial-review rounds)
parent: routing-mod-matrix-overhaul
related:
  - routed-value-storage-modulation
  - align-vs-reset-routing
---

# Plan: apply-fork slice (Note + PhaseFlux, None/TriangleFold)

Execution strategy for the back half of the routing mod-matrix overhaul
(`2026-05-31-002`). Two review rounds killed the earlier framings: a storage flip
breaks global/transport/bus/trigger routing and the no-bump rule (round 1); a pure
shadow (old `writeTarget` runs in parallel) double-applies and corrupts base for the
base-write params (round 2). **This version is an apply-level fork: same route
storage, no format change, but the engine apply dispatches by target-type — migrated
types take the new override path, everything else stays on the live old path.**

## The model

- **One route storage, unchanged.** No new serialized fields, no `ProjectVersion`
  bump, no format break. The clean-break (R9) stays at the real U7.
- **Apply forks per `(trackMode, paramKey)`** — *not* target-only. `updateSinks()`
  already loops per route, then per masked track, before
  `writeTarget(target, 1<<trackIndex, …)`. The fork decision is made **inside that
  per-track loop**, on the resolved track's *mode*:
  - track is a **migrated type** (Note / PhaseFlux) and the target is one of its
    per-track params → **new path**: compute `RouteApply::delta`, write the override
    table; **skip** `writeTarget` for this `(route, track)`.
  - otherwise (non-migrated track mode, or global / project / playstate / bus /
    trigger target) → **old `writeTarget`**, untouched.
  Per-track-mode (not target-only) is required because shared targets like
  `Scale`/`RootNote` back many modes — a target-only fork would wrongly steal other
  modes' routes from `writeTarget`. It also makes a route whose mask spans migrated
  and non-migrated tracks correct automatically (each track decides independently).
- **Override table** — model-owned transient `(trackIndex, paramKey) → delta`, not
  serialized, **cleared at the top of `updateSinks()` and rebuilt every pass** (the
  single apply point; not gated on `routeChanged`). An entry is written for every
  targeted `(track, paramKey)` even when the delta is 0, so *presence = routed* —
  matching today's `isRouted()` active semantics (which preserves the track-mask
  dimension), with no stale locks after a route is deleted / retargeted / unmasked.
- **Migrated getters read `clamp(base + override(track, paramKey))`**, fully dropping
  `_x.get(isRouted(Target))`. No entry → delta 0 → reads base. Because the fork skips
  old `writeTarget` for these, **base is never mutated under routing** — so the
  PhaseFlux Scale/RootNote base-write defect is fixed *in this slice*, for free.

Because old `writeTarget` is skipped for migrated targets, there is no double-apply
and no `Routable.routed`-vs-`base` ambiguity: migrated params live entirely in the
override table + base.

## What slice 1 adds

1. **Override table** + a `routeOverride(trackIndex, paramKey)` accessor reached the
   same global way deep getters already call `Routing::isRouted(target, _trackIndex)`
   (confirmed: `NoteSequence`/`PhaseFluxSequence` getters do exactly this today).
   Cleared/rebuilt each recompute.
2. **Generic override-write apply pass** — name-agnostic; does **not** call the
   per-type `Row::applyRouted` setter hooks (those were parity-staging only). Inputs:
   - `range` = inferred from the row's min/max: `min < 0` → bipolar `(max−min)/2`,
     else unipolar `(max−min)`. Verified correct for every Note/PhaseFlux row
     (Transpose ±60→60, Octave ±10→10, SlideTime 0..100→100, Scale 0..23→23, Phase
     0..1→1, nudges ±64→64). Explicit range-class flag deferred to real-U6b.
   - `combine` / `d` / `scaleSource` = derived **inline** from existing route fields
     (`combine = Modulate`, `d` from the route's current depth, `scaleSource = None`).
     **No new `Route` members this slice** — so no SRAM hit, no `clear()`/`operator==`/
     UI edit-copy changes. Persisted fields land with the UI (step 6).
3. **Shaper stage (None + TriangleFold only)** feeding `RouteApply`, fed a **bias-free
   centered source** (the new model drops bias; `d` subsumes it), so TriangleFold's
   center-preservation holds — today it shifts center by `bias*0.5`, and that input
   simply does not exist in the new path.
4. **Getter migration — Note + PhaseFlux only.** Routed getters → `clamp(base +
   override)`; edit-gating (`editX` guards) switches from `isRouted(Target)` to
   "override entry present." Snapshot note below.

## What stays fully live and untouched

Old `writeTarget` + `Routable` + the on-disk `Route` format + `ProjectVersion`;
global / project / playstate / bus / trigger routing; the other six track types. The
fork only diverts the *apply* of Note/PhaseFlux per-track params.

## How v3 answers the two review rounds

- *Storage flip breaks global/transport/bus + no-bump rule* (r1) → no storage change
  at all; those paths stay on live `writeTarget`.
- *Tables write setters not overrides* (r1) → the new pass is generic delta-write;
  hooks unused live.
- *combine/d/range have no home* (r1) → range inferred from min/max; combine/d derived
  inline, no new members.
- *Getters must drop isRouted + edit-gate* (r1) → done; presence = active.
- *Pure shadow double-applies / corrupts base for base-write params* (r2) → **apply
  fork skips old `writeTarget` for migrated targets**; base never mutated.
- *Stale edit-locks* (r2) → override table cleared/rebuilt every recompute, entry per
  targeted param.
- *Non-serialized Route fields cost SRAM + clear/==/copy* (r2) → no new members; derive
  inline.
- *range-from-min/max unprovable from Row* (r2) → infer from `min<0` for the slice
  (verified for all migrated rows); explicit class at real-U6b.

## Snapshot slots

Tracks hold `CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT` sequences. The override is
keyed `(track, paramKey)` — pattern-independent by design (modulation is
pattern-independent). Only the **active** sequence's getters are read for output;
snapshot sequences are not played, so a per-track override has no audible effect on
them. The getter migration tests must assert this explicitly (active sequence
modulated, output unaffected by snapshot slots).

## Build order

1. **Override table** + `routeOverride()` accessor + clear/rebuild-per-recompute +
   `clamp(base+delta)` read helper. Tested standalone.
2. **Shaper stage** (None/TriangleFold, bias-free) feeding `RouteApply`. Tested.
3. **Apply fork** — the override-write pass for migrated-type targets, old
   `writeTarget` skipped for those; range inferred; combine/d derived inline. Tested
   against `RouteApply` expectations.
4. **Migrate Note + PhaseFlux getters** to override-read; edit-gate on override
   presence; assert snapshot + stale-clear + base-write-exclusion behavior.
5. **Hardware audition** — Note + PhaseFlux, Modulate, None/TriangleFold.
6. **UI** — persisted `combine`/`d`/`scaleSource` on `Route` (+ `clear()`/`==`/copy) +
   local matrix; validates the model UX, not just plumbing.
7. **Expand** — other six getters; defect-heavy tracks to audition; remaining shapers;
   group scope.
8. **Real U7** — serialized slot format + `ProjectVersion` clean-break.
9. **U9** — delete old `writeTarget`, VcaNext neighbor read, `Routable` routed half.

## Deferred

Serialized format + `ProjectVersion` (→ U7), persisted combine/d/scaleSource + UI (→
step 6), defect-heavy tracks, group scope (U6), Bus ordering (R7), Absolute-mode +
stateful shapers, the other six types, U9.

## Audition caveat (intentional, not a bug)

Defaulting `combine = Modulate` makes the audition a **forward-model** test:
modulation is neutral-at-center and bipolar around base. It is **not** a like-for-like
replay of today's routes, which write an absolute `min + shaper·span`. So a migrated
route will *sound different* from the same route today — that is the new model under
test, not a regression. (The inline derivation can select Absolute for a closer
comparison if a like-for-like check is wanted.)

## Riskiest step (named)

The **getter migration** (step 4). It must prove, by test: stale override clearing
on route delete/retarget/unmask; snapshot sequences unaffected on output;
PhaseFlux/Note base never mutated under routing; edit-gating UX matches today's
`isRouted` behavior exactly. Nothing audible from steps 1–3 means anything until this
is proven.
