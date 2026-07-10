# Refactoring notes

## Teletype UI (src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp)

- `isMini()` branch-and-dispatch is duplicated identically 3x: `commitLine()` (:825-826), `duplicateLine()` (:856-857), `deleteLine()` (:897-898) — `if (isMini()) X(track.tt2MiniTrack().program(scene), …) else X(track.tt2Track().program(), …)`. One helper returning the active `TT2Program` for the current mode collapses all three to one-liners.
- `keyboard()` is a 227-line flat if-chain (:969-1194); the same shape repeats as 274 lines in `TeletypePatternViewPage.cpp`. These are the graph's #1 and #2 god nodes by edge count in the teletype-scoped graph. Split each into named sub-handlers (function-key dispatch, ctrl-shortcuts, navigation).
- Home/End cursor-reset is duplicated identically inside the ctrl-shortcut `switch` (:1054-1061) and again as plain-key handling (:1169-1178) in `keyboard()`. Hoist above the ctrl branch instead of keeping two copies in sync.
- Forward-delete (`KeyDelete`) logic is inlined directly in `keyboard()` (:1100-1106) instead of following the file's own established pattern — every other buffer op (`backspace()`, `insertChar()`, `moveCursorLeft/Right()`) is its own named method.
- Context-menu step indices in `keyPress()` (:433-451: copy/paste/duplicate/comment/delete/undo mapped to steps 8-13) are bare magic numbers, unlike script indices which use named constants (`kTriggerScriptCount` etc.) elsewhere in the same file.

## Whole-app engine (src/apps/sequencer/engine)

- `TT2TrackEngine.cpp` and `TT2MiniTrackEngine.cpp` duplicate their entire `TT2Host` interface implementation: 31 matching `host*` methods each (`hostTempo`, `hostBarFraction`, `hostWms`, `hostNoteGateGet/Set`, `hostGeodeMix`, `hostGeodeTriggerVoice/All`, etc.), byte-identical apart from the class-name prefix. ~180 of ~350-390 lines in each file. `TT2ConfigFull`/`TT2ConfigMini` and `TeletypeProgramT<Cfg>` already establish the pattern to fix this with — one `TT2HostImpl<Cfg>` mixin both engines derive from, instead of two hand-maintained copies.

## Whole-app UI (src/apps/sequencer/ui/pages)

- `DiscreteMapSequencePage.cpp:97-142` and `IndexedSequenceEditPage.cpp:79-124` — the `Voicing` struct, `kPianoVoicings[]`, `kGuitarVoicings[]` tables, and their count constants are byte-for-byte identical. `applyVoicing()` itself differs correctly per-model (DiscreteMap stage-based vs Indexed step-based selection) and should stay separate — only the struct+tables belong in a shared header.

## Bugs (adjacent, found during the same pass)

- `TeletypeScriptViewPage::deleteLine()` (:879-902) discards the bool return of `deleteScriptCommand()` (`TT2ScriptLoader.h:109`) and unconditionally shows "Line deleted" — even on a no-op (out-of-range line). `duplicateLine()`/`commentLine()` guard this correctly with an explicit `_selectedLine >= script.length` check first; `deleteLine()` doesn't.

## Optimization (adjacent, found during the same pass)

- `commitLine()` (:783) parses `_editBuffer` once to validate/show an error, then the non-live commit path (:825-826) calls `setScriptCommand()` (`TT2ScriptLoader.h:79`), which independently re-parses and re-lowers the same text from scratch. Every line commit does the parse+lower work twice.

---

# Adversarial architecture review

Four independent reviewers attacked the engine core, routing matrix, TT2 subsystem, and model/UI seam. Every finding was verified against source by its reviewer; finding 1 was additionally confirmed first-hand. Severity and CONFIRMED/PLAUSIBLE noted per item.

## Critical

- **[CONFIRMED] Out-of-bounds write from a malformed user-scale file.** `UserScale::read` (`model/UserScale.cpp:43-47`) reads `_size` (a `uint8_t`) raw from the file and loops `for (i < _size) reader.read(_items[i])` into `_items`, a `std::array<int16_t,32>`. The `setSize()` clamp (`UserScale.h:77-78`) is bypassed — `read()` assigns `_size` directly. A file with `_size = 255` writes ~510 bytes into a 64-byte array; the `checkHash()` that would reject the file runs *after* the overflow (:49). Hits standalone `.SCA` loads (`FileManager::readUserScale` `FileManager.cpp:444-466`, which unlike `readProject` never checks `header.valid()`/`type`/`dataVersion`) and scales embedded in a project (`Project.cpp:539`, before the project hash at :545). Silent RAM corruption on device.

- **[CONFIRMED] Routed bus voltage double-counts writers on tick frames.** Bus lanes sum each writer per frame (`Engine.h:155-169`; `_busCvWritten` reset once per frame at `Engine.cpp:125-126`), but on any tick-frame where a track produces a CV update the routing pass runs a second time (`Engine.cpp:172` then `:284-292`) and adds the same route's contribution again. A route to BUS 1 at ±2.5V carries ~5V on ticking frames and 2.5V between — a clock-synced square-wave artifact seen by every bus reader (bus-sourced routes, CV router, DiscreteMap input). The sum-onto-lane design assumes one write per writer per frame.

## Major (audible)

- **[CONFIRMED] Gates stick HIGH on MIDI/slave stop.** `TrackEngine::stop()` (`TrackEngine.h:62`) is dead code — Stochastic/Fractal/PhaseFlux override it to drop gates, nothing calls it. `Clock::Stop` only does `setRunning(false)` (`Engine.cpp:136-139`). Front-panel stop is clean only because it routes through `reset()`. A MIDI stop / Pause-mode stop / Run-input drop between a gate-on and its gate-off freezes the physical gate HIGH until Continue.
- **[CONFIRMED] Scale/RootNote route at zero depth destroys "follow project scale".** `routedValue*` returns `clamp(base + delta, lo, hi)` whenever an override entry exists (`Routing.cpp:318-332`); the `-1` Default sentinel is crushed to 0 (Chromatic / root C) the instant any route targets it, even at rest. `NoteSequence.h:391-392,419-420` and the six sibling sequence types (`StochasticSequence.h:30`, `TuesdaySequence.h:322`, `PhaseFluxSequence.h:322`, `DiscreteMapSequence.h:200`, `IndexedSequence.h:323`, `FractalSequence.h:112`).
- **[CONFIRMED] Harmony follower result depends on master track index.** `applyHarmony` reads `masterNoteEngine.currentStep()` with no `source < this` index guard (`NoteTrackEngine.cpp:967-976`); engines tick in ascending index order (`Engine.cpp:184-191`). Master above the follower → follower harmonizes against the master's previous step. Violates the doc's own §10 gotcha.
- **[CONFIRMED] Pulse-hold reads the wrong step under rotate/fill.** Hold duration read from the unrotated index on the main sequence (`NoteTrackEngine.cpp:194-195`); the audible step is rotated and possibly the fill sequence (`:554-557`). With rotate=1, setting pulses on the heard step does nothing.
- **[CONFIRMED] Fractional clockMultiplier drifts ratioed tracks permanently.** Each engine independently does `max(1, lround(divisor4x / clockMult))` (`NoteTrackEngine.cpp:167-169` and every engine). At 70% with seq divisors 12 and 24 (intended 1:2), periods round to 69 and 137 ticks — not 2:1. With `resetMeasure=0` they never realign. Same-divisor same-mult tracks are safe.
- **[CONFIRMED] Mute/Fill/Pattern routes are silent no-ops; MIDI-mapped transport uncreatable.** `RouteResolve::targetToParamKey` returns `None` for PlayState targets (`RouteResolve.h:123`) → no table row → no-op. The legacy `Routing::writeTarget` path is unreachable, so `PlayState::writeRouted`/`Project::writeRouted` are dead code. The new UI only mints ParamKey targets (`RoutingPage.cpp:228-236`); keys for Mute/Fill/FillAmount/Pattern and Play/Record/TapTempo are reserved-but-unminted (`RouteParamKey.h:35,40`). Legacy projects with a Mute route load and do nothing; MIDI-mapped transport has no creation path.
- **[CONFIRMED] MOD+ "extend" mutates the live route before a draft exists; CANCEL doesn't revert.** `ListPage.cpp:396-411` calls `route.setTracks(...)` and `setDepthPct(..., isInletTarget ? 100 : 0)` on the live route, then begins a non-new draft; `RouteDraft::cancel` frees only freshly-created slots (`RouteDraft.h:65-69`). For inlet targets the new track is modulated at depth 100 the instant MOD+ is pressed and stays so after CANCEL.
- **[CONFIRMED] New MIDI route applies a stale source value until the first CC arrives.** `_sourceValues[slot]` is never cleared on slot reuse (`RoutingEngine.cpp:190-194,208-215`); a MIDI route in a slot that previously held a CV route inherits its last sampled value and applies it until the mapped control is physically moved.
- **[CONFIRMED] No per-tick TT2 execution budget.** The `L` loop op runs its body `start..end` times with no cap (`TT2Evaluator.h:413-478`); `L 1 32000: SCRIPT 1` in a Metro script blocks the engine task every metro period — clock ticks queue, all outputs freeze. Legal, parseable input.

## Minor / latent

- **[PLAUSIBLE] TT2 recursion caps multiply against the 4KB engine stack.** Same-track nesting caps at 8 (`TeletypeNativeOps.cpp:2754`), cross-track at 4 (`TT2TrackEngine.cpp:242-246`), but bound different counters; a 4-track cross-trigger cycle with per-track nesting can stack ~32 live `runScript` frames against `CONFIG_ENGINE_TASK_STACK_SIZE 4096` (`Config.h:22`). Exact frame size needs a build.
- **[CONFIRMED] `Model::ConfigLock` is a no-op** (`Model.h:19-26`) — empty ctor/dtor, zero mutual exclusion. `ClipBoard::pasteTrack` (`ClipBoard.cpp:127`) trusts it; only masked today because its sole caller wraps the call in `_engine.lock()` (`TrackPage.cpp:220-226`). A landmine for any future `pasteTrack` caller.
- **[CONFIRMED] Route conflict guard is dead code.** `Routing::checkRouteConflict` (`Routing.cpp:190-217`) has zero callers; the "at most one route per (key,track)" comment (`Routing.cpp:277-278`) is enforced only structurally by the UI. Python bindings or hand-built files can create overlapping routes; last-processed delta wins and re-targeting clears a `routedSet` bit the other route still needs.
- **[CONFIRMED] Cross-bus feedback loops permitted.** `isBusSelfRoute` blocks only same-index feedback (`Routing.h:556-559`); bus1→bus2 + bus2→bus1 at depth 100 latches at ±5V until manually broken.
- **[CONFIRMED] Several clock wrap-safety gaps.** `Clock::setMode` reads `_state`/`_activeSlave` without the interrupt lock (`Clock.cpp:34-44`) — a narrow OOB `_slaves[-1]` window vs the ISR watchdog. `UpdateReducer` compare is not wrap-safe (`UpdateReducer.h:12`; stalls once per ~49.7-day uptime). Slave sub-tick compare not wrap-safe (`Clock.cpp:334`; hiccup once per ~71.6 min continuous slaving). Clock-out pulse width uses `_masterBpm` while slaved (`Clock.cpp:438`; wrong width when master/slave BPM differ).
- **[CONFIRMED] `DEL.G` geometric interval overflows int32** (`TT2Evaluator.h:563-570`) — signed UB on the 3rd iteration; blast radius bounded by the queue's own clamps.

## Non-findings (attacked, came back clean)

- **TT2 engine copy-drift** — the 31 duplicated `host*` methods have NOT drifted; every bounds check and clamp matches. Duplication is the maintenance risk (see engine section above), not a live bug.
- **Track-mode-change use-after-free** — structurally prevented: mode changes only happen engine-locked from `TrackPage`/`LayoutPage`, and no mode-specific page is on the stack when its track flips. Guard coverage is asymmetric but unguarded pages re-fetch each draw; worst case is type-punned garbage, not a crash. `SANITIZE_TRACK_MODE` asserts are compiled in.
- **Project-load atomicity** — load runs under `_engine.suspend()`; `updateTrackSetups()` is ordered before clock processing on resume. Clean.
- **ClipBoard pattern type-confusion** — `pastePattern` guards each track by `trackMode` with `default: break`; TT2/TT2Mini correctly absent from the `Pattern` union.
- **TT2 edit-vs-execute race, shared `gTeletypeLoadScratch` buffer, cross-track pattern write, serializer bounds, snapshot vs pattern requests, CCM/DMA, project version skew** — all verified defended.

## Doc drift (act on this — cheap leverage)

- `docs/performer-architecture.md` §3 and §10 no longer match the tick loop they document: the doc describes per-track `_trackUpdateReducers` and per-tick `updateTrackOutputs()`; the code uses one global `UpdateReducer<25ms>` (`Engine.h:295`) that recomputes all engines, re-runs routing, and double-composes only on CV-update frames, ignoring `GateUpdate` results (`Engine.cpp:284-292,192-194`). Since CLAUDE.md requires briefs to route through this doc, the drift seeds wrong freshness assumptions into new engine work — and is directly implicated in the bus double-count finding above.
