# Phase F: Tuesday AlgoGenerator (Revised)

## Goal

Create a generator that runs Tuesday's 15 algorithmic composers against a `NoteSequence`, producing gate/note/slide/gateOffset/gateLength/retrigger per step, with A/B preview via the existing generator infrastructure.

---

## Architecture: Extract `TuesdayAlgoCore` (shared between Engine and Generator)

The core insight from the audit: neither "wrap the existing engine" (generator has no access) nor "duplicate algorithms in the generator" (maintenance debt) works. The solution is to extract the algorithm logic into a standalone core that both consumers share.

```
                    TuesdayAlgoCore
              (standalone, zero engine/model deps)
                  /                     \
    TuesdayTrackEngine               AlgoGenerator
    (owns AlgoCore,                  (owns AlgoCore,
     wraps in tick pipeline)          calls generate() N times,
                                      maps results to steps)
```

### Why this fixes audit findings

| Finding | Fix |
|---|---|
| 1. No engine from generator | Generator owns its own AlgoCore — no engine dependency at all |
| 2. TuesdayTickResult private | Extracted as public `AlgoResult` in standalone header |
| 3. Algorithms read _sequence | Accept explicit `AlgoParams` struct — no model coupling |
| 4. RAM: Container + snapshot bloat | AlgoCore replaces engine's state 1:1 (engine container unchanged). Only Generator Container grows (~230 B in .bss) |
| 5. Gate mapping wrong | Gate from `velocity > 0 && gateRatio > 0`, cooldown from power param |
| 6. Builder can't write multi-layer | Adds `NoteSequenceBuilder::editSequence()` for direct step writes |
| 7. Note as MIDI vs scale degree | Writes scale degree to `step.note()` — matches NoteSequence semantics |

---

## TuesdayAlgoCore (new standalone class)

A self-contained class with no dependencies on Engine, Track, or Model systems.

### Public types

```cpp
struct AlgoParams {
    uint32_t seed = 0;
    int algorithm = 0;       // 0-14
    int flow = 8;            // 0-16 (seeds main RNG)
    int ornament = 8;        // 0-16 (seeds aux RNG, subdivisions)
    int glide = 0;           // 0-100 (slide probability)
    int power = 8;           // 0-16 (velocity/density range)
    int stepTrill = 0;       // 0-100 (intra-step retrigger probability)
    int gateLength = 50;     // 0-100 (gate % scaling factor)
};

struct AlgoContext {
    uint32_t divisor;              // ticks per step
    int tpb;                       // ticks per beat
    int loopLength;                // active loop length (0 = infinite)
    int effectiveLoopLength;       // floor(loopLength, 32)
    int rotatedStep;               // step index with rotation offset
    int stepIndex;                 // step index (replaces _stepIndex from engine)
    int ornament;                  // raw ornament parameter value
    int subdivisions;              // 3/4/5/7 from ornament
    int stepsPerBeat;              // CONFIG_PPQN / divisor
    bool isBeatStart;              // true when step % stepsPerBeat == 0
};

AlgoContext is structurally equivalent to the current GenerationContext with stepIndex added. The full set of fields preserves existing Tuesday algorithm behavior. During extraction: every `_stepIndex` reference in algorithm methods is replaced with `ctx.stepIndex`.

For generator use: divisor=1, tpb=1, loopLength=stepCount, effectiveLoopLength=stepCount, rotatedStep=stepIndex=i, stepIndex=i, ornament=params.ornament, subdivisions from params, stepsPerBeat=4, isBeatStart=(i % 4 == 0).

```cpp
struct AlgoResult {
    int note = 0;           // Scale degree 0-11
    int octave = 0;         // Octave offset
    uint8_t velocity = 0;   // 0-255 (0 = rest)
    bool accent = false;
    bool slide = false;
    uint16_t gateRatio = 75; // 0-200%
    uint8_t gateOffset = 0;  // 0-100%
    uint8_t trillCount = 1;  // 1-4
    uint8_t beatSpread = 0;  // 0-100% — engine uses in microgate/trill pipeline
    uint8_t polyCount = 0;   // 0/3/5/7 — engine uses for tuplet scheduling
    int8_t noteOffsets[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // micro-sequencing
    bool isSpatial = true;   // spatial vs temporal distribution
};
```

AlgoResult is structurally equivalent to the current TuesdayTickResult. Full result is kept for live Tuesday; AlgoGenerator is a lossy consumer that maps only the fields applicable to NoteSequence.

### Class interface

```cpp
class TuesdayAlgoCore {
public:
    TuesdayAlgoCore();
    void init(const AlgoParams &params);
    AlgoResult generate(const AlgoParams &params, const AlgoContext &ctx);

private:
    // Per-algorithm state structs (same as current engine, lines 191-301)
    // ... all 15 state structs ...

    union AlgorithmState {
        TestState test;
        TritranceState tritrance;
        StomperState stomper;
        AphexState aphex;
        AutechreState autechre;
        StepwaveState stepwave;
        MarkovState markov;
        ChipArp1State chiparp1;
        ChipArp2State chiparp2;
        WobbleState wobble;
        ScalewalkerState scalewalker;
        WindowState window;
        MinimalState minimal;
        BlakeState blake;
        GanzState ganz;
    };

    Random _rng;           // 4 B
    Random _extraRng;      // 4 B
    AlgorithmState _algoState; // 132 B (max = MarkovState)

    // 15 generate*() method declarations
    AlgoResult generateTest(const AlgoContext &ctx);
    AlgoResult generateTritrance(const AlgoContext &ctx);
    // ... all 15 ...
};
```

---

## TuesdayTrackEngine refactor

The engine loses ~1500 lines and gains ~30 lines of delegation glue.

### What moves to AlgoCore

- `AlgorithmState` union (132 B)
- `Random _rng`, `Random _extraRng` (8 B)
- All 15 `generate*()` methods
- `initAlgorithm()` body (parameterized)
- `calculateContext()` — becomes `AlgoContext` factory

### What stays in engine

- All pipeline state: `_coolDown`, `_coolDownMax`, `_microCoolDown`, `_microCoolDownMax`
- Gate scheduling: `_gateLength`, `_gateTicks`, `_gatePercent`, `_gateOffset`
- Retrigger/trill: `_retriggerCount/Period/Length/Timer`, `_isTrillNote`, `_trillCvTarget`, `_retriggerArmed`, `_ratchetInterval`
- CV glide: `_slide`, `_cvTarget`, `_cvCurrent`, `_cvDelta`, `_slideCountDown`
- Micro-gate queue: `_microGateQueue`, `_tieActive`
- Step tracking: `_stepIndex`, `_displayStep`
- Mask filtering: `_primeMaskCounter/State`, `_cached*`, `_timeModeStartTick`, `_currentMaskArrayIndex`
- Output: `_activity`, `_gateOutput`, `_cvOutput`, `_lastGatedCv`
- `tuesdayTrack()`, `pattern()`, `sequence()` — model accessors

### Seed semantics

Live TuesdayTrackEngine always passes `seed=0` to AlgoParams — this preserves the existing flow/ornament-only seeding behavior, unchanged.

AlgoGenerator passes a non-zero seed to obtain different results from the same flow/ornament. The seed is mixed inside `TuesdayAlgoCore::init()`, not by mutating flow/ornament (which would risk exceeding their 0-16 range):

```cpp
// Engine (preserves existing behavior):
AlgoParams params;
params.algorithm = alg;
params.flow = flow;
params.ornament = ornament;
params.glide = glide;
params.power = seq.power();
params.stepTrill = seq.stepTrill();
params.gateLength = seq.gateLength();
params.seed = 0;

// Generator:
AlgoParams params;
params.algorithm = alg;
params.flow = flow;
params.ornament = ornament;
params.glide = derivedGlide();
params.power = _params.power;
params.stepTrill = 0;
params.gateLength = 50;
params.seed = _params.seed;
```

Inside `TuesdayAlgoCore::init()`:
```cpp
void TuesdayAlgoCore::init(const AlgoParams &params) {
    int flow = params.flow;
    int ornament = params.ornament;
    uint32_t seed = params.seed;

    // Mix seed into seed derivation without exceeding param ranges
    uint32_t flowSeed = ((flow - 1) << 4) ^ ((seed >> 0) & 0xFF);
    uint32_t ornamentSeed = ((ornament - 1) << 4) ^ ((seed >> 8) & 0xFF);

    _rng = Random(flowSeed);
    _extraRng = Random(ornamentSeed + 0x9e3779b9);

    // Reset algorithm-specific state
    ...
}
```

### Refactored engine methods

```cpp
void TuesdayTrackEngine::initAlgorithm() {
    auto &seq = tuesdayTrack().sequence(pattern());
    AlgoParams params;
    params.algorithm = seq.algorithm();
    params.flow = seq.flow();
    params.ornament = seq.ornament();
    params.glide = seq.glide();
    params.power = seq.power();
    params.stepTrill = seq.stepTrill();
    params.gateLength = seq.gateLength();
    params.seed = 0;
    _algoCore.init(params);
}

TuesdayTickResult TuesdayTrackEngine::generateStep(uint32_t tick) {
    AlgoContext ctx = buildAlgoContext(tick);  // compute from engine state
    auto &seq = tuesdayTrack().sequence(pattern());
    AlgoParams params;
    params.algorithm = seq.algorithm();
    params.flow = seq.flow();
    params.ornament = seq.ornament();
    params.glide = seq.glide();
    params.power = seq.power();
    params.stepTrill = seq.stepTrill();
    params.gateLength = seq.gateLength();
    params.seed = 0;
    return wrap(_algoCore.generate(params, ctx));
}
```

TuesdayTickResult stays as the engine's own type (wraps AlgoResult with extra pipeline fields).

---

## AlgoGenerator class

### Params

| Param | Range | Default | UI label | Description |
|---|---|---|---|---|
| Seed | uint32_t | 0 | Footer F0/A/B | 32-bit seed |
| Algorithm | 0-14 | 0 | Footer F1 | Tuesday algorithm index |
| Flow | 0-16 | 8 | Footer F2 | Seeds main RNG |
| Ornament | 0-16 | 8 | Footer F3 | Seeds aux RNG + subdivisions |
| Power | 0-16 | 8 | Footer F4 | Gate density (probabilistic density via cooldown) |

### update() flow

```
update():
    AlgoParams params;
    params.algorithm = _params.algorithm;
    params.flow = _params.flow;
    params.ornament = _params.ornament;
    params.glide = derivedGlide();  // from variation or fixed
    params.power = _params.power;
    params.stepTrill = 0;          // no intra-step subdiv in generator
    params.gateLength = 50;        // default 50%
    params.seed = _params.seed;

    _algoCore.init(params);

    Random blendRng(_params.seed ^ 0xFEEDC0DE);
    int stepCount = _builder.length();
    auto &seq = static_cast<NoteSequenceBuilder &>(_builder).editSequence();

    int coolDown = 0;
    int coolDownMax = 16 - _params.power;  // higher power = shorter cooldown

    for i = 0 .. stepCount - 1:
        AlgoContext ctx;
        ctx.divisor = 1;
        ctx.tpb = 1;
        ctx.loopLength = stepCount;
        ctx.effectiveLoopLength = stepCount;
        ctx.rotatedStep = i;
        ctx.stepIndex = i;
        ctx.ornament = _params.ornament;
        ctx.subdivisions = ornamentToSubdivisions(_params.ornament);
        ctx.stepsPerBeat = 4;
        ctx.isBeatStart = (i % ctx.stepsPerBeat == 0);

        AlgoResult result = _algoCore.generate(params, ctx);
        applyResult(seq, i, result, blendRng, coolDown, coolDownMax);
```

### applyResult() — step mapping

```
applyResult(seq, stepIndex, result, rng, coolDown, coolDownMax):
    step = seq.step(seq.firstStep() + stepIndex)

    // Per-layer variation roll
    bool useGate  = rng.nextRange(100) < _params.variation;
    bool useNote  = rng.nextRange(100) < _params.variation;
    bool useSlide = rng.nextRange(100) < _params.variation;
    // ... etc

    // Gate decision: velocity > 0 && gateRatio > 0 && cooldown allows
    bool shouldGate = result.velocity > 0 && result.gateRatio > 0;
    if (shouldGate && coolDownMax > 0) {
        shouldGate = (--coolDown <= 0);  // cooldown must elapse to fire
        if (shouldGate) {
            coolDown = std::max(1, int(rng.nextRange(coolDownMax + 1)));  // backoff
        }
    } else if (shouldGate && coolDownMax == 0) {
        // power=16: all gates pass, no cooldown
    } else {
        shouldGate = false;
    }

    if (useGate) {
        step.setGate(shouldGate);
    }
    if (useNote && shouldGate) {
        step.setNote(result.note % 12);  // scale degree 0-11
    }
    if (useSlide && shouldGate) {
        step.setSlide(result.slide);
    }
    // GateOffset: 0-100% → NoteSequence::GateOffset::Max
    if (rng.nextRange(100) < _params.variation) {
        step.setLayerValue(NoteSequence::Layer::GateOffset,
                          result.gateOffset * NoteSequence::layerRange(NoteSequence::Layer::GateOffset).max / 100);
    }
    // Length from gateRatio: 0-200% → NoteSequence::Length::Max
    if (rng.nextRange(100) < _params.variation) {
        step.setLayerValue(NoteSequence::Layer::Length,
                          result.gateRatio * NoteSequence::layerRange(NoteSequence::Layer::Length).max / 200);
    }
    // Retrigger from trillCount: 1-4 → NoteSequence::Retrigger::Max
    if (rng.nextRange(100) < _params.variation) {
        step.setLayerValue(NoteSequence::Layer::Retrigger,
                          (result.trillCount - 1) * NoteSequence::layerRange(NoteSequence::Layer::Retrigger).max / 3);
    }
    // Note: beatSpread, polyCount, noteOffsets, isSpatial not mappable
    // to NoteSequence — silently ignored by generator (lossy consumer)
```

### Builder multi-layer access (addressing audit #6)

`NoteSequenceBuilder` is currently a type alias (`using NoteSequenceBuilder = SequenceBuilderImpl<NoteSequence>`) and `_edit` is private. Fix: make `_edit` protected in `SequenceBuilderImpl<T>`, then add a public typed accessor:

```cpp
template<typename T>
class SequenceBuilderImpl : public SequenceBuilder {
public:
    T &editSequence() { return _edit; }
    const T &originalSequence() const { return _original; }

protected:   // was private
    T &_edit;
    ...
};
```

AlgoGenerator calls `editSequence()` to write directly to NoteSequence steps. The builder's 3-copy state machine handles preview/original: `showOriginal()` restores from `_original`, `showPreview()` restores from `_preview`.

Alternatively, add a bulk `setStepFromAlgoResult(int stepIndex, const AlgoResult &result, int variation, Random &rng)` method on `SequenceBuilderImpl<T>` that handles all mapped layers in one call. This is cleaner but requires the template to know about AlgoResult. The direct accessor approach is simpler.

### displayValue()

Reads from cached `_pattern[]` populated during `update()`. This avoids mutating the algorithm core's RNG/state during draw:

```
update():
    ...
    for i = 0 .. stepCount-1:
        result = _algoCore.generate(...)
        applyResult(seq, i, result, rng)
        _pattern[i] = noteToDisplayValue(result)  // cache for visualization

displayValue(index):
    if showingPreview():
        return _pattern[index]
    else:
        return _builder.originalValue(index) * 255  // match GeneratorPattern 0-255
```

---

## RAM Assessment

### Engine side (CCMRAM)

| Before | After | Delta |
|---|---|---|
| `AlgorithmState` (132 B) inline | `TuesdayAlgoCore _algoCore` (~140 B) inline | **+8 B** (RNGs 8 B were separate, now inside core) |
| `Random _rng`, `_extraRng`, `_uiRng` (12 B) | `Random _uiRng` (4 B) stays | **−8 B** |
| Total change | | **0 B** |

Engine container size unchanged. AlgoCore replaces existing state 1:1.

### Generator side (.bss — static Container in Generator.cpp)

Current: `Container<EuclideanGenerator, RandomGenerator>` — max type ~90 B.
After: `Container<EuclideanGenerator, RandomGenerator, AlgoGenerator>` — AlgoGenerator ~230 B.

**Delta**: +140 B in `.bss`. Acceptable: current `.data + .bss = 119,960` of 128 KB budget.

### RAM measurement (required)

"~230 B" is an estimate. After Steps 1 and 3, run ARM `sizeof` probes:

- `sizeof(TuesdayAlgoCore)` — must match estimate (~140 B)
- `sizeof(TuesdayTrackEngine)` — must not exceed pre-refactor value
- `sizeof(AlgoGenerator)` — actual size for Container
- `sizeof(decltype(generatorContainer))` — verify Container growth is delta from max to min

### Heap

No heap allocation. AlgoGenerator lives in the static Container. No new `new` calls.

---

## Files

### New files

| File | Est. lines | Contents |
|---|---|---|
| `src/apps/sequencer/engine/generators/TuesdayAlgoCore.h` | ~250 | Public types (AlgoParams, AlgoContext, AlgoResult), AlgorithmState union, class TuesdayAlgoCore with init()/generate() declarations |
| `src/apps/sequencer/engine/generators/TuesdayAlgoCore.cpp` | ~1500 | 15 generate*() methods (moved from engine, adapted to read AlgoParams struct instead of _sequence), init() implementation |
| `src/apps/sequencer/engine/generators/AlgoGenerator.h` | ~70 | Generator subclass, Params struct, Param enum |
| `src/apps/sequencer/engine/generators/AlgoGenerator.cpp` | ~120 | update(), applyResult(), displayValue(), param accessors |

### Modified files

| File | Delta |
|---|---|
| `TuesdayTrackEngine.h` | −200 lines — remove AlgorithmState, 15 generate* declarations, GenerationContext, TuesdayTickResult. Add `TuesdayAlgoCore _algoCore` member. |
| `TuesdayTrackEngine.cpp` | −1500 +30 lines — remove 15 generate*() methods. Delegate to `_algoCore` in initAlgorithm()/generateStep(). |
| `Generator.h` | +3 lines — add `Algo` to Mode enum + modeName |
| `Generator.cpp` | +8 lines — add AlgoGenerator to Container, instantiate in execute() with NoteSequenceBuilder-only guard |
| `GeneratorPage.cpp` | +40 lines — footer labels, encoder routing, context menu, draw |
| `SequenceBuilder.h` | +2 lines — add `stepCount()` virtual or `editSequence()` accessor |
| CMakeLists (sequencer_shared) | +3 lines — add new .cpp files |

---

## Implementation Steps

### Step 1: Create TuesdayAlgoCore

1. Create `TuesdayAlgoCore.h` with public types + class interface
2. Create `TuesdayAlgoCore.cpp` — copy all 15 generate*() methods from engine
3. Adapt each method: replace `sequence.xxx()` reads with `_params.xxx`
4. Implement `init()` — copy engine's `initAlgorithm()` logic, parameterize on AlgoParams
5. Add to CMake
6. **Verify**: compile TuesdayAlgoCore tests standalone
7. **Hardware gate F1**: STM32 release build succeeds; size probes record `sizeof(TuesdayAlgoCore)` and current `.data`, `.bss`, `.ccmram_bss`

### Step 2: Refactor TuesdayTrackEngine to use AlgoCore

1. Remove AlgorithmState, _rng, _extraRng, 15 generate declarations from header
2. Replace with `TuesdayAlgoCore _algoCore`
3. `initAlgorithm()` → `_algoCore.init(buildParams())`
4. `generateStep()` → `_algoCore.generate(buildParams(), buildCtx())`
5. Remove the 15 method implementations from .cpp
6. **Verify**: existing Tuesday tests pass, same output as before
7. **Hardware gate F2**: flash hardware and verify existing Tuesday tracks still boot, play, reseed, change algorithms, respond to Flow/Ornament/Power/Glide/StepTrill, and show no cross-track regression

### Step 3: Implement AlgoGenerator

1. Create header + cpp with Generator interface
2. `update()` drives N steps via `_algoCore.generate()`, maps to builder
3. `displayValue()` for visualization
4. Add `editSequence()` to NoteSequenceBuilder
5. **Hardware gate F3**: STM32 release build succeeds; size probes record `sizeof(AlgoGenerator)` and `sizeof(decltype(generatorContainer))`; no RAM gate regression is accepted without identifying the symbol/container cliff

### Step 4: Wire into GeneratorPage

1. Add `Algo` to Generator::Mode
2. Add to Container in Generator.cpp
3. `Generator::execute()` — guard `Mode::Algo` to NoteSequenceBuilder only. Use `dynamic_cast` or check builder type at runtime. If a Curve sequence page requests Algo, return nullptr. Do not fall through — InitSteps would silently clear the Curve sequence.
4. Add footer, encoder, context menu, draw in GeneratorPage.cpp
5. **Hardware gate F4**: build, flash, and hardware test all 15 algorithms through the Note generator UI

### Hardware Testing Phases

Hardware is the release gate for this phase. Simulator checks can be used for UI crashes, but they do not replace STM32 verification.

Run the primary build from the repo root with:

```sh
cd build/stm32/release && make sequencer
```

#### F1: Core Extraction Build Gate (PASSED)

- Build STM32 release after adding `TuesdayAlgoCore`.
- Capture `.data`, `.bss`, `.data + .bss`, `.ccmram_bss`, and flash text size.
  - `.data = 6,320`
  - `.bss = 112,368`
  - `.data + .bss = 118,688`
  - `.ccmram_bss = 54,804`
  - `.text = 795,412`
- Run ARM `sizeof` probes for `TuesdayAlgoCore`, current `TuesdayTrackEngine`, and current `Engine::TrackEngineContainer`.
- Acceptance: no `.data + .bss` growth beyond direct new symbols needed for the core files; `TuesdayAlgoCore` size matches the estimate closely enough to keep the engine container below its current gate.
- **Result**: PASSED. TuesdayAlgoCore adds ~14 KB flash (.text), zero RAM (.data/.bss/.ccmram_bss unchanged). No instances created yet (engine refactor in Step 2).

#### F2: TuesdayTrackEngine Behavior Gate

- Build STM32 release after refactoring `TuesdayTrackEngine` to delegate to `TuesdayAlgoCore`.
- Flash hardware before adding AlgoGenerator UI.
- On hardware, verify existing Tuesday tracks:
  - boot without black screen or lockup
  - produce gates/CV in at least Test, Markov, Aphex, Autechre, and Stepwave
  - respond to Algorithm, Flow, Ornament, Power, Glide, StepTrill, and GateLength
  - reseed still changes the pattern
  - existing Note/Curve tracks still play while Tuesday tracks are active
- Acceptance: no audible/visible regression in existing Tuesday playback. Compile-only success is not sufficient for Step 2.

#### F3: AlgoGenerator RAM Gate

- Build STM32 release after adding `AlgoGenerator` and generator container wiring, before broad UI polish.
- Capture `.data`, `.bss`, `.data + .bss`, `.ccmram_bss`, `sizeof(AlgoGenerator)`, and `sizeof(decltype(generatorContainer))`.
- Acceptance: generator container growth is within the measured budget. If `.data + .bss` crosses or materially worsens the 120 KB warning zone, stop and identify the exact symbol or container growth before proceeding.

#### F4: Generator UI Hardware Gate

- Flash hardware after GeneratorPage wiring.
- On a Note track:
  - open Algo generator and confirm it starts in the expected A/B state
  - cycle all 15 algorithms and verify visible note/gate changes
  - adjust Flow, Ornament, Power, and Seed and verify preview changes
  - verify F0 A/B toggle, RESEED/NEW ALGO, REVERT, and COMMIT
  - verify committed output plays as a normal NoteSequence
  - verify leaving without commit restores the original sequence
- On a Curve track:
  - confirm Algo is not offered, or if invoked defensively, `Generator::execute()` returns `nullptr` without clearing or mutating the sequence
- Acceptance: Note generator workflow works on hardware and Curve tracks are protected from accidental mutation.

#### F5: Final Regression Gate

- Rebuild STM32 release after final cleanup.
- Record final RAM sections and sizeof values in this task document or `RESEARCH.md`.
- Hardware smoke test:
  - existing Random and Euclidean generators still preview/apply/revert
  - existing Tuesday playback still works after using AlgoGenerator
  - no boot/display regression
- Acceptance: hardware workflow passes and RAM deltas are documented.

### Generator context note

The AlgoGenerator's `AlgoContext` is a projection of the live Tuesday timing model, not an exact replica. Key differences:
- `divisor=1` (not live divisor)
- `tpb=1` (not derived from PPQN)
- `stepsPerBeat=4`, `isBeatStart=(i % 4 == 0)` (simplified beat model)
- `loopLength=stepCount`, `effectiveLoopLength=stepCount`

This means algorithms that are highly sensitive to timing context (e.g., Window's auto-mutation at 64-96 steps) will behave differently in generator mode vs live mode. The algorithm state machine still runs identically — only the timing projections differ. This is acceptable for a one-shot generator.

---

## Edge Cases

| Case | Handling |
|---|---|
| Algorithm produces velocity=0 | No gate fires (algo declares rest) |
| Power=0 | All gates suppressed; other layers still written if variation allows |
| Variation=0 | All original step values kept (no-op generation) |
| Flow=Ornament | AlgoCore adds golden ratio offset for distinct RNG streams |
| Step count < algorithm pattern length | Algorithm state machine wraps internally — no issue |
| Layer doesn't exist on NoteSequence | `setLayerValue()` with out-of-range layer is safe no-op |
