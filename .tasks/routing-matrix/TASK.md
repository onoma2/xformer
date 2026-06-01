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

## Next action

Remaining U4 per-type tables: Stochastic (defect-fixes: Feel dead slot, Scale/RootNote base-write),
PhaseFlux (A/B inlets + five nudges + Phase, defect-fix Scale/RootNote), Tuesday, MidiCv. Then U5
scaleSource, U6 groups, U6b override+combine (range-class lands here), U7 cutover.
