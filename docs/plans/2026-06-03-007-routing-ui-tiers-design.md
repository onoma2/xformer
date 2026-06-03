---
id: routing-ui-tiers
schema: design
title: "design: routing UI — user tiers, topic picker, layout-aware filtering"
type: design
status: open
scope: next
date: 2026-06-03
parent: routing-sliced-cutover
related:
  - routing-sliced-cutover
  - routed-base-editable
  - routed-value-storage-modulation
---

# Design: routing UI tiers + layout-aware target picker

_Status (2026-06-03): OPEN design. No code. Supersedes slice 6's "list with
combine/d/scaleSource rows" sketch in plan 005. Codex-validated 2026-06-03 — the
lifecycle/no-op claim was wrong (routes **misfire** on mode change, see Lifecycle); the
matrix-gating and topic-partition claims were corrected. Mode-change handling is the one
unresolved decision (see Open question)._

## Why now

The apply-fork cutover (plans 005/006) made migrated Note/PhaseFlux params
**base-anchored**: `out = clamp(base + override-delta)`. That killed the model the
current routing UI was built for. `RouteListModel` still exposes **Min/Max** rows
(RouteListModel.h:12, `printMin`/`editMin` at Routing.h:705) and a **Bias** slot in
the per-track overlay (RoutingPage.cpp:396) — both dead under the base-anchored model:
the center now lives on the param's own page, depth owns the span, bias was a fudge for
the missing center. The list also walks the full `Target` enum (`editTarget` →
`adjustedEnum`, Routing.h:677), ~80 entries, most irrelevant to any given layout.

This doc reframes the routing UI from the **user's** mental model, not from the
engine's value-pipeline tiers.

## The user's model — two axes

A route is one sentence: *use **this source** to move **this thing** by **this much**.*
Source, destination, amount. Everything else is engine bookkeeping that should not reach
the screen.

Two axes the user actually holds:

### Axis 1 — what the source does

- **Trigger** — source crosses a threshold, fires an action. No "amount."
  (Run, Reset, Play, Record, TapTempo, Mute, Fill.)
- **Modulate** — source continuously moves a value. Amount = **depth + direction**;
  optionally a **shape** on the source curve. (Tempo, Transpose, Scale, …)

### Axis 2 — how the route fans across tracks

- **Global** — no track dimension. Hits the whole instrument. `isPerTrackTarget` false
  (Routing.h:410): Tempo, Swing, TapTempo, transport.
- **Unified per-track** — hits the selected tracks, **all swing the same** depth/shape.
  The common case. One number.
- **Unique per-track** — hits the selected tracks, **each gets its own** depth/shape.
  The advanced case. The matrix.

The two axes are orthogonal: a global route can be a trigger (Run) or a modulate
(Tempo); a per-track route can be a trigger (Mute) or a modulate (Transpose).

### Storage reality behind the three tiers

Unified and unique are **not** two storage shapes. There is one per-track array
(`_depthPct[track]`, `_shaper[track]`, Routing.h:805). "Unified" = every column equal;
"unique" = after the user spreads them. The engine always reads per-track. So
unified→unique is **one UI toggle that unlocks the columns**, not a model change.

Global is the genuinely separate shape: no track array participates (`tracks()` returns
0 for non-per-track targets, Routing.h:682).

## What dies

- **Min/Max** — only ever meaningful for global *scalar* targets, and even there the
  base-anchored direction makes endpoints awkward. Remove from the per-route surface.
  Depth + direction replaces it.
- **Bias** — dead everywhere. Base is the center.

These removals are display-layer; the stored fields can stay until a later cleanup
(no ProjectVersion concern either way).

## The UI — two surfaces, opposite filtering

### Surface A — the route list (existing routes)

Shows **every** existing route, **always**, regardless of current layout. A route is
serialized by `Target` enum value; it does not care whether a matching track exists.
Never filter this surface — a route whose target-mode is currently absent (*dangling*)
must stay visible so the user can see, retarget, or delete it. This matters more given the
mode-change misfire (see Lifecycle): a dangling route may be actively driving the wrong
param, so hiding it would hide a live problem.

### Surface B — the target picker (choosing / retargeting)

Filtered to the **present** layout. This is the "hero / topic page" pattern, modeled on
`StochasticPerformancePage` (a `ListPage` + curated `ListModel`, no exotic machinery).

- **The topic axis is track mode, full stop.** One group per mode: **Note, Tuesday,
  Stochastic, Curve (incl. Chaos/Wavefolder targets), DiscreteMap, Indexed, PhaseFlux**.
  Plus one modeless **Global/Transport** group.
- **A mode's target set is gathered by membership, not by enum range.** The `Target` enum's
  internal `Track`/`Sequence` range buckets (`isTrackTarget`, `isSequenceTarget`) are **not**
  topics and never appear in the picker — they mix universal transport (Run, Reset, the
  rotates) with Note params (Transpose, Scale, FirstStep…). The Note topic gathers its
  params from wherever they sit (SlideTime from the Track range, Scale from the Sequence
  range); the bucket boundary is invisible. Implementation builds a `mode → [Target]` map,
  not a predicate range check.
- **Global/Transport** (modeless, always shown): the universal transport/project targets —
  Run, Reset, Play, PlayToggle, Record, RecordToggle, TapTempo, Tempo, Swing, CvRouteScan,
  CvRouteRoute, and PlayState (Mute/Fill/Pattern). Run/Reset/rotate are universal across
  tracks, so they live here, **not** in any mode group.
- At display time, scan the project's tracks → build the set of **present modes** → render a
  mode's group only when that mode is present; Global/Transport always. No Curve track in the
  layout → no Curve group (its Chaos/Wavefolder targets vanish with it).
- **The topic groups *are* the filter unit.** "Topic subset" and "adjusts to layout" are
  one mechanism, not two.
- **Exception:** the route's *current* target is always shown even if its group is
  filtered out — otherwise retargeting a dangling route is unreachable.

#### Edit mechanism (not adjustedEnum)

`editTarget` today is a linear `adjustedEnum` walk over the full enum (Routing.h:676). A
layout-filtered picker **cannot reuse it** — a linear walk over the raw enum either steps
onto hidden targets or skips unpredictably. The picker needs a **curated target list**: the
page builds an ordered vector of visible targets (present mode groups + Global/Transport +
the route's current target if otherwise filtered), the encoder moves a **selected index**
into that vector, and commit writes the indexed `Target` via `setTarget`. The raw
`editTarget`/`adjustedEnum` path stays only for any unfiltered legacy entry point.

### Layout changes mid-project — routes misfire (corrected)

Layout is not fixed for a project's life. `setTrackMode` (Project.cpp:135) swaps the mode
and fires `TrackModeChanged` but **touches no routes**. Consequences:

- The picker filter must be **live**, re-read on `TrackModeChanged`, not a one-time
  `enter()` snapshot.
- A route whose track changes mode **does not** safely go dead. The engine fork asks
  `migrated(newMode, target)` → false, then falls to `writeTarget`
  (RoutingEngine.cpp:519/574). `writeTarget` has **generic `isTrackTarget` handling that
  fires regardless of the track's current mode** (Routing.cpp:216–221), plus `Reset` and the
  rotate targets handled in the engine before the mode switch (RoutingEngine.cpp:567). So a
  Note `SlideTime` route, after its track flips to Curve, can start driving a *Curve* param;
  `Reset` keeps firing against any selected track. **Routes bleed onto whatever the new mode
  exposes — active wrong output, not a harmless no-op.**

This misfire is the crux of the lifecycle problem and the reason the Open question is about
*neutralizing* routes on mode change, not merely dimming them in the UI.

## The per-track matrix — gating (corrected)

Two surfaces, gated differently today:

- **Inline list rows** (Bias/Depth/Shaper in `RouteListModel`) appear only for
  `isBusTarget` (RouteListModel.h:40/55).
- **The overlay editor** (`Page+S5`, `drawBiasOverlay`) is **already gated on
  `isPerTrackTarget`** (RoutingPage.cpp:219), *not* `isBusTarget`.

So base-anchored migrated targets — non-bus, per-track — **already have a per-track editor**
via the overlay; the engine fork's `route.depthPct(trackIndex)`/`route.shaper(trackIndex)`
reads (RoutingEngine.cpp:519) are reachable. The gap is **not** "no editor" — it is (a) no
inline rows, and (b) the new persisted **combine / scaleSource** the value pipeline will
consume have no slot yet. Slice-6 work: extend the per-track editor (the overlay, already
`isPerTrackTarget`-gated) to hold depth / direction / shape **+ combine + scaleSource**,
with unified as the default and a spread toggle to go unique. The inline rows' `isBusTarget`
gate is the only gating that actually needs widening.

## What this collapses to, user-facing

1. Pick a **topic that exists in your layout** (absent track types never appear).
2. Pick a **target** in it (or it's Global/Transport).
3. Set **depth + direction** — unified, all selected tracks together.
4. Optionally **spread** per track — unfold the one depth into the matrix.

Each step shows only what is reachable. The WILD flat list of old/new/global/per-track
becomes a layout-filtered drill-down.

## Open question — mode-change misfire

When a route's track changes mode, the route does **not** go dead — `writeTarget`'s generic
`isTrackTarget` handling drives the *new* mode's param with the old route's source (see
Lifecycle). So a "dangling" route is potentially **active wrong output**, not idle clutter.
That rules out the pure-UI stances; this needs an engine/model decision about what happens
to a route when its track's mode changes. Candidates:

- **Neutralize on mode change.** In `setTrackMode`, deactivate (or clear) any route whose
  target no longer matches the new mode. Stops the misfire at the source. Cost: destructive
  — flipping a track Note→Curve→Note loses the route; the "sticky, wakes up" behavior is
  gone. Could soften by deactivating (keep stored, `active=false`) rather than clearing, so
  it can be re-armed but never fires silently.
- **Engine guard at write time.** Keep routes untouched in storage, but have `writeTarget`/
  the fork refuse a target the current track mode does not own (a `targetValidForMode`
  check before any write). Non-destructive and sticky, kills the misfire, but adds a
  per-write validity check on the hot path and leaves "armed but inert" routes the UI must
  still surface honestly.
- **Retarget-or-drop prompt.** On mode change, flag affected routes and make the user
  resolve each (retarget to a valid target or delete). No silent misfire, no silent loss,
  but interrupts the user mid-rearrange.

The UI dangling-route treatment (dim / mark / hide-from-picker) is **downstream** of this —
once the engine can't misfire, the UI choice is cosmetic. Pick the engine behavior first.
Leaning **engine guard at write time** (non-destructive, matches the sticky intent, the
hot-path check is one comparison) but the owner decides.

## Not in scope here

- The OLED pixel layout of either surface — render with `ui-preview/` before building.
- Whether Min/Max/Bias stored fields are physically removed vs left inert.
- combine / scaleSource persistence shape on `Route` (slice 6 model work).
