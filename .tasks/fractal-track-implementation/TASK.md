# FractalTrack Implementation — Task Status

Status: Blocked
Priority: Low

## Goal

A new TrackMode: a **step-sampled CV/Gate looper with mutation, branches, and ornamentation**. FractalTrack records gate/CV from 1-2 parent tracks into a heap-allocated loop buffer, plays back the captured loop, and evolves it through a three-layer architecture:

```
TRUNK (recorded buffer, may evolve via Proteus + evolution system)
  -> BRANCHES (trunk+math transforms at playback, zero extra storage)
    -> ORNAMENTATION (per-step classical flourishes on top)
```

**Why this matters:** XFORMER has deterministic step sequencers (NoteTrack), probabilistic generators (StochasticTrack), and algorithmic generators (TuesdayTrack). FractalTrack fills the recording/mutation/evolution niche: a track that captures live output from other tracks, loops it, and continuously transforms it through history-biased mutation at loop boundaries.

## Core Identity

FractalTrack is a **record-and-evolve child layer** over existing tracks. It is NOT a stochastic generator, NOT a step sequencer. It records once, loops indefinitely, and evolves at loop boundaries.

## References

- `docs/superpowers/specs/2026-05-17-fractal-track-design.md` — controlling spec (13 Key Decisions)
- `docs/superpowers/specs/2026-05-17-fractal-advanced-research.md` — post-MVP features (buffer math, recording variants, Teletype pattern ops)
- `.tasks/fractal-track-implementation/DICTIONARY.md` — immutable vocabulary and ownership contract

## Review Audit Log

This task has been audited for 9 issues identified by codebase research. All findings confirmed and applied:

| # | Finding | Codebase Grounding | Resolution |
|---|---------|-------------------|------------|
| 1 | Pattern behavior confusing | All tracks use `changePattern()` to swap sequence pointers; no track swaps buffers on pattern change. Stochastic has `_lockedParents` per-engine, not per-pattern. Fractal must match this. | DICTIONARY.md states: "Pattern switching is a config-only operation over the same live buffer." |
| 2 | Scope too large for first pass | Stochastic had the same failure: 8-phase plan with all features bundled. Real implementation proved 4-phase core was enough. | TASK.md restructured: Phase 1-3 = model + engine + record/loop + list UI + lock/windowing. Phase 4+ = mutation/evolution. Phase 5 = branches/ornament = post-core. |
| 3 | RAM assessment overclaims safety | Stochastic's `_lockedParents[CONFIG_STEP_COUNT]` = 64 x `LockedParentEvent` (~3,840 B heap). Fractal's heap buffer at 64 steps = 272 B. 8 Fractal tracks at 256 steps = ~8.7 KB worst case. Current headroom ~8 KB. | RAM allocation policy added: max 4 active Fractal engine buffers, cap length at 256, fail visibly on OOM (matching Stochastic's `std::nothrow` pattern). |
| 4 | Engine size estimate not credible | StochasticTrackEngine proved guesses wrong (locked at 512 B static_assert). TuesdayTrackEngine has ~600 B of algorithm-specific state alone. | Arm sizeof probe required before Phase 4 (mutation + history). Skeleton engine must be built and measured before adding MutationHistory/branches/ornamentation. |
| 5 | Teletype parent contradiction | TeletypeTrackEngine reads `gateOutput(0)`/`cvOutput(0)` from any engine via `_engine.trackEngine(index)` — exact same interface Fractal uses. | No Teletype exclusion. All lower-index engine types supported, including Teletype. |
| 6 | UI scheduled too late | Stochastic lesson: hardware validation was blocked by lack of list UI. | List UI merged into Phase 2, immediately after model. Not deferred to Phase 6. |
| 7 | Lock semantics need sharpening | Stochastic `_lock` freezes evaluated events output. Fractal lock is semantically different. | DICTIONARY.md: Lock = protect trunk buffer mutation only. Branch transforms and ornamentation continue while locked. Renamed to avoid stochastic confusion. |
| 8 | Mutation zone empty-zone magic | Invariant `loopFirst <= mutateFirst <= mutateLast <= loopLast` means empty zone (mutateFirst > mutateLast) is undefined. | Spec changed: no empty zone. Use `lock` for full freeze, `mutationProb=0` for mutation-only freeze. |
| 9 | Too many MVP params | Stochastic's ~30 list params are already overwhelming. | Phase 1 model reduced to MVP core: Source A/B, Gate/CV Logic, BufferLength, Record, Clear, Lock, Loop First/Last, Rotate, Density, Tilt, Complexity, Patience, MutationProb, OctaveShiftProb, Octave, Transpose, SlideTime. Branches/evolution/ornament params deferred to their respective phases. |

## Architecture Overview

FractalTrack follows the **TuesdayTrack pattern** — a new TrackMode with its own model/engine/UI layers, reusing all existing infrastructure.

### Reused Infrastructure (zero new code)

| Concern | Reused From |
|---------|-------------|
| Track container | `Track.h` union + `Container<>` |
| Pattern system | `CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT` |
| Track linking | `Track::_linkTrack`, existing link mechanism |
| Routing | `Routable<T>`, `Routing::Target`, `Routing::isRouted()` |
| Serialization | `VersionedSerializedWriter/Reader` |
| Engine lifecycle | `Engine::updateTrackEngines()`, `TrackEngine` base class |
| UI pages | `PageManager`, `BasePage` |
| Track setup | `TrackPage` + ListModels |
| Accumulator | Existing accumulator system |
| Clipboard | `ClipBoard` class |
| Parent engine resolution | Use `_engine.trackEngine(index)` pattern (confirmed in TeletypeTrackEngine.cpp: resolve `TrackEngine*` pointer, read `gateOutput(0)`/`cvOutput(0)` per tick). No LogicTrackEngine file exists in codebase — the pattern is simply reading engine outputs. |
| Heap allocation failure | `new (std::nothrow)` (confirmed in StochasticTrackEngine: `_lockedParents = new (std::nothrow) LockedParentEvent[...]`) |

### Model Layer

| Component | File | Lines | Description |
|-----------|------|-------|-------------|
| FractalSequence | `model/FractalSequence.h` | ~20 | Minimal per-pattern: divisor, scale, root, runMode, firstStep, lastStep, resetMeasure |
| FractalTrack | `model/FractalTrack.h` | ~60 | 17 sequences + MVP track params (see Phase 1). Additional params added incrementally per phase. |

**Estimated sizes:**
- FractalSequence: ~20 B
- FractalTrack (MVP): ~400 B
- **Compare:** NoteTrack = 9,544 B. FractalTrack ~400 B — well under gate. No container inflation risk.

### Engine Layer

| Component | File | Lines | Description |
|-----------|------|-------|-------------|
| FractalTrackEngine.h | `engine/FractalTrackEngine.h` | ~100 | Engine: parent resolution, buffer pointers, lock, loop state, output state |
| FractalTrackEngine.cpp | `engine/FractalTrackEngine.cpp` | ~300 | Core: tick(), record, loop playback |

**Estimated sizes (MVP, no history/branches):**
- FractalTrackEngine (MVP): ~400 B (TrackEngine base + sequence state + queues + counters + buffer pointers)
- **Compare:** Engine container gate = TeletypeTrackEngine = 912 B. Estimated ~400 B — under gate.
- **Phase 4/5 estimate with history+branches+ornament:** ~600-700 B

### UI Layer

| Component | File | Lines | Description |
|-----------|------|-------|-------------|
| FractalTrackListModel | `ui/model/FractalTrackListModel.h` | ~40 | TrackPage list model for Phase 2-3 core params |
| FractalTrackPage (post-hardware) | `ui/pages/FractalTrackPage.h` | ~300 | Visual pages (post-core) |

**Estimated sizes:** Comparable to any existing TrackMode page.

### New Files Summary (MVP)

| File | Lines | Purpose |
|------|-------|---------|
| `model/FractalSequence.h` | ~20 | Minimal per-pattern params |
| `model/FractalTrack.h` | ~60 | Track container + MVP params |
| `engine/FractalTrackEngine.h` | ~100 | Engine class (skeleton, no history/branches) |
| `engine/FractalTrackEngine.cpp` | ~300 | Core: record/loop |
| `ui/model/FractalTrackListModel.h` | ~40 | List model for TrackPage |
| **Total new (MVP)** | **~520** | |

### Files Modified (additions only, MVP)

| File | Lines Added | What Changes |
|------|------------|--------------|
| `model/Track.h` | ~30 | Add Fractal to TrackMode enum, Container, union, accessors, initContainer |
| `model/Routing.h` | ~15 | Add FractalFirst..FractalLast routing targets |
| `engine/Engine.h` | ~3 | Add FractalTrackEngine to TrackEngineContainer typedef |
| `engine/Engine.cpp` | ~3 | Add case to creation switch |
| `ui/Pages.h` | ~3 | Register FractalTrackPage for list model |
| `ui/pages/TopPage.cpp` | ~2 | Route to FractalTrackPage |
| `ui/pages/TrackPage.cpp` | ~3 | Instantiate FractalTrackListModel |
| `ui/Ui.h` | ~2 | Include FractalTrackPage header |
| **Total modified** | **~61** | |

## RAM Assessment

### Current Budget Context

| Resource | Current | Limit | Headroom |
|----------|---------|-------|----------|
| `.data + .bss` | 119,960 (91.4%) | 128 KB | ~8 KB |
| `.ccmram_bss` | 54,096 (84.5%) | 64 KB | ~10 KB |
| Flash .text | 763,436 (74.6%) | 1 MB | ~261 KB |

### SRAM Headroom Reality Check

Current headroom is ~8 KB. Eight FractalTrack engines at 256-step buffers each would consume ~8.7 KB heap — exceeding headroom. This makes the allocation policy a hard constraint, not a suggestion.

**RAM Allocation Policy:**
1. **Buffer length cap:** Default 64 steps (272 B heap). Maximum 256 steps (1,088 B per active engine).
2. **Active Fractal engine limit:** At most 4 FractalTrack engines can allocate buffers simultaneously. Worst-case Fractal heap = ~4 KB at 256 steps each (within headroom).
3. **Allocation failure:** Use `new (std::nothrow)`. On OOM, engine enters idle mode (gate=false, cv=0) with internal OOM flag. List UI surfaces "Buffer OOM" warning. No crash, no assert. Follows StochasticTrackEngine's `_lockedParents = new (std::nothrow) LockedParentEvent[...]` pattern exactly.
4. **ARM sizeof probe required before Phase 4-5:** Build skeleton FractalTrackEngine on STM32 and report `sizeof` before adding MutationHistory (256 B), branch counters, or ornamentation. Validate ~600 B estimate against 912 B gate.
5. **No heap amplification:** One buffer per engine instance. Pattern switching does not reallocate. No per-pattern buffers.

### Model Impact (SRAM)

| Component | Size | Notes |
|-----------|------|-------|
| FractalTrack (MVP) | ~400 B | 17 sequences + core params |
| 8 tracks max | 400 B | Container sized by NoteTrack (9,544 B); FractalTrack is smaller |
| **Model delta** | **~0 B** | No container inflation |

### Engine Impact (CCMRAM, MVP)

| Component | Size | Notes |
|-----------|------|-------|
| FractalTrackEngine (MVP) | ~400 B | No history/branches yet |
| **Engine delta** | **~0 B** | Under 912 B gate |

### Heap Buffer (SRAM)

| Config | Size | Notes |
|--------|------|-------|
| 64 steps (default) | 256 B | Single uint32_t array, 4 B per step |
| 256 steps (max, up to 4 engines) | 1,024 B | Per active engine, capped at 4 engines |

### Phase 4-5 Direct Additions (CCMRAM)

| Component | Size | Phased In |
|-----------|------|-----------|
| MutationHistory (16 records) | ~256 B | Phase 4 (after sizeof probe) |
| SelectionPressure mode | 2 bits on model | Phase 4 |
| EvolutionDepth | 1 B on model | Phase 4 |
| Trunk/branch counters | ~6 B in engine | Phase 5 |
| Branch flags, path, order | ~4 B on model | Phase 5 |
| Ornament prob + mode | 2 B on model | Phase 5 |

## Phased Implementation Plan

### Phase 1: Model & Serialization (MVP Core)
**Goal:** FractalTrack + FractalSequence in Track container, serializable.
**Files:** NEW `model/FractalSequence.h`, `model/FractalTrack.h`. EDIT `model/Track.h`, `model/Routing.h`, `engine/Engine.h`, `engine/Engine.cpp`.
**Params (MVP core only):** sourceA, sourceB, gateLogic, cvLogic, bufferLength, recordArmed, recordMode, punchMode, loopMode, recordQuantize, lock, loopFirst, loopLast, loopBars, beatOffset, loopPhase, rotate, complexity, patience, mutationProb, octaveShiftProb, density, tilt, slideTime, octave, transpose, clockSource, routedScan.
**Deferred to later phases:** mutateFirst/mutateLast (Phase 3), evolutionDepth/pressureMode (Phase 4), trunkCycles/branchCount/pathType/orderMode/branchTransformFlags (Phase 5), ornamentProb/ornamentMode (Phase 5).
**Verification:** STM32 build compiles. `sizeof(FractalTrack)` probe. `sizeof(FractalTrackEngine)` probe (skeleton).

### Phase 2: Engine Foundation — Record & Loop + Minimal List UI
**Goal:** Record from 1-2 parent tracks into loop buffer, play back. All features accessible from TrackPage list view immediately.
**Files:** NEW `engine/FractalTrackEngine.h`, `engine/FractalTrackEngine.cpp`. NEW `ui/model/FractalTrackListModel.h`.
**Engine behavior:** `tick()` resolves parent engines via `_engine.trackEngine(index)`, reads `gateOutput(0)`/`cvOutput(0)`. **Clock source check:** if clockSource==External, read `routedScan()` CV, map to step via floor-truncation + edge detection (DiscreteMap pattern). Bypass all clock advancement, divisor, and PlayMode. **Timing computation if Internal:** if loopBars > 0, derive loop window from bar count and apply beatOffset; use loopFirst/loopLast directly otherwise. Apply loopPhase for fractional rotation of playback read position. Recording features: Replace mode vs Latch mode, PunchIn wait state, Loop/Once boundary behavior, optional record quantize to sequence scale. Playing replays.
**Buffer lifecycle:** Volatile engine state. Survives transport reset and pattern change. Cleared on mode/length/explicit/power. Pattern change = config-only (matching all existing tracks' `changePattern()`).
**List UI exposes (immediately):** Clock Source (Internal/External), PlayMode, Divisor, Scale, Root, Source A/B, Gate/CV Logic, BufferLength, Record Arm, Clear (action), Record Mode (Replace/Latch), Punch Mode (Immediate/PunchIn), Loop Mode (Loop/Once), Rec Quantize (Off/On), Lock, Density, Tilt, Loop First/Last, Loop Bars, Beat Offset, Loop Phase, Rotate, Complexity, Patience, MutationProb, OctaveShiftProb, SlideTime, Octave, Transpose. When ClockSource=External, divisor, PlayMode, and gateLength are hidden.
**Integration:** `TrackPage.cpp` routes to `FractalTrackListModel` when Fractal mode is active.
**Verification:** Assign NoteTrack parent, record loop, hear it repeat. All params editable from list view.

### Phase 3: Loop Controls — Windowing, Rotation, Lock, Mutation Zone, Recording Extent
**Goal:** Loop window, rotation, lock (trunk buffer protection), mutation zone, and separate recording extent vs loop window.
**Adds:** recordFirst, recordLast (recording extent), mutateFirst, mutateLast (mutation zone). Extent invariant: `recordFirst <= loopFirst <= mutateFirst <= mutateLast <= loopLast <= recordLast <= bufferLength-1`.
**Recording extent behavior:** Recording engine writes to `recordFirst..recordLast` and wraps from `recordLast` back to `recordFirst`. Clear buffer clears extent only. Loop window is a subset of extent.
**Lock semantics:** Lock = protect trunk buffer mutations only (evolution, recording, boredom recapture). Branch transforms and ornamentation continue while locked. This is NOT stochastic Hold (frozen evaluated output) — it's a trunk freeze.
**Verification:** Record into long buffer, narrow loop window, confirm playback is constrained. Rotate window, lock/unlock, set zone narrower than window (anchor steps survive mutation). Confirm branch transforms still execute while locked.

### Phase 4: Proteus Mutation + Evolution System
**Goal:** Loop-boundary lifecycle with evolution. **Requires ARM sizeof probe before coding.**
**Adds:** evolutionDepth (0-100%), pressureMode (Even-Spread/Hot-Spot/Pattern-Follow), MutationHistory (16 records inline, ~256 B).
**ARM sizeof gate check:** Skeleton FractalTrackEngine must be built on STM32 and `sizeof` reported before adding history. If engine exceeds 912 B, history must move to heap or be re-budgeted.
**Loop boundary order:** boredom -> evolution step selection via pressure+depth -> mutate CV -> record to history -> octave shift.
**Verification:** Set patience moderate + evolution active, observe buffer evolving over loops. Lock prevents mutation.

### Phase 5: Branches + Ornamentation + Density (Post-Core)
**Goal:** Branch transforms (trunk+math), ornamentation (per-step flourishes), density.
**Adds:** branchCount, trunkCycles, pathType, orderMode, ornamentProb, ornamentMode, branchTransformFlags.
**Branches:** Playback-time math transform on trunk. No buffer storage. No mutation during branch phase. Zero extra RAM.
**Ornamentation:** Tick-path evaluation. Zero per-step storage. ~2 B model params.
**Verification:** Sweep density. Cycle trunk + 3 branches. Enable ornamentation.

### Phase 6: Visual UI Pages (Post-Hardware)
**Goal:** Visual pages for hardware workflow.
**Pages:** `FractalBufferPage` (buffer waveform with Compass-style loop bar: 128px horizontal bar showing loopFirst/loopLast markers, playhead position, density-dimmed steps, record extent brackets, lock and record arm indicators), `FractalMutationPage` (evolution), `FractalPerformancePage` (branches/ornament/density).
**Verification:** Full UI flow: set parents, record, view buffer, adjust evolution.

## Future Research (Compass-Derived)

Features noted for post-core consideration, not in any phased plan:

**Command palette for mutation dispatch:** Compass's 18-command palette with per-step enable/disable could replace probabilistic ornamentation with step-accurate ornament type selection. Palette index (4 bits) fits in the 13 reserved bits of the `_stepBuffer` word.

**Discrete rate table:** Compass uses 6 playback rates {-2, -1, -0.5, 0.5, 1, 2} with slew. A FractalTrack speed multiplier would skip/repeat buffer reads for half-speed or double-speed effect. Model cost: 1 B. Engine cost: 4 B float. Changes the step-time relationship significantly.

**Grid gesture recording:** Concept of recording encoder gestures (direction, speed, duration) as a modulation layer over the FractalTrack buffer. Architecturally closer to the accumulator system than to FractalTrack's core.

### Phase 7: Validation & Polish
**Goal:** STM32 release validation, RAM probe, hardware testing.
**Checks:** `sizeof(FractalTrack)` under 9,544 B. `sizeof(FractalTrackEngine)` under 912 B. Heap buffer OOM path tested. Config serialization round-trips. `.data + .bss` under 120 KB.

## Dependencies

| Task | Relationship |
|------|-------------|
| `resource-optimization` | **Blocks.** RAM headroom at 91.4%. FractalTrack allocation policy assumes ~4 KB headroom for heap buffers. |
| `stochastic-track-port` | **Watch.** Active on feat/stochastic -- higher priority. May absorb remaining headroom. |

## Blocks

Nothing directly blocked.
