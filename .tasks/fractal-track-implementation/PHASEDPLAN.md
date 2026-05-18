# FractalTrack Phased Implementation Plan

## Purpose

This plan decomposes the FractalTrack feature into implementable phases, following the same structure as the Stochastic track port plan. Each phase has clear files, acceptance criteria, and RAM gates. Scope is tightly bounded to avoid the original design doc's sprawl of 9 extended rule types.

## Product Contract

- FractalTrack reads from a parent track and produces mutated output.
- Mutations are influenced by history: what worked before is more likely to happen again.
- Evolution Depth (0-100%) controls how strongly history influences future mutations.
- The track produces gate + CV output from the parent's data, mutated per its rules.
- Scale constraint is the core rule: mutations stay in the active scale.
- No extended rule types (Markov, LSystem, CA, Fibonacci, etc.) in the first delivery.
- No Bloom v2 features (ratchet, slew, trill, branches) in the first delivery.
- First delivery: NoteTrack parent only. Other parent sources phased later.

## Mental Model

User-facing explanation:

> FractalTrack links to any NoteTrack and generates mutated copies of its steps. It remembers which mutations sounded good and leans toward similar mutations in the future. Evolution Depth controls how much the past influences the present.

Internal ownership:

- Parent owns the source step data (notes, gates, velocity, length).
- MutationHistory owns the circular buffer of past mutation results.
- SelectionPressure owns the success-bias calculation.
- FractalTrackEngine owns combining parent data + history + rules into output.

## Key Files

Model:
- `src/apps/sequencer/model/FractalSequence.h` (new)
- `src/apps/sequencer/model/FractalTrack.h` (new)
- `src/apps/sequencer/model/Track.h` -- add Fractal to TrackMode enum, Container, union
- `src/apps/sequencer/model/Routing.h` -- add FractalFirst..FractalLast targets
- `src/apps/sequencer/model/ClipBoard.h` -- if FractalSequence needs special handling

Engine:
- `src/apps/sequencer/engine/FractalTrackEngine.h` (new)
- `src/apps/sequencer/engine/FractalTrackEngine.cpp` (new)
- `src/apps/sequencer/engine/Engine.cpp` -- add case to track factory switch

UI:
- `src/apps/sequencer/ui/pages/FractalTrackPage.h` (new)
- `src/apps/sequencer/ui/model/FractalTrackListModel.h` (new)
- `src/apps/sequencer/ui/pages/Pages.h` -- register FractalTrackPage
- `src/apps/sequencer/ui/pages/TopPage.cpp` -- route to FractalTrackPage
- `src/apps/sequencer/ui/pages/TrackPage.cpp` -- add list model
- `src/apps/sequencer/ui/Ui.h` -- include FractalTrackPage header

Docs:
- `doc/fractal-track-research.md`
- `.tasks/fractal-track-implementation/TASK.md`
- `.tasks/fractal-track-implementation/PHASEDPLAN.md`

---

## Phase 1: Model Layer + Serialization (RAM-safe)

**Goal:** Add FractalSequence and FractalTrack to the model layer. Verify Track container size on STM32 release.

### Model Types

#### FractalSequence (per-pattern params)

```cpp
class FractalSequence {
public:
    // Parent link
    int parentTrack() const;            // 0-7, which track to read from
    void setParentTrack(int track);

    // Mutation ranges (all Routable for CV modulation)
    Routable<int8_t> _pitchRange;       // +/-1-24 semitones
    Routable<int8_t> _velRange;         // +/-1-127 velocity
    Routable<uint8_t> _gateProb;        // 0-100% gate survival
    Routable<int8_t> _lenRange;         // +/-1-100% length variation

    // Evolution system (MVP: no memory toggle, no extended rule types)
    uint8_t _evolutionDepth;            // 0-100%
    uint8_t _ruleType;                  // 0=SCALE_CONSTRAINED (only MVP rule type)

    // Reused from TuesdaySequence pattern
    int8_t _scale = -1;
    int8_t _rootNote = -1;
    Routable<uint16_t> _divisor;
    Routable<uint8_t> _clockMultiplier;
    uint8_t _resetMeasure = 8;
    Routable<int8_t> _octave;
    Routable<int8_t> _transpose;
    Routable<uint8_t> _gateLength;
    Routable<uint8_t> _gateOffset;
    Routable<int8_t> _rotate;

    // Seed for reproducibility
    uint32_t _baseSeed;

    // Branch seeds (storage only, branch UI deferred)
    uint32_t _branchSeeds[8];

    void clear();
    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    void setTrackIndex(int index) { _trackIndex = index; }
    int8_t _trackIndex = -1;
    friend class FractalTrack;
};
```

**Estimated size:** ~48 B (Routable params + byte fields + 9 uint32_ts). Similar to TuesdaySequence.

#### FractalTrack (track-level container)

```cpp
class FractalTrack {
public:
    using FractalSequenceArray = std::array<FractalSequence,
        CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT>;

    Types::PlayMode playMode() const;
    const FractalSequenceArray &sequences() const;

    void clear();
    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    void setTrackIndex(int trackIndex);
    int8_t _trackIndex = -1;
    FractalSequenceArray _sequences;
    Types::PlayMode _playMode = Types::PlayMode::Aligned;
    friend class Track;
};
```

### Track Integration

Add to `Track.h`:
- `TrackMode::Fractal` in enum (after Tuesday)
- `Container<..., FractalTrack>` in `Track::_container`
- `FractalTrack *fractal` in `Track::_track` union
- Accessors, `initContainer()`, `setContainerTrackIndex()`, serialization dispatch

### Routing Targets

Add `FractalFirst..FractalLast` to `Routing::Target`:
- `ParentTrack`
- `PitchRange`, `VelocityRange`, `GateProbability`, `LengthRange`
- `EvolutionDepth`
- `ActiveBranch` (storage, UI deferred)

### Serialization

Append fields at the end of `FractalSequence::write()`/`read()` in exact order. No version bump unless explicitly requested.

### Files

- Create: `src/apps/sequencer/model/FractalSequence.h`
- Create: `src/apps/sequencer/model/FractalTrack.h`
- Modify: `src/apps/sequencer/model/Track.h` -- container/union/enum
- Modify: `src/apps/sequencer/model/Routing.h` -- routing targets
- Modify: `src/apps/sequencer/model/ClipBoard.h` -- if FractalSequence size changes

### RAM Check

Build STM32 release. Verify:
- `sizeof(FractalTrack)` <= `sizeof(NoteTrack)` (9,544 B)
- `sizeof(FractalSequence)` recorded for reference
- `.data`, `.bss`, `.ccmram_bss` flat (no container inflation)

### Hardware Check

- Project save/load with FractalTrack instantiated.
- Old projects load without regression.

### Acceptance

- FractalTrack model compiles, serializes, and lives within the Track container gate.
- Routing targets route without crash.
- Clipboard copies FractalSequence data.

---

## Phase 2: Engine Core -- Parent Link + Scale-Constrained Mutation

**Goal:** Implement FractalTrackEngine with parent link to NoteTrack, basic pitch/velocity/gate/length mutation, and scale constraint. No memory, no evolution depth yet.

### Engine State

```cpp
class FractalTrackEngine : public TrackEngine {
public:
    FractalTrackEngine(Engine &engine, const Model &model, Track &track,
                       const TrackEngine *linkedTrackEngine);

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Fractal; }
    virtual void reset() override;
    virtual void restart() override;
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual bool activity() const override { return _activity; }
    virtual bool gateOutput(int index) const override { return _gateOutput; }
    virtual float cvOutput(int index) const override { return _cvOutput; }

private:
    FractalTrack &_fractalTrack;

    // Parent data
    const TrackEngine *_parentEngine = nullptr;
    int _parentTrackIndex;

    // Mutation state
    Random _rng;

    // Output
    bool _activity = false;
    bool _gateOutput = false;
    float _cvOutput = 0.f;
};
```

### Tick Behavior

```
tick(tick):
  1. Resolve parent track engine from _fractalTrack.sequences()[_activeSequence].parentTrack()
  2. Read parent's current step data (linkData() from parent engine)
  3. If no parent or parent inactive -> output silence, return NoUpdate
  4. Apply mutation:
     a. Pitch: parent note + randomSemitoneOffset(+/-pitchRange), clamped 0-127
     b. Velocity: parent velocity + randomOffset(+/-velRange), clamped 1-127
     c. Gate: survives with gateProb probability
     d. Length: parent length * (1 + randomOffset(+/-lenRange/100.0))
  5. Apply scale constraint: if scale set on FractalSequence, snap mutated note to scale
  6. Output CV = scaleToVolts(mutatedNote, octave)
  7. Output gate = mutatedGate
```

### Scale Constraint

```cpp
void applyScaleConstraint(int8_t &note, const Scale &scale, int rootNote) {
    // Convert note to degree in the active scale
    // Snap to nearest in-scale degree
    // Convert back to note
}
```

This is the only rule in the MVP. No position-aware, no neighbor-aware, no extended rule types.

### Files

- Create: `src/apps/sequencer/engine/FractalTrackEngine.h`
- Create: `src/apps/sequencer/engine/FractalTrackEngine.cpp`
- Modify: `src/apps/sequencer/engine/Engine.cpp` -- add to track factory

### RAM Check (build STM32 release)

- `sizeof(FractalTrackEngine)` -- target under current engine container gate (912 B)
  - Expected: ~200 B (no history, no pressure, no large buffers)
- `sizeof(Engine::TrackEngineContainer)` -- must not increase
- `.ccmram_bss` must not increase measurably

### Hardware Check

1. Create FractalTrack, link to NoteTrack parent.
2. Verify FractalTrack outputs parent's notes/gates when pitchRange=0, gateProb=100%.
3. Increase pitchRange; verify notes are mutated but stay in scale.
4. Reduce gateProb; verify gates disappear proportionally.
5. Change parent's notes; verify FractalTrack tracks changes (new mutations on next tick).
6. Verify parent's scale changes are reflected in FractalTrack's output.
7. Test with different parent scales (major, minor, user scales).

### Acceptance

- FractalTrack produces audible mutated output from NoteTrack parent.
- Scale constraint works: all output notes are in the active scale.
- Mutation ranges (pitch/vel/gate/len) behave as expected.
- No crashes, no CCMRAM regression.

---

## Phase 3: Evolution System -- MutationHistory + SelectionPressure

**Goal:** Add the Hallucigenia-inspired memory system: MutationHistory buffer, SelectionPressure tracking, and EvolutionDepth parameter that makes mutations "smarter" over time.

### MutationHistory

```cpp
struct MutationRecord {
    int8_t pitchOffset;     // -24 to +24
    int8_t velOffset;       // -64 to +64
    bool gateSurvived;
    int8_t lenOffset;       // -100 to +100 (percentage)
    uint8_t success;        // 0-255: how "successful" this mutation was
};

class MutationHistory {
    static constexpr uint8_t MAX_RECORDS = 16;
    MutationRecord records[MAX_RECORDS];
    uint8_t head = 0;
    uint8_t count = 0;

public:
    void push(int8_t pitchOffset, int8_t velOffset, bool gateSurvived,
              int8_t lenOffset, uint8_t success);
    int8_t getAveragePitchOffset() const;
    float getSuccessRate() const;
    void decay(float factor);
    void clear();
};
```

**Estimated size:** 16 records x ~10 B = ~160 B + overhead = ~180 B.

### SelectionPressure

```cpp
class SelectionPressure {
    float pitchSuccessRate = 0.5f;
    float gateSuccessRate = 0.5f;
    float lengthSuccessRate = 0.5f;

public:
    void record(uint8_t mutationType, bool success);
    float calculateBias(uint8_t mutationType) const;
    void decay(float factor = 0.95f);
};
```

**Estimated size:** ~16 B (3 floats).

### Evolution Depth Integration

Modify `tick()`:

```
4a. If evolutionDepth > 0 AND history has records:
    pitchOffset = lerp(randomOffset, historyBiasedOffset, evolutionDepth / 100.0)
    gateProb = lerp(gateProb, historyBiasedGateProb, evolutionDepth / 100.0)
    velocityOffset = lerp(randomOffset, historyBiasedVelOffset, evolutionDepth / 100.0)
    lengthOffset = lerp(randomOffset, historyBiasedLenOffset, evolutionDepth / 100.0)
```

### When Does a Mutation "Succeed"?

Automatic success criteria for MVP:
- **Pitch**: note is in scale (already constrained) AND interval from parent is within a "musical" range (<= 7 semitones)
- **Gate**: gate fires (gate survived = success)
- **Velocity**: velocity is in "musical" range (40-120)
- **Length**: length is not extreme (10-90% of step)

In Phase 4, this becomes user-configurable via `SuccessCriteria` in the UI.

### Files

- Modify: `src/apps/sequencer/engine/FractalTrackEngine.h` -- add history + pressure members
- Modify: `src/apps/sequencer/engine/FractalTrackEngine.cpp` -- integrate into tick()

### RAM Check

- `sizeof(FractalTrackEngine)` with history + pressure inline
  - Estimated: ~200 B (Phase 2) + ~200 B (history) + ~16 B (pressure) = ~416 B
  - **Still under 912 B gate** ✅

### Hardware Check

1. Enable evolution with low depth (20%). Verify mutation output shifts subtly toward repeated patterns.
2. Set evolution depth to 100%. Verify strong bias toward previous mutation patterns.
3. Clear history (future UI button or reset). Verify bias resets to random.
4. Set evolution depth to 0%. Verify purely random mutation (no history bias).

### Acceptance

- History correctly biases mutations when evolution depth > 0.
- Decay function ages old records appropriately.
- Clear resets history and bias.
- No regression in Phase 2 behavior when evolution depth = 0.

---

## Phase 4: UI Layer (MVP)

**Goal:** Multi-section FractalTrackPage and FractalTrackListModel.

### FractalTrackPage Sections (MVP, using existing BasePage pattern)

**LINK page:**
- Parent Track select (1-8) -- displays parent track type icon
- Parent info display (current step, note, gate state)

**MUTATION page:**
- Pitch Range: F1
- Velocity Range: F2
- Gate Probability: F3
- Length Range: F4

**EVOLUTION page:**
- Evolution Depth: F1 (0-100%)
- Memory ON/OFF: F2 (enable/disable history)
- Max Leap: F3 (1-24 semitones)
- Rules: F4 (SCALE_CONSTRAINED only in MVP)

**SEED page:**
- Base Seed: F1
- Reset History: F4 (momentary action)

### FractalTrackListModel

For TrackPage setup:
- PlayMode
- Link (parent track)
- Divisor, ClockMult
- ResetMeasure
- Scale, Root, Octave, Transpose

### Files

- Create: `src/apps/sequencer/ui/pages/FractalTrackPage.h`
- Create: `src/apps/sequencer/ui/model/FractalTrackListModel.h`
- Modify: `src/apps/sequencer/ui/pages/Pages.h` -- add FractalTrackPage instance
- Modify: `src/apps/sequencer/ui/pages/TopPage.cpp` -- route to FractalTrackPage
- Modify: `src/apps/sequencer/ui/pages/TrackPage.cpp` -- add FractalTrackListModel
- Modify: `src/apps/sequencer/ui/Ui.h` -- include FractalTrackPage header

### Hardware Check

1. Navigate to FractalTrack from TrackPage.
2. LINK page: select parent track, verify parent info updates.
3. MUTATION page: edit all 4 ranges, verify mutation changes audibly.
4. EVOLUTION page: sweep depth, verify audible bias change.
5. SEED page: change seed, verify different mutation sequence.
6. Verify all pages render correctly with correct labels.
7. Verify context menus (Init, Copy, Paste, Duplicate) work.

### Acceptance

- All MVP pages navigable with correct F1-F4 labels.
- Parameter edits produce audible changes.
- No UI crashes on page entry/exit/scroll.

---

## Phase 5: Parent Source Interface (Multi-Track Parent Support)

**Goal:** Add `FractalSourceInterface` so FractalTrack can read from CurveTrack, TuesdayTrack, IndexedTrack, and DiscreteMapTrack in addition to NoteTrack.

### FractalSourceInterface

```cpp
// In TrackEngine.h or new FractalSourceInterface.h
struct FractalSourceData {
    int8_t noteIndex;        // -63 to +64 (normalized pitch)
    float cvVoltage;         // -5V to +5V
    bool gate;
    uint8_t velocity;
    int currentStep;
    int loopLength;
    float progress;          // 0.0 to 1.0
    int8_t scaleIndex;       // -1 = project scale
    int8_t rootNote;         // -1 = project root
};

class FractalSourceInterface {
public:
    virtual ~FractalSourceInterface() {}
    virtual FractalSourceData getFractalSourceData() const = 0;
};
```

### Per-Track Implementation

- **NoteTrackEngine**: Extend existing -- full support (scale, root, notes, velocity, gate)
- **CurveTrackEngine**: CV values, gate, timing (not scale-based; scaleIndex = -1)
- **TuesdayTrackEngine**: Algo-generated notes, velocity, accent
- **IndexedTrackEngine**: Indexed notes, duration, gate
- **DiscreteMapTrackEngine**: Stage index as "note", ramp phase, gate

### Files

- Create or extend: `engine/FractalSourceInterface.h` -- new file or add to TrackEngine.h
- Modify: `engine/NoteTrackEngine.h/.cpp` -- add `getFractalSourceData()`
- Modify: `engine/CurveTrackEngine.h/.cpp` -- add `getFractalSourceData()`
- Modify: `engine/TuesdayTrackEngine.h/.cpp` -- add `getFractalSourceData()`
- Modify: `engine/IndexedTrackEngine.h/.cpp` -- add `getFractalSourceData()`
- Modify: `engine/DiscreteMapTrackEngine.h/.cpp` -- add `getFractalSourceData()`
- Modify: `engine/FractalTrackEngine.cpp` -- use interface instead of direct NoteTrackEngine link

### Hardware Check

1. Link FractalTrack to CurveTrack parent; verify CV values produce audible mutation.
2. Link to TuesdayTrack parent; verify algo-generated notes are mutated.
3. Link to IndexedTrack parent; verify indexed notes are mutated.
4. Verify parent type auto-detection works (LINK page shows correct type icon).

### Deferred

- `TeletypeTrackEngine` -- explicitly excluded. Script-defined output is too variable for reliable mutation.

---

## Phase 6: Bloom v2 Per-Step Features (Post-MVP)

**Goal:** Port per-step modifiers from Bloom v2: ratchet, slew, trill, and the 8-slot branch system.

### Per-Step Ratchet

```cpp
// Each step can subdivide into 1-8 micro-steps
uint8_t _ratchetCount;       // 1-8 subdivisions
uint8_t _ratchetProb;        // 0-100%
```

Ratchet fires after gate-on evaluation. Each micro-step fires a gate + CV at the same pitch, subdivided within the step duration.

### Per-Step Slew

```cpp
// Portamento/slew between consecutive CV outputs
uint8_t _slewAmount;         // 0-100%
```

Implemented as slewed CV output at the DAC level (or interpolated CV in the engine). Reuses any existing slew/portamento infrastructure.

### Per-Step Trill

```cpp
// Ornamental rapid notes on a step
bool _trillEnabled;
uint8_t _trillProb;          // 0-100%
uint8_t _trillInterval;      // semitones between trill notes
```

Trill fires rapid alternation between the mutated note and a note at _trillInterval. Scheduled into the gate queue like ratchet but alternating pitch.

### Branch System

```cpp
uint32_t _branchSeeds[8];    // Already allocated in model
uint8_t _activeBranch;       // 0-7
bool _cvBranchSelect;        // CV input selects branch
```

Branches store alternate seed values. Switching branches produces different mutation sequences from the same parent. CV branch select allows external voltage to choose branch.

### UI Additions

**BRANCHES page:**
- Slot: F1 (0-7)
- Save: F2 (save current seed to slot)
- Load: F3 (load seed from slot)
- CV Sel: F4 (enable CV select)

**BLOOM page:**
- Ratchet: F1 (1-8)
- R.Prob: F2 (0-100%)
- Slew: F3 (0-100%)
- Trill: F4 (ON/OFF + prob/interval sub-page)

### Files

- Modify: `model/FractalSequence.h` -- add ratchet/slew/trill params
- Modify: `engine/FractalTrackEngine.h/.cpp` -- per-step modifier logic
- Modify: `ui/pages/FractalTrackPage.h` -- add BRANCHES and BLOOM sections

### Hardware Check

1. Enable ratchet on step; verify micro-gate subdivisions.
2. Enable slew; verify portamento between consecutive CVs.
3. Enable trill; verify rapid pitch alternation.
4. Save/load branch seeds; verify different mutation output per branch.
5. CV branch select; verify external CV selects branch.

---

## Phase 7: Extended Rule Types (Post-MVP, Deferred)

**Goal:** Add the 9 extended mutation rule types from the design doc. Each rule type is an independent algorithm that runs alongside or replaces the basic scale-constrained mutation.

This phase is intentionally deferred. Each rule type requires:
- An enum value in RuleType
- A RuleParameters struct/union in FractalSequence
- A rule application function in FractalTrackEngine
- UI sections for parameter editing

### Deferred Rule Priority

1. **Position-aware**: downbeat = stronger mutations (low effort, high musical value)
2. **Neighbor-aware**: similar context -> similar mutation (medium effort)
3. **Markov transitions**: probability matrix from parent's note sequence (high effort)
4. **Perlin noise**: smooth continuous variation (medium effort)
5. **Gravitation/attraction**: gravity wells toward target notes (medium effort)
6. **Echo/delay memory**: echo parent mutations with decay (medium effort)
7. **Fibonacci intervals**: constrain to Fibonacci semitone intervals (low effort)
8. **Symmetry/mirror**: palindrome transformations (medium effort)
9. **Tension/resolution**: bias toward stable notes (high effort)
10. **L-System growth**: rewriting rules (very high effort)
11. **Cellular automata**: neighbor-based gate/velocity rules (high effort)

### Do Not Implement in MVP

All 11 rule types above are post-MVP. The MVP ships with `SCALE_CONSTRAINED` only, plus optionally `POSITION_AWARE` if implementation is trivial.

---

## Phase 8: Validation

**Goal:** Full STM32 size and hardware verification.

### Size Checks

```bash
cd build/stm32/release && make sequencer
```

Record:
- `.data`
- `.bss`
- `.ccmram_bss`
- `sizeof(FractalTrack)`
- `sizeof(FractalSequence)`
- `sizeof(FractalTrackEngine)`
- `sizeof(Engine::TrackEngineContainer)`
- `sizeof(Track)`

Verify:
- `FractalTrack <= NoteTrack` (no model container inflation)
- `FractalTrackEngine <= 912 B` (no engine container inflation)
- `.data + .bss` flat or minimal increase
- `.ccmram_bss` flat or minimal increase

### Hardware Checks (MVP Regression)

1. Create FractalTrack, link to NoteTrack.
2. Verify output with no mutation (pitchRange=0, gateProb=100%).
3. Verify basic mutation (pitchRange=12, gateProb=75%).
4. Verify scale constraint keeps all notes in scale.
5. Verify evolution depth biases mutations.
6. Verify history clear resets bias.
7. Verify seed determinism (same seed = same mutation sequence).
8. Save/load project with FractalTrack; verify params persist.
9. Verify all other track types still work (Note, Curve, Tuesday, etc.).
10. Regression: verify no hard faults, no audio dropout.

### Hardware Checks (Phase 5 Multi-Parent)

11. Verify FractalTrack outputs with CurveTrack parent.
12. Verify with TuesdayTrack parent.
13. Verify with IndexedTrack parent.
14. Verify with DiscreteMapTrack parent.

---

## Effort Estimate

| Phase | Effort | RAM Risk |
|-------|--------|----------|
| Phase 1: Model + Serialization | ~2h | Low -- under container gate |
| Phase 2: Engine Core (no memory) | ~3h | Low -- ~200 B engine |
| Phase 3: Evolution System | ~2h | Low -- ~200 B additional engine |
| Phase 4: UI Layer (MVP) | ~3h | Low -- UI pages in .bss |
| Phase 5: Multi-Parent Interface | ~2h | Low -- interface + adapters |
| Phase 6: Bloom v2 Features | ~4h | Low -- params + per-step logic |
| Phase 7: Extended Rules | ~8-12h | Medium -- rule state in engine |
| Phase 8: Validation | ~2h | None |
| **MVP Total (Phases 1-4)** | **~10h** | **Low** |
| **Full Total** | **~26-30h** | **Low-Medium** |

## Open Questions

- [ ] Should SuccessCriteria be user-configurable in Phase 4, or auto-evaluated as described in Phase 3?
- [ ] Does FractalTrack need its own grid edit page, or is the list/section page sufficient for MVP?
- [ ] How does FractalTrack interact with the global clock/divisor system? Same as NoteTrack?
- [ ] Should the parent link be per-pattern (in FractalSequence) or per-track (in FractalTrack)?
- [ ] Does FractalTrack need a "thru" mode where it passes parent data unchanged (mutation ranges = 0)?
- [ ] What is the default parent track when a FractalTrack is first created? Track 0?

## Commit Slices

1. Model types + serialization + Track.h integration + Routing targets.
2. Engine with parent link + basic scale-constrained mutation.
3. Evolution system: MutationHistory + SelectionPressure + EvolutionDepth.
4. UI pages + list model + page registration.
5. FractalSourceInterface + multi-track parent support.
6. Bloom v2 features (ratchet, slew, trill, branches).
7. Extended rule types (deferred from MVP).
8. Validation + task status update.

## Next Action

Phase 1: Create FractalSequence.h and FractalTrack.h model files, add Fractal to Track.h Container/enum/union, add Routing targets.
