# State Lifecycle Contract

Derived from `assumption-matrix.md`. Classifies every major state owner by lifecycle, marks which are safe to compact, which need documentation, and which are load-bearing.

---

## Classification Definitions

| Category | Definition | Reset Behavior |
|---|---|---|
| **Persistent** | Survives project save/load. Source of truth across power cycles. | Preserved on project load; lost on factory reset |
| **Runtime-rebuildable** | Derived from persistent state. Reconstructable at lifecycle boundaries (init, load) without musical data loss. **Not safe to clear during playback.** | Rebuilt from model on engine init / project load only |
| **Musical time-memory** | Runtime state that accumulates over musical time. Reset behavior affects audible output. | Reset behavior must be explicit and intentional for each owner |
| **Transactional** | Scratch/temporary state for user operations or file I/O. Not musically authoritative. | Discarded after operation completes or fails |
| **Compatibility** | Bridge state for imported subsystems (Teletype). Exists for API compatibility, not product semantics. | Preserved as needed by imported subsystem contract |

---

## Model Layer — State Owners

| State Owner | Category | Safe to Compact | Needs Documentation | Load-Bearing | Notes |
|---|---|---|---|---|---|
| `Project` | Persistent | No | Partial | Yes | Root persistent container. Contains tracks, routing, song, clock, settings. |
| `Track` + `_container` | Persistent | No | No | Yes | `Container<...>` is max-size union. Note/Curve genuinely need ~9,500 B; others pay tax. |
| `NoteSequence` (×17 per NoteTrack) | Persistent | No | No | Yes | 17 full sequence objects. Value-copy semantics. Core to Performer identity. |
| `CurveSequence` (×17 per CurveTrack) | Persistent | No | No | Yes | Same model as NoteSequence. |
| `TuesdaySequence` (×17 per TuesdayTrack) | Persistent | No | No | Yes | Parameter-only generator state, not grid data. |
| `IndexedSequence` (×17 per IndexedTrack) | Persistent | No | No | Yes | Parameter-only generator state. |
| `DiscreteMapSequence` (×17 per DiscreteMapTrack) | Persistent | No | No | Yes | Stage-based sequence. |
| `TeletypeTrack::PatternSlot` (×2) | Persistent | No | Yes | Yes | Swap-buffer mechanism. Atomic slot switching with rollback. `scene_state_t` ↔ `PatternSlot` sync is load-bearing. |
| `TeletypeTrack::_state` (scene_state_t) | Runtime (with persistent sync points) | No | Yes | Yes | VM runtime state. During execution: authoritative. `PatternSlot` is the persistent mirror; sync happens at slot switch and save/load. Do not call the live VM state "persistent" — it is reconstructed from `PatternSlot` on load. |
| `Routing` | Persistent | No | Yes | Yes | 16 routes with per-track `biasPct[8]`, `depthPct[8]`, `creaseEnabled[8]`, `shaper[8]`. Model is user-facing parameters. |
| `Song` | Persistent | No | No | Yes | Slot/repeat state for song mode. |
| `ClockSetup` | Persistent | No | No | Yes | Tempo, clock source, shift mode. |
| `Settings` | Persistent | No | No | Yes | Global project settings. |
| `UserSettings` | Persistent | No | No | Yes | Display, screensaver, brightness. |
| `Calibration` | Persistent | No | No | Yes | Per-CV-output calibration data. |
| `UserScale` | Persistent | No | No | Yes | Custom scales. |
| `PlayState` | Persistent (verify) | No | Yes | Yes | Mute/pattern request state machine. Request semantics are load-bearing. **Verify:** `TrackState` and `SongState` are written via `write()` / `read()` — confirm they survive project save/load. |
| `ClipBoard` | Transactional | Partial | Yes | No | Scratch state for copy/paste. Holds one `Track`, `NoteSequence`, `Pattern`, or `UserScale`. Never survives power cycle. |
| `HarmonyEngine` (in Model) | Runtime-rebuildable | No | No | No | Derived from current scale/chord settings. No persistent state of its own. **Not a RAM target** — it has no allocated state beyond temporary computation.

---

## Engine Layer — State Owners

| State Owner | Category | Safe to Compact | Needs Documentation | Load-Bearing | Notes |
|---|---|---|---|---|---|
| `Engine::_trackEngineContainers` | Runtime-rebuildable | No | No | Yes | Max-size union. `TeletypeTrackEngine` (~904 B) dominates. Only compactable if product caps simultaneous Teletype tracks. |
| `Engine::_trackEngines` (pointers) | Runtime-rebuildable | Yes | No | No | Pointers into `_trackEngineContainers`. Rebuilt on init. |
| `EngineState` | Runtime-rebuildable | Yes | No | No | Running/recording flags. Rebuilt from play state. **Rebuildable on init/load only; resetting mid-play stops transport.** |
| `Clock` | Runtime-rebuildable | Yes | No | No | BPM, tick counter. Rebuilt from `ClockSetup`. **Rebuildable on init/load only; resetting mid-play breaks timing.** |
| `TapTempo` / `NudgeTempo` | Runtime-rebuildable | Yes | No | No | User interaction state. Safe to reset (user-triggered). |
| `CvInput` | Runtime-rebuildable | Yes | No | No | ADC sampling state. Rebuilt on init. **Not safe to reset mid-play — causes CV glitch.** |
| `CvOutput` | Runtime-rebuildable | Yes | No | No | DAC output buffers. Rebuilt on init. **Not safe to reset mid-play — causes output glitch.** |
| `SequenceState` | Runtime-rebuildable | Yes | No | No | Step/direction/iteration per track engine. Rebuilt from sequence data. **Rebuildable on init/load or pattern switch; reset mid-step is audible.** |
| `RecordHistory` | Runtime-rebuildable | Yes | No | No | 4-event ring buffer for MIDI recording. |
| `MidiOutputEngine` | Runtime-rebuildable | Yes | No | No | MIDI output scheduling. |
| `MidiLearn` | Runtime-rebuildable | Yes | No | No | MIDI CC learning state. |
| `CvGateToMidiConverter` | Runtime-rebuildable | Yes | No | No | Conversion state. |

---

## RoutingEngine — State Owners

| State Owner | Category | Safe to Compact | Needs Documentation | Load-Bearing | Notes |
|---|---|---|---|---|---|
| `RoutingEngine::_routeStates` | **Musical time-memory** | **Partial** | Yes | Yes | `RouteState[16]` × `TrackState[8]`. ~7.4 KB baseline CCMRAM. Most entries zero — only per-track targets with stateful shapers need state. **Conditional state layout is the primary RAM recovery opportunity.** |
| `RoutingEngine::_routeStates[].TrackState.location` | Musical time-memory | — | Yes | Yes | Accumulated position. Reset on route change/project load. Audible if reset mid-operation. |
| `RoutingEngine::_routeStates[].TrackState.envelope` | Musical time-memory | — | Yes | Yes | Envelope follower state. Time-dependent output. |
| `RoutingEngine::_routeStates[].TrackState.freqAcc` / `freqSign` / `ffHold` | Musical time-memory | — | Yes | Yes | Frequency follower accumulator and hold timer. |
| `RoutingEngine::_routeStates[].TrackState.activityPrev` / `activityLevel` / `activitySign` / `actHold` | Musical time-memory | — | Yes | Yes | Activity detector state. |
| `RoutingEngine::_routeStates[].TrackState.progCount` / `progThreshold` / `progSign` / `progOut` / `progOutSlewed` / `progHold` | Musical time-memory | — | Yes | Yes | Progressive divider state. |
| `RoutingEngine::_cvRotateValues` / `_cvRotateInterp` | Runtime-rebuildable | Yes | No | No | CV rotation interpolation. Recomputed from model. |
| `RoutingEngine::_lastPlayToggleActive` / `_lastRecordToggleActive` / `_lastResetActive` | Runtime-rebuildable | Yes | No | No | Edge-detection state for routing sources. |

### RoutingEngine State Lifecycle Contract

```
Persistent:   Routing model (bias/depth/crease/shaper per route, per track)
                ↓ (read on init)
Runtime:        RouteState array allocated unconditionally
                ↓ (update() every tick)
Musical:        TrackState fields accumulate (envelope, freq, activity, prog)
                ↓ (resetShaperState() called on:)
Reset points:   project load, route target change, route shaper change
```

**Hypothesis (source-verify needed):** `resetShaperState()` resets all state unconditionally, even for unchanged routes. If true, this is the architectural mismatch: routing is coded as passthrough but behaves as modulation. **Any lifecycle redesign must preserve accumulated-state behavior for routes whose configuration did not change.** Verify by checking `Engine::reset()` and `RoutingEngine::resetShaperState()` call sites. If it only resets on `Engine::reset()` (project load), the problem is smaller than stated.

---

## TeletypeTrackEngine — State Owners

| State Owner | Category | Safe to Compact | Needs Documentation | Load-Bearing | Notes |
|---|---|---|---|---|---|
| `_performerCvOutput[8]` / `_performerGateOutput[8]` | Runtime-rebuildable | Yes | No | No | Output buffers. Recomputed every tick. **Rebuildable on init; resetting mid-tick causes output glitch.** |
| `_trActivityCountdownMs[4]` | Musical time-memory | No | Yes | Yes | Trigger activity decay timers. |
| CV slew arrays | Musical time-memory | No | Yes | Yes | Per-CV slew interpolation state. |
| Envelope state machines | Musical time-memory | No | Yes | Yes | ADSR phases per envelope. |
| LFO state | Musical time-memory | No | Yes | Yes | Phase, rate, wave position. |
| Geode voice allocation | Musical time-memory | No | Yes | Yes | `GeodeEngine::_voices` — 6 voices with phase/level/envelope. |
| Trigger dividers / pulse timers | Musical time-memory | No | Yes | Yes | `setTrDiv`, `beginPulse` state. |
| `_teletypeTrack.state()` | **Dual** Persistent + Runtime | No | Yes | Yes | `scene_state_t` is VM runtime. Sync to `PatternSlot` on slot switch. |
| `_manualScriptIndex` | Runtime-rebuildable | Yes | No | No | UI selection state. |
| `_activity` | Runtime-rebuildable | Yes | No | No | Computed activity flag. |

### TeletypeTrackEngine State Lifecycle Contract

```
Persistent:   PatternSlot (swap buffers ×2) + scene_state_t (live VM)
                ↓ (applyPatternSlot / syncToActiveSlot)
Runtime:        scene_state_t is authoritative during script execution
                ↓ (file I/O)
Transactional:  ttSlot globals (FileManager.cpp) hold file buffer copies
                ↓ (parse failure)
Rollback:       ttSlot1Backup / ttSlot2Backup restore previous slot state
```

**Problem:** 4-layer state (text → parsed → VM → PatternSlot → file buffers). The backup buffers (`ttSlot1Backup`, `ttSlot2Backup`, ~2,452 B) are transactional and **may be consolidatable** if rollback semantics can tolerate a single shared backup.

---

## GeodeEngine — State Owners

| State Owner | Category | Safe to Compact | Needs Documentation | Load-Bearing | Notes |
|---|---|---|---|---|---|
| `_voices[6]` (phase, divs, repeats, stepIndex, active, level, envelopePhase, inAttack) | Musical time-memory | No | Yes | Yes | Voice allocation and envelope state. Reset on `reset()` or Teletype script command. |
| `_prevMeasureFraction` | Musical time-memory | No | No | Yes | Previous measure position for phase wrap detection. |
| `_tuneNum` / `_tuneDen` | Runtime-rebuildable | Yes | No | No | Tuning ratios set by script. |
| `_cachedTimeScale` / `_cachedIntone` / `_cachedTuneNum` / `_cachedTuneDen` / `_timeScaleValid` | Runtime-rebuildable | Yes | No | No | Caches to avoid recomputation. |

---

## ArpeggiatorEngine — State Owners

| State Owner | Category | Safe to Compact | Needs Documentation | Load-Bearing | Notes |
|---|---|---|---|---|---|
| Step/order state | Musical time-memory | No | Yes | Yes | Current arpeggiator step, direction, octave. |
| Note buffer | Runtime-rebuildable | Yes | No | No | Currently held notes. Rebuilt from MIDI input. |

---

## FileManager — Transactional State

| State Owner | Category | Safe to Compact | Needs Documentation | Load-Bearing | Notes |
|---|---|---|---|---|---|
| `ttSlot1` / `ttSlot2` | Transactional | No | Yes | Yes | File I/O working copies. Hold parsed slot data during read/write. |
| `ttSlot1Backup` / `ttSlot2Backup` | Transactional | **Yes** | Yes | **Maybe — verify first** | Rollback backups on parse failure. **~2,452 B total. Consolidatable to 1 shared backup (~1,226 B saved) ONLY if a single backup buffer can hold both slots atomically for restore.** Current code restores both independently, so a shared backup must survive dual-slot restore. **Not low-risk: verify rollback atomicity before consolidating.** |

### FileManager Transaction Contract

```
Read path:
    1. Backup current slots → ttSlot1Backup / ttSlot2Backup
    2. Parse file → ttSlot1 / ttSlot2
    3. Validate
    4. On success: commit to track (setPatternSlotForPattern)
    5. On failure: restore from backup

Write path:
    1. Snapshot current slots → ttSlot1 / ttSlot2
    2. Write to file
    3. No rollback needed (write is forward-only)
```

**Verification needed:** Can `ttSlot1Backup` and `ttSlot2Backup` share one backup buffer? Only if a parse failure never needs to restore both slots simultaneously. The current code restores both independently (`track.setPatternSlotForPattern(0, ttSlot1Backup)` and `(1, ttSlot2Backup)`), so a single backup must hold both slots atomically.

---

## Teletype Compatibility State

| State Owner | Category | Safe to Compact | Needs Documentation | Load-Bearing | Notes |
|---|---|---|---|---|---|
| `scene_grid_t` (LITE mode) | Compatibility | Partial | Yes | No | Grid structures in LITE mode. Most fields unused if grid ops excluded. |
| `mutes[8]` in `scene_state_t` | Compatibility | **Yes** | Yes | No | Packed mute bits. Consolidated to `uint8_t` in previous cleanup. Did not move `.bss` because container max unchanged. |
| `state.turtle` | Compatibility | No | No | Yes | Turtle op state if turtle ops compiled in. |
| `state.delay` | Compatibility | No | No | Yes | Delay op queue if delay ops compiled in. |
| `state.stack` | Compatibility | No | No | Yes | Stack op state if stack ops compiled in. |

---

## Summary — Safe to Compact

| State Owner | Current Size | Compactability | Action |
|---|---|---|---|
| `RoutingEngine::_routeStates` (conditional layout) | ~7.4 KB | **High** | Allocate `TrackState` only for routes using per-track targets with stateful shapers. Global targets need no `TrackState`. |
| `ttSlot1Backup` + `ttSlot2Backup` | ~2,452 B | **Medium** | Consolidate to 1 shared backup buffer if rollback semantics allow atomic dual-slot restore. |
| `TeletypeTrackEngine` in engine Container | ~1,832 B gap | **Low-Medium** | Only if product caps simultaneous Teletype tracks below 8. Requires product decision. |
| `scene_grid_t` (LITE) | Variable | **Low** | Audit unused grid fields; stub if ops excluded. |

---

## Summary — Needs Documentation

| State Owner | Why Undocumented | What to Document |
|---|---|---|
| `RoutingEngine::_routeStates` reset behavior | No formal lifecycle | Document when shaper state resets (project load, route change, shaper change) and whether this is user-expected. |
| `TeletypeTrack` dual-path state | `scene_state_t` vs `PatternSlot` authority | Document transition points: execution (VM authoritative), save/load (PatternSlot authoritative), file I/O (ttSlot authoritative). |
| `Engine::updateTrackOutputs()` | 7+ output paths | Document authority priority: track engine → routing → bus CV → CV routes → rotation → mutes → overrides. |
| `TeletypeTrackEngine` bypass | Teletype writes directly to `_performerCvOutput` | Document whether bypass is intentional for latency or accidental. |
| `PlayState` request semantics | Immediate/Synced/Latched behavior implicit | Document request lifecycle: set → latch → commit on tick boundary. |

---

## Summary — Load-Bearing (Do Not Change Without Verification)

| State Owner | Risk If Changed |
|---|---|
| `TeletypeTrack::PatternSlot` swap mechanism | Atomic slot switching with rollback breaks; data corruption on file parse failure. |
| `scene_state_t` ↔ `PatternSlot` sync | Script execution sees stale or corrupted pattern data. |
| `RoutingEngine` stateful shaper state | Envelope followers, progressive dividers restart from zero mid-operation; audible artifacts. |
| `GeodeEngine` voice phases | Voice allocation restarts; audible envelope retriggering. |
| `PlayState` mute/pattern request state machine | Mute/pattern switch timing breaks; race conditions between UI and engine. |
| `NoteSequence` / `CurveSequence` value-copy semantics | Copy/clear/snapshot operations corrupt data if semantics change. |

---

## Contract Violations (Current Mismatches)

1. **RoutingEngine has no explicit state lifecycle.** `TrackState` is allocated unconditionally. Shaper state resets are coarse (all routes, all tracks). **Contract needed:** create/destroy `TrackState` on route configuration change; persist state for unchanged routes.

2. **Teletype state authority transitions are implicit.** `scene_state_t` → `PatternSlot` → `ttSlot` → `ttSlotBackup` transitions happen in multiple files with no formal contract. **Contract needed:** document which state is authoritative at each phase of execution, save, load, and rollback.

3. **Engine output ownership is undocumented.** 7+ paths write to CV/gate outputs. **Contract needed:** single document showing authority priority and which paths are additive vs overriding.

4. **Runtime state reset semantics are undefined.** Does pattern switch reset `RoutingEngine` shaper state? Does it reset Teletype LFO phase? **Contract needed:** per-state-owner reset matrix (project load, pattern switch, track type change, route change).

---

## Where Findings Go

This section maps which findings feed which downstream documents. It is **not an execution plan**.

- `RoutingEngine` conditional state layout → `ram-recovery-experiments.md` (Direction 3)
- `ttSlot` backup consolidation → `ram-recovery-experiments.md` (Direction 6)
- Output ownership documentation → architecture research only, no RAM recovery
- State authority transition contract → architecture research (Direction 5)
- Reset semantics matrix → architecture research (Direction 5)
