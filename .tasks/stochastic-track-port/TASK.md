# Stochastic Track Port (Vinx → XFORMER)

## Goal

Port the Vinx fork's Stochastic track type to XFORMER. A probability-driven sequencer where each step has gate/note/length variation probabilities, stage repeats with modes, rest probabilities, octave ranges, and a loop-buffer caching mechanism.

**Why this matters:** Stochastic is one of Vinx's flagship track types alongside Logic and Arp. It enables generative sequencing with weighted probability distributions per step — a capability XFORMER lacks.

**Reference:** `temp-ref/vinx-performer/src/apps/sequencer/{model,engine,ui}/Stochastic*`

---

## Architecture Overview

### Model Layer

| Component | File | Lines | Description |
|-----------|------|-------|-------------|
| StochasticSequence | `model/StochasticSequence.h` | 814 | 64 steps, 2×uint32_t bit-packed step (8 B/step). 16 layers: gate, probability, length variation, note variation, octave, stage repeats, condition, slide, etc. |
| StochasticTrack | `model/StochasticTrack.h` | 341 | 17 sequences (16 patterns + 1 snapshot). Track-level: fill mode, cv update mode, biases (gate/retrigger/length/note probability), octave/transpose/rotate. |
| StochasticSequence.cpp | `model/StochasticSequence.cpp` | 392 | layerRange, layerDefaultValue, Step::layerValue, clear, shift, duplicate, serialize |
| StochasticTrack.cpp | `model/StochasticTrack.cpp` | ~100 | clear, writeRouted, serialize |

**Step bit packing:**
- `_data0` (32 bits): gate(1) + slide(1) + length(4) + lengthVarRange(4) + lengthVarProb(4) + note(7) + noteOctave(3) + noteVarProb(4) + noteOctaveProb(4)
- `_data1` (32 bits): bypassScale(1) + retrigger(3) + gateProb(4) + retriggerProb(4) + gateOffset(4) + condition(7) + stageRepeats(3) + stageRepeatMode(3) + 5 bits free

**Size estimate:**
- StochasticSequence: 64 steps × 8 B = 512 B
- StochasticTrack: 17 sequences × 512 B + track overhead ≈ 8,800 B
- **Compare:** NoteTrack = 9,544 B. StochasticTrack fits **under** the Track container gate.

### Engine Layer

| Component | File | Lines | Description |
|-----------|------|-------|-------------|
| StochasticEngine | `engine/StochasticEngine.h` | 217 | Tick-based engine with probability evaluation, gate/CV scheduling queues, loop buffer caching |
| StochasticEngine.cpp | `engine/StochasticEngine.cpp` | 687 | Core: triggerStep, evalStepGate, evalStepNote, evalStepLength, evalStepRetrigger, evalRestProbability, getNextWeightedPitch |

**Engine behavior:**
1. On each tick, evaluates step probabilities to select a pitch slot from active gates
2. Evaluates gate probability + bias, condition (Fill/NotFill/Pre/NotPre/First/Loop), length variation
3. Schedules gate/CV events into `SortedQueue<Gate, 16>` and `SortedQueue<Cv, 16>`
4. Caches evaluated steps into `_lockedSteps` (loop buffer) for deterministic replay
5. Supports "sequence loop" (sub-range of steps with independent first/last) vs "playback loop"
6. Fill modes: Gates, NextPattern, Condition
7. Stage repeats: Each/First/Middle/Last/Odd/Even/Triplets/Random

**Size estimate:**
- StochasticEngine: TrackEngine base (~120 B) + _lockedSteps (64×~28 B = 1,792 B) + queues (~224 B) + state (~200 B) ≈ **2,300 B**
- **Compare:** Current engine gate = TeletypeTrackEngine = 912 B
- **Delta:** +1,388 B × 8 tracks = **+11,104 B CCMRAM**
- Current CCMRAM: 54,804 B. After port: ~65,900 B. **CCMRAM limit = 64 KB. BLOCKED.**

### UI Layer

| Component | File | Lines | Description |
|-----------|------|-------|-------------|
| StochasticSequenceEditPage | `ui/pages/StochasticSequenceEditPage.h` | 87 | Step selection, section (bank) navigation, layer editing, generator overlay, context menus |
| StochasticSequenceEditPage.cpp | `ui/pages/StochasticSequenceEditPage.cpp` | 1,187 | Full edit page with step grid, detail view, layer switching, copy/paste/duplicate, init, generate |
| StochasticSequencePage | `ui/pages/StochasticSequencePage.h` | 31 | List page for pattern/snapshot selection |
| StochasticSequencePage.cpp | `ui/pages/StochasticSequencePage.cpp` | ~200 | Pattern list, context menu |
| StochasticSequenceListModel | `ui/model/StochasticSequenceListModel.h` | ~80 | List model for pattern names |
| StochasticTrackListModel | `ui/model/StochasticTrackListModel.h` | ~80 | List model for track settings |

**UI features:**
- 64-step grid with 4 banks (sections) of 16 steps
- Layer editing: Gate, GateProbability, GateOffset, Length, LengthVariation, NoteVariation, Octave, etc.
- Step selection with shift-persist
- Generator integration (`Container<StochasticSequenceBuilder>`)
- Launchpad generator overlay support
- Context menus: Init, Copy, Paste, Duplicate, Tie, Generate

### Generator Integration

Vinx's `SequenceBuilder.h` includes `StochasticSequenceBuilder` alongside `NoteSequenceBuilder` and `CurveSequenceBuilder`. The generator system supports stochastic via entropy targets and the 3-copy state machine.

---

## Critical Porting Issues

### 1. CCMRAM Budget — BLOCKER

StochasticEngine's inline `_lockedSteps` buffer dominates engine size:

```cpp
std::vector<StochasticLoopStep> _lockedSteps;  // caches up to 64 evaluated steps
```

StochasticLoopStep ≈ 28 B (int + bool + Step(8 B) + float + uint32_t + int). 64 × 28 = 1,792 B.

**Options:**
- **A) Heap-allocate `_lockedSteps`** (like SequenceBuilder's `_preview`): `StochasticLoopStep* _lockedSteps = nullptr`, allocate on first use. Reduces engine to ~600 B. CCMRAM delta: ~(600-912)×8 = **-2,496 B** (actually saves CCMRAM!). Requires `new`/`delete` in reset/changePattern/destructor.
- **B) Cap buffer at 16 steps**: Reduces to 448 B. Loses 64-step loop caching.
- **C) Accept CCMRAM overflow**: Not recommended — leaves no margin.

**Recommendation:** Option A (heap-allocate `_lockedSteps`). Matches existing pattern in SequenceBuilder.

### 2. std::vector in Engine — BLOCKER

Three uses in `StochasticEngine.cpp`:

```cpp
// Line 49: rest probability evaluation
std::vector<StochasticStep> probability;

// Line 449: pitch slot probability evaluation  
std::vector<StochasticStep> probability;

// Line 189: _lockedSteps member
std::vector<StochasticLoopStep> _lockedSteps;
```

`std::vector` is not used elsewhere in XFORMER engine code. Must replace with fixed arrays or heap.

**Fix:** Replace with `StochasticStep _probability[CONFIG_STEP_COUNT]` and heap-allocated `_lockedSteps`.

### 3. C++11 Random — BLOCKER

```cpp
// Line 486-488
std::mt19937 e2(m);
std::normal_distribution<float> normal_dist(mean, 2);
rnd = std::round(normal_dist(e2));
```

`std::mt19937` and `std::normal_distribution` are **not available** in ARM nano newlib. Also `std::rand()` and `time(NULL)` are used.

**Fix:** Replace with XFORMER's `Random` class and a simple normal approximation (Box-Muller or central limit theorem with `Random::nextRange()`).

### 4. std::sort on std::vector

```cpp
std::sort(std::begin(probability), std::end(probability), sortTaskByProbRev);
```

With fixed arrays, use `std::sort(arr, arr + count, ...)`.

### 5. TrackMode Enum & Serialization

XFORMER's `TrackMode` enum must add `Stochastic`:
- `trackModeName()` → "Stochastic"
- `trackModeSerialize()` → needs value 7 (after Teletype=6)
- `Track::_container` → add `StochasticTrack`
- `Track::_track` union → add `StochasticTrack *stochastic`
- `Track::setTrackIndex()`, `initContainer()`, all accessors

**Serialization risk:** Adding a new track mode changes project file format. Need forward compatibility: old projects without Stochastic tracks must load cleanly.

### 6. Routing Targets

Vinx adds routing targets specific to Stochastic:
- `RestProbability2`, `RestProbability4`, `RestProbability8`
- `LowOctaveRange`, `HighOctaveRange`
- `LengthModifier`
- `GateProbabilityBias`, `RetriggerProbabilityBias`, `LengthBias`, `NoteProbabilityBias`

These must be added to XFORMER's `Routing::Target` enum and `targetInfos[]`.

### 7. Project Integration

- `Project::selectedStochasticSequence()` / `setSelectedStochasticSequence()`
- `Project::selectedStochasticSequenceLayer()` / `setSelectedStochasticSequenceLayer()`
- `PlayState::trackState()` — pattern/mute/solo already generic
- `Engine` track factory — add `StochasticEngine` instantiation
- `Pages` struct — add `StochasticSequenceEditPage`, `StochasticSequencePage`
- `OverviewPage` — add stochastic track rendering

---

## Phased Implementation Plan

### Phase 1: Model Layer + Serialization (RAM-safe)

**Goal:** Add StochasticSequence and StochasticTrack to model. Verify Track container size.

**Files:**
- `src/apps/sequencer/model/StochasticSequence.h` (port from Vinx)
- `src/apps/sequencer/model/StochasticSequence.cpp`
- `src/apps/sequencer/model/StochasticTrack.h`
- `src/apps/sequencer/model/StochasticTrack.cpp`
- `src/apps/sequencer/model/Track.h` — add StochasticTrack to container
- `src/apps/sequencer/model/Project.h/.cpp` — selectedStochasticSequence accessors

**RAM check:** Build STM32 release. Verify `sizeof(Track)` unchanged or decreased. `StochasticTrack` must be ≤ `NoteTrack` (9,544 B).

**Hardware check:** Project save/load with Stochastic track. Old projects load without regression.

---

### Phase 2: Engine Compaction (RAM-critical)

**Goal:** Port StochasticEngine with heap-allocated `_lockedSteps` and all std::vector/std::rand replacements.

**Files:**
- `src/apps/sequencer/engine/StochasticEngine.h`
- `src/apps/sequencer/engine/StochasticEngine.cpp`
- `src/apps/sequencer/engine/Engine.cpp` — add to track factory

**Changes:**
1. Replace `_lockedSteps` std::vector with heap-allocated `StochasticLoopStep*`
2. Replace local std::vector probability arrays with fixed arrays on stack
3. Replace `std::mt19937` + `std::normal_distribution` with `Random` + Box-Muller approx
4. Replace `std::rand()` / `time(NULL)` with `Random`
5. Replace `std::sort(std::begin, std::end)` with `std::sort(arr, arr+count)`

**RAM check:** Build STM32 release. Run ARM `sizeof` probe:
- `sizeof(StochasticEngine)` — target < 1,000 B (under current 912 B gate would be ideal; under 1,200 B acceptable)
- `sizeof(Engine::TrackEngineContainer)` — must not exceed current gate significantly
- `.ccmram_bss` — must stay under 60 KB (hard limit 64 KB)

**Acceptance:** If `sizeof(StochasticEngine)` > 1,200 B, iterate on compaction. If CCMRAM > 60 KB, stop and redesign.

---

### Phase 3: Routing Targets

**Goal:** Add Stochastic-specific routing targets.

**Files:**
- `src/apps/sequencer/model/Routing.h` — add targets to enum
- `src/apps/sequencer/model/Routing.cpp` — add to targetInfos, targetName, targetSerialize

**Note:** Routing enum reordering affects serialization. Must use `targetSerialize()` decoupling (already in place in XFORMER).

---

### Phase 4: UI Layer

**Goal:** Port StochasticSequenceEditPage, StochasticSequencePage, list models.

**Files:**
- `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.h/.cpp`
- `src/apps/sequencer/ui/pages/StochasticSequencePage.h/.cpp`
- `src/apps/sequencer/ui/model/StochasticSequenceListModel.h`
- `src/apps/sequencer/ui/model/StochasticTrackListModel.h`
- `src/apps/sequencer/ui/pages/Pages.h` — add page instances
- `src/apps/sequencer/ui/painters/SequencePainter.h/.cpp` — add stochastic draw routines

**RAM check:** UI pages are in `.bss` (Pages struct). Estimate: StochasticSequenceEditPage ≈ NoteSequenceEditPage + list model overhead ≈ similar size. Should be under existing page gate.

---

### Phase 5: Generator Integration

**Goal:** Add StochasticSequenceBuilder to generator system.

**Files:**
- `src/apps/sequencer/engine/generators/SequenceBuilder.h` — add StochasticSequenceBuilder typedef
- `src/apps/sequencer/engine/generators/Generator.cpp` — add to Container

**Depends on:** generator-preview-apply (already done — 3-copy state machine in place).

---

### Phase 6: Hardware Verification

**Goal:** Full hardware test.

**Test plan:**
1. Create Stochastic track, verify step grid editing
2. Set gate/note probabilities, verify weighted selection
3. Test stage repeats (Each/First/Last/Odd/Even/Triplets/Random)
4. Test rest probabilities (1/2/4/8 step)
5. Test sequence loop vs playback loop
6. Test fill modes (Gates/NextPattern/Condition)
7. Test routing targets (probability biases, octave range)
8. Test generator integration
9. Test project save/load with Stochastic tracks
10. Regression test: Note/Curve/Tuesday tracks still work

---

## RAM Assessment

### Model (SRAM .data + .bss)

| Component | Size | Impact |
|-----------|------|--------|
| StochasticTrack | ~8,800 B | Under NoteTrack gate (9,544 B). Track container unchanged. |
| StochasticSequenceEditPage | ~NoteSequenceEditPage size | Pages struct grows by page size. Existing pages already large; need sizeof probe. |
| **Total model delta** | ~0 B (track container) + page size | Acceptable if page stays under current max page size. |

### Engine (CCMRAM)

| Component | Size | Impact |
|-----------|------|--------|
| StochasticEngine (heap _lockedSteps) | ~600-800 B | Under current 912 B gate → **saves CCMRAM** |
| StochasticEngine (inline _lockedSteps) | ~2,300 B | Exceeds gate by +1,388 B × 8 = **+11,104 B CCMRAM** → **BLOCKED** |

**Critical decision:** Heap-allocate `_lockedSteps` in Phase 2. This is non-negotiable for RAM acceptance.

### Current Budget Context

- `.data + .bss = 118,884` (92.9% of 128 KB) — tight but headroom exists
- `.ccmram_bss = 54,804` (53.4% of 64 KB) — comfortable if engine stays under gate
- If engine compaction succeeds, Stochastic track is **RAM-viable**
- If engine stays inline, Stochastic track is **RAM-blocked**

---

## Open Questions

- [ ] Does Vinx's `std::normal_distribution` length modifier produce audible differences vs a simpler random offset? Can we approximate with `Random::nextRange()` + clamp?
- [ ] Should Stochastic track use the same 64-step bank visualization as GeneratorPage (Phase D), or keep Vinx's simpler grid?
- [ ] Does Stochastic track need Launchpad controller support in Phase 1, or defer to `launchpad-track-port`?
- [ ] Should the `_lockedSteps` heap allocation use `std::nothrow` and fail gracefully (skip loop caching)?

---

## Effort Estimate

| Phase | Effort |
|-------|--------|
| Phase 1: Model + Serialization | ~3h |
| Phase 2: Engine Compaction | ~4h |
| Phase 3: Routing Targets | ~1h |
| Phase 4: UI Layer | ~6h |
| Phase 5: Generator Integration | ~1h |
| Phase 6: Hardware Verification | ~3h |
| **Total** | **~18h** |

## Next Action

**Phase 1 prep:** Verify `sizeof(StochasticTrack)` under NoteTrack gate by creating a temporary sizeof probe in `build/stm32/release`. If pass, begin Phase 1 model port.

## Depends On

- `resource-optimization` (for RAM baseline)
- `generator-preview-apply` (for 3-copy SequenceBuilder state machine — already done)

## Blocks

Nothing directly blocked, but this is a large feature that should wait until RAM headroom is confirmed.
