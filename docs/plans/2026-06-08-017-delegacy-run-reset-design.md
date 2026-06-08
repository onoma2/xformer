# 017 — De-legacy Run + Reset (base-less shell triggers)

_Design. Branch `refactor/routing-matrix`. First slice of removing the legacy shaper
branch; CvOutputRotate/GateOutputRotate deferred to a later slice._

## Goal

Move the two **base-less shell targets** Run and Reset off the legacy apply branch
(`RoutingEngine.cpp:495-543` — `applyBiasDepthToSource` + bias + min/max window + the
9-shaper switch) onto a clean, shaper-free consumer. They aren't params with a base, and
they aren't continuous value sinks — they're a toggle and a trigger. The legacy machinery
around them is meaningless (a fold/integrator on a rising-edge does nothing useful).

Scope is **Run + Reset only.** CvOutputRotate/GateOutputRotate have a real user base
(−8..+8) and belong on the override path; they're a separate slice. The legacy branch and
the 7 stale shapers are NOT deleted here — they still serve the two rotates until that
slice. This change only relocates Run/Reset out of the branch.

## Current path (to remove for these two)

Per member track in the per-track loop, after the `migrated()` fork:
- `applyBiasDepthToSource(src, route, track)` = `clamp(0.5 + (src−0.5)·depth + bias)` — at
  the default depth 100% / bias 0 this is just `src`.
- the shaper switch (None by default → identity).
- `routed = route.min() + shaped·span` (default min0/max1 → `routed = src`).
- **Reset** (`:537`): `gateRisingEdge(gateMask, track, routed)` → `trackEngine(track).reset()`.
- **Run**: falls to `writeTarget(Run, 1<<track, routed)` → `setRunGate(routed > 0.55)`.

So at default settings both already reduce to the raw source. Only non-default depth/bias
diverges — and depth/bias on a trigger is the legacy noise we're shedding.

## New path

A clean per-track handler, run **before** the legacy block, consuming the **raw source**
value (no bias, depth, window, or shaper):

- **Reset** — `if (gateRisingEdge(routeState.gateMask, track, sourceValue)) trackEngine(track).reset();`
  Reuses the existing, tested rising-edge detector (`RouteParam::gateRisingEdge`, threshold
  0.5). One reset per low→high crossing; re-arms on the fall. Identical to the o_C clocked-
  input model the detector already documents.
- **Run** — `track.setRunGate(routeRunGate(sourceValue), true)`, where
  `routeRunGate(s) = s > 0.55f` (the legacy run threshold, now a named tested helper). The
  `routed=true` flag preserves the `isRouted(Run)` bookkeeping the getter dispatches on.

Both `continue` past the legacy block. The route-level `setRouted` bookkeeping (`:578`) and
the shaper-state reset on route change stay untouched (still needed by the rotates).

### Deliberate behavior change

Run/Reset stop honoring **depth** and **bias**. At the default depth 100% / bias 0 there is
no change. A route that set depth < 100% on a trigger previously attenuated the source toward
center (a quirk that could keep a trigger from ever crossing threshold); that quirk is gone —
triggers now read the raw source. This is the point of the de-legacy, not a regression.

## TDD

- `engine/RouteShellTrigger.h` — `routeRunGate(float source)`. Test the threshold boundary
  (≤0.55 off, >0.55 on) in `TestRouteShellTrigger`. Red → green.
- Reset reuses `gateRisingEdge` (already covered by `TestRouteParam`); no new edge test.
- Engine wiring (updateSinks) is the one seam without a unit harness — verified by
  sim + STM32 release build + the existing routing suite staying green.

## Constraints

- No ProjectVersion bump (no model-field or serialization change — Run stays a `Routable`
  shell field, just fed from the clean source instead of the shaped one).
- sim + STM32 release green; flash under the 983040 ceiling.

## Out of scope

CvOutputRotate/GateOutputRotate migration; deleting the legacy branch + the 7 stale shapers
(Crease/Location/Envelope/FrequencyFollower/Activity/ProgressiveDivider/VcaNext) — they
remain until the rotates move. scaleSource wiring.
