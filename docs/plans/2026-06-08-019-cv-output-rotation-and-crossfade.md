# 019 — CV output rotation (discrete) + CvOutputCrossfade

_Design. Branch `refactor/routing-matrix`. Two slices. Builds on the gate group-rotation
model (spec 018). Settled with the owner._

## Two targets, one group model

CV rotation splits into two distinct operations on the **same group** (the route's track
mask, members sorted by track index → their Layout CV jacks):

- **CvOutputRotate** — *discrete*. One integer amount hard-switches which member's CV each
  member jack emits, `(p − N) mod K`. Exact mirror of the gate target. Use case: move a
  melody/CV from one track to another.
- **CvOutputCrossfade** — *continuous* (new appended Target). One float position; each member
  jack outputs the crossfade of the two adjacent members' CVs by the fractional part. Use
  case: crossfade several modulations — sweep and the group's CV sources morph through each
  other. Gates have no crossfade analogue (binary), so this is CV-only.

The legacy per-track `_cvOutputRotate` base is never settable (same dead pattern as gate) and
the old `_cvRotateInterpolate` route flag has **no setter anywhere** — the interpolation path
has never executed. CvOutputCrossfade revives that math as a first-class, reachable target;
the flag is retired (the target itself means "crossfade," no toggle).

## Slice 1 — CvOutputRotate → discrete group rotation

Mirror spec 018 for CV; drop the dead interpolation path entirely.

- **RoutingEngine**: CvOutputRotate handled at route level → `(_cvRotateMask, _cvRotateAmount)`
  via the shared `gateRotationFromSource` (source → int over ±8) + `rotatedGroupMember`
  (already TDD'd, generic). Single group, lowest-index route wins. Reset per frame. Off the
  legacy shaper branch.
- **Engine CV loop** (`Engine.cpp:658-713`): pool = mask members; rotate by the one int amount
  with `rotatedGroupMember`. **Delete** the float-interp block (`cvRotateInterpolated`/
  `cvRotateValue` crossfade, lines 663-689) and the per-track `cvOutputRotate()` read.
- **Remove**: the legacy-branch `_cvRotateValues`/`_cvRotateInterp` writes; the CvOutputRotate
  case in `writeTarget`; the now-unused `_cvRotateValues`/`_cvRotateInterp` arrays +
  `cvRotateValue()`/`cvRotateInterpolated()` accessors (only the dead interp block read them).
- **Vestigial**: per-track `_cvOutputRotate` (base 0) and the route `_cvRotateInterpolate`
  field — serialized bytes kept, no ProjectVersion bump.
- No new pure logic (reuses the gate helpers) → verified by build + the route suite; no
  redundant unit test.

## Slice 2 — CvOutputCrossfade (new target)

- Append `Target::CvOutputCrossfade` (targetSerialize ID assigned at the tail — format-safe).
  `targetInfos` row ±8, `targetName` "CV Out Xfade", register the source-picker entry.
- **RoutingEngine**: route-level → `(_cvXfadeMask, _cvXfadeAmount float)` (no round — keep the
  fractional position). Single group, lowest-index wins. Shares the mask-group model.
- **Engine CV loop**: for a member jack at position `p`, `pos = (p − N) mod K` (float);
  `lo = floor(pos)`, `t = frac`, `hi = (lo+1) mod K`; emit `cv(member_lo)·(1−t) + cv(member_hi)·t`.
  This is the existing interpolation math, now driven by an intentional target.
- TDD a pure `crossfadeMix(loValue, hiValue, t)` + the float position helper
  (`floatGroupPosition(p, N, K)`); engine wiring by build + suite.

## Constraints

- No ProjectVersion bump (vestigial fields kept; new target appends format-safely).
- sim + STM32 release green, under the 983040 ceiling.

## Out of scope

Deleting the legacy per-track branch + 7 stale shapers — it still catches mismatched
(target, mode) pairs as a no-op fallthrough; assess separately once confirmed nothing
legitimately falls through. scaleSource.
