# Enhanced Stochastic Track Port (Vinx → XFORMER)

## Goal

Port the Vinx fork's Stochastic track type to XFORMER, then extend the MVP with global musical constraints from `docs/superpowers/specs/2026-05-17-enhanced-stochastic-track-design.md`: signed Pitch Ticket Table, scale-mask exclusion, Semi/Mask rotation, Linearity, Min/Max note range, Marbles-style spread/bias/steps pitch shaping, and a compact captured-event lock buffer.

**Why this matters:** Stochastic is one of Vinx's flagship track types alongside Logic and Arp. The enhanced MVP gives XFORMER a mono generative sequencer with step probability, global pitch shaping, and lockable captured performances without taking on polyphony or drift scope yet.

**References:**
- `docs/superpowers/specs/2026-05-17-enhanced-stochastic-track-design.md` — controlling MVP spec.
- `.tasks/stochastic-track-port/UI-DESIGN.md` — XFORMER-native UI design plan.
- `temp-ref/vinx-performer/src/apps/sequencer/{model,engine,ui}/Stochastic*` — Vinx Stochastic source.
- `../others/mutable/marbles/random/output_channel.cc`
- `../others/mutable/marbles/random/distributions.h`
- `../others/mutable/marbles/random/random_sequence.h`
- `temp-ref/o_c/O_C-Phazerville/software/src/applets/ProbabilityMelody.h` — ProbMeloD signed ticket table, scale mask, rotation, range, deterministic loop seed.
- `temp-ref/o_c/O_C-Phazerville/docs/_applets/ProbMeloD.md` — raffle-ticket UI framing and mask/semi semantics.
- `temp-ref/o_c/O_C-Phazerville/software/src/HSProbLoopLinker.h` — non-applicable reference for why Performer does not need the Hemisphere linker pattern.

## MVP Scope

Included:
- Vinx Stochastic model, engine, and core grid UI.
- Compact captured-event lock buffer.
- Signed Pitch Ticket Table (12 semitone tickets; `-1` excluded, `0` included with zero tickets).
- Semi/Mask rotation for key and chord movement.
- Linearity / melodic smoothing.
- Min/Max note range.
- Marbles spread/bias/steps pitch shaping.
- Accent and slew/legato probability only if they stay RAM/queue-cheap.

Deferred post-MVP gates:
- Split rhythm/melody locks.
- Generative drift/mutation of locked buffers.
- Reverse locked-loop playback.
- Polyphonic ghost voices and neighboring-track spillover.
- Full generator integration, unless the core port is already verified and RAM-flat.

---

## Architecture Overview

### Model Layer

| Component | File | Lines | Description |
|-----------|------|-------|-------------|
| StochasticSequence | `model/StochasticSequence.h` | 814 | 64 steps, 2×uint32_t bit-packed step (8 B/step). 16 layers: gate, probability, length variation, note variation, octave, stage repeats, condition, slide, etc. |
| StochasticTrack | `model/StochasticTrack.h` | 341 | 17 sequences (16 patterns + 1 snapshot). Track-level: fill mode, cv update mode, biases, octave/transpose/rotate, plus enhanced MVP settings. |
| StochasticSequence.cpp | `model/StochasticSequence.cpp` | 392 | layerRange, layerDefaultValue, Step::layerValue, clear, shift, duplicate, serialize |
| StochasticTrack.cpp | `model/StochasticTrack.cpp` | ~100 | clear, writeRouted, serialize |

**Step bit packing:**
- `_data0` (32 bits): gate(1) + slide(1) + length(4) + lengthVarRange(4) + lengthVarProb(4) + note(7) + noteOctave(3) + noteVarProb(4) + noteOctaveProb(4)
- `_data1` (32 bits): bypassScale(1) + retrigger(3) + gateProb(4) + retriggerProb(4) + gateOffset(4) + condition(7) + stageRepeats(3) + stageRepeatMode(3) + 5 bits free

**Vinx size estimate:**
- StochasticSequence: 64 steps × 8 B = 512 B + track-level members + padding = **552 B** (measured)
- StochasticTrack: 17 sequences × 552 B + track overhead = **9,416 B** (measured)
- **Compare:** NoteTrack = 9,544 B. StochasticTrack is **128 B smaller** than NoteTrack. ✅ Track container safe.

**Enhanced MVP model additions:**
- `pitchWeights[12]`: `int8_t`
- `linearity`: `uint8_t`
- `semiRotation`, `maskRotation`: `int8_t`
- `lock`: `bool`
- `loopFirst`, `loopLast`: `uint8_t`
- `marblesMode`: `uint8_t` or compact enum
- `marblesSpread`, `marblesBias`, `marblesSteps`: `uint8_t`
- `minNote`, `maxNote`: `uint8_t`
- `accentProb`, `legatoProb`: `uint8_t` only if accepted after size probe

**PWT semantic correction:** Use `int8_t pitchWeights[12]`, not `uint8_t[12]`. `-1` means excluded from the scale mask, `0` means included but no raffle tickets, and positive values are ticket counts. This is byte-neutral versus `uint8_t[12]` and enables ProbMeloD-style mask rotation.

**Model acceptance:** The Vinx baseline is under the current `NoteTrack` gate, but the enhanced settings require an STM32 release `sizeof` probe. Do not assume the final model is accepted until `StochasticTrack <= NoteTrack` is measured on ARM.

### Engine Layer

| Component | File | Lines | Description |
|-----------|------|-------|-------------|
| StochasticEngine | `engine/StochasticEngine.h` | 217 | Tick-based engine with probability evaluation, gate/CV scheduling queues, loop buffer caching |
| StochasticEngine.cpp | `engine/StochasticEngine.cpp` | 687 | Core: triggerStep, evalStepGate, evalStepNote, evalStepLength, evalStepRetrigger, evalRestProbability, getNextWeightedPitch |

**Engine behavior:**
1. On each tick, evaluates step probabilities to select a pitch slot from active gates
2. Applies enhanced global constraints: signed PWT, scale mask, Semi/Mask rotation, Linearity, Min/Max range, optional Marbles spread/bias/steps shaping
3. Evaluates gate probability + bias, condition (Fill/NotFill/Pre/NotPre/First/Loop), length variation
4. Schedules gate/CV events into fixed `SortedQueue<Gate, 16>` and `SortedQueue<Cv, 16>`
5. Caches evaluated events into a compact lock buffer for deterministic replay
6. Supports loop windowing and rotation over captured events
7. Fill modes: Gates, NextPattern, Condition
8. Stage repeats: Each/First/Middle/Last/Odd/Even/Triplets/Random

**Size estimate:**
- StochasticEngine (inline _lockedSteps): TrackEngine base + _lockedSteps[64] × 28 B + queues + state = **2,228 B** (measured)
- StochasticEngine (heap _lockedSteps): Same but pointer instead of array = **440 B** (measured)
- **Compare:** Current engine gate = TeletypeTrackEngine = 912 B
- Inline delta: (2,228 - 912) × 8 = **+10,528 B CCMRAM** → BLOCKED
- Heap delta: (440 - 912) × 8 = **-3,776 B CCMRAM** → **SAVES CCMRAM**
- Current CCMRAM: 54,804 B. With heap engine: ~51,028 B. Well under 64 KB limit. ✅

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
- Enhanced config controls: PWT, Linearity, range, Marbles mode/spread/bias/steps, lock/window controls
- Launchpad generator overlay support
- Context menus: Init, Copy, Paste, Duplicate, Tie, Generate

### Generator Integration (Post-MVP)

Vinx's `SequenceBuilder.h` includes `StochasticSequenceBuilder` alongside `NoteSequenceBuilder` and `CurveSequenceBuilder`. This is useful follow-up work, but it is not part of the enhanced mono MVP unless the core port is hardware-verified and RAM-flat.

---

## Critical Porting Issues

### 1. MVP Scope Alignment — BLOCKER

The old task plan was a Vinx-only port. The controlling spec is now the enhanced mono MVP:
- Vinx foundation
- compact captured-event lock buffer
- PWT
- Scale-mask exclusion with `-1` ticket values
- Semi/Mask rotation
- Linearity
- Min/Max range
- Marbles spread/bias/steps
- optional cheap accent/legato

Do not implement split locks, drift, reverse playback, ghost voices, or generator integration as core acceptance criteria for the first pass.

### 2. CCMRAM Budget — BLOCKER

Vinx's inline `_lockedSteps` buffer dominates engine size:

```cpp
std::vector<StochasticLoopStep> _lockedSteps;  // caches up to 64 evaluated steps
```

StochasticLoopStep ≈ 28 B (int + bool + Step(8 B) + float + uint32_t + int). 64 × 28 = 1,792 B.

**Options:**
- **A) Compact captured-event buffer**: heap/pool allocate a fixed-capacity replay buffer containing only evaluated event data. This preserves Vinx locked-loop semantics without copying full `Step` objects or vectors.
- **B) Heap-allocate Vinx-shaped `StochasticLoopStep`**: workable, but keeps the oversized event shape and encourages a direct vector-style port.
- **C) Cap buffer at 16 steps**: Reduces memory but loses 64-step loop caching.
- **D) Accept CCMRAM overflow**: Not recommended — leaves no margin.

**Recommendation:** Option A. The engine object must stay under the current 912 B engine gate; the captured-event buffer must not live inline in CCMRAM.

### 3. std::vector in Engine — BLOCKER

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

**Fix:** Replace rest and pitch probability vectors with direct weighted sampling over fixed small domains (4 rest weights, up to 12 pitch slots). Replace `_lockedSteps` with the compact captured-event buffer. No `std::vector`, no `std::sort`, no vector copies by value, no slicing.

### 4. C++11 Random — BLOCKER

```cpp
// Line 486-488
std::mt19937 e2(m);
std::normal_distribution<float> normal_dist(mean, 2);
rnd = std::round(normal_dist(e2));
```

`std::mt19937` and `std::normal_distribution` are **not available** in ARM nano newlib. Also `std::rand()` and `time(NULL)` are used.

**Fix:** Replace with XFORMER's `Random` class and a simple normal approximation (Box-Muller or central limit theorem with `Random::nextRange()`).

### 5. Marbles Logic — MVP Requirement

Marbles spread/bias/steps must be added as a pitch-selection shape, not as a full Marbles subsystem:
- Use Marbles source as a control-law reference only.
- Do not import Marbles `OutputChannel`, quantizer, lag processor, register mode, or lookup-table stack unless a later proof shows the cheap approximation is musically wrong.
- Marbles shaping runs after allowed pitch slots are established by step gates, note probabilities, PWT, and range.
- The evaluated CV produced by Marbles shaping is stored in the captured-event cache.

### 6. ProbMeloD Pitch Semantics — MVP Requirement

ProbMeloD contributes the cleanest pitch-table mechanics:
- `int8_t weights[12]` where `-1` is excluded from the scale mask and non-negative values remain in the mask.
- `0` is not played by direct weighting, but it rotates with the included scale degrees.
- Semi rotation shifts all 12 ticket slots for key changes.
- Mask rotation rotates only included scale degrees while excluded notes remain fixed, enabling chord movement inside a key mask.
- Range is a pre-selection filter: out-of-range notes never enter the raffle pool.
- Raffle-ticket UI language should be used for PWT: positive value equals number of tickets in the draw.

Reference implementation:
- `ProbabilityMelody.h:170-173` — edit range `-1..MAX`.
- `ProbabilityMelody.h:234-243` — non-negative mask.
- `ProbabilityMelody.h:269-304` — masked and semitone rotation.
- `ProbabilityMelody.h:306-323` — range-filtered weighted draw.
- `ProbabilityMelody.h:326-340` — deterministic loop seed.
- `ProbMeloD.md:32` and `ProbMeloD.md:42-58` — raffle and mask/semi semantics.

Do not adopt:
- `HSProbLoopLinker.h` linker architecture.
- ProbMeloD's dual independently clocked pitch channels.
- Full CV modulation bitmask matrix in MVP.

Deterministic seed may be useful for preview/regeneration, but it does not replace the captured-event lock buffer because Stochastic lock must freeze gate, CV, length, retrigger, slide, offset, and rest decisions.

### 7. std::sort on std::vector

```cpp
std::sort(std::begin(probability), std::end(probability), sortTaskByProbRev);
```

Do not replace this with `std::sort(arr, arr + count, ...)` unless a later algorithm actually requires ordering. Direct weighted sampling is cheaper and clearer here.

### 8. TrackMode Enum & Serialization

XFORMER's `TrackMode` enum must add `Stochastic`:
- `trackModeName()` → "Stochastic"
- `trackModeSerialize()` → needs value 7 (after Teletype=6)
- `Track::_container` → add `StochasticTrack`
- `Track::_track` union → add `StochasticTrack *stochastic`
- `Track::setTrackIndex()`, `initContainer()`, all accessors

**Serialization risk:** Adding a new track mode changes project file format. Need forward compatibility: old projects without Stochastic tracks must load cleanly.

### 9. Routing Targets

Vinx adds routing targets specific to Stochastic:
- `RestProbability2`, `RestProbability4`, `RestProbability8`
- `LowOctaveRange`, `HighOctaveRange`
- `LengthModifier`
- `GateProbabilityBias`, `RetriggerProbabilityBias`, `LengthBias`, `NoteProbabilityBias`
- Enhanced MVP candidates: `MarblesSpread`, `MarblesBias`, `MarblesSteps`, `Linearity`, `SemiRotation`, `MaskRotation`, and possibly PWT editing/routing if UI space and serialization remain clean.

These must be added to XFORMER's `Routing::Target` enum and `targetInfos[]`.

### 10. Project Integration

- `Project::selectedStochasticSequence()` / `setSelectedStochasticSequence()`
- `Project::selectedStochasticSequenceLayer()` / `setSelectedStochasticSequenceLayer()`
- `PlayState::trackState()` — pattern/mute/solo already generic
- `Engine` track factory — add `StochasticEngine` instantiation
- `Pages` struct — add `StochasticSequenceEditPage`, `StochasticSequencePage`
- `OverviewPage` — add stochastic track rendering

---

## Phased Implementation Plan

### Phase 1: Model Layer + Serialization (RAM-safe)

**Goal:** Add StochasticSequence and StochasticTrack to model, including enhanced MVP track settings. Verify Track container size on STM32 release.

**Files:**
- `src/apps/sequencer/model/StochasticSequence.h` (port from Vinx)
- `src/apps/sequencer/model/StochasticSequence.cpp`
- `src/apps/sequencer/model/StochasticTrack.h`
- `src/apps/sequencer/model/StochasticTrack.cpp`
- `src/apps/sequencer/model/Track.h` — add StochasticTrack to container
- `src/apps/sequencer/model/Project.h/.cpp` — selectedStochasticSequence accessors
- Enhanced fields: signed PWT tickets, Semi/Mask rotation, Linearity, Min/Max range, lock/window controls, Marbles mode/spread/bias/steps, optional accent/legato

**RAM check:** Build STM32 release. Verify `sizeof(Track)` unchanged or decreased. `StochasticTrack` must be ≤ `NoteTrack` (9,544 B).

**Hardware check:** Project save/load with Stochastic track. Old projects load without regression.

---

### Phase 2: Engine Foundations + Compaction (RAM-critical)

**Goal:** Implement StochasticEngine as an XFORMER-native engine that preserves Vinx locked-loop semantics without porting Vinx's vector/sort/slicing implementation.

**Locked-loop semantic target:** A locked loop is a captured playback result. Once a step has been evaluated into the lock buffer, later edits to the source sequence step must not change that locked event's gate result, CV, length, retrigger, slide, or gate offset. Current sequence loop-window controls (`bufferLoopLength`, `sequenceFirstStep`, `sequenceLastStep`, `sequenceLength`) may still affect which cached events are replayed, matching Vinx's current behavior.

**Files:**
- `src/apps/sequencer/engine/StochasticEngine.h`
- `src/apps/sequencer/engine/StochasticEngine.cpp`
- `src/apps/sequencer/engine/Engine.cpp` — add to track factory

**Changes:**
1. Replace `_lockedSteps` `std::vector<StochasticLoopStep>` with a compact heap/pool allocated fixed-capacity replay buffer.
2. Store evaluated replay events, not mutable model pointers. The cached event should contain only replay data such as source step index/rest marker, gate flag, slide flag, gate offset, evaluated CV, evaluated length ticks, and evaluated retrigger count.
3. Do not copy full vectors or subarrays during playback. Replace Vinx `slicing()` behavior with direct index arithmetic over the cached buffer and current loop-window settings.
4. Replace rest and pitch probability vectors with direct weighted sampling over the small fixed domains (4 rest weights, up to 12 pitch slots). No `std::vector`, no `std::sort`.
5. Replace `std::mt19937` + `std::normal_distribution` with `Random` and a cheap bounded approximation for the length modifier.
6. Replace `std::rand()` / `time(NULL)` with per-engine `Random` seeded from deterministic track/sequence state or an existing XFORMER timing seed.
7. Keep gate/CV scheduling behavior aligned with NoteTrackEngine: fixed `SortedQueue`s, no runtime allocation in the tick hot path except lock-buffer creation/clear at explicit loop-cache boundaries.

**Implementation shape:**

```cpp
struct LockedStep {
    int8_t sourceStep;      // -1 = rest
    uint8_t gateOffset;
    uint8_t retrigger;
    uint8_t flags;          // gate, slide
    uint16_t lengthTicks;
    float cv;
};
```

This preserves Vinx's captured-performance behavior while avoiding the 28 B `StochasticLoopStep`, full `Step` copy, vector metadata, vector copies by value, and per-step slicing allocations.

**RAM check:** Build STM32 release. Run ARM `sizeof` probe:
- `sizeof(StochasticEngine)` — target under current 912 B engine gate; anything above the gate must identify exact members and CCMRAM delta
- `sizeof(Engine::TrackEngineContainer)` — must not exceed current gate significantly
- `.ccmram_bss` — must stay under 60 KB (hard limit 64 KB)

**Acceptance:** If `sizeof(StochasticEngine)` exceeds the current engine gate, iterate on compaction before UI/generator work. If CCMRAM > 60 KB, stop and redesign.

---

### Phase 3: Global Pitch Logic

**Goal:** Implement enhanced MVP pitch selection: signed PWT tickets, scale-mask exclusion, Semi/Mask rotation, Linearity, Min/Max range, and Marbles spread/bias/steps shaping.

**Files:**
- `src/apps/sequencer/engine/StochasticEngine.cpp`
- `src/apps/sequencer/model/StochasticTrack.h/.cpp`
- `src/apps/sequencer/model/StochasticSequence.h/.cpp` if sequence-level accessors are needed

**Rules:**
1. PWT uses `int8_t[12]`: `-1` excluded, `0` included with zero tickets, positive values are raffle tickets.
2. Normal weighted mode uses step gates/probabilities plus PWT, Semi/Mask rotation, Linearity, and Min/Max range.
3. Range filters the raffle pool before selection; do not clamp selected output notes after the draw.
4. Marbles mode replaces the final pitch-slot choice after allowed slots are known.
5. Marbles mode uses cheap local shaping based on `spread`, `bias`, and `steps`.
6. Captured events store the evaluated CV, so locked loops are independent of later source edits.

---

### Phase 4: Loop Controls

**Goal:** Implement captured-event lock buffer, loop windowing, and loop rotation.

**Files:**
- `src/apps/sequencer/engine/StochasticEngine.h/.cpp`
- `src/apps/sequencer/model/StochasticTrack.h/.cpp`
- UI files only for controls required to exercise lock/window/rotation

**Acceptance:** Locked events replay captured gate, CV, length, retrigger, slide, and gate offset. Later step edits must not affect the locked event until clear/regenerate.

---

### Phase 5: Dynamics + Routing Targets

**Goal:** Add cheap accent/legato if queue behavior and RAM remain flat, then add required Stochastic routing targets.

**Files:**
- `src/apps/sequencer/engine/StochasticEngine.cpp`
- `src/apps/sequencer/model/Routing.h`
- `src/apps/sequencer/model/Routing.cpp`

**Note:** Routing enum reordering affects serialization. Must use `targetSerialize()` decoupling (already in place in XFORMER).

---

### Phase 6: UI Layer

**Goal:** Implement the XFORMER-native Stochastic UI from `.tasks/stochastic-track-port/UI-DESIGN.md`: core step grid, visual pitch/distribution pages, captured lock page, and compact track console. Do not clone Vinx's list-heavy UI directly.

**Files:**
- `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.h/.cpp`
- `src/apps/sequencer/ui/pages/StochasticSequencePage.h/.cpp`
- `src/apps/sequencer/ui/model/StochasticSequenceListModel.h`
- `src/apps/sequencer/ui/model/StochasticTrackListModel.h`
- `src/apps/sequencer/ui/pages/Pages.h` — add page instances
- `src/apps/sequencer/ui/painters/SequencePainter.h/.cpp` — add stochastic draw routines

**Enhanced UI requirements:**
- Preserve Performer header/footer conventions from Note, Curve, Indexed, DiscreteMap, and Tuesday pages.
- Prefer custom visual pages over list models for PWT, Marbles, lock buffer, and probability overview.
- PWT visual editor.
- Linearity, Min/Max range.
- Marbles Mode, Spread, Bias, Steps.
- Lock, loop first/last, and rotation controls via context menu + encoder where possible.
- `StochasticTrackListModel` may exist as a fallback/settings bridge, but normal performance editing should live on visual pages.

**RAM check:** UI pages are in `.bss` (Pages struct). Estimate: StochasticSequenceEditPage ≈ NoteSequenceEditPage + list model overhead ≈ similar size. Should be under existing page gate.

---

### Phase 7: Validation

**Goal:** Full STM32 size and hardware verification.

**Size checks:**
- `sizeof(StochasticTrack)` <= `sizeof(NoteTrack)`
- `sizeof(StochasticEngine)` <= current engine gate, or exact CCMRAM delta documented
- `.data`, `.bss`, `.ccmram_bss`
- UI page size impact

**Hardware checks:**
1. Create Stochastic track, verify step grid editing.
2. Set gate/note probabilities, verify weighted selection.
3. Verify PWT, Linearity, Min/Max range.
4. Verify `-1` excluded notes stay fixed under Mask rotation and never enter the draw.
5. Verify `0` included notes rotate with the mask but do not play without tickets.
6. Verify Semi rotation changes key and Mask rotation changes chord shape within the included mask.
7. Verify Marbles Spread/Bias/Steps are musically distinct and captured into locked loops.
8. Test stage repeats (Each/First/Last/Odd/Even/Triplets/Random).
9. Test rest probabilities (1/2/4/8 step).
10. Test captured lock buffer, loop windowing, and rotation.
11. Test fill modes (Gates, NextPattern, Condition).
12. Test routing targets.
13. Test project save/load with Stochastic tracks.
14. Regression test: Note/Curve/Tuesday/Teletype tracks still work.

---

### Post-MVP Gates

- Split rhythm/melody locks.
- Drift/mutation of locked buffers.
- Reverse playback.
- Ghost voices / neighboring-track spillover.
- StochasticSequenceBuilder / generator integration.

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
| StochasticEngine (compact captured-event buffer) | target < 912 B inline | Buffer pointer/state only in engine container; event storage heap/pool allocated outside CCMRAM gate. |
| StochasticEngine (heap Vinx-shaped _lockedSteps) | ~600-800 B | Likely under current 912 B gate, but keeps oversized event shape. |
| StochasticEngine (inline _lockedSteps) | ~2,300 B | Exceeds gate by +1,388 B × 8 = **+11,104 B CCMRAM** → **BLOCKED** |

**Critical decision:** Use a compact captured-event buffer in Phase 2. The buffer must not live inline in the engine container; heap/pool allocation is the mechanism, not the whole optimization.

### Current Budget Context

- `.data + .bss = 118,884` (92.9% of 128 KB) — tight but headroom exists
- `.ccmram_bss = 54,804` (53.4% of 64 KB) — comfortable if engine stays under gate
- If engine compaction succeeds, Stochastic track is **RAM-viable**
- If engine stays inline, Stochastic track is **RAM-blocked**

---

## Why Vinx `_lockedSteps` Is Unpacked (and What We Keep)

### Model Step vs Engine Cache Step

| | Model Step | Engine Cache Step |
|---|---|---|
| **NoteSequence::Step** | 8 B (2×uint32_t bitfields) | — |
| **StochasticSequence::Step** | 8 B (2×uint32_t bitfields) | — |
| **StochasticLoopStep** | — | **28 B** (unpacked evaluated results) |

The model step stores *probability settings* (e.g., `gateProbability = 12`, `lengthVariationRange = 3`). The engine cache stores *evaluated results* after rolling dice:
- `float _noteValue` — computed CV voltage after scale lookup (4 B)
- `uint32_t _stepLength` — computed duration in ticks after probability roll (4 B)
- `int _stepRetrigger` — evaluated retrigger count after probability roll (4 B)
- `bool _gate` — final gate on/off after probability + condition evaluation (1 B + padding)
- `StochasticStepRaw _step` — full copy of the source step for gate offset, slide, etc. (8 B)
- `int _index` — which model step was selected (4 B)

### Full Vinx Shape Is the Wrong Optimization Target

Could we pack Vinx `StochasticLoopStep` tighter?
- `_index` (0-63) → `uint8_t` saves **3 B**
- `_stepRetrigger` (1-4) → fits in 2 bits, but in practice needs its own byte
- `_stepLength` is duration in ticks → could be `uint16_t` if max < 65K
- `_noteValue` is a float → **needs 4 B** for CV precision

Best-case aggressive packing of the Vinx-shaped object is still a compromise because it preserves the full copied `Step`. The better target is an evaluated replay event: source/rest marker, gate, slide, gate offset, CV, length ticks, and retrigger.

### Why Compact Heap/Pool Storage Is the Right Fix

Bit-packing the Vinx object adds complexity for marginal gain. Compact heap/pool storage is:
- **Semantically correct** — locks replay captured events, not mutable model pointers.
- **Smaller** — avoids full `Step` copies, vector metadata, vector copies by value, and slicing.
- **Engine-gate safe** — the engine container pays for pointer/state, not 64 cached events.
- **Consistent with XFORMER** — allocation can follow existing explicit-buffer patterns such as `SequenceBuilder::_preview`, while keeping the tick hot path allocation-free.

**Conclusion:** Do not port Vinx `_lockedSteps` literally. Store compact captured events outside the engine container.

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
| Phase 1: Model + Serialization | ~3-4h |
| Phase 2: Engine Foundations + Compaction | ~4-5h |
| Phase 3: Global Pitch Logic | ~3h |
| Phase 4: Loop Controls | ~2h |
| Phase 5: Dynamics + Routing Targets | ~2h |
| Phase 6: UI Layer | ~6h |
| Phase 7: Validation | ~3h |
| **Total** | **~23-25h** |

## Next Action

**Phase 1 prep:** Verify enhanced `sizeof(StochasticTrack)` under the NoteTrack gate by creating a temporary STM32 release sizeof probe. Include signed PWT tickets, Semi/Mask rotation, Linearity, range, Marbles fields, lock/window fields, and optional accent/legato fields in the probe.

## Depends On

- `resource-optimization` (for RAM baseline)
- `generator-preview-apply` (for 3-copy SequenceBuilder state machine — already done)

## Blocks

Nothing directly blocked, but this is a large feature that should wait until RAM headroom is confirmed.
