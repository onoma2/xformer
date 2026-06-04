---
id: routing-ui-nav-spec
schema: spec
title: "spec: routing UI — lenses, screens, navigation (slice 6)"
type: spec
status: draft
scope: next
date: 2026-06-03
parent: routing-sliced-cutover
related:
  - routing-ui-tiers
  - routing-sliced-cutover
  - routed-base-editable
---

# Routing UI + navigation spec (slice 6)

_Status (2026-06-03): DRAFT, Codex-validated. Concrete screens + key bindings for the
routing UI overhaul. Builds on the tier/lens design (007). Screens rendered against the real
tiny5x5 font in `ui-preview/routing-design/`. No firmware yet — the spec to build slice 6
from. Codex pass fixed: `Page+Left` collision (lens switch → `F4 TARGET`), incomplete tab
bands (added GLOBAL/DMAP/PFLX/Phase, corrected STOC 130–144), and LEARN slot (F4, not F2)._

## Model recap (from 007 + the matrix work)

- A route is a relation `(source, target, track-set, depth/shape)`. Base-anchored:
  `out = clamp(base + delta)`.
- **Three tiers**, all falling out of the track-set, not a mode field:
  - **Global** — no track dimension (Tempo, Swing, CVR Scan/Route).
  - **Unified** — a track-set sharing one value (the per-track array held equal).
  - **Unique** — a track-set with per-track values (the array spread).
- **Source of truth = the per-type `ParamTable*.cpp`** (U4). Every engine's routable params
  are already staged on the shared `ParamKey` registry. The UI reads those tables; it does
  not reintroduce the flat `Target` enum.

_Model half landed (2026-06-03): Global base+delta (997f8b84) + combine/scaleSource
persisted on Route (d8a4a16b). Engine now honors route.combine() (96c08e17, Absolute/Modulate)._

_UI landed + hardware-verified (2026-06-04): overview (ee2f6514) → editable tab editor
(e601fa36→ac589d48, HW-verified) → RouteBrowse read model (eb659fba) → band aggregation
(8ed4ab1c/6e5a23c2) → per-row cursor (73926bd5/2c409112) → **route creation** on `+ADD`
(cd9bc39a/24045fa2) → **depth QuickEdit modal + LIVE editing** (bbf41f3d/ca092375). The tab
editor (Page+S6) is now a live param-centric editor: encoder navigates a band's params,
press opens the DEPTH modal (Generator's QuickEditPage ported — big readout + LED ring) or
creates a route on an empty row; F2 toggles combine; Left/Right cycle bands. Depth + combine
edit the committed route LIVE (audible immediately) — draft/commit removed. **Source selection
landed (2026-06-04): F3 SRC opens a scrollable source-list overlay (`RouteSourceSelectPage`,
mirrors `GeneratorSelectPage`), encoder press commits the picked source live; list = CV-domain
sources only (`RouteBrowse::sourceList`, MIDI + self-route bus excluded).** As-built footer is
`BACK · COMBINE · SRC` (the binding table below proposed F1 SRC / F5 EXIT; the built editor
keeps BACK on F1, combine on F2, and put SRC on the free F3). Next: F4 LEARN (MIDI source
in-editor — MIDI is deferred to it), Page+S5 spread (per-track unique depths), then promote
over the old RouteListModel rows._

### Global is base+delta (decided 2026-06-03)

The four Global params (Tempo, Swing, CVR Scan, CVR Route) **migrate to the base-anchored
model**, same as Note/PhaseFlux. They already store a `Routable` base on `Project`
(`_tempo`, `_swing`). Decision:

- `out = clamp(base + delta)`. **base = the value the user set** (e.g. 120 BPM) — a live
  center, no longer inert under a route (the plan-006 editable-base win, applied to Global).
- **`combine` picks behavior**: *Modulate* = source centered, wiggle ±delta around the set
  value; *Absolute* = sweep from the set value. The old absolute-replace use case survives as
  `combine: Absolute` — nothing lost.
- **Getter migrates inline.** `tempo()`/`swing()`/CVR getters return `clamp(base + delta)`
  reading a **global override slot** — no per-track array, no sentinel track index. Mirrors
  `routedValueInt` minus the track dimension.
- **Depth default runs small** for Global — large ranges (Tempo span ≈ 999) mean a low
  default `d` so the swing is musical out of the box.

This puts Global in the **first UI pass** alongside Note + PhaseFlux (the two engines already
on the override path). The six unmigrated engines stay out of the new UI until their pass —
a tab is visible only when the engine under it is base-anchored, so the screen never promises
base+delta while the engine does absolute-replace.

## Lenses — three doors into one editor

There is no single tree; intent roots differently. Same routes, multiple entry points.

| Lens | Root | For | Render |
|---|---|---|---|
| **Route overview** | the route | survey / audit all routes | `routing-list*.png` |
| **Scope lens** | tracks | "these tracks, this param" | `routing-scope*.png` |
| **Target lens** | a param | "this param across tracks" | `routing-target.png` |
| **Editor** | (lands from scope) | set source/depth/spread | `routing-unified*.png` |
| **Spread** | a routed param | per-track unique values | `routing-spread.png` |

### Route overview (home)

All 16 slots. Row: `slot · source › target · 8-track mask · depth` + scrollbar. `global`
label for no-track routes; `TRIG` for triggers. A **dangling** route (target's track gone)
shows dim with a mark — must stay visible (see 007 Open question; engine guard backstops it).

### Scope lens

8 cells, **one per track, engine tag inside** (`NOTE/CURV/ALGO/DMAP/PFLX/STOC/IDX`). Multi-
select the set; summary line shows `T3 T5 · NOTE ×2`. `BY TYPE` selects all tracks of the
cursor's engine. No tracks selected = the Global route.

### Editor (the shared param surface)

- **Vertical tab column** (left) = the registry tier bands, **derived from the scope's mode**.
  Shared tabs:
  - `PITCH` — pitch band **40–46** (Scale/RootNote/Transpose/Octave/SlideTime/Rotate/Offset)
  - `CLOCK` — **20–21** (Divisor, ClockMultiplier)
  - `LOOP` — **64–66** (RunMode, FirstStep, LastStep)
  - `PROB` — **80–84** (the biases)
  - `TRANSP` — universal (Run, Reset, Mute, Fill, Pattern, CV/Gate rotate)
  - `GLOBAL` — **1–4** (Tempo, Swing, CvRouteScan, CvRouteRoute) — the only tab when scope
    is Global; otherwise hidden.

  **Engine tabs** (appear only when the whole scope is that engine), each = the mode's own band:
  - `CURVE` — **90–97** (Wavefolder/Chaos/CurveRate/DjFilter) **+ Phase 60**
  - `IDX` — **100–101** (Index A/B)
  - `DMAP` — **102–103, 110–111** (Input/Scanner/RangeHigh/RangeLow)
  - `ALGO` — **120–128** (Algorithm/Flow/Ornament/Power/Glide/Trill/StepTrill/GateLength/GateOffset)
  - `STOC` — **130–144** (Complexity…Feel — the Stochastic signature block)
  - `PFLX` — **150–154** (Warp/Response/Len/CyclePhase/Pulse nudges) **+ Phase 60**

  `Phase` (key 60) is shared by Curve and PhaseFlux, so it lives in **both** those engine
  tabs — present for whichever of the two is the scope.
- **Param rows** (right): `name · source › depth · 8-cell track+engine mini-map`. Unrouted
  rows dim with `--`.
- **Mini-map three states** (per cell): **member** (filled block, knocked-out engine letter),
  **eligible-empty** (engine letter, this track owns the param but isn't a member),
  **ineligible** (dim dash — the track's engine does not own this param; can't host it).
  Engine-specific params show ineligible dashes on every non-matching track.

### Spread (unique)

`SPREAD` unfolds a routed row into one row per member track, each with its own depth + a
bipolar bar. Same source, per-track value. `UNIFY` collapses back (pick a value to flatten
to). A set of one (e.g. a single Indexed track) never offers spread.

### Target lens

Root = a param. Two columns of four tracks; each shows that param's route per track
(source · depth), `n/a` where the engine lacks the param, `--` where eligible but unrouted.

## Navigation — reuse, don't invent

All bindings are existing conventions lifted from current Routing + Generator / PhaseFlux /
Stochastic / Modulator. Lens switching uses an **F-key on the overview** (no new chord) —
`Page+Left/Right` is **not** available: `Key::Left` is the Overview page-key (PageKeyMap.h:14)
and TopPage consumes `Page + page-key` (TopPage.cpp:144), so `Page+Left` is already taken.

### Precedent map

| Gesture | Established meaning | Source page |
|---|---|---|
| Left / Right | cycle sections/tabs | Generator `_section`, PhaseFlux `_currentSet` |
| Track keys T1–T8 | toggle track membership | current Routing (Tracks row) |
| Encoder turn / push | move cursor / toggle edit | ListPage (all) |
| Encoder + Shift | coarse / jump instance | current Routing (route change) |
| Page + Step | enter sub-overlay | current Routing **Page+S5 = bias overlay** |
| F5 (fn 4) | Next / Exit | PhaseFlux, Stochastic |
| F1–F4 | contextual actions | all |

### Per-lens bindings

**Route overview (home)**
- Encoder turn = move selection · push = edit selected → Editor
- `F1 ADD` → Scope · `F2 DUP` · `F3 CLR` · `F4 TARGET` → Target lens · `F5 EXIT`

**Scope lens**
- **T1–T8** = toggle scope membership (LED feedback). *Broader than current Routing, where
  T-keys toggle only while editing the Tracks row — here the scope lens makes it always-on.*
- `F1 GLOBAL` · `F2 BY TYPE` · `F3 ALL` · `F5 NEXT` → Editor
- Encoder = move cursor cell

**Editor**
- **Left / Right** = cycle tabs
- Encoder turn = row cursor · push = toggle edit · turn = depth · **Shift = ×10 coarse**
- `F1 SRC` · `F2 TRACKS` (re-open Scope) · `F4 LEARN` (MIDI-learn, **same F4 slot as current
  Routing**) · `F5 EXIT` → Overview
- **Page + S5** = SPREAD → the per-track matrix (same key as today's bias overlay)

**Spread**
- Left / Right = move member track · encoder = that track's depth
- `F1 SHAPE` · `F3 UNIFY` · `F5 BACK` → Editor

**Target lens** (entered via `F4 TARGET` on the overview)
- `F1 PARAM` = pick param · encoder = move track rows · push = edit a track's route · `F5 EXIT`

### Zero-learning reuses (call-outs)

- **Page+S5 = the matrix** — fingers already do this for bias today; spread is the same
  gesture in the same place (spread literally replaces the bias overlay).
- **T1–T8 = scope** — track buttons already toggle route membership; scope-first just makes
  it step one.
- **Left/Right = tabs** — every multi-section page already trains this.

## What this replaces / keeps from current Routing

- **Replaces:** the single-route `RouteListModel` (one route at a time, flat name/value
  list, inert Min/Max/Bias rows).
- **Keeps:** MIDI-learn (`F4 LEARN`, same slot as today), track-key membership toggling,
  Page+S5 → matrix, encoder+Shift coarse, the `Routing::Route` storage. The Route already
  serializes `_target`, `_tracks`, per-track `_depthPct`/`_shaper`, **plus** the now-inert
  `_min`/`_max`/`_biasPct`, `_source`, CV/MIDI source, CV-rotate (Routing.cpp:87). The UI
  reads the live fields and leaves the inert ones untouched — **no serialized-format change**
  (slice 8 owns format; physical removal of Min/Max/Bias is deferred there).

## Open items (carried)

- **Dangling route** treatment (007): sit-dim vs surface-for-cleanup — pending the engine
  mode-change decision (neutralize vs write-time guard). The ineligible-dash state + a
  write-time eligibility guard together prevent misfire; UI treatment is then cosmetic.
- **`BY TYPE` semantics** — select all of cursor's engine vs open an engine submenu.
- **Mini-map letter on same-engine scopes** — when all tracks share an engine the letter is
  redundant (position encodes track); drop it for clarity, keep for mixed layouts.
- **UNIFY flatten** — which member's value the collapse adopts.
- **STEP tab split** — LOOP (First/Last/RunMode) vs CLOCK (Divisor/ClockMult) to keep every
  tab ≤4 rows (the render caught the 5-row overflow).

## Not in scope

- Engine mode-change neutralization (the misfire fix) — model/engine work, tracked in 007.
- Serialized `combine`/`scaleSource` persistence on `Route` — slice 6 model half / slice 8.
- Deleting the old `writeTarget`/`Routable` routed half — slice 9.
