# 018 — Gate output rotation, reimagined (group rotation)

_Design. Branch `refactor/routing-matrix`. Gate-only; CvOutputRotate stays on the legacy
branch until a follow-up. Settled via `/grillme` with the owner._

## Why

The current GateOutputRotate is incoherent. A track joins the rotation "pool" iff
`isGateOutputRotated()` (`base != 0 || isRouted`) — and the base is never settable (no UI),
so membership = "is it routed." Each output then shifts by **its own track's** rotation
value (`Engine.cpp:643`), so a multi-track set never rotates as a unit; it only looks like a
rotation if every routed track happens to carry the same value. Membership, amount, and
ordering are all conflated into one per-track field. Tracks don't even own jacks — Layout
does — so a per-track rotation value is the wrong home.

## Model

**This is not a new mechanism — it's legacy's coherent case made the only case.** Legacy
already produces a real cyclic rotation when every routed track carries the same value (one
route, uniform depth): `(poolIndex − rotation) mod poolSize` is then a uniform shift. The
redesign keeps that exact cycle math and the existing ±8 amount range; it only stops each jack
reading its **own** track's value (the source of the incoherence) and reads **one** group
amount instead.

- **Group = the tracks routed for GateOutputRotate** — i.e. legacy's pool, unchanged
  (`isRouted`), which for the single route is its track mask. Sorted by track index →
  `[t0 … t_{K-1}]`, K = member count.
- Each member's jack is its Layout-assigned gate output. The cycle is legacy's, one group
  amount `N`: **jack of `t_p` sources member `(p − N) mod K`** (same sign as legacy
  `(poolIndex − rotation) mod poolSize`). N=1 on a 2-track group swaps the two gates.
  Non-members untouched.
- **Single group** (matches legacy's single pool): one GateOutputRotate route drives it; if
  more than one exists, the lowest-index active route wins.
- `K = 1` → no rotation. `K = 0` → inactive.

### Amount

Modulation-only, base 0. `N` is the route's source mapped over the **existing ±8 range**
(`targetInfos`, unchanged from legacy), rounded to an int; the wrap to the group happens at
**apply** time (`mod K`), exactly as legacy wrapped `mod poolSize`:

```
N = round( computeDelta(source, shaper(0), depthPct(0), ±8, combine) )   // legacy ±8 range
// engine applies (p − N) mod K  (negative-safe), as legacy did
```

The only departure from legacy here: `N` is computed **once for the group** from the route
(`depthPct(0)`, one amount) on the clean unified path — instead of `denormalizeTargetValue`
per track inside the legacy shaper branch. Combine/shaper ride the same path as every other
target (Modulate centered / Absolute sweep; None/TriangleFold).

## Wiring

- **updateSinks** (RoutingEngine): on the GateOutputRotate route, compute `N` and publish
  `(_gateRotateMask, _gateRotateAmount)` to the engine. Replaces the per-track legacy write;
  GateOutputRotate leaves the legacy shaper branch. Remove its `writeTarget` case (`Routing.cpp:234`).
- **Engine gate-output loop** (`Engine.cpp:600-650`): the pool build + `(p − rotation) mod K`
  cycle stay structurally as-is; the only change is the rotation value — read the single
  `(mask, N)` instead of each jack's own track `gateOutputRotate()`. Membership = the mask
  (= legacy's routed pool). Jack at group-position `p` sources member `(p − N) mod K`.
- **Track** `_gateOutputRotate` Routable → **vestigial**: base stays 0, the serialized byte is
  kept (no ProjectVersion bump), `gateOutputRotate()`/`isGateOutputRotated()` stay as model API
  but the engine no longer reads them. `setGateOutputRotate` routed-write path retired.

## TDD

Pure helpers (engine-consumed logic is otherwise un-unit-testable):

- `gateRotateAmount(float source, int depthPct, Combine combine) -> int` — the source→N
  mapping over ±8 (round). Cases: Modulate center = 0, Absolute full-swing = +8, sign.
- `rotatedGroupMember(int p, int N, int groupSize) -> int` = `(p − N) mod K`, negative-safe.
  Cases: identity at N=0, swap at K=2/N=1, wrap (N≡N+K), matches legacy's `(p − rotation) mod K`.
  Cases: identity at N=0, swap at K=2/N=1, full-cycle wrap.

Engine/updateSinks wiring verified by sim + STM32 release build + routing suite green.

## Constraints

- No ProjectVersion bump (vestigial field kept; no serialization change).
- sim + STM32 release green, under the 983040 ceiling.

## Out of scope

CvOutputRotate (same idea + a float-interp path — separate slice). Manual base rotation
(modulation-only chosen). Multiple simultaneous groups (single group chosen). Deleting the
legacy branch + 7 stale shapers — waits until CvOutputRotate also moves.
