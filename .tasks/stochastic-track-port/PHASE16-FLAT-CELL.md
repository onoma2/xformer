# Phase 16 — Flat cell model + Feel + optimizations

_Drafted 2026-05-23. Status: design, no code yet. Replaces the Patch C step 3 / mutation-log / Repeat-bake follow-on from Phase 14B (those identity items roll into Patch P7 here)._

## Core idea

> Stochastic rhythm generates timed cells on the tick fabric.

Not:

> Slot creates parent plus child hits.

One uniform cell concept. Burst is a duration-bias on the picker, not a parent/child relationship. Mask, mutate, rank, lock all operate on cells uniformly. Polyrhythm and odd-time come free from variable durations.

## One new knob: Feel

`Routable<uint8_t>` 0..100. Controls how strictly cell durations are normalized to the `resetMeasure` bar boundary.

```
if (feel > 0 && resetMeasure > 0):
    barLen = resetMeasure × measureDivisor
    sumLen = sum of all cell durations
    scale = barLen / sumLen
    for each cell:
        adjustedDur = lerp(dur, dur × scale, feel / 100)
    recompute relTicks from adjustedDurs
else:
    leave durations as-picked (interval walk)
```

- Feel = 0: pure interval walk. Cycle length = sum of natural durations.
- Feel = 100: cycle exactly fills the bar. Odd `Size` + bar = odd-time intrinsic (e.g. Size=5 in 4/4 → quintuplet feel).
- Feel = 50: blend. Cycle drifts halfway toward bar boundary.

Semantic shift to acknowledge: at Feel > 0, NoteDuration becomes a *relative weight*, not an absolute duration. NoteDuration shape (long-vs-short) preserved; absolute lengths fall out of the bar fit.

## Rhythm laws

1. **Cells are flat.** One uniform cell type: `relTick + duration + degree + octave + gateLen + flags`. No parent reference.
2. **Generator places cells sequentially.** Each cell consumes its own duration. `relTick(N) = sum(durations 0..N-1)`. Cumulative.
3. **Duration is the rhythm knob.** `NoteDuration` = center of LUT pick. `Variation` = spread. Same LUT (×8, ×4, ×3, ×2, ×4/3, ×1, ×2/3, ×1/2).
4. **Clusters reinterpret burst knobs.** No new knobs.
   - `Burst` (probability) = chance this cell starts a cluster.
   - `BurstCount` = how many consecutive cells in the cluster.
   - `BurstRate` = which short-LUT slot the cluster cells use.
   - `BurstPitch::Parent` = cluster cells inherit start cell's degree+octave.
   - `BurstPitch::Generate` = each cluster cell rolls its own pitch.
   - Cluster cells advance the playhead. They eat slot budget.
5. **Cycle wrap is structural.** Cycle ends when generator emits cell `last`, regardless of musical time. Phrase identity = N events; musical length follows from durations + Feel.
6. **Mask / Tilt / Mutate / Repeat all per-cell.** No special burst-child rules. Cluster cells participate in ranking, can be muted, mutated. Loses "mute parent → children silent" — accept it.
7. **Rest = silent cell.** Cell exists, has full duration, gate stays off. Cycle length unchanged.

## Cache + RAM footprint

Aggressive packing under flat:
- 4 B `CachedCell`: steal 1 bit from `octave` (5→4 bits) to pack `audible` into the cell.
- 1 B `CellAux`: rank only. Drop `burstChild` flag.
- Drop `parentCacheIdx[]` (64 B): under flat, `K - first` is trivial inline math.

Per-cell cost: 5 B (was 7).

| cap | per-track | Δ from today's CCMRAM |
|---|---|---|
| 64  | 320 B | −1,008 B |
| **80**  | **400 B** | **−368 B** |
| 96  | 480 B | +272 B (Stoch becomes Container max) |
| 128 | 640 B | +1,552 B |

**Land at cap = 80.** 25% density headroom over today's effective max, slightly cheaper than today's CCMRAM footprint.

## Patch sequencing

Each patch builds clean, passes all 6 stochastic suites, passes STM32 release.

### Prep

**P1 — Queue contract audit.**
Document `SortedQueue<Gate,16>` / `SortedQueue<Cv,16>` overflow + same-tick ordering. Add tests for boundary cases. Fix any latent bugs found.

**P2 — Cell packing.**
Steal 1 bit from cell.octave. Pack audible into cell. Drop `CellAux::burstChild`. Drop `parentCacheIdx[]`. Pure footprint reduction; no behavior change.

**P3 — Bump `kCellCap` to 80.**
Single constant after P2 lands. Verify STM32 CCMRAM accounting.

### Feel + flat model

**P4 — Feel knob.**
Add `_feel`, `Routing::Target::StochasticFeel`. Wire to LIVE page (slot TBD at UI pass). Stored, inert.

**P5 — Flat generator (the big one).**
Rewrite `generateRhythm` as interval walk. Burst becomes duration-bias state (`clusterRemaining`, `clusterLutSlot`). Apply Feel scaling after the walk. Cache holds cells uniformly. Engine `triggerStep` simplifies — drop `evaluateChildren`, drop burst-walking loop. Tests updated for flat layout. Default `_size` re-defaulted (probably 32) to preserve perceived density.

### Identity work (Phase 14B's deferred completion)

**P6 — CycleEndPlan.**
Struct: `jumpDelta, sleepCount, rhythmDirty, melodyDirty, mutationRequest, rankDirty`. Cycle-end branch builds plan, applies in fixed order. Removes hidden ordering coupling.

**P7 — Mutation log + Repeat baked + seed-lock semantics.**
- `MutationOp mutationLog[16]` on sequence (~64 B).
- Mutate ops append to log; generator applies after base pass.
- Repeat: 1-bit `isRepeat` on cell, baked at generation time. Kill `selectMaskRank` `useRepeat` parameter. Kill engine's `useRepeat` branch. Kill `_lastEvent` capture.
- Patience seedLock guard: no-op when `rhythmValid && melodyValid && !live`.

**P8 — Delete `events[]`.**
Generator writes cells directly. `StochasticSourceEvent` struct deleted. UI consumers switch to cache cells. Final consolidation.

Per AGENTS.md line 81: **no project version bumps during dev-stage feature work. Dev projects may be disposable.** No migration code, no v1 loader, no format-break ceremony.

## What dies

- `StochasticGenerator::evaluateChildren` / `EvaluatedChild`.
- Cache `burstChild` aux flag, `parentCacheIdx[]` array.
- `selectMaskRank::useRepeat` parameter.
- Engine `useRepeat` / `_lastEvent` capture path.
- `StochasticSourceEvent::childCount` / `burstRate` per-event fields (P5).
- `StochasticSourceEvent` struct entirely (P8).
- Parent/child concept everywhere — code, comments, UI.

## What survives

- `kCellCap` (resized to 80).
- 4 B `CachedCell` shape (repacked).
- Mask filter math (rank-based).
- Tilt deterministic salt.
- Cache lifecycle (`refreshCache`, `syncWindowEdit`).
- Direct-history walker.
- Ticket dictionaries, scale, jump, sleep.
- All existing knobs (with reinterpreted semantics for Burst).

## Acknowledged feel changes

- **Density at default Size=16 drops 4×** (today's max = 16 parents × ~4 children, flat = 16 cells). Mitigate: re-default `_size` to 32 in P5.
- **NoteDuration becomes relative weight at Feel > 0.**
- **`Burst` no longer multiplies density** — biases duration only.
- **Repeat decisions become part of stored cycle identity** (baked, not per-trigger random).
- **Burst-children-as-decoration → cluster-cells-as-events.** Mute one, the others continue.

## Open items

- Which LIVE-page slot for Feel (P4)?
- Default `_size` value after density shift (P5) — 32 or higher?
- Should Mask treat consecutive cluster cells cohesively, or strictly per-cell? Strict-per-cell is the default; revisit if hardware feedback says glitchy.

## Out of scope

- Teletype extraction from `Engine::TrackEngineContainer` (would drop container ceiling, free CCMRAM, but separate task per PROJECT.md "Engine gate").
- Phase 15 pitch-math review (still queued, depends on Phase 16 landing).
- Project file format break (per AGENTS.md, dev-stage disposable).

## Next step

Land P1 first (independent prep, low risk, may surface bugs). P2-P3 next (footprint prep). Then P4-P8 in sequence. No mid-patch direction changes; if a patch fails its test gate, revert and re-scope before moving on.
