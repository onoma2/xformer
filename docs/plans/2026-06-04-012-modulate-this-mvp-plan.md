---
id: modulate-this-mvp-plan
schema: plan
title: "plan: MVP — redirect the legacy ROUTE hook into the lean modulate flow"
type: plan
status: active
scope: now
date: 2026-06-04
parent: routing-front-door-design
related:
  - routing-front-door-design
  - routing-legacy-vs-matrix
---

# MVP — "modulate this" = redirect the legacy ROUTE hook

First implementation off the front-door design (011). **Not building a front door** — the
legacy hook exists (`docs/routing-legacy-vs-matrix.md`). This redirects its destination for
migrated targets from the legacy `RouteListModel` editor into our lean flow:
**source overlay → depth modal → back to the page you were on.**

## Scope (only this)

- Migrated targets (`RouteFork::migrated` Note/PhaseFlux per-track, `migratedGlobal` Tempo/
  Swing/CVR) → lean flow.
- Everything else (Curve + the 6 unmigrated engines) → **unchanged**, keeps legacy `showRoute`.
- Depth uses the **existing** `RouteDepthQuickEditModel` (the QuickEdit modal). The inline
  bipolar bar is a *later* slice, not this one.
- No inline-bar render, no scope chips, no new model fields. Reuse what exists.

## The change

### 1. `TopPage::editRoute(target, trackIndex)` — fork on migration
Today: find/create route → `setMode(Routing)` → `routing.showRoute(...)` (legacy editor).

New: after find/create, decide:
- **migrated?** `RouteFork::migrated(project.track(trackIndex).trackMode(), target, key, range)`
  OR `RouteFork::migratedGlobal(target, key, range)`.
- **migrated → lean flow:** do **not** `setMode(Routing)`. On create, set `combine = Modulate`
  (source stays None — the overlay sets it). Call `routing.beginModulate(routeIndex, wasCreated)`.
  Stay on the current page; the flow is modal overlays.
- **not migrated → unchanged:** `setMode(Routing)` + `showRoute(...)` exactly as today.

`wasCreated` = whether this call allocated the route (vs found an existing one).

### 2. `RoutingPage::beginModulate(int routeIndex, bool wasCreated)` — the lean flow
Lives in `RoutingPage` because the depth model (`gRouteDepthQuickEditModel`) is file-static
there. RoutingPage need not be the visible page — it only pushes modal overlays over the
current top (e.g. `NoteSequencePage`).

1. Show `routeSourceSelect.show(route.target(), route.source(), callback)`.
2. Callback `(ok, source)`:
   - **ok:** `route.setSource(source)`; then open the depth modal —
     `gRouteDepthQuickEditModel.configure(&route); quickEdit.show(gRouteDepthQuickEditModel, 0)`.
   - **cancel + `wasCreated` + source still None:** `route.clear()` — don't leave an inert
     skeleton from an abandoned create. (Found/existing routes are left untouched on cancel.)

Capture `routeIndex` (not a Route&) in the callback and re-fetch from `project.routing()` —
the route slot is stable in the fixed array, but re-fetch keeps it unambiguous.

## Why no TDD step here
The only branch logic — "is this target migrated?" — is `RouteFork::migrated`, already unit-
covered (`TestRouteFork`). The rest is page/modal orchestration (no clean unit seam). Verify by
sim build + the user's hardware audition. If a pure helper falls out (e.g. an
`editRoute`-side `shouldModulateInline(...)`), unit-test that.

## Verify
- `build/sim/release` green, zero warnings.
- Manual: on a Note track sequence page, ROUTE on Transpose → source overlay → pick CV In 1 →
  depth modal → set depth → back on the sequence page, route live. ROUTE on a **Curve** track
  param → still the legacy editor (unchanged).
- Codex-gate the change.

## Out of scope (later slices)
Inline bipolar depth bar; scope chips + by-type; PhaseFlux `currentRouteTarget()`; spread;
F4 LEARN; fixing DiscreteMap/Indexed hardcoded-Divisor ROUTE; demoting the routing page.
