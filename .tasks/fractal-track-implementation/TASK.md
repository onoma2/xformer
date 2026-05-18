# FractalTrack Implementation — Task Status

Status: Blocked
Priority: Low

## Goal

A new TrackMode inspired by Hallucigenia (VCV Rack) and Bloom v2. FractalTrack links to a parent track and applies smart mutation rules with evolutionary memory -- mutations get "smarter" over time via history-based bias. This gives XFORMER a generative track that evolves its output through parent-child inheritance rather than pure randomization.

**Why this matters:** XFORMER has NoteTrack (deterministic step sequencer), StochasticTrack (probabilistic step generator), and TuesdayTrack (algorithmic pattern generator). FractalTrack fills the evolutionary/mutation niche: a track that reads live output from another track and continuously mutates it with memory of what worked.

## Key Design

- **Parent-child inheritance**: FractalTrack links to a parent track (NoteTrack first, others phased later) and reads its step data as the mutation base
- **Memory-based mutation**: `MutationHistory` buffer (16 records) + `SelectionPressure` tracks which mutations succeeded and biases future ones toward winning patterns
- **Evolution Depth** (0-100%): controls how strongly history influences future mutations
- **Scale-constrained rules**: Scale-aware, leap-limited, position-aware mutations as the initial rule set
- **Deterministic seeds**: Full seed control for reproducibility
- **Bloom v2 features phased later**: Ratchet, slew, trill, 8-branch system

## References

- `doc/fractal-track-research.md` -- full design doc (~12K words)
- `.tasks/fractal-track-implementation/PHASEDPLAN.md` -- controlling phased implementation plan

## Architecture Overview

FractalTrack follows the **TuesdayTrack pattern** -- a new TrackMode with its own model/engine/UI layers, reusing all existing infrastructure (routing, serialization, track management, pattern system, page manager).

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

### Model Layer

| Component | File | Lines | Description |
|-----------|------|-------|-------------|
| FractalSequence | `model/FractalSequence.h` | ~120 | Per-pattern mutation parameters (parent link, pitch/vel/gate/len ranges, evolution depth, rule type, seed, branch seeds) |
| FractalTrack | `model/FractalTrack.h` | ~80 | Track-level container: 17 sequences array, play mode |

**Estimated sizes:**
- FractalSequence: ~48 B (Routable params + byte fields -- similar to TuesdaySequence at 42 B)
- FractalTrack: ~48 B + 17 x ~48 B + overhead = ~880 B
- **Compare:** NoteTrack = 9,544 B. FractalTrack would be **~8,664 B smaller** than the container gate. ✅

### Engine Layer

| Component | File | Lines | Description |
|-----------|------|-------|-------------|
| FractalTrackEngine | `engine/FractalTrackEngine.h` | ~130 | Engine class: parent link resolution, MutationHistory, SelectionPressure, output state |
| FractalTrackEngine.cpp | `engine/FractalTrackEngine.cpp` | ~350 | Core: parent source update, biased mutation, rule constraints, CV/gate output |

**Estimated sizes:**
- FractalTrackEngine: ~400-500 B (TrackEngine base 32 B + pointer to parent + MutationHistory 16 records + SelectionPressure + output state)
- MutationHistory: 16 records at ~12 B each = 192 B inline
- SelectionPressure: ~16 B (3 floats)
- **Compare:** Current engine gate = TeletypeTrackEngine = 912 B
- Estimated FractalTrackEngine size: ~500 B -- **under gate** ✅

### UI Layer

| Component | File | Lines | Description |
|-----------|------|-------|-------------|
| FractalTrackPage | `ui/pages/FractalTrackPage.h` | ~250 | Multi-section editor: LINK, MUTATION, EVOLUTION, BRANCHES (deferred), BLOOM (deferred) |
| FractalTrackListModel | `ui/model/FractalTrackListModel.h` | ~50 | TrackPage list model for setup params |

**Estimated sizes:** Comparable to any existing TrackMode page. Under existing page gates.

### New Files Summary

| File | Lines | Purpose |
|------|-------|---------|
| `model/FractalTrack.h` | ~80 | Track-level container |
| `model/FractalSequence.h` | ~120 | Per-pattern parameters |
| `engine/FractalTrackEngine.h` | ~130 | Engine class definition |
| `engine/FractalTrackEngine.cpp` | ~350 | Mutation + generation logic |
| `ui/pages/FractalTrackPage.h` | ~250 | Multi-section UI page |
| `ui/model/FractalTrackListModel.h` | ~50 | Track setup list |
| **Total new** | **~980** | |

### Files Modified (additions only)

| File | Lines Added | What Changes |
|------|------------|--------------|
| `model/Track.h` | ~30 | Add Fractal to TrackMode enum, Container, union |
| `model/Routing.h` | ~15 | Add FractalFirst..FractalLast routing targets |
| `engine/Engine.cpp` | ~3 | Add case to creation switch |
| `ui/Pages.h` | ~3 | Register FractalTrackPage |
| `ui/pages/TopPage.cpp` | ~2 | Route to FractalTrackPage |
| `ui/pages/TrackPage.cpp` | ~3 | Instantiate FractalTrackListModel |
| `ui/Ui.h` | ~2 | Include FractalTrackPage header |
| **Total modified** | **~58** | |

## RAM Assessment

### Current Budget Context (from resource-optimization TASK.md)

| Resource | Current | Limit | Headroom |
|----------|---------|-------|----------|
| `.data + .bss` | 119,960 (91.4%) | 128 KB | ~8 KB |
| `.ccmram_bss` | 54,096 (84.5%) | 64 KB | ~10 KB |
| Flash .text | 763,436 | 1 MB | ~261 KB |

### Model Impact (SRAM)

| Component | Estimated Size | Notes |
|-----------|---------------|-------|
| FractalTrack (per-track model) | ~880 B | 17 sequences at ~48 B + overhead |
| 8 tracks max | 880 B | Container sized by NoteTrack at 9,544 B; FractalTrack is smaller |
| **Model delta** | **~0 B** | Fits under existing container gate; no Track::_container inflation |

### Engine Impact (CCMRAM)

| Component | Estimated Size | Notes |
|-----------|---------------|-------|
| FractalTrackEngine | ~500 B | Under current 912 B gate; no CCMRAM container inflation |
| 8 engines max | 500 B | Fits within existing TrackEngineContainer |
| **Engine delta** | **~0 B** | No CCMRAM inflation if under current gate |

### Direct Additions

| Component | Size | Section |
|-----------|------|---------|
| MutationHistory (16 records inline) | 192 B | Engine (CCMRAM) |
| SelectedPressure | 16 B | Engine (CCMRAM) |
| Page instances | ~Page size | UI (.bss) |
| Routing targets | Negligible | .text |

**Verdict: FractalTrack is RAM-viable within current budget.** The model fits under the NoteTrack container gate and the engine fits under the current TeletypeTrackEngine gate. No `.data + .bss` or `.ccmram_bss` inflation through container/cliff effects. Direct additions are small (~200 B CCMRAM for history + pressure).

**However:** This analysis is estimated. Build STM32 release and probe ARM sizeof before merging any Phase 1 code. The FractalTrack model and engine must be measured on hardware, not assumed.

## Phased Implementation Plan

Phase descriptions follow in `.tasks/fractal-track-implementation/PHASEDPLAN.md`.

## Dependencies

| Task | Relationship |
|------|-------------|
| `resource-optimization` | **Blocks.** RAM headroom currently at 91.4% SRAM; the fractal model/engine adds direct CCMRAM and model storage. Current headroom is tight but estimated to fit. |
| `stochastic-track-port` | **Watch.** Active on feat/stochastic -- will consume next RAM from resource-optimization. FractalTrack is lower priority. |

## Blocks

Nothing directly blocked.
