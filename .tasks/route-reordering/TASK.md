# route-reordering

## Goal
Rearrange the `Routing::Target` enum into a sensible signal-flow ordering. `targetSerialize()` already decouples serialization from enum values, so no version bump or migration is needed. Only internal array indices (`targetInfos[]`, `routedSet[]`) and sentinel range checks need updating to match the new numeric values.

## Status
Paused. Ordering agreed; no code started.

## Key files
- `src/apps/sequencer/model/Routing.h` — `Target` enum definition (11 groups, 62 targets + sentinels), `targetSerialize()`, `targetName()`, `isXxxTarget()` range checks.
- `src/apps/sequencer/model/Routing.cpp` — `targetInfos[int(Last)]` designated-initializer array, `routedSet[size_t(Last)]`, `Route::read()`/`Route::write()`.
- `src/apps/sequencer/model/ProjectVersion.h` — not touched (no version bump).

## Current target order (by group)
```
None(0)
Engine:       Play(1), PlayToggle(2), Record(3), RecordToggle(4), TapTempo(5)
Project:      Tempo(6), Swing(7), CvRouteScan(8), CvRouteRoute(9)
PlayState:    Mute(10), Fill(11), FillAmount(12), Pattern(13)
Track:        Run(14), Reset(15), SlideTime(16), Octave(17), Transpose(18), Offset(19),
              Rotate(20), GateP.Bias(21), RetrigP.Bias(22), LengthBias(23),
              NoteP.Bias(24), ShapeP.Bias(25), CvOutputRot(26), GateOutputRot(27)
Sequence:     FirstStep(28), LastStep(29), RunMode(30), Divisor(31), ClockMult(32),
              Scale(33), RootNote(34)
Tuesday:      Algorithm(35), Flow(36), Ornament(37), Power(38), Glide(39), Trill(40),
              StepTrill(41), GateOffset(42), GateLength(43)
Chaos:        ChaosAmount(44), ChaosRate(45), ChaosParam1(46), ChaosParam2(47)
Wavefolder:   Fold(48), FoldGain(49), DJFilter(50), CurveRate(51)
DiscreteMap:  DMapInput(52), DMapScanner(53), DMapSync(54),
              DMapRangeHigh(55), DMapRangeLow(56)
Indexed:      IndexedA(57), IndexedB(58)
Bus:          BusCv1(59), BusCv2(60), BusCv3(61), BusCv4(62)
```

## Decisions log
- 2026-05-16: No version bump. `targetSerialize()` already decouples serialized byte from enum numeric value. The renumbering is purely internal.
- 2026-05-16: No display-order array. The enum IS the ordering. One source of truth.
- 2026-05-16: GateOffset/GateLength move from Tuesday group to Gate/Timing section.
- 2026-05-16: Mute/Fill/FillAmount/Pattern merge with transport section.
- 2026-05-16: CvRouteScan/CvRouteRoute move from Project group to end (infrastructure).

## What changes
- `Routing.h`: rearrange `Target` enum constants with new groupings, new numeric values.
- `Routing.h`: update `First`/`Last` sentinel values.
- `Routing.h`: update `isXxxTarget()` range checks if groups no longer use consecutive ranges.
- `Routing.cpp`: update `targetInfos[]` designated-initializer indices.
- `Routing.cpp`: update `routedSet[size_t(Target::Last)]` (size unchanged).
- `Routing.cpp`: update `targetName()` switch statement order to match.

## What does NOT change
- `targetSerialize()` — frozen byte mapping, maps by constant name.
- `ProjectVersion` — no bump.
- `Route::read()` / `Route::write()` — serialization is name-mapped, value-independent.
- Python bindings — map by constant name, unaffected.
- `RoutableListModel` subclasses — map by constant name, unaffected.

## Open questions
None. Ordering is agreed; only execution remains.

## Completed steps
- [x] Research: `targetSerialize()` decouples serialization from enum position.
- [x] Research: no `for` loop iterates Target values sequentially.
- [x] Research: internal arrays (`targetInfos`, `routedSet`) use `int(target)` directly and must track.
- [x] Decision: no version bump, no migration, no display-order array.
- [x] Decision: agreed new grouping order.

## Notes
- See `reorder-spec.md` for the full target list in its new order.
- This task is purely about the enum definition and its internal consumers. No user-facing behavior changes.
