# Phase 17 — Sequence reduction + mutate redesign

**Status:** Drafted 2026-05-23. Refreshed 2026-05-24 after the slot-keyed cache /
seed-domain isolation session.

## Goal

Collapse pattern storage from full events array to `(seed + knobs + sparse delta)`.
Align mutate semantics with Proteus's clean model. Make the existing Lock concept
own a real meaning. Drop the dead per-event fields the cache no longer reads.

## Anchor (locked design)

A pattern's audible state at any moment is the deterministic function of:

```
(rhythmSeed, melodySeed, all sequence knobs, degree tickets, duration tickets,
 window) + mutation delta
```

Knob movement reshapes via deterministic regen. Mutate writes to the delta.
Patience rotates the seed and clears the delta.

## What landed before this refresh

Cache-pick / scrubbing migration: cache walk now owns duration, variation, burst
firing, burst count, burst spacing, gate length, and BurstPitch Roll. Pitch
knobs scrub Loop melody on a fixed `melodySeed`. Per-trigger Feel scaling.
Coalesced refresh between Live rhythm + melody writes. Routed-CV invalidation
via shaping snapshot.

## What landed in the 2026-05-24 session

- **Slot-keyed cache.** `cells[K]` is a pure function of `(seed, events, K)`.
  Cache walk iterates `0..size-1`; First does not perturb cell content. The
  engine reads `cache.cells[readIndex]` directly. Window-edit refresh keys
  off Size diff only.
- **NewR / NewM domain isolation.** Anchor pitches in Loop melody keyed per
  slot by `melodySeed` via `keyed_rng::cellSeed(melodySeed, slot)`. Cluster-
  tail Generate-mode pitch moved off `rhythmSeed` onto `melodySeed` with a
  tail-channel salt. Slide moved to `melodySeed` with a slide-channel salt.
  NewR no longer shifts pitch; only NewM does.
- **Legato moved to cache-pick** under `cellRng` (rhythm-domain Bernoulli;
  same shape as Burst). `event.legato` is dead storage.
- **Slide moved to cache-pick** under melody-keyed RNG. `event.slide` is
  dead storage. Cluster tails inherit anchor's slide decision.
- **Tilt is pure trigger-time.** `event.densityRank` set by `generateMaskRanks`
  at RENEW / Patience / Mutate using a Tilt-free duration sort. Trigger-time
  Mask blends rank-percentile and salt-percentile by `|tilt|/100`.
- **Last collapsed into Size.** `sequence.last()` returns `size-1`;
  `setLast()` is a no-op. UI no longer exposes Last.
- **Jump octave drift resets** in `renewMelody`, `refreshLoopSources`, and
  the patience-driven melody regen branch of `ensureLoopSources`.
- **Cycle-end hooks bundled** into `rollCycleEndHooks()` — Jump, Sleep,
  Patience-R/M, Mutate all fire on both natural pattern wrap AND preempted
  reset-measure boundary. Preempt guard `_patternIndex != seq.first()`
  prevents double-counting at the boundary.
- **Engine-layer comment audit.** Stale phase/patch annotations and
  removal-history dropped from the cache, generator, engine, and headers.
- **Project rule added:** single code comments cap at 3 lines (CLAUDE.md +
  auto-memory).

Post-refresh contract: rhythm AND pitch knobs are deterministic, reversible,
scrubbable on a fixed `(rhythmSeed, melodySeed)`. Cluster placement
(rhythm-domain) does not shift pitches (melody-domain) anywhere — the cross-
domain leak via cellRng-driven anchor counting / cluster-tail pitch is closed.
RENEW is the only destructive seed rotation.

## Open questions (resolve before implementation)

### Q1 — Mutate knob model

**Current:** bipolar Mutate knob (-100..+100). Positive = permute (Marbles swap),
negative = regenerate one event in place.

**Proteus model:** two separate axes — `noteChangeProbability` (Bernoulli, one
slot rewritten per cycle if roll hits), `octaveChangeProbability` (Bernoulli,
whole melody octave-bump up/down).

**Decision options:**
- **A**: keep current bipolar Mutate
- **B**: split into two axes (note % + octave bump %)
- **C**: keep bipolar Mutate, add octave-bump as a separate knob

Octave bump is musically distinct from "rewrite one slot" — different feature
shape, possibly belongs in a separate knob regardless.

### Q2 — Lock semantic via accumulate flag

Lock toggle exists in `StochasticTrack` but doesn't ship a behavior. Phase 17
is its chance.

**Proteus's three-state switch:**
- mutate + accumulate (normal) — mutations persist
- mutate, no accumulate (preview) — mutations audible but baseline preserved
- neither (locked) — no changes

**Decision options:**
- **A**: Lock = "no mutate, no patience" (kills all writes to delta + suppresses
  seed rotation)
- **B**: Lock = "accumulate off" — mutations roll per-cycle but discarded at
  cycle end; baseline always recoverable
- **C**: Add a tri-state config (normal / preview / lock) like Proteus

B is the musically interesting one. Live-performance friendly: hear variations
without losing your current pattern.

### Q3 (deferred — Patience stays Poisson, matches Proteus + current code)

### Q4 — Delta storage shape

**Options:**
- **A**: keep current full events array, add a "modified" bit per slot. Save
  format unchanged shape, just smaller per-event (after Q5 reduction).
- **B**: sparse map (slot → override event). Smaller in steady state, more
  code to maintain.
- **C**: events array becomes ephemeral runtime cache; saved state is just
  `(seed + knobs + tickets + sparse delta entries)`.

A is the cheapest path. B/C are bigger refactors.

### Q5 — Pitch in tape vs cache

**Current:** `event.degree`, `event.octave` stored on tape, read by cache for
Hold-mode pitch. Cache only rerolls for Roll-mode cluster tails.

**Options:**
- **A**: keep pitch on tape. Mutate's per-slot pitch writes go into tape.
  Simplest, but pitch isn't yet "knob-scrubbable in real time" — only Phase 11's
  `generateMelody` regen path makes it so, and that rewrites the whole tape.
- **B**: move Hold-mode pitch into cache walk too. Cache picks via
  `generateDegree` seeded by `(melodySeed, cellIdx)`. Tape becomes redundant for
  pitch except as a UI mirror or as Mutate's override storage.

B aligns pitch with rhythm under the post-P7 contract. Required if we want to
drop `event.degree/octave` entirely.

### Q6 — Live mode interaction with Lock

If Lock = "accumulate off" (Q2 option B), what happens in Live mode?

- Live writes new events to tape every trigger by design — that IS accumulation.
- Lock + Live could mean "Live continues to generate, but writes are discarded
  at cycle end" — every cycle replays from the same baseline, different content
  inside.
- Or Lock + Live = nonsense, force the mode to flip to Loop when Lock engages.

## Term rename (cheap, independent commit)

Pre-Phase-16 the (idea) was parent / child — one anchor event spawning N sub-notes
inside its duration window. Phase 16 retired that (idea) for the flat path and
replaced it with anchor / cluster (idea). The (code) names lagged behind. Land
a one-commit rename pass so (code) matches the post-Phase-16 (idea):

- `event.childCount` (code) → `event.clusterCount` (code) — number of cluster
  cells beyond the anchor, **before** dropping the field in the deeper cleanup.
- `event.burstRate` (code) — name is fine, but reinterpret-as-spacing-LUT-slot
  is implicit; document or rename to `event.burstSpacingSlot` (code) if clearer.
- `StochasticGenerator::evaluateChildren` (code) → `evaluateBurst` (code) or
  `evaluateRepeatBurst` (code). Tied to the Repeat path only.
- `EvaluatedChild` (code) struct → `EvaluatedBurstNote` (code).
- `StochasticTrackEngine::kMaxChildren` (code) → `kMaxBurst` (code).
- `CellAux::burstChild()` (code) — bit always false under flat. Delete the
  accessor and the bit entirely; keep the byte (binary compat with project
  loads is not required per AGENTS.md, but the byte is free and may host a
  future flag).
- All comments referencing "child" / "parent" — replace with "cluster" /
  "anchor" (idea) terminology.
- **BurstPitch (code) → BurstHold (code)** — the (ui) toggle message reads
  "BURST HOLD" / "BURST ROLL" (set in Phase 16 P9 demotion) but the
  underlying enum is still `StochasticBurstPitch::Parent` / `::Generate`
  (code) and the accessor is `sequence.burstPitch()` (code). The (code) name
  predates Phase 16 and survived untouched. Wire-format note: the enum is
  serialized as `uint8_t` so renaming values is safe as long as integer
  ordering stays (Parent=0=Hold, Generate=1=Roll, Last=2). Rename targets:
    - `enum class StochasticBurstPitch` → `StochasticBurstHold`
    - `::Parent` → `::Hold`
    - `::Generate` → `::Roll`
    - `sequence.burstPitch()` / `setBurstPitch` → `burstHold()` / `setBurstHold`
    - `_burstPitch` field → `_burstHold`
    - `printBurstPitch` / `editBurstPitch` → `printBurstHold` / `editBurstHold`
    - `ContextAction::BurstPitch` (in `StochasticSequenceEditPage.h`) →
      `ContextAction::BurstHold`
    - Performance list model `BurstPitch` entries → `BurstHold` (display
      string "Burst Pitch" → "Burst Hold" or "Burst Hold/Roll")
    - Engine snapshot `_lastShapingBurstPitch` → `_lastShapingBurstHold`
    - Comments + test names that reference "BurstPitch" or "burstPitch"
  Open (ui) question: keep menu label "BPITCH" or rename to "HOLD" / "BHOLD" —
  decide before the commit.

Done as a pure rename commit, no behavior change. Tests' literal string
references stay green; only identifier names move.

## Architectural split — keep two `triggerStep` (code) paths

The two cluster (idea) playback mechanisms are NOT redundant. They are
musically distinct and the dual `triggerStep` (code) implementation reflects
that distinction:

- **Flat path** (non-Repeat) — each cluster cell (idea) is a full event-slot
  trigger. `_patternIndex` (code) advances one slot per `triggerStep` (code)
  call. Cluster cells exist as consecutive cells with shorter durations.
  Audible: burst that **moves the sequence forward**.
- **Repeat path** — `_patternIndex` (code) frozen at the captured slot.
  `triggerStep` (code) fires once but schedules anchor + multiple intra-trigger
  burst-note pushes via `evaluateBurst` (code). Audible: burst that **freezes
  the position** and stutters the same anchor.

The user values the Repeat (ui) musical character. Do NOT collapse the two
paths into one. The rename is fine; the dual scheduling stays.

## Repeat path's burst source after `event.childCount` drop

When the cheap-win cleanup drops `event.childCount` / `event.burstRate` (code),
the Repeat path loses its current source for "how many sub-notes should I
schedule." Resolution:

- Repeat (ui) re-rolls burst on each Repeat fire using the **current**
  `sequence.burst()`, `sequence.burstCount()`, `sequence.burstRate()` (code)
  knobs via the same pickers the cache walk uses
  (`StochasticGenerator::pickBurstCount`, `pickBurstSpacingSlot` (code)).
- Side effect: Repeat (ui) bursts become knob-reactive — turning BurstCount or
  BurstRate while Repeat is firing reshapes the stutter in real time. Matches
  the post-P7 scrubbing contract for the flat path.
- The captured `_lastEvent` (code) still owns the anchor pitch and gate
  length. Only the burst parameters get fresh-rolled. (Slide and legato no
  longer live on the event — cache owns them; `_lastEvent` carries the
  cache's last-picked values, so Repeat replays them as-is.)

## Likely scope of changes

**Cheap wins (independent of Q1 / Q2 / Q4-Q6 decisions):**
- **Term rename pass** — single commit, pure (code) refactor. Parent/child →
  anchor/cluster across the engine, generator, and tests. `BurstPitch` →
  `BurstHold`; enum values `Parent / Generate` → `Hold / Roll`. See full
  rename list in the "Term rename" section above.
- **Drop dead event fields** — `event.durationIndex`, `event.childCount`,
  `event.burstRate`, plus the already-dead `event.legato` / `event.slide`
  bits. Cache picks all five from knobs now. Saves ~2 B per event after
  re-packing (6 B → 4 B), ~128 B per pattern × 17 patterns ≈ 2 KB per
  stochastic track (doesn't shrink the model variant cap on its own — see
  PROJECT.md, the cap is set by CurveTrack).
- **Repeat-path burst source migration** — Repeat re-rolls burst from current
  `burst() / burstCount() / burstRate()` knobs via the same pickers the cache
  walk uses. Side effect: Repeat bursts become knob-reactive in real time
  (matches the flat path's scrubbing contract). Required after the dead-field
  drop because `_lastEvent.childCount/burstRate` are gone.

**Mutate redesign (open design — see Q1 / Q2 / Q4):**
- Refactor `mutateRhythmOne` / `mutateMelodyOne` to write delta entries (Q4).
- Decide and implement Q1 (mutate knob shape).
- Decide and implement Q2 (Lock semantic).

**Bigger restructure (only if Q1 / Q2 push there):**
- Move Hold-mode pitch into cache walk (Q5) — drop `event.degree / octave`
  storage, tape becomes pure UI mirror.
- Sparse delta storage (Q4 option B or C) — store `(seed + knobs + sparse
  overrides)` instead of a full events array.
- New tri-state Lock UI (Q2 option C).

## Wire format

Dev branch per AGENTS.md line 81 — no migration ceremony, no project version
bump. New saves use the new shape; old saves either auto-migrate at load
(silently fill defaults for dropped fields) or get discarded as disposable.

## References

- Proteus mutation model — `temp-ref/docs/SeasideModularVCV/src/Proteus.cpp:680-789`
  - Poisson patience path — line 742-758
  - Bernoulli mutate rolls — line 760-789
  - Three-state switch (mutate + accumulate combos) — line 408-422
- `LOCK-DESIGN-DEFERRED.md` — earlier sketch of Lock semantics, may be folded in
- `PHASE15-PITCH-MATH-REVIEW.md` — sibling parking lot for the pitch-law
  revamp (Marbles continuous Bias/Spread, Range bipolar, ticket transparency,
  layer ownership). Independent of PHASE17 structural cleanup.
- Current Mutate / Patience code (line numbers move; grep by symbol):
  - `StochasticGenerator::mutateRhythmOne`
  - `StochasticGenerator::mutateMelodyOne`
  - `StochasticTrackEngine::rollPatience`
  - `StochasticTrackEngine::rollCycleEndHooks` (bundles Jump / Sleep /
    Patience / Mutate so all four fire on the same boundary)

## Decisions log

- 2026-05-23: Draft created. Phase 16 P11 just landed; sequence reduction
  becomes feasible because pitch knobs are now scrubbable on a fixed seed.
  Patience stays Poisson (Q3 dropped — matches Proteus).
- 2026-05-23: Locked the parent/child → anchor/cluster rename as a separate
  cheap-win commit. Locked the dual `triggerStep` (code) scheduling — flat
  and Repeat (ui) paths stay distinct because they produce different audible
  characters (sequence-advancing burst vs frozen-slot stutter). User values
  the Repeat (ui) musicality; collapsing into one path would lose it.
  Decided Repeat (ui) burst params will re-roll from current knobs after
  the dead-field cleanup.
- 2026-05-23: Added BurstPitch → BurstHold (code) rename to the cheap-win
  list. (ui) toggle already reads HOLD/ROLL; (code) still used
  Parent/Generate from the pre-flat-cell parent/child (idea). Enum integer
  ordering preserved so wire format is unaffected.
- 2026-05-24: Slot-keyed cache + cross-domain seed isolation landed (see
  "What landed in the 2026-05-24 session" above). Slide / Legato cache
  migration that was queued as future work is done; both fields are dead
  storage now. Cheap-wins list trimmed accordingly — the per-event field
  drop now includes the dead `legato / slide` bits alongside `durationIndex`
  / `childCount` / `burstRate`. Tilt is realtime; rank lives on the event
  via `generateMaskRanks`. Last collapsed into Size. Cycle-end hooks bundled
  so all four (Jump / Sleep / Patience / Mutate) advance on the same
  boundary. PHASE15 pitch-law revamp (Marbles continuous, Range bipolar,
  ticket transparency, layer ownership) tracks separately — independent of
  this structural cleanup.
