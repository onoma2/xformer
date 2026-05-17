# Fractal Track Engine — Recorder/Mutator Design

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
| Relationship | Stand-alone sequencer | Post-processing child of other tracks |

---

## Key Design Decisions

### KD-1: Input Source Resolution — Any-Engine via TrackEngine Output

**Decision:** Read `_gateOutput` and `_cvOutput` directly from the linked track engines, not from sequence step data.

**Rationale:** The existing `LogicTrackEngine` already demonstrates per-tick resolution of parent engines via `_engine.trackEngine(index)`. The Fractal Track does NOT need a `FractalSourceInterface` — it just reads the concrete output of any engine. This avoids adding virtual methods to all 8 existing engine types.

**Pattern:** Same as `LogicTrackEngine` lines 195–212: resolve `TrackEngine*` pointers once, read `gateOutput(0)` and `cvOutput(0)` each tick.

**Cycle safety rule:** Fractal must read source tracks that are evaluated *before* Fractal in the same tick. The Engine evaluates tracks in index order (0→7). Therefore:
- Source track A and source track B must have indices **lower** than the Fractal track's index.
- If either source index ≥ Fractal track index, the engine treats that source as idle (gate=false, cv=0).
- **Invalid source UX:** The list model prevents selecting source indices ≥ Fractal track index (clamped in UI). Engine does not log warnings — it silently treats out-of-range sources as idle. The user never sees a broken configuration from the UI.
- No cross-track feedback is possible by construction. This matches the existing Performer convention where linked tracks must be earlier in the track array.
- Alternative (rejected for MVP): read from previous tick's latched output. This would allow any source index but requires a per-engine output latch — too much architectural change for MVP.

### KD-2: Buffer Format — Split Arrays, Configurable Length

**Decision:** Heap-allocate **three separate arrays** (not a struct-per-step):
```cpp
float *_cvBuffer;       // cvBuffer[step] = V/Oct voltage at that step
uint8_t *_gateBitmap;   // bit i = gate state for step i
uint8_t *_validBitmap;  // bit i = 1 if step has been recorded
```
Configurable length: 16/32/64/128/256 steps. Default 64.

**Why split arrays, not struct-per-step:** A `{float cv; bool gate;}` struct pads to 8 bytes due to alignment. Split arrays avoid padding waste and let the gate bitmap double as the density masking input directly.

**RAM budget at max length (256 steps):**
- CV array: 256 × 4 B = 1024 B
- Gate bitmap: 256 / 8 = 32 B
- Valid bitmap: 256 / 8 = 32 B
- **Total: 1088 B heap** — comparable to Stochastic's 768 B.

**RAM budget at default length (64 steps):**
- CV array: 64 × 4 B = 256 B
- Gate bitmap: 8 B
- Valid bitmap: 8 B
- **Total: 272 B heap** — very light.

**Rationale:** Unlike Stochastic which stores per-step probability grids (512 B per pattern), Fractal stores only the step-sampled result — gate on/off and final CV voltage. Step-aligned timing keeps the buffer naturally synchronized with the Performer's divisor system.

**Deferred:** Per-step slide/legato/length capture. Each step captures one instantaneous gate+CV snapshot. Replayed gate length is one divisor division unless legato is added later.

**Buffer persistence (MVP):** The recorded buffer is **volatile engine state**, not model state. One buffer per active FractalTrackEngine, heap-allocated, not serialized. Project save stores configuration only (model params, no buffer data). The buffer survives transport reset and pattern change (config-only changes). The buffer is cleared on: track mode change, buffer length change, explicit user clear action, or power cycle.

> **Note:** Buffer persistence (saving/loading recorded loops as project data) may be revisited post-MVP. Options include: one saved buffer per pattern, one shared buffer per track, or external sample import. This decision affects project file format, flash wear, and RAM budget and should not be made until the volatile engine proves the concept.

**Buffer length change:** Changing `bufferLength` clears the entire buffer. No attempt to preserve overlapping steps — the new buffer starts empty.

**Buffer ownership:** One buffer per FractalTrackEngine instance, not per pattern. Pattern change updates configuration (divisor, scale, mutation params) but does not swap the recorded buffer. There is no per-pattern buffer array.

### KD-3: Loop Lifecycle Model — Proteus-Inspired Boredom/Mutation

**Decision:** Adopt the Proteus spec's loop-boundary lifecycle:

1. **Complexity** parameter controls how aggressively **mutation pitch selection** works. It affects mutation only — recording and recapture always come from source track outputs directly.
2. **Patience** parameter ramps a boredom probability at each loop boundary. At max patience, the loop never resets.
3. **Mutation** probability re-rolls one buffer index per loop pass.
4. **Octave Shift** probability offsets the entire buffer ±12 semitones, constrained to ±1 octave from base.

**Rationale:** These are the Proteus §3.2–3.4 mechanics, adapted from "melody buffer reset" to "captured-loop mutation." They map cleanly to the Stochastic track's existing Phase 7 plan.

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

### KD-6: Mutation Engine — Persistent Engine RNG (MVP)

**Decision:** MVP mutations use a **persistent engine RNG** (same `Random` class as StochasticTrackEngine). The RNG is a single `uint32_t` seed that advances normally with each roll:
- Engine holds `_rng` as a member. Reseeded on transport reset from track index + pattern.
- At each loop boundary, consume RNG rolls for each mutation type (mutation, octave shift, boredom reset).
- Each loop boundary produces different results because the RNG state advances naturally.
- No mutation history array, no Turing register — "stateless" means no saved mutation log, not that the RNG is re-seeded identically each loop.

**Why NOT Turing DNA for MVP:** A shift register makes mutation stateful — the engine must store the register, its length, and its probability parameter. This adds engine RAM beyond the single `uint32_t` RNG seed. Turing DNA is a strong post-MVP candidate (see Reference Porting B) but the MVP should prove the loop recorder first, then layer in correlated mutation patterns.

**Post-MVP upgrade path:** Replace the per-loop random index with a Turing shift register (`util_turing.h`) where set bits select mutation targets. This adds ~12 B to the engine (uint64_t register + uint8_t length + uint8_t probability) and creates correlated, drifting mutations.

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

### KD-9: Engine Container Impact

**Decision:** FractalTrackEngine must stay at or below the current engine container gate (912 B = TeletypeTrackEngine).

**Engine members estimate:**
- TrackEngine base (~100 B)
- SequenceState, linkData, queues (~200 B)
- RNG, loop counters, boredom counter (~30 B)
- No mutation history or Turing register — single persistent RNG seed, no mutation log
- No lockedSteps — the main buffer IS the loop (heap-allocated)
- **Target: ~400–600 B** — well under the 912 B gate.

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
- All-steps-anchor: zone = empty (mutateFirst > mutateLast) effectively freezes the whole buffer without using lock — lock blocks recording too, but an empty zone only blocks mutation

**Relationship to Proteus anchors:** The Proteus spec describes "anchor notes" that survive regeneration (complexity low = repeat 1-2 anchors). The mutation zone IS the Proteus anchor mechanism made explicit: steps outside the zone are anchors that never change, steps inside evolve. The Proteus complexity knob then controls *how aggressively the mutable zone evolves*, while the zone itself controls *what is mutable*.

**Relationship to P.WTH (Tidal `within`):** The mutation zone is the Fractal equivalent of Tidal's `within(arc, transform)` and the Teletype `P.WTH start end transform` op. Instead of specifying the range per-transform, the range is a persistent model parameter. Post-MVP buffer math operations can also accept explicit per-call ranges that override the model zone.

---

## Phased Implementation Plan

### Phase 1: Model & Serialization

**Goal:** `FractalTrack` and `FractalSequence` in the Track container, serializable.

**Files:**
- NEW `model/FractalSequence.h` — minimal sequence: divisor, scale, root, runMode, firstStep, lastStep, playMode, resetMeasure (~20 B).
- NEW `model/FractalTrack.h` — 17 sequences + track params: complexity, patience, mutationProb, octaveShiftProb, density, tilt, inputTrack1, inputTrack2, slideTime, octave, transpose, rotate, fillMode, gateLogic, cvLogic, bufferLength, recordArmed, mutateFirst, mutateLast. (cvUpdateMode deferred — see Phase 5. overdubMode deferred to post-MVP blend phase.)
- EDIT `model/Track.h` — add `Fractal` to TrackMode enum, Container, union, accessors, initContainer.
- EDIT `model/Routing.h` — add `FractalFirst..FractalLast` routing targets.
- EDIT `engine/Engine.h` — add `FractalTrackEngine` to TrackEngineContainer typedef.
- EDIT `engine/Engine.cpp` — add `case TrackMode::Fractal` to creation switch.

**Verification:** STM32 build compiles, `sizeof(FractalTrack)` probe.

### Phase 2: Engine Foundation — Record & Loop

**Goal:** Record CV/Gate from 1–2 master tracks into a heap-allocated loop buffer, play it back on loop.

**Files:**
- NEW `engine/FractalTrackEngine.h`
- NEW `engine/FractalTrackEngine.cpp`

**Core behavior:**
- `tick()`: resolve parent engine pointers (LogicTrack pattern), read `gateOutput(0)` and `cvOutput(0)`.
- When recording: write source gate into `_gateBitmap`, source CV into `_cvBuffer`, set bit in `_validBitmap` at current step index.
- When playing: replay buffer in sequence order within `loopFirst..loopLast`.
- Sequence advancement follows existing aligned/free playMode pattern (StochasticTrackEngine pattern).
- Gate/CV queue system (same as all engines).

**Key functions:**
- `triggerStep()`: if recording, capture current parent output into buffer; if playing, replay from buffer.
- `reset()`: reset sequence state, clear queues. Do NOT clear buffer on transport reset — only on explicit "clear" action.
- `update()`: slide interpolation, monitoring.

**Cycle safety:** Engine checks source track indices < Fractal track index at creation time. Warns and treats invalid sources as idle.

**Verification:** STM32 build, manual test via list UI: assign NoteTrack as parent, record one loop, hear it repeat.

### Phase 3: Loop Controls — Windowing, Rotation, Lock, Mutation Zone

**Goal:** Loop windowing (first/last step), rotation, lock (freeze buffer from further mutation/recording), and mutation zone (scoped transform range).

**Model additions:**
- `loopFirst`, `loopLast` on FractalTrack (already planned).
- `lock` bool — when true, buffer is read-only; mutations and recording are blocked.
- `rotate` — address rotation over `loopFirst..loopLast`.
- `mutateFirst`, `mutateLast` — mutation zone boundaries (KD-10). Default to `loopFirst`/`loopLast`. Invariant: `loopFirst ≤ mutateFirst ≤ mutateLast ≤ loopLast`.

**Engine changes:**
- `triggerStep()` respects lock — if locked, replay only, no writes.
- Rotation applied via `SequenceUtils::rotateStep()` (existing).
- Mutation zone boundaries stored on model, read by mutation logic in Phase 4.

**Verification:** Rotate loop window, lock/unlock, confirm playback matches. Set mutation zone narrower than loop window, confirm anchor steps survive mutation.

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

### Phase 5: Density & Performance Mechanics

**Goal:** Deterministic density masking and basic performance controls.

**Reused from Stochastic Phase 6:**
- `density` (0–100): deterministic ranked rest-priority thinning over the loop. **Density is non-destructive:** it reads `_gateBitmap` but writes to a per-tick replay mask. `_gateBitmap` is never modified by density — only mutation and recording change the bitmap.
- `tilt` (-100 to +100): bias rest-priority toward front or back of loop.
- **Replay gate-off:** When density mask or invalid buffer mutes a step, the engine schedules gate-off at step start (clears any prior gate). No gate hang — muted steps always produce a clean gate-off.
- Record arm/disarm via UI (Fractal-local, not global `_engine.recording()`).

**Deferred from MVP:**
- Overdub/blend mode: post-MVP weighted lerp (see KD-5).

**Deferred from MVP:**
- Jitter: the buffer stores step-aligned CV/gate snapshots with no intra-step timing. Jitter would require a `_jitterTicks[]` buffer or live timing perturbation — too much for MVP.
- cvUpdateMode: a step-sampled buffer replaying stored CV does not need gate/always update semantics. Remove until continuous source-following is a feature.

**Verification:** Sweep density from 100→0, confirm steps drop out in stable priority order. Test overdub: record second pass over existing loop.

### Phase 6: Minimal List UI (Before Hardware Validation)

**Goal:** Enough UI to test all engine features on hardware without needing visual pages.

**`FractalTrackListModel` items (follows existing ListModel pattern):**
- PlayMode, Divisor, ResetMeasure, Scale, Root
- Source A (track 1–8), Source B (track 1–8, or Off)
- Gate Logic (A/B/AND/OR/XOR/NAND/Random)
- CV Logic (A/B/Sum/Avg/Min/Max/Random)
- Buffer Length (16/32/64/128/256)
- Record Arm (Yes/No), Clear Buffer (action)
- Lock (Yes/No)
- Density, Tilt
- Complexity, Patience, Mutation Prob, Octave Shift Prob
- Loop First, Loop Last, Rotate
- Mutate First, Mutate Last (mutation zone — KD-10)
- SlideTime, Octave, Transpose

**Integration:** `TrackPage.cpp` routes to `FractalTrackListModel` when track mode is Fractal.

**Verification:** All parameters accessible and editable from existing TrackPage list view. No custom pages needed for hardware validation.

**Post-MVP:** Visual pages (buffer waveform, mutation visualization) as Phase 8+.

### Phase 7: Visual UI Pages (Post-Hardware Validation)

**Goal:** Visual pages for Fractal Track in the Performer UI.

**Pages (following existing patterns):**
- `FractalTrackListModel` — track setup: playMode, input tracks, divisor, scale, root, loop window.
- `FractalBufferPage` — visual display of captured CV/Gate buffer, loop window, lock state.
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
- `sizeof(FractalTrackEngine)` under 912 B.
- Heap-allocated buffer: 64 steps × (4 B CV + 1/8 B gate + 1/8 B valid) ≈ 272 B.
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
| **Phase 4** | Per-loop mutation via persistent RNG | Stochastic RNG pattern | Low — one Random roll per mutation type per loop boundary |
| **Phase 4** | Complexity → mutation weights | Proteus §3.1 | Low — 3-tier weight table |
| **Phase 4** | Patience boredom ramp | Proteus §3.2 | Low — linear ramp formula |
| **Phase 5** | Deterministic density masking | StochasticTrackEngine (already in codebase) | Low — copy evalDensityMask |
| **Phase 5+** | Blend overdub (weighted lerp) | o_c Palimpsest compose/decompose rates | Low — lerp with blend weight param |
| **Phase 5+** | Turing shift register mutation | o_c ShiftReg / util_turing.h | Medium — 12 B added to engine |
| **Phase 5+** | RunglBook freeze behavior | o_c RunglBook | Low — deterministic rotation when locked |
| **Phase 5+** | Logistic-map complexity | o_c util_logistic_map.h | Medium — Q24 fixed point + scale quantizer |
| **Phase 5+** | 1D CA evolution mode | o_c GameOfLife (simplified to 1D) | Medium — 64-bit row, rule evaluation |
| **Phase 6+** | Pattern-aware boredom | o_c PatternPredictor | Medium — 128 B history, autocorrelation |

---

## Open Questions (Decide Before Phase 4)

1. **Patience ramp function:** Linear or exponential? Linear is simpler and predictable. Exponential creates longer "plateaus" before sudden change. **Recommendation: Linear for MVP.**
2. **Mutation pitch source:** Simplified complexity-based selection (anchor/adjacent/full) or full degree-ticket integration from Stochastic track? **Recommendation: Simplified for MVP, tickets as upgrade.**
3. **Scale awareness:** Should mutation snap to the project scale, or the parent track's scale? **Recommendation: FractalSequence has its own scale/root (like Stochastic), defaulting to project scale.**
4. **Buffer clear trigger:** Only via explicit user action, or also on pattern change? **Resolved:** Buffer is volatile engine state. Clear on: track mode change, buffer length change, explicit clear, power cycle. Survives transport reset and pattern change.
5. **Overdub semantics:** **Resolved:** MVP is overwrite only (KD-5). Post-MVP blend mode uses weighted lerp (NOT additive CV accumulation, which breaks V/Oct pitch). Palimpsest-style compose/decompose rates control blend weight.
6. **Turing DNA timing:** **Resolved:** MVP uses persistent engine RNG (KD-6). Turing shift register deferred to post-MVP (~12 B added to engine when adopted).
7. **UI before hardware:** Phase 6 (minimal list UI) must come before Phase 8 (hardware validation). Lesson from Stochastic: no manual testing without at least a list model.
8. **Buffer persistence:** **Deferred to post-MVP.** MVP uses volatile engine buffer (not serialized). Options for persistence (per-pattern, per-track, external import) will be decided after the volatile engine proves the concept. Affects project file format, flash wear, and RAM budget.

## Resolved Decisions (Audit Round 4)

| # | Issue | Resolution |
|---|-------|-----------|
| 1 | Buffer ownership/persistence | Volatile engine state, not model state. Not serialized. Clear on mode/length/explicit/power change. |
| 2 | Mutation RNG contradiction | Persistent engine RNG (`_rng`), reseeded on transport reset. Not re-seeded per loop. "Stateless" means no mutation history, not no RNG state. |
| 3 | Boredom mutating user recordArmed | Internal `_autoCapturePending` flag. User `recordArmed` is never touched by boredom. |
| 4 | 17 patterns × buffer explosion | One volatile buffer per engine, not per pattern. Pattern change = config only. |
| 5 | Reference table contradicted MVP | Palimpsest, Turing, RunglBook moved to Section B (post-MVP). Porting priority table updated. |
| 6 | Buffer length change semantics | Clear buffer on length change. No step preservation. |
| 7 | Invalid source UX | List model clamps source < Fractal index. Engine silently treats out-of-range as idle. |
| 8 | Replay gate-off | Muted steps schedule gate-off at step start. No gate hang. |
| 9 | Density destructiveness | Density reads `_gateBitmap`, writes to per-tick replay mask. Bitmap never modified by density. |
| 10 | Complexity scope | Complexity controls mutation pitch selection only. Recording/recapture always from source outputs. |
| 11 | overdubMode in Phase 1 | Removed from model. Added when blend mode is implemented (post-MVP). |
