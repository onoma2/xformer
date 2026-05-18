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

## Architecture Overview

FractalTrack follows the **TuesdayTrack pattern** — a new TrackMode with its own model/engine/UI layers, reusing all existing infrastructure.

### Three-Layer Architecture

```
┌──────────────────────────────────────────────────────────────┐
│  TRUNK                                                       │
│  Single recorded buffer from parent tracks. Evolves at       │
│  loop boundaries via:                                        │
│    Proteus: complexity, patience, mutationProb,              │
│             octaveShiftProb                                   │
│    Evolution: MutationHistory (16 records),                  │
│               SelectionPressure (3 modes),                   │
│               EvolutionDepth (0-100%)                         │
│                                                              │
│  (trunk cycles N loops, then switches to branches)           │
└──────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌──────────────────────────────────────────────────────────────┐
│  BRANCHES                                                    │
│  "trunk + math transform" at playback time.                  │
│  No separate buffer storage. Transforms: Reverse, Inverse,   │
│  Transpose, Mutate, Randomize.                               │
│  Path: 8 curated navigation patterns (editable LUT).         │
│  Order: Forward, Reverse, Pendulum, Random, Converge, etc.   │
│                                                              │
│  (branches cycle M times, then return to trunk phase)        │
└──────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌──────────────────────────────────────────────────────────────┐
│  ORNAMENTATION                                               │
│  Per-step classical flourishes during playback.              │
│  Two-step: anticipation, suspension, syncopation, etc.       │
│  Four-step: runs, arps, turns, mordents.                    │
│  Max: 8-step trills.                                         │
│  Zero per-step storage — evaluated in tick path.             │
└──────────────────────────────────────────────────────────────┘
```

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
| Parent engine resolution | LogicTrackEngine pattern (read gateOutput/cvOutput per tick) |

### Model Layer

| Component | File | Lines | Description |
|-----------|------|-------|-------------|
| FractalSequence | `model/FractalSequence.h` | ~20 | Minimal per-pattern: divisor, scale, root, runMode, firstStep, lastStep, resetMeasure |
| FractalTrack | `model/FractalTrack.h` | ~80 | 17 sequences + track-level params (all KD params: complexity, patience, mutationProb, octaveShiftProb, density, tilt, sourceA, sourceB, gateLogic, cvLogic, bufferLength, recordArmed, lock, loopFirst/Last, rotate, mutateFirst/Last, evolutionDepth, pressureMode, trunkCycles, branchCount, pathType, orderMode, ornamentProb, ornamentMode, branchTransformFlags, slideTime, octave, transpose, fillMode) |

**Estimated sizes:**
- FractalSequence: ~20 B
- FractalTrack: ~20 B + 17 x ~20 B + overhead = ~540 B
- **Compare:** NoteTrack = 9,544 B. FractalTrack ~540 B -- well under gate. No container inflation risk.

### Engine Layer

| Component | File | Lines | Description |
|-----------|------|-------|-------------|
| FractalTrackEngine.h | `engine/FractalTrackEngine.h` | ~150 | Engine: parent resolution, buffer management (heap pointer), MutationHistory (256 B inline), SelectionPressure, counters, output state |
| FractalTrackEngine.cpp | `engine/FractalTrackEngine.cpp` | ~500 | Core: tick(), record, loop playback, loop-boundary lifecycle, branch transforms, ornamentation eval |

**Estimated sizes:**
- FractalTrackEngine: ~600 B (TrackEngine base + sequence state + queues + MutationHistory 256 B + counters)
- **Compare:** Engine container gate = TeletypeTrackEngine = 912 B. Estimated ~600 B -- under gate.

### UI Layer

| Component | File | Lines | Description |
|-----------|------|-------|-------------|
| FractalTrackListModel | `ui/model/FractalTrackListModel.h` | ~60 | TrackPage list model for all setup params |
| FractalTrackPage (post-MVP) | `ui/pages/FractalTrackPage.h` | ~300 | Visual pages: Buffer, Mutation, Performance, Setup |

**Estimated sizes:** Comparable to any existing TrackMode page.

### New Files Summary

| File | Lines | Purpose |
|------|-------|---------|
| `model/FractalSequence.h` | ~20 | Minimal per-pattern params |
| `model/FractalTrack.h` | ~80 | Track container + all KD params |
| `engine/FractalTrackEngine.h` | ~150 | Engine class + inline history |
| `engine/FractalTrackEngine.cpp` | ~500 | Core logic: record/loop/evolve/branch/ornament |
| `ui/model/FractalTrackListModel.h` | ~60 | List model for TrackPage |
| `ui/pages/FractalTrackPage.h` | ~300 | Visual pages (post-hardware-validation) |
| **Total new** | **~1,110** | |

### Files Modified (additions only)

| File | Lines Added | What Changes |
|------|------------|--------------|
| `model/Track.h` | ~30 | Add Fractal to TrackMode enum, Container, union, accessors |
| `model/Routing.h` | ~15 | Add FractalFirst..FractalLast routing targets |
| `engine/Engine.cpp` | ~3 | Add case to creation switch |
| `ui/Pages.h` | ~3 | Register FractalTrackPage |
| `ui/pages/TopPage.cpp` | ~2 | Route to FractalTrackPage |
| `ui/pages/TrackPage.cpp` | ~3 | Instantiate FractalTrackListModel |
| `ui/Ui.h` | ~2 | Include FractalTrackPage header |
| **Total modified** | **~58** | |

## RAM Assessment

### Current Budget Context

| Resource | Current | Limit | Headroom |
|----------|---------|-------|----------|
| `.data + .bss` | 119,960 (91.4%) | 128 KB | ~8 KB |
| `.ccmram_bss` | 54,096 (84.5%) | 64 KB | ~10 KB |
| Flash .text | 763,436 (74.6%) | 1 MB | ~261 KB |

### Model Impact (SRAM)

| Component | Size | Notes |
|-----------|------|-------|
| FractalTrack (per-track) | ~540 B | 17 sequences at ~20 B + overhead |
| 8 tracks max | 540 B | Container sized by NoteTrack (9,544 B); FractalTrack is smaller |
| **Model delta** | **~0 B** | No container inflation |

### Engine Impact (CCMRAM)

| Component | Size | Notes |
|-----------|------|-------|
| FractalTrackEngine | ~600 B | Under 912 B gate; no container inflation |
| MutationHistory (16 records) | ~256 B | Inline in engine |
| **Engine delta** | **~0 B** | Fits under gate |

### Heap Buffer (SRAM)

| Config | Size | Notes |
|--------|------|-------|
| 64 steps (default) | 272 B | CV: 256 B + Gate: 8 B + Valid: 8 B |
| 256 steps (max) | 1,088 B | CV: 1,024 B + Gate: 32 B + Valid: 32 B |
| **Heap budget** | **272-1,088 B** | Allocated per active FractalTrackEngine |

### Direct Additions

| Component | Size | Section |
|-----------|------|---------|
| MutationHistory | 256 B | Engine (CCMRAM, inline) |
| SelectionPressure mode | 2 bits | Model |
| EvolutionDepth | 1 B | Model |
| Trunk/branch counters | ~6 B | Engine (CCMRAM) |
| Branch flags, path, order | ~4 B | Model |
| Ornament prob + mode | 2 B | Model |
| Page instances | ~Page size | UI (.bss) |
| Routing targets | Negligible | .text |

**Verdict: FractalTrack is RAM-viable within current budget.** Model fits under NoteTrack gate (~540 B vs 9,544 B). Engine fits under 912 B gate (~600 B with history). Heap buffer at default 64 steps is 272 B. Direct additions are ~270 B CCMRAM (history) + negligible model bytes.

**However:** ARM sizeof probes on hardware are required before merging any Phase 1 code. These estimates must be confirmed on STM32.

## Phased Implementation Plan

### Phase 1: Model & Serialization
**Goal:** FractalTrack + FractalSequence in Track container, serializable.
**Files:** NEW `model/FractalSequence.h`, `model/FractalTrack.h`. EDIT `model/Track.h`, `model/Routing.h`, `engine/Engine.h`, `engine/Engine.cpp`.
**Params:** All KD params (complexity, patience, mutationProb, octaveShiftProb, density, tilt, sourceA, sourceB, gateLogic, cvLogic, bufferLength, recordArmed, lock, loopFirst/Last, rotate, mutateFirst/Last, evolutionDepth, pressureMode, trunkCycles, branchCount, pathType, orderMode, ornamentProb, ornamentMode, branchTransformFlags, slideTime, octave, transpose, fillMode).
**Verification:** STM32 build compiles. `sizeof(FractalTrack)` probe.

### Phase 2: Engine Foundation — Record & Loop
**Goal:** Record CV/Gate from 1-2 parent tracks into loop buffer, play back.
**Files:** NEW `engine/FractalTrackEngine.h`, `engine/FractalTrackEngine.cpp`.
**Behavior:** `tick()` resolves parent engines, reads gateOutput/cvOutput. When recording: writes to buffer arrays. When playing: replays from buffer. Follows aligned/free playMode.
**Key:** Buffer is volatile (survives transport reset, clear on mode/length/explicit/power).
**Verification:** Assign NoteTrack parent, record loop, hear it repeat.

### Phase 3: Loop Controls — Windowing, Rotation, Lock, Mutation Zone
**Goal:** Loop playback window, rotation, lock, mutation zone.
**Adds:** loopFirst/Last (window), lock (read-only), rotate, mutateFirst/Last (zone).
**Invariant:** `loopFirst <= mutateFirst <= mutateLast <= loopLast`.
**Verification:** Rotate window, lock/unlock, set zone narrower than window (anchor steps survive).

### Phase 4: Proteus Mutation + Evolution System
**Goal:** Loop-boundary lifecycle with Proteus mutation and per-loop-cycle evolution.
**Adds:** complexity, patience, mutationProb, octaveShiftProb, evolutionDepth (0-100%), pressureMode (Even-Spread/Hot-Spot/Pattern-Follow), MutationHistory (16 records inline).
**Loop boundary order:** boredom -> evolution step selection via pressure+depth -> mutate CV -> record to history -> octave shift.
**Verification:** Set patience moderate + evolution active, observe buffer evolving over loops. Lock prevents mutation. History biases step selection per mode.

### Phase 5: Branches + Ornamentation + Density
**Goal:** Branch transforms (trunk+math), per-step ornamentation, density masking.
**Adds:** branchCount, trunkCycles, pathType, orderMode, ornamentProb, ornamentMode, density, tilt.
**Branches:** During branch phase, playback applies math transform (Reverse/Inverse/Transpose/Mutate/Randomize) over trunk buffer at render time. No storage, no buffer mutation during branch phase.
**Path:** 8 curated navigation patterns in editable LUT.
**Ornamentation:** Per-step flourishes after CV+gate resolved. Zero per-step storage.
**Density:** Deterministic thinning (non-destructive replay mask).
**Verification:** Sweep density. Cycle trunk + 3 branches. Enable ornamentation, confirm flourishes.

### Phase 6: Minimal List UI (Before Hardware)
**Goal:** List model accessible from TrackPage. No visual pages needed for hardware validation.
**Edit `FractalTrackListModel` with:** PlayMode, Divisor, Scale, Root, Source A/B, Gate/CV Logic, BufferLength, Record, Clear, Lock, Density, Tilt, Complexity, Patience, MutationProb, OctaveShiftProb, EvolutionDepth, PressureMode, Loop First/Last, Rotate, Mutate First/Last, TrunkCycles, BranchCount, PathType, OrderMode, OrnamentProb, OrnamentMode, SlideTime, Octave, Transpose.
**Verification:** All params editable from existing TrackPage list view.

### Phase 7: Visual UI Pages (Post-Hardware)
**Goal:** Visual pages for FractalTrack in Performer UI.
**Pages:** `FractalBufferPage` (buffer waveform), `FractalMutationPage` (evolution), `FractalPerformancePage` (branches/ornament/density), `FractalTrackListModel` (setup).
**Verification:** Full UI flow: set parents, record, view buffer, adjust evolution, observe changes.

### Phase 8: Validation & Polish
**Goal:** STM32 release validation, RAM probe, hardware testing.
**Checks:** `sizeof(FractalTrack)` under 9,544 B. `sizeof(FractalTrackEngine)` under 912 B. Heap buffer allocation works. Config serialization round-trips. Routing modulates key params. `.data + .bss` under 120 KB.

## Dependencies

| Task | Relationship |
|------|-------------|
| `resource-optimization` | **Blocks.** RAM headroom at 91.4%. FractalTrack is estimated to fit, but needs ARM sizeof probe confirmation. |
| `stochastic-track-port` | **Watch.** Active on feat/stochastic -- higher priority for RAM allocation. |

## Blocks

Nothing directly blocked.
