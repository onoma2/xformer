# Phase 6 — Curve engine read-side migration

> **For Claude:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development (same-session, fresh
> subagent per task, spec-then-quality review). Implements spec 013 §17 for the Curve engine.

**Goal:** Light up Curve modulation end-to-end — the override path already *writes* Curve overrides
(once `migrated()` knows Curve); this migrates the *read* side so the engine actually consumes them.

**Architecture:** Slice-4 pattern, three parts: (1) add Curve to `RouteFork::migrated`; (2) migrate
every routed Curve getter from the legacy `_x.get(isRouted(Target::X))` dual-slot read to
`Routing::routedValueInt(ParamKey::X, _trackIndex, base, lo, hi)`; (3) swap interactive edit-gating
`isRouted(Target::X)` → `routeOverridden(ParamKey::X)` so editing under a route writes base (no lurch).
No table/key changes — `applyRouted` fn pointers are already dead in the override path.

**Invariants:**
- Getter `lo`/`hi` MUST equal the matching `CurveParamTable` row range (override delta is computed
  in that domain). Verify each field's clamp range against the table row; reconcile in the test.
- `base` = `_x.base` for `Routable` fields (matches Note/PhaseFlux).
- Gate via STM32 release build (`make -C build/stm32/release sequencer`) + sim (`build/sim/debug`)
  clean (no new warnings) + Codex ALLOW per task. Owner flashes once after phase 7 — do NOT flash.

**Tech stack:** C++14, header getters, `Routing::routedValueInt`/`routeOverridden`, UnitTest harness.

---

## Curve routed fields (from master `writeRouted` + CurveParamTable)

**CurveTrack** (track-level): `slideTime` {0,100}, `offset` {-500,500}, `rotate` {-64,64},
`shapeProbabilityBias` {-Range,Range}, `gateProbabilityBias` {-Range,Range}, `curveRate` (float,
table centi {0,400}).

**CurveSequence** (sequence-level): `divisor` {1,768}, `clockMultiplier` {50,150}, `runMode` {0,5},
`firstStep` {0,63}, `lastStep` {0,63}, `wavefolderFold`, `wavefolderGain`, `djFilter`,
`chaosAmount`, `chaosRate`, `chaosParam1`, `chaosParam2` (ranges per CurveParamTable rows).

**Deliberately NOT migrated this pass:** `Phase` (Curve `globalPhase`) — plain field, no `isRouted`
gate; matches PhaseFlux `globalPhase` (also un-migrated). Latent: its table row makes the UI offer
a no-op modulation. Leave for a paired Phase-wiring decision (sibling: PhaseFlux globalPhase).

---

### Task 1: `RouteFork::migrated` — add Curve case

**Files:** Modify `src/apps/sequencer/model/RouteFork.h`; Test `src/tests/unit/sequencer/TestRouteFork.cpp`

**Step 1 — failing test:** Curve mode + a Curve target (e.g. `Target::Divisor`, and a track-level
`Target::CurveRate`) returns `migrated() == true` with the table's range; a non-Curve target
(`Target::IndexedA`) returns false.

**Step 2:** Run, expect FAIL (default branch returns false for Curve).

**Step 3 — implement:** add `case Track::TrackMode::Curve: table = &CurveParamTable::table(); break;`
to the switch in `migrated()`. Include `ParamTableCurve.h`.

**Step 4:** Run, expect PASS. Run the full routing test set + STM32/sim build.

**Step 5:** Commit `feat(routing): RouteFork::migrated lights Curve (phase 6)`.

---

### Task 2: CurveTrack int getters + edit-gates

**Files:** Modify `src/apps/sequencer/model/CurveTrack.h`; Test `src/tests/unit/sequencer/TestRouteGetterMigration.cpp` (extend)

For `slideTime`, `offset`, `rotate`, `shapeProbabilityBias`, `gateProbabilityBias`:

**Step 1 — failing test:** with a committed Curve route on the field (depth → override present),
the getter returns `clamp(base+override)`; with no route it returns base; editing under a route
moves base (assert `routeOverridden` gates the edit, not `isRouted`).

**Step 3 — implement:** getter → `Routing::routedValueInt(ParamKey::X, _trackIndex, _x.base, lo, hi)`
with `lo`/`hi` = the CurveParamTable row range; edit-gate `if (!isRouted(...))` →
`if (!routeOverridden(ParamKey::X, _trackIndex))`, editing from `_x.base`.

**Step 5:** Commit `feat(routing): migrate CurveTrack int getters to override path`.

---

### Task 3: CurveTrack `curveRate` (float / centi domain)

**Files:** Modify `src/apps/sequencer/model/CurveTrack.h`; Test extend.

**Step 1 — failing test:** with a Curve `CurveRate` route, `curveRate()` returns the override in
0..4 (read in centi {0,400} then `/100`); edit under route gated by `routeOverridden`.

**Step 3 — implement:**
`float curveRate() const { return Routing::routedValueInt(ParamKey::CurveRate, _trackIndex, int(std::lround(_curveRate.base*100.f)), 0, 400) / 100.f; }`
edit-gate → `routeOverridden(ParamKey::CurveRate, _trackIndex)`. Table range stays {0,400}.

**Step 5:** Commit `feat(routing): migrate CurveTrack curveRate (centi domain) to override path`.

---

### Task 4: CurveSequence getters + edit-gates

**Files:** Modify `src/apps/sequencer/model/CurveSequence.h`; Test extend.

For `divisor`, `clockMultiplier`, `runMode`, `firstStep`, `lastStep`, `wavefolderFold`,
`wavefolderGain`, `djFilter`, `chaosAmount`, `chaosRate`, `chaosParam1`, `chaosParam2`: same as
Task 2 (getter → `routedValueInt` with table-row range; edit-gate → `routeOverridden`). `runMode`
casts through `Types::RunMode`. Verify each field clamp == table row range; reconcile in test.

**Step 5:** Commit `feat(routing): migrate CurveSequence getters to override path`.

---

### Task 5: Build gate + final review

Run STM32 release + sim builds (zero new warnings), full routing test set green, dispatch final
code reviewer over the Curve diff. Update `.tasks/routing-matrix/SPEC-013-TODO.md` (phase 6 Curve
done) + STATUS.md routing block.
