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

Working canvas (ephemeral): `.scratch/param-groups.html` (grouped) / `.scratch/param-census.html`
(full per-type surface). **Snapshot below is the durable copy.** Status: **now** = routable today
(parity baseline), **LAUNCH** = owner-promoted into the launch set, **deferred** = decided out of
launch (append per-key later), **fix** = routable but defective today, **excl** = structural/internal (out).

**Triage RESOLVED (owner, 2026-06-01):** launch = all **now** + **LAUNCH** rows + the **fix**
items repaired. Everything **deferred** is out of v1 (append-only key when a use appears). Teletype
is out of the matrix entirely.

Range class: bipolar ± / unipolar % / index N / gate / inlet.

### Shared concepts (one row, many types) — all **now**
Transpose (bip), Octave (bip), SlideTime (uni), Rotate (bip), Scale (idx), RootNote (idx),
Divisor (idx), ClockMult (uni), RunMode (enum), FirstStep (idx), LastStep (idx), GateLength (uni),
Offset (bip), ProbabilityBias×5 (bip), **Phase** (uni, Curve+PhaseFlux globalPhase — **LAUNCH**, kept).

### Universal / Tier-0 — all **now**
Mute/Fill/FillAmount/Pattern, Run, Reset, Cv/GateOutputRotate · Tempo, Swing, CvRouteScan/Route,
Play/Rec/PlayToggle/RecordToggle/TapTempo, BusCv1–4.

### Type-specific
- **Tuesday** — now: Algorithm/Flow/Ornament/Power/Glide/Trill/StepTrill, GateLength/GateOffset,
  Octave/Transpose/Rotate, Divisor/ClockMult/Scale/RootNote. deferred: Skew/Start/LoopLength/
  MaskParameter/MaskProgression.
- **Curve** — now: Wavefolder Fold/Gain, DjFilter, Chaos×4, CurveRate, Offset/Rotate/SlideTime,
  Shape&Gate ProbBias, Divisor/ClockMult/RunMode/First/LastStep, Phase (shared, LAUNCH).
  deferred: Min/Max/Range/XFade, ShapeVariation/ShapeVariationProbability.
- **MidiCv** — now: Transpose/SlideTime. deferred: PitchBendRange/ModulationRange.
- **DiscreteMap** — now: Octave/Transpose/SlideTime/Offset, RangeHigh/RangeLow, Divisor/ClockMult/
  Scale/RootNote; inlets Input/Scanner/Sync. deferred: Threshold/GateLength.
  **SlewTime ≡ SlideTime** (owner) — drop as a separate concept; it is the SlideTime concept
  (resolve track `_slideTime` vs seq `_slewTime` storage at build).
- **Indexed** — now: Octave/Transpose/SlideTime, Divisor/ClockMult/Scale/RootNote/RunMode/FirstStep;
  inlets A/B. deferred: Duration/ActiveLength/GateLength.
- **Stochastic** — now: Complexity/Variation/Rest/Slide/Burst/Sleep/Mutate/Jump, Contour,
  MaskRhythm/TiltRhythm/GateLength/PatienceRhythm/NoteDuration/Rotate, Octave/Transpose/SlideTime.
  **fix:** Scale/RootNote/Divisor (base-write defect); **Feel** (dead slot — Routable, undispatched).
  deferred: LegatoProb/RepeatProb/Marbles Spread/Bias, MaskMelody/TiltMelody/PatienceMelody,
  DegreeRotation/MaskRotation.
- **PhaseFlux** — now: Divisor/ClockMult, Octave/Transpose/SlideTime. **LAUNCH:** A/B inlets
  (stage-group routing, like Indexed); **all five nudges** WarpNudge/ResponseNudge/LenNudge/
  CyclePhaseWarp/PulseNudge (bipolar), Phase (shared, LAUNCH). **fix:** Scale/RootNote (base-write).
  deferred: PulseCount, Pitch* (Range/Warp/Response/Window/Repeat/Rate), Temporal* (Warp/Response/
  Window/Repeat/Curve), StageLen/StageDivisor/PhaseShift, MaskMelody/TiltMelody.
- **Teletype** — **OUT of the matrix entirely** (owner, 2026-06-01). No matrix targets; Teletype
  stays script-routed (its own CV/trigger in/out, clips, pattern slots). No Teletype param table.

## Open / still owner's call

- Bus write/read ordering (R7, deferred — only bites at U7 cutover or a real collision).
- Per-track shaper-array composition (drop stateful shapers? — deferred).
- Polarity (uni vs bipolar) for any deferred param only when it's promoted later.
- **Indexed External sync inlet — owner call (Codex review, 2026-06-02).** Indexed has a live
  `SyncMode::External` whose engine reads `routedSync()` (`IndexedTrackEngine.cpp:202`), driven
  today by routing `DiscreteMapSync` into `IndexedTrack::_routedSync` (a borrowed target). The
  resolved Indexed launch set ("inlets A/B") omits any Sync inlet, and R13 drops the borrow — so
  the U4 Indexed table has no Sync row. Nothing breaks pre-U7 (old dispatch still feeds it), but
  **U7 must either retire Indexed External sync or add an Indexed-owned Sync inlet** (key 102).
- **paramKey storage width — U7 watch-item.** Registry key is `uint8_t` (`RouteParam::Row::key`),
  Tier-4 type-specific starts at 80 → ~176 slots left. Projected full registry (launch + all
  deferred) ≈ 130–160 keys, fits — but the width is part of the (clean-break) wire format and
  gets settled at U7 with the full count known, not widened speculatively now (Codex review).

## Registry landed (2026-06-02)

- **Shared `ParamKey` registry** in `model/RouteParamKey.h` — the single append-only numbering
  authority (F6). Tier-grouped with reserved gaps: Tier-0 Global 1–4, Tier-1 universal 20–21,
  Tier-2 pitched 40–45, Tier-3 step-range 64–66, Tier-4 Note-specific 80–83. Keys minted on use.
- **U2/U3 reworked onto it** — Global + Note tables dropped their local `enum Key`; rows reference
  `ParamKey::`. writeTarget-parity tests still green; STM32 release clean.
- **`TestParamRegistry`** sweeps every real table for unique non-zero row keys — turns the
  no-duplicate-key contract from comment-only into a run guard (`Table::find` returns first scan
  hit, so a dup would silently alias). Every per-type table appends to the sweep.

## U4 Curve landed (2026-06-02)

- **`ParamTableCurve.{h,cpp}`** — second per-type table, validating "one key, many types":
  reuses 8 keys it shares with Note (SlideTime/Rotate/GateProbabilityBias/Divisor/ClockMult/
  RunMode/First/LastStep) and appends Curve-specific (Offset, ShapeProbabilityBias, Wavefolder
  Fold/Gain, DjFilter, Chaos×4, CurveRate) + shared Phase (Tier-2b globalPhase, keys 46/60/84/90–97).
- Hooks mirror `writeTarget` exactly: CurveRate denorm 0–400 then /100; Wavefolder/DjFilter
  **truncate** float→int16_t (not round). Phase is a launch addition (no writeTarget parity),
  tested directly; lights up at U7. Added to the uniqueness sweep. Sim + STM32 release clean.

## U4 Indexed + inlet row-kind landed (2026-06-02)

- **`ParamTableIndexed.{h,cpp}`** — first table to carry **INLET** rows (R12). A/B inlets fill the
  per-track routed-CV field (`_routedIndexedA/B`, −100..100 → −1..1), flagged `RouteParam::Inlet`;
  added `setRoutedIndexedA/B` setters as the write API. Each type owns its inlets (R13) — legacy
  `DiscreteMapSync` borrow dropped. Registry inlet block: IndexedA=100, IndexedB=101.
- Direct rows reuse 9 shared keys. Hooks mirror `writeTarget` incl. the parity quirk that
  Divisor/Scale/RootNote write the **base** (no routed flag) while the rest write the routed slot.
  `Table::applyRouted` fires Inlet rows normally (only Structural is rejected).
- `TestParamTableIndexed`: routed-slot-aware parity, inlet-flag asserts, inlet fan-out. Added to
  the uniqueness sweep. Sim + STM32 release clean.

## U4 DiscreteMap landed (2026-06-02)

- **`ParamTableDiscreteMap.{h,cpp}`** — owns Input/Scanner/Sync inlets (the Sync the Indexed
  engine borrows lives here): track-level routed-CV fields filled raw (no scaling, no fan-out),
  flagged `Inlet`; added `setRoutedInput/Scanner/Sync`. Inlet keys 102–104; sig RangeHigh=110/
  RangeLow=111. **SlewTime≡SlideTime resolved:** no track `_slideTime`; shared SlideTime key →
  sequence `setSlewTime`. Parity: only SlideTime(slewTime)+ClockMult write the routed slot, rest
  base. Reuses 8 shared keys. Parity test + inlet flags + fan-out; sweep; sim + STM32 clean.
- **Codex BLOCK fixed:** DiscreteMap's track-routed params (Octave/Transpose/Offset/SlideTime/
  RangeHigh/RangeLow) fan to ALL `sequences()` incl. snapshot slots (`DiscreteMapTrack::writeRouted`
  range-for), not just patterns — hooks now mirror that; Divisor/ClockMult/Scale/RootNote stay
  pattern-only (writeTarget pattern loop). Snapshot-slot parity assertion added. DiscreteMap-only
  quirk (Note/Curve/Indexed write track fields, not fanned).

## U4 batch 1: MidiCv + Tuesday landed (2026-06-02, Codex-clean)

- **MidiCv** — two track-level routed-slot rows (Transpose/SlideTime), no fan-out; reuses shared keys.
- **Tuesday** — 14 sequence-level routed-slot rows, pattern fan-out; reuses Octave/Transpose/Rotate/
  Divisor/ClockMult; appends signature block Algorithm/Flow/Ornament/Power/Glide/Trill/StepTrill/
  GateLength/GateOffset (keys 120–128). GateLength/GateOffset use the clamped setter (no-op vs
  writeTarget's unclamped `Routable.write`, range always 0..100). Parity tests + sweep; sim + STM32 clean.
- **OWNER CALL — Tuesday Scale/RootNote:** dispatched no-ops today (`TuesdaySequence::writeRouted`
  has no case → `default:break`), so the triage's "now" listing is optimistic. Adding them is a
  fix-forward (like StochasticFeel), not parity — **omitted from batch 1, pending owner decision**
  on whether to wire them as a fix (append keys then) or drop from the launch set.

## U4 batch 2: Stochastic + PhaseFlux landed (2026-06-02, Codex-clean) — U4 COMPLETE

- **Live bugfix shipped** (`Routing.cpp` + `StochasticSequence.h`): routing *any* Stochastic-specific
  param was a dead no-op (outer guard omitted `isStochasticTarget`; Feel had no writeRouted case;
  Rotate took the track branch with no Rotate case). Fixed all three → Stochastic routing now works
  live. Codex-verified safe for non-Stochastic tracks.
- **Stochastic table** — 23 rows, clean parity port after the fix (keys 130–144). Scale/RootNote/
  Divisor are plain-int base writes (defect closes at U6b override table).
- **PhaseFlux table** — parity (Octave/Transpose/SlideTime/Divisor/ClockMult/Scale/RootNote) + LAUNCH
  nudges (WarpNudge/ResponseNudge/LenNudge/CyclePhaseWarp/PulseNudge, keys 150–154) + Phase (shared
  key 60, sequence-level binding vs Curve's track-level). **A/B inlets DEFERRED** — no model storage
  or stage-group engine support exists (separate model+engine task, not a table row).

**All per-type tables now staged on the registry: Global, Note, Curve, Indexed, DiscreteMap, MidiCv,
Tuesday, Stochastic, PhaseFlux.** Every one parity-tested (or direct-tested for launch/fix-forward),
sim + STM32 release clean, nothing wired live.

## Open items carried to later units
- **PhaseFlux A/B inlets** — DEFERRED (owner, 2026-06-02): needs inlet storage + stage-group engine
  consumption first (own task), not a table row.
- ~~Tuesday Scale/RootNote~~ — RESOLVED (owner, 2026-06-02): wired as base writes (were dispatched
  no-ops); Tuesday now on par. Defect closes at U6b like the other tracks.
- **Re-anchor target — RESOLVED as Align/lifecycle**, subspec
  `docs/plans/2026-06-02-004-reset-vocabulary-and-sync-subspec.md`. Frame: **restart = pointer/
  time-base alignment (the routable gesture); reset = lifecycle (play/stop/load) — not a routing
  target.** Routing only ever triggers `restart` (an **Align** target binding `restart()`); External
  sync folds into Align; the Indexed→DiscreteMapSync borrow dies.
  - **NOW done (2026-06-02):** dropped the DiscreteMap **Sync** inlet row (Input/Scanner stay);
    retired `ParamKey` 104 (never reuse); subspec rewritten to align/lifecycle. Model `_routedSync`
    left in place for the deferred engine task. Sim + STM32 clean.
  - **Align target apply-binding** (→ each engine `restart()`) wired at U7 cutover with the other
    target bindings; not built yet.
- **DEFERRED engine task — what reset/restart mean per track** (separate, pairs with
  `wallclock-time-architecture`): trim each track's `restart()` to minimal pointer/time-base
  (fork's drifted heavy); repoint `resetMeasure` from `reset()`→`restart()` (inherited upstream
  over-reach); Tuesday `restart()`=`reset()` → real restart; Stochastic RNG re-seed decision; define
  `reset()` canonically as lifecycle; retire `SyncMode::External` + `_routedSync` once Align covers it.
- **Scale/RootNote/Divisor base-write** (Stochastic, PhaseFlux, Indexed, DiscreteMap) — closes
  structurally at U6b (override table = transient delta), not per-hook.
- **uint8_t paramKey width** — revisit at U7 with full registry count.

## Engine units started (2026-06-03)

- **`RouteApply.h` — value pipeline (U5 + U6b math), pure unit landed.** `scaled` (scaleSource
  centered-scale, replacing the VcaNext `routeIndex+1` hack, R6) + `toU` (Modulate centered vs
  Absolute raw) + `delta` ((d/100)·u·range). Structural contracts tested (Modulate neutral-at-center,
  full swing, Absolute sweep, scale closes VCA). Header-only, included only by the test — firmware
  untouched. `TestRouteApply`.

## Sliced cutover plan — Codex-validated (2026-06-03)

`docs/plans/2026-06-03-005-sliced-cutover-plan.md` (v3, three adversarial-review rounds → SAFE).
**Apply-fork shadow slice:** no storage/format change; in `updateSinks()` the apply forks per
`(trackMode, paramKey)` — Note/PhaseFlux per-track params take the new path (compute
`RouteApply::delta` → write a transient override table, skip old `writeTarget` for that
route+track), everything else stays on live `writeTarget`. Override table cleared/rebuilt each pass
(presence=routed). Migrated getters read `clamp(base+override)`, drop `isRouted`, edit-gate on
presence. No new `Route` members (combine/d derived inline, default Modulate; range inferred
`min<0`→bipolar). Shapers None/TriangleFold (bias-free). **Fixes PhaseFlux base-write in-slice.**
Caveat: Modulate default = forward-model audition, not like-for-like vs today's absolute routes.
Riskiest step = getter migration (stale-clear, mixed-mask, base non-mutation, zero-delta presence).

## Next action

Build the slice in order (see plan 005):
1. **Override table** + `routeOverride()` accessor (mirror `isRouted` reach) + clear/rebuild in
   `updateSinks()` + `clamp(base+delta)` read helper.
2. Shaper stage (None/TriangleFold, bias-free) feeding `RouteApply`.
3. Apply fork in `updateSinks()` (per-trackMode predicate; skip `writeTarget` for migrated).
4. Migrate Note + PhaseFlux getters → override-read + edit-gate on presence.
5. Hardware audition → 6. UI (persist combine/d/scaleSource) → 7. expand → 8. real U7 → 9. U9.

(`RouteApply.h` value pipeline already landed.)
- **Override table** (U6b storage) — model-owned transient `(track, paramKey)→delta`, not serialized;
  read = `clamp(base+delta)`. Closes the Scale/RootNote/Divisor base-write defects by construction.
- **Shaper stage** — `shape(shaper, s, state)→h` feeding RouteApply; classify center-preserving
  shapers (Modulate allows None/TriangleFold; deny Crease/stateful/VcaNext per R15).
- **Slot model** — `(source, scope, paramKey, combine, d[8], shaper[8], scaleSource)`.
- Then **U6 group scope**, **U7 re-address + engine cutover** (the single flip; deletes old dispatch
  + VcaNext neighbor read + per-pattern `Routable` routed half).

## Slices 1-4 landed — apply-fork cutover live for Note + PhaseFlux (2026-06-03)

Plan 005 build order, TDD + Codex-gated each slice. Sim + STM32 release green throughout.

- **Slice 1 — override table.** `Routing::clearRouteOverrides / writeRouteOverride /
  routeOverride / routedValue` backed by a file-scope sparse `RouteOverrideEntry[CONFIG_ROUTE_COUNT
  * CONFIG_TRACK_COUNT]` (~1KB .bss; chosen over a dense [track][key] table ~8KB for the tight
  SRAM budget). Linear scan, last-write-wins, presence=routed (a zero-delta entry still reads
  present). `clearRouteOverrides()` wired at the top of `updateSinks()` (rebuilt every recompute).
  `routedValue = clamp(base+delta)`. `TestRouteOverride` (8 cases).
- **Slice 2 — bias-free shaper stage.** `model/RouteShaper.h` `shape(Shaper, h)` — None +
  TriangleFold (center locked at 0.5, exact match to live `applyTriangleFold` at bias=0); the other
  seven shapers fall through to identity this slice. Header-only. `TestRouteShaper` (6 cases).
- **Slice 3 — apply fork.** `model/RouteFork.h`: `targetToParamKey` (legacy Target -> shared
  ParamKey), `inferRange` (min<0 -> half-span, else full span), `migrated(mode,target,&key,&range)`
  gating on the Note/PhaseFlux param tables. In `updateSinks()` per-track loop, a migrated
  Note/PhaseFlux target computes `RouteShaper::shape` -> `RouteApply::delta` (Modulate,
  d=depthPct, scaleSource=None, range from the table) -> `writeRouteOverride`, then `continue`
  (skips old `writeTarget`). Everything else (Reset/CvOutRot/Run/global/bus/non-migrated modes)
  stays on live `writeTarget`. `_project` member added to RoutingEngine. `TestRouteFork` (8 cases).
- **Slice 4 — getter migration (the cutover).** `Routing::routeOverridden` + `routedValueInt`
  companions. 22 getters -> `clamp(base+override)`: NoteSequence (7), NoteTrack (8),
  PhaseFluxSequence (4, incl. `selectedScale/RootNote` raw-field reads rerouted through the getter),
  PhaseFluxTrack (3). Clamp range = the param routing range (matches `inferRange`); base passes
  through unclamped when not overridden, so Scale/RootNote -1 Default survives. Edit-gating swapped
  `isRouted`->`routeOverridden` on every interactive knob path: editX methods, PhaseFluxTrackListModel,
  NoteSequenceListModel::setIndexedValue (QuickEdit), LaunchpadController First/Last/RunMode.
  `TestRouteGetterMigration` (9 cases). **PhaseFlux Scale/RootNote base-write defect fixed for free**
  (fork skips the base-mutating writeTarget; getter reads base+override).

### Gate boundary (Codex-agreed)

Interactive single-knob UI edits gate on override presence; **programmatic / bulk / structural
writes intentionally write base** (Python bindings, `duplicateSteps`, clipboard, init, generator,
routing-apply hooks). Under the base-anchored model writing base shifts the modulation center, not
corruption. No interactive knob path on a migrated param remains ungated.

### UI status — no new editor yet (slice 6)

Routes are still configured through the existing `RouteListModel` (Target/Tracks/Min/Max/Bias/
Depth/Shaper/Source). For a migrated Note/PhaseFlux target: **Depth** = the live `d` knob;
**Shaper** = None/TriangleFold honored (others act as None); **Min/Max window and Bias are inert**
(still render + accept edits, but do nothing — the new path uses the full param range, bias-free).
combine (Modulate) / scaleSource (None) are not exposed. Slice 6 replaces the dead fields with real
persisted combine/d/scaleSource controls.

### Audition caveat (slice 5)

Modulate default = forward-model: bipolar around base. A migrated route sounds DIFFERENT from the
same route today (which wrote an absolute `min + shaper*span`). Not a regression — the new model.

### Carry-forwards

1. Unported shapers (Crease/Location/Envelope/FreqFollower/Activity/ProgDiv/VcaNext) shape as
   identity in the new path — accept, or gate those routes back to old `writeTarget`.
2. Divisor override not divisor-quantized (non-musical divisions possible under modulation) — defer
   to a quantize stage.
3. Snapshot "unaffected on output" not unit-tested (engine harness disabled); structurally holds
   (engine reads the active sequence only).
4. `isRouted`/`setRouted` still maintained for UI markers + old-path targets.
5. Pre-existing failures on clean HEAD (NOT this work): TestDiscreteMapSequence,
   TestStochasticDurationDictionary, TestHarmonyInversionIssue, TestCurveTrackLfoShapes(+Integration).
