# Routing: legacy vs routing-matrix — provenance reference

_Reference doc (2026-06-04). Settles what is **legacy original-Performer routing** vs **our
routing-matrix work**, so the distinction is not re-derived every conversation. Verified
against `temp-ref/original-performer` and `temp-ref/phazer-dev-performer`._

## Why this exists

The "modulate this" hook (ROUTE context action → `routingTarget` → `editRoute` → `showRoute`)
is **legacy** and is easily mistaken for our work because we *reuse* it. Our new pieces (source
overlay, depth modal, tab editor, override path) are a different vintage. Conflating the two
produced repeated wrong framings ("build a front door" when the hook already exists; treating
legacy `RouteListModel` controls as live when our engine ignores them).

## Legacy — original Westlicht Performer (`temp-ref/original-performer`)

Present since 2019, unchanged on master. The **entire existing routing UX is legacy**:

- **Model** — `Routing::Route` = flat `Target` enum + `Source` enum + **`min`/`max`** + `tracks`.
  No depth, no combine, no override. (Verified: original `Routing.{h,cpp}` has **0** occurrences
  of `depthPct`/`combine`/`scaleSource`/`writeRouteOverride` — it is min/max only.)
- **Engine** — `RoutingEngine` writes target values directly (`writeTarget`).
- **Editor UI** — `RoutingPage` (per-route editor) + `RouteListModel` (Target / Source / Min /
  Max / Tracks rows).
- **The "modulate this" hook (all legacy):**
  - `RoutableListModel` + `routingTarget(row)` on `NoteSequenceListModel`,
    `CurveSequenceListModel`, `NoteTrackListModel`, `CurveTrackListModel`, `MidiCvTrackListModel`,
    `ProjectListModel`.
  - The **ROUTE context action** on `NoteSequencePage` / `CurveSequencePage`.
  - `TopPage::editRoute(target, trackIndex)` → `RoutingPage::showRoute(routeIndex, &initRoute)`.

The phazer-dev base (`temp-ref/phazer-dev-performer`) is the same here: also min/max, also no
depth/combine/registry. So per-track depth/shaper/override are **not** fork-inherited.

## Ours — this project's work

### New track engines (built in this repo)
PhaseFlux, Stochastic, Tuesday, DiscreteMap, Indexed. Absent from original and phazer-dev.

### routing-matrix refactor (`refactor/routing-matrix`)
- **Name-agnostic addressing:** `RouteParamKey.h` (append-only registry), `RouteParam.h`, nine
  per-type `ParamTable*.{h,cpp}` (Global/Note/Curve/Indexed/DiscreteMap/MidiCv/Tuesday/Stochastic/
  PhaseFlux).
- **Value pipeline:** `RouteApply.h`, `RouteShaper.h`, `RouteFork.h`, `RouteBrowse.h`.
- **Base+delta override path:** `Routing::writeRouteOverride`/`routeOverride`/`routedValue`
  (`out = clamp(base + delta)`); `combine` (Modulate/Absolute) + `scaleSource` + per-track
  `depthPct[]`/`shaper[]` on `Route`. (Verified: our `Routing.{h,cpp}` has these; original has none.)
- **UI (ours):** the Page+S6 **tab editor** inside our `RoutingPage.cpp`; `RouteSourceSelectPage`
  + `RouteDepthQuickEditModel`; the design docs (009/010/011).
- **Live status:** only **Note + PhaseFlux** run on the override path. The other six engines'
  param tables are staged but inert.

## The boundary that bites (read before planning UI)

1. **The "modulate this" hook is legacy, and it lands in the legacy `RouteListModel` editor.**
   Our routing-matrix work added the override path + tab editor but **never rewired the hook.**
   So today "modulate this" still opens the legacy per-route editor. Any front-door plan is a
   **redirect** of the legacy hook's destination — not building a hook.

2. **Legacy `Route` was min/max; our engine reads depth+combine+base instead.** So
   `RouteListModel`'s **Min/Max/Bias rows are inert** for migrated targets — legacy controls our
   engine no longer reads. The lean flow sidesteps them by not showing them.

3. **PhaseFlux is a new engine with no legacy hook.** It has no `routingTarget()` and no ROUTE
   action. Its front door is genuinely **new work** (author a `(_currentSet,_topicPage,
   _selectedSlot)` → `ParamKey`/`Target` mapping), not a reuse.

## Quick reference

| Component | Owner | Where |
|---|---|---|
| `Target`/`Source` enums, `Route` min/max+tracks | legacy | `model/Routing.h` (original fields) |
| `RoutingEngine` `writeTarget` | legacy | `engine/RoutingEngine.cpp` |
| `RouteListModel` per-route editor | legacy | `ui/model/RouteListModel.h` |
| `RoutableListModel` + `routingTarget(row)` | legacy | `ui/model/*ListModel.h` |
| ROUTE context action, `editRoute`/`showRoute` | legacy | `NoteSequencePage.cpp`, `TopPage.cpp` |
| ParamKey registry + `ParamTable*` | ours | `model/RouteParamKey.h`, `model/ParamTable*.{h,cpp}` |
| Override path, `combine`, `depthPct[]`, `shaper[]` | ours | `model/Routing.{h,cpp}` (new fields) |
| `RouteApply/RouteShaper/RouteFork/RouteBrowse` | ours | `model/Route*.h` |
| Page+S6 tab editor | ours | `ui/pages/RoutingPage.cpp` |
| Source overlay, depth modal | ours | `ui/pages/RouteSourceSelectPage.*`, `RoutingPage.cpp` |

## Re-verify (if ever in doubt)
`grep -c "depthPct\|combine\|writeRouteOverride" temp-ref/original-performer/.../model/Routing.h`
→ **0** = legacy. Same grep on `src/.../model/Routing.h` → non-zero = our additions.
