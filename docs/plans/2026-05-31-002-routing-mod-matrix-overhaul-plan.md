---
id: routing-mod-matrix-overhaul
schema: plan
title: "refactor: Routing → unified scope-addressed mod matrix (name-agnostic engine)"
type: refactor
status: draft
date: 2026-05-31
---

# refactor: Routing → unified scope-addressed mod matrix

## Summary

Unify Performer's fragmented modulation-routing into one coherent, **name-agnostic**
matrix. Today the capability is spread across overlapping subsystems with their own
vocabularies, UIs, and serialization; the plan consolidates the ones that belong
together and makes the engine deliver a normalized value into an addressed parameter
slot without ever knowing the target's name, range, or meaning.

In scope to unify: the **Routing matrix** (16 slots), the **Bus**, the **Shaper bank**
(per-route processing, incl. the `VcaNext` → explicit scale-source extraction), and
**scope** (track mask + groups). Left independent and merely *referenced*: the
**CvRoute** engine (CV→CV physical routing — its own job) and the **Modulator engine**
(`Mod1–8`; it also emits independent MIDI, so it does more than feed routing).

Almost no new DSP. Every "feature" already exists fragmented; the work is unify, extract,
and make-explicit. The slot pool, normalized pipeline, depth window, and shaper bank are
reused. Routing splits into **two surfaces by broadcastability**: a **global masked pool**
(Performer-wide + broadcastable common params, track-mask kept so one scarce source drives many
tracks in one linked route) and **per-track local matrices** (Indexed's `RouteConfig`
generalized — the type-specific/cell params, in-context). Targets split into **direct**
(modulate a param) and **inlet** (a named per-track CV bus global routing fills); each type owns
its own inlets.

---

## Problem Frame

Two layered problems.

**Surface — the five-sources-of-truth smear** (`routing-five-sources-map.md`):
`Routing::Target` is a flat **103-entry** enum whose identity is spread across the enum,
`targetInfos[]`, the `writeTarget` per-trackMode dispatch switch, the picker predicates,
and each model's `writeRouted` leaf. Miss one → silent dead route (`StochasticFeel`
routes but never writes). Built for 2 homogeneous types, now stretched over 8.

**Deeper — fragmentation.** The modulation story is split across subsystems with
overlapping source/target vocabularies, separate UIs, and positional coupling:
- 3 routers: Routing (16-slot Source→Target), CvRoute (4-lane CV→CV), Indexed (2-slot
  internal `RouteConfig`).
- Shaper bank: 8 per-route processors (`Crease/Location/Envelope/TriangleFold/
  FrequencyFollower/Activity/ProgressiveDivider/VcaNext`) with per-track state.
- `VcaNext` implements scale-source by reading the **physically adjacent** route
  (`_sourceValues[routeIndex+1]`) — fragile positional coupling.
- Bus (`BusCv1–4`) is a Source *and* Target in Routing *and* an Input *and* Output in
  CvRoute — a shared bus with no single owner.

Root cause (product owner's cut): the `Target` enum conflates the **address** (which
scope, which parameter) with the **semantics** (name, range, what writing does). The
engine needs only address + a normalized number; the engine already computes
`denormalizeTargetValue(target, normalized)` then dispatches by name — the naming layer
is pure redundancy. "Complexity"/"Flow"/"Algorithm" are UI labels on a generic knob.

---

## Subsystem inventory (the rigor)

| Subsystem | What it is | Disposition |
|---|---|---|
| **Routing matrix** | 16 `Route` slots: `Source → Target`, `_tracks` mask, `_min/_max` depth, per-route `Shaper` | **Unify** — becomes the matrix core |
| **Shaper bank** | 8 per-route processors + per-track state (`RoutingEngine`) | **Unify + extract** — explicit node + scaleSource |
| **Bus (CV1–4)** | shared CV bus, src+dst in both routers | **Unify** — first-class matrix source/target, single owner |
| **Scope** | per-route `_tracks` mask; Indexed `targetGroups` cell-bitmask; PhaseFlux stages/cells | **Unify** — track + group scope as route property |
| **Inlets** | named per-track CV buses filled by global routing: DiscreteMap `_routedInput/Scanner/Sync`, Indexed `_routedIndexedA/B` | **Unify** — first-class target kind (R12); each type owns its own (no borrow, R13) |
| **Indexed local matrix** | `RouteConfig` (routeA/B → cell `ModTarget`, group-scoped, scale-aware) + `IndexedRouteConfigPage` | **Reference** — the prototype for `applyRouted` + the local-matrix UX; generalize, don't centralize |
| **CvRoute** | 4-lane CV→CV physical router + scan/route | **Independent** — referenced, not absorbed (different job) |
| **Modulator engine** | `Mod1–8` (Sine…ADSR…chaos) + independent MIDI out | **Independent** — referenced as sources, not absorbed |

What's genuinely absent (and stays out): Step / parameter-locks.

---

## What already exists (do not rebuild)

- Fixed slot pool: `CONFIG_ROUTE_COUNT = 16`, `std::array<Route,16>`.
- `Route` = `{ Source _source; Target _target; uint8_t _tracks; float _min,_max; Shaper per-track }`.
- Normalized pipeline: `writeTarget(target,tracks,normalized)` → `denormalizeTargetValue` → dispatch.
- Single management page: `RoutingPage`.
- Per-scope precedent: Indexed `RouteConfig {source, targetParam, amount, targetGroups}` × 2 + combine, own page — group scope already real.
- Modulator engine: the "internal LFO/env/random/chaos sources" already built.

---

## Requirements

- R1. **Name/semantics-agnostic engine.** Apply path reads a source (normalized), runs the
  route's shaper + depth window, and calls `scope.applyRouted(paramId, value)`. No target
  names/ranges/`switch` in the engine.
- R2. **One param table per scope** (global; each track type) = the single source for UI
  label, range/format, applicability, and the engine apply hook. Add a routable param = add
  one row.
- R3. Route addresses `(scope, paramKey)` not a flat `Target`. `scope ∈ {Global, Track(mask),
  Group(...)}`.
- R4. **Fixed Core + Extended targets.** Fixed Core (~16–24) is a hot-path direct array
  (the existing common Track/Sequence params); Extended is `(scope, function, param)`
  resolved to an index at load. Per-param **safety flags**: `fixed / continuous / discrete /
  structural` (structural = UI-only, never routable).
- R5. **Scope = track mask + group.** Generalize Indexed `targetGroups` so a route can target
  a cell/stage group within a track (Indexed, PhaseFlux); track-wide stays the default.
- R6. **Shapers stay, as explicit nodes.** Keep the processor bank; **extract `VcaNext`
  into an explicit `scaleSource` field** on the route, instead of the positional next-route hack.
  **F3 caveat:** old `VcaNext` scaled by the neighbor route's *processed output* (after its
  shaper+depth), not a raw source. `scaleSource` reading a raw source value is a **semantic
  change** — R11 parity for VcaNext patches only holds if `scaleSource` can also reference a
  *route's post-shaper output*, not just a raw `Source`. Decide whether scaleSource ∈ {Source}
  or {Source ∪ route-output} at U5. (Per-track-array composition for shapers is deferred — see
  Design.)
- R7. **Bus ownership stays with Engine** (it already owns `_busCv[]` + the safety
  slew/decay via `setBusCv`/`busCv`/`applyBusSafety`). The matrix and CvRoute remain
  **co-clients** that write through `setBusCv` and read through `busCv` — neither "owns" it.
  **Write-order is UNRESOLVED (F9), and it's a correctness question, not perf.** Because
  `setBusCv` slews, two writers to the same index produce *different CV depending on order* —
  the loser's slew still moved the rail. So "last-writer-wins" is insufficient: the plan must
  (a) pin a deterministic order (matrix recompute vs CvRoute update), and (b) ensure only the
  winning writer's delta is applied per index per tick (skip the loser's slew). Decide before
  U2 (which routes the global/Bus branch). Consistent with R10 (CvRoute behavior unchanged —
  only the Engine-side write arbitration changes).
- R8. **Two routing surfaces, split by broadcastability** (slot-ownership decision below).
  - **Global masked pool** — Performer-wide (Tier 0) + broadcastable common params (Tier 1/2):
    keeps `scope = Track(mask)` so one scarce source (4 CV in) drives many tracks in **one
    route, one linked edit** — the "easy to assign" worth preserving.
  - **Per-track local matrix** — Tier 3/4 + cell/group + inlets (the Indexed `RouteConfig`
    model generalized); single-track, in-context, with **assign-from-parameter** (long-press a
    param → local route). This is where the 10–30 per-track params live, off the global list.
- R12. **Three target kinds (taxonomy).** (F3 — Bus is a distinct third kind, not direct/inlet.)
  - **Direct** — modulate a param straight (Octave, Tempo, Stochastic Complexity, a cell's
    Duration). The matrix writes the value to the param.
  - **Inlet** — a named per-track CV bus that global routing fills (`_routedX`), consumed
    internally either **hardwired** (DiscreteMap: Input/Scanner/Sync feed the engine) or by a
    **local matrix** (Indexed: A/B feed `RouteConfig`). Inlets are the global→local bridge.
  - **Shared rail (Bus)** — `BusCv1–4`, Engine-owned, both source and target, read by other
    routes/CvRoute. Not a param, not a per-track inlet — a global CV rail. Carries the
    write-order/slew hazard (R7/F9).
- R13. **Each track type owns its inlets — no borrowing.** Kill the Indexed-borrows-DiscreteMap
  drift (`isDiscreteMapTarget` in the Indexed dispatch). A type declares its own inlets in its
  param table; no shared enum group across types.
- R9. **Clean break — new project format, no migration** (owner decision, 2026-05-31).
  Design the new `Route` bit-structure directly. **Bump `ProjectVersion`** to a new
  `VersionRoutingMatrix`; the loader **rejects any file `< VersionRoutingMatrix`** with a clear
  message (F4: the floor is symbolic — latest shipped is **Version35**, not v34; rejecting "≤34"
  would wrongly accept and misparse a v35 file). No sentinel, no forward-compat, no migration.
  **This deliberately overrides the post-Version35 no-bump policy** in `ProjectVersion.h`
  ("do NOT add a new enum entry / do NOT bump") — that policy is for incremental dev changes; a
  fundamental format overhaul is the documented exception (owner). **F4 blast radius:** the
  project file is one linear version-gated stream (`Project::read`); `_routing.read` sits
  mid-stream (before midiOutput/cvRoute/tracks), so changing the Route record length shifts
  every downstream byte — the bump re-floors the *whole file*, not "routes only." Harmless under
  full rejection, but the format change is project-wide, not routing-local.
- R10. **CvRoute and Modulator engine untouched** in behavior; the matrix references `Mod1–8`
  as sources and leaves CvRoute as the independent CV→CV path.
- R11. No behavior regression for **Absolute** routes rebuilt in the new format (a given
  source→param→depth produces the same movement as the old equivalent). Absolute is the
  parity-preserving default. **Modulate (R15) is an owner-approved intentional behavior change**
  for the bipolar params — a rebuilt Transpose/Octave/etc. route modulates around base instead of
  replacing it; that is the desired new semantics, not a regression. Tests prove both: Absolute
  matches the old movement, Modulate is neutral at source-center. STM32 flash/RAM within budget.
- R14. **Routed value is transient, not per-pattern model data** (sub-spec
  `2026-06-01-003-routed-value-storage-subspec.md`). Today the `routed` half of
  `Routable<T>` is stored per `Sequence` ×17 (patterns+snapshot) and `writeTarget`
  loops all 17 writing the same value. Split it: `Sequence`/`Track` keep **base only**;
  the live routed value lives once per `(track, paramKey)` in a **model-owned transient
  override table** (not serialized). Read combines base + override at access; the
  ×17 copy loop and routed duplication are deleted. Closes two defects the split
  exposes — the **StochasticFeel dead routed slot** (Routable, never dispatched) and
  the **Stochastic/PhaseFlux Scale/RootNote base-mutation** (routing writes serialized
  base). Migration is per `(param × type)` — see the sub-spec's inventory (kinds
  S/T/P/B/D in scope; X/I/G/R untouched).
- R15. **Per-param combine: Absolute vs Modulate** (`RouteParam::Flag`). Absolute =
  route replaces base (today's only mode), uses the `min/max` window. Modulate =
  `clamp(base + d·source_centered, hardMin, hardMax)`, ignores the window, and only
  **center-preserving** shapers allowed (None/TriangleFold; Crease and all stateful denied;
  VcaNext denied by policy → `scaleSource`, R6). Neutral at source-center is structural and
  testable. The already-bipolar params (Octave/Transpose/Offset/Rotate/biases) are the
  Modulate set; indices/enums/transport stay Absolute. Owner confirms the assignment.
- R16. **Combine subsumes bias + depth** (collapses the per-track shaping arrays). `biasPct`
  is **dropped** — a DC source shift that Modulate forbids (breaks neutral) and Absolute
  makes redundant (the window positions the value). `depthPct` becomes the route's **signed
  per-track gain `d[8]`** on the centered source (the Modulate magnitude; for Absolute, the
  source→window gain). Two per-track ×8 arrays → one; `applyBiasDepthToSource` loses the bias
  term. Per-track `d` is kept so a masked global route still tunes each track (F1).

---

## Key Technical Decisions

- **Forks resolved (owner, 2026-05-31):** CvRoute stays independent (CV→CV is a different
  job); Modulator engine stays an independent source provider (it also emits MIDI — folding
  it into slots would break that and buy little on hardware).
- **Address = `(scope, paramKey)`**, not the flat enum. Single change that kills the
  103-entry namespace and the per-name dispatch.
- **Per-scope param table = the one source of truth.** Row: `{ name, range/format, flags,
  applyRouted(scopeObj, normalized) }`. Replaces enum + `targetInfos` + dispatch switch +
  `isXxxTarget` predicates + per-model `writeRouted`. A track type adds a param by adding a
  row; the row *is* the apply hook, so no silent dead route is possible.
- **Fixed Core hot array + Extended tree** (R4) — array for the common params on the hot
  path, hierarchical resolution for the long tail, resolved to index at load.
- **Shaper bank kept; `VcaNext` extracted** to an explicit `scaleSource` (R6) — removes the
  positional `routeIndex+1` coupling.
- **Scope includes groups** (R5) by generalizing Indexed's `targetGroups`.
- **Direct vs Inlet targets** (R12) is the core taxonomy. Direct = modulate a param; Inlet =
  named per-track CV bus filled by global routing, consumed internally. Verified in code:
  DiscreteMap `_routedInput/Scanner/Sync` and Indexed `_routedIndexedA/B` are inlets, not param
  mods. The flat enum hid this distinction.
- **Two pools, split by broadcastability** (R8; slot-ownership resolved). Global **masked**
  pool for Tier 0/1/2 (one scarce source → many tracks, linked edit — source scarcity makes
  the mask worth keeping for common params). Per-track local pools for Tier 3/4 + cell/group +
  inlets (single-track, in-context). The mask's ×8 shaper-state cost is contained to the small
  global pool; type-specific params can't broadcast across types so they're naturally local.
- **Indexed is the reference, not an exception.** Indexed = inlets (A/B) **+ local decision
  matrix** (`RouteConfig` → cell `ModTarget`, group-scoped, scale-aware apply) =
  `applyRouted` made concrete. DiscreteMap = inlets **+ hardwired** consumption. Generalize
  Indexed's pattern to every type; **do not collapse its local matrix into the global page**
  (that would destroy the better affordance). "Fold" means *share the engine mechanism*, not
  *centralize the UI*.
- **Each type owns its inlets** (R13) — remove the DiscreteMap borrow from Indexed.
- **Concept-equivalence gate** for cross-type naming (see the applicability matrix): unify to one
  canonical name + shared target only if same axis + domain + result + kind; else keep distinct.
  Resolves "Phase" (Curve/PhaseFlux `globalPhase` → canonical **Phase**, a Tier-2b mix-in) vs
  DiscreteMap `scan` (stays distinct — spatial index, inlet-fed). Guards UI naming, not just targets.
- **Keep 16 slots + depth window.** Reasonable, bounded, shipped. (Slot ownership — one global
  pool vs per-track local pools — is the open UX/RAM question below.)
- **Clean-break serialization** (R9): new `Route` format, `ProjectVersion` bump, old projects
  rejected on load — no migration, no sentinel. The `Target` enum is **deleted**, not kept.

---

## High-Level Design

Slot, name-free, absorbing shaper + scale-source + scope:

**Per-track gain + shaper are retained (F1, owner 2026-05-31), but bias/depth collapse to one
signed gain (R15).** The current `Route` carries `_biasPct[8]`, `_depthPct[8]`, `_shaper[8]`
(Routing.h:731-746) so one masked route tunes *each masked track* — the broadcast value to keep.
With the Absolute/Modulate combine (R15), `biasPct` is **dropped** (a DC shift; Modulate forbids
it, Absolute positions via the window) and `depthPct` becomes the route's **signed per-track gain
`d[8]`** on the centered source. Two per-track ×8 arrays → one. So the slot shapes:

```
// Global masked pool slot — fat, per-track tuning, few of these:
GlobalRouteSlot {
  Source  source;            // incl. Mod1-8, Bus
  uint8_t trackMask;         // broadcast set
  uint8_t paramKey;          // into Tier-0/1/2 table
  uint8_t combine;           // Absolute | Modulate (R15)
  float   min, max;          // window — ABSOLUTE only (Modulate ignores; base + d*centered)
  int8_t  d[8];              // PER-TRACK signed gain (was depthPct; biasPct removed)
  Shaper  shaper[8];         // PER-TRACK — Modulate restricts to center-preserving (R15)
  Source  scaleSource;
}
// Per-track local pool slot — thin, single-track, scalar:
LocalRouteSlot {
  Source source; Scope scope;   // Track(single) | Group(cellMask)
  uint8_t paramKey; uint8_t combine; float min,max; int8_t d; Shaper shaper; Source scaleSource;
}
```

**Deferred (owner): the composition of the per-track array** — which shapers it carries, and
whether to **drop the stateful shapers** (Envelope/FreqFollow/Activity/ProgressiveDivider —
the ones whose `shaperState[8]` drives the ×8 RAM). Dropping the stateful ones would shrink the
fat slot substantially. Decide at implementation; the RAM figures below are upper bounds until then.

Engine apply (replaces `writeTarget` name dispatch), at the single-pass per-tick recompute:

```
for slot in active routes:
    for each track t in slot.scope:
        s = sourceValue(slot.source)                    // raw normalized [0,1]
        s = shape(slot.shaper[t], s, state[t])          // per-track shaper + state
        if slot.scaleSource != None: s *= scaleValue(slot.scaleSource, t)   // see F3 caveat
        if slot.combine == Absolute:
            v = slot.min + s * (slot.max - slot.min)    // map shaped source through window
            v = applyGain(v, slot.d[t])                 // signed per-track gain (R16); no bias
            scopeObj(t).writeOverride(slot.paramKey, v)             // replaces base
        else: // Modulate
            offset = slot.d[t] * (s - 0.5) * 2          // centered, no window, no bias (R15/R16)
            scopeObj(t).writeOverrideOffset(slot.paramKey, offset)  // read = clamp(base + offset)
```
Bias is gone (R16); `min/max` is consulted only on the Absolute branch; Modulate never
touches the window. The override table (R14) holds the per-`(track,paramKey)` value/offset that
the model read combines with base.

Param table per scope (global; common-track; per-track-type) declared once. A row is either a
**direct** param or an **inlet** (R12):

```
ParamRow { const char *name; Range range; Flags flags;   // fixed/continuous/discrete/structural/inlet
           void applyRouted(Scope&, float normalized); }  // direct: writes param; inlet: fills _routedX
```

**Two-stage routing (the Indexed/DiscreteMap pattern, generalized):**

```
global page:   source (CvIn/Mod/Midi) ── route ──> track INLET  (_routedX, e.g. A/B, Input/Scanner)
local matrix:  inlet (or any source) ── route ──> direct param / cell-group target
                                                   (Indexed-style; DiscreteMap hardwires instead)
```

Global routing transports CV into named track inlets (small, scalable list). The per-track
**local matrix** decides what inlets (and other sources) do to that track's direct/cell params
— this is where the 10–30 params live, in context.

UI:
- **Global page** — Performer-wide targets (Tempo/Swing/Bus/transport) + source→inlet patches
  only. Stays small.
- **Local in-track matrix** — per track, lists that track's route slots; param picker shows
  only that type's rows (Teletype shows Teletype params, never Octave). Quick = Fixed Core,
  Deep = function→param. **Assign-from-param** (long-press a param) creates a local route to it.
- Indexed's existing `IndexedRouteConfigPage` is the prototype for the local matrix page.

---

## Open Questions

### Resolved this design pass (2026-05-31)
- **Local-first vs global routing** → two surfaces by broadcastability (R8): global masked pool
  for Tier 0/1/2 (keeps the mask — source scarcity), per-track local for Tier 3/4/cell/inlets.
- **Fold Indexed?** → generalize its *mechanism* (param table + `applyRouted` + group scope),
  keep its *local matrix UX* (don't centralize). Indexed is the reference, not an exception.
- **Direct vs inlet** taxonomy adopted (R12); each type owns its inlets (R13).

### Slot ownership — RESOLVED: two pools, split by broadcastability (2026-05-31)
Source scarcity decides it. A scarce source (4 CV in) fanning to many tracks is only useful
for **common params**, and the global track-mask makes that one route + one linked edit — the
"easy to assign" worth keeping. Type-specific params can't broadcast across types anyway. So:
- **Global masked pool** (bound small, ~8 routes) — Tier 0/1/2 (Performer-wide + universal +
  pitched). Keeps `scope = Track(mask)` → one CV → many tracks, **linked edit**. Pays the ×8
  shaper-state cost, contained to this small pool (~1.4 KB).
- **Per-track local pools** (N≈3–4/track) — Tier 3/4 + cell/group + inlets. Single-track scope,
  no mask, no ×8 fan-out (~0.6 KB). The Indexed-style local matrix UX.
- Combined ≈ ~2 KB (vs ~3.2 KB pure-global), and **both** broadcast-linked-edit *and* local
  in-context routing are preserved. The `scope` field already models both (`Track(mask)` vs
  `Track(single)`/`Group`); ownership = which pool a route lives in, by scope.
- **F2 caveat — the saving is conditional, not automatic.** Today `_routeStates[CONFIG_ROUTE_COUNT]`
  is one static `[16]` array with the ×8 `shaperState` baked in; merely *labelling* routes as
  two pools saves nothing. The ~2 KB only materializes if the engine actually allocates **a
  small global pool (`RouteState[~8]`, ×8 state) + separate per-track scalar slots** — and the
  ×8 figure is an **upper bound** that shrinks further if the deferred per-track-array
  composition drops the stateful shapers (see Design/F1). Treat the numbers as a target to
  validate at U7/U8, not a settled budget.

### paramKey stability — RESOLVED: append-only numeric keys (F6)
Keys are explicit numeric, **append-only, never reused after deletion**, never the array index.
This is the constraint that makes "add a routable param = add one row" safe for saved files
(reordering/adding rows never shifts existing routes). Hard rule, enforced at U7. (Was wrongly
listed as open while U7 depended on it.)

### Still open
- Param-table representation on a no-heap MCU: `constexpr` rows with function pointers (flash
  cost) vs virtual `applyRouted` per scope (vtable indirection). Measure on STM32.
- Group scope granularity: reuse Indexed's exact `targetGroups` semantics, or a unified
  cell/stage group model shared by Indexed + PhaseFlux?
- Source-side fan-out (`RoutingEngine::updateSources`) has the same smear — in scope or a
  follow-up? (Leaning: follow-up; sources are a closed set.)
- Bus write-order: matrix-recompute vs CvRoute-update within a tick — which runs first, and
  is last-writer-wins acceptable, or does one source take priority per index? (Ownership
  itself is settled: Engine owns storage; both are co-clients — R7.)
- Shaper-per-route vs the current per-track shaper state array — does the unified slot carry
  its own shaper state?

---

## Param applicability matrix (source data for U3/U4)

Derived mechanically from setter presence per model (`grep "void setX" *Sequence.h / *Track.h`),
not assumption. The flat "generic Track/Sequence block" is actually four overlapping
applicability sets. In the new model **applicability = param-table membership**: each type
composes its table from the tiers it backs, so an inapplicable param simply has no row (can't
be picked, can't silently no-op). Deleting the universal block is the untangle.

**Tier 0 — Whole-Performer (global scope, not per-track):** Play, PlayToggle, Record,
RecordToggle, TapTempo, Tempo, Swing, CvRouteScan, CvRouteRoute.

**Tier 1 — Universal per-track (all 8 types):** Mute, Fill, FillAmount, Pattern, Run, Reset,
CvOutputRotate, GateOutputRotate (on base `Track`), Divisor, ClockMultiplier.

**Tier 2 — Pitched mix-in (per backing type):**
| Param | Backed by |
|---|---|
| Scale, RootNote | Note, Tuesday, DiscreteMap, Indexed, Stochastic, PhaseFlux (not Curve/MidiCv) |
| Transpose | Note, MidiCv, Indexed, Stochastic, PhaseFlux |
| Octave | Note, Indexed, Stochastic, PhaseFlux |
| SlideTime | Note, Curve, MidiCv, Indexed, Stochastic, PhaseFlux (not Tuesday/DiscreteMap) |

**Tier 3 — Step-range mix-in (Note/Curve-family only):**
| Param | Backed by |
|---|---|
| FirstStep | Note, Curve, Indexed |
| LastStep | Note, Curve only (asymmetry — not Indexed) |
| RunMode | Note, Curve, Indexed |

Tuesday/DiscreteMap/PhaseFlux/Stochastic back **none** of Tier 3 — each has its own traversal.

**Tier 2b — Phase mix-in (temporal-phase backers):**
| Canonical | Backed by (per-type field) | Note |
|---|---|---|
| **Phase** | Curve (`globalPhase`), PhaseFlux (`globalPhase`) — both `clamp 0..1`, temporal-cycle offset | verify Note/Tuesday Free-mode for the same axis at U3/U4 |

One canonical name "Phase" across temporal-phase types (passed the gate below). **Not** the same
as DiscreteMap `scan` — that's a spatial segment index (0..34) fed by an inlet → different axis
*and* kind, kept distinct as "Scan."

**Tier 4 — Type-specific:** Offset (Curve) · Rotate (Note/Curve) · 5× ProbabilityBias
(Note/Curve) · plus each type's signature block (Tuesday algo, Stochastic, Curve DSP, etc.)
and **inlets** (Indexed A/B, DiscreteMap Input/Scanner/Sync).

### Concept-equivalence gate (cross-type naming criterion)

When two types name a param differently, unify to **one canonical name + one shared routable
target** only if **all four** hold; if any fail, keep them distinct (a shared name would lie):

1. **Same axis** — {temporal-phase, read-position/index, pitch, gate/length, level, probability,
   rate/clock}.
2. **Same domain semantics** — "fraction of cycle 0..1" vs "index into N" are different even if
   both normalize.
3. **Same modulation result** — smooth drift vs stepped jumps.
4. **Same target kind** (R12) — direct param vs inlet.

Worked example: Curve/PhaseFlux `globalPhase` pass all four → canonical **Phase**. DiscreteMap
`scan` fails 1, 2, and 4 → stays **Scan**. Run this gate during U3/U4 table construction; it is
the guard against cosmetic over-merging (UI naming) as much as target sharing.

A type's param table = Tier 1 ∪ (Tier 2/2b rows it backs) ∪ (Tier 3 rows it backs) ∪ Tier 4 (own).
No universal block; no `isXxxTarget` predicates; the matrix above is the literal composition.

---

## Implementation Units (provisional)

- **U1. Param-table + Scope abstraction.** `ParamRow`/`Flags`/`Scope`, per-scope
  `applyRouted(paramKey, normalized)`. Pure addition.
- **U2. Global scope table.** Tier-0 params (Engine/Project/Bus) → global table; route the
  global branch through it. Characterize: tempo/swing/bus routes unchanged.
- **U3. Common-track table + mix-ins** (per the applicability matrix). Tier-1 universal table;
  Tier-2 pitched + Tier-3 step-range as mix-ins a type includes only if it backs them. A
  type's table = Tier 1 ∪ backed Tier-2/3 rows. No universal block; no `isXxxTarget`.
- **U4. Per-track-type tables + inlets** (Tier-4). Tuesday/Curve/DiscreteMap/Stochastic port
  their signature blocks; rows tagged **direct** or **inlet** (R12). Each type declares its
  **own** inlets — **remove Indexed's DiscreteMap borrow** (`isDiscreteMapTarget` in the
  Indexed dispatch); Indexed keeps only A/B. **PhaseFlux/Teletype gain routable params** by
  declaration. Per-type table = Tier-1 ∪ backed Tier-2/3 (from U3) ∪ Tier-4 own.
- **U5. Shaper-as-node + scaleSource extraction.** Add the explicit `scaleSource` field, apply
  it, and retire the `routeIndex+1` neighbor read outright. No migration needed — old projects
  don't load (R9 clean break), so there are no stored legacy `VcaNext` patches to round-trip.
- **U6. Group scope.** Generalize Indexed `targetGroups` into the slot's `Scope`.
- **U6b. Routed-value override table + combine mode — ADDITIVE** (R14/R15/R16; sub-spec
  `2026-06-01-003-routed-value-storage-subspec.md`). **Builds alongside the live old storage;
  deletes nothing** (the destructive removal is U7, so U1–U6b all stay buildable behind the
  authoritative old dispatch). Add the **model-owned transient override table** keyed by
  `(track, paramKey)` (not serialized), the **Absolute/Modulate** combine flag with the
  center-preserving Modulate contract (depth-only, allowed shapers None/TriangleFold), and the
  signed per-track gain `d` (R16). Wire the new apply path to write the override table and the
  read/combine to consult it — exercised by tests, not yet the engine default. The existing
  `Routable` routed half + the `writeTarget` copy loop **stay live and unchanged** this unit.
  Closes-by-design (verified at U7): the **StochasticFeel dead slot** and **Stochastic/PhaseFlux
  Scale/RootNote base-mutation** — the new path never mutates base. Depends on the per-type
  tables (U3/U4); the `scaleSource`/VcaNext handling depends on **R6 staying open until U5**
  (see below). Tests per the sub-spec (neutral-at-center matrix, base-at-clamp, denied-shaper
  rejection). **R6 caveat:** R6's `scaleSource ∈ {Source}` vs `{Source ∪ route-output}` choice
  is unresolved (decided at U5); U6b/U7 must not treat VcaNext extraction as settled until then.
- **U7. Route re-addressing + new format + ENGINE CUTOVER + destructive storage flip** (F5).
  `Route` stores `(source, scope, paramKey, combine, per-track `d`/shaper, scaleSource)`; bump
  `ProjectVersion`; loader rejects `< VersionRoutingMatrix` (R9). **This is the cutover and the
  only destructive step:** drop the per-pattern `Routable` routed half, delete the ×17
  `writeTarget` copy loop, drop `biasPct`, rename `depthPct`→`d` (R16); `Sequence`/`Track` keep
  base only; engine switches to the param-table `applyRouted` + override-table read. New-format
  routes have no `_target`, so the old dispatch *cannot run* on them. U1–U6b build the tables +
  override path *alongside* the still-authoritative old dispatch; U7 flips storage **and** engine
  together; U9 only deletes the now-dead old code. ("Old dispatch stays live until **U7**," F5.)
  **paramKey rule (F6, hard, not open):** keys are explicit numeric, **append-only, never
  reused after deletion** — never the array index. This is the constraint that makes "add a
  routable param = add one row" safe for saved files. Fixtures: round-trip with stable keys;
  reorder rows → saved routes unchanged; too-old project rejected cleanly.
- **U8. UI — global masked page + per-track local matrix.** Global `RoutingPage` = the masked
  pool: Tier 0 + broadcastable Tier 1/2 common params (track-mask intact) + source→inlet
  patches. Build the **per-track local matrix** page (generalize `IndexedRouteConfigPage`):
  Tier 3/4 + cell/group + inlets, scoped param picker (only that type's rows), Quick/Deep,
  **assign-from-param** (long-press → local route). Two pools by scope (slot-ownership
  decision, resolved).
- **U9. Retire the smear.** Delete the now-dead `targetInfos`, dispatch switch, `isXxxTarget`,
  per-model `writeRouted`, **and the `Target` enum itself** — all unused since the U7 cutover
  (no migration table; old files don't load).

---

## System-Wide Impact

- **Serialization:** clean break (R9). New `Route` format; `ProjectVersion` bump; old projects
  rejected on load. No migration, no sentinel, no forward-compat layer.
- **Hot path:** the new `applyRouted` loop runs at the single-pass recompute site the spine's
  engine-rework (the merged U4 of the *spine* plan) built; it **replaces** the name dispatch at
  the **U7 cutover** of *this* plan (F5 — not before; U1–U6 stage the tables behind the live old
  dispatch).
- **CvRoute / Modulator:** untouched (R10).
- **Every track type** gains a param-table declaration — heterogeneity lives there cleanly,
  not in one global enum. **tt2:** PhaseFlux/Teletype routing becomes declarable.

---

## Risk Analysis & Mitigation

- **Clean-break load safety (R9/R11)** — old projects must be *rejected cleanly*, never
  misparsed. Mitigation: the loader checks `ProjectVersion` first and refuses `< VersionRoutingMatrix`
  (symbolic floor; latest shipped is v35 — F4) with a message; fixture proves a too-old file is
  rejected without crash or partial load.
- **Flash/RAM of param tables** — measure on STM32; share the common-track table across types;
  prefer `constexpr` flash tables.
- **Acknowledged added scope (F7)** — two items are *expansion*, not pure unification, and are
  named as such: (a) **PhaseFlux/Teletype gain routability** (new surface, new tests) — kept
  because the overhaul is the natural time to add it, but it is added scope; (b) the
  **concept-equivalence gate** is owner-requested (Phase vs Scan) but currently has one worked
  example — treat as a lightweight rule, not heavy machinery, until a second case appears.
- **Minimum-viable was considered (F8)** — the lone reported symptom (`StochasticFeel` dead
  route) could be patched with a compile-time "every Target has a writer" assert. The owner
  chose the full overhaul deliberately (architectural-fix preference, no velocity pressure), not
  by default. Table correctness is where dead-route-class bugs reappear — `applyRouted` per row
  must be reviewed, not assumed mechanical.
- **Scope creep (held out)** — sources fan-out, Step/p-locks, internal-modulator expansion all
  explicitly out; CvRoute/Modulator untouched.
- **Big-bang risk** — U1–U6 stage param tables + the new apply path *behind* the live old
  dispatch (old still authoritative); **U7 is the single cutover** (storage + engine flip
  together); U9 deletes the dead old code. The risky moment is U7, not spread across all units.

---

## Sequencing

U1 → U2 → U3 → U4 → U5 (shaper/scaleSource) → U6 (groups) → **U6b (routed-value storage +
combine mode, R14/R15)** → **U7 (re-address + new format + engine cutover)** → U8 (UI) → U9
(delete dead old code). U1–U6b stage behind the live old dispatch; the cutover is U7; U9 is
pure deletion. U6b must precede U7 (the cutover flips Route storage onto the override-table
read path). CvRoute and the Modulator engine are not modified.
