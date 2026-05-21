# Performer / XFORMER Architecture Reference

Reference for agents working on engine code, track types, or new TrackModes.
Read this before designing any feature that touches timing, output, or cross-track behavior.

Every claim here is grounded in a specific file and line number — verify against the code
if the codebase has moved on. Last reconciled: 2026-05-21.

---

## 1. Process model

The hardware/simulator runs on FreeRTOS. Three periodic tasks at 1 ms each:

```
src/apps/sequencer/Sequencer.cpp:100-114
```

- `driverTask` — shift register, button/LED matrix, encoder polling.
- `engineTask` — calls `engine.update()`. **This is where sequencing happens.**
- `usbhTask` — USB host stack.

Engine update is therefore **1 kHz**, regardless of musical tempo. Tempo-locked timing happens
inside `engine.update()` via the master clock subsystem.

---

## 2. Clock and PPQN

Two PPQN constants in `src/apps/sequencer/Config.h`:

```
CONFIG_PPQN          192   // master clock pulses per quarter note (Config.h:33)
CONFIG_SEQUENCE_PPQN  48   // sequence step resolution per quarter note (Config.h:36)
```

The ratio `CONFIG_PPQN / CONFIG_SEQUENCE_PPQN = 4` means one sequence-PPQN tick = 4 master
ticks. Track engines that divide sequence steps multiply their per-sequence-tick divisor by
this ratio to get a per-master-tick value. See NoteTrack timing below.

`Clock::checkTick(&tick)` (in `Engine::update`) reads pending master clock ticks since last
call. Multiple ticks can fire per 1 ms update at high tempos.

`Clock::tickPosition()` returns a `double` = current integer tick + the sub-tick fraction
between ticks, derived from a high-resolution wall-clock timer:

```cpp
// src/apps/sequencer/engine/Clock.cpp:460-478
double Clock::tickPosition() const {
    const uint32_t periodUs = tickPeriodUs();
    ...
    const uint32_t now = HighResolutionTimer::us();
    const uint32_t elapsed = now - lastTickUs;
    const uint32_t baseTick = tick > 0 ? tick - 1 : 0;
    return double(baseTick) + (double(elapsed) / double(periodUs));
}
```

PlayMode::Free uses this for sub-tick precision. PlayMode::Aligned ignores it.

---

## 3. The Engine update loop

`Engine::update()` runs once per 1 ms tick. Sequence of work each cycle:

```
src/apps/sequencer/engine/Engine.cpp (around 130-205)
```

1. Update routing (CV inputs evaluated, route to targets).
2. Update play state.
3. While clock has unconsumed master ticks (`_clock.checkTick(&tick)`):
   - For each track in **index order 0 → 7**:
     - Call `trackEngine.tick(tick)`.
     - If the tick result requested a CV update AND the per-track update reducer permits, call `trackEngine.update(0.f)`.
   - Call `updateTrackOutputs()` to push current logical outputs to the routing layer.
4. After the tick loop, unconditionally call `updateTrackOutputs()` so hardware refreshes at 1 kHz even when no clock tick fired.

The track loop:

```cpp
// src/apps/sequencer/engine/Engine.cpp:162-176
for (size_t trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
    auto &track = _model.project().track(trackIndex);
    auto &trackEngine = *_trackEngines[trackIndex];
    TrackEngine::TickResult result = TrackEngine::TickResult::NoUpdate;
    if (track.runGate()) {
        result = trackEngine.tick(tick);
    }
    if ((result & TrackEngine::TickResult::CvUpdate) && _trackUpdateReducers[trackIndex].update()) {
        trackEngine.update(0.f);
    }
}
```

**Tick-order guarantee:** track N's `tick()` runs strictly after tracks 0..N-1 in the same tick.
Reading `_engine.trackEngine(M).cvOutput(0)` from inside track N sees the *current* tick's value
when M < N, and the *previous* tick's value when M > N. No latching mechanism beyond natural
ordering.

---

## 4. The TrackEngine interface

All track types subclass `TrackEngine`. Pure virtuals every subclass implements:

```cpp
// src/apps/sequencer/engine/TrackEngine.h:85-90
virtual bool  activity() const = 0;
virtual bool  gateOutput(int index) const = 0;
virtual float cvOutput(int index) const = 0;
```

- `gateOutput(index)` returns the engine's logical gate bool (mute and fill applied per-engine).
- `cvOutput(index)` returns the engine's logical CV in volts as a float.
- The `int index` parameter is a per-track output slot. **All single-output engines ignore the
  index** (Note, Curve, Tuesday, Stochastic, Indexed, DiscreteMap, Teletype). Only
  `MidiCvTrackEngine` uses it for polyphonic CV slots.

Other commonly-overridden methods:
- `tick(uint32_t tick)` — main per-master-tick entry. Returns a `TickResult` bitmask.
- `update(float dt)` — 1 kHz refresh; typically used for CV slide interpolation.
- `reset()`, `restart()`, `stop()` — transport state changes.
- `changePattern()` — called when the project's selected pattern changes; engines typically
  re-point a sequence pointer here.
- `trackMode()` — returns the engine's `Track::TrackMode` enum value.

---

## 5. Output pipeline (logical → physical)

The Engine maintains:

- `_gateOutput` — a `uint8_t` bitfield of 8 logical gate bits, one per physical gate jack.
  Declared in `src/apps/sequencer/engine/Engine.h:248`; updated to hardware in
  `src/platform/stm32/drivers/GateOutput.cpp`.
- `_cvOutput` — an array of 8 float volts, one per physical CV jack. Pushed to hardware via
  `Dac::set...()`.

Mapping from logical track outputs to physical jacks happens in `Engine::updateTrackOutputs()`
at `src/apps/sequencer/engine/Engine.cpp:559-698`. Each jack `i` has:

- `gateOutputTracks[i]` — which track index drives this gate jack.
- `cvOutputTracks[i]` — which track index drives this CV jack.

Within a single track, multiple jacks can pull from successive output slots via:

```cpp
// Engine.cpp:639 (approximate)
_gateOutput.setGate(i, _trackEngines[gateOutputTrack]->gateOutput(trackGateIndex[gateOutputTrack]++));
...
// Engine.cpp:691 (approximate)
_cvOutput.setChannel(i, applyModulatorOffset(i, _trackEngines[cvOutputTrack]->cvOutput(cvSlot)));
```

Two consequences:

1. **One engine can drive multiple jacks.** Pool rotation walks the slot index per call.
   For single-output engines (everything except `MidiCvTrackEngine`) the rotated reads all
   return the same value.
2. **Logical engine output ≠ physical jack output.** What `cvOutput(0)` returns is the
   engine's pre-routing, pre-modulator-offset, pre-calibration volts. The physical pin sees
   that value after modulator offset (`applyModulatorOffset`) and after `Calibration` runs
   it through the DAC's volts-to-code conversion (`src/apps/sequencer/engine/CvOutput.cpp:14-19`).

For most user setups the two match. When a track uses a rotated output pool, they diverge.

---

## 6. DAC and GateOutput drivers

### CV DAC

`src/platform/stm32/drivers/Dac.{h,cpp}` — TI **DAC8568** over SPI3. 16-bit input register.

```cpp
// src/platform/stm32/drivers/Dac.cpp:26-32
Dac::Dac(Type type) {
    switch (type) {
    case Type::DAC8568C: _dataShift = 0; break;  // 16-bit
    case Type::DAC8568A: _dataShift = 1; break;  // effective 15-bit (data left-shifted by 1)
    }
}
```

Volts-to-DAC-code conversion lives in `Calibration::cvOutput(i).voltsToValue(float volts)`
(per-channel calibrated). `CvOutput::update()` writes the calibrated 16-bit codes via DMA
every 1 ms.

### Gate Output

`src/platform/stm32/drivers/GateOutput.cpp`. Gates are literal 1-bit values pushed to shift
register channel 2:

```cpp
// src/platform/stm32/drivers/GateOutput.cpp:10-12
void GateOutput::update() {
    _shiftRegister.set(ShiftRegister::Gate, _gateOutput);
}
```

The shift register is clocked by `driverTask` at 1 kHz. All gate shaping (envelope, length,
retrigger, swing) is done **inside the track engines** before they expose a final boolean
via `gateOutput(0)`. The platform layer is pass-through.

---

## 7. Timing patterns per TrackMode

Each TrackMode picks a timing strategy that fits its musical model. New track types should
pick the closest existing pattern rather than inventing a new one.

### 7.1 NoteTrackEngine — divisor + modulo + SortedQueue

File: `src/apps/sequencer/engine/NoteTrackEngine.{h,cpp}`. The reference pattern for
"step-quantized sequencing with sub-step events."

Each call to `tick(uint32_t tick)`:

```cpp
// NoteTrackEngine.cpp:174-183
float clockMult = sequence.clockMultiplier() * 0.01f;
uint32_t divisor = sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
divisor = std::max<uint32_t>(1, std::lround(divisor / clockMult));
uint32_t resetDivisor = sequence.resetMeasure() * _engine.measureDivisor();
uint32_t relativeTick = resetDivisor == 0 ? tick : tick % resetDivisor;
if (resetDivisor > 0 && relativeTick == 0) reset();
```

- `divisor` is ticks-per-step at master-tick resolution.
- `relativeTick` is the tick position within the current bar (when `resetMeasure > 0`).

PlayMode::Aligned fires a step on `relativeTick % divisor == 0`. Hard grid.

PlayMode::Free uses `tickPosition()` and fires a step when `floor(tickPos / divisor)` changes
to a new value. Same musical grid result; Free exposes the sub-tick fraction for downstream
math (Curve uses it, Note doesn't).

When a step fires, NoteTrack pushes future events into two scheduling queues:

```cpp
// NoteTrackEngine.h:119,133
SortedQueue<Gate, 16> _gateQueue;
SortedQueue<Cv,   16> _cvQueue;
```

Each event has an absolute future tick. The tick loop drains anything at or before the
current tick:

```cpp
// NoteTrackEngine.cpp:380 (approximate)
while (!_gateQueue.empty() && tick >= _gateQueue.front().tick) {
    auto event = _gateQueue.front(); _gateQueue.pop();
    _activity = event.gate;
    _gateOutput = (!mute() || fill()) && _activity;
    ...
}
```

This pattern handles gate length, gate offset (sub-step delay), retriggers (ratchets), and
swing. Without queues, sub-step events couldn't land at fractional offsets.

`update(float dt)` runs at 1 kHz and only handles CV slide via `Slide::applySlide` and
monitoring overrides.

### 7.2 CurveTrackEngine — continuous CV via phase accumulator

File: `src/apps/sequencer/engine/CurveTrackEngine.{h,cpp}`. Reference for "continuously
varying CV that's not step-aligned."

Aligned mode uses the same divisor-modulo as NoteTrack to compute `_currentStepFraction =
(relativeTick % divisor) / divisor`. Free mode is a phase accumulator advanced by
`tickPosition` deltas scaled by a configurable curve rate:

```cpp
// CurveTrackEngine.cpp:181-230 (approximate)
double speed = (1.0 / divisor) * curveRate;
double tickPos = _engine.clock().tickPosition();
double deltaTicks = tickPos - _lastFreeTickPos;
_freePhase += deltaTicks * speed;
if (_freePhase >= 1.0) { _freePhase -= 1.0; advance + trigger; }
```

CV is recomputed every tick by sampling the active step's curve shape at the current
fraction, then `update(float dt)` linearly interpolates between successive tick samples for
smooth 1 kHz output:

```cpp
// CurveTrackEngine.cpp:273-281 (approximate)
_tickPhase += dt / tickDuration;
_cvOutput = _cvPrev + (_cvCurrent - _cvPrev) * std::min(1.f, _tickPhase);
```

`globalPhase` (0..1) shifts the *lookup position across the whole sequence*, not the current
step — useful for global phase rotation of multi-step shapes.

### 7.3 IndexedTrackEngine — time-driven counters

File: `src/apps/sequencer/engine/IndexedTrackEngine.{h,cpp}`. Reference for "variable-length
steps with explicit durations."

Instead of divisor-modulo, each step has `duration` and `gateLength` in ticks. The engine
maintains `_stepTimer` and `_gateTimer` and decrements them per tick using fractional
`tickPosition` deltas scaled by `clockMultiplier`:

```cpp
// IndexedTrackEngine.cpp:225-272 (approximate)
double tickPos = _engine.clock().tickPosition();
double deltaTicks = tickPos - _lastFreeTickPos;
_lastFreeTickPos = tickPos;
double scaledDelta = deltaTicks * clockMult;
if (_gateTimer > 0.0) { _gateTimer -= scaledDelta; ... }
_stepTimer += scaledDelta;
if (_stepTimer >= stepDuration) { advanceStep(); triggerStep(); }
```

Gate state is read directly from the timer:

```cpp
// IndexedTrackEngine.cpp:490-496
bool IndexedTrackEngine::gateOutput(int index) const {
    if (_monitorOverrideActive) return _gateOutput;
    return _engine.state().running() && !mute() && _gateTimer > 0;
}
```

`SyncMode::External` uses rising-edge detection on `routedSync()` (`prevSync <= 0 && now > 0`)
to align step starts to an external CV signal.

### 7.4 DiscreteMapTrackEngine — Internal ramp vs External CV scan

File: `src/apps/sequencer/engine/DiscreteMapTrackEngine.{h,cpp}`. Reference for "CV-driven
step position bypassing the clock entirely."

Selected by `sequence.clockSource()`. **Internal** mode computes a ramp (Aligned: from
`relativeTick % periodTicks`; Free: from `tickPosition`) and sets `_currentInput = _rampValue`.
**External** mode reads `_discreteMapTrack.routedInput()` directly and sets
`_currentInput = getRoutedInput()`. PlayMode and divisor are bypassed:

```cpp
// DiscreteMapTrackEngine.cpp:205-208
} else {
    // External mode: ignore PlayMode, use external input
    _currentInput = getRoutedInput();
}
```

Step detection uses `findActiveStage(input, prevInput)` — each stage defines a threshold and
a trigger direction (Rise/Fall/Both/Off). When `prevInput` crosses a threshold to enter a new
stage, a gate is fired with a configurable `gateLengthPercent` of the step's period (or a
fixed short pulse if zero). `_prevInput = _currentInput` is updated at the end of each tick,
so detection is one-tick-delta against the previous tick's sample.

Important: routing engine writes to `routedInput()` must happen **before** this engine's
`tick()`. The routing engine runs first in `Engine::update()` (see Engine.cpp around line 152).

### 7.5 TuesdayTrackEngine — divisor + per-step micro-gate queue

File: `src/apps/sequencer/engine/TuesdayTrackEngine.{h,cpp}`. Reference for "algorithm-driven
generation with per-step trills, polyrhythms, and per-micro-event CV targets."

Macro tick is divisor-based like NoteTrack. On each step boundary, the engine calls
`generateStep()` which dispatches to one of 15 algorithms (Tritrance, Aphex, Autechre, etc.)
and returns a `TuesdayTickResult` carrying note/octave/velocity/accent/slide/gateRatio/
gateOffset/trillCount/polyCount/isSpatial and an array of `noteOffsets[8]`.

The result is decomposed into sub-step events pushed to a sorted queue, with each event
carrying its own `cvTarget`:

```cpp
// TuesdayTrackEngine.cpp:1903-1926 (approximate)
while (!_microGateQueue.empty() && tick >= _microGateQueue.front().tick) {
    auto event = _microGateQueue.front(); _microGateQueue.pop();
    _gateOutput = event.gate;  _activity = event.gate;
    if (event.gate) _cvOutput = event.cvTarget;
}
```

Per-event CV target is what makes algorithm-driven trills with per-step pitch work. Plain
NoteTrack uses a separate `_cvQueue` because all gate events for one step share the same CV.

A `tickMasking` mechanism (prime-number progression) can suppress entire ticks from algorithm
processing — when masked, outputs are frozen.

### 7.6 TeletypeTrackEngine — cross-track output reads

File: `src/apps/sequencer/engine/TeletypeTrackEngine.cpp`. Reference for "reading other
tracks' outputs."

```cpp
// TeletypeTrackEngine.cpp:909-915
if (source >= TeletypeTrack::TriggerInputSource::LogicalGate1 &&
    source <= TeletypeTrack::TriggerInputSource::LogicalGate8) {
    int trackIndex = int(source) - int(TeletypeTrack::TriggerInputSource::LogicalGate1);
    if (trackIndex >= 0 && trackIndex < CONFIG_TRACK_COUNT) {
        return _engine.trackEngine(trackIndex).gateOutput(0);
    }
}
```

Also reads `cvOutput(0)` at line 845 to feed Teletype script variables. Both reads return
**logical engine output**, not post-routing physical pin values. See §5.

---

## 8. Common engine patterns

### 8.1 SortedQueue

`src/apps/sequencer/engine/SortedQueue.h`. Fixed-capacity circular buffer with
insertion-sort by a comparator. Capacity is a template parameter; typical use is 16.

Used to schedule future gate/CV/event triggers at absolute ticks. Drained when `tick >=
front().tick`. The fixed capacity matters: if too many sub-events are pushed (e.g. 4 burst
children × 3 events each = 12, plus parent gate-on/off = 14, plus next parent setup), the
queue can overflow. Push behavior on overflow depends on the implementation — verify before
designing dense scheduling.

### 8.2 Slide interpolation in update()

Used by NoteTrack and StochasticTrack (and adoptable by any track that wants CV slew):

```cpp
// NoteTrackEngine.cpp:501-505 (approximate)
if (_slideActive && _noteTrack.slideTime() > 0) {
    Slide::applySlide(_cvOutput, _cvOutputTarget, ...);
}
```

`slideTime` is in arbitrary units (typically 0-100). The slide moves `_cvOutput` toward
`_cvOutputTarget` over `slideTime`-scaled milliseconds at the 1 kHz update rate.

### 8.3 resetMeasure

Bar-boundary hard reset. When `sequence.resetMeasure() > 0`:

- `resetDivisor = sequence.resetMeasure() * _engine.measureDivisor()` — total ticks per
  resetMeasure period.
- `relativeTick = tick % resetDivisor` — position within the period.
- When `relativeTick == 0`, engine calls `reset()` to clear queues, sequence state, current
  step, and accumulators.

This is the standard mechanism for making tracks lock to bar boundaries. If you add new
engine state, decide whether `reset()` should clear it.

### 8.4 PlayMode Aligned vs Free

`Aligned` — step boundaries fire on `relativeTick % divisor == 0`. Hard grid.

`Free` — step boundaries fire on `floor(tickPosition / divisor)` changing. Same musical
result, but `tickPosition` is a sub-tick-precise `double` so downstream phase math (Curve's
free-phase accumulator) can be smoother.

Most engines support both. The choice is sequence-owned.

### 8.5 changePattern()

The Engine calls `trackEngine.changePattern()` whenever the project's selected pattern
changes. Engines typically just re-point a sequence pointer:

```cpp
// StochasticTrackEngine.h:32-34
void changePattern() override {
    _sequence = &(_stochasticTrack.sequence(pattern()));
}
```

What does NOT happen on pattern change:
- Engine RNG is not re-seeded.
- Scheduling queues are not cleared.
- `_patternIndex`/`_stepIndex` is not reset.
- Buffers (heap or inline) are not cleared.

If your new track type needs any of those to happen on pattern change, override
`changePattern()` and do it explicitly.

### 8.6 The `_engine.measureDivisor()` helper

Returns ticks-per-bar at the current tempo and time signature. Used by:
- resetMeasure math (`resetDivisor = resetMeasure * measureDivisor()`).
- Bar-quantized loop length calculations.
- Any feature that needs to align to musical bars rather than absolute tick counts.

### 8.7 Engine lifecycle invariants (XFORMER additions)

Two safety properties that aren't in upstream Performer. Both fix hard-fault classes that
surfaced once XFORMER added track types with non-trivial engine destruction (specifically
StochasticTrackEngine, which allocates `_lockedParents` on the heap and frees it in
`~StochasticTrackEngine`).

#### 8.7.1 `updateTrackSetups()` runs before clock events in `Engine::update()`

Engine containers must match model `trackMode` before any engine code reads
`_trackEngines[i]`. Upstream Performer ran `updateTrackSetups()` later in `Engine::update()`
(after `updateBusSafetyMode` and the clock-event loop). That order is unsafe when the model
union is swapped while the engine is suspended or locked: `_engine.suspend()` internally
calls `_clock.masterStop()` which queues a `Clock::Stop` and possibly `Clock::Reset` event;
on resume, those events fire BEFORE the engine container rebuild, and `Engine::reset()`
iterates `_trackEngines[]` calling `trackEngine->reset()` on the stale type whose underlying
union memory has just been overwritten by `Track::clear() → initContainer()`. ARM hard fault.

XFORMER moves `updateTrackSetups()` to the top of `Engine::update()` (immediately after the
suspend-handling block), so engine containers are reconciled to model state before any code
touches `_trackEngines[]`. The relevant invariant: **inside `Engine::update()` after line
~106, `_trackEngines[i]->trackMode() == _project.track(i).trackMode()` for every `i`.**

Files: `src/apps/sequencer/engine/Engine.cpp` `Engine::update()`.

#### 8.7.2 `Container::create<U>()` destructs the prior occupant

Upstream `Container::create<U>()` did `return new(_data) U(args...)` — placement-new with
no destruction of whatever was there before. Callers were expected to remember to call
`destroy()` first, but no caller did, and the original tracks (NoteTrack, CurveTrack, etc.)
were trivially destructible so the omission was harmless.

StochasticTrackEngine breaks that assumption: `~StochasticTrackEngine` calls
`freeLockedSteps()` which `delete[] _lockedParents`. Skipping the destructor leaks ~1.8 KB
of heap (`64 × sizeof(LockedParentEvent)`) on every Stochastic→other engine swap. On
sim the leak is invisible (huge heap). On STM32 with newlib `sbrk`-managed heap, two or
three cycles exhaust the heap; the next `new` either returns nullptr (silently broken) or
corrupts heap state (hard fault).

XFORMER's `Container::create<U>()` stores a type-erased destructor pointer
(`destructorThunk<U>`) on each construction and invokes it on the next `create()` or on
Container destruction. Side effect: the typed-pointer `destroy(U *object)` overload was
replaced with no-arg `destroy()` (the destructor pointer is stored internally; the caller
doesn't need to pass it). Container also gains its own destructor, so scope-exit cleanup
is automatic.

```cpp
// src/core/utils/Container.h
template<typename U, typename... Args>
U *create(Args&&... args) {
    if (_destructor) { _destructor(_data); _destructor = nullptr; }
    U *obj = new(_data) U(std::forward<Args>(args)...);
    _destructor = &destructorThunk<U>;
    return obj;
}
```

This is a generic fix that protects any future track-engine type with non-trivial
destruction, not just Stochastic. New engines that allocate on the heap (or hold any
resource that needs cleanup) get correct destructor invocation on type swap automatically.

Files: `src/core/utils/Container.h`. Call-site changes: `LaunchpadController.cpp` and
`ControllerManager.cpp` use the no-arg `destroy()` form.

---

## 9. RAM budget reference

Two relevant regions:

- **SRAM (`.data + .bss`)** — currently ≈ 119,960 B (91.4% of 128 KB). Headroom ≈ 8 KB.
  Model state lives here.
- **CCMRAM (`.ccmram_bss`)** — currently ≈ 54,096 B (84.5% of 64 KB). Headroom ≈ 10 KB.
  Engine instances (the `_trackEngines[]` array) live here.

Per-track container gates:
- **Track container (model)** — 9544 B, set by `NoteTrack`. New TrackModes must fit under
  this; the `Track` union allocates this much regardless.
- **TrackEngine container (engine)** — 912 B, set by `TeletypeTrackEngine`. The engine pool
  allocates this much per track regardless. `StochasticTrackEngine` has a `static_assert(
  sizeof(StochasticTrackEngine) <= 512)` for stricter self-imposed bound.

Use STM32 release builds to measure actual sizes (`sizeof(YourTrack)`, `sizeof(YourEngine)`)
before relying on estimates. The simulator toolchain (host clang) can lay out structs
differently than `arm-none-eabi-gcc`.

---

## 10. Gotchas

- **Update reducer.** `_trackUpdateReducers[i]` (defined in Engine, 25 ms minimum interval)
  gates the in-tick-loop `trackEngine.update(0.f)` call for CV refreshes. The post-loop
  unconditional `updateTrackOutputs()` still runs at 1 kHz. Don't rely on `update()` being
  called every tick — it's reduced. Use `update(float dt)` for slide and similar smooth-time
  work; gate logic belongs in `tick()`.

- **High tempos can fire multiple ticks per update.** The `while (_clock.checkTick(&tick))`
  loop processes all pending master ticks each 1 ms cycle. At very high tempos this can be
  several ticks per loop iteration.

- **Cross-track reads see one-tick-old data when source > this index.** Engines tick in
  index 0→7 order. Reading a higher-index track's output gives last tick's value. Either
  enforce source < this index (most engines do this) or accept the one-tick lag.

- **`cvOutput(0)` is not the literal jack value.** It's the engine's logical pre-routing
  output. If the user's track uses pool rotation, the jack sees a different value. See §5.

- **The `int index` argument to `cvOutput`/`gateOutput` is mostly ignored.** Single-output
  engines (everything except MidiCvTrackEngine) return the same value regardless of `index`.
  Don't rely on the index parameter to disambiguate.

- **`MidiCvTrackEngine` is the only polyphonic engine.** Its `cvOutput(slot)` and
  `gateOutput(slot)` return per-voice values. For all other engine types, `index` 0 and
  `index` 5 return the same thing.

- **STM32 release is the source of truth for compilation.** The sim build uses host clang
  and tolerates STM32-incompatible code. Always cross-compile with `cd build/stm32/release
  && make sequencer` before declaring a change clean.

---

## 11. Where to put things

- New TrackMode model → `src/apps/sequencer/model/<Name>Track.h`, `<Name>Sequence.h`.
- New TrackMode engine → `src/apps/sequencer/engine/<Name>TrackEngine.{h,cpp}`.
- TrackMode enum entry + Container union → `src/apps/sequencer/model/Track.h`.
- Engine creation switch → `src/apps/sequencer/engine/Engine.cpp` and the `TrackEngineContainer`
  typedef in `Engine.h`.
- Routing target additions → `src/apps/sequencer/model/Routing.h` and `Routing.cpp` (enum,
  sentinels, `targetInfos[]`, and per-track `writeRouted()`).
- List UI model → `src/apps/sequencer/ui/model/<Name>TrackListModel.h`. TrackPage routes to it
  automatically once added.
- Visual page (post-MVP) → `src/apps/sequencer/ui/pages/<Name>Page.{h,cpp}`, registered in
  `ui/Pages.h` and routed from `ui/pages/TopPage.cpp`.
- Unit tests → `src/tests/unit/sequencer/Test<Name>*.cpp`, registered in that directory's
  `CMakeLists.txt` via `register_sequencer_test(...)`.
- Python integration tests (engine-level behavior) → `src/apps/sequencer/tests/ui/`,
  exercised via the `testsim` pybind11 module.

---

## 12. UI control vocabulary

The hardware has 16 step buttons, 5 function keys (F0–F4), Page, Shift, Left, Right, Track 0–7, Play, Tempo, Pattern, Performer, and a clickable encoder. Edit pages combine these primitives via a small set of recurring patterns. New pages should reuse these patterns instead of inventing new combos.

### 12.1 Input primitives

All defined in `src/apps/sequencer/ui/Key.h`:

| Primitive | Detect | Notes |
|---|---|---|
| Step (0–15) | `key.isStep() && !key.pageModifier()` | `key.step()` returns 0–15 |
| Function (F0–F4) | `key.isFunction()` | `key.function()` returns 0–4; footer renders as F1–F5 |
| Shift held | `key.shiftModifier()` | modifier flag on any key |
| Page held | `key.pageModifier()` | modifier flag on any key |
| Page + Step (8–15) | `key.isQuickEdit()`, `key.quickEdit()` returns 0–7 | 8-slot quick-edit bank |
| Page + Step (0–7) / Page + Track | `key.isPageSelect()` | navigation to other pages |
| Page + Step (specific) | `key.pageModifier() && key.is(Key::StepN)` | sub-context shortcuts (Indexed/DiscreteMap use Step4/5/6/14) |
| Page + Shift | `key.isContextMenu()` | universal context menu trigger |
| Left / Right | `key.isLeft()`, `key.isRight()` | section nav, bank scroll |
| Encoder turn | `EncoderEvent::value()` | continuous edit |
| Encoder pressed (while turning) | `EncoderEvent::pressed()` | usually passed as `shift` flag → coarse step |

KeyPressEvent additionally carries `event.count()` — `count() == 2` is a double-press of the same key.

### 12.2 Common page patterns

Patterns recur across the codebase. Pick the closest match and follow its convention rather than inventing a new one.

#### Pattern A — Params console (list of slots × pages)

Used by `TuesdayEditPage.cpp:64-106` and `IndexedSequenceEditPage`. Four parameter columns (51 px each) under F1–F4, value + bar per slot, F5 cycles pages. Status box at the right edge. Encoder edits whichever slot is selected by the last F-key press. Shift + F5 commonly does a related secondary action (Tuesday: reseed; Indexed: navigate to route config — `TuesdayEditPage.cpp:267-276`, `IndexedSequenceEditPage.cpp:631-642`).

Step buttons are repurposed for *track-global* params (octave/transpose/root/divisor) split into top-row +1 / bottom-row −1 nudges — see `TuesdayEditPage.cpp:704-790`.

#### Pattern B — Step-tab + sub-page edit

Used by the legacy `StochasticSequenceEditPage.cpp`. Step buttons select a per-step target; F-keys pick a sub-axis (TIX / DROT / MROT for pitch, DUR / RST for duration); encoder edits the selected axis on the selected step(s); F5 NEXT cycles between sub-pages. Selection mask is a `uint32_t` set/cleared by step press, persistent in a Shift-toggled persist mode. Context menu provides INIT / EVEN / RAND.

#### Pattern C — Multi-select with shift-double = "match all"

Used by `DiscreteMapSequencePage.cpp:570-600`. Holding a step adds to a selection mask. Double-pressing a step does a per-page-specific quick action (auto-place threshold midpoint — `DiscreteMapSequencePage.cpp:603-640`; toggle gate min — `IndexedSequenceEditPage.cpp:582-603`). Shift + double-press selects all entries matching the source's value field — useful for editing groups of identical thresholds at once.

Page + Step (specific positions) opens sub-context menus for advanced operations (distribute / cluster / transform): `DiscreteMapSequencePage.cpp:642-664`.

#### Pattern D — Generator / preview workflow

`GeneratorPage.cpp:370-511`. F0 toggles A/B preview; F4 triggers re-roll; Shift + Step randomizes generator params; pressing two step buttons in steps 0–7 sets a value-range (`GeneratorPage.cpp:476-496`). Page + double-press opens the context menu in "double-click" alt mode — used by the page to surface a commit-on-doubleclick variant. Context menu items can launch a `QuickEdit` modal sub-editor for a single param: `_manager.pages().quickEdit.show(model, slot)` (`GeneratorPage.cpp:927-931`).

#### Pattern E — Teletype-style "view-only" with external editor

`TeletypeEditPage.cpp` is intentionally empty (one stub `keyPress` that swallows pageModifier). Teletype scripts are too large for hardware editing so the page delegates to `TeletypeScriptViewPage` / `TeletypePatternViewPage`. Use this pattern when the data shape doesn't fit a 16-step grid; provide a viewer with cursor nav and route real editing to an external host.

#### Pattern F — Live status box

Reusable component (not a class — copy the draw pattern). 48 × 30 px box at `x=204`, drawn by `TuesdayEditPage.cpp:570-630`. Shows live engine state (current note name, gate indicator square, CV voltage, step counter). Used to give the page a "what's playing right now" readout without consuming step or F-key bandwidth.

### 12.3 LED conventions

Step LEDs are bi-color (top = green, bottom = red, each independent on/off — 4 states per LED via `leds.set(index, redOn, greenOn)`).

- Top color (green) = positive direction / up / active / selected.
- Bottom color (red) = negative direction / down / disabled.
- Both = orange (high attention).
- Off = inactive.

Page modifier held → page may *temporarily override* LED state by `leds.unmask(index) ; leds.set(...) ; leds.mask(index)`. Pattern used in `TuesdayEditPage.cpp:158-185` to show QuickEdit slot availability while Page is held. This is the right way to surface "secondary mode" LED hints without polluting normal LED state.

### 12.4 Reusable widgets

- **`showContextMenu(ContextMenu(items, count, onSelect, isEnabled, doubleClick=false))`** — universal modal. Items are `ContextMenuModel::Item[]`. `doubleClick=true` flags an alt variant (typically COMMIT-on-doubleclick). Trigger via `isContextMenu()` or `pageModifier() && count()==2`.
- **`StepSelection<N>`** — multi-select helper. Steps keypress goes through `_stepSelection.keyDown/keyPress(event, stepOffset())`. Selection persists; query via `_stepSelection.selected(i)` / `count()`.
- **`_manager.pages().quickEdit.show(model, slot)`** — single-param modal sub-editor. Bind a model that exposes one editable cell and a label; useful for "deep" sub-params that don't deserve a full page.
- **`showMessage("TEXT")`** — toast banner across the bottom for action confirmations.

### 12.5 What not to invent

- **No new modifier keys.** Use Shift, Page, Page+Shift, Page+Step. Anything else fights muscle memory.
- **No step-button "group ranges"** (e.g. "steps 1–5 = density"). One param per step button is the established idiom — color/brightness encodes domain.
- **No new "tab" UI** — F-keys are the tab mechanism. If you need more tabs than F1–F4, use F5 NEXT to cycle sub-pages.
- **No persistent step assignments without an LED tell.** If pressing step N edits param X, step N's LED must light when that page is active so the user can see the map.
- **No silently-magic step double-press.** If `count()==2` does something distinct from `count()==1`, the action must be confirmable (toast or visible state change) so users learn the binding.

---

## 13. Reading order for a new agent

If you're new to this codebase and about to design a new TrackMode:

1. Read this document.
2. Read `AGENTS.md`, `PROJECT.md`, `CLAUDE.md`, `STM32.md`.
3. Read `TrackEngine.h` and `Engine.cpp` (the loop in §3).
4. Pick the timing pattern closest to your need from §7. Read that engine fully.
5. Read one existing TrackMode end-to-end (Note is the cleanest, Curve is the most
   continuous, Indexed is the most variable-step-length).
6. Read `Track.h` to understand the Container union and how `trackMode()` dispatches.
7. Only then start writing the model header.

The cheapest way to get something wrong is to invent a new timing model. The cheapest way to
get something right is to copy the closest existing one.
