# routing-matrix — unified scope-addressed mod matrix (name-agnostic engine)

Branch: `refactor/routing-matrix` (off dev). Won't merge until hardware testing.
Plan: `docs/plans/2026-05-31-002-routing-mod-matrix-overhaul-plan.md`.
Sub-spec: `docs/plans/2026-06-01-003-routed-value-storage-subspec.md`.

## What it is

Unify the fragmented routing (Routing matrix + Bus + Shaper bank + scope) into one
name-agnostic matrix: a route delivers a normalized value into an addressed param
slot `(scope, paramKey)`; the engine never knows the target's name/range/meaning.
CvRoute and the Modulator engine stay independent (referenced, not absorbed).

## Landed on branch (code, all green, Codex-clean, nothing wired live)

- **U1** `RouteParam` — `(scope, paramKey)` → row → apply hook; append-only keys;
  Structural/unknown rejected; **fail-closed guard** (null object / kind mismatch).
- **U2** `GlobalParamTable` — Tier-0 value-writes (Tempo/Swing/CvRouteScan/CvRouteRoute),
  parity-tested vs `writeTarget`.
- **Trigger unification** — Reset/PlayToggle/RecordToggle/TapTempo collapsed onto one
  `RouteState.gateMask` (rising-edge, o_C IOFrame model); fixed the TapTempo
  function-static. Live behavior-preserving refactor.
- **U3 (first type)** `NoteParamTable` — track + sequence rows, parity-tested + pattern
  fan-out.

> Note: the U2/U3 per-type tables predate the registry + `d`-coefficient model below
> and will be reworked when build resumes under the registry structure.

## Decisions locked (this is the durable record)

- **Address = `(scope, paramKey)`**, per-scope param tables; no flat `Target` enum.
  Per-type tables (not one shared table) to keep the engine name-blind; the only
  per-type code is the apply binding (the setter). One 8-way `tableForTrackMode`
  switch, run once per route resolution — not the 103-name smear.
- **Track/Sequence collapse to one per-track scope.** The 16-pattern fan-out is a
  hook detail, not an address. No per-pattern routing addressing.
- **Routed value is transient, not per-pattern model data** (R14). Drop the per-pattern
  `Routable` routed half + the ×17 copy loop; live value lives in a **model-owned
  transient override table** keyed `(track, paramKey)`, not serialized. Override stores
  an additive **delta**; read = `clamp(base + delta)`.
- **Unified combine — one signed per-track `d`, base-anchored, NO `min/max` window**
  (R15/R16). `out = clamp(base + d·u·range)`; combine flag picks `u`:
  - Modulate: `u = centered source` → bipolar around base, neutral at center (structural).
  - Absolute: `u = raw source` → sweep from base, `d·range` travel; base = anchor/floor.
  `d` subsumes the old `biasPct`+`depthPct`+`min/max` (three controls → one).
  `range` = the **`d`=100% displacement**: `(max−min)/2` bipolar, `(max−min)` unipolar/index.
- **Modulate shapers** must be center-preserving: allow None/TriangleFold; deny Crease
  (stateless but maps 0.5→1.0) + all stateful + VcaNext (→ `scaleSource`).
- **`scaleSource = Source` only** (R6, incl. Bus). Route-output-as-scaleSource rejected
  (re-introduces inter-route coupling). Bus-mediated scaleSource inherits the deferred
  Bus write/read ordering (R7).
- **Defects the overhaul closes:** `StochasticFeel` dead routed slot (Routable, undispatched);
  Stochastic/PhaseFlux Scale/RootNote base-mutation (routing writes serialized base).
- **Clean-break serialization** (R9): new format, `ProjectVersion` bump, old projects rejected.
- **Two surfaces:** global masked pool (broadcast, per-track `d`/shaper) + per-track local
  matrices (Indexed `RouteConfig` generalized). Inlets are a first-class target kind; each
  type owns its own.

## Param registry — lean & additive (in progress)

Curate **musically-usable** routable params into one registry, grouped by concept
(Transpose is one row backed by N types), not per-type lists. Range is a **class**
(bipolar/unipolar/index/gate); the route stores depth `%` only.

**Build lean, grow by append** — `paramKey` is append-only/never-reused, so adding is
cheap and removing-after-ship is costly. Triage is additive (promote a candidate when
there's a use), not subtractive.

Launch set = routable-today + owner-picked candidates + inlets. Decided so far:
- **PhaseFlux:** A/B inlets (stage-group routing, like Indexed); all five nudges
  (Warp/Response/Len/CyclePhaseWarp/Pulse, bipolar) are launch targets. PulseCount +
  Pitch*/Temporal* deep params stay candidates (append per-key later).

Working canvas: `.scratch/param-groups.html` (grouped concepts + triage),
`.scratch/param-census.html` (full per-type surface).

## Open / still owner's call

- The rest of the candidate triage (keep/drop) across the other types.
- Polarity confirm on a few PhaseFlux/Curve concepts (uni vs bipolar).
- Bus write/read ordering (R7, deferred — only bites at U7 cutover or a real collision).
- Per-track shaper-array composition (drop stateful shapers? — deferred).

## Next action

Finish the param triage → build the registry (one subsectioned list, range-class,
per-type apply bindings derived from it) → rework U2/U3 scaffolding onto it →
continue units (U4 inlets, U5 scaleSource, U6 groups, U6b override+combine, U7 cutover).
