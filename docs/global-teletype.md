# Teletype Runtime Architecture: Per-Track VM vs. Global VM

**Source-grounded design note. Do not implement from this document — use for analysis only.**

---

## Verdict

**One global VM is unsafe** given current code structure. The `scene_state_t` is a single-writer mutable blob (variables, patterns, delay queue, stack, random state, turtle, script execution context). Multiple Teletype tracks running scripts concurrently on a shared `scene_state_t` would produce nondeterministic output because `run_script()`, `tele_tick()`, `process_command()`, and all op-handlers mutate the state in-place without any ownership discipline, locking, or partitioning.

The architecture is *already* heading toward a global runtime via `TeletypeBridge::ScopedEngine` (the `g_activeEngine` singleton in `TeletypeBridge.cpp:14`), but this is a *callout* pattern, not shared state. The bridge routes I/O side-effects to whichever engine holds the scope; the VM state itself remains per-track.

A **global runtime/scheduler with per-track contexts (Architecture C)** preserves safety while consolidating scheduling, bridge, and I/O. This is the principled path.

---

## Teletype Runtime Semantics Matrix

Current ARM size anchors from MonitorPage SIZES page 6:

| Object | Size | Meaning |
|---|---:|---|
| `TeletypeTrack` | 7,104 B | Below the current model container gate (`NoteTrack=9544 B`) |
| `TeletypeTrack::PatternSlot` | 1,226 B | Partial/shadow slot storage, not a full VM snapshot |
| `scene_state_t` | 4,640 B | Full Teletype VM state |
| `tele_command_t` | 52 B | Command struct |
| `scene_pattern_t` | 138 B | One VM pattern |
| `TeletypeTrackEngine` | 912 B | Current engine container gate |
| `Engine::TrackEngineContainer` | 912 B | Confirms TeletypeTrackEngine sizes the engine variant |
| `NoteTrackEngine` | 588 B | Next-largest measured engine |

RAM implication: a global VM or Teletype model cleanup is not automatically a top-level model RAM win under the current `Track::_container` gate. Engine-side extraction/compaction is the measurable Teletype container opportunity: `(912 - 588) * 8 = 2,592 B` CCMRAM, conditional on capped/separate Teletype engine residency or shrinking TeletypeTrackEngine below 588 B. It should follow a cleaner slot/VM ownership contract.

Rows are semantics dimensions. Columns are where each piece lives today, with source references.

| Dimension | TeletypeTrack (model) | TeletypeTrackEngine (runtime) | TeletypeBridge (global) | scene_state_t (VM core) |
|-----------|----------------------|------------------------------|------------------------|------------------------|
| **Scripts** | `_state.scripts[]` stored in VM. Slots copy scripts in/out of VM at index 3 (SlotScript) and METRO_SCRIPT. (`TeletypeTrack.cpp:393-398`) | `runScript()` calls `run_script()` under ScopedEngine; `loadScriptsFromModel()` calls `applyActivePatternSlot()`. (`TeletypeTrackEngine.cpp:1416-1418`) | Not involved | Owns `scene_script_t scripts[TOTAL_SCRIPT_COUNT]`. `TOTAL_SCRIPT_COUNT=7` with `TELETYPE_TRACK_LITE`. (`state.h:283`, `script.h:12-15`) |
| **Variables** (a-z, cv, drunks, q, etc.) | Accessible via `_state.variables`. | `updateAdc()` writes `in`, `param`, `x`, `y`, `z`, `t`. (`TeletypeTrackEngine.cpp:861-879`) | Not involved | `scene_variables_t variables` — 70+ fields including `m` (metro), `m_act`, `tr[]`, `tr_pol[]`, `tr_time[]`, `j[]`, `k[]`, `q[]`, `drunk*`, `o*`, `seed`, etc. (`state.h:79-135`) |
| **Patterns** | `_state.patterns[PATTERN_COUNT]`. Slots contain shadow copies. Pattern 0 -> slot 0, pattern 1 -> slot 1, etc. (`TeletypeTrack.h:647-649`) | Reads other tracks' sequences via `noteGateGet/Set`, `noteNoteGet/Set`. (`TeletypeTrackEngine.cpp:667-710`) | Not involved | `scene_pattern_t patterns[4]` — each has `idx`, `len`, `wrap`, `start`, `end`, `val[64]`. (`state.h:138-145`) |
| **Delay queue** | Stored in `_state.delay`. | `panic()` calls `clear_delays()`. `tele_tick()` processes delays (decrements, fires expired). (`TeletypeTrackEngine.cpp:169`, `teletype.c:362-418`) | `g_hasDelays` flag, `g_hasStack` flag set by `tele_has_delays()`, `tele_has_stack()`. (`TeletypeBridge.cpp:585-591`) | `scene_delay_t delay` — 8 entries (TELETYPE_TRACK_LITE). Each has `commands[]`, `time[]`, `origin_script[]`, `origin_i[]`, etc. (`state.h:147-156`) |
| **Metro** | `activeSlot().metro[]`, `activeSlot().metroLength` — slot shadow. `_state.variables.m`, `m_act` in VM. | `runMetro()` reads VM's `m`/`m_act`, fires METRO_SCRIPT. `syncMetroFromState()` syncs VM `m` -> engine `_metroPeriodMs`. Engine::setTeletypeMetroAll() iterates ALL Teletype tracks. (`TeletypeTrackEngine.cpp:1123-1166`, `Engine.cpp:350-372`) | `tele_metro_updated()` calls `syncMetroFromState()`. `tele_metro_all_set/act/reset()` -> Engine global. (`TeletypeBridge.cpp:94-116`) | `m` (period), `m_act` (active flag). Default 1000ms. (`state.h:102-103`) |
| **Stack** | Not directly; stored in `_state.stack_op`. | Not involved | `g_hasStack` flag. | `scene_stack_op_t stack_op` — 16 entries. (`state.h:158-161`) |
| **Random state** | Not directly; stored in `_state.rand_states`. | Not involved | Not involved | `scene_rand_t rand_states` — 5 separate RNGs (rand, prob, toss, pattern, drunk). Initialized from `rand()`. (`state.h:265-275`) |
| **CV outputs** | `PatternSlot::cvOutputDest[4]`, `cvOutputRange[4]`, `cvOutputOffset[4]`, `cvOutputQuantizeScale[4]`, `cvOutputRootNote[4]`. | `handleCv()` routes TO-CV1->4 to physical outputs via cvOutputTracks layout. Applies range, offset, quantize, interpolation, slew. Owns LFO/env/Geode output synthesis. (`TeletypeTrackEngine.cpp:463-531`) | `tele_cv()` routes to `activeEngine()->handleCv()`. (`TeletypeBridge.cpp:167-171`) | VM owns `cv[]`, `cv_off[]`, `cv_slew[]` — raw output values and config. (`state.h:93-95`) |
| **TR outputs** | `PatternSlot::triggerOutputDest[4]`. | `handleTr()` routes TO-TRA->D to physical gates via gateOutputTracks. Owns pulse timers, width, div. (`TeletypeTrackEngine.cpp:338-373`) | `tele_tr()`, `tele_tr_pulse()`, `tele_tr_pulse_end()`, etc. all -> `activeEngine()`. (`TeletypeBridge.cpp:124-165`) | `tr[]`, `tr_pol[]`, `tr_time[]` — only 4 outputs. (`state.h:123-125`) |
| **Input mappings** | `PatternSlot::triggerInputSource[4]`, `cvInSource`, `cvParamSource`, `cv{X,Y,Z,T}Source`. | `updateAdc()` reads physical/logical CVs based on mappings, writes to VM `in`, `param`. `inputState()` reads trigger sources. `updateInputTriggers()` edge-detects and fires scripts 0-3. (`TeletypeTrackEngine.cpp:805-921`, `1113-1121`) | `tele_update_adc()` -> `activeEngine()->updateAdc()`. `tele_get_input_state()` -> `activeEngine()->inputState()`. | `in`, `param` raw values. `script_pol[8]` for trigger polarity. (`state.h:101,108,120`) |
| **Boot/reset** | `bootScriptIndex()`, `requestBootScriptRun()`, `consumeBootScriptRequest()`. `_resetMetroOnLoad`. | `reset()` saves/restores scripts 0-2 across `ss_init()`, applies slot, seeds presets if empty. `runBootScriptNow()` resets metro if `resetMetroOnLoad` set. (`TeletypeTrackEngine.cpp:68-152`, `158-165`) | Not involved | `ss_init()` resets all variables, patterns, scripts, rand, delay, grid, midi, turtle to defaults. (`state.c:19-41`) |
| **File import/export** | `write()`/`read()` serialize everything including both slots + VM state. | Not involved | Not involved | Scripts 0-2 and patterns 0-3 are serialized as VM state (not per-slot). |
| **Note-track R/W** | Not directly. | `noteGateGet/Set`, `noteNoteGet/Set`, `noteGateHere`, `noteNoteHere` — read/write other tracks' NoteSequences via Engine. Cross-track mutation with no locking. (`TeletypeTrackEngine.cpp:667-750`) | `tele_wng()`, `tele_wng_set()`, `tele_wnn()`, `tele_wnn_set()` -> `activeEngine()`. | Not involved |
| **Track mode switching** | When switching from/to Teletype mode, Engine `updateTrackSetups()` placement-news engine into `TrackEngineContainer`. (`Engine.cpp:504-539`) | Constructor calls `reset()` which calls `ss_init()`. | ScopedEngine scope is stack-local — no issue. | State is reinitialized on construction. |

---

## Architecture Comparison: A, B, C

### Architecture A: Current Model Refined (per-track VM, cleaner boundaries)

**State ownership:** Each `TeletypeTrack` owns one `scene_state_t` + `_patternSlots[2]`. Each `TeletypeTrackEngine` owns all runtime synthesis state (LFOs, envelopes, Geode, pulses, slew, interpolation). `TeletypeBridge` owns the global active-engine pointer and dashboard globals.

**Execution ordering:** One `Engine::update()` loop iterates all tracks calling `trackEngine->update(dt)`. Within each Teletype update, scripts run synchronously under `ScopedEngine`. MS-timebase tracks call `tele_tick()` with small steps; Clock-timebase tracks derive delta from clock position. Cross-track note mutation happens via Engine references with no ordering guarantees.

**Save/load shape:** `TeletypeTrack::write()` syncs VM -> active slot, then serializes scripts 0-2 from VM, patterns from VM, active slot index, both PatternSlot structs (including their slotScript, metro, patterns, I/O mappings, timing config, CV output config, MIDI source, boot script, reset flags). `FileManager::writeTeletypeTrack()` exports to a human-readable text format with `#IO`, `#S4P1`, `#M1`, `#S4P2`, `#M2`, `#S1`, `#S2`, `#S3`, `#PATS` sections. Backup/rollback is done via shared global buffers (`ttSlot1Backup`, `ttSlot2Backup` in FileManager.cpp:628-635).

**Pattern/slot semantics:** 2 slots per track (PatternSlotCount=2). Each CONFIG_PATTERN_COUNT pattern maps to one of two slots via `patternIndex % 2`. Slot 3 in the VM (`SlotScriptIndex=3`) holds the slot-specific script. METRO_SCRIPT (=4 in TELETYPE_TRACK_LITE) holds metro commands. Scripts 0-2 are shared across both slots. `applyPatternSlot()` copies slot scripts and patterns into the VM; `syncToActiveSlot()` does the reverse. This creates a double-buffering model where the VM is the "live" copy and slots are the "stored" copies.

**Track mode switching:** Placement-new into `TrackEngineContainer`. Constructor calls `reset()` -> `ss_init()` -> fresh VM state. No persistent state survives mode switching beyond what's serialized in the model.

**Compatibility:** Fully backward-compatible with existing projects.

**RAM direction:** No change from current. Roughly `sizeof(TeletypeTrack)` ≈ 9560 bytes per track (as per AGENTS.md) — 8 tracks × 9560 = ~76 KB just for models. `sizeof(TeletypeTrackEngine)` per track adds to CCMRAM footprint.

**Biggest correctness risks:**
1. `syncToActiveSlot()` is called from `write()`, `patternSlotSnapshot()`, `setPatternSlotForPattern()`, `clearPatternSlot()`, `copyPatternSlot()`, `onPatternChanged()` — all via `const_cast`. This can mutate state during read operations. (`TeletypeTrack.cpp:331-332`)
2. Cross-track note mutation via `noteGateSet`/`noteNoteSet` is lock-free and ordered by Engine's track iteration; scripts on different tracks could produce nondeterministic results when they read each other's sequences within the same tick.
3. `panicTeletype()` iterates all tracks after the per-track update loop — if a delayed command fires between per-track update and panic, state is inconsistent.
4. The `g_activeEngine` singleton means callbacks are routed to whichever engine last set the scope. If engine A's `tele_tick()` fires a delay that executes a command calling back to `tele_cv()`, engine A must still be the active engine — this is guaranteed by the `ScopedEngine` RAII stack, but nested scopes could cause issues if a command triggers another script.

---

### Architecture B: One Global Teletype VM

**State ownership:** A single `scene_state_t` shared across all Teletype tracks. Each track becomes a thin adapter providing trigger routing, CV input mapping, and CV/TR output destination routing into the shared VM.

**Execution ordering:** All tracks feed into one `tele_tick()` call per tick. Script execution must be serialized (only one script can run at a time on the shared VM). Multiple triggers in one tick mean either: (a) queued execution, or (b) merged triggers. The current `run_script()` is synchronous and non-reentrant.

**Save/load shape:** One VM state per project instead of 8. Per-track I/O mappings would need to live outside the VM in the adapter layer. FileManager would need restructuring.

**Pattern/slot semantics:** Patterns live in the shared VM. Track-specific pattern selection (which track is `P.I 0`?) becomes ambiguous — `P.I` is currently a per-VM concept. Slot scripts no longer make sense per-track; the slot concept would need to be reimagined as something like "scene" or "bank."

**Track mode switching:** Non-Teletype tracks don't interact with the VM. Mode switching a Teletype track to Note would remove its I/O adapter but the VM itself persists.

**Compatibility:** BREAKING. Existing projects with per-track Teletype state cannot map 1:1 into a global VM.

**RAM direction:** No real savings are accessible here. The `Track::_container` is sized to the largest track type (NoteTrack at ~9.5 KB); removing `scene_state_t` from TeletypeTrack would not change the top-level container allocation unless TeletypeTrack became the new smallest arm. The `TrackEngineContainer` in CCMRAM is sized to the largest engine (TeletypeTrackEngine or TeletypeTrackEngine's CCM counterpart); TeletypeTrackEngine's large LFO/env/pulse/Geode state dwarfs any savings from removing the VM buffer. The VM is not the RAM problem — the routing / synthesis state / I/O mapping tables are.

**Biggest correctness risks:**
1. **Variable collision**: Variables a-z are global mutable state in the VM. Track A's script sets `X 5`; Track B's script reads `X` expecting its own value — data race without partitioning.
2. **Pattern namespace**: `P.I 0`, `P.N 0`, `P 0 0` — which track is pattern 0? If patterns are track-scoped in user intent, a global namespace breaks the mental model.
3. **Delay queue collision**: 8 delay slots (TELETYPE_TRACK_LITE) shared across up to 8 tracks — each track gets 1 delay slot on average. Unusable for real scripts.
4. **Random state sharing**: `RAND`, `PROB`, `TOSS`, `P.R`, `DRUNK` — all tracks draw from the same RNG stream. Scripts that seed RNG (`R.SEED`) would affect other tracks.
5. **Stack overflow**: 16-entry stack shared across all tracks.
6. **Metro**: One metro per world — can't have different metro periods per track.
7. **CV/TR output conflict**: `CV 1 N 48` writes to CV output 1 — which track's output routing map applies? The `tele_cv()` callback goes to `activeEngine()`, which has per-track routing. If the VM is shared, which track's routing map is active?
8. **Turtle state**: `scene_turtle_t` is one per VM. Turtles are usually per-pattern/per-track concepts.
9. **Note-track read/write**: `noteGateSet`/`noteNoteSet` need to know *which* Teletype track is making the mutation — currently this is implicit through the engine that set `g_activeEngine`. With a shared VM, there's no implicit track identity.
10. **Re-entrancy**: Current code base assumes at most one script runs at a time per `scene_state_t`. The `es_init(&es)` + `es_push(&es)` pattern in `run_script()` (teletype.c:170-175) creates a fresh exec context per script entry. A global VM with concurrent scripts would need a different exec model.

---

### Architecture C: Global TeletypeRuntime Scheduler + Per-Track Contexts

**State ownership:**
- **Global runtime** (`TeletypeRuntime`): owns clock/tick scheduling, the bridge I/O dispatch table, metro-all coordination, bus CV, dashboard, II bus stubs, calibration. Essentially what `TeletypeBridge` + `Engine` own today for Teletype, promoted into a proper subsystem.
- **Per-track VM context**: each active Teletype track retains its own `scene_state_t`. This is the minimal viable shared-world model: scheduling is unified but VM state stays separate.

**Execution ordering:** Runtime calls `tele_tick()` on each active track's VM in deterministic track order (track 0..7). Script execution is still synchronous within each track's VM (no reentrancy needed). Cross-track operations (`tele_metro_all_*`, `noteGateSet`) go through the runtime which has authoritative access. Same-tick script ordering is deterministic: track 0 metro, track 0 trigger, track 1 metro, track 1 trigger, etc.

**Save/load shape:** Each track's VM saved independently (as today), plus a runtime config section (metro-all state, dashboard values, bus CVs). The runtime section is small.

**Pattern/slot semantics:** Unchanged from Architecture A. Each track retains its own patterns and slot system.

**Track mode switching:** Unchanged from Architecture A for per-track state. Runtime-side config (metro-all state) persists across mode switching.

**Compatibility:** Fully backward-compatible if serialization adds a version tag for the runtime section. Old projects load with default runtime config.

**RAM direction:** Adds a small `TeletypeRuntime` struct (~100-200 bytes) to globals. Does not save per-track VM RAM. Net RAM increase is negligible.

**Biggest correctness risks:**
1. **Tick ordering determinism**: Must ensure all `tele_tick()` calls for all tracks happen before any output is committed, so cross-track CV/TR outputs are consistent. This is already the pattern in `Engine::update()` (tick all, then updateTrackOutputs).
2. **Metro-all correctness**: `tele_metro_all_set()` currently writes `m` into every track's VM. If a script in track A changes `M` during its tick, and metro-all fires before track B's tick, track B's metro script sees a different period than track A's. The runtime must either: (a) apply metro-all *before* any per-track tick, or (b) snapshot what "all" means and apply to each VM atomically.
3. **Cross-track note mutation ordering**: `noteGateSet(trackIndex, step, val)` written by track A during its tick is visible to track B's script during the same tick if track B reads it via `noteGateGet`. Deterministic ordering (track index order) makes this predictable but may surprise users who expect "simultaneous" execution.
4. **ScopedEngine evolution**: Today `ScopedEngine` sets `g_activeEngine` for I/O routing. In Architecture C, the Runtime would own the dispatch; `ScopedEngine` becomes unnecessary or is replaced by a track-context handle passed into the runtime.

---

## Unanswered Questions (Cannot Be Determined From Source Alone)

1. **Do users routinely run 2+ Teletype tracks with scripts that mutate each other's state?** Source shows `noteGateSet`/`noteNoteSet` cross-track mutation exists (`TeletypeTrackEngine.cpp:678-710`), but how users actually use this is unknown. Answer: user survey or instrumented firmware logging.

2. **Do existing projects have Teletype tracks where both slots are actively used during performance?** The slot system supports rapid switching; we don't know if users actually switch slots mid-performance or just use one slot per project load. Answer: project file analysis of existing user content.

3. **What is the maximum observed delay queue depth and stack depth during real use?** 8-entry delay queue (TELETYPE_TRACK_LITE) may already be tight. Answer: runtime instrumentation with overflow counters.

4. **Do users expect `M.ACT 1` on one track to affect other tracks?** The `tele_metro_all_set/act/reset` functions exist but their invocation path is unclear — are they called from scripts (via `$` prefix in Performer-specific ops) or only from UI? Answer: grepping for op dispatch that calls these in the Performer-specific ops table, plus user behavior testing.

5. **Is the pattern-per-track indexing in scripts (`P.I 0` = track local) the correct mental model, or do users conceptualize patterns globally?** The source makes patterns per-VM (= per-track today). Architecture B would break this. Answer: user research.

---

## Recommended Next Step

Resolve the slot/VM ownership contract first — see `docs/slots-teletype.md` for the full analysis.

The `syncToActiveSlot()` / `applyPatternSlot()` dance is the most fragile part of the current design, spans both the model and engine layers, and all three architectures depend on either preserving or eliminating it. Until the ownership relationship between the PatternSlot storage and the VM state is resolved, any refactor toward a global runtime or consolidated bridge will inherit the same fragility. Defer Architecture C / `TeletypeRuntime` extraction until after the slot/VM contract is settled.

Secondary priority: instrument delay queue and stack overflow in firmware to collect real-world usage data, which informs whether 8-entry delay queues can ever be shared (Architecture B viability).
