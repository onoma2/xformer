# Phase 14B — Seed + Log Model (chosen direction)

_Drafted 2026-05-22. Status: chosen direction; design only, no code yet. Supersedes the trunk model in PHASE14-TRUNK-MODEL.md, which is retained as the considered alternative._

The seed + mutation-log + dictionaries shape (Tuesday-style recipe identity) is the direction. Trunk model (PHASE14-TRUNK-MODEL.md) is kept on disk as the considered alternative — its analysis of storage/playback shape, cell pack tradeoffs, and Lock collapse remain useful reference material — but it is not what we are building.

The acceptance bar below was distilled from an adversarial review of an earlier seed+log draft. Every item must be designed and resolved before any patch lands.

## Why this exists

Tuesday stores ~50 B of knobs + two uint32 seeds and produces consistent loops by reseeding RNG at every loop wrap. Stochastic stores a 64×6 event tape (~400 B per pattern) AND re-rolls part of the data at trigger time anyway. The split between "stored" and "re-rolled" is the source of the coherence problem we've been hitting.

The seed+log shape says: store the recipe (seeds, dictionaries, knobs, a small mutation log), regenerate the output every cycle wrap, never persist the output. Tuesday-shape.

The catch: **identity changes from "stored musical result" to "recipe replay."** If knobs are part of the recipe, knob edits change locked playback. That's the central design problem this proposal has to solve before it can ship.

## Model shape

```
struct StochasticSequence {
    // Track-level (unchanged)
    int8_t   trackIndex, scale, rootNote;
    uint16_t divisor;
    Routable<uint8_t> clockMultiplier;
    uint8_t  resetMeasure;

    // Generation seeds (already present in model today)
    uint32_t rhythmSeed;
    uint32_t melodySeed;

    // Validity (unchanged)
    bool rhythmValid, melodyValid;

    // All knobs & Routables (unchanged — every parameter today)
    // ... ~30 fields ...

    // Dictionaries (unchanged)
    int8_t   degreeTickets[7];
    uint8_t  durationTickets[8];
    uint8_t  first, last, size;

    // NEW — mutation log (16 entries × 4 B = 64 B)
    struct MutationOp {
        uint8_t op;     // SWAP_CELLS | REGEN_CELL | ROTATE | …
        uint8_t a, b;   // cell indices
        uint8_t domain; // 0=rhythm, 1=melody
    };
    MutationOp mutationLog[16];
    uint8_t    mutationLogCount;

    // NO new lock fields. Seed lock is a derived state:
    //   seedLocked = (rhythmValid && melodyValid && !live)
    // i.e. Loop mode with both domains generated. Content lock is deferred.
}
```

Per pattern size: **~115 B** (vs ~400 B today). Big RAM win.

## Engine cache

Output lives in a transient per-track cache, engine-owned, CCMRAM-resident, **never serialized**:

```
struct CachedCell {
    uint16_t relTick;
    int16_t  cv;
    uint16_t gateLen;
    uint8_t  flags;     // slide, legato, audible, burst-child, rank
};
CachedCell _cache[96];
uint8_t    _cacheCount;
uint16_t   _cycleTicks;
```

8 B × 96 × 8 tracks = 6 KB CCMRAM. Same footprint as the trunk model's per-track-active strategy, but here the cache is *always transient* — nothing persists between power cycles.

## Generation contract

```
regenerateRhythm(sequence, cache, rng):
    seedPerCell(cellIdx) = hash(sequence.rhythmSeed, cellIdx)   // keyed RNG
    cellIdx = 0
    cycleTicks = 0
    for step in 0..size-1:
        if cellIdx >= 96: break
        rng.seed(seedPerCell(cellIdx))
        decide rest/duration/burst/gateLen/flags from rng + knobs
        emit cell at cycleTicks
        cycleTicks += stepDuration
    cache._count = cellIdx
    cache._cycleTicks = cycleTicks
    applyMutationLog(cache, sequence.mutationLog, domain=rhythm)
    computeRanks(cache, sequence.tilt)

regenerateMelody(sequence, cache, rng, scale, rootNote):
    for c in 0..cache._count-1:
        if cache[c] is rest: continue
        rng.seed(hash(sequence.melodySeed, c))
        cache[c].cv = pickPitch(sequence, scale, rootNote, lastDegree, rng)
    applyMutationLog(cache, sequence.mutationLog, domain=melody)
```

**Critical:** each cell's RNG is keyed by its index, not consumed sequentially. This is what makes per-cell regen cheap (no stream replay).

The melody-needs-lastDegree dependency is real (`StochasticGenerator.cpp:85`). Two options:

- **Make pitch picking independent per cell** — `lastDegree` becomes an explicit input only when contour rules need it; otherwise each cell's pitch is pure function of `(melodySeed, cellIndex, scale, root, knobs)`. Requires refactoring `pickPitch`.
- **Replay melody pass for [N..end] after REGEN_CELL at N.** Cheap because melody pass is fast; only the regen op pays the cost.

**Per-cell keyed RNG is mandatory.** Without it, REGEN_CELL forces a full stream replay every time the log changes, and mutation cost scales with cell index. This is in the acceptance bar.

## Locking — seed lock now, content lock deferred

Phase 14B ships **one lock concept: seed lock.** Content lock is documented but pushed to future work.

**Seed lock = derived state, no new field.** Decision 2026-05-22:
```
seedLocked = (rhythmValid && melodyValid && !live)
```
This is what today's Loop mode already evaluates to. Reusing the existing flags keeps the model surface stable and makes Phase 14B's lock semantics identical to today's Loop UX.

**While seed-locked:**
- `rhythmSeed`, `melodySeed`, `mutationLog` are frozen. NewR/NewM/Patience refuse.
- Cache regenerates every cycle from frozen recipe + **current knobs**.
- Knob edits, dictionary edits, mask, tilt, rotation: all take effect at the next cycle wrap.

**Behavior shift from today:** Today's Loop mode plays back stored `_events[]`. Knob edits to mask/tilt/gateLength take effect at trigger time, but rest/duration/burst etc. do not — those decisions are baked into the event tape. Under seed+log, Loop mode regenerates every cycle from current knobs, so **all knobs affect locked output at cycle wrap.**

This is the user-visible change. Explicitly accepted per the design decision of 2026-05-22 alongside the seed-lock simplification. Bloom-shape "lock freezes the actual output bytes" is the future content lock feature.

### Content lock — deferred to future work

Content lock means: freeze the engine cache itself, regardless of knob movement. Implementation sketch for the future:
- Add a `bool contentLock` field, persisted.
- While content-locked, suppress cache regeneration at cycle wrap. Engine plays the snapshot indefinitely.
- Save/load problem: cache is engine-owned CCMRAM and not normally serialized. If we want content lock to survive save/load, the cache must serialize while locked (~768 B per locked pattern). Alternative: document that content-lock state is volatile across save/load.

Not in Phase 14B scope. Revisit after seed+log is shipped and we know what users actually want from a Bloom-shape lock.

## Engine playback

```
triggerStep(stepIndex):
    if cycle just wrapped:
        if !seedLocked OR !cacheValid:
            regenerateRhythm(sequence, _cache, _rng)
            regenerateMelody(sequence, _cache, ...)
        else:
            // seed-locked: regenerate cache from frozen seeds + current knobs
            regenerateRhythm(sequence, _cache, _rng)
            regenerateMelody(sequence, _cache, ...)
    cell = _cache[stepIndex]
    push to _gateQueue / _cvQueue
```

Note: under seed-lock, the cache still regenerates each cycle — the seeds are frozen but knob inputs are live. Content lock (deferred) would skip regen entirely. Today's `_gateQueue` / `_cvQueue` drain path is unchanged.

## Operations

- **NewR** → `rhythmSeed = rngPick(); clear rhythm entries in log;` refuse if seedLocked.
- **NewM** → `melodySeed = rngPick(); clear melody entries in log;` refuse if seedLocked.
- **Patience** → on Poisson hit, replace seed; refuse if seedLocked.
- **Mutate (permute)** → append `{SWAP_CELLS, a, b, domain}` to log; log entries apply after the generator's base pass. Refuse if seedLocked.
- **Mutate (regen one cell)** → append `{REGEN_CELL, idx, _, domain}` to log; generator re-hashes that cell's RNG (cheap via per-cell keys) and re-rolls. Refuse if seedLocked.
- **Mask** → generator computes per-cell rank during pass; engine filters at trigger time vs knob value. Knob is live (acts on next cycle regen even when seedLocked).
- **Tilt** → input to rank assignment; same as Mask.
- **Seed lock** → derived state `rhythmValid && melodyValid && !live`. No new field. While seedLocked: NewR/NewM/Patience/Mutate no-op. Cache still regenerates each cycle from frozen seeds + current knobs.
- **Repeat** → **baked into generator**, deterministic from seed. On Bernoulli hit during generation, generator marks cell as repeat-of-previous. Loop is bit-identical between iterations. *Feel change from today's per-trigger Bernoulli — explicit acceptance required.*
- **Jump / Sleep / Variation** → unchanged.
- **Burst** → decided during generation, expressed as multiple cells in cache. Same as Phase 14 trunk model.
- **Polyrhythm** → generator places cells at non-grid `relTick` values. Free.
- **Trill / mordent** → C1 (stored substeps as extra cells in cache). C2 (flag-driven inline expansion) remains an option if cell budget breaks.

## What might be lost or different (from what's actually present today)

### LOST

Nothing musical. No knob removed. No dictionary removed.

### CHANGED

1. **LOOP page tape visualization** — today reads `seq.events()[i]` for per-slot bar widths and pitch (`StochasticSequenceEditPage.cpp:476, 510`). Under seed+log, reads the engine cache instead. **Cache must be available before the UI tries to draw it** — startup ordering matters. UI gates on `_cacheCount > 0` or shows a "regenerating" placeholder. Visualization quality unchanged once cache is populated.

2. **Live mode → wholesale per-cycle regen.** Today: per-step writeback during playback. Proposed: regenerate whole cache at every cycle wrap. Per-event freshness inside a cycle disappears. Different feel — probably more musical, possibly less surprising mid-cycle. **Explicit acceptance required.**

3. **Repeat probability — baked, not floating.** Today: per-trigger Bernoulli, loop iterations vary. Proposed: bake at generation, loop is bit-identical. Stronger loop-as-identity. **Explicit acceptance required.**

4. **Project file format break.** Old projects have 64×6 event records. New format drops them. Old happy-accident content can't be reconstructed from seeds because the path is one-way (knobs+seed → events, not events → seeds). **Explicit acceptance of content loss required.** Mitigation: keep saved seeds + knobs intact; old projects regenerate similar-but-different output on load.

5. **Mutation log is bounded.** 16 entries. When full: discard oldest? Refuse new? Compact by folding into seed (impossible without re-deriving)? Bounded mutation history is a behavioral change from today's per-event mutation storage.

### PRESERVED

- Every knob and Routable.
- Both ticket dictionaries.
- All routing IDs.
- Scale, root, octave/range, divisor.
- LIVE page walker (reads engine recent-event history pool, not `_events[]`).
- Burst architecture (generation-time decisions, multiple cache cells).
- Mask / Tilt / Patience / Jump / Sleep mechanics — same math, same knobs, operating on cache.
- Pitch and duration ticket page editing — dictionaries unchanged.

## Acceptance bar (before this design can ship)

Per the review of 2026-05-22:

1. **Lock semantics defined.** Decision 2026-05-22: ship seed lock only; content lock deferred to future work. Seed lock is derived from existing state (`rhythmValid && melodyValid && !live`), no new field. Knob edits affect locked output at the next cycle regen — explicitly accepted as the Phase 14B behavior; Bloom-shape output freeze is future.

2. **Per-cell regen is deterministic without full RNG stream replay.** Implemented via per-cell keyed RNG: `rngState = hash(seed, cellIdx)`. Required before accepting REGEN_CELL semantics. Melody dependency on `lastDegree` either refactored away or accepted as "replay [N..end] for melody on REGEN_CELL," with replay cost proven cheap.

3. **Repeat decision is baked.** Documented as feel change; explicitly accepted (or rejected — in which case the entire design fails the bar).

4. **Live regen cadence is accepted as a feel change.** Per-cycle wrap regen, not per-step writeback. User-acknowledged behavioral shift.

5. **Old event tape migration loss is accepted in writing.** Migration plan: read old `_events[]` byte sequence, discard, load saved seeds, regenerate on first trigger. Old happy-accident content lost.

6. **Cache is engine-owned CCMRAM, not serialized.** No cache fields in `StochasticSequence`. No cache writes to project file. Cache lives in `StochasticTrackEngine`.

7. **UI reads a stable engine cache/history, not model data that can be absent.** LOOP page guards on cache availability. LIVE page already reads the engine pool, unaffected.

All seven items must be designed and signed-off before any patch lands.

## Serialization

Decided before Patch C lands; deferred for Patches A and B (no on-disk changes until C).

### What changes in `StochasticSequence::write` / `read`

**v1 (today):** seeds + knobs + dictionaries + 64×6 B `_events[]` tape.

**v2 (Phase 14B post-C):** seeds + knobs + dictionaries + 16×4 B `mutationLog[]` + `mutationLogCount`. No `_events[]`.

Loader logic:
```
if (storedVersion <= v1) {
    read seeds + knobs + dictionaries as today
    skip 384 bytes (legacy _events tape)
    mutationLog = empty
    mutationLogCount = 0
    // seedLock is derived; rhythmValid/melodyValid loaded from existing fields
} else {  // v2+
    read seeds + knobs + dictionaries
    read mutationLog[16], mutationLogCount
}
```

Cache is **not** serialized — engine regenerates from seeds at first trigger after load. One-cycle hydration delay on pattern switch is accepted.

### Decisions

1. **NewR clears rhythm-domain log entries.** NewR is a fresh-recipe reset; prior mutations don't survive a new seed. Same for NewM clearing melody-domain entries.
2. **No new lock fields persisted.** Seed lock is derived from `rhythmValid && melodyValid && !live` — those flags already serialize today, no change.
3. **Content lock deferred** — no related serialization work in Phase 14B.

### Migration loss

v1 projects lose their `_events[]` content. Saved seeds + knobs + dictionaries hydrate a new cache on first cycle wrap; old happy-accident content can't be reconstructed. Documented in the acceptance bar (item 5). Mitigation: ship Phase 14B with a project version bump prompt explaining the one-time content shift.

### Routing IDs

No new routable parameters. Existing `StochasticReserved` slot (routing ID 67) remains available for future use; not consumed by Phase 14B.

## Parameter coverage test (planned, Patch B → C boundary)

Risk: a knob that's silently disconnected from the generator (accent-alias regression) goes unnoticed after Patch C rewires generator output to the cache. Hedge with a coverage test that exercises every knob in `StochasticSequence`:

```
TestStochasticKnobCoverage:
  for each knob in StochasticSequence:
    set knob = MIN;  generate; fingerprint cache
    set knob = MAX;  generate; fingerprint again
    expect divergence above threshold (or expected stationarity)
```

About 30 cases, one per knob. Runs as part of CI on any commit touching the generator. **Not required before Patch A.** Land it as part of Patch B prep — same time as switching playback to the cache, where wire breakage would actually surface.

## Comparison vs trunk

|                          | Trunk (Phase 14)                   | Seed+Log (Phase 14B)              |
|---|---|---|
| Per-pattern storage       | ~768 B (96 cells × 8 B)            | ~120 B                            |
| Output persists           | Yes — trunk IS the output          | No — cache regenerates per cycle |
| Project save preserves accidents | Yes (trunk serialized)        | No (seeds regenerate, knobs shape) |
| Migration                | Read old events, synthesize trunk | Drop old events, regenerate       |
| Lock (Phase 14B scope)   | 1 bit (content lock)               | Seed lock only (derived; content lock deferred) |
| Knob edits affect locked output | No                          | Yes (next cycle regen) — content freeze is future work |
| Per-cell editing surface | Possible via trunk cell edits      | Not present (matches today)       |
| RAM win                  | Modest (768 B per active track)    | Large (~120 B per pattern)        |
| Identity model           | Output = identity (Bloom-shape)    | Recipe = identity (Tuesday-shape) |

## Why seed+log over trunk

Trunk's "output is the identity" pitch is musically tidy but the cost is real:

- **Per-pattern storage** — trunk needs 768 B (96 cells × 8 B). Seed+log needs ~120 B. On a board where every kilobyte fought for has been an event, the smaller shape wins on principle.
- **Identity-as-output is a Bloom convention, not a law.** Tuesday, Marbles, Turing, Hemisphere all treat recipe (seed + knobs) as the identity. The community is not unanimous, and our shipped product already exposes seeds in `StochasticSequence`. Following that grain is cheaper than fighting it.
- **The "knob edits change locked playback" failure mode is solved** by the two-tier lock (seed lock + content lock). Content lock = engine cache snapshot, untouched until released. That gives Bloom-shape lock when the user wants it, recipe-shape lock when they want that. Trunk only offers one.
- **Per-cell editing was never a feature** in our stochastic UI. The pitch and duration ticket pages edit dictionaries (the generator's pick pool), not per-step output. Trunk's main editing advantage doesn't apply to our actual workflow.

The legitimate trunk wins remaining: project save preserves bit-exact happy accidents, and 1-bit lock semantics. Both are real, but neither outweighs the RAM win + architectural alignment with the rest of the codebase.

## Foundational invariant — validated 2026-05-22

Per-cell keyed RNG is the precondition for everything else in this design. Validated on `feat/stochastic-seed-log`:

- **Header:** `src/apps/sequencer/engine/KeyedRng.h` — 32-bit murmur3-style mixer over `(baseSeed XOR cellIdx * goldenRatio)`. Branch-free, ~6 multiplies + shifts. `cellSeed(seed, idx)` for fresh cells, `cellSeedAfterRegen(seed, idx, mutationGen)` for REGEN_CELL.
- **Test:** `src/tests/unit/sequencer/TestStochasticKeyedRng.cpp` — 8 cases, all green:
  - Hash determinism.
  - Adjacent cells get >8-bit-different seeds (mixer not weak).
  - Same `(seed, idx, knobs)` → identical output (property 1).
  - Forward / reverse / shuffled generation orders all match (property 2).
  - REGEN_CELL on cell N changes that cell and leaves every other untouched (property 3).
  - Successive regens keep producing distinct outputs (no modular collision).
  - Seed change propagates to >2/3 of cells (mixer not idx-dominated).
  - Knob change at fixed seed diverges >2/3 of cells (generator actually reads knobs).

The three load-bearing properties hold. REGEN_CELL is mechanically free — no stream replay needed.

## Picker inventory

Cataloging every RNG draw in `src/apps/sequencer/engine/StochasticGenerator.cpp` against the keyed-RNG model. **Pure per-cell** means the picker reads only `(rng, knobs)` and can be migrated by swapping its `Random` for `keyed_rng::forCell(seed, idx)`. **Cross-cell dependency** means the picker reads state from prior cells and needs refactor.

### Rhythm path

| Picker | Location | Draws | Per-cell pure? | Notes |
|---|---|---|---|---|
| `pickDuration` | `StochasticGenerator.cpp:262` | 1 draw over weighted LUT | **Yes** | Reads only knobs + dictionaries. Migrate trivially. |
| Rest Bernoulli | `:427` | 1 | **Yes** | `rng.nextRange(100) < sequence.rest()`. |
| Legato Bernoulli | `:429` | 1 | **Yes** | Same shape. |
| Slide Bernoulli | `:430` | 1 | **Yes** | Same shape. |
| Burst-gate Bernoulli | `:447` | 1 | **Yes** | Conditional draw — only fires when parentTicks ≥ 96. |
| `pickBurstCountFromLut` | `:451` | up to 2 | **Yes** | Reads `burstCount` knob. Conditional on burst-gate. |
| `pickBurstSpacingFromLut` | `:457` | up to 2 | **Yes** | Reads `burstRate` knob. Conditional on burst-gate. |
| `generateMaskRanks` weight | `:344` | 1 per cell | **Yes** | Reads `tilt` + `durationIndex` from the just-generated event. Cross-cell only in the sort — the random weight per cell is pure. Migrate: weight each cell with its own keyed RNG, then sort. |

**All rhythm draws are pure per-cell.** The current sequential RNG is an artifact, not a dependency.

### Melody path

| Picker | Location | Draws | Per-cell pure? | Notes |
|---|---|---|---|---|
| `generateDegree` (Mutate path) | `:184-238` | variable | **Yes** | Reads `mutateMagnitude`, scale, `range`, dictionaries — no `lastDegree`. |
| `generateDegree` (NewM path) | `:493` | variable | **No — depends on `lastDegree`** | Complexity kernel narrows around the previous cell's degree. Cross-cell state. |
| Burst child `generateDegree` | `:399` | variable | **Yes** | Locally sets `lastDegree = -1` before calling, so no cross-cell coupling. |

### The `lastDegree` problem

`generateDegree` in the NewM path uses `lastDegree` to bias the next pitch toward melodic continuity (the complexity kernel narrows around the previous degree). This is the only cross-cell dependency in the entire generator.

Three options, in order of preference:

1. **Replay melody pass for `[N..end]` after REGEN_CELL at N.** Melody pass is fast and writes only the CV field. For typical 16-32 cell patterns this is <10 µs on STM32. The amortized cost across a session is negligible compared to the RAM and architectural win. **Recommended.**

2. **Refactor `generateDegree` to use a stable "anchor" derived from cell index instead of prior cell's degree.** e.g. `anchorDegree = keyedRng(seed ^ MELODY_ANCHOR_TAG, idx)` produces a per-cell anchor; complexity kernel narrows around that. Loses literal melodic continuity in favor of statistical continuity. Different musical feel — would need user A/B before committing.

3. **Cache `lastDegree` per cell as part of the cell output.** Cell N stores its own `lastDegree` so cell N+1 can read it. Brings cross-cell state back in a localized way but defeats some of the keyed-RNG win.

Option 1 is the working plan. Cost is bounded and known; we just run melody pass twice for a cell when it's regenerated. Property tests can pin it.

## Go / no-go

**Go.** The keyed-RNG approach is mechanically sound; all three load-bearing properties (determinism, order independence, mutation isolation) hold under a generator that mirrors the real picker shape. The one cross-cell dependency — `lastDegree` in melody generation — has three viable mitigations, with melody-pass replay being the natural default.

Patch A scope can proceed: engine-side cache structure + plumbing alongside existing event tape, no behavior change, no model changes yet.

## Next step

Write Patch A:
- `CachedCell` struct in `StochasticTrackEngine` — engine-owned CCMRAM, 8 B per cell × 96 cells per active track.
- Helper `engine->regenerateCacheFromSeeds()` that calls existing `StochasticGenerator::generateRhythm` / `generateMelody` and copies the result into the cache. Engine still plays from `_events[]`; cache is shadow-only.
- Sim build asserts cache matches the event tape's playback semantics (every cell's `(relTick, cv, gateLen, flags)` derives the same gate/CV stream).
- No UI changes. No model changes. No behavior changes.

Patch A unblocks Patches B (switch playback to cache) and C (delete `_events[]`, mutation log lands, generator writes cache directly).
