---
id: routing-tab-editor-complete-design
schema: spec
title: "spec: routing tab editor — the complete editor (one screen, every control)"
type: spec
status: draft
scope: next
date: 2026-06-04
parent: routing-sliced-cutover
supersedes-nav-for: routing-ui-nav-spec (009, lens-tree navigation for migrated/global targets)
related:
  - routing-ui-nav-spec
  - routing-ui-tiers
  - routed-base-editable
---

# Routing tab editor — the complete editor

> **SUPERSEDED by 011 (routing-front-door-design).** This doc framed the editor matrix-first
> (tabs/scope/registry). 011 reframes from the musician's "modulate this" user story: the front
> door is destination-first from the param, depth is an inline bipolar bar, and the tab
> editor/chips/spread become the management layer. Kept for the build-status + scope/by-type
> mechanics it still describes accurately.

_Status (2026-06-04): DRAFT. One coherent design for the whole tab editor, replacing the
piecemeal unit-by-unit drift. Every control (scope, param, source, depth, spread, shaper,
combine) placed in one screen-flow, every gesture assigned, with an explicit build-status
matrix so the implementation sequence falls out of the whole, not the reverse._

## Premise

The **param-centric tab editor** (entered `Page+S6`, `RoutingPage::drawTabEditor`) is the
canonical routing editor for **migrated + global targets** (Note, PhaseFlux, Global today;
more engines as they migrate). It edits the committed route **live** — no draft/commit.

This **supersedes the 009 lens-tree navigation** (overview → Scope lens → Editor → Spread as
separate screens) for these targets. The scope lens, target lens, and standalone screens in
009 are not built and not planned; their *intent* (scope-first selection, by-type, per-track
spread) is folded into this single editor. 009's renders (`routing-scope*`, `routing-target`,
`routing-unified*`) are historical design context, not build targets.

The old `RouteListModel` single-route editor stays only for: MIDI source config (until F4
LEARN lands here) and the six unmigrated engines (until their tabs land).

## The one screen

```
┌────────────────────────────────────────────────────────────┐
│ A 120.0   ROUTE                                  TRANSPOSE   │  header: clock · title · active param
├──────┬─────────────────────────────────────────────────────┤
│ PITCH│ SCALE                                            --   │  tab column (left) │ param rows (right)
│ CLOCK│ ROOT NOTE                                        --   │
│ PROB │ TRANSPOSE                                        +5   │  ← cursor row (encoder), routed shows depth
│ GLOB │ OCTAVE                                            --   │     unrouted shows -- / +ADD on cursor
│      │ MOD                       N [N] N [N] N  N  N [N]      │  combine label · 8-track chip strip
├──────┴─────────────────────────────────────────────────────┤
│  BACK  │ COMBINE │  SRC  │        │        │                 │  footer
└────────────────────────────────────────────────────────────┘
```

Renders: `routing-tabscope.png` (explicit scope), `routing-tabscope-bytype.png` (by-type),
`routing-source.png` (source overlay), `routing-spread.png` (per-track spread).

Four regions:
- **Tab column** (left, `x2 w40`) — the param bands, **derived from the current scope**.
- **Param rows** (right) — the focused band's params; routed rows show depth, unrouted show
  `--` / `+ADD` on the cursor.
- **Chip strip** (bottom-right, `MAP_X=167`, 8×11px) — the focused route's track set. Pure
  readout; the hardware track buttons are the control.
- **Combine label** (bottom-left) — `MOD`/`ABS`, or `global` when the route has no tracks.

## Controls — every input, one table

| Input | Action |
|---|---|
| Encoder turn | move param-row cursor |
| Encoder turn + Shift | (reserved) coarse / jump |
| Encoder press on routed row | open **Depth** modal (unified depth, live) |
| Encoder press on `+ADD` row | create route here (target from band key, current scope, CV1, depth 0) |
| **Left / Right** | cycle tabs (bands) |
| **T1–T8** | toggle that track in/out of the focused route's set (live; chip lights) |
| **Shift + Tn** | by-type: toggle every track sharing track n's engine (live) |
| **Page + S5** | open **Spread** (per-track depth + shaper) for the focused route |
| **F1 BACK** | exit editor → overview |
| **F2 COMBINE** | toggle Modulate ↔ Absolute (live) |
| **F3 SRC** | open **Source** overlay (CV-domain pick, live) |
| F4 LEARN | _(planned)_ MIDI-learn → source=Midi + midiSource on the live route |
| F5 | _(free)_ |

The chip strip **never takes focus** — there is no cursor in it. Track buttons toggle it the
same way they select tracks everywhere else on the device; the strip just shows the result.

## Scope (the chip strip)

- Each chip = one track, drawn at its fixed position (chip 1 = T1 … chip 8 = T8), labelled
  with the track's **engine letter** (`N C M A D I T S P`, `trackModeLetter`). Position gives
  the number, the letter gives the engine — no memorising the layout.
- **Filled** = member of the focused route's track mask. **Dim letter** = not a member.
- **T1–T8** toggle membership live. **Shift+Tn** = by-type (select all of track n's engine):
  the pressed button is the example, so the system needs no engine cursor.
- No tracks = the **Global** route (combine label shows `global`). The strip is empty of fills.

### Tabs derive from scope

The tab column is **computed from the current scope's engine make-up** (009's "derived from
the scope's mode", applied to the live chip set):

| Scope | Tabs shown |
|---|---|
| no tracks (Global) | `GLOBAL` only |
| all one engine | shared tabs **+ that engine's tab** |
| mixed engines | shared tabs only (params every selected engine owns) |

- **Shared tabs:** `PITCH` (40–46), `CLOCK` (20–21), `PROB` (80–84). _(LOOP 64–66 and TRANSP
  split deferred; see Open items.)_
- **Engine tabs** (homogeneous scope only): `PFLX` (150–154 +Phase 60) today; `CURVE`, `IDX`,
  `DMAP`, `ALGO`, `STOC` as those engines migrate.
- `GLOBAL` (1–4) only when scope is empty.

This is why by-type matters: `Shift+Tn` on a Note track makes the scope homogeneous, which
unlocks Note's bands for the whole set in one gesture.

## Param rows

`RouteBrowse::bandParams` lists the focused band's `ParamKey`s; each row resolves to its
routed state at the current scope (`routeForBandParam`). Routed → name bright + signed depth
right-aligned; unrouted → name dim + `--`, or `+ADD` on the cursor. Encoder press acts on the
cursor row (open depth, or create).

## Source — F3 overlay (built)

`RouteSourceSelectPage`: scrollable CV-domain source list (`RouteBrowse::sourceList` — every
`Source` in enum order minus MIDI and the self-route bus). Encoder press / OK commits live.
MIDI is **deferred to F4 LEARN** (a bare MIDI pick would leave `midiSource` unconfigured).

## Depth — unified vs spread

Two tiers off one per-track `depthPct[8]` array (Unified = all columns equal, Unique = spread):

- **Unified (Depth modal):** encoder press on a routed row → the QuickEdit modal (big readout
  + LED ring). Writes **all member tracks to one value** (`RouteDepthQuickEditModel::setAll`).
  This is the default tier — one source, all scoped tracks swing the same.
- **Unique (Spread, `Page+S5`):** unfolds the focused route into one row per member track,
  each its own depth + bipolar bar, same source (`routing-spread.png`). Encoder moves
  track-to-track and sets each depth. **`F3 UNIFY`** flattens back to one value. A single-track
  scope never offers spread.

The engine already reads per-track `depthPct[trackIndex]`, so spread is **UI-only**.

## Shaper — F1 SHAPE in Spread

Per-track `shaper[8]` lives beside per-track depth in the Spread view (**`F1 SHAPE`**).

**Engine caveat (gates the UI):** the migrated Note/PhaseFlux path currently honours only
`None` and `TriangleFold`; the other shapers act as identity. So a full shaper picker would
promise more than the engine delivers. **Shaper UI waits on the shaper port** — until then the
Spread view exposes depth only, or a two-entry None/TriangleFold toggle at most.

## Combine — F2

`F2` toggles the focused route's combine: **MOD** (Modulate — centred source, wiggle around
base) ↔ **ABS** (Absolute — sweep from base). Shown in the bottom-left label. Live.

## Full walkthroughs

**Transpose on T2/T4/T7, each a different amount:**
1. `Page+S6` → `PITCH` tab. 2. Encoder to TRANSPOSE, press → route created.
3. Tap `T2 T4 T7` → three chips light (toggle the default track off if present).
4. Press encoder → set a depth (all three equal, Unified).
5. `Page+S5` → Spread → give T2/T4/T7 their own depths. `F5 BACK`. Done.

**Clock Divisor on every Note track, don't care which numbers:**
1. `Page+S6` → `CLOCK` tab. 2. Encoder to DIVISOR, press → route created.
3. `Shift + any Note track button` → every `N` chip lights at once; read the strip to confirm.
4. Press encoder → set depth. Done. (Scope is now homogeneous Note — Note's engine tab is
   available if you need a Note-specific param next.)

## Build-status matrix

| Piece | State | Work |
|---|---|---|
| Tab shell, param rows, live edit | **built** | — |
| Create route on `+ADD` | **built** | — |
| Depth modal (unified) | **built** | — |
| Combine F2 | **built** | — |
| Source overlay F3 (CV-domain) | **built** | — |
| Chip strip (display) | **built** (read-only) | — |
| Chip strip toggling (T1–T8) | unbuilt | wire T-keys → focused route mask, live |
| By-type (Shift+Tn) | unbuilt | engine-match expansion off the pressed track |
| Tabs derived from scope | unbuilt | replace static 4 tabs; homogeneity → engine tab |
| Engine tabs (PFLX now, others later) | unbuilt | add engine bands to `RouteBrowse` |
| Spread (Page+S5, per-track depth) | unbuilt | per-track editor; engine-ready; `routing-spread` done |
| UNIFY | unbuilt | flatten per-track → one value |
| Shaper (F1 SHAPE) | unbuilt + **engine-gated** | wait on shaper port (only None/TriangleFold live) |
| MIDI source (F4 LEARN) | unbuilt | MidiLearn → source=Midi + midiSource on live route |

## Implementation sequence (falls out of the whole)

1. **Scope on the strip** — T1–T8 toggle the focused route's mask, live (chips already render
   it). Then **Shift+Tn by-type**. _(Smallest, unblocks both core scenarios.)_
2. **Tabs derive from scope** — homogeneity rule; add the PFLX engine tab.
3. **Spread (`Page+S5`)** — per-track depth + UNIFY. Engine-ready, render done.
4. **F4 LEARN** — MIDI source in-editor.
5. **Shaper** — only after the shaper port; gate the UI to live options.
6. **Engine migration** — each remaining engine: param path live + its tab, paired.
7. Promote over `RouteListModel`; later serialized-format + dead-code removal.

## Open items

- LOOP (First/Last/RunMode) and TRANSP (Run/Reset/Mute/Fill/Pattern/CV-rotate) bands not yet
  placed in this editor — shared-tab set above is PITCH/CLOCK/PROB only. Decide their tabs.
- Create-on-empty-row default scope: currently the single selected track. With live T-keys,
  consider creating with the **current chip mask** instead, so scope set before `+ADD` carries.
- Spread entry affordance — `Page+S5` reuses the bias-overlay muscle memory (009), but the
  editor gives no on-screen hint; consider a footer marker on routed multi-track rows.
- Dangling route (target's track changed mode) — engine write-time eligibility guard + the
  chip's ineligible state; cosmetic treatment still open (007).
