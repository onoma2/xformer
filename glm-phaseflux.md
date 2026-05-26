# PhaseFlux Track — Full Implementation Outline

Monophonic 4×4 grid sequencer. Dual independent X/Y cursors address 16 stages.
Each stage holds sub-triggers shaped by warp/curve/response math borrowed from
CurveTrack's `Curve::eval()` and DiscreteMap's phase-ramp pipeline.

---

## 1. Spec Decisions (resolved open questions)

| Q | Decision | Rationale |
|---|----------|-----------|
| Q1 Grid intersection | **Last-arriving clock** — whichever axis ticks last triggers the new stage | Most musically useful, predictable, sequencer-like |
| Q2 Cursor-leave-mid-stage | **Cut** — stop immediately on cursor move | Mono-safe, no overlap ambiguity |
| Q3 Curve interpolation | N/A — curves evaluated at discrete sub-trigger phase points | No interpolation needed |
| Q4 Stage duration | **Per-sequence divisor** — same as DiscreteMap/Stochastic | Reuses existing clock infra |
| Q5 Accumulator reset | **Reset zeros all 16 counters** | Cleanest |
| Q6 Counter direction | **Monotonic up** — simplest, defer direction bit | Keeps stage struct small |
| Q7 Wrap | **mod N wrap** — simplest | Defer ping-pong to v2 |

---

## 2. RAM Budget

All estimates are ARM `sizeof` targets. Final numbers from STM32 release build.

### Stage struct layout (16 bytes)

```
int8_t   _basePitch          // -50 to +50 (0.01V units → -0.50V to +0.50V)
uint8_t  _subTriggerCount    // 1-8
uint8_t  _temporalCurve      // 0-5
int8_t   _temporalWarp       // -100 to +100
int8_t   _temporalResponse   // -100 to +100
uint8_t  _pitchCurve         // 0-4
int8_t   _pitchWarp          // -100 to +100
int8_t   _pitchResponse      // -100 to +100
uint8_t  _gateLength         // 0-100 (%)
uint8_t  _transform          // 0 = None, 1-4 = transform type
uint8_t  _accLength          // 1-16
int8_t   _accStep            // -100 to +100
uint8_t  _accAdvance         // 0=A, 1=B, 2=C
uint8_t  _skip               // 0 or 1
uint8_t  _pitchRange         // 0-100 (0.01V units → 0-1.00V)
// padding: 1 byte to align
= 16 bytes per stage × 16 stages = 256 bytes
```

### Sequence layout

```
16 Stages                   = 256 B
uint8_t divisor             = 1 B
uint8_t xDivisor            = 1 B   (X-axis clock divisor)
uint8_t yDivisor            = 1 B   (Y-axis clock divisor)
uint8_t xDirection          = 1 B   (Fwd/PP/Random)
uint8_t yDirection          = 1 B
int8_t  stageDivisor        = 1 B   (stage duration multiplier)
int8_t  trackIndex          = 1 B
                       total ≈ 262 B per sequence
```

### Track layout

```
17 sequences (16 patterns + 1 snapshot)  = ~4,454 B
Track-level config (~10 B)
                       total ≈ 4,470 B
```

**Under NoteTrack (9,544 B) container gate. Safe.**

### Engine layout (~120 B)

```
Phase accumulators, cursor state, sub-trigger scheduler,
accumulator counters, output state. Under 200 B.
```

**Under StochasticTrackEngine (912 B) engine container gate. Safe.**

---

## 3. New Files

### 3.1 `src/apps/sequencer/model/PhaseFluxSequence.h`

```cpp
#pragma once

#include "Config.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Types.h"

#include "core/math/Math.h"
#include "core/utils/StringBuilder.h"

#include <array>
#include <cstdint>

class PhaseFluxSequence {
public:
    static constexpr int StageCount = 16;
    static constexpr int GridSize = 4;

    // --- Enums ---

    enum class TemporalCurve : uint8_t {
        Linear,       // A — even spacing
        Exponential,  // B — accelerating bursts
        Logarithmic,  // C — decelerating, lazy onset
        Bell,         // D — density peak in middle
        Bounce,       // E — ballistic decay
        SampleHold,   // F — stepped pseudo-random
        Last
    };

    enum class PitchCurve : uint8_t {
        Flat,    // 1 — constant 0
        Ramp,    // 2 — monotonic 0→1
        Arc,     // 3 — peak at φ=0.5
        Walk,    // 4 — pseudo-random contour
        Mirror,  // 5 — |2φ−1|
        Last
    };

    enum class Direction : uint8_t {
        Forward,
        PingPong,
        Random,
        Last
    };

    enum class AccAdvance : uint8_t {
        PerSubTrigger,      // A — N increments per visit
        PerStage,           // B — 1 increment per visit
        PerGridCycle,       // C — 1 per full traversal
        Last
    };

    enum class Transform : uint8_t {
        None,
        Reverse,        // play sub-triggers in reverse order
        OctaveShift,    // ±1V on alternating sub-triggers
        Humanize,       // jitter on timing + CV
        Ratchet,        // double count, halve gate
        Last
    };

    // --- Stage ---

    class Stage {
    public:
        // Getters/setters for all 15 params
        int basePitch() const;
        void setBasePitch(int v);
        int subTriggerCount() const;
        void setSubTriggerCount(int v);
        TemporalCurve temporalCurve() const;
        void setTemporalCurve(TemporalCurve v);
        int temporalWarp() const;           // -100 to +100
        void setTemporalWarp(int v);
        int temporalResponse() const;       // -100 to +100
        void setTemporalResponse(int v);
        PitchCurve pitchCurve() const;
        void setPitchCurve(PitchCurve v);
        int pitchWarp() const;
        void setPitchWarp(int v);
        int pitchResponse() const;
        void setPitchResponse(int v);
        int gateLength() const;             // 0-100%
        void setGateLength(int v);
        Transform transform() const;
        void setTransform(Transform v);
        int accLength() const;              // 1-16
        void setAccLength(int v);
        int accStep() const;                // -100 to +100
        void setAccStep(int v);
        AccAdvance accAdvance() const;
        void setAccAdvance(AccAdvance v);
        bool skip() const;
        void setSkip(bool v);
        int pitchRange() const;             // 0-100 (centiVolts)
        void setPitchRange(int v);

        // print/edit helpers for each param
        // ...

        void clear();
        void write(VersionedSerializedWriter &writer) const;
        void read(VersionedSerializedReader &reader);

    private:
        int8_t   _basePitch = 0;           // -50..+50 centiVolts
        uint8_t  _subTriggerCount = 1;     // 1..8
        uint8_t  _temporalCurve = 0;       // TemporalCurve enum
        int8_t   _temporalWarp = 0;        // -100..+100
        int8_t   _temporalResponse = 0;    // -100..+100
        uint8_t  _pitchCurve = 0;          // PitchCurve enum
        int8_t   _pitchWarp = 0;           // -100..+100
        int8_t   _pitchResponse = 0;       // -100..+100
        uint8_t  _gateLength = 50;         // 0..100
        uint8_t  _transform = 0;           // Transform enum
        uint8_t  _accLength = 1;           // 1..16
        int8_t   _accStep = 0;             // -100..+100
        uint8_t  _accAdvance = 0;          // AccAdvance enum
        uint8_t  _skip = 0;                // bool
        uint8_t  _pitchRange = 100;        // 0..100 centiVolts
    };
    // static_assert(sizeof(Stage) == 16, "Stage must be 16 bytes");

    // --- Sequence-level params ---

    // divisor (same as other track types — base clock division)
    int divisor() const;
    void setDivisor(int v);

    // xDivisor, yDivisor — independent clock divisors for each axis
    int xDivisor() const;
    void setXDivisor(int v);
    int yDivisor() const;
    void setYDivisor(int v);

    // direction per axis
    Direction xDirection() const;
    void setXDirection(Direction v);
    Direction yDirection() const;
    void setYDirection(Direction v);

    // stageDivisor — multiplier for stage duration
    int stageDivisor() const;
    void setStageDivisor(int v);

    // stages
    const std::array<Stage, StageCount> &stages() const;
    std::array<Stage, StageCount> &stages();
    const Stage &stage(int index) const;
    Stage &stage(int index);

    // trackIndex
    void setTrackIndex(int index);

    // Serialization
    void clear();
    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    int8_t _trackIndex = -1;
    uint8_t _divisor = 6;            // 1/16 default
    uint8_t _xDivisor = 1;
    uint8_t _yDivisor = 1;
    uint8_t _xDirection = 0;         // Direction::Forward
    uint8_t _yDirection = 0;
    uint8_t _stageDivisor = 4;       // ×4 default stage duration
    std::array<Stage, StageCount> _stages;
};

// static_assert(sizeof(PhaseFluxSequence) <= 300, "PhaseFluxSequence too large");
```

### 3.2 `src/apps/sequencer/model/PhaseFluxTrack.h`

Follows StochasticTrack pattern exactly.

```cpp
#pragma once

#include "Config.h"
#include "Types.h"
#include "PhaseFluxSequence.h"
#include "Serialize.h"
#include "Routing.h"

#include <array>

class PhaseFluxTrack {
public:
    using PhaseFluxSequenceArray =
        std::array<PhaseFluxSequence, CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT>;

    // --- Properties ---
    // playMode, cvUpdateMode (same pattern as StochasticTrack)
    Types::PlayMode playMode() const;
    void setPlayMode(Types::PlayMode v);

    // sequences
    const PhaseFluxSequenceArray &sequences() const;
    PhaseFluxSequenceArray &sequences();
    const PhaseFluxSequence &sequence(int index) const;
    PhaseFluxSequence &sequence(int index);

    // Routing
    inline bool isRouted(Routing::Target target) const {
        return Routing::isRouted(target, _trackIndex);
    }
    inline void printRouted(StringBuilder &str, Routing::Target target) const {
        Routing::printRouted(str, target, _trackIndex);
    }
    void writeRouted(Routing::Target target, int intValue, float floatValue);

    // Lifecycle
    PhaseFluxTrack() { clear(); }
    void clear();

    void gateOutputName(int index, StringBuilder &str) const;
    void cvOutputName(int index, StringBuilder &str) const;

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    void setTrackIndex(int trackIndex);

    int8_t _trackIndex = -1;
    Types::PlayMode _playMode = Types::PlayMode::Aligned;
    PhaseFluxSequenceArray _sequences;

    friend class Track;
};

// static_assert(sizeof(PhaseFluxTrack) <= 9544, "PhaseFluxTrack must fit Track container");
```

### 3.3 `src/apps/sequencer/engine/PhaseFluxTrackEngine.h`

```cpp
#pragma once

#include "TrackEngine.h"
#include "model/PhaseFluxSequence.h"
#include "model/PhaseFluxTrack.h"

#include <cmath>

class PhaseFluxTrackEngine : public TrackEngine {
public:
    PhaseFluxTrackEngine(Engine &engine, const Model &model,
                         Track &track, const TrackEngine *linkedTrackEngine);

    Track::TrackMode trackMode() const override {
        return Track::TrackMode::PhaseFlux;
    }

    void reset() override;
    void restart() override;
    TickResult tick(uint32_t tick) override;
    void update(float dt) override;
    void changePattern() override;

    bool activity() const override;
    bool gateOutput(int index) const override;
    float cvOutput(int index) const override;
    float sequenceProgress() const override { return _stagePhase; }

    // UI accessors
    int cursorX() const { return _cursorX; }
    int cursorY() const { return _cursorY; }
    int activeStage() const { return _activeStageIndex; }
    float stagePhase() const { return _stagePhase; }
    int accCounter(int stageIndex) const { return _accCounters[stageIndex]; }

private:
    // --- Math ---

    // PowerBend: z^((1-p)/(1+p)), clamped safe for z=0
    static float powerBend(float z, float p);

    // WarpAxis(x, w) = PowerBend(clamp(x,0,1), w)
    static float warpAxis(float x, float w);

    // ResponseAxis(y, r) = PowerBend(clamp(y,0,1), r)
    static float responseAxis(float y, float r);

    // Evaluate temporal curve at warped/responded phase
    float evalTemporal(float rawPhase, const PhaseFluxSequence::Stage &stage) const;

    // Evaluate pitch curve at warped/responded phase
    float evalPitch(float phase, const PhaseFluxSequence::Stage &stage) const;

    // --- Sub-trigger scheduling ---

    struct SubTrigger {
        float timeFraction;   // 0..1 within stage
        float pitchCV;        // output voltage
        float gateFraction;   // gate duty as fraction of inter-trigger gap
    };

    // Compute sub-trigger schedule for a stage
    void computeSubTriggers(const PhaseFluxSequence::Stage &stage,
                            float accOffset, int stageIndex);

    // --- Cursor ---

    void advanceCursorX();
    void advanceCursorY();
    int stageIndex(int x, int y) const { return y * 4 + x; }

    // --- Stage lifecycle ---

    void startStage(int stageIdx);
    void endStage();
    void triggerSubTrigger(int idx);

    // --- Accumulator ---

    void advanceAccumulator(const PhaseFluxSequence::Stage &stage,
                            int stageIndex, AccAdvanceReason reason);
    float accOffset(int stageIndex) const;

    enum class AccAdvanceReason : uint8_t {
        SubTrigger,
        StageComplete,
        GridCycle
    };

    // --- Pitch curve functions (inline, not in Curve.h) ---

    static float pitchFlat(float phi);
    static float pitchRamp(float phi);
    static float pitchArc(float phi);
    static float pitchWalk(float phi, int stageIndex, int triggerIndex);
    static float pitchMirror(float phi);

    // --- Temporal curve functions ---

    // Reuse Curve::eval for Linear/Exp/Log/Bell, custom for Bounce/S&H
    static float temporalBounce(float x);
    float temporalSampleHold(int stageIndex, int triggerIndex, int count) const;

    // --- Members ---

    PhaseFluxTrack &_phaseFluxTrack;
    PhaseFluxSequence *_sequence = nullptr;

    // Cursor state
    int _cursorX = 0;
    int _cursorY = 0;
    int _cursorXDir = 1;   // for ping-pong
    int _cursorYDir = 1;

    // Clock dividers — accumulate ticks, fire on threshold
    uint32_t _xTickAccum = 0;
    uint32_t _yTickAccum = 0;
    uint32_t _xDivTicks = 0;
    uint32_t _yDivTicks = 0;

    // Stage state
    int _activeStageIndex = -1;
    float _stagePhase = 0.f;       // 0..1
    uint32_t _stageStartTick = 0;
    uint32_t _stageDuration = 0;   // in ticks
    bool _stageActive = false;

    // Sub-trigger schedule (max 8 per stage, or 16 with Ratchet transform)
    static constexpr int kMaxSubTriggers = 16;
    SubTrigger _subTriggers[kMaxSubTriggers];
    int _subTriggerCount = 0;
    int _nextSubTrigger = 0;
    uint32_t _lastSubTriggerTick = 0;

    // Accumulator counters — one per grid cell
    uint8_t _accCounters[16] = {};

    // Grid-cycle tracking (for AccAdvance::PerGridCycle)
    int _gridCycleVisits = 0;
    static constexpr int kGridCycleTarget = 16; // 4×4

    // Output state
    bool _gateOutput = false;
    float _cvOutput = 0.f;
    bool _activity = false;
    uint32_t _gateTimer = 0;
    uint32_t _activityTimer = 0;

    // Random for S&H temporal, Walk pitch, Humanize transform
    Random _rng;

    // Walk seed cache — one per stage, deterministic
    uint16_t _walkSeed[16] = {};
};

// static_assert(sizeof(PhaseFluxTrackEngine) <= 912, "PhaseFluxTrackEngine must fit engine container");
```

### 3.4 `src/apps/sequencer/engine/PhaseFluxTrackEngine.cpp`

Full engine implementation. Key algorithms:

#### PowerBend

```cpp
float PhaseFluxTrackEngine::powerBend(float z, float p) {
    if (z <= 0.f) return 0.f;
    if (z >= 1.f) return 1.f;
    float numerator = 1.f - p;
    float denominator = 1.f + p;
    if (fabsf(denominator) < 1e-7f) return z;  // p ≈ -1, near-identity
    float exponent = numerator / denominator;
    return powf(z, exponent);
}
```

#### Tick algorithm

```
tick(t):
    1. Advance X/Y tick accumulators
    2. Check if X or Y fires (respective divisor reached)
    3. If either fires:
       a. Advance that cursor
       b. If stage active → endStage() (cut behavior)
       c. Compute new stage index from (cursorX, cursorY)
       d. If stage not skipped → startStage()
    4. If stage active:
       a. Advance stage phase: φ = (tick - stageStartTick) / stageDuration
       b. If φ ≥ 1.0 → stageComplete(), advanceAccumulator(StageComplete)
       c. Check if next sub-trigger should fire (tick ≥ threshold)
       d. If so → triggerSubTrigger()
    5. Decay gate timer
    6. Return CvUpdate|GateUpdate as appropriate
```

#### Cursor advancement

```
advanceCursor(axis, divisor, direction):
    Forward:  pos = (pos + 1) % 4
    PingPong: pos += dir; if pos > 3 or pos < 0: dir = -dir; clamp
    Random:   pos = rng.next() % 4
```

#### Sub-trigger scheduling

```
computeSubTriggers(stage, accOffset, stageIndex):
    N = stage.subTriggerCount
    if stage.transform == Ratchet: N *= 2

    for i in 0..N-1:
        raw_x = i / (N-1)   // [0..1]

        // Temporal pipeline
        warped_x = warpAxis(raw_x, stage.temporalWarp / 100.f)
        curved   = evalTemporal(warped_x, stage.temporalCurve)
        final_x  = responseAxis(curved, stage.temporalResponse / 100.f)
        timeFrac = clamp(final_x, 0, 1)

        // Pitch pipeline — uses same final temporal position as phase input
        phi = timeFrac
        phi_warped = warpAxis(phi, stage.pitchWarp / 100.f)
        p_curved   = evalPitch(phi_warped, stage.pitchCurve, stageIndex, i)
        p_final    = responseAxis(p_curved, stage.pitchResponse / 100.f)

        pitchCV = basePitch/100.f + accOffset + pitchRange/100.f * p_final

        // Gate duty = stage.gateLength% of inter-trigger gap
        gateFrac = stage.gateLength / 100.f

        // Transforms
        if transform == Reverse:
            swap subTriggers[i] with subTriggers[N-1-i]
        if transform == OctaveShift:
            if i % 2 == 1: pitchCV += 1.0f
        if transform == Humanize:
            timeFrac  += jitter_ms / stageDuration
            pitchCV   += jitter_cv
        if transform == Ratchet:
            gateFrac /= 2  // already doubled N above

        subTriggers[i] = { timeFrac, pitchCV, gateFrac }

    // Sort by timeFraction (for Reverse transform reordering)
    sort subTriggers by timeFraction
```

#### Temporal curve evaluation

```
evalTemporal(warped, curveId):
    Linear     → warped                    (even spacing)
    Exponential → Curve::eval(ExpUp, warped)  (accelerating)
    Logarithmic → Curve::eval(LogUp, warped)  (decelerating)
    Bell       → Curve::eval(Bell, warped)    (center peak)
    Bounce     → temporalBounce(warped)        (ballistic decay)
    SampleHold → temporalSampleHold(...)       (stepped random)
```

#### Pitch curve evaluation

```
evalPitch(phi, curveId, stageIndex, triggerIndex):
    Flat   → 0.f
    Ramp   → phi
    Arc    → 1.f - abs(2*phi - 1)
    Walk   → deterministic pseudo-random from stage seed
    Mirror → abs(2*phi - 1)
```

---

## 4. Wiring into Performer Infrastructure

### 4.1 `model/Track.h`

```diff
 enum class TrackMode : uint8_t {
     Note,
     Curve,
     MidiCv,
     Tuesday,
     DiscreteMap,
     Indexed,
     Teletype,
     Stochastic,
+    PhaseFlux,
     Last,
     Default = Note
 };
```

- Add `trackModeName()` case: `return "PhaseFlux"`
- Add `trackModeSerialize()` case: `return 8`
- Add `#include "PhaseFluxTrack.h"`
- Add accessor pair (`phaseFluxTrack()` const/mutable)
- Add to `_container` typedef:
  ```
  Container<NoteTrack, CurveTrack, ..., StochasticTrack, PhaseFluxTrack>
  ```
- Add `phaseFlux*` to `_track` union
- Add cases in `setTrackIndex()`, `initContainer()`

### 4.2 `model/Track.cpp`

Add cases in every switch statement:
- `clearPattern()` → `_track.phaseFlux->sequence(i).clear()`
- `copyPattern()` → assignment
- `gateOutputName()` → `str("Gate")`
- `cvOutputName()` → `str("CV")`
- `write()` → `_track.phaseFlux->write(writer)`
- `read()` → `_track.phaseFlux->read(reader)`

### 4.3 `engine/Engine.h`

```diff
- using TrackEngineContainer = Container<..., StochasticTrackEngine>;
+ using TrackEngineContainer = Container<..., StochasticTrackEngine, PhaseFluxTrackEngine>;
```

Add `#include "PhaseFluxTrackEngine.h"`

### 4.4 `engine/Engine.cpp`

Add factory case:
```cpp
case Track::TrackMode::PhaseFlux:
    trackEngine = trackContainer.create<PhaseFluxTrackEngine>(*this, _model, track, linkedTrackEngine);
    break;
```

### 4.5 `model/Routing.h`

Add new target range after Stochastic:
```cpp
// PhaseFlux Track Targets
PhaseFluxFirst,
PhaseFluxBasePitch = PhaseFluxFirst,
PhaseFluxPitchRange,
PhaseFluxTemporalWarp,
PhaseFluxPitchWarp,
PhaseFluxSubTriggerCount,
PhaseFluxGateLength,
PhaseFluxLast = PhaseFluxGateLength,
```

Add to:
- `targetName()` switch
- `targetSerialize()` switch (use IDs 80-85)
- `isPhaseFluxTarget()` helper
- `isPerTrackTarget()` — add `isPhaseFluxTarget()`

### 4.6 `model/Routing.cpp`

- Add `isPhaseFluxTarget()` range check
- Add PhaseFlux branch in `writeRouted()` dispatch (same pattern as Stochastic)
- Add range table entries for PhaseFlux targets

### 4.7 `model/ClipBoard.h`

- Add `PhaseFluxSequence` to `Type` enum
- Add `copyPhaseFluxSequence()` / `pastePhaseFluxSequence()` / `canPastePhaseFluxSequence()`
- Add `PhaseFluxSequence` to `Pattern::sequences[]` union
- Add `PhaseFluxSequence` to `Container<>` template (or Pattern union, following Stochastic pattern)

### 4.8 `model/ClipBoard.cpp`

Implement copy/paste/canPaste following Stochastic pattern.

### 4.9 `ui/pages/TopPage.cpp`

Three switch statements:

```cpp
// setTrackView (S+Track)
case Track::TrackMode::PhaseFlux:
    setMainPage(pages.phaseFluxConfig);
    break;

// setSequenceView (S+Sequence)
case Track::TrackMode::PhaseFlux:
    setMainPage(pages.phaseFluxSequence);
    break;

// setSequenceEditPage (S+Edit)
case Track::TrackMode::PhaseFlux:
    setMainPage(pages.phaseFluxSequenceEdit);
    break;
```

### 4.10 `ui/pages/TrackPage.h`

Add `PhaseFluxTrackListModel _phaseFluxTrackListModel;` member.

### 4.11 `ui/pages/TrackPage.cpp`

```cpp
case Track::TrackMode::PhaseFlux:
    _phaseFluxTrackListModel.setTrack(track.phaseFluxTrack(), _project, &_engine);
    newListModel = &_phaseFluxTrackListModel;
    break;
```

### 4.12 `ui/pages/Pages.h`

```cpp
#include "PhaseFluxConfigPage.h"
#include "PhaseFluxSequencePage.h"
#include "PhaseFluxSequenceEditPage.h"
// ...
PhaseFluxConfigPage phaseFluxConfig;
PhaseFluxSequencePage phaseFluxSequence;
PhaseFluxSequenceEditPage phaseFluxSequenceEdit;
```

### 4.13 `ui/pages/OverviewPage.cpp`

```cpp
case Track::TrackMode::PhaseFlux:
    canvas.setColor(Color::Medium);
    canvas.drawText(36, y, "PhaseFlux");
    break;
```

---

## 5. UI Files (stub phase — functional skeleton)

### 5.1 `ui/model/PhaseFluxTrackListModel.h`

List model exposing track-level params for the Track page.
Pattern follows `StochasticTrackListModel.h`.

### 5.2 `ui/pages/PhaseFluxConfigPage.h`

Track config page — shows playMode, cv output routing, etc.
Minimal: cursor positions, divisor settings, axis direction.
Displayed on S+Track shortcut.

### 5.3 `ui/pages/PhaseFluxSequencePage.h`

Grid overview — shows 4×4 grid with cursor position, stage skip states.
Displayed on S+Sequence shortcut.

### 5.4 `ui/pages/PhaseFluxSequenceEditPage.h`

Stage editor — edit all 15 parameters of the selected stage.
Select stage via encoder, edit params via S+encoder pages.
Displayed on S+Edit shortcut.

---

## 6. Serialization

PhaseFlux track mode serializes as **8** (after Stochastic=7).

`PhaseFluxSequence` serialization order:
1. `_divisor` (uint8)
2. `_xDivisor` (uint8)
3. `_yDivisor` (uint8)
4. `_xDirection` (uint8)
5. `_yDirection` (uint8)
6. `_stageDivisor` (uint8)
7. 16 × Stage (each Stage writes its 15 params as compact bytes)

`PhaseFluxTrack` serialization:
1. `_playMode` (uint8)
2. 17 × Sequence

---

## 7. Implementation Order

1. **PhaseFluxSequence.h** — data model, no dependencies
2. **PhaseFluxTrack.h** — thin wrapper around sequence array
3. **Wire Track.h/cpp** — enum, container, accessors, serialization
4. **PhaseFluxTrackEngine.h/.cpp** — core engine, math, scheduling
5. **Wire Engine.h/cpp** — container typedef, factory
6. **Wire Routing.h/cpp** — routing targets
7. **Wire ClipBoard.h/cpp** — clipboard support
8. **PhaseFluxTrackListModel.h** — UI model
9. **PhaseFluxConfigPage.h** — stub config page
10. **PhaseFluxSequencePage.h** — stub grid page
11. **PhaseFluxSequenceEditPage.h** — stub edit page
12. **Wire Pages.h, TopPage.cpp, TrackPage.h/cpp, OverviewPage.cpp**
13. **Build, check sizeof, verify RAM budget**

---

## 8. Testing Strategy

### Unit tests (sim build)

1. **PowerBend math** — identity at p=0, compression at p>0/<0, edge cases
2. **Temporal curve evaluation** — all 6 curves produce monotonic/bounded output
3. **Pitch curve evaluation** — all 5 curves produce bounded [0,1] output
4. **Sub-trigger scheduling** — correct count, sorted by time, within [0,1]
5. **Cursor advancement** — Fwd/PingPong/Random for both axes
6. **Accumulator** — wrap at length, correct advance timing (A/B/C modes)
7. **Transform: Reverse** — sub-triggers reversed in time
8. **Transform: OctaveShift** — alternating ±1V
9. **Transform: Ratchet** — doubled count, halved gate
10. **Serialization roundtrip** — write/read produces identical state

### Integration tests

1. Stage lifecycle — start, phase advance, sub-trigger fire, end
2. Cut behavior — cursor move kills active stage
3. Gate/CV output — correct timing and voltage at sub-trigger boundaries
4. Pattern switch — clean state transition
5. Mute — gate suppressed, CV behavior depends on cvUpdateMode

---

## 9. Deferred to v2

- Per-stage CV routing (modulation inputs)
- Accumulator direction bit (bi-directional counting)
- Accumulator ping-pong mode
- Mutation system
- Stage overlap / polyphonic mode
- External phase CV input
- Per-stage duration (decoupled from global stageDivisor)
- Snapshot save/restore UI
- Grid sizes other than 4×4
