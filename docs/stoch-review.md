# Stochastic Track Adversarial Review

Date: 2026-05-22

Scope: implemented stochastic track behavior across model, engine, and UI. This excludes missing artwork and pure placeholder ideas unless a placeholder is currently user-visible or affects runtime state.

## Verification

- Read live model/engine/UI implementation.
- Built STM32 release target from `build/stm32/release` with `make sequencer`.
- Build result: `.text=867268`, `.data=6332`, `.bss=112616`, `.ccmram_bss=55088`.
- Built ARM unit-test targets: `TestStochasticDurationDictionary`, `TestStochasticGenerator`, `TestStochasticL1Macros`, `TestStochasticSequenceSerialization`.
- Did not run those unit tests locally; they were built as ARM binaries, not host executables.

## Topology

- Persistent state lives in `StochasticTrack` and `StochasticSequence`.
- Runtime state lives in `StochasticTrackEngine`.
- Output timing lives in `StochasticTrackEngine::tick()` and `triggerStep()`.
- Generated loop source material lives in the model, not just the engine.
- Lock replay state lives in an engine heap buffer, not in the model.
- UI state is split between list models and `StochasticSequenceEditPage`.

## Findings

### 1. Engine Mutates Project Model State From the Engine Task

Severity: high.

`StochasticTrackEngine::triggerStep()` casts away constness for both sequence and track:

- `src/apps/sequencer/engine/StochasticTrackEngine.cpp:237`
- `src/apps/sequencer/engine/StochasticTrackEngine.cpp:238`

It then writes generated rhythm/melody events back into `StochasticSequence::events()` during playback:

- live rhythm writeback: `src/apps/sequencer/engine/StochasticTrackEngine.cpp:335-340`
- live melody writeback: `src/apps/sequencer/engine/StochasticTrackEngine.cpp:344-349`
- loop regeneration: `src/apps/sequencer/engine/StochasticTrackEngine.cpp:312-317`
- patience invalidation: `src/apps/sequencer/engine/StochasticTrackEngine.cpp:545-566`
- mutation/permutation: `src/apps/sequencer/engine/StochasticTrackEngine.cpp:501-523`

The UI reads and edits the same selected sequence directly:

- list model reads/edits selected pattern sequence: `src/apps/sequencer/ui/model/StochasticPerformanceListModel.h:126-140`
- edit page reads selected stochastic sequence in draw/input paths, for example `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.cpp:64-65`, `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.cpp:687-707`, `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.cpp:1504-1517`

Risk: engine task and UI task can race on packed 6-byte event records and validity flags. The writes are not protected by a model write lock in the stochastic path. This can produce torn loop-tape display data, lost UI edits, or inconsistent rhythm/melody validity.

Recommendation: either move generated/live event history to engine-owned runtime state, or introduce a clear locked write path for stochastic model mutations. The current const_cast boundary hides state ownership.

### 2. Lock Replay Buffer Is Engine-Local, Not Pattern-Scoped

Severity: high.

`changePattern()` only repoints `_sequence`:

- `src/apps/sequencer/engine/StochasticTrackEngine.h:33-35`

The locked replay cache is a single heap array keyed only by source index:

- allocation: `src/apps/sequencer/engine/StochasticTrackEngine.cpp:98-107`
- lock read: `src/apps/sequencer/engine/StochasticTrackEngine.cpp:260-307`
- lock write: `src/apps/sequencer/engine/StochasticTrackEngine.cpp:394-419`

Risk: with Lock active, changing project pattern can replay cached evaluated events captured from a previous pattern at the same read index. Queues and lock cache are not cleared on pattern change, and the cache has no pattern identity.

Recommendation: make lock cache pattern-scoped, clear it on `changePattern()`, or make pattern changes force a rebuild/reset of stochastic engine replay state.

### 3. Stochastic UI Pages Do Not Locally Guard Track Type

Severity: high.

The project rule says type-specific pages must guard every draw/LED/input/context callback or be replaced immediately after track selection changes.

TopPage does remap stochastic sequence/edit pages after normal track select:

- `src/apps/sequencer/ui/pages/TopPage.cpp:90-130`
- `src/apps/sequencer/ui/pages/TopPage.cpp:351-379`

But stochastic pages themselves call `selectedTrack().stochasticTrack()` without local guards:

- draw paths: `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.cpp:64-65`, `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.cpp:126-127`, `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.cpp:202-203`
- LED path: `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.cpp:1110-1112`
- input/edit paths: `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.cpp:1241-1247`, `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.cpp:1504-1508`, `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.cpp:1569-1571`

The list pages only set their model if the current track is stochastic:

- `src/apps/sequencer/ui/pages/StochasticConfigPage.cpp:9-14`
- `src/apps/sequencer/ui/pages/StochasticPerformancePage.cpp:38-43`

But their list models keep raw pointers and can still dereference stale state:

- `src/apps/sequencer/ui/model/StochasticPerformanceListModel.h:113-140`
- `src/apps/sequencer/ui/model/StochasticConfigListModel.h:414-428`

Risk: normal track-select seems covered. Project load, mode change outside TopPage remapping, or any future selected-track mutation path can leave a stale stochastic page alive and hit type-specific accessors.

Recommendation: add page-local `trackMode() == Stochastic` guards to draw/LED/input/context callbacks, or centralize page replacement for every selected-track/mode mutation path.

### 4. `PlayMode` Is Exposed But Not Implemented In Timing

Severity: medium.

`StochasticTrack` exposes and persists play mode:

- model accessors: `src/apps/sequencer/model/StochasticTrack.h:86-90`
- config UI row: `src/apps/sequencer/ui/model/StochasticConfigListModel.h:397-400`, `src/apps/sequencer/ui/model/StochasticConfigListModel.h:447-470`

`StochasticTrackEngine::tick()` computes divisor and reset measure, then advances by `_eventElapsed++` and `_eventDuration`:

- `src/apps/sequencer/engine/StochasticTrackEngine.cpp:175-204`

There is no branch on `track.playMode()`.

Risk: Aligned and Free are user-visible but behave the same. This is worse than a missing UI because it creates false timing expectations.

Recommendation: either remove/hide Play Mode for stochastic, or implement the documented Aligned/Free timing distinction using the same clock-position contract as other track modes.

### 5. Runtime Lock Allocation Has No User-Visible Failure Path

Severity: medium.

The lock buffer uses heap allocation:

- `src/apps/sequencer/engine/StochasticTrackEngine.cpp:98-107`

If allocation fails, `_lockedParents` stays null. Lock then silently behaves like unlocked playback because `lockActive` requires the pointer:

- `src/apps/sequencer/engine/StochasticTrackEngine.cpp:260`

Risk: this is a RAM-critical firmware. A failed allocation produces an apparently working UI control with no lock behavior and no diagnostic.

Recommendation: expose allocation failure through activity/status feedback, disable the lock control when unavailable, or allocate from a bounded project-owned pool with explicit capacity.

### 6. Track-Level Lock Is Not Serialized

Severity: low/medium.

`StochasticTrack::write()` serializes slide, octave, transpose, fill mode, CV mode, play mode, and sequences:

- `src/apps/sequencer/model/StochasticTrack.h:149-160`

`StochasticTrack::read()` restores the same set:

- `src/apps/sequencer/model/StochasticTrack.h:162-179`

`_lock` is cleared in `clear()` but is not written or read:

- `src/apps/sequencer/model/StochasticTrack.h:135-147`
- `src/apps/sequencer/model/StochasticTrack.h:191`

Risk: if Lock is meant as performance-only state, this is acceptable but should be explicit. If it is meant as a project state, saves lose it.

Recommendation: decide whether Lock is runtime-only. If runtime-only, label/document it as such. If persistent, serialize it.

### 7. Invalid Serialized PlayMode Defaults Differ From Clear Defaults

Severity: low.

Fresh tracks default to Aligned:

- `src/apps/sequencer/model/StochasticTrack.h:140-143`

Invalid serialized play mode defaults to Free:

- `src/apps/sequencer/model/StochasticTrack.h:172-175`

Risk: corrupted or future-incompatible values load differently than freshly initialized stochastic tracks.

Recommendation: use the same default in `read()` as in `clear()`, unless Free is intentionally the compatibility fallback.

## Positive Checks

- Model and engine have explicit size gates:
  - `StochasticSequence <= 560`: `src/apps/sequencer/model/StochasticSequence.h:855`
  - `StochasticTrack <= 9544`: `src/apps/sequencer/model/StochasticTrack.h:204`
  - `StochasticTrackEngine <= 512`: `src/apps/sequencer/engine/StochasticTrackEngine.h:186`
- STM32 release build passes with current stochastic implementation.
- Stochastic engine creation is wired into `Engine::updateTrackSetups()`:
  - `src/apps/sequencer/engine/Engine.cpp:517-559`
- Engine container destruction support exists for heap-owning engines:
  - `src/core/utils/Container.h:40-59`

## Bottom Line

The largest architectural issue is state ownership. Stochastic generation currently treats persistent model sequences as both saved project data and live engine scratch tape. That makes UI/engine races and pattern/lock identity bugs likely.

Fix order:

1. Define whether generated loop tape is model state or engine runtime state.
2. Fix lock cache pattern identity.
3. Add UI type guards or broader navigation replacement guarantees.
4. Either implement stochastic PlayMode or remove it from the UI.
5. Add feedback for lock allocation failure.
