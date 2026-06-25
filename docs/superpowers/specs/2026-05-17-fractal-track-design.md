# Fractal Track Engine — Recorder/Mutator Design

> Current status, 2026-05-22: historical spec. The current best contract is in
> `.tasks/fractal-track-implementation/TASK.md`,
> `.tasks/fractal-track-implementation/DICTIONARY.md`, and
> `docs/fractal-track-options-comparison.html`.
>
> Current topology: FractalTrack is a parent-dependent command/rule sequencer over
> sampled melodic CV/gate material. Parent tracks provide the motif; the engine keeps a
> small volatile trunk of 16-bit pitch/gate-length sections; the model owns capture/read rules
> such as record extent, Replace/Latch/Once, PunchIn, lock, and loop lens. Fractal is a
> melodic material mirror, not a high-fidelity CV recorder. It should not aim to capture
> every CV movement inside a section. Fractal does not know why the parent output changed:
> parent sequencing state, mutes, routing, resetMeasure, held notes, curves, and scripts are all opaque.

## Summary

FractalTrack is a step-sampled CV/gate child layer over one or two source tracks: it captures their gate+CV output once per step into a small grid (the trunk), then plays that grid back on loop with Proteus-style mutation evolving it at loop boundaries. On top of the trunk sit zero-storage branches (live math transforms — reverse, invert, transpose, etc.) and per-step ornamentation (trills, anticipations, mordents) that compose into a generative output. Identity: not a stochastic generator, not a step sequencer — a record-and-evolve child layer with trunk → branches → ornamentation as three composable layers, driven by TuesdayTrack-style track-level macro controls, not per-step programming.

## Core Concept

The Fractal Track is a **step-sampled CV/Gate looper with mutation**. It samples the gate/CV output of 1–2 master tracks (any engine type) once per step, loops that captured buffer, and applies Proteus-inspired mutations at loop boundaries. It is NOT a stochastic generator — it is a **record-and-evolve** child layer over existing tracks.

**Important:** This is a step-sampled recorder, not a continuous audio recorder. Each step captures one instantaneous gate + CV snapshot at step-clock boundaries. It does not capture gate length, retriggers, legato, slide, or intra-step timing. If those are needed later, they are a separate feature gate.

### Identity: How it differs from Stochastic Track

| Aspect | Stochastic Track | Fractal Track |
|--------|------------------|---------------|
| Source material | Step probability grid (self-contained) | 1–2 external master tracks (any engine) |
| Core loop | Generate → evaluate → lock | Record → loop → mutate/evolve |
| Mutation trigger | Per-step probability rolls | Loop-boundary lifecycle events |
| Buffer semantics | Capture evaluated events into lock buffer | Step-sample gate+CV, loop, mutate |
| Record modes | Binary capture (Hold lock) | Replace vs Latch, PunchIn/Immediate, Loop/Once, record quantize |
| Relationship | Stand-alone sequencer | Post-processing child of other tracks |

---

## Key Design Decisions

### KD-1: Input Source Resolution — Any-Engine via TrackEngine Output

**Decision:** Capture the parent's **final emitted output** — `gateOutput(0)` / `cvOutput(0)`, the rendered gate+CV the parent sends to its jack (after slide, transpose, gate-length, accumulator — everything its engine applies). **Not** sequence step data, and **not** any intermediate/pre-output engine state. Fractal mirrors what the parent *plays*: a parent rest (gate low) records a rest cell; a slewed CV records the slewed value at the section boundary.

**Rationale:** The existing `LogicTrackEngine` already demonstrates per-tick resolution of parent engines via `_engine.trackEngine(index)`. The Fractal Track does NOT need a `FractalSourceInterface` — it just reads the concrete output of any engine. This avoids adding virtual methods to all 8 existing engine types.

**Pattern:** Same as `LogicTrackEngine` lines 195–212: resolve `TrackEngine*` pointers once, read `gateOutput(0)` and `cvOutput(0)` each tick.

**Cycle safety rule:** Fractal must read source tracks that are evaluated *before* Fractal in the same tick. The Engine evaluates tracks in index order (0→7). Therefore:
- Source track A and source track B must have indices **lower** than the Fractal track's index.
- If either source index ≥ Fractal track index, the engine treats that source as idle (gate=false, cv=0).
- **Invalid source UX:** The list model prevents selecting source indices ≥ Fractal track index (clamped in UI). Engine does not log warnings — it silently treats out-of-range sources as idle. The user never sees a broken configuration from the UI.
- No cross-track feedback is possible by construction. This matches the existing Performer convention where linked tracks must be earlier in the track array.
- Alternative (rejected for MVP): read from previous tick's latched output. This would allow any source index but requires a per-engine output latch — too much architectural change for MVP.

### KD-2: Buffer Format — Inline Bitpacked uint16 Array (in-engine, CCMRAM)

**Decision:** The trunk is a single **inline** `uint16_t` array — one cell per fractal section — held as a member of FractalTrackEngine (which lives in `CCMRAM_BSS`). **No heap.** Bitpacked:

```cpp
uint16_t _trunk[CONFIG_FRACTAL_MAX_CELLS];  // inline engine member, CCMRAM BSS

// bits 0-10:  CV (11-bit fixed-point, semitones rel root, ~5.9 cents/LSB)
// bits 11-14: gate length (4-bit: 0=rest, 1=trigger, 2-14=proportional, 15=full/tie)
// bit 15:     valid (uncaptured cells output the project root, gate off)
```

Configurable length: default **64 cells**, max **128**.

**Why inline, not heap:** the engine is a discriminated-union member in `CCMRAM_BSS`. An inline array is pre-allocated BSS — it costs nothing beyond the engine's union slot, needs no allocator, and honours Stage Gate D ("no heap in the tick path"). A heap buffer would change the rail, require OOM handling, and break that gate.

**RAM:**
- Default 64 cells × 2 B = **128 B**
- Max 128 cells × 2 B = **256 B**
- In **CCMRAM**, inside the FractalTrackEngine union slot → **net-zero** while the engine stays under the engine-union max (944 B). Not in SRAM, not on the heap.

**Why bitpacked uint16:** one cell = one section snapshot. 11-bit CV (~5.9 cents/LSB) is ample for a mirrored melodic line; the 4-bit gate-length field carries rest / trig / proportional / tie in a single word. 16 bits/cell with no split arrays and no padding.

**Why no per-cell duration:** one cell per section on a uniform grid — each cell *is* one section. Intra-section motion is intentionally discarded (the mirror identity). Sub-section microtiming is the deferred **Feel** mode (KD-14b), which would widen the cell to carry an onset-phase field (a CCMRAM cost, gated there).

**Persistence:** the trunk is **volatile engine state**, not serialized. Project save stores model config only; the trunk starts empty on power cycle and clears on track-mode change, buffer-length change, or explicit user clear. (Saving recorded loops as project data is a possible post-MVP revisit — it would affect file format and flash wear.)

**Buffer length change:** Changing `bufferLength` clears the entire buffer. No attempt to preserve overlapping steps — the new buffer starts empty.

**Buffer ownership:** One buffer per FractalTrackEngine instance, not per pattern. Pattern change updates configuration (divisor, scale, mutation params) but does not swap the recorded buffer. There is no per-pattern buffer array.

### KD-3: Loop Lifecycle Model — Proteus-Inspired Boredom/Mutation

> **Status: DEFERRED.** The whole mutation/evolution subsystem (KD-3, KD-6, KD-10) is out of current scope — it duplicates Stochastic's mutate/degree-rotation/sleep and adds the most engine + UI weight for the least unique payoff. The mutation zone (`mutateFirst`/`mutateLast`) survives, repurposed as the **ornament eligibility zone** (`ornFirst`/`ornLast`, KD-13). Revisit as a later phase.

**Decision:** Adopt the Proteus spec's loop-boundary lifecycle, augmented with a per-loop-cycle evolution system (KD-6) and a trunk/branch cycle (KD-12):

1. **Boredom/Patience**: resets the loop via auto-capture at loop boundaries.
2. **Mutation via Evolution**: selects a step index in the mutation zone, influenced by Evolution mode/depth/history (KD-6), and re-generates its CV.
3. **Octave Shift**: offsets CV values in the mutation zone by ±12 semitones.
4. **Trunk/Branch cycle management**: track trunk/branch phase counters and switch layers at boundaries (KD-12).

**Loop-boundary execution order (during trunk phase):**
```
1. Boredom reset (patience): may set _autoCapturePending
2. Select mutation target via evolution mode + depth + history
3. Mutate selected step's CV via complexity + scale
4. Record mutation to history: {stepIndex, oldCV, newCV, loopCount}
5. Octave shift
6. Decrement trunk cycle counter; if zero, switch to branch phase
```

**Loop-boundary execution order (during branch phase):**
```
1. Advance to next branch in path
2. Decrement branch cycle counter; if zero, switch to trunk phase
3. No buffer mutation during branch phase — branches are non-destructive transforms
```

**Rationale:** These are the Proteus §3.2–3.4 mechanics, adapted from "melody buffer reset" to "captured-loop mutation." The evolution system (KD-6) adds per-loop-cycle memory; the trunk/branch cycle (KD-12) adds Bloom-style generative navigation.

### KD-4: Two-Master Input Mixing

**Decision:** Port Vinx LogicTrackEngine's gate/note logic modes for combining 2 master track outputs.

**Gate logic** (when both inputs are active):
- `A` / `B`: Use only one master's gate
- `AND` / `OR` / `XOR` / `NAND`: Boolean combination
- `Random`: Randomly pick one master per step

**CV logic** (when combined gate is on):
- `A` / `B`: Use only one master's CV
- `Sum` / `Avg` / `Min` / `Max`: Arithmetic combination
- `Random`: Randomly pick one master's CV per step

**Rationale:** LogicTrackEngine.cpp already implements this exact set of 8 gate modes and 8 note modes. Adapting it from step-data → output-reading is trivial: replace `step.note()` with `trackEngine.cvOutput(0)`, replace step gates with `trackEngine.gateOutput(0)`.

### KD-5: Recording Modes — Overwrite (MVP), Blend (Post-MVP)

**Decision:** MVP has **overwrite only**: recording replaces buffer content with new gate+CV snapshots.

**Post-MVP blend mode:** Weighted blend of old and new CV on overlapping gates:
- `newCV = lerp(oldCV, sourceCV, blendAmount)` where `blendAmount` ∈ [0,1] is a user parameter.
- Gate: OR of old and new gates (overdub adds activity, never removes).
- Valid bitmap: sets bits, never clears.

**Why NOT Palimpsest additive CV accumulation:** Palimpsest works because it accumulates *activity level* (0–MAX_CV envelope), not pitch. Additive accumulation on V/Oct CV means `0V + 2V + 2V = 4V` — the pitch drifts sharp by an octave per overdub pass. That is musically broken for pitch. Blend (weighted average) preserves pitch semantics.

**Compose/decompose semantics (post-MVP):** The compose/decompose *rates* from Palimpsest can be repurposed to control the `blendAmount` parameter (compose = high blend toward new, decompose = low blend, keep old). But they must never add/subtract raw V/Oct voltages.

### KD-6: Per-Loop-Cycle Evolution System

**Decision:** Loop-boundary mutation step selection is biased by a **per-loop-cycle evolution system** — MutationHistory, SelectionPressure, and EvolutionDepth — rather than purely random RNG picks.

**Components:**

```
MutationHistory: circular buffer of 16 records
  struct MutationRecord {
      int8_t  stepIndex;       // which step (0-255)
      float   oldCV;           // CV before mutation
      float   newCV;           // CV after mutation
      uint16_t loopCount;      // when it happened (for age/decay)
  };
  Records: 16 x ~16 B = ~256 B inline in engine

SelectionPressure: mode-selectable bias function over history
  Three user-selectable modes:
    1. Even-Spread (Explore):
       Bias toward steps least-recently-touched. Feels like a cursor
       systematically scanning through the mutation zone.
    2. Hot-Spot (Cluster):
       Bias toward recently-mutated steps + their neighbors. Creates
       clusters of related mutations in one region before drifting.
    3. Pattern-Follow (Mimic):
       Bias toward steps whose current CV matches historical mutation
       source values. If step 2 went C->E last cycle, and step 8 is
       currently at C, bias toward picking step 8.
  Each mode is a pure bias function over the history buffer — no
  state beyond the mode enum (2 bits on model).

EvolutionDepth: single uint8_t (0-100%) on model
  0% = purely random step selection (ignore history)
  100% = full bias toward history-selected step
```

**How it works at loop boundaries:**
```
On trunk-phase loop boundary:
  1. Roll evolution RNG (persistent _rng reseeded on transport reset)
  2. If mutationProb triggers:
     a. SelectionPressure biases which step index in mutateFirst..mutateLast
     b. EvolutionDepth controls bias vs random: depth=0 -> pure random,
        depth=100 -> full pressure weighting
     c. Mutate selected step's CV via complexity + scale
  3. Record mutation to history: {stepIndex, oldCV, newCV, loopCount}
```

**Success scoring deferred:** History records mutations but cannot "learn" until a success mechanism exists. Pressure biases toward exploration/recency/pattern, not "better" outcomes. When success scoring is added (post-MVP), it will update per-type weights in the bias function.

**Persistent RNG preserved for underlying randomness:** The engine still holds `_rng` (uint32_t, reseeded on transport reset). The evolution system biases RNG rolls, it doesn't replace them. Turing DNA remains a post-MVP upgrade (see Reference Porting B).

### KD-7: Record Trigger — Fractal-Local Record Arm

**Decision:** Recording is controlled by a **Fractal-local `_recordArmed` flag**. Not tied to the global `_engine.recording()` state.
- User toggles record arm via list UI parameter or dedicated button.
- When armed and transport is running: records source gate+CV into buffer on each step.
- Auto-stop conditions: buffer fills to `loopLast`, or user disarms.
- Auto-capture: if buffer is empty and recording starts, first loop pass fills it.

**Why Fractal-local, not global:** Global record (`_engine.recording()`) is for MIDI/external input capture. Fractal records from internal track outputs — different source, different lifecycle. Coupling them would mean every global-record toggle also starts/stops Fractal capture, which is not what the user intends. Post-MVP integration (e.g., global-record-arms-all-recording-tracks) can be added if needed.

### KD-8: Model Size Target — Under NoteTrack Gate (9544 B)

**Decision:** FractalTrack model must stay below 9544 B. The container gate is `NoteTrack` at 9544 B.

**Budget strategy:**
- No per-step probability grid (unlike StochasticSequence's 512 B per pattern).
- FractalSequence is minimal: divisor, scale, root, runMode, firstStep, lastStep (~20 B).
- Track-level params: complexity, patience, mutation, octaveShift, density, tilt, inputTrack1, inputTrack2, mutateFirst, mutateLast (~32 B).
- 17 sequences × ~20 B = 340 B + ~200 B overhead = ~540 B total.
- Well under the gate. No container inflation risk.

### KD-9: Engine Container Impact + RAM Allocation Policy

**Decision:** FractalTrackEngine must stay at or below the engine-union max (944 B = TT2TrackEngine). It is a union member in `CCMRAM_BSS`, so staying under that max means **zero net CCMRAM** (no additive ×8 allocation).

**RAM allocation policy:**
1. **Inline trunk, no heap:** the trunk is an inline `uint16_t` member (KD-2) — default 64 cells (128 B), max 128 cells (256 B), in the engine's CCMRAM slot. No allocator, no OOM path, no per-engine heap cap.
2. **One trunk per engine, not per pattern:** pattern switching changes config only; the single inline trunk is not swapped or reallocated. No 17× amplification.
3. **ARM `sizeof` probe required before Phase 4-5:** build a skeleton FractalTrackEngine on STM32 and report `sizeof` before adding deferred state (e.g. the KD-14b onset-phase cell widening, or MutationHistory if it un-defers). This validates the ~600 B estimate against the 944 B gate; if it's wrong, re-budget or cut.

**Engine members estimate:**
- TrackEngine base (~100 B)
- SequenceState, linkData, queues (~200 B)
- RNG, loop counters, boredom counter, trunk/branch counters (~50 B)
- MutationHistory: 16 records x ~16 B = ~256 B inline
- SelectionPressure mode + EvolutionDepth (2 B on model, 0 B in engine)
- Inline `uint16_t` trunk (KD-2): 128 cells × 2 B = 256 B, in the engine's CCMRAM slot
- **Target: ~600 B** — under the 944 B engine-union gate. (MutationHistory is deferred with the rest of the evolution subsystem; if it un-defers it adds ~256 B inline and must be re-probed against the gate.)

### KD-10: Mutation Zone — Scoped Transform Range

**Decision:** Add `mutateFirst` and `mutateLast` parameters that define the **mutation zone** — the subset of the loop buffer where destructive transforms (mutation, octave shift, boredom recapture) may operate. Steps outside the mutation zone are **anchors** — they always replay their recorded values unchanged.

**Why a separate range, not just loopFirst/loopLast:** `loopFirst`/`loopLast` define the **playback** window. Changing them changes what plays. A mutation zone is a **transform scope** within the playback window — all steps in the loop play, but only steps in the zone transform. Shrinking `loopLast` to "protect" steps would also stop playing them, which is not what the user intends.

**Model params:**
- `mutateFirst` (0..bufferLength-1): first step eligible for mutation. Default = `loopFirst`.
- `mutateLast` (0..bufferLength-1): last step eligible for mutation. Default = `loopLast`.

**Model cost:** 2 × `uint8_t` = 2 B. Negligible.

**What the mutation zone scopes:**
- **Mutation** (Phase 4): random step pick is constrained to `mutateFirst..mutateLast`. Anchor steps outside the zone are never selected for mutation.
- **Octave shift** (Phase 4): CV offset applies only to steps in `mutateFirst..mutateLast`. Anchor pitches are preserved.
- **Boredom recapture** (Phase 4): when `_autoCapturePending` fires, only overwrite steps in `mutateFirst..mutateLast`. Anchor steps keep their original recorded values.
- **Buffer math transforms** (post-MVP): transpose, invert, quantize, etc. all scoped to `mutateFirst..mutateLast` when the zone is active.

**What the mutation zone does NOT scope:**
- **Density masking** (Phase 5): operates on the full `loopFirst..loopLast` range. Density is a non-destructive replay mask — it doesn't modify the buffer, so anchors and mutable steps are equally eligible for thinning.
- **Recording** (Phase 2): recording writes to the full `loopFirst..loopLast`. Recording captures source output — it doesn't mutate, so the zone concept doesn't apply. The user explicitly chooses to record.
- **Playback** (Phase 2): the full `loopFirst..loopLast` plays. Anchors are audible.
- **Lock** (Phase 3): lock blocks ALL mutation regardless of zone. When locked, even the mutable zone is frozen.

**Default behavior:** `mutateFirst = loopFirst`, `mutateLast = loopLast`. When the zone equals the full loop, all steps are eligible — backward compatible with the Proteus "whole loop mutates" behavior. The user only constrains the zone when they explicitly want anchor sections.

**Invariant:** `loopFirst ≤ mutateFirst ≤ mutateLast ≤ loopLast`. The list model enforces this — mutating zone boundaries are clamped to the playback window. If the user changes `loopFirst` to a value above `mutateFirst`, `mutateFirst` snaps to the new `loopFirst`.

**Musical use cases:**
- 16-step loop with steps [0..3] as a fixed melodic motif, [4..15] evolving each loop
- 64-step loop with first 8 steps as a rhythmic anchor, rest mutating pitch
- Split loop: first half anchors, second half evolves — the anchor half provides harmonic context for the evolving half
- All-steps-anchor: set `mutateFirst = mutateLast` (single-step zone) and `mutationProb = 0` to freeze all step content without using lock. Unlike lock, this still allows recording. However, the idiomatic "freeze everything" approach is `lock = true`, not an empty zone.

**Relationship to Proteus anchors:** The Proteus spec describes "anchor notes" that survive regeneration (complexity low = repeat 1-2 anchors). The mutation zone IS the Proteus anchor mechanism made explicit: steps outside the zone are anchors that never change, steps inside evolve. The Proteus complexity knob then controls *how aggressively the mutable zone evolves*, while the zone itself controls *what is mutable*.

**Relationship to P.WTH (Tidal `within`):** The mutation zone is the Fractal equivalent of Tidal's `within(arc, transform)` and the Teletype `P.WTH start end transform` op. Instead of specifying the range per-transform, the range is a persistent model parameter. Post-MVP buffer math operations can also accept explicit per-call ranges that override the model zone.

---

### KD-12: Branches — Concatenated Chained Transforms (Post-Trunk), Bit-Word Path

**Decision:** Branches grow the played sequence by **concatenation** (Bloom). The trunk and `branchCount` generated branches play as **one long phrase**, then loop:

```
Trunk → B1 → B2 → … → BN → (loop)
```

Each segment is one loop-window length; total played length = loopLen × (1 + branchCount). No separate buffers — every branch is "previous segment + math transform" evaluated at playback from the single trunk buffer.

**Chained, not off-trunk.** Branch N transforms **branch N-1** (the trunk for B1), so transforms **compound** down the chain — each limb is a variation of the limb before it, the Bloom "watch it grow." (The prior design transformed every branch off the trunk; that is replaced.)

**Transform set (8 ops) — all deterministic.** Every branch is a pure transform of captured material; **no op generates content** (honouring the core constraint "no RNG content generation — all content comes from parent capture"). Branch pitch math is **chromatic** (raw semitones) — the trunk stays raw, scale is ornament-only. Each branch applies one of:
- **Transpose** (pitch): offset all CV by a **perfect 4th, 5th, or octave** (±5/±7/±12 semitones — consonant intervals only). Chained → a transposition ladder.
- **Reverse** (order): play the previous segment backwards. Self-inverse (toggles per branch).
- **Inverse** (pitch): mirror around a center note (`center - (cv - center)`). Self-inverse.
- **Retrograde-Inverse** (order+pitch): Reverse then Inverse — completes the twelve-tone row ops.
- **Rotate** (order): cyclic shift by k (`p → (p+k) mod len`) — a canon at a temporal offset. Chained → progressive rotation.
- **Interval-Compress** (pitch): scale each interval toward the center (`center + (cv-center)·f`, f<1). Chained → the melody progressively narrows.
- **Interval-Expand** (pitch): scale each interval away from center (f>1). Chained → progressively widens — the most fractal op.
- **Gate-thin** (rhythm): drop every Nth gate to a rest. Chained → progressively sparser.

**Mutate and Randomize are both excluded** — re-rolling CV invents notes the parent never played, which is RNG content generation and violates the mirror identity. Variety comes from the seeded *assignment*, the pool, the chain combinatorics, and the bit-word Path — never from inventing pitches.

**Transform assignment — generative, reseeded, pool-limited.** The engine assigns each branch's transform from an RNG seeded per track, **reseeding when the trunk is edited** (Bloom's regenerate-on-edit) — the chain grows on its own, not user-picked per branch. But the assignment draws **only from the enabled pool**: `branchPool` (uint8_t bitmask, one bit per op, default all-on) lets the user **constrain the palette** — e.g. pitch-only, or no rhythm changes. Empty pool falls back to Transpose. The seed is stored so the assignment is stable across power cycles until the trunk changes.

**Path — branch navigation order, as a bit-word.** Path sets the **order in which branches play** (Bloom's semantic — "the order in which the generative branches are navigated"), encoded as a bit-word: `path` (uint8_t), bit *k* = whether branch *k* is in the **outward leg** (0) or **held for the return leg** (1). The route is `Trunk → outward branches (ascending) → held branches (descending)`. all-0 = Forward Ladder `T B1 B2 … BN`; all-1 = Reverse Ladder `T BN … B1`; mixed = the orders between (e.g. B1 outward, B2/B3 held → `T B1 B3 B2`). The knob/CV sweeps `0 .. 2^branchCount − 1` → **2^N distinct orders** (7→**128**, matching Bloom). Every segment plays exactly once, so total length stays `loopLen × (1 + branchCount)`. The index *is* the route selector — no stored table, no LUT. This generalises the old named patterns (Forward/Reverse Ladder, etc.) into one CV-swept byte. **Routable.**

**Order vs Path — two orthogonal axes:**
- **Order** = how a single segment is read internally (Forward / Reverse / Pendulum / Random / Converge / Diverge / Page-Jump).
- **Path** = the across-segments **branch order** — which branch plays when (Trunk + outward-ascending + held-descending).

**Routable.** Both **Branch count** and **Path** are Routable destinations (CV / internal-mod, depth scaled to range — §6), matching Bloom's dedicated Branch / Path CV inputs. These plus Ornament rate + intensity (KD-13) are the four live performance controls.

**RAM impact:** Zero buffers. Model params: branchCount (uint8_t, 0-7), path (uint8_t bit-word), orderMode (uint8_t, 0-6), branchSeed (uint16_t — the generative transform assignment), branchPool (uint8_t bitmask — enabled transform palette). Branches evaluated at playback from the single trunk buffer.

### KD-13: Ornamentation — Per-Step Classical Flourishes (Post-Trunk/Branch)

**Decision:** Ornamentation applies per-step classical flourishes during playback, on top of whatever layer (trunk or branch) is playing. No storage — evaluated in the tick path.

**Two-step ornaments (anticipation, suspension, syncopation, octave/fifth up, half-turn toward/away):**
- Anticipation: play next step's note one eighth early
- Suspension: hold previous step's note for one eighth before current step's note
- Syncopation: rest for one eighth, then play current step's note
- Octave Up: play the octave above one eighth later
- Fifth Up: play the fifth above one eighth later
- Half Turn Toward: one eighth that goes one scale degree beyond the next note toward it
- Half Turn Away: one eighth that goes one scale degree away from the next note

**Four-step ornaments (run toward/away, turn, arp toward/away, mordent up/down):**
- Run Toward: four notes moving toward the next step's pitch
- Run Away: four notes moving away from the next step's pitch
- Turn: current note, one up, one down, then current again
- Arp Toward: arpeggiated chord toward the next step
- Arp Away: arpeggiated chord away from the next step
- Mordent Up/Down: rapidly alternate between current note and neighbor

**Max ornaments:** Full 8-step trills.

**User control — standalone, two Routable axes.** Decoupled from the (deferred) mutation system: `ornamentRate` (uint8_t, 0-100%) = P(fire) per gated cell — *how often*; `ornamentIntensity` (uint8_t, 0-100%) = the complexity tier — *how fancy* (off → 2-step → +4-step ≥40% → +8-step trill ≥75%). The two are independent, so flourishes can be elaborate-but-occasional (Bloom fuses both into one Mutate-knob zone; we split them). **Both are Routable** — with Branch count + Path (KD-12) they are the four live performance controls, a 1:1 with Bloom's Branch / Path / Mutate CV inputs (Ornament fills the Mutate slot, since mutation is deferred).

**Ornament eligibility zone.** Ornaments fire only when the sounding trunk cell falls within `ornFirst`/`ornLast` — `recordFirst ≤ loopFirst ≤ ornFirst ≤ ornLast ≤ loopLast ≤ recordLast`. The zone is defined on trunk within-positions, so the same "live region" stays expressive across every branch repetition (B1…BN). This was the mutation zone; with mutation/evolution deferred, it survives as the ornament eligibility region.

**Scale source — inherited, ornament-only.** FractalTrack inherits the **project Scale + `scaleRotate`** (a setup field, standard `-1`=inherit, like every other track — `selectedScale(projectScale, projectRotate)`). The scale is applied **only to ornaments**: the flourish notes (runs, arps, half-turns, mordents, octave/fifth, trills) move by **scale degrees** off it. The **trunk stays raw** — captured CV is never quantised (the looper mirrors the parent's literal output). A raw cell that lands off-scale references its **nearest scale degree** for the ornament steps; the main note still plays raw, only the ornament notes snap to the scale around it. "Toward/Away" walks scale degrees toward/away the next cell's nearest degree.

**RAM impact:** Negligible. Evaluated in tick path, no per-step storage. ~4 B model params (ornamentRate, ornamentIntensity, ornFirst, ornLast) + the inherited scale group.

### KD-14: Recording Features — Replace vs Latch, Punch-In, Loop Mode, Record Quantize

**Decision:** Four recording-focused features modeled after common Eurorack CV recorder patterns (Hermod Hard Rec/Rec Wait, Flame Quad's loop modes, o_C Hemisphere's quantized capture).

**Components:**

```
RecordMode: enum (2 bits on model)
  - Replace (0, default): every tick while armed, overwrite buffer[stepIndex] with source gate+CV.
  - Latch (1): only write to buffer when source gate=1. Silent steps keep previous content.
    Enables surgical per-step overdub without clearing the rest of the loop.

PunchMode: enum (1 bit on model)
  - Immediate (0, default): recording begins on first tick after recordArmed transitions false->true.
  - PunchIn (1): after arming, the engine enters a wait state (_punchWait=true) and continues
    playback. Recording releases on the first tick where resolved source gate=1. Matches Hermod
    "Rec Wait" behavior -- you arm, the loop keeps playing, and recording kicks in when the
    source fires a gate.

LoopMode: enum (2 bits on model)
  - Loop (0, default): at loopLast, wrap to loopFirst and continue playback.
  - Once (1): after reaching loopLast once, set _onceDone=true. Gates go off, step advancement
    stops. Transport restart or re-arming record resets _onceDone. Enables capture-one-loop workflow.

RecordQuantize: bool (1 bit on model)
  - Off (0, default): source CV written to buffer as-is (int16_t fixed-point).
  - On (1): source CV is quantized to the active FractalSequence scale before writing.
    Uses selectedScale().noteToVolts() or equivalent. Gate is unchanged. Ensures microtonal
    source CV (from TuesdayTrack, Teletype, LFOs) snaps to scale-degree grid on capture.
```

**Total model cost:** 6 bits (fits in 1 uint8_t). Two engine bools: `_punchWait`, `_onceDone`.

**Why these belong in Phase 2, not deferred:**
- RecordMode (Replace vs Latch) is a fundamental recording paradigm choice. Without it, every record pass clears the loop entirely. Latch makes the recorder usable for live performance where you want to replace specific steps.
- PunchIn makes recording interactive -- you can start a loop, hear it cycle, then trigger capture at the right moment.
- LoopMode=Once is essential for "capture one loop and hold" -- the most common recorder workflow. Without it, the buffer keeps repeating and you can't inspect a single captured pass.
- RecordQuantize ensures the captured loop is musical even when the source produces non-scale CV. This is especially important for Teletype and TuesdayTrack parents.

**Deferred:** Blend/overdub (weighted lerp, post-MVP). RecordQuantize-on-playback (post-MVP).

**Recording state machine (Phase 2):**
```
On tick() when recordArmed=true and not locked:
  if punchMode==PunchIn && _punchWait:
    // Keep replaying buffer, wait for source gate
    replay buffer[stepIndex] normally
    if resolvedSourceGate == true:
      _punchWait = false
      // Do NOT capture this tick -- let next tick capture
    advance stepIndex normally
  else:
    Read resolved source gate + CV
    if recordQuantize==On: snap CV to selectedScale
    if recordMode==Replace || (recordMode==Latch && sourceGate==true):
      Write gate + (quantized)CV to buffer[stepIndex]
      Set valid bit, clear slide bit
    advance stepIndex
```

**Playback after boundary (Phase 2):**
```
On loopLast -> loopFirst boundary:
  if loopMode==Once && _onceDone==false:
    set _onceDone=true   // gates off, stop advance
  if loopMode==Loop:
    wrap to loopFirst normally
```

### KD-14b: Two-Axis Capture Model — Cadence × Fidelity

> **Status: DEFERRED.** This pushes the fractal toward a *recorder*, which the design explicitly disclaims ("not a CV recorder, not a high-fidelity automation trace; intra-section motion is intentionally discarded"). Pinned here as a coherent future model, **not** in current scope. Build only if a feel-preserving transcriber is wanted alongside the mirror-looper.

The MVP capture is one S&H per fractal section, snapped to the cell. A fuller model is **two orthogonal toggles** that span "grid looper → faithful recorder," sharing the harmony track's Rings (`cv_scaler`) trigger code (§4 of harmony-spec.md):

**Axis 1 — Cadence (when a cell is written, `captureCadence`):**
- **Section** (default): the fractal-divisor strobe. A time **grid** — rests where the parent was silent. The looper identity.
- **Event (delta)**: capture on the parent's **hysteretic note-change** — Rings `cv_scaler` directional hysteresis (±⅓ semitone past a boundary) + an **inhibit window** (Rings `inhibit_strum_`). `recordPos` advances per *parent note event*, so the trunk becomes a compact **note-list** (no rests; glide/noise can't spray redundant cells). This is the harmony S&H trigger ported verbatim — no new theory.

**Axis 2 — Fidelity (what timing is kept, `captureFidelity`):**
- **Quantized** (default): snap the event to the cell; onset discarded.
- **Feel**: sample CV **at the gate rising edge** (also kills the boundary-aliasing where the section strobe lands on the wrong step) + store the **onset phase** within the section. Playback schedules the gate at `sectionStart + onsetPhase·sectionDur` → swing/microtiming reproduced. Captures the **first** edge per section only (multiple sub-section hits = a separate ratchet feature).

**The four corners:** section+quantized = today's grid looper · section+feel = grid rhythm with the parent's swing · event+quantized = clean note-list transcription · event+feel = faithful pitch + inter-note feel (most recorder-like).

**Cost:** Event cadence is ~free (reuses harmony's trigger). **Feel needs an onset-phase field** (~3-4 bits) — the 16-bit cell (11 CV + 4 gateLen + 1 valid) is full, so it must widen to 24/32-bit: +1 B × 64-128 cells. The trunk is an engine member in **CCMRAM** (not SRAM), so this lands on the roomier rail and only bites if the wider cell pushes the engine past the 944 B union max. That CCMRAM headroom is the gate on Feel mode.

### KD-15: Timing Alignment — Bar-Quantized Loop Length, Beat Offset, Track Delay

**Decision:** Add timing alignment features for locking loop length to bars, shifting loop position relative to the master transport, and microtiming nudging.

**Components:**

```
loopBars (uint8_t, 0-16, default 0):
  0 = manual mode -- loopLast is a direct model param editable by the user.
  Non-zero: auto-compute loop window length in bar units.
    loopLast = loopFirst + (loopBars * engine.measureDivisor() / divisor()) - 1
    Clamped to [loopFirst, bufferLength-1].
  Changing divisor or bufferLength auto-recomputes. User edits to loopLast
  are ignored while loopBars > 0 (the list model hides loopLast and shows
  loopBars instead).

beatOffset (int8_t, -16 to +16 beats, default 0):
  Shift the derived loop window by N beats relative to bar lines.
  Applied as: beatOffset * (measureDivisor() / 4) ticks added to loopFirst
  (and correspondingly to loopLast since the bar-derived length is constant).
  Positive shift = later in the measure (loop starts on later beats).
  Does NOT change loop length -- only slides the window position within the
  bar grid. Useful when recording from a parent track on a different beat
  alignment.

loopPhase (float, 0.0 to 1.0, default 0.0):
  Free rotation of playback start within the loop, as a fraction of loop
  length. Applied like CurveTrack `globalPhase`:
    float stepFraction = (stepIndex - loopFirst) / (float)(loopLast - loopFirst + 1);
    float phasedPos = fmodf(stepFraction + loopPhase, 1.0f);
    readStep = loopFirst + (int)(phasedPos * (loopLast - loopFirst + 1));
  At 0.0 = normal start. At 0.5 = start halfway through the loop. Wraps
  naturally. This rotates loop content, not output timing. Combined with
  `rotate` (integer step offset via SequenceUtils::rotateStep), gives
  full free-form rotation of the playback head. Matches CurveTrack's
  `globalPhase` pattern exactly.
```

**Rationale:** Eurorack CV recorders commonly offer bar-quantized loop lengths (Flame "record mode" ties to gate length) and beat-aligned start points. Phase rotation lets the user freely adjust where the loop starts in the cycle. CurveTrack already proves this pattern with `globalPhase`; FractalTrack adopts it directly. `rotate` handles coarse integer-step shifts, `loopPhase` handles continuous fractional rotation. Together they replace the need for a fixed tick delay.

**Model cost:** loopBars (1 B), beatOffset (1 B), loopPhase (4 B) = 6 B total. No engine state beyond reading them. Negligible.

**Timing computation flow (Phase 2):**
```
On tick() at step boundary:
  1. Resolve effective loop window:
     if loopBars > 0:
       effectiveLen = (loopBars * engine.measureDivisor()) / divisor
       winFirst = loopFirst + (beatOffset * measureDivisor / 4)
       winLast  = winFirst + effectiveLen - 1
       clamp winFirst/winLast to [0, bufferLength-1]
     else:
       winFirst = loopFirst, winLast = loopLast (direct from model)

  2. Apply phase rotation to step index lookup:
     float stepFraction = (stepIndex - winFirst) / (float)(winLast - winFirst + 1);
     float phasedPos = fmodf(stepFraction + loopPhase, 1.0f);
     int readStep = winFirst + (int)(phasedPos * (winLast - winFirst + 1));

  3. Process readStep within [winFirst..winLast]:
     recording or replay per existing Phase 2 logic
     (phase rotation applies to playback reads; on record, write to
      stepIndex normally, but readStep determines which step is heard)

  4. Schedule output normally:
     gateQueue.push({tick, gateState})
     cvQueue.push({tick, cvValue})
```

**Interaction with existing features:**
- rotate is applied on top of the bar-derived window: beatOffset shifts the window, rotate + loopPhase shifts the playback start within it. rotate provides integer-step rotation, loopPhase provides continuous fractional rotation. Both can be active simultaneously.
- loopMode=Once works normally: after one pass of the derived window, _onceDone fires.
- PunchIn is unaffected: punch still fires on first source gate after arming, regardless of where the bar-aligned window starts.
- resetMeasure (from FractalSequence) is unrelated: it resets the sequence on bar-boundary transport.

### KD-16: Separate Recording Extent from Loop Window

**Decision:** Add `recordFirst`/`recordLast` as the recording target window, distinct from `loopFirst`/`loopLast` (playback window). Based on the Compass norns looper pattern where `sPoint`/`ePoint` define the recording extent and `loopStart`/`loopEnd` define the active loop subset.

**Components:**
```
recordFirst (uint16_t, default 0): first step of the recording extent.
recordLast (uint16_t, default bufferLength-1): last step of the recording extent.
```

**Behavior:**
- Recording engine writes to `recordFirst..recordLast` and wraps from `recordLast` back to `recordFirst`.
- Playback reads from `loopFirst..loopLast` (existing param, subset of record extent).
- Clear buffer clears `recordFirst..recordLast` only, not the full buffer.
- `loopBars` mode derives loop window relative to `recordFirst`.
- Loop window cannot extend outside the record extent.

**Invariants:**
- `recordFirst <= loopFirst <= loopLast <= recordLast <= bufferLength-1` (enforced by list model).
- Steps outside the loop window within the record extent are stored but muted during playback.
- Recording always targets `recordFirst..recordLast` — you cannot record outside the extent.

**Model cost:** recordFirst (uint16_t), recordLast (uint16_t) = 4 B.

**Rationale:** Without this, the user records into the full buffer and can only loop the whole thing. Separating recording extent from loop window lets you record a long 256-step passage and loop only bars 1-4, or record 8 bars and loop a specific 2-bar section. Compass proves this pattern works well with its `sPoint`/`ePoint` vs `loopStart`/`loopEnd` distinction.

**Phase:** Phase 3 (Loop Controls — windowing). Phase 2 uses `recordFirst=0, recordLast=bufferLength-1` (legacy default). Phase 3 adds the params and the extent-constrained loop window.

### KD-17: ClockSource — CV-Driven Playback Step (DiscreteMap Pattern)

**Decision:** Add a `ClockSource` enum to `FractalSequence` (Internal/External), matching DiscreteMapSequence's pattern. When External, a routed CV directly determines the playback step position within the loop window — no clock advancement, no divisor, no PlayMode.

**Components:**
```
FractalSequence::_clockSource (uint8_t):
  Internal (default): step advancement via clock ticks. PlayMode (Aligned/Free) applies normally.
  External: step position = floor(routedScan). PlayMode and divisor are ignored.

FractalTrack::_routedScan (Routable<float>): CV source for External mode.
```

**Engine behavior (Phase 2):**
```
if clockSource == External:
  float scanCv = track.routedScan()
  int step = winFirst + int(clamp(scanCv, 0.f, float(winLast - winFirst)))
  // Edge detection: DiscreteMap scanner pattern
  if step != _lastScanStep:
    _lastScanStep = step
    triggerStep(tick, step)
else:
  // existing clock-based step advancement (Aligned/Free)
```

**Invariants:**
- External mode bypasses ALL clock-based step machinery: no divisor, no phase accumulator, no bar alignment, no PlayMode.
- External mode uses the DiscreteMap floor-truncation pattern: `int(cv)` maps the continuous CV range to discrete steps. Edge detection fires triggerStep once per crossing.
- Recording still uses clock-relative step position in both modes. External mode affects playback only.
- `routedScan` is an independent routing target, not sharing infrastructure with sourceA/sourceB.
- When External is active, the list UI hides divisor, gateLength, and PlayMode (they have no effect).

**Model cost:** clockSource (1 B, within existing FractalSequence budget), Routable<float> on FractalTrack (8 B). Engine state: _lastScanStep (int8_t, 1 B). Negligible.

**Rationale:** DiscreteMapTrack already proves this pattern — its `clockSource == External` branch reads `getRoutedInput()` and maps to stage position directly, ignoring PlayMode entirely. For FractalTrack, External mode turns the loop buffer into a CV-indexed wavetable: any CV source (LFO, envelope, sequencer, modulation track) directly selects the playback step. This enables tape-head-style scanning, LFO-driven wobbly loops, or step selection from a master track's CV output.

**Phase:** Phase 1 (model: clockSource enum, routedScan routing target) + Phase 2 (engine: External branch in tick()).

## Phased Implementation Plan

### Phase 1: Model & Serialization

**Goal:** `FractalTrack` and `FractalSequence` in the Track container, serializable.

**Files:**
- NEW `model/FractalSequence.h` — minimal sequence: divisor, scale, root, runMode, firstStep, lastStep, playMode, clockSource, resetMeasure (~22 B).
- NEW `model/FractalTrack.h` — 17 sequences + track params (MVP core only): sourceA, sourceB, gateLogic, cvLogic, bufferLength, recordArmed, recordMode, punchMode, loopMode, recordQuantize, lock, loopFirst, loopLast, loopBars, beatOffset, loopPhase, rotate, complexity, patience, mutationProb, octaveShiftProb, density, tilt, slideTime, octave, transpose, routedScan. (mutateFirst, mutateLast deferred to Phase 3. evolutionDepth, pressureMode deferred to Phase 4. trunkCycles, branchCount, pathType, orderMode, branchTransformFlags deferred to Phase 5. ornamentProb, ornamentMode deferred to Phase 5.)
- EDIT `model/Track.h` — add `Fractal` to TrackMode enum, Container, union, accessors, initContainer.
- EDIT `model/Routing.h` — add `FractalFirst..FractalLast` routing targets.
- EDIT `engine/Engine.h` — add `FractalTrackEngine` to TrackEngineContainer typedef.
- EDIT `engine/Engine.cpp` — add `case TrackMode::Fractal` to creation switch.

**Verification:** STM32 build compiles, `sizeof(FractalTrack)` probe.

### Phase 2: Engine Foundation — Record & Loop + Minimal List UI (Before Hardware)

**Goal:** Record CV/Gate from 1-2 parent tracks into the inline trunk buffer (KD-2), play it back on loop. **All engine features immediately accessible from existing TrackPage list view.**

**Files:**
- NEW `engine/FractalTrackEngine.h`
- NEW `engine/FractalTrackEngine.cpp`

**Core behavior:**
- `tick()`: resolve parent engine pointers, read `gateOutput(0)` and `cvOutput(0)`.
- **Clock source check (new):** If `clockSource == External`, read `_fractalTrack.routedScan()`, map to step via `winFirst + int(clamp(scanCv, 0, winLen-1))`. Edge detection fires triggerStep on step change. Bypass all clock-based advancement, divisor, and PlayMode.
- **Timing computation (new):** If `clockSource == Internal` and `loopBars > 0`, derive effective loop window: `winLast = loopFirst + (loopBars * engine.measureDivisor() / divisor) - 1`, apply `beatOffset` shift to both winFirst and winLast, clamp to buffer bounds. Otherwise use `loopFirst`/`loopLast` directly. Apply `loopPhase` for fractional rotation of the playback read position: `phasedPos = fmodf(stepFraction + loopPhase, 1.0f)` maps to a readStep within the window. Record writes go to stepIndex normally; readStep determines what is heard on output.
- When recording (recordArmed=true, not locked):
  - PunchIn wait: if punchMode=PunchIn and `_punchWait` is set, replay buffer normally and don't record until source gate fires.
  - Otherwise: write source gate+CV into `_trunk[cell]` (raw, S&H). If recordMode=Latch, only write when source gate=1.
- When playing: read and decode `_trunk[cell]` — extract CV, gateLen, valid via shift+mask.
- Playback boundary: if loopMode=Loop, wrap derived loopLast to derived loopFirst. If loopMode=Once, set `_onceDone`, gates off, stop advance.
- Sequence advancement: when clockSource==Internal, follows aligned/free playMode pattern (StochasticTrackEngine pattern). When External, no advancement — step position comes from routedScan CV.
- Gate/CV queue system (same as all engines).

**Key functions:**
- `triggerStep()`: if recording, capture current parent output into buffer; if playing, replay from buffer.
- `reset()`: reset sequence state, clear queues. Do NOT clear buffer on transport reset — only on explicit "clear" action.
- `update()`: slide interpolation, monitoring.

**Cycle safety:** Engine checks source track indices < Fractal track index at creation time. Warns and treats invalid sources as idle.

**List UI integration (Phase 2, not deferred):** `FractalTrackListModel` with MVP core params immediately: Clock Source (Internal/External), PlayMode, Divisor, Scale, Root, Source A/B, Gate/CV Logic, BufferLength, Record Arm, Clear Buffer (action), Record Mode (Replace/Latch), Punch Mode (Immediate/PunchIn), Loop Mode (Loop/Once), Rec Quantize (Off/On), Lock, Density, Tilt, Loop First/Last, Loop Bars, Beat Offset, Loop Phase, Rotate, SlideTime, Octave, Transpose. When ClockSource=External, divisor, PlayMode, and gateLength are hidden (they have no effect). When LoopBars > 0, Loop Last is derived and hidden from the list UI (LoopBars displayed instead). This list lives in TrackPage.cpp's existing list routing — no new page type needed. Mutation/evolution params (Complexity, Patience, MutationProb, OctaveShiftProb) are also included from Phase 1 since they're model fields.

**Verification:** STM32 build, manual test via list UI: assign NoteTrack as parent, record one loop, hear it repeat.

### Phase 3: Loop Controls — Windowing, Rotation, Lock, Mutation Zone

**Goal:** Loop windowing (first/last step), rotation, lock (freeze buffer from further mutation/recording), and mutation zone (scoped transform range).

**Model additions:**
- `loopFirst`, `loopLast` on FractalTrack (already planned).
- `recordFirst`, `recordLast` — recording extent (KD-16). Default `recordFirst=0`, `recordLast=bufferLength-1`. Recording wraps within extent, not full buffer.
- `lock` bool — when true, buffer is read-only; mutations and recording are blocked.
- `rotate` — address rotation over `loopFirst..loopLast`.
- `mutateFirst`, `mutateLast` — mutation zone boundaries (KD-10). Default to `loopFirst`/`loopLast`. Extent invariant: `recordFirst <= loopFirst <= mutateFirst <= mutateLast <= loopLast <= recordLast <= bufferLength-1`.

**Engine changes:**
- Recording targets `recordFirst..recordLast`. Record position wraps from `recordLast` to `recordFirst`.
- `triggerStep()` respects lock — if locked, replay only, no writes.
- Rotation applied via `SequenceUtils::rotateStep()` (existing).
- Mutation zone boundaries stored on model, read by mutation logic in Phase 4.
- Clear buffer clears `recordFirst..recordLast` only, not the full buffer.

**Verification:** Record into long buffer, narrow loop window to subset, confirm playback is constrained. Extend loop window across record extent boundary — confirm list model enforces constraint.

### Phase 4: Proteus Mutation — Loop-Boundary Lifecycle

**Goal:** Add Proteus-inspired mutation/evolution that modifies the captured buffer at loop boundaries.

**Model additions:**
- `complexity` (0–100): controls how much the buffer changes on regeneration.
  - Low: repeat 1–2 anchor notes, minimal variation.
  - Mid: prefer adjacent scale-degree runs.
  - High: wider weighted jumps, full algorithmic range.
- `patience` (0–100): boredom timer. At each loop boundary, compute `P_reset = f(loopCount, patience)`. At max patience, never reset.
  - Ramp function: linear `P_reset = loopCount × k × (100 - patience) / 100`.
- `mutationProb` (0–100): per-loop probability of re-rolling one random buffer index through current Complexity/Scale.
- `octaveShiftProb` (0–100): per-loop probability of shifting entire buffer ±12 semitones, constrained to ±1 octave from original base.

**Engine changes:**
- At loop boundary (when step wraps from `loopLast` to `loopFirst`):
  1. Roll boredom reset. If triggered, set internal engine flag `_autoCapturePending = true`. This does NOT touch the user-facing `recordArmed` — boredom is an internal lifecycle event. On the *next* loop pass, the engine auto-captures source output into the buffer **only for steps in `mutateFirst..mutateLast`** (KD-10), then clears `_autoCapturePending`. Anchor steps outside the zone keep their original values. The user's manual record arm is unaffected.
  2. Roll mutation. If triggered, pick one random step in **`mutateFirst..mutateLast`** (not the full loop — KD-10 mutation zone), re-generate its **CV only** from current scale + complexity. Gate and valid bit are NOT changed by mutation — a muted step stays muted, an active step stays active. Mutation only repitches existing notes. Anchor steps outside the zone are never selected.
  3. Roll octave shift. If triggered, offset all buffer CV values **in `mutateFirst..mutateLast`** by ±1V (±12 semitones), clamped. Anchor steps outside the zone are not shifted.
- Complexity governs the pitch selection when mutating: use the existing Stochastic degree-ticket raffle or simplified adjacent-degree preference.

**Key decision point:** Should mutation re-use the Stochastic track's degree-ticket system, or have its own simplified pitch selection?

**Recommendation:** Start with simplified: complexity = 0 anchors current note, complexity = 50 prefers ±1 scale degree, complexity = 100 allows full range. Can adopt degree tickets later as an advanced option.

**Verification:** Set moderate patience + mutation, observe buffer evolving over multiple loops. Confirm lock prevents mutation.

### Phase 5: Branches + Ornamentation + Density

**Goal:** Branch transforms (trunk+math at playback), ornamentation (per-step flourishes), and deterministic density masking.

**Branches (KD-12):**
- `branchCount` (0-7): how many branch transforms to cycle through
- `branchTransformFlags` (uint8_t bitmask): enable transforms per-branch
- `trunkCycles` (1-16): how many loop reps of trunk before switching to branches
- `pathType` (0-7): navigation path from curated LUT
- `orderMode` (0-6): Bloom playback order (Forward, Reverse, Pendulum, etc.)
- Engine playback: during trunk phase → read trunk buffer; during branch phase → read trunk buffer + apply current branch's transform
- No buffer mutation during branch phase — transforms are render-time only

**Ornamentation (KD-13):**
- `ornamentProb` (0-100%): per-step ornamentation likelihood
- `ornamentMode`: 2-step / 4-step / max (trills) / off
- Tick-path evaluation: after trunk or branch resolves CV+gate, ornamentation may override CV or advance playback by 1-8 micro-steps
- Reuses existing gate queue (same as ratchet/trill scheduling)

**Density (moved from old Phase 5, unchanged):**
- `density` (0-100), `tilt` (-100 to +100): deterministic rest-priority thinning
- Non-destructive: reads `_gateBitmap`, writes to per-tick replay mask
- Replay gate-off on muted steps

**Deferred:**
- Micro Mutate Mode (per-step ratchet/slew/mod probability mutation) — post-core
- Performance Mode (non-destructive per-step ratchet/slew/ornamentation effects) — post-core

**Verification:** Sweep density 100→0, confirm step drop. Set branchCount=3, cycle through trunk+3 branches. Enable ornamentation, confirm flourishes on active steps.

### Phase 6: Visual UI Pages (Post-Hardware Validation)

**Goal:** Visual pages for Fractal Track in the Performer UI.

**Pages (following existing patterns):**
- `FractalTrackListModel` — track setup: playMode, input tracks, divisor, scale, root, loop window.
- `FractalBufferPage` — visual display of captured CV/Gate buffer, loop window, lock state.
  - **Compass loop bar:** 128px horizontal bar representing `loopFirst..loopLast`. Start/end markers as bright vertical ticks. Current playhead position as a tall tick. Density-affected steps dimmed (non-destructive replay mask visible). Lock indicator and record arm indicator in the same view. Loop window markers at loopFirst/loopLast; record extent shown as secondary brackets.
- `FractalMutationPage` — complexity, patience, mutation, octave shift controls.
- `FractalPerformancePage` — density, tilt, overdub toggle, record arm.

**Integration points:**
- `TrackPage.cpp` — route to FractalTrackListModel.
- `TopPage.cpp` — route to FractalTrack pages.
- `Pages.h` — register pages.

**Verification:** Full UI flow: set parent tracks, record, view buffer, adjust mutations, observe changes.

### Phase 8: Validation & Polish

**Goal:** STM32 release validation, RAM probe, hardware testing.

**Checks:**
- `sizeof(FractalTrack)` under 9544 B.
- `sizeof(FractalTrackEngine)` under the 944 B engine-union gate (TT2TrackEngine).
- Inline trunk (CCMRAM): 64 cells × 2 B = 128 B default, 128 cells = 256 B max.
- Full `.data + .bss` stays under 120 KB.
- Config serialization round-trip (model params only — buffer is volatile).
- Routing CV modulation of mutation/density/patience.

---

## Reference Porting Analysis

Sources audited: `temp-ref/o_c/O_C-Phazerville/` (applets, enigma, util), `temp-ref/norns/` (tulpamancer, thirtythree, spirals, zxcvbn), `temp-ref/vinx-performer/` (LogicTrackEngine), `paste_1.txt` (Proteus spec).

### A. Directly Portable — Adopt in MVP

| Source | Mechanism | Fractal Application | Porting Notes |
|--------|-----------|---------------------|---------------|
| **Vinx LogicTrackEngine** | Per-tick resolution of 2 parent engines via `_engine.trackEngine(index)`. Gate logic: One/Two/And/Or/Xor/Nand/Random. Note logic: Min/Max/Sum/Avg/Random. | **Input mixing KD-1.** Port the `evalStepGate` and `evalStepNote` gate/note logic modes for combining 2 master tracks. Simplified: since Fractal reads raw CV/Gate output (not step data), the logic applies to *output* gate/CV rather than step parameters. | LogicTrackEngine.cpp lines 469–556 (gate logic), 83–161 (note logic with variation). Remove step-data dependencies, operate on `gateOutput()` and `cvOutput()` directly. |
| **Proteus spec (paste_1.txt)** | §3.1 Complexity: low = repeat 1-2 anchors, mid = prefer n±1 runs, high = full weighted jumps. §3.2 Patience: P_new ramps linearly over loopCount. §3.3 Density: deterministic rest-priority list. §3.4 Mutation: per-loop reroll of 1 index. Octave: ±1 constrained. | **Phase 4 lifecycle (KD-3).** Direct adoption. Complexity weights map to mutation pitch selection only. Patience = loopCount × k × (100 - patience) / 100. Density = already implemented in Stochastic. Mutation = persistent engine RNG reroll. | Already covered in KD-3 and Phase 4. No additional porting needed. |

### B. Strong Candidates — Adopt Post-MVP

| Source | Mechanism | Fractal Application | Why Post-MVP |
|--------|-----------|---------------------|-------------|
| **o_c Palimpsest** | Compose/Decompose: brush input *adds* energy to a step on gate, *subtracts* on no-gate. Pure overdub with accumulation. | **Blend overdub mode (KD-5 post-MVP).** Replace Palimpsest's `compose/decompose` rates with Fractal's blend weight. Each recording pass lerps between old and new CV: `newCV = lerp(oldCV, sourceCV, blendAmount)`. Gate: OR of old and new. Valid: sets bits, never clears. | KD-5 explicitly defers blend to post-MVP. MVP is overwrite-only. Palimpsest-style additive accumulation is rejected for V/Oct pitch (see KD-5), but the compose/decompose *rates* can control blend weight. |
| **o_c ShiftReg / TuringMachine** | Probabilistic bit-flip shift register. `AdvanceRegister(prob)`: flip LSB with probability `p`, shift left, wrap MSB to LSB. | **Correlated mutation engine.** Replace per-loop random index with a Turing shift register. Register bits select mutation targets across the buffer, creating correlated drifting patterns instead of independent random picks. | KD-6 explicitly defers Turing DNA. Adds ~12 B to engine (uint64_t register + uint8_t length + uint8_t probability). `util_turing.h` already self-contained. Adopt after MVP proves the basic loop recorder. |
| **o_c RunglBook** | 8-bit shift register with threshold-comparison input. Gate input freezes (rotate-only). Two taps at bits 0-2 and 5-7. | **Deterministic mutation when locked.** Freeze behavior maps to Fractal lock — locked register rotates without new input, mutations repeat. | MVP lock simply blocks all mutation; RunglBook adds deterministic rotation while locked. Extremely compact (1 `uint8_t` register). Deferred until Turing DNA is adopted. |
| **o_c LogisticMap** (`util_logistic_map.h`) | Deterministic chaotic oscillator: `x_{n+1} = r × x_n × (1 - x_n)`. r ∈ [3.0, 4.0) produces chaotic bifurcation. Fixed-point (int64, Q24). | **Complexity replacement.** Instead of a linear complexity knob, use logistic map output as the mutation CV source. `r` = complexity parameter: low r → stable fixed point (repetition), high r → chaos (wild mutation). Musical: the bifurcation cascade creates natural "increasingly varied" behavior. | Needs integration with scale quantizer. The Q24 fixed-point math is proven on STM32 but adds complexity. Start with simplified linear complexity. |
| **o_c GameOfLife** | Conway's GoL on a 64×40 toroidal board. Global density → CV out. Local density (8×8 neighborhood) → second CV out. | **Neighborhood-aware mutation.** Instead of mutating random single indices, use a 1D CA or 2D GoL rule over the loop buffer. Alive cells = active steps, dead = rests. The CA evolves the *structure* of the loop, not just individual notes. | RAM concern: 80 × `uint64_t` = 640 B for a 64×40 board. A 1D CA (64-cell row) would be only 8 B. Start with simpler per-index mutation, add CA as an evolution mode. |
| **o_c PatternPredictor** (`util_pattern_predictor.h`) | Finds the best periodic match in a history buffer. Uses autocorrelation over candidate periods 1–8. Returns prediction or raw value if prediction was bad. | **Smart boredom.** Instead of a blind linear patience ramp, use PatternPredictor to detect when the loop output becomes "predictable" (high autocorrelation). Boredom = measured predictability, not just time. This makes the patience system responsive to musical content, not just clock count. | 32 × `uint32_t` = 128 B history. Computationally light. But the concept needs careful UX design — "boredom" as a measurable quantity is a different mental model than a knob. |
| **Norns Spirals** | Golden-ratio spiral: angle += 2π × 0.618, radius += 0.2. Note index = `ceil(angle / rads_per_note)`. Lock mode loops a subsequence of generated points. | **Spiral mutation mode.** When mutating a buffer index, instead of random pitch selection, walk along a golden spiral through the scale degrees. Locked spirals = repeatable melodic contours that evolve slowly. | Requires float trig (cos/sin). The `angle → scale degree` mapping is elegant but may be over-engineering for a first pass. Good "organic mutation" alternative to purely random. |
| **o_c Pigeons** | Modular arithmetic: `val[index] = (val[0] + val[1]) % mod_v`. Bump alternates which slot gets updated. Quantized output. | **Harmonic mutation constraint.** Use modular arithmetic to constrain mutations: `newNote = (baseNote + mutationOffset) % scaleSize`. The `mod_v` parameter creates harmonic windows — small mod = constrained within a few notes, large mod = full range. | Simple enough for MVP, but the two-value alternating pattern is more useful as a dedicated feature than a general mutation mechanism. Consider as a "harmonic window" parameter. |

### C. Architectural Patterns — Design Influence Only

| Source | Pattern | Fractal Application |
|--------|---------|-------------------|
| **Norns Thirtythree** | Operator model: each operator has sounds, patterns (16 steps × 16 sounds), parameter locks, FX locks. Per-step lock overrides base parameters. | **Parameter lock inspiration.** The Fractal buffer could support per-step CV offset locks (like plocks). Instead of recording raw CV, record *base CV + per-step offsets*. Locks can then be mutated independently of the base material. |
| **Norns Tulpamancer** | Exogenous seed: fetches real-world data (stock volumes), converts to binary string, plays as gate pattern. Refreshes periodically. | **External seed concept.** The Fractal track's mutation seed could be derived from something other than the PRNG — e.g., the master track's step data hashed into a seed. This creates a deterministic relationship between parent and child. |
| **Norns Zxcvbn** | TLI text-based patterns + chain expressions (`a*4 b*2 c*3`). LFOs as modulation shortcodes. | **Chain/arrangement concept.** The Fractal track could chain multiple loop buffers (like pattern chains) with different mutation settings per segment. This is the "song mode" for evolving loops. Post-MVP. |
| **o_c TLNeuron** | Weighted sum of 3 dendrite inputs with threshold firing. Binary output. | **Gate mixing model.** The Fractal track's 2-input gate mixing could use TLNeuron's weighted threshold model: `gateOut = (w1 × gate1 + w2 × gate2 + w3 × cv1_threshold) > threshold`. More expressive than boolean logic. |
| **o_c LowerRenz (Lorenz)** | Lorenz attractor with frequency and rho parameters. Freeze = gate hold. X/Y outputs. | **Chaotic drift mode.** Instead of Proteus-style discrete mutation events, use a Lorenz attractor to *continuously* drift the loop's CV values. Creates smooth organic evolution vs. sudden jumps. |

### D. Explicitly Excluded

| Source | Why Excluded |
|--------|-------------|
| **o_c Cumulus** (accumulator) | 8-bit accumulator math is interesting but too low-level for the Fractal concept. The Fractal buffer stores V/Oct CV, not binary data. Accumulator feedback would require a CV→binary→CV round-trip that loses resolution. |
| **o_c Trending** (CV trend detection) | Useful concept but requires continuous CV sampling at audio-rate granularity. The Fractal track operates at step-clock resolution, not continuous. Would need a fundamentally different tick architecture. |
| **o_c Enigma (full Turing Machine app)** | 40 stored Turing Machines, song chains, SysEx backup — too heavy. Fractal adopts only the core shift-register mechanism from `util_turing.h`, not the full library/chain/backup system. |
| **Norns Thirtythree (softcut/recorder)** | Audio-rate sample recording and playback via SuperCollider. Fractal records step-rate CV/Gate, not audio. The recorder.lua's 60-second tape is irrelevant to the Fractal buffer model. |

---

## Porting Priority for Each Phase

| Phase | Ported Feature | Source | Effort |
|-------|---------------|--------|--------|
| **Phase 2** | 2-input gate/CV mixing | Vinx LogicTrackEngine | Low — adapt existing gate/note logic to output-reading |
| **Phase 3** | Loop freeze/rotate (bool flag) | Fractal-specific | Trivial — lock flag + existing rotation |
| **Phase 4** | Per-loop mutation bias via SelectionPressure + EvolutionDepth | Evolution system (KD-6) | Low — history 16 x 16 B inline, bias function per mode |
| **Phase 4** | Complexity -> mutation weights | Proteus Sec3.1 | Low — 3-tier weight table |
| **Phase 4** | Patience boredom ramp | Proteus Sec3.2 | Low — linear ramp formula |
| **Phase 5** | Branch transforms + Path LUT | Bloom v2 (KD-12) | Low — playback-time math on trunk, zero buffer storage |
| **Phase 5** | Ornamentation per-step | Bloom v2 (KD-13) | Low — tick-path eval, ~2 B model params |
| **Phase 5** | Deterministic density masking | StochasticTrackEngine | Low — copy evalDensityMask |
| **Phase 5+** | Blend overdub (weighted lerp) | o_c Palimpsest compose/decompose rates | Low — lerp with blend weight param |
| **Phase 5+** | Turing shift register mutation | o_c ShiftReg / util_turing.h | Medium — 12 B added to engine |
| **Phase 5+** | RunglBook freeze behavior | o_c RunglBook | Low — deterministic rotation when locked |
| **Phase 5+** | Logistic-map complexity | o_c util_logistic_map.h | Medium — Q24 fixed point + scale quantizer |
| **Phase 5+** | 1D CA evolution mode | o_c GameOfLife (simplified to 1D) | Medium — 64-bit row, rule evaluation |
| **Phase 6+** | Pattern-aware boredom | o_c PatternPredictor | Medium — 128 B history, autocorrelation |

---

## Future Research (Compass Norns)

The following Compass-derived features are noted for future consideration, not in any phased plan:

**Command palette for ornament dispatch (future):** Compass's 18-command palette with enable/disable per step suggests a similar approach for ornamentation. Instead of the global `ornamentRate`/`ornamentIntensity`, each cell could index a palette of ornament types (anticipation, suspension, mordent, run, etc.) — making ornamentation cell-accurate rather than probabilistic. **Model cost:** the 16-bit trunk cell is full (11 CV + 4 gateLen + 1 valid), so a per-cell palette index would require widening the cell — the same CCMRAM cost gated under KD-14b's Feel mode.

**Discrete rate table (Phase 5+):** Compass uses 6 discrete rates {-2, -1, -0.5, 0.5, 1, 2} with slew smoothing. FractalTrack could support a playback speed multiplier, enabling half-speed (2 octaves down), double-speed (1 octave up), or reverse-speed loop playback. **Model cost:** uint8_t rate index (1 B). **Engine cost:** float accumulator per engine (4 B). **Trade-off:** Speed modulation changes the step-time relationship — the FractalTrack tick fires at the master clock rate, so speed would skip or repeat buffer reads, not change the recording. This is a fundamentally different timing model than the current step-aligned approach.

**Grid gesture recording (Phase 6+):** Compass records grid touches (x,y positions) into 8 pattern slots and replays them. The grid's row 4-5 touches correspond to buffer position, while row 7 touches advance the command sequencer step. FractalTrack has no grid hardware, but the *concept* of recording gestural inputs as a pattern layer applies: a Performer encoder gesture (knob turn direction, speed, duration) could be recorded as a modulation track over the FractalTrack buffer. This is architecturally closer to the existing accumulator system than to FractalTrack.

---

## Open Questions (Decide Before Phase 4)

1. **Patience ramp function:** Linear or exponential? Linear is simpler and predictable. Exponential creates longer "plateaus" before sudden change. **Recommendation: Linear for MVP.**
2. **Mutation pitch source:** Simplified complexity-based selection (anchor/adjacent/full) or full degree-ticket integration from Stochastic track? **Recommendation: Simplified for MVP, tickets as upgrade.**
3. **Scale awareness:** Should mutation snap to the project scale, or the parent track's scale? **Recommendation: FractalSequence has its own scale/root (like Stochastic), defaulting to project scale.**
4. **Buffer clear trigger:** Only via explicit user action, or also on pattern change? **Resolved:** Buffer is volatile engine state. Clear on: track mode change, buffer length change, explicit clear, power cycle. Survives transport reset and pattern change.
5. **Overdub semantics:** **Resolved:** MVP is overwrite only (KD-5). Post-MVP blend mode uses weighted lerp (NOT additive CV accumulation, which breaks V/Oct pitch). Palimpsest-style compose/decompose rates control blend weight.
6. **Turing DNA timing:** **Resolved:** KD-6 uses evolution system (history + pressure + depth) for mutation step selection, not purely random RNG. Turing shift register deferred to post-MVP (~12 B added to engine when adopted) for correlated drift patterns only.
7. **UI before hardware:** Phase 6 (minimal list UI) must come before Phase 8 (hardware validation). Lesson from Stochastic: no manual testing without at least a list model.
8. **Buffer persistence:** **Deferred to post-MVP.** MVP uses volatile engine buffer (not serialized). Options for persistence (per-pattern, per-track, external import) will be decided after the volatile engine proves the concept. Affects project file format, flash wear, and RAM budget.
9. **Branches vs Post-MVP:** **Resolved:** Branches (KD-12) are Phase 5, not post-MVP. Trunk+math transforms at playback time with zero buffer storage.
10. **Ornamentation vs Post-MVP:** **Resolved:** Ornamentation (KD-13) is Phase 5, not post-MVP. Per-step flourishes evaluated in tick path, ~2 B model params.
11. **Evolution system vs Persistent RNG:** **Resolved:** KD-6 replaces old "persistent RNG only" with MutationHistory + SelectionPressure + EvolutionDepth. Persistent RNG is preserved for underlying rolls but step selection is biased by history.

## Resolved Decisions (Audit Rounds 4-5)

| # | Issue | Resolution |
|---|-------|-----------|
| 1 | Buffer ownership/persistence | Volatile engine state, not model state. Not serialized. Clear on mode/length/explicit/power change. |
| 2 | Mutation RNG contradiction | Evolution system (KD-6) replaces purely random step selection. Persistent RNG preserved for underlying rolls. |
| 3 | Boredom mutating user recordArmed | Internal `_autoCapturePending` flag. User `recordArmed` is never touched by boredom. |
| 4 | 17 patterns × buffer explosion | One volatile buffer per engine, not per pattern. Pattern change = config only. |
| 5 | Reference table contradicted MVP | Palimpsest, Turing, RunglBook moved to Section B (post-MVP). Porting priority table updated. |
| 6 | Buffer length change semantics | Clear buffer on length change. No step preservation. |
| 7 | Invalid source UX | List model clamps source < Fractal index. Engine silently treats out-of-range as idle. |
| 8 | Replay gate-off | Muted steps schedule gate-off at step start. No gate hang. |
| 9 | Density destructiveness | Density reads `_gateBitmap`, writes to per-tick replay mask. Bitmap never modified by density. |
| 10 | Complexity scope | Complexity controls mutation pitch selection only. Recording/recapture always from source outputs. |
| 11 | overdubMode in Phase 1 | Removed from model. Added when blend mode is implemented (post-MVP). |
| 12 | Branches storage | Branches are trunk+math at playback time. No separate buffers. Zero branch buffer RAM. |
| 13 | Ornamentation storage | Ornamentation is tick-path evaluation. No per-step storage, ~2 B model params. |
| 14 | Evolution system RAM | MutationHistory 256 B inline in engine, SelectionPressure mode 2 bits on model, EvolutionDepth 1 byte on model. |
| 15 | Timing features | KD-15: bar-quantized loop length (loopBars), beat offset (beatOffset), free phase rotation (loopPhase). 6 B model. Phase 2. |
| 16 | Recording extent vs loop window | KD-16: recordFirst/recordLast separate from loopFirst/loopLast. 4 B model (uint16_t+uint16_t). Phase 3. |
| 17 | CV-driven playback | KD-17: clockSource (Internal/External) on FractalSequence. External = routedScan CV direct-maps to step position via int(cv) floor-truncation + edge detection. Bypasses divisor, PlayMode, all clock machinery. DiscreteMap pattern. Phase 1 (model) + Phase 2 (engine). |
