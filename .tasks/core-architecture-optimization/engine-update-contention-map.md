# Engine update contention map

_Surveyed 2026-05-29. Research artifact — no direction chosen. Maps what runs inside `Engine::update()` (`engine/Engine.cpp:65-214`), how many times each subsystem runs, the data dependencies between them, and where passes are redundant or mis-ordered. Many subsystems compete for the same update: routing, CV router (overrides), track-output composition, track engines, modulators, MIDI out._

## Pass order in `Engine::update()`

Per call (one RTOS tick of the engine task), in order:

1. lock / suspend gates (`:67-100`). Suspended path runs its own mini-pipeline: `_cvInput.update` → `updateOverrides` → cv/gate output (`:95-98`) — a duplicate of the tail.
2. `updateTrackSetups()` (`:106`) — rebuild engine containers for mode-changed tracks.
3. `updateBusSafetyMode()` + clear `_busCvWritten` (`:108-109`).
4. clock events (Start/Stop/Continue/Reset → `reset()`) (`:112-133`).
5. tempo + clock setup + `updatePlayState(false)` (`:136-143`).
6. `_cvInput.update()` (`:146`).
7. `receiveMidi()`, `receiveKeyboard()` (`:149-152`).
8. **`_routingEngine.update()`** (`:155`) — routing pass #1.
9. **per queued clock tick** (`while checkTick`, `:158-199`):
   - `updatePlayState(true)`.
   - **per track** (`:165-183`): `trackEngine.tick(tick)`; if `CvUpdate` **and** the track's `UpdateReducer<25ms>` fires → `trackEngine.update(0)` + **`updateTrackOutputs()`** + **`updateOverrides()`** + **`_routingEngine.update()`**.
   - `_midiOutputEngine.update(true)` on tick 0.
   - **per modulator** (`:190-198`): `_modulatorEngine.tick(...)` + `sendModulator`.
10. all tracks `trackEngine.update(dt)` (`:201-203`).
11. `_midiOutputEngine.update()` (`:205`).
12. `updateTrackOutputs()` + `updateOverrides()` + `applyBusSafety()` (`:207-209`).
13. `_cvOutput.update()` + `_gateOutput.update()` (`:212-213`).

## Run-count per `Engine::update()`

Let T = tracks firing `CvUpdate` (≤8), K = queued ticks this call.

| Subsystem | Runs | Where |
|-----------|------|-------|
| `_routingEngine.update()` | 1 + (T × K) | `:155` + per-track loop `:181` |
| `updateTrackOutputs()` | (T × K) + 1 | loop `:179` + tail `:207` |
| `updateOverrides()` | (T × K) + 1 (+1 suspended) | loop `:180` + tail `:208` |
| track `tick()` | 8 × K | `:172` |
| track `update(dt)` | 8 | `:202` |
| modulator `tick()` | CONFIG_MODULATOR_COUNT × K | `:196` |

## The contention

- **Global work driven by per-track events.** `updateTrackOutputs()` recomputes *all* tracks + all channels (gate/CV rotation pools, CV-route lanes — `Engine.cpp` `updateTrackOutputs`). So does `updateOverrides()` (CV router) and `_routingEngine.update()`. Yet all three are invoked **inside the per-track loop** (`:179-181`), once per firing track. T tracks updating in one tick ⇒ T full global recomputes of the same data. The `UpdateReducer<25ms>` per track caps frequency (~40 Hz) so the cost is bounded, but the structure is wrong: per-track triggers doing whole-engine recompute.
- **Ordering bug (the CV-router-stale class).** In the loop the order is `updateTrackOutputs()` → `updateOverrides()` → `_routingEngine.update()`. `updateOverrides()` recomputes `_cvRouteOutputs` (CV router); `updateTrackOutputs()` consumes them — but runs *first*, so it uses the previous iteration's routing. One-update-stale CV-route physical outputs. (Tracked in `stability-fixes`.)
- **Modulator → routing reverse dependency.** Modulators tick (`:190`) *after* all routing passes in the tick. Modulator outputs are `Routing::Source` inputs, so routing reads last-tick modulator values — modulator-as-source is one tick stale.
- **Duplicate suspended pipeline** (`:95-98`) re-implements a slice of the tail (`:207-213`).

## Optimization candidates (deferred — not chosen)

- **Hoist the three global passes out of the per-track loop** → run `updateTrackOutputs`/`updateOverrides`/`_routingEngine.update` **once after** the track loop per tick, not per firing track. Removes the T× redundancy. **Caution:** verify no intra-loop dependency — a track that links another (`linkedTrackEngine`, see IO/linking map) may read the linked track's freshly-composed output mid-loop; that coupling is the likely reason the recompute is per-track today. Must be confirmed before hoisting.
- **Fix loop ordering**: `_routingEngine.update()` / `updateOverrides()` (CV router) **before** `updateTrackOutputs()`, so physical outputs use current-tick routing. Resolves the CV-router-stale stability item at the structural level.
- **Resolve modulator/routing order**: tick modulators before routing, or accept and document the 1-tick latency.
- **Unify the suspended mini-pipeline** with the tail to avoid drift.

## Relation to other maps

- The CV-router-stale ordering bug is the same one in `stability-fixes` (CV Router output ordering) — this map is its structural context.
- `routing-five-sources-map.md` covers routing's *definition* fan-out; this covers routing's *execution* frequency/ordering.
- The hoisting caution depends on the track-linking coupling — see the planned IO/layout/linking map.
