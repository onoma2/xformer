# Enhanced Stochastic Track Port (Vinx → XFORMER)

## Goal

Port the Vinx fork's Stochastic track type to XFORMER, then extend the MVP with global musical constraints from `docs/superpowers/specs/2026-05-17-enhanced-stochastic-track-design.md`: signed Degree Ticket Table, scale-mask exclusion, degree/mask rotation, Linearity, Min/Max degree range, Marbles-style spread/bias/steps pitch shaping, and a compact captured-event lock buffer.

**Why this matters:** Stochastic is one of Vinx's flagship track types alongside Logic and Arp. The enhanced MVP gives XFORMER a mono generative sequencer with step probability, global pitch shaping, and lockable captured performances without taking on polyphony or drift scope yet.

**References:**
- `docs/superpowers/specs/2026-05-17-enhanced-stochastic-track-design.md` — controlling MVP spec.
- `.tasks/stochastic-track-port/UI-DESIGN.md` — XFORMER-native UI design plan.
- `.tasks/stochastic-track-port/PHASE7-DICTIONARY.md` — current V5 vocabulary, ownership, and behavior contract.
- `.tasks/stochastic-track-port/PHASE8-V3-PLAN.md` — controlling Phase 8 V3 rebuild plan: split Rhythm/Melody `Loop`/`Live` source modes, no `New`, `Patience` as loop refresh, `Mutate` as loop-only edit, single runtime Hold/Lock freeze, UI deferred to Phase 9.
- `.tasks/stochastic-track-port/PHASE10-V5-CONTROL-GRANULARITY.md` — controlling V5 control-granularity spec: Level 1 Core, Level 2 Direct, Level 3 Weights/Advanced, generator/looper segmentation, `Density` vs `Mask`, and duration tickets.
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
- Signed Degree Ticket Table (`CONFIG_USER_SCALE_SIZE` tickets; engine uses the active scale's `notesPerOctave()` prefix; `-1` excluded, `0` included with zero tickets).
- Degree/Mask rotation for scale-aware transposition and chord movement.
- Linearity / melodic smoothing.
- Min/Max degree range.
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
- `degreeTickets[CONFIG_USER_SCALE_SIZE]`: `int8_t`
- `linearity`: `uint8_t`
- `degreeRotation`, `maskRotation`: `int8_t`
- `lock`: `bool`
- `loopFirst`, `loopLast`: `uint8_t`
- `marblesMode`: `uint8_t` or compact enum
- `marblesSpread`, `marblesBias`, `marblesSteps`: `uint8_t`
- `minDegree`, `maxDegree`: `uint8_t`
- `accentProb`, `legatoProb`: `uint8_t` only if accepted after size probe

**PWT semantic correction:** Use `int8_t degreeTickets[CONFIG_USER_SCALE_SIZE]`, not `uint8_t[12]`. `-1` means excluded from the active scale-degree mask, `0` means included but no raffle tickets, and positive values are ticket counts. The first implementation reads only `scale.notesPerOctave()` entries, so user scales up to 32 degrees are first-class. ProbMeloD's 12-slot table is the chromatic/semitone case, not the Performer model limit.

**Model acceptance:** The Vinx baseline is under the current `NoteTrack` gate, but the enhanced settings require an STM32 release `sizeof` probe. Do not assume the final model is accepted until `StochasticTrack <= NoteTrack` is measured on ARM.

### Engine Layer

| Component | File | Lines | Description |
|-----------|------|-------|-------------|
| StochasticEngine | `engine/StochasticEngine.h` | 217 | Tick-based engine with probability evaluation, gate/CV scheduling queues, loop buffer caching |
| StochasticEngine.cpp | `engine/StochasticEngine.cpp` | 687 | Core: triggerStep, evalStepGate, evalStepNote, evalStepLength, evalStepRetrigger, evalRestProbability, getNextWeightedPitch |

**Engine behavior:**
1. On each tick, evaluates step probabilities to select a pitch slot from active gates
2. Applies enhanced global constraints: signed degree tickets, scale mask, degree/mask rotation, Linearity, Min/Max degree range, optional Marbles spread/bias/steps shaping
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
- Enhanced config controls: degree tickets, scale/root, Linearity, degree range, Marbles mode/spread/bias/steps, lock/window controls
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
- Degree Ticket Table
- Scale-mask exclusion with `-1` ticket values
- Degree/Mask rotation
- Linearity
- Min/Max degree range
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
- Marbles shaping runs after allowed pitch slots are established by step gates, note probabilities, degree tickets, active scale, and range.
- The evaluated CV produced by Marbles shaping is stored in the captured-event cache.

### 6. ProbMeloD Pitch Semantics — MVP Requirement

ProbMeloD contributes the cleanest pitch-table mechanics, but Performer generalizes them through its scale system:
- `int8_t weights[12]` maps to `int8_t degreeTickets[CONFIG_USER_SCALE_SIZE]`; the engine uses the active scale's `notesPerOctave()` entries.
- `0` is not played by direct weighting, but it rotates with the included scale degrees.
- Degree rotation shifts all active degree ticket slots.
- Mask rotation rotates only included scale degrees while excluded notes remain fixed, enabling chord movement inside a key mask.
- Range is a pre-selection filter: out-of-range notes never enter the raffle pool.
- Raffle-ticket UI language should be used for degree tickets: positive value equals number of tickets in the draw.

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

**Goal:** Implement enhanced MVP pitch selection: signed degree tickets, scale-mask exclusion, degree/mask rotation, Linearity, Min/Max degree range, and Marbles spread/bias/steps shaping.

**Files:**
- `src/apps/sequencer/engine/StochasticEngine.cpp`
- `src/apps/sequencer/model/StochasticTrack.h/.cpp`
- `src/apps/sequencer/model/StochasticSequence.h/.cpp` if sequence-level accessors are needed

**Rules:**
1. Degree tickets use `int8_t[CONFIG_USER_SCALE_SIZE]`: `-1` excluded, `0` included with zero tickets, positive values are raffle tickets.
2. Normal weighted mode uses step gates/probabilities plus degree tickets, active scale/root, degree/mask rotation, Linearity, and Min/Max degree range.
3. Range filters the raffle pool before selection; do not clamp selected output notes after the draw.
4. Marbles mode replaces the final degree choice after allowed slots are known.
5. Marbles mode uses cheap local shaping based on `spread`, `bias`, and `steps`.
6. Engine selects a scale degree, converts with `Scale::noteToVolts(degree)`, applies root according to existing scale semantics, then stores the evaluated CV so locked loops are independent of later source edits.

---

### Phase 4: Loop Controls

**Goal:** Implement captured-event lock buffer, loop windowing, and loop rotation.

**Files:**
- `src/apps/sequencer/engine/StochasticEngine.h/.cpp`
- `src/apps/sequencer/model/StochasticTrack.h/.cpp`
- UI files only for controls required to exercise lock/window/rotation

**Acceptance:** Locked events replay captured gate, CV, length, retrigger, slide, and gate offset. Later step edits must not affect the locked event until clear/regenerate.

---

### Phase 5: Dynamics + Routing Targets — COMPLETE, REPORTED

**Goal:** Add cheap accent/legato if queue behavior and RAM remain flat, then add required Stochastic routing targets.

**Files:**
- `src/apps/sequencer/engine/StochasticEngine.cpp`
- `src/apps/sequencer/model/Routing.h`
- `src/apps/sequencer/model/Routing.cpp`

**Reported status:** Complete. Accent and Legato probabilities are implemented with full lock-invariant capture. Required stochastic routing targets are reportedly in place.

**Note:** Routing enum reordering affects serialization. Must use `targetSerialize()` decoupling (already in place in XFORMER).

---

### Phase 6: Stochastic Performance Mechanics

**Goal:** Keep the live stochastic layer small and semantically tight. Do not mix Proteus density, Tuesday pressure, and Proteus patience into one control set.

**Source references:**
- `src/apps/sequencer/model/TuesdaySequence.h`
- `src/apps/sequencer/engine/TuesdayTrackEngine.h`
- `src/apps/sequencer/engine/TuesdayTrackEngine.cpp`
- User-provided Proteus reverse-engineered functional spec: deterministic density muting by rest priority.

**Additions:**
1. **Density:** Proteus-style deterministic rest-priority thinning over `loopFirst..loopLast`. This is not another random gate roll. Turning density down mutes the same ordered steps; turning it back up restores the same rhythm skeleton.
2. **Tilt:** Optional bias used when generating the rest-priority order. Positive values preserve later-loop positions longer; negative values preserve earlier-loop positions longer. It is not a second density probability.
3. **Jitter:** Optional small timing humanize. It is evaluated once per accepted event and stored in the lock buffer. Locked replay must not reroll timing.
4. **Burst:** Optional bounded micro-gate layer based on the existing retrigger model. It may borrow Tuesday's queue discipline, but it must not introduce a hidden Tuesday algorithm or cooldown engine.
5. **CV:** Verify and expose the existing `gate/always` update choice: hold pitch until a gate vs evaluate pitch every step.
6. **Rotate:** Bounded address transform over `loopFirst..loopLast`.

**Non-goals:**
- Do not port Tuesday algorithms (`Autechre`, `Ganz`, `Blake`, `Window`, `ChipArp`, etc.).
- Do not add a hidden `Algorithm` selector to Stochastic.
- Do not add large algorithm state unions to `StochasticTrackEngine`.
- Do not implement Tuesday `power` / cooldown pressure in this phase.
- Do not add `patience` here; patience belongs only to Phase 7 buffer evolution.
- Do not expose compound underscore names such as `timing_jitter` or `density_tilt` to the user.
- Internal code names should follow existing codebase style. For example, prefer local C++ style such as `timingJitter()` / `_timingJitter` if that is the surrounding pattern.
- User-facing labels should be short, preferably one word: `Density`, `Tilt`, `Jitter`, `Burst`, `CV`, `Rotate`.

**Acceptance:**
- All six additions remain compatible with captured-event lock semantics.
- No unbounded queue growth, STL containers, or tick-path allocation.
- Default settings preserve current stochastic behavior: density 100, tilt 0, jitter 0.
- Step gate probability remains the only per-step random gate probability. Density is deterministic masking after a step wants to fire.
- STM32 release build remains within current SRAM/CCMRAM gates.

---

### Phase 7: Reimagined Stochastic Core Retopology

**Goal:** Stop extending the current Vinx-shaped step-probability engine and rebuild the stochastic core as a Performer-native probability instrument: generator controls create parent events, pattern controls capture and window those events, burst creates child hits inside parent events, and evolution controls mutate the generated buffer. Keep the existing `TrackMode::Stochastic` shell, save/load integration, routing entry points, project scale/root conventions, and hardware-tested page access plumbing.

**Source references:**
- MeloDICER user-guide excerpts provided by the user: rhythm `NOTE VALUE` / `VARIATION` / `LEGATO` / `REST`, melody faders, separate dice/realtime behavior, pattern length, lock.
- Stochastic Instruments SIG user-guide excerpts provided by the user: note/octave/duration probability, captured loops, repeat, linearity, contour, portamento, ratchet, force barline.
- Proteus reverse-engineered functional spec and user excerpts: `New`, complexity, deterministic density/rest-order thinning, sleep, patience/boredom, mutate, octave transposition.
- Tuesday track behavior: burst should mean child events inside a parent event when musically useful, not just same-pitch retrigger count.
- Marbles references: spread/bias as a bandwidth window over selectable pitch mass.
- Performer-native requirements: user scales up to 32 degrees, normal root/scale/transpose/static octave semantics, and STM32 RAM gates. Tickets, Spread, Bias, and Jump are stochastic controls constrained by that pitch contract, not Performer-native controls.

#### Phase 7a: Immutable Dictionary of Truth

**Status:** Superseded by the V5 rewrite in `.tasks/stochastic-track-port/PHASE7-DICTIONARY.md`.

**Current dictionary summary:**
- One generator + looper model under multiple control-granularity levels.
- `Density` now means generator-level sound/rest amount.
- Deterministic loop thinning is `Mask`; `Tilt` belongs to `Mask`.
- `Shape`, `Spread`, `Bias`, and `Steps` are Level 1 macro distribution controls.
- `Burst`, `Burst Count`, `Burst Rate`, and `Burst Pitch` are visible from Level 1.
- Split `Rhythm` and `Melody` `Live`/`Loop` source modes appear starting at Level 2.
- `Degree Rotate` and `Ticket Rotate` are Level 3 advanced pitch-ticket controls.
- `Duration Tickets` are Level 3 parent-duration weights using exactly:
  `1/2`, `1/4`, `1/8`, `1/16`, `3/16`, `5/16`, `7/16`, `1/8T`.

Do not use the older inline Phase 7 terms from git history as implementation truth.

#### Phase 7b: Core Model + Engine Rebuild

**Goal:** Implement the dictionary as a new internal core while preserving the existing Stochastic track shell.

**Implementation plan:**

Phase 7b is a ground-up core rebuild inside the existing `TrackMode::Stochastic` shell. Do not start from UI. Start from storage and deterministic engine behavior, then keep the current list/stub pages only as temporary hardware access.

##### 7b.0 Baseline and Guard Rails

**Files to read before editing:**
- `PROJECT.md` — version bump policy and RAM gates.
- `.tasks/stochastic-track-port/PHASE7-DICTIONARY.md` — required vocabulary and ownership contract.
- `src/apps/sequencer/model/StochasticTrack.h`
- `src/apps/sequencer/model/StochasticSequence.h`
- `src/apps/sequencer/engine/StochasticTrackEngine.h`
- `src/apps/sequencer/engine/StochasticTrackEngine.cpp`
- `src/apps/sequencer/model/Routing.h`
- `src/apps/sequencer/model/Routing.cpp`
- `src/apps/sequencer/ui/model/StochasticTrackListModel.h`
- `src/apps/sequencer/ui/model/StochasticSequenceListModel.h`

**Before code changes:**
1. Capture current STM32 release size numbers with `cd build/stm32/release && make sequencer`.
2. Capture `sizeof(StochasticTrack)`, `sizeof(StochasticSequence)`, `sizeof(StochasticTrackEngine)`, and `sizeof(Engine::TrackEngineContainer)` via the existing MonitorPage/size-probe workflow.
3. Confirm the current `tools/ui-preview` deletions are unrelated and do not include them in Phase 7b commits.

**Hard constraints:**
- No STL containers in the tick path.
- No tick-path heap allocation.
- No large inline engine buffers that exceed the engine container gate.
- No `Run` parameter.
- No raw user-facing `0..15` probability values.
- No final UI work in Phase 7b.

##### 7b.1 Add the Core Event Types

**Files:**
- Create: `src/apps/sequencer/model/StochasticTypes.h`
- Modify: `src/apps/sequencer/model/StochasticSequence.h`
- Modify: `src/apps/sequencer/model/StochasticTrack.h`

**New file responsibility:** `StochasticTypes.h` owns the compact vocabulary-level storage types shared by model and engine. Keep this separate so `StochasticSequence.h` does not keep growing around old Vinx bitfields.

**Types to add:**
- `enum class StochasticMode : uint8_t { Dice, Realtime, Last };`
- `enum class StochasticBurstPitch : uint8_t { Parent, Generate, Last };`
- `struct StochasticChildHit`
  - `int16_t degree;`
  - `int8_t octave;`
  - `uint8_t offset;` normalized 0..255 inside parent duration.
  - `uint8_t length;` normalized 0..255 inside parent duration.
  - `bool gate;`
  - `bool slide;`
  - `bool accent;`
- `struct StochasticParentEvent`
  - `int16_t degree;`
  - `int8_t octave;`
  - `uint8_t rate;` encoded duration index, not raw ticks.
  - `uint8_t length;` normalized gate length.
  - `uint8_t childFirst;`
  - `uint8_t childCount;`
  - `uint8_t densityRank;`
  - bit flags for `rest`, `legato`, `slide`, `accent`, `valid`.

**Storage rule:** Parent events are pattern-buffer data. Child hits are burst-buffer data. Neither should live only in the engine if the user expects Dice/Lock/project recall to reconstruct the pattern.

**How to integrate:**
- Add `#include "StochasticTypes.h"` to `StochasticSequence.h` and `StochasticTrack.h`.
- Leave the old `StochasticSequence::Step` type in place for migration and current stub UI access, but mark in comments that it is Phase 6 scaffolding.
- Add a compact event buffer to `StochasticSequence`:
  - `std::array<StochasticParentEvent, CONFIG_STEP_COUNT> _events;`
  - `std::array<StochasticChildHit, CONFIG_STEP_COUNT * 4> _children;`
  - `uint8_t _size;`
  - `uint8_t _first;`
  - `uint8_t _last;`
  - `bool _patternValid;`
- Use a fixed child capacity of `CONFIG_STEP_COUNT * 4` for Phase 7b. Four child hits per parent is enough to verify Tuesday-style burst without introducing unbounded queue pressure.

**Expected RAM impact:** This will add direct model storage. Probe before continuing. If `sizeof(StochasticTrack)` crosses `sizeof(NoteTrack)`, reduce child capacity or pack flags before touching engine behavior.

##### 7b.2 Move Stochastic Pattern Controls Into the Model

**Files:**
- Modify: `src/apps/sequencer/model/StochasticSequence.h`
- Modify: `src/apps/sequencer/model/StochasticTrack.h`
- Modify: `src/apps/sequencer/model/StochasticTrack.cpp`
- Modify: `src/apps/sequencer/model/ClipBoard.h`
- Modify: `src/apps/sequencer/model/ClipBoard.cpp`

**StochasticSequence additions:**
- `size()` / `setSize(int)` / `printSize()` / `editSize()`
- `first()` / `setFirst(int)` / `printFirst()` / `editFirst()`
- `last()` / `setLast(int)` / `printLast()` / `editLast()`
- `patternValid()` / `setPatternValid(bool)`
- `event(int)` accessors
- `child(int)` accessors
- `clearEvents()` clearing `_events`, `_children`, `_size`, `_first`, `_last`, `_patternValid`

**Bounds:**
- `Size`: clamp `2..CONFIG_STEP_COUNT`.
- `First`: clamp `0..size - 1`.
- `Last`: clamp `first..size - 1`.
- Default: `size = 16`, `first = 0`, `last = 15`.

**StochasticTrack additions:**
- Pitch/control fields:
  - `StochasticMode _mode;`
  - `uint8_t _complexity;`
  - `int8_t _contour;` range `-100..100`
  - `uint8_t _rate;`
  - `int8_t _variation;` range `-100..100`
  - `uint8_t _rest;`
  - `uint8_t _slide;`
  - `uint8_t _burstRate;`
  - `uint8_t _burstCount;`
  - `StochasticBurstPitch _burstPitch;`
  - `uint8_t _sleep;`
  - `uint8_t _patience;`
  - `uint8_t _mutate;`
  - `uint8_t _jump;`
- Existing fields to keep but reinterpret:
  - `_degreeTickets` = `Tickets`.
  - `_marblesSpread` = `Spread`; consider renaming later only if serialization can stay safe.
  - `_marblesBias` = `Bias`.
  - `_linearity` = `Linearity`.
  - `_density` = deterministic pattern thinning.
  - `_tilt` = rest-order generation bias.
  - `_octave` = static Performer track octave.
  - `_transpose` = Performer transpose.
- Existing fields to demote/scaffold:
  - `_jitter`, `_rotate`, `_gateBias`, `_retriggerBias`, `_lengthBias`, `_noteBias`, old `loopFirst/loopLast` should not drive Phase 7b core output unless explicitly bridged.

**Serialization:**
- Append new fields at the end of `StochasticTrack::write()` and `read()` in exactly the same order.
- Add event/child arrays to `StochasticSequence::write()` and `read()` after existing old step data to avoid another `end_of_file` regression.
- No version bump unless the user explicitly approves; dev projects may break, but read/write symmetry must be exact.
- After read, clamp enum values with `ModelUtils::clampedEnum()` or explicit range checks.

**Clipboard:**
- Update `ClipBoard.h` stochastic union arm if `StochasticSequence` size changes.
- Update `ClipBoard.cpp` copy/paste only if existing full-sequence copy no longer includes new event buffers automatically.

##### 7b.3 Add Deterministic Generation Helpers

**Files:**
- Create: `src/apps/sequencer/engine/StochasticGenerator.h`
- Create: `src/apps/sequencer/engine/StochasticGenerator.cpp`
- Modify: `src/apps/sequencer/engine/StochasticTrackEngine.cpp`

**Responsibility:** `StochasticGenerator` creates and mutates `StochasticSequence` event buffers from `StochasticTrack` controls. It should be deterministic, small, and testable without the full track engine.

**Public API:**
- `static void generatePattern(StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, uint32_t seed);`
- `static void mutateOne(StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, Random &rng);`
- `static StochasticParentEvent generateParentEvent(...);`
- `static int generateDegree(...);`
- `static void generateDensityRanks(StochasticSequence &sequence, const StochasticTrack &track, uint32_t seed);`
- `static int generateJumpOctave(const StochasticTrack &track, int currentJump, Random &rng);`

**Pitch generation rules:**
1. Build a local allowed degree list of at most `CONFIG_USER_SCALE_SIZE` active degrees from `scale.notesPerOctave()`.
2. Exclude tickets `< 0`.
3. Exclude degrees outside `Range`.
4. Compute final weight as:
   - `ticket_weight`
   - multiplied by Spread/Bias window weight
   - multiplied by Linearity distance weight
   - multiplied by Contour direction weight
   - constrained by Complexity.
5. If all final weights are zero, fall back to the first non-excluded degree in range.
6. Complexity behavior:
   - `0..32`: choose a tiny anchor subset from the ticket pool and strongly prefer repeats.
   - `33..66`: prefer repeat, `last - 1`, `last`, and `last + 1`.
   - `67..100`: use full weighted pool with wider jumps.

**Spread/Bias rule:**
- Treat Bias as a center index across the allowed degree list.
- Treat Spread as bandwidth:
  - low Spread = narrow window around Bias,
  - mid Spread = soft Gaussian-ish window,
  - high Spread = flat enough that tickets dominate.
- Spread/Bias must never wake an excluded or zero-ticket degree.

**Density ranks:**
- Generate one stable `densityRank` per parent event when a new pattern is created.
- Rank is unique from `0..size-1`.
- Tilt biases rank generation before deterministic shuffle:
  - negative preserves earlier positions longer,
  - positive preserves later positions longer.
- Density playback uses rank threshold only; changing Density must not consume RNG.

##### 7b.4 Rebuild Engine Playback Around Parent Events

**Files:**
- Modify: `src/apps/sequencer/engine/StochasticTrackEngine.h`
- Modify: `src/apps/sequencer/engine/StochasticTrackEngine.cpp`
- Add include: `src/apps/sequencer/engine/StochasticGenerator.h`

**Engine state changes:**
- Replace `_currentStep` semantics with parent-event index semantics. Keep method name if required by UI, but document it as current parent event.
- Add:
  - `uint8_t _patternIndex;`
  - `uint8_t _sleepRemaining;`
  - `uint16_t _boredomCounter;`
  - `int8_t _jumpOctave;`
  - `bool _patternCycleEnded;`
- Historical Phase 7b note, superseded by Phase 8 Runtime Hold. Do not implement this semantic lock shape in new work:
  - `LockedParentEvent`: final CV, duration ticks, gate/rest, legato, slide, accent, first child, child count, valid.
  - `LockedChildHit`: final CV, offset ticks, length ticks, gate, slide, accent.
- Historical buffer note also superseded: Phase 8 uses one runtime `HeldStep[CONFIG_STEP_COUNT]` scheduled-output buffer, not separate semantic parent/child lock buffers.

**Tick advancement:**
1. Do not use `sequence.runMode()` for Phase 7b stochastic playback.
2. Parent events advance forward through `sequence.first()..sequence.last()`.
3. Parent event duration comes from the generated event's `rate` and `length`, not from old per-step length probability.
4. `Realtime` mode generates the next parent event on demand and does not reuse the stored pattern, except for monitoring/debug if needed.
5. `Dice` mode generates the pattern when invalid, then reuses it until `New`, `Mutate`, or `Patience` changes it.

**Pattern-cycle boundary:**
- When index passes `last`, wrap to `first` and evaluate:
  - `Sleep`: insert wait cycles/ticks before next parent event.
  - `Patience`: if not 100, increase boredom and maybe call `generatePattern()`.
  - `Mutate`: maybe call `mutateOne()`.
  - `Jump`: maybe update `_jumpOctave` within bounded pitch behavior.
- If `lock()` is true, skip all mutation/new/jump changes to evaluated playback.

**Output scheduling:**
- Parent event schedules its CV/gate first.
- Burst child hits are scheduled relative to the parent event start and duration.
- Child hits do not advance `_patternIndex`.
- If `Burst Pitch == Parent`, child CV equals parent CV.
- If `Burst Pitch == Generate`, child CV is generated and captured independently.
- A parent hidden by Density schedules a gate-off and no child hits.
- `cvUpdateMode == Always` may still update CV on rests; `Gate` holds pitch until gate.

**Old logic to bypass or remove from main path:**
- `evalStepGate()`
- `evalStepLength()`
- `evalStepRetrigger()`
- old `evalStepNote()` fallback-to-step-note behavior
- `SequenceUtils::rotateStep()`
- `sequence.runMode()`
- `loopFirst/loopLast` as active stochastic playback source

Keep these only if needed for migration/debug until Phase 8 removes old UI assumptions.

##### 7b.5 Routing and Temporary Hardware Access

**Files:**
- Modify: `src/apps/sequencer/model/Routing.h`
- Modify: `src/apps/sequencer/model/Routing.cpp`
- Modify: `src/apps/sequencer/model/StochasticTrack.cpp`
- Modify: `src/apps/sequencer/ui/model/StochasticTrackListModel.h`
- Modify: `src/apps/sequencer/ui/model/StochasticSequenceListModel.h`
- Modify only if required: `src/apps/sequencer/ui/pages/StochasticConfigPage.h/.cpp`
- Modify only if required: `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.h/.cpp`

**Routing targets:**
- Add only high-value performance routables in Phase 7b:
  - `StochasticComplexity`
  - `StochasticRest`
  - `StochasticBurst`
  - `StochasticDensity`
  - `StochasticPatience`
  - `StochasticMutate`
- Do not add routable targets for every dictionary term yet. UI comes in Phase 8.
- Use `targetSerialize()` stable IDs; do not reorder existing IDs.

**Temporary list access:**
- `StochasticTrackListModel.h` should expose enough to hardware-test Phase 7b:
  - Mode, New, Complexity
  - Scale, Root, Transpose, Octave
  - Range, Tickets access path or at least a selected-ticket editor
  - Spread, Bias
  - Rate, Variation, Rest, Legato, Slide
  - Burst, Burst Rate, Burst Count, Burst Pitch
  - Size, First, Last, Lock
  - Density, Tilt, Sleep, Patience, Mutate, Jump
- Labels must be user-facing dictionary names, not internal names.
- `StochasticSequenceListModel.h` may expose event-buffer inspection only; it should not imply old step probabilities are the main model.

##### 7b.6 Project Save/Load and Backward Safety

**Files:**
- Modify: `src/apps/sequencer/model/StochasticTrack.h`
- Modify: `src/apps/sequencer/model/StochasticSequence.h`
- Review: `src/apps/sequencer/model/Track.cpp`
- Review: `src/apps/sequencer/model/ClipBoard.cpp`

**Checklist:**
1. Write/read order is byte-for-byte symmetric.
2. No conditional reads based on runtime state.
3. All enum reads are clamped.
4. `clear()` initializes every new field and every event/child valid flag.
5. Saved project with Stochastic track loads without `end_of_file`.
6. Clipboard copy/paste preserves generated pattern buffers.

##### 7b.7 Verification Plan

**Build/size checks:**
- `cd build/stm32/release && make sequencer`
- Record:
  - `.data`
  - `.bss`
  - `.ccmram_bss`
  - `sizeof(StochasticTrack)`
  - `sizeof(StochasticSequence)`
  - `sizeof(StochasticTrackEngine)`
  - `sizeof(Engine::TrackEngineContainer)`

**Hardware checks before Phase 8 UI:**
1. Create Stochastic track and confirm no boot/load crash.
2. Generate `New` pattern in Dice mode; confirm repeatable playback.
3. Switch to Realtime; confirm event stream changes continuously.
4. Set active scale/root/transpose/static octave; confirm output follows Performer pitch behavior.
5. Use user scale with more than 12 degrees; confirm tickets/range do not assume chromatic 12.
6. Set Complexity low; confirm small pitch vocabulary/repetition even with many tickets.
7. Set Complexity mid; confirm adjacent runs appear.
8. Set Complexity high; confirm wider pool use.
9. Sweep Density down/up; confirm the same parent events disappear/reappear.
10. Change Tilt, regenerate, and confirm early/late density survival changes.
11. Enable Burst with `Burst Pitch = Parent`; confirm child hits keep parent pitch.
12. Enable Burst with `Burst Pitch = Generate`; confirm child hits can change pitch and lock captures them.
13. Enable Lock, edit generator controls, and confirm evaluated parent/child output does not change.
14. Unlock, trigger New/Mutate/Patience behavior, and confirm output updates.
15. Save/load the project and confirm no `end_of_file`, same generated Dice pattern, and same locked replay.

**Commit slicing:**
1. Commit dictionary-linked model storage only.
2. Commit generator helpers and deterministic generation.
3. Commit engine playback rewrite.
4. Commit routing/list hardware access.
5. Commit serialization/clipboard/save-load fixes.
6. Commit verification notes and task status update.

**Non-goals:**
- Do not preserve `Run` as a Phase 7 control.
- Do not keep `Burst` as merely same-pitch retrigger if the user-facing term remains Burst; same-pitch repeats should be called ratchets if retained.
- Do not expose raw 0..15 probability values as the final user-facing model.
- Do not adopt Proteus CV summing, knob-lock, DAC range, C3 root, flash persistence, or 16-slot scale assumptions.
- Do not clone MeloDICER, SIG, Proteus, Tuesday, or Marbles UI vocabulary wholesale.

**Acceptance:**
- Phase 7a dictionary exists and is referenced by this task before Phase 7b implementation starts.
- The new engine can produce deterministic generated patterns, realtime generation, lock replay, density thinning, and burst child hits.
- STM32 release RAM remains under track and engine container gates, or exact deltas are documented before continuation.
- Hardware can verify the core without final visual UI; any temporary list/stub controls are explicitly throwaway.

---

### Phase 8: V3 Generator + Looper Core Rebuild

**Goal:** Replace the current single global Dice/Realtime stochastic core with the V3 two-layer architecture documented in `.tasks/stochastic-track-port/PHASE8-V3-PLAN.md`.

**Historical plan:** `.tasks/stochastic-track-port/PHASE8-V3-PLAN.md`

**Superseded by V5 where it conflicts:** `.tasks/stochastic-track-port/PHASE10-V5-CONTROL-GRANULARITY.md`. In particular, old V3 wording that says `Density` is deterministic loop thinning must now be read as `Mask`; V5 `Density` is generator-level sound/rest amount.

**Core topology:**
- Rhythm and Melody each have independent source modes: `Loop` or `Live`.
- No user-facing `Dice`, `Realtime`, or `New`.
- `Patience` controls automatic refresh of Loop domains only: `0` refreshes constantly, `100` never refreshes.
- `Mutate` edits Loop source material only. Live domains are already fresh and must not be mutated.
- `Rest` creates rhythm-generator silence.
- `Density` deterministically thins rhythm playback by stable priority/rank.
- Runtime Hold/Lock replays final evaluated output above generator, loop, patience, mutation, density, and source edits.
- Runtime Hold/Lock stores compact scheduled output hits, not parent/child semantic source data. Required engine helper wording: `initHoldBuffer()`, `freeHoldBuffer()`, `clearHoldBuffer()`, `replayHeldStep(readIndex, tick)`, `beginHoldCapture(readIndex)`, `captureHeldHit(readIndex, onOffset, offOffset, cv, accent, slide, forceCv)`, and `finishHoldCapture(readIndex)`.
- Do not add Lock A/B banks. Performer’s 16 patterns plus pattern-owned Loop source material are the saved snapshot system.

**Primary files:**
- `src/apps/sequencer/model/StochasticTypes.h`
- `src/apps/sequencer/model/StochasticSequence.h`
- `src/apps/sequencer/model/StochasticTrack.h`
- `src/apps/sequencer/model/StochasticTrack.cpp`
- `src/apps/sequencer/engine/StochasticGenerator.h`
- `src/apps/sequencer/engine/StochasticGenerator.cpp`
- `src/apps/sequencer/engine/StochasticTrackEngine.h`
- `src/apps/sequencer/engine/StochasticTrackEngine.cpp`
- `src/apps/sequencer/model/Routing.h`
- `src/apps/sequencer/model/Routing.cpp`
- `src/apps/sequencer/ui/model/StochasticPerformanceListModel.h` — temporary hardware access only
- `src/apps/sequencer/ui/model/StochasticConfigListModel.h` — temporary hardware access only

**Acceptance:**
- `Rhythm Loop + Melody Loop`: full phrase repeats.
- `Rhythm Loop + Melody Live`: rhythm repeats while pitch changes.
- `Rhythm Live + Melody Loop`: rhythm changes while pitch repeats.
- `Rhythm Live + Melody Live`: fully fresh generation.
- `Patience` and `Mutate` affect only Loop domains.
- Hold/Lock captures and replays one evaluated parent/child output buffer during the runtime session, without project persistence in Phase 8.
- Hold replay schedules stored CV/gate hits directly and does not remember why a hit existed.
- STM32 RAM gates remain documented after implementation.

**Non-goals:**
- No final visual UI.
- No evaluated-output A/B banks.
- No persistent evaluated lock buffers.
- No separate exposed rhythm/melody sizes yet.
- No hidden mutation of Live material.

---

### Phase 9: UI Layer

**Goal:** Implement the XFORMER-native Stochastic UI from `.tasks/stochastic-track-port/UI-DESIGN.md` and the V5 control-granularity spec only after the generator/looper core is hardware-verified. The UI must expose the reimagined `Rhythm/Melody` `Loop/Live` source model, not the current Vinx-shaped step-probability internals.

**Files:**
- `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.h/.cpp`
- `src/apps/sequencer/ui/pages/StochasticSequencePage.h/.cpp`
- `src/apps/sequencer/ui/model/StochasticSequenceListModel.h`
- `src/apps/sequencer/ui/model/StochasticTrackListModel.h`
- `src/apps/sequencer/ui/pages/Pages.h` — add page instances
- `src/apps/sequencer/ui/painters/SequencePainter.h/.cpp` — add stochastic draw routines

**Enhanced UI requirements:**
- Preserve Performer header/footer conventions from Note, Curve, Indexed, DiscreteMap, and Tuesday pages.
- Prefer custom visual pages over list models for degree tickets, generated pattern, burst child hits, lock buffer, and probability overview.
- Degree-ticket visual editor sized by the active scale's `notesPerOctave()`.
- UI must be segmented by `Generator` and `Looper`, not one long mixed list.
- Level 1 Core must expose `Complexity`, `Density`, `Shape`, `Spread`, `Bias`, `Steps`, `Burst`, `Burst Count`, `Burst Rate`, `Burst Pitch`, `Mode`, `Size`, `First`, `Last`, `Rotate`, and `Patience`.
- Level 2 Direct must expose split `Rhythm` and `Melody` source modes, `Rate`, `Variation`, `Rest`, `Legato`, `Slide`, `Accent`, `Range`, `Low Degree`, `High Degree`, and `Pitch Tickets`.
- Level 3 Weights/Advanced must expose `Duration Tickets`, optional `Rest Tickets`, `Degree Rotate`, `Ticket Rotate`, `Linearity`, `Contour`, `Mask`, `Tilt`, `Mutate`, `Jump`, and `Lock`.
- Pitch pages must use the current V5 dictionary: Scale, Root, Transpose, Octave, Range, Low/High Degree, Pitch Tickets, Degree Rotate, and Ticket Rotate.
- Rhythm pages must expose Rate, Variation, Rest, Legato, Slide, Accent, and Duration Tickets.
- Burst pages must make parent events vs child hits visually clear and must not hide Burst until advanced pages.
- Evolution/loop pages must distinguish `Density` from `Mask`: Density is generator sound/rest amount; Mask is deterministic playback thinning.
- Existing Phase 6/8 UI stubs are acceptable only as hardware access scaffolding; final Phase 9 UI should not be a list-only workflow.
- `StochasticTrackListModel` may exist as a fallback/settings bridge, but normal performance editing should live on visual pages.

**RAM check:** UI pages are in `.bss` (Pages struct). Estimate: StochasticSequenceEditPage ≈ NoteSequenceEditPage + list model overhead ≈ similar size. Should be under existing page gate.

---

### Phase 10: V5 Control Granularity Retopology

**Goal:** Implement the V5 control model documented in `.tasks/stochastic-track-port/PHASE10-V5-CONTROL-GRANULARITY.md`.

**Controlling plan:** `.tasks/stochastic-track-port/PHASE10-V5-CONTROL-GRANULARITY.md`

**Required semantic changes:**
- Implement from model truth upward: persistent Level 3 tables/transforms first, then Level 2 direct controls, then Level 1 macro control laws.
- Preserve V4 ownership: new stochastic entities are sequence-owned by default; do not move pattern-defining controls back to `StochasticTrack`.
- Rename deterministic loop thinning from `Density` to `Mask` in user-facing UI.
- Keep `Tilt` as the priority bias for `Mask`.
- Reintroduce `Density` as generator-level sound/rest amount.
- Keep Marbles-style `Shape`, `Spread`, `Bias`, and `Steps` together as Level 1 macro generator controls.
- Keep `Range` available across all levels as generator register/octave spread; it is not a Shape sub-control.
- Expose full burst controls from Level 1: `Burst`, `Burst Count`, `Burst Rate`, and `Burst Pitch`.
- Add Level 3 `Duration Tickets` with the approved parent-duration set:
  `1/2`, `1/4`, `1/8`, `1/16`, `3/16`, `5/16`, `7/16`, `1/8T`.
- Do not add `1 bar`, `1/64`, `1/128`, or quintuplet parent durations.
- Starting at Level 2, expose separate `Rhythm` and `Melody` `Live`/`Loop` source modes.
- Keep Level 1 `Mode` as a UI macro over both source modes, not a third stored source mode.
- Keep `Degree Rotate` and `Ticket Rotate` as Level 3 advanced pitch-ticket controls.
- Add a manual `Refresh` action that repopulates Loop source domains immediately, like momentary `Patience=0`, without changing stored Patience.
- Implement the new Patience rule: `0` refreshes every loop, `100` never auto-refreshes, and `1..99` maps exponentially to loop counts.

**Files likely involved:**
- `src/apps/sequencer/model/StochasticSequence.h`
- `src/apps/sequencer/model/StochasticTypes.h`
- `src/apps/sequencer/engine/StochasticGenerator.h`
- `src/apps/sequencer/engine/StochasticGenerator.cpp`
- `src/apps/sequencer/engine/StochasticTrackEngine.cpp`
- `src/apps/sequencer/ui/model/StochasticPerformanceListModel.h`
- `src/apps/sequencer/ui/model/StochasticConfigListModel.h`
- `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.h/.cpp`
- future custom visual pages for V5 levels

**Acceptance:**
- Default Level 1 produces audible output.
- Density changes generator sound/rest amount.
- Mask down/up restores the same stored loop skeleton.
- Range affects register/octave spread consistently at all levels.
- Full Burst controls are accessible without entering Level 3.
- Duration Tickets select only approved parent durations and Burst remains responsible for faster child hits.
- Split Rhythm/Melody source modes work at Level 2.
- Existing `StochasticSequenceEditPage` ticket page is retained for pitch-ticket editing during Phase 10; final UI can wrap or rename it later, but it must not be removed as part of the model/engine rewrite.
- Refresh command repopulates Loop source material and is ignored by locked replay.
- STM32 RAM and container gates remain documented.

---

### Phase 11: Validation

**Goal:** Full STM32 size and hardware verification.

**Size checks:**
- `sizeof(StochasticTrack)` <= `sizeof(NoteTrack)`
- `sizeof(StochasticEngine)` <= current engine gate, or exact CCMRAM delta documented
- `.data`, `.bss`, `.ccmram_bss`
- UI page size impact

**Hardware checks:**
1. Create Stochastic track, verify step grid editing.
2. Set gate/note probabilities, verify weighted selection.
3. Verify degree tickets, scale/root selection, Linearity, and Min/Max degree range.
4. Verify `-1` excluded notes stay fixed under Mask rotation and never enter the draw.
5. Verify `0` included notes rotate with the mask but do not play without tickets.
6. Verify Degree rotation moves the active degree table and Mask rotation changes chord shape within the included mask.
7. Verify Marbles Spread/Bias/Steps are musically distinct and captured into locked loops.
8. Test stage repeats (Each/First/Last/Odd/Even/Triplets/Random).
9. Test rest probabilities (1/2/4/8 step).
10. Test captured lock buffer, loop windowing, and rotation.
11. Test fill modes (Gates, NextPattern, Condition).
12. Test routing targets.
13. Test project save/load with Stochastic tracks.
14. Test Phase 6 mechanics: deterministic density, tilt priority generation, jitter lock capture, burst queue bounds, CV update behavior, and bounded rotate ergonomics.
15. Test Phase 7 buffer evolution: complexity generation, patience reset, per-loop mutation, octave-shift bounds, and locked replay.
16. Regression test: Note/Curve/Tuesday/Teletype tracks still work.

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
| Phase 5: Dynamics + Routing Targets | complete, reported |
| Phase 6: Stochastic Performance Mechanics | ~3-4h |
| Phase 7a: Immutable Dictionary of Truth | complete, documented |
| Phase 7b: Reimagined Core Retopology | ~6-8h |
| Phase 8: V3 Generator + Looper Core Rebuild | ~6-8h |
| Phase 9: UI Layer | ~6h |
| Phase 10: V5 Control Granularity Retopology | ~5-8h |
| Phase 11: Validation | ~3h |
| **Remaining** | **~8-11h before UI polish; ~14-19h including final UI** |

## Next Action

**Phase 10 prep:** Implement `.tasks/stochastic-track-port/PHASE10-V5-CONTROL-GRANULARITY.md`: rename deterministic thinning to `Mask`, make `Density` generator sound/rest amount, expose full Burst at Level 1, keep Marbles `Shape/Spread/Bias/Steps` together at Level 1, add the approved 8-slot Duration Tickets, and keep split Rhythm/Melody source modes starting at Level 2.

## Depends On

- `resource-optimization` (for RAM baseline)
- `generator-preview-apply` (for 3-copy SequenceBuilder state machine — already done)

## Blocks

Nothing directly blocked, but this is a large feature that should wait until RAM headroom is confirmed.
