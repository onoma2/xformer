---
id: routing-front-door-design
schema: spec
title: "spec: routing — the 'modulate this' front door + depth language + management view"
type: spec
status: draft
scope: next
date: 2026-06-04
parent: routing-sliced-cutover
supersedes: routing-tab-editor-complete-design (010, inside-out tab-editor framing)
supersedes-nav-for: routing-ui-nav-spec (009 lens-tree navigation)
related:
  - routing-legacy-vs-matrix   # provenance: what is legacy vs our work
  - routing-tab-editor-complete-design
  - routing-ui-nav-spec
  - routing-ui-tiers
  - routed-base-editable
---

# Routing front door — "modulate this"

> **SUPERSEDED by 013 (modulation-ui-spec).** The grillme session reframed this into the frozen
> contract: two doors (param-inline + matrix grid), draft→COMMIT editing, legacy hidden now.
> 013 is canonical; this doc is kept for the research/prior-art and the depth-language renders it
> introduced.


_Status (2026-06-04): DRAFT. The canonical routing-UI design, derived from the musician's
user story rather than the matrix internals. Supersedes 010's inside-out tab-editor framing
and 009's lens-tree navigation. Grounded in external prior art (Deluge/Bitwig destination-
first, Hermod+ depth, NerdSeq overview) — see References. Renders in
`ui-preview/routing-design/`._

## The user story (the only requirement that matters)

> "I'm playing music with this sequencer. I set up a param. I want to modulate it. How?"

Answer: **right where you are.** You're already on the param. One gesture — *modulate this* —
picks the source and sets the amount, then you keep playing. Scope is automatic: the track
you're on. Combine defaults to Modulate. Source and amount are the only choices.

The routing page is **not** the front door. It's the place you go to *review* and do advanced
work. The musician never opens it to make a sound move.

## Three layers

1. **(ui) Front door — modulate-this from the param.** Same gesture wherever you edit a param.
2. **(code) Engine spine — the `Param*.cpp` tables.** Each page answers "what param is under my
   hand?" from these. Wiring a table to the live override path = bringing that engine onto the
   front door. Note + PhaseFlux live today; the rest staged but inert.
3. **(ui) Management — the routing page.** Review every route, delete, and the advanced moves
   (scope spread, by-type, shaper). The chips/overview live here.

## Front door — "modulate this"

**Provenance (settled — see `docs/routing-legacy-vs-matrix.md`):** the "modulate this" hook is
**legacy** original-Performer code, not our routing-matrix work. We *reuse* it; we do not build
it. The work is to **redirect the legacy hook's destination** away from the legacy
`RouteListModel` editor into our lean flow. The legacy `Route` was min/max; our override path
reads depth+combine+base, so `RouteListModel`'s Min/Max/Bias rows are **inert** for migrated
targets — the lean flow drops them.

The legacy hook exists on list pages and just needs that redirect:

- **List pages (Note, Curve, config-list siblings) — LEGACY hook, reuse:** the `ROUTE` context
  action → `routingTarget(selectedRow())` → `TopPage::editRoute(target, thisTrack)` → find/create
  a route for *(this param, this track)*. **Legacy, since 2019.** Today it dumps into the legacy
  `RouteListModel`; the change is to land in the lean flow instead.
- **Graphical pages (PhaseFlux, Tuesday, DiscreteMap, Indexed) — NEW work, no legacy hook:**
  these have no `routingTarget()`. Each already tracks the param under the encoder (PhaseFlux
  `(_currentSet,_topicPage,_selectedSlot)`, Tuesday `paramForPage`, DiscreteMap/Indexed
  `_editMode`). Author a per-page **`currentRouteTarget()`** (the graphical analogue of
  `routingTarget(selectedRow())`) + a `ROUTE` action. _(DiscreteMap
  and Indexed already have a `ROUTE` action but it hardcodes `Divisor` — a latent half-feature
  this fixes.)_ Stochastic's performance surface has no per-param anchor; its routable params
  live on its config list, which already has `ROUTE`.

**The flow (everywhere):** trigger ROUTE → (find/create route for this param+track) →
**Source overlay** → **Depth view** → back to playing.

- **Source overlay** — `RouteSourceSelectPage`, scrollable CV-domain list
  (`RouteBrowse::sourceList`; MIDI deferred to F4 LEARN). **Built.** Render `routing-source.png`.
- **Depth** — the inline bar (below).

Destination-first is the proven small-hardware gesture (Synthstrom Deluge SHIFT+pad, Bitwig
right-click, Hydrasynth hold-source+press-dest). We adopt it.

## Depth language — inline bipolar bar (the settled vocabulary)

Depth reads as a **horizontal bipolar bar in each param row's dead space**, between the name and
the right-aligned value (the TeletypeScriptView `drawBipolarBar` idiom, rotated 90°). Render
`routing-rowbar.png`.

- Centre tick = base / neutral. Fill **right = +**, **left = −**, length = amount.
- Every routed row shows its bar — the whole band's depths read at a glance, no drill-in.
- **Cursor row gets a live source dot** riding the bar (Hermod+'s moving-dot feedback).
- Encoder-turn on the cursor row grows/shrinks the bar directly. The full-screen depth view
  (`routing-depth.png`) is the focused/fine editor, opened on press — optional, not required.

This maps 1:1 to **Hermod+'s attenuverter + polarity + offset**, which is already our model:

| Hermod+ | Ours |
|---|---|
| attenuverter amount | depth `d` |
| polarity (bi/unipolar) | combine — **MOD** (centred, wiggle ±d around base) / **ABS** (sweep from base) |
| offset (rest value) | base (the value you set) |

`routing-depth.png` = MOD (bipolar travel around base); `routing-depth-abs.png` = ABS (sweep).

## Scope — chips + by-type

The bottom strip stays the **letter chips** (one per track, engine letter, fill = member).
Render `routing-tabscope.png`.

- **T1–T8** toggle the focused route's track set, live. Pure readout strip; the hardware track
  buttons are the control (no cursor in the strip).
- **Shift+Tn** = by-type: toggle every track sharing track n's engine. The pressed button is the
  example — no engine cursor needed. Render `routing-tabscope-bytype.png`.
- No tracks = the **Global** route.
- **Tabs derive from scope:** empty → `GLOBAL` only; all-one-engine → shared tabs + that engine's
  tab; mixed → shared tabs only. By-type makes scope homogeneous, which unlocks the engine tab.

Depth = the inline row bar (unified, all members equal). Per-track different depths = **Spread**.

## Spread — vertical bipolar bars (`Page+S5`)

The unified inline bar covers the common case. For per-track different depths, `Page+S5` opens
**Spread**: the same bipolar idiom **vertical**, one bar per member track, full height — room to
read and set each precisely. `UNIFY` flattens back. Render `routing-spread.png` _(to be redrawn
from sliders to the vertical bipolar-bar form)_. Engine already reads per-track `depthPct[]`, so
Spread is UI-only. A single-track scope never offers it.

_(The vertical bipolar lane crammed into the editor was tried and rejected — no vertical room
beside the param rows; see `routing-tablane.png`. Vertical bars belong in the full-height Spread
screen; inline horizontal bars belong in the rows.)_

## Shaper — in Spread, engine-gated

Per-track `shaper[8]` lives in the Spread view (`F1 SHAPE`). **Gated on the shaper port** — the
migrated path honours only None/TriangleFold today; until the port lands, expose at most those
two, or omit.

## Management view — the routing overview

The routing page becomes the **review/manage** home, not a creation path:

- A list of all routes — `slot · source › target · track-chips · depth-bar`. Render
  `routing-list.png` (the overview seed).
- Delete, audit dangling routes, and the advanced moves (re-scope, by-type, spread, shaper).
- **Steal from NerdSeq:** prioritise a single-screen "what modulates what" overview — the chip
  strip per route already gives the track axis; depth bars give the amount. Consider a denser
  source×track matrix as a later power view.

What to avoid (criticised prior art): Peak/Summit single-slot-per-screen pickers (our old
`RouteListModel`), Metropolix cryptic-glyph + hidden-chord overload.

## MVP — Note "modulate this" = redirect the legacy hook

Not "build a front door" — the hook is legacy and works. The MVP **redirects its destination**:

- ROUTE context action on `NoteSequencePage` — **legacy, reuse**.
- `routingTarget(selectedRow())` + `editRoute` find/create for (target, this track) — **legacy, reuse**.
- Source overlay (`RouteSourceSelectPage`) — **ours, built**.
- Depth — QuickEdit modal **ours, built** (use as-is for slice 1; inline bar is slice 2).

**The change:** `editRoute` lands in the lean flow — source overlay → depth — *instead of*
`RoutingPage::showRoute()` opening the legacy `RouteListModel` (whose Min/Max/Bias rows are inert
under the override path anyway). Set `combine = Modulate` + leave source for the overlay on
create. That's the user story working on a live engine, list archetype, minimal new code.

## Build-status matrix

| Piece | State |
|---|---|
| ROUTE action + find/create (Note/Curve list) | built (**legacy** — reuse) |
| Source overlay (CV-domain) | built |
| Depth QuickEdit modal (number+ring) | built |
| Inline bipolar depth bar per row | unbuilt |
| Live source dot on cursor row | unbuilt |
| Full Hermod depth view | unbuilt (modal exists) |
| Scope chips display | built (read-only) |
| Chip toggle (T1–T8) + by-type (Shift+Tn) | unbuilt |
| Tabs derive from scope | unbuilt |
| Spread (vertical bipolar bars, Page+S5) | unbuilt (engine-ready) |
| Shaper (F1 SHAPE) | unbuilt + engine-gated |
| MIDI source (F4 LEARN) | unbuilt |
| `currentRouteTarget()` on graphical pages | unbuilt |
| Fix DiscreteMap/Indexed hardcoded-Divisor ROUTE | unbuilt (latent bug) |
| Management overview (review/delete) | partial (read-only list exists) |

## Implementation sequence

1. **MVP** — Note ROUTE → source → inline depth bar → done.
2. **Depth language** — inline bipolar bar per row + live dot; encoder edits it inline.
3. **Scope** — T1–T8 toggle live + Shift+Tn by-type; tabs derive from scope.
4. **Graphical front door** — `currentRouteTarget()` on PhaseFlux (live engine) first; fix the
   DiscreteMap/Indexed Divisor hardcode as those engines come on.
5. **Spread** — `Page+S5` vertical bipolar bars + UNIFY.
6. **F4 LEARN** — MIDI source.
7. **Shaper** — after the shaper port.
8. **Engine migration** — each remaining engine: `Param*.cpp` table live + `currentRouteTarget()`.
9. **Management overview** — promote the routing page to review/manage; later format + dead-code.

## References (external prior art)

- Destination-first gesture: Synthstrom Deluge (SHIFT+pad → source grid → encoder depth);
  Bitwig unified modulation (right-click destination); Ashun Hydrasynth (hold-source +
  press-destination). Roland SH-4D AUTO ASSIGN (wiggle both ends).
- Depth as attenuverter + polarity + offset with live moving dot: Squarp Hermod+ ModMatrix.
- "What modulates what" single-screen overview: XOR NerdSeq automation matrix.
- Avoid: Novation Peak/Summit single-slot list; Intellijel Metropolix glyph/chord density.
- Closest architectural peer (source→target, additive-on-base): Hermod+. Metropolix/Vector/
  NerdSeq's lane-as-drawn-signal model is filled in our system by the Modulators, not routes.

## Open items

- Cursor-row inline bar contrast (sub-blend on the bright highlight) — tune.
- Create-on-empty default scope: use the current chip mask rather than only the selected track,
  so scope set before ROUTE carries.
- LOOP / TRANSP shared-tab placement (009 carryover).
- `BY TYPE` exact semantics; `UNIFY` flatten-to value.
- Dangling route cosmetic treatment (007) — engine eligibility guard backstops it.
