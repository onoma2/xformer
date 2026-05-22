# Phase 14 — Trunk Model

_Drafted 2026-05-22. Status: **considered alternative**, not chosen. The seed+log model in [PHASE14B-SEED-LOG.md](PHASE14B-SEED-LOG.md) is the chosen direction. This doc is retained for reference — its analysis of cell shape, playback simplification, and Lock-collapses-to-1-bit is useful background — but the trunk shape is not what we are building._

## Why this exists

Today's loop tape stores **upstream decisions** — `durationIndex` (LUT slot), `rest`, `slide`, `legato`, `accent`, `childCount`, `burstRate-LUT-slot`, `densityRank`. These exist only to drive the generator. Once the generator has produced its output, the data has done its job. Storing it forever — and re-running LUT lookups, mask comparisons, and burst-child evaluation on every trigger — keeps the recipe in the freezer alongside the meal.

It also creates the "what's stored vs what's re-rolled" incoherence we tried to clean up: gate length re-rolls per trigger but `childCount` is stored; child pitches re-roll but the spacing LUT slot is stored. The split is an artifact of byte budgets and code locality, not a musical decision.

Bloom v2 and the broader community consensus (Marbles, Turing, PNW, Melodicer, SIG) treat the loop as the **baked output stream**: pitch + gate length + ratchets + slew per step, all stored, all replayed identically. Ornament in their world means optional classical music flourishes applied on top, not "which decisions happen to re-roll."

Phase 14 reorganizes the stochastic engine around that model: a per-pattern trunk of CV/gate cells, generator code that produces cells from knobs + RNG, and playback that reads cells without consulting LUTs.

## The trunk schema

Per pattern:

```
struct TrunkCell {
    uint16_t relTick;    // when gate-on fires, relative to cycle start
    int16_t  cv;          // V/oct as int16 / 1024 → ±32V range at ~1mV step
    uint16_t gateLen;     // ticks from gate-on to gate-off
    uint8_t  flags;       // bit0 slide, bit1 legato, bit2 audible (Mask survivor),
                          // bits 3-7 rank (0..31) for Mask/Tilt filter
};                        // 7 B, padded to 8
```

Tight-pack option (fractal-style): `uint16_t` per cell, 11-bit CV + 4-bit gate length encoded as a fraction of the parent-step duration + 1-bit slide. That gives 2 B per cell but loses absolute tick precision and the rank/legato encoding. **Probably 6-8 B per cell is the right pragmatic choice** — the upper bound is set by what fits in CCMRAM, not by fractal's specific bit budget.

Per pattern, declare a fixed cell array:

```
static constexpr int kMaxTrunkCells = 96;   // 64 parents × 1.5 cells avg (light burst)
TrunkCell _trunk[kMaxTrunkCells];
uint8_t   _trunkCount;                       // active cells, 0..kMaxTrunkCells
uint16_t  _trunkCycleTicks;                  // cycle length in ticks
```

8 B × 96 = **768 B per pattern**. 16 patterns × 8 tracks = **96 KB**. Doesn't fit.

Realistic constraint: tape lives in `.ccmram_bss` (currently ~10 KB free). Two options:

- **One trunk per track**, not per pattern. Active-pattern's trunk only lives in RAM; other patterns persist their trunk in the project file and re-hydrate on pattern switch. 768 B × 8 tracks = 6 KB. Fits.
- **Bit-tighten the cell** to 4 B (16-bit relTick, 12-bit CV, 8-bit gateLen, 4 bits flags packed into the high nibble of CV). 4 × 96 × 16 × 8 = 49 KB. Doesn't fit in CCMRAM.

Per-track is the cleaner choice. Matches fractal's "trunk is per-track, shared across patterns" model — except in our case the patterns *do* have distinct content, so the trunk's contents reload on pattern switch (one-cycle re-hydrate delay or a serialized blob in the project file).

This is identical to the question the deferred Lock design hit. Worth resolving once for both.

## Playback contract

`triggerStep` becomes a tape pumper:

```
tick(currentTick):
    while _trunkReadIdx < _trunkCount:
        cell = _trunk[_trunkReadIdx]
        absTick = _cycleStartTick + (_cycleIteration × _trunkCycleTicks) + cell.relTick
        if currentTick < absTick: break
        if cell.audible:
            push {absTick, cell.cv, cell.slide} → _cvQueue
            push {absTick, gateOn=true}         → _gateQueue
            if !cell.legato:
                push {absTick + cell.gateLen, gateOn=false} → _gateQueue
        _trunkReadIdx++

    if _trunkReadIdx == _trunkCount:
        _trunkReadIdx = 0
        _cycleIteration++
        runCycleEndHooks()  // patience, mutate, jump, sleep — possibly regen trunk

    drainGateAndCvQueuesAsToday()
```

That's it. No LUT lookups. No mask filter. No burst child evaluation. No densityRank check. The cell is the truth.

Mask/Tilt filtering happens at trunk **generation** time, not at trigger time. Generator ranks cells and sets the `audible` bit; playback respects it. Knob changes to Mask after generation still take effect (engine reads the knob and compares against each cell's rank as it pumps), but the rank itself is baked.

## Generator contract

```
generateRhythm(trunk[], sequence, rng):
    cellIdx = 0
    cycleTicks = 0
    for step in 0..size-1:
        if cellIdx >= kMaxTrunkCells: break    // hard cap
        stepDurationTicks = computeFromLut(durationIndex pick, divisor)
        if Bernoulli(rest): continue           // no cell emitted, time still advances
        emitParentCell(trunk, cellIdx++, cycleTicks, cv, gateLen, flags)
        if Bernoulli(burst):
            childCount = pickBurstCount(rng)
            childSpacing = pickBurstSpacing(rng)
            for c in 0..childCount-1:
                if cellIdx >= kMaxTrunkCells: break
                emitChildCell(trunk, cellIdx++, cycleTicks + c × childSpacing, …)
        cycleTicks += stepDurationTicks
    _trunkCount = cellIdx
    _trunkCycleTicks = cycleTicks
    generateMaskRanks(trunk, _trunkCount, tilt, seed)
```

```
generateMelody(trunk[], sequence, scale, rootNote, rng):
    for c in 0.._trunkCount-1:
        if cell is rest-equivalent: continue
        trunk[c].cv = pickPitch(sequence, scale, rootNote, lastDegree, rng)
```

Two-pass: rhythm structure determines cells + timing, melody fills CV. Same separation as today.

All the existing LUT logic (`getDurationFraction`, `pickGateLength`, `evaluateChildren`, `pickBurstSpacingFromLut`, `pickBurstCountFromLut`, `pickFromLutTriangular`) stays — but lives **inside** the generator, used at generation time only. Playback never calls them.

## Operations on the trunk

- **NewR (renew rhythm)** → call `generateRhythm(_trunk, …, freshSeed)`. Whole trunk rewritten.
- **NewM (renew melody)** → call `generateMelody(_trunk, …, freshSeed)`. Only CV fields updated; rhythm structure untouched.
- **Patience** → roll Poisson per cycle. On hit, call `generateRhythm` or `generateMelody`. Same semantic as today, just acting on the trunk.
- **Mutate (positive — permute)** → pick two cell indices, swap their CV+gate fields (or just CV for melody-only mutation). Trunk structure preserved.
- **Mutate (negative — regenerate)** → pick one cell index, call generator's per-cell helper to overwrite. Length/timing of the trunk may shift if the regenerated cell changes its duration.
- **Mask** → knob 0..100. Engine reads cell rank at trigger time; if `rank × 100 >= mask × _trunkCount`, skip the cell (don't push to queue).
- **Tilt** → knob -100..+100. Input to the rank assignment during generation. Positive tilt = long-duration cells get low ranks (survive Mask cuts). Negative tilt = short cells survive.
- **Lock** → 1 bit on the sequence (or track, TBD). When true, generator can't write to the trunk. Patience/Mutate/NewR/NewM all silently refuse. Trunk replays as-is. **No tape, no RNG snapshot, no engine-state freeze** — the trunk IS the locked content.
- **Repeat (`_repeatProb`)** → at trigger time, before pushing the cell to the queue, Bernoulli vs `repeatProb`. On hit, re-push the *previous* cell's values instead of the current cell. Cheap, doesn't touch the trunk. Or: bake repeat decisions into the trunk at generation time (so the loop is bit-identical). Decision to make.

## What goes away

- `StochasticSourceEvent` struct (6 B per slot × 64 = 384 B per pattern) → replaced by trunk.
- `events()` accessor → replaced by `trunk()`.
- Live writeback path in `triggerStep` (the const_cast removal saga from earlier patches) — no need to write into a per-slot event struct because the trunk *is* the playback source. Live mode in the new model: every cycle wrap regenerates the trunk from knobs + fresh RNG.
- `densityRank` byte packed into events → replaced by rank bits in the trunk cell.
- `pickGateLength` called from playback → only called from generator.
- `evaluateChildren` called from playback → only called from generator.
- `getDurationFraction` looked up at playback → only at generator time.
- The whole `if (rhythmMode == Live)` writeback dance disappears. Mode is now: Loop = trunk persists between cycles, Live = trunk regenerates every cycle wrap.

`StochasticSourceEvent` deletion frees the dead accent bit and the now-pointless `setRhythmValid/setMelodyValid` per-event flags. The per-pattern `_rhythmValid` / `_melodyValid` flags stay (they gate whether to regenerate at the next cycle).

## What stays

- Sequence-owned knobs (everything in `StochasticSequence` model). Drive the generator.
- Track-owned controls (Divisor, Scale, Octave, Transpose, SlideTime, etc.).
- Engine queues (`_gateQueue`, `_cvQueue`). Same drain path. Same `tick()` shape.
- Patience / Mutate / Jump / Sleep cycle-end hooks. Same Poisson rolls. Different write target (trunk instead of per-event struct).
- Loop/Live rhythm/melody source toggle. Loop = persist between cycles; Live = regen each cycle.
- Repeat probability semantic. Open question whether it stays per-trigger or moves to generator-bake.
- Direct page UI walker — already reads engine output via the BSS history pool. No change.
- LIVE/LOOP/PITCH/DURATION hero pages — they edit knobs, not stored event content. No change.

## What this unlocks

Two musical primitives the current model can't reach. Both fall out of trunk geometry without engine changes.

### Polyrhythm (cross-step)

Current model: `slot[i]` fires at `i * divisor`. Onsets snap to the parent grid. Bursts subdivide *within* a slot but can't reach across slots at a non-grid rate. No 3-against-4, no 5-against-8.

Trunk model: `relTick` is a raw tick offset. Generator can place cells at `(slotIndex * cycleTicks) / N` for any N. Polyrhythm is a generator decision, not an engine feature. Pump plays what's in the trunk — it never knew there was a "grid."

Generator surface: one extra optional pass that re-times the trunk after rhythm generation. Or a per-cell tupletAgainst byte chosen at generation time. The engine doesn't care which.

### Trills, mordents, ornament

Two ways to spell them in the trunk:

**C1 — stored substeps.** A 4-note trill on one parent = 4 trunk cells at adjacent relTicks with alternating CV. Mordent = 2-3 cells. Pump doesn't know it's a trill; it plays the cells. Pitch contour is *stored*, so reps are bit-identical without RNG snapshotting. Cost: cells. 16 parents × 4-note trill = 64 cells, fits inside a 96-cap with room. Worst case (every parent trills + bursts) truncates.

**C2 — ornament flag on the cell.** Cell stays one entry; a flag bit says "expand into trill at playback." Pump synthesizes substeps inline. Costs 1 cell + a few flag bits (count, interval). Pitch contour has to be deterministic from the cell's data alone (seed from `relTick`, no live RNG roll).

**Recommendation: C1.** Stored substeps are the whole point of moving to trunk. C2 puts re-roll logic back in the pump and reintroduces the "stored vs rolled" coherence problem we're killing. Pick C2 only if real-world trunk fill consistently breaks the cap.

## Cell count budget

Per `[first..last]` window of size N:

- Best case (all rests): 0 cells.
- Typical (16 steps, ~30% rest, ~20% burst @ avg 3 children): ~11 parents + 6 burst children = **17 cells**.
- Moderately busy (32 steps, ~20% rest, ~40% burst @ avg 3 children): ~26 parents + 30 children = **56 cells**.
- With ornament (16 steps, ~30% rest, ~25% of parents carry a 4-note trill, no bursts): ~11 parents + 12 ornament cells = **23 cells**. Trills more than double per-parent cost when present but typically apply to <50% of parents.
- Worst case (size=64, no rests, all bursting with 5 children, every parent trilling 4-wide): **>384 cells**. Truncates.

`kMaxTrunkCells = 96` covers typical + moderately busy comfortably. The worst case truncates — generator stops at 96, remaining steps in the cycle play as silence (the `_trunkCycleTicks` length covers the whole `[first..last]` window even if the trunk only fills the first portion).

If 96 feels stingy, 128 cells × 8 B = 1024 B per pattern. Per-track-active-only: 8 KB. Per-pattern-resident: 16 KB. The math is linear; pick a cap that matches the use case the product wants to support.

For comparison, fractal v2's trunk is `_trunk[128]` cells at 2 B = 256 B per channel, 3 channels per module. Our worst case is bigger because we have nested burst expansion they don't.

## Migration path

This is a structural refactor — not a small change. **Land in three patches.**

**Patch A — shadow trunk alongside existing event tape.**
Add `TrunkCell _trunk[]`, `_trunkCount`, `_trunkCycleTicks` to `StochasticSequence`. Add a generator helper `populateTrunkFromEvents()` that walks the existing `events()[]` and synthesizes a trunk from current behavior. Engine still plays from `events()[]`; the trunk is debug-visible but unused. Verify trunk content matches expected output of one cycle of current playback.

**Patch B — switch playback to trunk.**
Reroute `triggerStep` to pump from the trunk. Keep `events()[]` write paths for one release (so patience/mutate keep writing to events; `populateTrunkFromEvents` runs after each write to keep the trunk in sync). All audio output now comes from the trunk; events struct becomes a "shadow" of the trunk for compatibility with mutation logic that still expects it.

**Patch C — flip ownership.**
Move generator output target from `events()[]` to `_trunk[]` directly. Patience, Mutate, NewR, NewM all write trunk cells. Delete `StochasticSourceEvent`, `events()[]`, the rhythm/melody domain validity flags within the event struct (sequence-level flags stay). Engine size shrinks by ~5 KB per pattern.

**Project file format break in Patch C.** Saved projects from before Patch C have the old `events()[]` byte sequence. Either:
- Read the old bytes and run `populateTrunkFromEvents()` to rehydrate, then never write old format again. One-shot migration on load.
- Version-bump the sequence format. Old projects load with `_rhythmValid = _melodyValid = false`, regenerate trunk on first trigger from knobs + RNG. Loses the saved happy-accident content but is simpler.

The first option preserves user content. Pick that.

## Open questions

1. **Per-track or per-pattern trunk?** Per-track: only active pattern's trunk lives in RAM; switching patterns hydrates from project file (1-cycle delay). Per-pattern: all 16 patterns × 8 tracks × 768 B = 96 KB → won't fit. **Per-track is forced by RAM unless we drop pattern count or bit-tighten the cell.**

2. **Cell pack: 8 B or 4 B?** 8 B keeps absolute tick + clean rank + flag bits. 4 B forces compromises (relTick gets less range, rank shares bits with CV). 8 B is what's costed above. 4 B halves the storage.

3. **Trunk cap: 96, 128, or per-track configurable?** 96 covers typical use; busy patterns truncate to silence. 128 is generous; 64 is tight. A configurable max would let users trade off pattern length for trunk depth, but adds UI/UX surface.

4. **Repeat probability in trunk or live?** If baked at generation time (each cell knows "I'm a repeat of the previous cell"), the loop is bit-identical and Lock is trivial. If kept as a per-trigger Bernoulli, Repeat continues to introduce variation within a locked loop — which contradicts the Bloom-shape model we've committed to.

5. **Live mode under trunk?** Today Live writes back per-event mid-audio. Under trunk model, Live becomes "regenerate trunk at every cycle wrap" — wholesale per-cycle reroll instead of per-event. Slightly different feel: in current Live, each event regenerates the moment it plays. In trunk-Live, the whole cycle regenerates at the wrap boundary. Probably more musical, but worth verifying user expectation.

6. **Mask/Tilt rank: per-cell or recomputed?** Per-cell bakes the rank during generation. Cheaper at playback. Knob changes to Tilt require regenerating ranks (cheap function, but a generator call). Recomputed at playback gives knob changes instant audible feedback but costs CPU per trigger. **Bake per cell, regenerate on Tilt change** is the natural fractal-aligned answer.

7. **Burst representation in the trunk?** Children become independent cells, equal status to parents. Loses the explicit "this parent has 3 children" grouping. Does the UI need to know about burst groups for visualization? If yes, add a 1-bit "burst-child" flag on the cell so the loop tape display can color them differently. If no, simpler — cells are cells.

8. **What does the LIVE hero page walker show?** Today it reads the engine's recent-event history pool. Under trunk model, walker can read from the trunk directly (the loop tape) AND/OR the engine pool (what just played). Decide which.

9. **Polyrhythm — exposed how?** Trunk geometry supports any onset spacing for free, but the user needs a way to ask for it. Options: dedicated knob ("tuplet rate, 0/3/5/7-against parent grid"), an algorithm-tagged cycle ("this NewR pass lays out 5-against-4"), or per-step tuplet tag in the generator output. First option is most discoverable, last is most flexible. Worth a UI sketch before deciding.

10. **Ornament — cells (C1) or flag (C2)?** See "What this unlocks" above. C1 is the working recommendation. Re-decide only if profiling shows realistic patterns blowing the cell cap. If C2 wins, pick where ornament params live (steal flag bits, grow the cell to 10 B, or carry a parallel per-cell ornament byte).

## Phasing relative to Lock

**Lock collapses to one bit on the sequence.** When `_lock == true`:
- `generateRhythm` / `generateMelody` early-return without writing.
- Patience invalidation no-ops.
- Mutate/Permute no-op.
- `NewR` / `NewM` toast "Locked" and refuse.

The deferred LOCK-DESIGN-DEFERRED.md becomes a one-paragraph commit in this phase. The flat-tape and engine-flag designs both go away — the trunk IS the lock.

This is the strongest argument for Phase 14: it absorbs Lock as a trivial side-effect of getting the storage model right.

## Risk and timeline

This is a multi-week refactor, not a session-sized change. Patches A → B → C are 1-3 days each on hardware. Total touch area:
- `StochasticSequence.h` / `StochasticTypes.h` — schema rewrite.
- `StochasticGenerator.cpp` — generator output rewrite.
- `StochasticTrackEngine.cpp` — playback rewrite (~30 line `triggerStep`).
- All four hero-page draws (`drawLivePage`, `drawLoopPage`, `drawPitchPage`, `drawDurationPage`) — if they read `events()[]` for the loop tape display, they read `_trunk[]` instead. Mostly mechanical.
- Routing — no impact.
- Tests — `TestStochasticGenerator`, `TestStochasticL1Macros`, `TestStochasticSequenceSerialization`, `TestStochasticDurationDictionary` all touch the event struct or generator output. Each needs review/rewrite.

**Suggested order:** finish current UI cleanup (Mutate readout, source-state chip, PlayMode/FillMode truth pass) → ship a stable Phase 13 release → then Phase 14 as a standalone branch.

## Next step

If this design is accepted, write Patch A (shadow trunk alongside existing) as a no-behavior-change commit to validate the cell shape and budget against real hardware. After hardware confirms the trunk faithfully mirrors current playback, proceed to Patch B.

If the design needs revision before code: discuss in this doc, no implementation yet.
