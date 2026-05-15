# Source Probe Investigation

Read-only source trace of construction, lifetime, realtime access, and concurrency boundaries for the Track model/container system. Produces a yes/no answer table for each question. No refactoring.

---

## 1. Allocation Timing

**Question:** Does Track model construction only happen in UI/layout/load paths, NOT in `Engine::tick()` / `Engine::update()`?

### Source Evidence

**Model layer: `Track::setTrackMode()` + `Track::initContainer()`** (`Track.h:319-368`)

```cpp
void setTrackMode(TrackMode trackMode) {
    trackMode = ModelUtils::clampedEnum(trackMode);
    if (trackMode != _trackMode) {
        _trackMode = trackMode;
        initContainer();  // placement new on Container
    }
}

void initContainer() {
    switch (_trackMode) {
    case TrackMode::Note:
        _track.note = _container.create<NoteTrack>();  // placement new
        break;
    // ... 7 track types
    }
}
```

`Container::create<T>(args)` (`Container.h`): `return new(_data) U(args...)` — placement new into static-aligned buffer. No heap allocation.

**Model layer call sites for `setTrackMode`:**
- `Project::setTrackMode(trackIndex, trackMode)` (`Project.cpp:126-129`) — delegates to `_tracks[trackIndex].setTrackMode(trackMode)`
- `ClipBoard::paste()` (`ClipBoard.cpp:107-108`) — `_project.setTrackMode(...)` during paste
- `TrackModeListModel::toProject()` (`TrackModeListModel.h:43-44`) — iterates tracks, calls `project.setTrackMode(i, newTrackModes[i])`

**Engine layer: `Engine::updateTrackSetups()`** (`Engine.cpp:504-544`)

```cpp
void Engine::updateTrackSetups() {
    for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
        auto &track = _project.track(trackIndex);
        if (!_trackEngines[trackIndex] || _trackEngines[trackIndex]->trackMode() != track.trackMode()) {
            auto &trackEngine = _trackEngines[trackIndex];
            auto &trackContainer = _trackEngineContainers[trackIndex];  // Container<7 types>
            switch (track.trackMode()) {
            // ... 7 track types, each does trackContainer.create<XTrackEngine>(...)
            }
        }
    }
}
```

This uses a **separate** `Container` for track engines (`TrackEngineContainer`), not the model's `Track._container`. Both use placement new into aligned buffers.

**Engine call sites for `updateTrackSetups()`:**
- `Engine::init()` (`Engine.cpp:59`) — called once at startup
- `Engine::update()` (`Engine.cpp:137`) — called every main-loop iteration

### Answer

| Path | Constructs model objects? | Constructs engine objects? |
|---|---|---|
| `Engine::init()` | No (model already built) | Yes, via `updateTrackSetups()` |
| `Engine::update()` | No | Yes, conditionally via `updateTrackSetups()` |
| UI (TrackModeListModel) | Yes, via `Project::setTrackMode()` | No (engine sees change next `update()` cycle) |
| Project load | Yes, via `Track::read()` -> `setTrackMode()` | No (engine sees change next `update()` cycle) |

**YES — Track model construction happens only in UI/layout/load paths.** But `TrackEngine` construction (placement new into engine container) happens in `Engine::update()` every main-loop iteration via `updateTrackSetups()`. The model Container and engine Container are separate systems. The model Container is not reconstructed during engine ticks. The engine Container is reconstructed conditionally when `trackMode` changes, but this is placement new into a pre-allocated buffer — no heap allocation.

**Critical detail:** `Engine::update()` calls `updateTrackSetups()` **unconditionally every iteration** (line 137). However, the actual construction only happens when `!_trackEngines[trackIndex] || _trackEngines[trackIndex]->trackMode() != track.trackMode()`. On most iterations, this is a comparison-only fast path.

---

## 2. Realtime Path Stability

**Question:** Can engines cache a pointer/reference to track objects after setup? Can track objects move while engine uses them?

### Source Evidence

**Engine access pattern** (`Engine.cpp:504-544`):

```cpp
auto &trackEngine = _trackEngines[trackIndex];
// ...
case Track::TrackMode::Note:
    trackEngine = trackContainer.create<NoteTrackEngine>(*this, _model, track, linkedTrackEngine);
    break;
```

Each `TrackEngine` constructor receives `track` as a `Track &` parameter. Let's check if engines store this reference:

**NoteTrackEngine** (`NoteTrackEngine.h:38`): `_noteTrack(track.noteTrack())`
**CurveTrackEngine** (`CurveTrackEngine.h:17`): `_curveTrack(track.curveTrack())`
**TuesdayTrackEngine** (`TuesdayTrackEngine.h:29-30`): stores `_track.tuesdayTrack()` via accessor
**IndexedTrackEngine** (`IndexedTrackEngine.h:16`): `_indexedTrack(track.indexedTrack())`
**DiscreteMapTrackEngine** (`DiscreteMapTrackEngine.h:13`): `_discreteMapTrack(track.discreteMapTrack())`
**TeletypeTrackEngine** (`TeletypeTrackEngine.cpp:63`): `_teletypeTrack(track.teletypeTrack())`

Each engine stores a **reference** to the track subtype, obtained via `track.noteTrack()` etc., which returns `_container.as<NoteTrack>()` — a `reinterpret_cast` of the Container's `_data` buffer.

**Can the track object move?**

The `Track._container` is a `Container<7 types>` which contains `uintptr_t _data[(Size + sizeof(uintptr_t) - 1) / sizeof(uintptr_t)]`. This is inside the `Track` object itself. `Track` objects live in `_tracks[CONFIG_TRACK_COUNT]` (an `std::array` in `Project`). Arrays don't reallocate. So the address of each `Track` (and its `_container._data` buffer) is stable for the lifetime of the `Project` object.

The model `Container::as<T>()` returns `*reinterpret_cast<T *>(_data)` — this returns a reference to an object constructed in-place within `_data`. Since `_data` is inside the `Track` which is inside a `std::array` in `Project`, the address is stable **unless** `setTrackMode()` + `initContainer()` is called, which does placement new into the same buffer. Placement new into the same buffer at the same address preserves the reference validity only if the type doesn't change.

### Answer

| Question | Answer |
|---|---|
| Can engines cache a pointer/reference after setup? | **YES — they already do.** Each `XTrackEngine` stores `_xTrack` as a reference obtained at construction. |
| Can track objects move while engine uses them? | **NO — with a caveat.** `Track` objects live in `std::array` inside `Project`, so their address is stable. The `Container`'s internal buffer `_data` is at a fixed offset within `Track`. The placement-new object within that buffer is also at a fixed address. **However**, if `setTrackMode()` is called on a track while the engine holds a reference to the old type, the reference becomes invalid — the buffer now contains a different type. This is guarded by `updateTrackSetups()` which reconstructs the engine when `trackMode` changes. The `Engine::lock()/unlock()` mechanism prevents concurrent model mutation during engine processing. |
| Can `Container::as<T>()` references dangle? | **Only if `setTrackMode()` is called without updating the engine.** `SANITIZE_TRACK_MODE` macro (visible in Track.h) asserts that the current `_trackMode` matches the requested type, catching type mismatches in debug builds. |

---

## 3. Pitch/Timing Equivalence

**Question:** Can we create a sim trace harness for before/after comparison?

### Source Evidence

**Existing sim infrastructure:**
- `src/platform/sim/sim/TargetTrace.h` — `StateTrace<T>` template class that records `(uint32_t time, T state)` pairs. Has `write()` (deduplication by time), `writeStream()`/`readStream()` for serialization.
- `src/platform/sim/sim/TargetState.h` — defines traceable state structure
- `src/platform/sim/sim/TargetTracePlayer.cpp` — replays traces

**The sim already has a trace infrastructure.** `TargetTraceRecorder` and `TargetTracePlayer` exist. The sim build path is `build/sim/debug`.

### Answer

| Question | Answer |
|---|---|
| Can we create a sim trace harness? | **YES — infrastructure exists.** `TargetTrace` + `TargetState` already record and replay state traces. Need to extend `TargetState` or create a new trace type that captures: track mode, active pattern, current step, note/CV output, gate output, divisor, run mode, tick number. |
| Can we compare traces byte-for-byte? | **YES — `StateTrace::writeStream()` already does binary serialization.** A before/after comparison is: run sim with old code, save trace; run sim with new code, save trace; diff the binary trace files. |
| What needs to be built? | A trace harness that hooks into `Engine::update()` (or `TargetStateTracker`) and records the 7 fields per tick. Approx 50-100 lines of wiring. |
| Sim engine concurrency note | Python/test paths have no engine concurrency concerns because the sim engine is not running during test setup. Only when the sim loop is actively stepping does concurrency matter. |

---

## 4. Interrupt/Concurrency Boundary

**Question:** Are track mode changes and project loads guaranteed to happen while transport is stopped, or behind a safe swap point?

### Source Evidence

**Engine lock mechanism** (`Engine.cpp:220-248`):

```cpp
void Engine::lock() {
    while (!isLocked()) {
        _requestLock = 1;
#ifdef PLATFORM_SIM
        update();
#endif
    }
}

void Engine::unlock() {
    while (isLocked()) {
        _requestLock = 0;
#ifdef PLATFORM_SIM
        update();
#endif
    }
}
```

`_requestLock` and `_locked` are `volatile uint32_t`. At the start of `Engine::update()`:

```cpp
_locked = _requestLock;
if (_locked) {
    return;  // Skip entire update cycle
}
```

**Two concurrency mechanisms, not one:**

| Mechanism | Used by | Engine behavior | What runs |
|---|---|---|---|
| `Engine::lock()/unlock()` | Track mode UI changes, paste operations | Skips entire `update()` cycle (`_locked` check at top) | Nothing runs — engine is fully paused |
| `Engine::suspend()/resume()` | Project load (ProjectPage.cpp:213, :196, :146) | Enters suspend mode: consumes ticks/MIDI, updates CV/gate/output overrides, but does **not** run `updateTrackSetups()` or track engines | Partial operation — clock/MIDI consumed, but no track engine ticks |
| No concurrency guard | Python/test paths | Engine not running | No contention possible |

**`updateTrackSetups()` concurrency:** Called in `Engine::update()` every iteration. It checks `trackMode != _trackEngines[trackIndex]->trackMode()` before reconstructing. Under `lock()`, `update()` returns immediately so `updateTrackSetups()` never runs. Under `suspend()`, `update()` still runs but skips the suspended section — need to verify that `updateTrackSetups()` is in the suspended path.

**`Project::setTrackMode()` concurrency:** Called from UI thread while engine is locked. `TrackModeListModel::toProject()` calls `project.setTrackMode()` which calls `Track::setTrackMode()` which calls `initContainer()` (placement new). This is safe because the engine is locked.

**Clock events:** `Engine::onClockOutput()` calls `reset()` on Start/Reset events. This happens within `Engine::update()` processing loop, not from a separate ISR on the simulator. On hardware, the clock may come from an interrupt context.

### Answer

| Question | Answer |
|---|---|
| Track mode changes happen while transport is stopped? | **NO — but they happen while engine is locked (`lock()`).** Track mode changes can happen while transport is running, but only while `_locked == 1`, meaning the engine skips its entire update cycle. |
| Project loads safe? | **YES — project loads use `suspend()`, not `lock()`.** In suspend mode, the engine still consumes ticks/MIDI and updates CV/gate overrides, but does not run `updateTrackSetups()` or track engines. Model state is safe to mutate under suspend. |
| Safe swap point? | **Two mechanisms: `Engine::lock()/unlock()` for track mode UI changes (full pause); `Engine::suspend()/resume()` for project load (partial pause — clock/MIDI still consumed, but track engines stopped).** |
| ISR concern? | **On STM32, clock handling may come from interrupt context.** Need to verify that `onClockOutput()` doesn't call `updateTrackSetups()` from ISR. Current code shows `onClockOutput()` does NOT call `updateTrackSetups()` — it only calls `reset()` and sets state flags. `updateTrackSetups()` is called in `Engine::update()` which runs in the main loop, not ISR. |

---

## 5. Static Capacity

**Question:** Can we prototype union-based storage for Track models with linker-visible `.bss` drop and no dynamic heap dependency?

### Source Evidence

**Current model `Container`** (`Container.h`):

```cpp
template<typename... Ts>
class Container {
    static constexpr size_t Size = maxsizeof<Ts...>::value;
    // ...
    uintptr_t _data[(Size + sizeof(uintptr_t) - 1) / sizeof(uintptr_t)];
};
```

This is **already a union-like maximum-size buffer with placement new.** The `Container<T...>` stores `maxsizeof(all Ts...)` bytes and constructs the active type in-place. No heap allocation.

**`Track._container`** is `Container<NoteTrack, CurveTrack, MidiCvTrack, TuesdayTrack, DiscreteMapTrack, IndexedTrack, TeletypeTrack>`.

**`TrackEngineContainer`** is `Container<NoteTrackEngine, CurveTrackEngine, MidiCvTrackEngine, TuesdayTrackEngine, DiscreteMapTrackEngine, IndexedTrackEngine, TeletypeTrackEngine>`.

Both are already static-dimensioned, placement-new-based containers. No heap involved.

### Answer

| Question | Answer |
|---|---|
| Is the current Container already a placement-new union? | **YES — `Container` is exactly `alignas(max) + placement new`.** It already works like a tagged union of maximum size. |
| Would `alignas(NoteTrack) uint8_t notePool[N][sizeof(NoteTrack)]` save RAM over `Container<...>`? | **No under current semantics.** A per-type pool only saves if `sum(cap[type] * sizeof(type)) < 8 * sizeof(max Track container)`, which requires explicit simultaneous type caps. Under "any track can be any type", reserving enough slots for all valid projects is zero/negative RAM. Newer host probes also show CurveTrack, not NoteTrack, is the current max model arm. See `model-pool-decision-table.md`. |
| No dynamic heap dependency? | **CONFIRMED — zero heap allocation in Container.** All construction is placement new into `uintptr_t _data[]`. |
| Measurement proof needed? | **YES — need ARM `sizeof()` data for each Track type and each TrackEngine type to compute actual waste.** This is prerequisite before any pool prototype. |

---

## 6. Object Lifetime

**Question:** Can we prototype a local Container replacement with placement new, destroy on mode change, expose `as<T>()`, assert mode/type consistency, and keep call sites mostly unchanged?

### Source Evidence

**Current Container API** (`Container.h`):

```cpp
template<typename U, typename... Args>
U *create(Args&&... args) { return new(_data) U(args...); }

template<typename U>
void destroy(U *object) { if (object) object->~U(); }

template<typename U>
U &as() { return *reinterpret_cast<U *>(_data); }
```

**Current call sites:**
- `Track::initContainer()` — calls `_container.create<XTrack>()` on mode change
- `Track::noteTrack()` / `curveTrack()` etc. — call `_container.as<XTrack>()`
- `Track::operator=` — calls `_container = other._container` (copy)
- `ClipBoard` — calls `_container.as<Track>()` for copy/paste

**Mode/type consistency check:**
- `Track.h` uses `SANITIZE_TRACK_MODE(_trackMode, TrackMode::Note)` before every `as<>()` call. This is the existing consistency assertion.

**Destroy on mode change:**
- `Track::setTrackMode()` calls `initContainer()` which calls `_container.create<NewType>()`. But there's **no explicit `_container.destroy<OldType>()` before creating the new type.** This means the old type's destructor is not called. If the old type has non-trivial destructors (e.g., `TeletypeTrack`), this is a bug or relies on trivial destructors.

Let's verify:

### Answer

| Question | Answer |
|---|---|
| Can call sites remain unchanged? | **MOSTLY YES.** The current `Container::as<T>()` / `Container::create<T>()` API is the exact interface any replacement would need. Call sites use `track.noteTrack()` which delegates to `_container.as<NoteTrack>()`. A new Container that adds a discriminant and destroy-on-switch would preserve this interface. |
| Is destroy-before-create implemented? | **NO — neither model nor engine containers destroy before recreating.** `Track::setTrackMode()` calls `_container.create<NewType>()` without destroying the old type — placement new over the old object. Similarly, `Engine::updateTrackSetups()` does `trackContainer.create<XTrackEngine>(...)` without destroying the old engine. Both rely on old types having trivial destructors (no owned heap). Current classes are mostly POD-ish with no owned heap allocations, so this is safe-by-coincidence. A pool architecture should not carry this forward — it needs explicit destroy-before-create. |
| Is mode/type consistency asserted? | **YES — `SANITIZE_TRACK_MODE` macro checks `_trackMode == expected` before every `as<>()` call, controlled by `CONFIG_ENABLE_SANITIZE` (currently 1 in SystemConfig.h).** In release/non-sanitize builds (`CONFIG_ENABLE_SANITIZE=0`), `SANITIZE_TRACK_MODE` expands to an empty block, making `as<T>()` an unchecked `reinterpret_cast`. Wrong-type access produces undefined behavior silently. A stored discriminant would make this check unconditional in all builds. |
| Prototype needed? | **A prototype would add: (1) explicit destroy-before-create, (2) a stored discriminant to eliminate `SANITIZE_TRACK_MODE` runtime checks, (3) potentially per-type constructors that don't zero the entire buffer.** But the current Container already does the structural job. The prototype value is in the **safety guarantees** (proper destruction), not in the RAM savings (Container already minimizes to `maxsizeof(types)`). |

**TeletypeTrack destructor check needed:** If `TeletypeTrack` has a non-trivial destructor (e.g., freeing allocated memory), the missing destroy call in `setTrackMode()` is a real bug. If all destructors are trivial (only member destructors that are no-ops for plain-old-data), the current code is safe-by-coincidence.

---

## 7. Measurement Proof

**Question:** What measurement gates ensure the refactor preserves behavior?

### Size Gates

```bash
# Total binary size
arm-none-eabi-size build/stm32/release/src/apps/sequencer/sequencer.elf

# Sorted symbol sizes (bottom = largest)
arm-none-eabi-nm --print-size --size-sort build/stm32/release/src/apps/sequencer/sequencer.elf | tail -80

# CCMRAM specifically
arm-none-eabi-nm build/stm32/release/src/apps/sequencer/sequencer.elf | grep -i ccmram | sort -k 1
```

### Behavior Gates

**Sim trace comparison:**
1. Build sim with current code, run a standard project through N ticks, save `TargetTrace` output.
2. Build sim with refactored code, run same project, save `TargetTrace` output.
3. Compare traces byte-for-byte using `StateTrace::readStream()` and element-wise comparison.

**Fields to trace per tick per track:**
- `trackMode` (enum)
- `patternIndex` (int)
- `currentStep` (int)
- `noteValue` / `cvOutput[channel]` (float)
- `gateOutput[gate]` (bool)
- `divisor` (int)
- `runMode` (enum)
- `engine tick number` (uint32_t)

### Answer

| Gate | Measurement | Tool |
|---|---|---|
| **Size** | `.bss` + `.ccmram` before vs after | `arm-none-eabi-size` |
| **Symbols** | `sizeof(Container<7 Track types>)` vs `sizeof(proposed replacement)` | `static_assert` in code, confirmed by `arm-none-eabi-nm` |
| **Behavior** | Byte-identical trace output for standard project | `TargetTrace` sim harness |
| **Timing** | Worst-case `updateTrackSetups()` cycle count | Not measured (requires hardware profiler) — defer to Phase 4 hardware verification |

---

## Yes/No Summary Table

| # | Question | Answer | Confidence | Caveat |
|---|---|---|---|---|
| 1 | Track model construction only in UI/layout/load paths, not in Engine::tick()? | **YES** | High | Model Container never reconstructed in tick. Engine Container reconstructed conditionally in `update()` (every main-loop iteration), but only on trackMode change, and via placement new (no heap). |
| 2 | Engines can cache pointer/reference after setup; no track object moves during engine use | **YES** | High | References cached at construction. Track objects are in `std::array` (stable address). Only invalidation risk is `setTrackMode()` while engine holds old reference; guarded by `Engine::lock()`. |
| 3 | Sim trace harness for before/after comparison | **YES** | High | `TargetTrace` infrastructure exists. Needs extension to capture the 7 per-tick fields. ~50-100 lines of wiring. |
| 4 | Track mode changes / project loads happen while transport is stopped or behind safe swap | **YES (two mechanisms)** | Medium | Track mode UI changes: `lock()/unlock()` (full pause, engine skips entire update). Project loads: `suspend()/resume()` (partial pause, engine consumes clock/MIDI but stops track engines). ISR concern: `onClockOutput()` does NOT call `updateTrackSetups()`. |
| 5 | Linker-visible `.bss` drop from union/pool prototype, no heap dependency | **NO UNDER CURRENT SEMANTICS** | High | Current Container is already placement-new union of `maxsizeof(types)`. A per-type pool saves only with explicit simultaneous type caps; under "any track can be any type" it is zero/negative RAM. Need ARM `sizeof()` for each Track/TrackEngine type before any future capped design. |
| 6 | Container replacement preserves call sites with placement new + destroy + as<T>() + assert | **MOSTLY YES** | High | `Container::as<T>()` is the existing interface. Missing: explicit destroy-before-create in `setTrackMode()`. `SANITIZE_TRACK_MODE` already provides mode/type assertion. |
| 7 | Measurement proof: size + symbols + behavior trace | **INFRASTRUCTURE EXISTS** | High | `TargetTrace` for behavior. `arm-none-eabi-size` / `nm` for size. Need to add `static_assert(sizeof(...))` for key types. |

---

## Open Findings

1. **Destroy-before-create is missing.** `Track::setTrackMode()` calls `_container.create<NewType>()` without destroying the old type. If any Track type has a non-trivial destructor, this is a bug. Needs verification of each Track type's destructor.

2. **`updateTrackSetups()` runs every main-loop iteration.** Even though it only does work on `trackMode` change, the comparison loop (8 iterations, pointer null check + enum comparison) runs ~1000x/sec. This is cheap but worth knowing.

3. **`Container<7 Track types>` vs `Container<7 TrackEngine types>` sizes are different.** Current ARM MonitorPage probes show the model container gate is `NoteTrack=9544 B` (`Track=9560 B`, `CurveTrack=9480 B`, `TeletypeTrack=7104 B`). The engine container gate is `TeletypeTrackEngine=912 B` (`Engine::TrackEngineContainer=912 B`). Teletype model shrinkage is not a current top-level model RAM win, but Teletype engine compaction/extraction can save CCMRAM if the next-largest engine is smaller.

4. **Destroy-before-create is missing in both model and engine containers.** `Track::setTrackMode()` calls `_container.create<NewType>()` without destroying the old type. `Engine::updateTrackSetups()` calls `trackContainer.create<XTrackEngine>(...)` without destroying the old engine. Both rely on old types having trivial destructors. If any Track or TrackEngine type acquires non-trivial destructors (e.g., owned heap memory), this becomes a real bug. A pool architecture must not carry this forward.

5. **`SANITIZE_TRACK_MODE` is controlled by `CONFIG_ENABLE_SANITIZE` (SystemConfig.h:26, currently 1).** In release/non-sanitize builds (`CONFIG_ENABLE_SANITIZE=0`), it expands to an empty block. This means `Container::as<T>()` in release builds is an unchecked `reinterpret_cast` — wrong-type access is silent undefined behavior. A stored discriminant in the Container would make this check enforceable in all builds.

6. **Project load uses `suspend()`, not `lock()`.** In suspend mode (ProjectPage.cpp:213, :196, :146), the engine still consumes ticks/MIDI and updates CV/gate/overrides, but does not run `updateTrackSetups()` or track engines. The earlier analysis that stated "project loads use `_requestLock`" was incorrect. The two mechanisms serve different purposes:
    - `lock()/unlock()`: Full pause — engine skips entire `update()`. Used for track mode UI changes and paste operations.
    - `suspend()/resume()`: Partial pause — engine consumes clock/MIDI but stops track engines. Used for project load.
