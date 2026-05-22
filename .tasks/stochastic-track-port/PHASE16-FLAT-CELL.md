# Phase 16 — Flat cell model + Feel + optimizations

_Drafted 2026-05-23. Status: design, no code yet. Replaces the Patch C step 3 / mutation-log / Repeat-bake follow-on from Phase 14B (those identity items roll into Patch P7 here)._

## Core idea

> Stochastic rhythm generates timed cells on the tick fabric.

Not:

> Slot creates parent plus child hits.

One uniform cell concept. Burst is a duration-bias on the picker, not a parent/child relationship. Mask, mutate, rank, lock all operate on cells uniformly. Polyrhythm and odd-time come free from variable durations.

## One new knob: Feel

`Routable<uint8_t>` 0..100. Default **50** (center, in detent → no effect, identical to today). Locks the cycle to a target beat count; off in the detent zone.

```
naturalSum = sum of picked cell durations (LUT picks + cluster denom math)

if feel in [45..55]:
    // Detent: Feel off. Use natural sum, cycle length floats with picks.

elif feel < 45:
    targetBeats = 4 - (45 - feel) / 45.0      // knob 0 → 3 beats
    scale = (targetBeats × CONFIG_PPQN) / naturalSum
    for each cell: cell.duration = picked × scale
    recompute relTicks from scaled durations

elif feel > 55:
    targetBeats = 4 + (feel - 55) / 45.0      // knob 100 → 5 beats
    scale = (targetBeats × CONFIG_PPQN) / naturalSum
    for each cell: cell.duration = picked × scale
    recompute relTicks from scaled durations
```

- Feel = 0: cycle = 3 beats (3/4 feel against master clock).
- Feel = 45..55: detent zone — Feel inactive. Cycle length floats with LUT picks. Default 50 sits here = identical to today's behavior.
- Feel = 100: cycle = 5 beats (5/4 feel).

Detent gives the user a safe "Feel off" zone in the middle of the knob range. Transitions outside detent (knob = 44 or 56) engage the metric lock. There IS a discontinuity at detent edges — at default knobs (natural cycle ≈ 4 beats) it's inaudible; at extreme knob settings the jump is larger but it's the user's deliberate "engage Feel" action.

Semantic shift to acknowledge: when Feel is active, `NoteDuration` becomes a *relative weight* (cell-shape bias) rather than an absolute length. `Variation` still drives shape spread. `Size` still controls cell count per phrase. Cycle length is what Feel locks; internal shape is what NoteDuration + Variation determine.

Routing target: reuse `Routing::Target::StochasticReserved` (ID 67, currently unused) renamed to `StochasticFeel`.

## Rhythm laws

1. **Cells are flat.** One uniform cell type. Cache stores per cell: `relTick + gateLen + degree + octave + flags (slide, legato, audible)`. **Duration is implicit** — `dur(N) = relTick(N+1) - relTick(N)`. No parent reference, no separate child cells.
2. **Generator places cells sequentially.** Each cell consumes a picked duration; the generator accumulates ticks and writes the next cell's relTick. Cell durations are arbitrary tick counts, not constrained to any fixed LUT.
3. **Duration is the rhythm knob.**
   - Ordinary cells: pick from the duration LUT (×8, ×4, ×3, ×2, ×4/3, ×1, ×2/3, ×1/2) weighted by `NoteDuration` (center) and `Variation` (spread).
   - Cluster cells (Burst path): see law 4.
4. **Bursts reinterpret existing knobs as relative-to-context cluster math.** No new knobs.
   - `Burst` (probability) = chance this cell starts a cluster.
   - `BurstCount` = how many consecutive cells the cluster spans.
   - `BurstRate` = **denominator applied to the previous cell's duration**. cluster_cell_duration = `prev_cell_duration / BurstRate_denom`. denom ∈ {2,3,4,5,6} — the same set today's burst spacing uses. Cluster cell durations are arbitrary ticks; they do NOT have to land on a LUT slot, which is what produces genuine polyrhythmic placement (e.g. prev=×1 at 48 ticks, denom=3 → cluster cells at 16 ticks = triplet-of-1/16 spacing, unreachable from the LUT alone).
   - All `BurstCount` cluster cells emit at the same cluster_cell_duration. Generator returns to LUT picking after the cluster ends.
   - `BurstPitch::Parent` = cluster cells inherit the cluster-starting cell's degree+octave.
   - `BurstPitch::Generate` = each cluster cell rolls its own pitch via the keyed-RNG → generateDegree path.
   - **Cluster cells advance the playhead.** They consume slot budget. Cycle's audible event count = N cells, including cluster cells.
5. **`gateLen` is fraction-of-cell-duration.** 6-bit field stores `(actualGateTicks * 64) / cellDur`. Engine decodes at trigger: `gateTicks = (cell.gateLen * cellDur) / 64`. Uniform encoding for ordinary cells AND cluster cells — same shape as the existing fix for burst-child gates. No special clamps, no special cases.
6. **Cycle wrap is structural.** Cycle ends when generator emits the cell at `last`, regardless of musical time. Phrase identity = N events; musical length follows from sum of durations + Feel scaling.
7. **Mask / Tilt / Mutate / Repeat all per-cell.** Cluster cells participate in ranking, can be muted, mutated, repeated. Today's "mute parent → children silent" goes away — under flat, cluster cells are independently maskable. Accept the trade.
8. **Rest = silent cell.** Cell exists, has its full duration, gate stays off. Cycle length unchanged. Cell still counts toward slot budget.

## Cache + RAM footprint

Cell layout under flat (32 bits exact):
- 12 bits `relTick` (when this cell starts, 0..4095 ticks from cycle start)
- 7 bits `degree` (0..127)
- 4 bits `octave` (0..15 — narrowed from 5 bits to make room for audible)
- 6 bits `gateLen` (fraction-of-cell-duration in 64ths)
- 1 bit `slide`
- 1 bit `legato`
- 1 bit `audible` (inverse of rest)

**Duration is implicit.** `cellDur(N) = relTick(N+1) - relTick(N)`. Engine derives duration at trigger. Cluster cells with non-LUT durations (arbitrary tick counts from `prev_dur / BurstRate_denom`) cost nothing extra to store — relTicks just land where they land.

CellAux: 1 byte holding `rank` only. No `burstChild` flag (no children exist).

`parentCacheIdx[]` (64 B in today's cache): dropped. Under flat, slot-to-cell-index is `K - first` if K is in window, else 0xff. Compute inline.

Per-cell cost: 4 (cell) + 1 (aux) = **5 B**.

| cap | per-track | Δ from today's CCMRAM |
|---|---|---|
| 64  | 320 B + ~3 B housekeeping | −1,008 B |
| **80**  | **400 B + housekeeping** | **−368 B** |
| 96  | 480 B + housekeeping | +272 B (Stoch becomes Container max) |
| 128 | 640 B + housekeeping | +1,552 B |

**Land at cap = 80.** 25% density headroom over today's effective max (which was capped at 64 by the existing cell array). Slightly cheaper than today's CCMRAM footprint.

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
Rewrite `generateRhythm` as interval walk. Generator state during the walk:
- `prevDur`: ticks of the most recently emitted cell (or first cell's LUT pick for the start of cycle).
- `clusterRemaining`: 0 = normal; > 0 = inside a cluster.
- `clusterDur`: cluster_cell_duration set when cluster starts, = `prevDur / BurstRate_denom`. Used for every cluster cell.

Per cell:
1. If `clusterRemaining > 0`: emit cell with duration = `clusterDur`. Decrement.
2. Else: roll cluster start (`Bernoulli(Burst)`). If hit: set `clusterRemaining = BurstCount`, `clusterDur = prevDur / pickBurstRateDenom()`. Emit one cluster cell.
3. Else: pick duration from LUT (NoteDuration + Variation kernel). Emit ordinary cell.
4. Update `prevDur` to this cell's duration.
5. Roll rest, slide, legato. Pick degree/octave (per current generateDegree logic).

After the walk: apply Feel scaling pass per the "One new knob: Feel" section above. If knob is in detent [45..55], leave durations as-picked. Else compute `scale = targetBeats × CONFIG_PPQN / naturalSum` (targetBeats derived from knob position) and multiply each cell's duration by scale. Recompute relTicks from the result.

Cache holds cells uniformly. Engine `triggerStep` simplifies — drop `evaluateChildren` and `EvaluatedChild`, drop burst-walking loop, drop the parent/child path entirely.

Tests updated for flat layout: add a denom-based duration check (BurstRate=3 with prev=48 → cluster cells at 16 ticks each, regardless of LUT slot values). Add a cluster-eats-budget test (Size=16 with Burst=100, BurstCount=4 → cycle covers fewer beats than Size=16 without bursts).

Default `_size` re-defaulted (probably 32) to preserve perceived density across the slot-budget shift.

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
- "Duration LUT slot" stored on a cell — replaced by implicit duration from relTick deltas. Cluster cells take arbitrary tick counts derived from `prev_dur / BurstRate_denom`.
- Parent/child concept everywhere — code, comments, UI.

## What survives

- `kCellCap` (resized to 80).
- 4 B `CachedCell` shape (repacked — fields reshuffled).
- The duration LUT itself: still used by NoteDuration + Variation to pick *ordinary* cell durations. Cluster cells bypass it via the BurstRate denom path.
- The BurstRate denom set {2, 3, 4, 5, 6}: same as today's burst spacing denominators. Knob behavior identical to the user's ear; the math just runs at generation time per cluster instead of at trigger time per child.
- Mask filter math (rank-based).
- Tilt deterministic salt.
- Cache lifecycle (`refreshCache`, `syncWindowEdit`).
- Direct-history walker.
- Ticket dictionaries, scale, jump, sleep.
- All existing knobs (with reinterpreted semantics for Burst — see Rhythm laws).

## Acknowledged feel changes

- **Density at default Size=16 drops 4×** (today's max = 16 parents × ~4 children, flat = 16 cells). Mitigate: re-default `_size` to 32 in P5.
- **NoteDuration becomes relative weight at Feel > 0.**
- **`Burst` no longer multiplies density** — biases duration only.
- **Repeat decisions become part of stored cycle identity** (baked, not per-trigger random).
- **Burst-children-as-decoration → cluster-cells-as-events.** Mute one, the others continue.

## Locked decisions (2026-05-23)

- **Queue overflow**: drop oldest + debug log.
- **BurstPitch fate**: keep both modes, rename UI to **Hold** (cluster cells share prior pitch — ratcheting) and **Roll** (cluster cells each pick own pitch). Demote toggle to context menu. Model field stays.
- **Repeat baked**: content + duration (full cell copy; downstream relTicks shift).
- **Mask + cluster**: strict per-cell.
- **Feel**: detent [45..55] = off; endpoints 0 → 3 beats, 100 → 5 beats; default 50.
- **Default `_size` after P5**: 32 (doubles default density to compensate for cluster-cells-eat-budget shift).
- **LIVE page slot 7** = Feel (replaces BurstPitch).
- **Routing target**: reuse `StochasticReserved` (ID 67) renamed `StochasticFeel`.

## Open items (post-implementation)

- Hardware feedback on Mask + cluster cohesion behavior — revisit only if "cluster started but stopped mid-way" sounds glitchy in practice.
- Whether the detent discontinuity is perceptually acceptable across extreme NoteDuration settings.

## Out of scope

- Teletype extraction from `Engine::TrackEngineContainer` (would drop container ceiling, free CCMRAM, but separate task per PROJECT.md "Engine gate").
- Phase 15 pitch-math review (still queued, depends on Phase 16 landing).
- Project file format break (per AGENTS.md, dev-stage disposable).

## Next step

Land P1 first (independent prep, low risk, may surface bugs). P2-P3 next (footprint prep). Then P4-P8 in sequence. No mid-patch direction changes; if a patch fails its test gate, revert and re-scope before moving on.
