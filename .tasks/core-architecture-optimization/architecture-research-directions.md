# XFORMER Core Architecture Research Directions

## Agent Entry Point

If asked "what is the first step," do **not** start with `sizeof` measurements or RAM experiments.

Start with the assumption/mismatch map:

1. Build the Original Performer Assumption Map from current source.
2. For each assumption, classify it as:
   - still valid
   - stressed but acceptable
   - false / causing mismatch
3. Use the musical/workflow assumptions and six storage/runtime assumptions as the first pass:
   - pattern = X0X sequence grid
   - track = self-contained sequencer lane
   - edit state is close to playback state
   - output intent is track-owned
   - track type is the UI identity
   - runtime state is disposable
   - track residency
   - pattern ownership
   - routing role
   - output ownership
   - state lifecycle
   - UI branching
4. For each mismatch, separate:
   - architecture problem
   - user-visible quirk
   - future feature friction
   - RAM/flash side benefit
   - verification needed
5. Only after the mismatch map is source-grounded, decide which findings feed:
   - `architecture-research-directions.md` (this document)
   - `ram-recovery-experiments.md` (measurable RAM recovery only)

Size measurements are supporting evidence. They are not the first step of architecture research.

The first concrete output should be a short assumption/mismatch table, not a measurement patch.

---

## Executive Summary

**What is the real architectural problem?**

Original Performer was a compact X0X-style sequencer built around Note and Curve tracks with full-resident state, simple source-to-target routing, track engines directly owning CV/gate output, and UI branching by a small fixed track type set. XFORMER added Teletype (a full imported VM), Tuesday, DiscreteMap, Indexed, advanced RoutingEngine behavior (stateful modulation), Harmony, bus CV routes, USB keyboard, and Launchpad. These additions stress original assumptions unevenly — some still work, some cause RAM pressure, and some create unclear ownership or future-feature friction.

**Why RAM recovery alone is too narrow.**

RoutingEngine's baseline footprint is ~7.4 KB of CCMRAM; the recoverable amount depends on the design chosen. The architectural question is whether routing is still a sidecar or has become a modulation engine needing explicit state lifecycle. Similarly, Teletype backup buffer consolidation saves ~1,226 B, but the architectural question is whether the state ownership across `scene_state_t`, `PatternSlot`, and file buffers is coherent. RAM recovery treats symptoms. Architecture research identifies which structural changes reduce both RAM and conceptual debt.

**Top 5 research directions:**

1. **Routing as modulation** — RoutingEngine carries accumulated state for 5 stateful shapers across 16 routes × 8 tracks, but is coded as a sidecar. It needs conditional state lifecycle.
2. **Track lifecycle split** — Model Container inflates to Note/Curve's ~9,500 B because all track families share one max-sized variant. Engine Container inflates to TeletypeTrackEngine's ~904 B. These are different problems with different solutions.
3. **Pattern semantics divergence** — Note/Curve have 17 full sequence objects. Teletype has 2 swap-buffer slots. Tuesday/Indexed are generators. One storage/copy model does not fit all.
4. **Teletype compatibility boundary** — The VM is coherent as an imported subsystem, but its swap mechanism, file I/O buffers, and scene_state bridges create designed redundancy that should be simplified, not eliminated.
5. **Output ownership clarity** — CV/gate flows through 7+ paths with no single authority. Mostly intentional but undocumented, creating risk for future output features.

---

## Musical / Workflow Assumptions

The fuller Performer assumption map must include musical semantics and workflow identity, not just storage layout. Original Performer was not merely a set of C++ containers; it encoded a particular way of thinking about tracks, patterns, editing, and output ownership.

| Assumption | Original Meaning | XFORMER Stress Point |
|---|---|---|
| Pattern = X0X sequence grid | A pattern is mostly steps, gates, notes, curves, and loop bounds. Pattern switching selects another grid-like object. | Teletype patterns are script/VM slots; Tuesday/Indexed patterns are generator or mapping states. A pattern is no longer always a grid. |
| Track = self-contained sequencer lane | A track owns its pattern data, advances in time, and produces its own CV/gate output. | Teletype acts like a script VM inside a track. Routing, bus CV, CV route lanes, mutes, and output rotation can affect outputs outside the simple lane model. |
| Edit state is close to playback state | Editing a sequence directly changes the data the engine reads. | Teletype splits typed script text, parsed commands, live VM state, PatternSlot mirrors, and file buffers. Editing and playback are separated by parse/commit/sync steps. |
| Output intent is track-owned | Track engines produce CV/gate for assigned outputs; global routing is secondary. | Teletype, RoutingEngine, bus CV, CV route lanes, rotation, quantization, slew, mutes, and calibration all participate in final output behavior. |
| Track type is the UI identity | UI can branch on Note/Curve mode because each mode implies a small, known workflow. | Capabilities now matter more than type names: scripts, grid editing, macros, generators, per-track routing targets, pattern params, CV/gate ownership. |
| Runtime state is disposable | Engine state can reset/rebuild cheaply because musical truth lives in sequence/project data. | Routing shapers, Teletype delays, Geode, LFO/envelope phases, and live VM state have musical time memory. Reset behavior needs explicit semantics. |

Use this table as the first interpretive layer for future passes. Container sizes and RAM symbols are evidence, not the whole architecture.

---

## Original Performer Assumptions

| # | Assumption | Evidence | Still Valid? | Consequence In XFORMER |
|---|---|---|---|---|
| 1 | Every track slot can hold the largest model track type | `Track.h:273` — `Container<NoteTrack, CurveTrack, MidiCvTrack, TuesdayTrack, DiscreteMapTrack, IndexedTrack, TeletypeTrack> _container` | **Partially.** Model container maxes at NoteTrack (~9,500 B) — which Note/Curve actually need. The other 5 track types are much smaller but pay the NoteTrack tax. | A model split would only help if Note/Curve store patterns differently (see Direction 2). |
| 2 | Every engine slot can hold the largest engine type | `Engine.h:41` — `Container<NoteTrackEngine, CurveTrackEngine, MidiCvTrackEngine, TuesdayTrackEngine, DiscreteMapTrackEngine, IndexedTrackEngine, TeletypeTrackEngine>` | **No for the engine container.** TeletypeTrackEngine (~904 B) inflates all 8 slots. Next largest is ArpTrackEngine (~675 B in Vinx). | 1,832 B direct gap in CCMRAM. Only matters if Teletype engine count can be capped. |
| 3 | Every track family stores full pattern/snapshot objects (17 per track) | `NoteTrack.h:15` — `NoteSequenceArray = std::array<NoteSequence, 17>` (16 patterns + 1 snapshot) | **No.** Teletype stores 2 PatternSlots (swap buffers), not 17 sequences. Tuesday stores 17 `TuesdaySequence` objects (parameter-only, no step arrays). Pattern semantics diverged. | Pattern copy/clear/snapshot UI assumes uniformity, but Teletype's swap-buffer semantics differ from Note/Curve's value-copy model. |
| 4 | Routing is a lightweight sidecar | `RoutingEngine.h:30-65` — `RouteState` with `TrackState[8]` per route, 9 shaper types including 5 stateful | **No.** It evolved into a modulation engine with time-accumulated state (envelope, frequency follower, activity detector, progressive divider). | ~7.4 KB baseline CCMRAM; recoverable amount depends on conditional-state design. Shapers need explicit lifecycle. |
| 5 | Track engines own CV/gate output directly | `Engine.cpp:546-645` — `updateTrackOutputs()` collects output from track engines, then applies rotation, CV routes, and bus CV | **Partially.** Track engines produce values, but RoutingEngine writes targets, Teletype bypasses routing for CV/TR, bus CV and CV route lanes are separate pipes, and mutes override. | No single authority. Verify whether the dual path is intentional for Teletype latency guarantees. |
| 6 | UI can branch by track type with a small switch | `Track.cpp:18-65` — 6 switch/case blocks; similar in `Engine.cpp:514-537` (8 cases), `OverviewPage.cpp:372-392`, `LaunchpadController.cpp` (~15 call sites) | **Still works, but costly.** Each new track type touches ~15-20 files. | Maintenance multiplier, not a RAM issue. |

---

## Mismatches Introduced By XFORMER

### Mismatch 1: Routing evolved from sidecar into modulation engine

- **Evidence:** `RoutingEngine.h:30-65` — `RouteState` with `TrackState[8]` carrying `location`, `envelope`, `freqAcc`, `activityPrev`, `activityLevel`, `activitySign`, `ffHold`, `actHold`, `progCount`, `progThreshold`, `progSign`, `progOut`, `progOutSlewed`, `progHold`. Five shapers accumulate state over time. `Routing.h:115-160` defines 9 shaper types. `Routing::Target` spans 54 target enums.
- **What changed:** Original Performer routing was passthrough: source → min/max → target. XFORMER added per-track bias/depth/crease/shaper shaping, stateful shapers with time-dependent output, bus CV feedback loops, CV route lanes with interpolation, and per-track gate/CV rotation.
- **Cost:** ~7.4 KB CCMRAM baseline for 16 routes × 8 TrackState entries. Most entries are zero — shaper state only exists for per-track targets using stateful shapers. `Routing.h:814-822` stores per-track `biasPct[8]`, `depthPct[8]`, `creaseEnabled[8]`, `shaper[8]` per route.
- **User-visible quirks:** Shaper state resets on project load, route shaper change, and route target change. Verify whether envelope followers and progressive dividers actually restart from zero rather than resume, and whether this is expected behavior.
- **Maintenance/future-feature friction:** Adding a shaper requires adding fields to TrackState (inflating 128 entries), a case in `updateSinks()`, per-track UI controls, and serialization. Each new shaper adds ~448 B unconditional CCMRAM.
- **RAM/flash side effect:** ~7.4 KB CCMRAM baseline. Recoverable amount depends on conditional-state design — not recoverable through runtime logic alone; the `RouteState[16]` array must change its storage layout.
- **Risk if changed:** Location, Envelope, FrequencyFollower, Activity, and ProgressiveDivider produce time-dependent output. Verify whether destroying state mid-operation causes audible artifacts. Any lifecycle change must preserve accumulated-state behavior.
- **Research direction:** Routing needs conditional TrackState storage keyed by shaper type, with explicit create/destroy lifecycle. See Direction 3.

### Mismatch 2: Model Container pays NoteTrack tax

- **Evidence:** `Track.h:273` — `Container<NoteTrack, CurveTrack, MidiCvTrack, TuesdayTrack, DiscreteMapTrack, IndexedTrack, TeletypeTrack> _container`. `Container.h`: max-size union. NoteTrack ≈ 9,500 B (17 × NoteSequence).
- **What changed:** Note/Curve genuinely need all 17 sequences. Tuesday, DiscreteMap, Indexed, and Teletype are much smaller but pay NoteTrack's size.
- **Cost:** 8 tracks × ~9,500 B = ~76,000 B for model Track containers. Waste depends on how many tracks are non-Note/Curve.
- **User-visible quirks:** None — Container uses placement new, unused bytes are never touched.
- **Maintenance/future-feature friction:** Adding a track type to the Container template inflates all slots if the new type is larger, or forces the new type to pay NoteTrack's tax if smaller.
- **RAM/flash side effect:** Distributed waste — only measurable per active non-Note/Curve track.
- **Risk if changed:** Pattern copy/clear/snapshot semantics rely on value-copy of the Container variant. Splitting model storage requires redesigning these operations.
- **Research direction:** Investigate whether lazy sequence storage or type-pooled model storage is feasible without breaking value-copy semantics. See Direction 1.

### Mismatch 3: Engine Container pays TeletypeTrackEngine tax

- **Evidence:** `Engine.h:41` — `TrackEngineContainerArray` with TeletypeTrackEngine at ~904 B vs next largest at ~675 B. Direct gap: (904 - 675) × 8 = ~1,832 B CCMRAM.
- **What changed:** TeletypeTrackEngine carries CV slew arrays, envelope state machines, LFO state, Geode voice allocation, trigger dividers, pulse timers, ADC input state, and output buffers for 8 CV + 8 gate channels. Other track engines are much simpler.
- **Cost:** ~1,832 B direct CCMRAM. Potentially more if alignment cascade is verified.
- **User-visible quirks:** None — Container is placement-new'd.
- **Risk if changed:** Only saves RAM if Teletype engine count can be capped below 8 or allocated separately. If 8-simultaneous-Teletype is a product requirement, a separate pool of 8 engines cancels the savings. Verify whether users actually use >2 Teletype tracks.
- **Research direction:** See Direction 1.

### Mismatch 4: Pattern storage semantics diverged across families

- **Evidence:** Note/Curve: 17 full sequences per track (value-copy). Teletype: 2 PatternSlots (swap buffers with rollback). Tuesday/Indexed: parameter-only sequences. Teletype holds 3 copies of pattern data (`_state.patterns[4]` + `_patternSlots[0].patterns[4]` + `_patternSlots[1].patterns[4]`) = 544 B × 3 = 1,632 B per track, plus `ttSlot1/2/1Backup/2Backup` = 4,904 B in `.bss`.
- **What changed:** Original Performer only had Note/Curve. Teletype needs atomic slot switching with rollback (PatternSlot mechanism). Tuesday/Indexed need generator state, not grid data.
- **Cost:** Teletype triple-store is designed swap/rollback, not accidental duplication. But `ttSlot1Backup`/`ttSlot2Backup` (2,452 B total) could be consolidated.
- **Risk if changed:** Teletype's swap-buffer mechanism is load-bearing. Verify whether pattern slot switching can tolerate losing one backup without data corruption on file parse failure. Reducing copies requires careful state management.
- **Research direction:** See Direction 2.

### Mismatch 5: Output ownership is deliberate dual-path but undocumented

- **Evidence:** Normal track engines produce output via `trackEngine->cvOutput()/gateOutput()`. Teletype bypasses routing and writes directly to `_performerCvOutput[8]`/`_performerGateOutput[8]`. RoutingEngine writes to model properties. Bus CV, CV routes, and mutes provide additional pipes. 7+ distinct output paths with different authorities.
- **What changed:** Original Performer had one path: track engine → output. XFORMER has multiple authorities.
- **Cost:** No RAM cost — this is a clarity/maintenance issue.
- **User-visible quirks:** Verify whether Teletype CV output is intentionally excluded from routing targets like "CV Output Rotate" for latency reasons, and whether this is documented in the UI.
- **Risk if changed:** Verify whether forcing single-path output would break Teletype latency requirements before attempting unification. The dual path may be musically intentional.
- **Research direction:** See Direction 4.

### Mismatch 6: State lifecycle is scattered and undocumented

- **Evidence:** Persistent project state (`Track::_container`, `Project`, `Routing`), runtime/rebuildable state (`RoutingEngine::_routeStates`, `TrackEngine` objects), transaction/scratch state (`ClipBoard`, `ttSlot` globals), compatibility state (`scene_grid_t` in LITE, `mutes[8]`), dual-path mirror state (Teletype `PatternSlot ↔ scene_state_t`, `ttSlot ↔ file buffers`).
- **What changed:** Scattered state lifecycle is a direct result of importing Teletype and adding RoutingEngine modulation.
- **Cost:** Conceptual — no single authority for "what is the source of truth at this moment."
- **Risk if changed:** Low — this is primarily a documentation and clarity issue.
- **Research direction:** See Direction 5.

### Mismatch 7: UI branching multiplied across track types

- **Evidence:** `Track.cpp` (6 switch/case blocks), `Engine.cpp` (8-case switch), `OverviewPage.cpp`, `LaunchpadController.cpp` (~15 call sites), per-type page classes.
- **Cost:** Maintenance cost, not RAM.
- **Research direction:** See Direction 7.

---

## Research Directions

### 1. Track Lifecycle And Residency

**Why it matters:** Model Container and Engine Container both use max-size variants. Model pays NoteTrack's ~9,500 B tax (Note/Curve genuinely need it). Engine pays TeletypeTrackEngine's ~904 B tax (only Teletype needs it).

**Evidence to gather:**
- Measure actual per-track-type model sizes: `sizeof(NoteTrack)`, `sizeof(CurveTrack)`, `sizeof(TeletypeTrack)`, `sizeof(TuesdayTrack)`, etc. (Use a temporary `static_assert` in an existing build unit, not a standalone compile.)
- Profile typical track configurations: how often do users have non-Note/Curve tracks active?
- Check if NoteSequence storage can become lazy (active-pattern-only, loading others from flash/SD on demand).

**What a good architecture would clarify:**
- Whether track "type" should split into persistent project data (what gets saved) and runtime generator/service (what runs during playback).
- Whether heavy track types (Teletype, future Fractal-style) should have caps, pools, or lazy engine allocation.
- Whether model storage should be type-pooled rather than variant-containers.

**What not to change yet:**
- Do not split model Container without a clear plan for pattern copy/clear/snapshot semantics across different storage backends.
- Do not extract TeletypeTrackEngine from the engine Container unless you can answer: should any/all 8 tracks simultaneously be Teletype?

### 2. Pattern Storage Semantics

**Why it matters:** Note/Curve have 17 full sequence objects per track. Teletype has 2 swap-buffer PatternSlots. Tuesday/Indexed have parameter-only sequences. The UI treats "pattern" uniformly but storage semantics diverge.

**Evidence to gather:**
- Audit `ClipBoard.cpp` to see how copy/paste/clear branches by track type.
- Check `Project.cpp` pattern serialization for per-type differences.
- Verify `PlayState::TrackState` pattern index works for all track families.
- Check whether `CONFIG_SNAPSHOT_COUNT=1` is ever likely to change.

**What a good architecture would clarify:**
- Which fields are pattern-level (change with pattern switch) vs track-level (persist across patterns) vs runtime-only (rebuilt) vs transaction-only (scratch).
- For Teletype: verify whether `scene_state_t` is authoritative during script execution and `PatternSlot` during save/load, or whether this understanding has exceptions.
- Whether sparse defaults or pattern diffs are realistic. Verify whether they would break value-copy semantics for Note/Curve.

**What not to change yet:**
- Do not change pattern count without understanding Teletype's 2-slot mechanism.
- Do not introduce pattern diffs without verifying copy/paste/undo still works.

### 3. Routing As Modulation

**Why it matters:** RoutingEngine is the largest single CCMRAM consumer (~7.4 KB baseline) and has evolved from source-to-target plumbing to a stateful modulation engine with 5 time-dependent shapers.

**Evidence to gather:**
- Count how many routes typically use stateful shapers in user projects.
- Verify `Routing::isPerTrackTarget()` classification accuracy — confirm that global-target routes (Tempo, Swing, etc.) never need TrackState.
- Check whether any global-target routes currently use shapers.

**What a good architecture would clarify:**
- Should RoutingEngine have an explicit state lifecycle (create/destroy on shaper configuration change)?
- Should stateful modulation state be a separate subsystem (ModulationEngine) rather than folded into routing?
- Should shaper state persist across project save/load, or is "always resets on load" acceptable?
- Should the Routing target model expand to include per-track CV/gate direct output (currently Teletype bypasses it)?

**What not to change yet:**
- Do not remove stateful shapers. They produce time-dependent output.
- Do not change the `Route` model's per-track configuration arrays without understanding serialization backward compatibility.

### 4. Output Ownership

**Why it matters:** 7+ output paths with different authorities. The system works, but undocumented dual-path ownership creates risk for future features.

**Evidence to gather:**
- Map every writer of CV/gate output in `Engine::updateTrackOutputs()`.
- Verify whether Teletype bypass routing is intentional and complete.
- Check whether CV route lanes interact with track engine output (they should be additive/separate).

**What a good architecture would clarify:**
- Documentation of "who wins" when multiple writers target the same physical output.
- Whether RoutingEngine write-to-model-property writes should be considered "parameter modulation" rather than "output."
- Whether bus CV and CV route lanes should have explicit authority priority documentation.

**What not to change yet:**
- Do not force Teletype through the routing output path without verifying latency requirements.
- Do not unify bus CV and track CV routing without understanding safe-mode slew behavior.

### 5. Runtime/Project/Transaction State

**Why it matters:** State ownership is scattered. No formal model distinguishes persistent, rebuildable, transactional, and compatibility state.

**Evidence to gather:**
- Categorize every major state container by lifecycle: persistent project state, runtime/rebuildable, transaction/scratch, compatibility, dual-path mirror.
- Measure how often ClipBoard actually holds a full NoteTrack vs smaller types.
- Document state transition points for each track type.

**What a good architecture would clarify:**
- Which state survives project load? Which is rebuilt? Which is discarded?
- For Teletype: during script execution, `scene_state_t` is authoritative. During save/load, `PatternSlot` is authoritative. During file I/O, `ttSlot` globals are authoritative. Verify whether these transition points are clean or have edge cases.

**What not to change yet:**
- Do not eliminate Teletype's dual-path state without understanding why it exists.
- Do not eliminate ttSlot buffers without a replacement for file I/O atomicity.

### 6. Teletype Compatibility Boundary

**Why it matters:** Teletype imports an entire vintage project. Understanding what is essential, what is useful bridge, and what is dead weight is critical for maintenance.

**Evidence to gather:**
- Verify that the 22 excluded `.c` files in `teletype/src/ops/` are truly dead code (no linker symbols pulled in).
- Verify that bridge stubs are the minimum necessary for the compiled ops.
- Check whether `scene_grid_t` fields in LITE mode are minimal enough.

**What a good architecture would clarify:**
- Required musical compatibility: script ops, pattern ops, variable ops, envelope/LFO ops, Geode ops.
- Useful Performer-native bridge: CV/TR output, dashboard, bus CV, tempo, transport, mute.
- Harmless stale source: excluded `.c` files.
- Removable/stubbable RAM or flash cost: `mutes[8]` packing, grid structures in LITE, ttSlot backups.

**What not to change yet:**
- Do not change `scene_state_t` field order without verifying `MAKE_SIMPLE_VARIABLE_OP` offsetof compatibility.
- Do not remove bridge stubs without verifying all compiled ops that reference them.

### 7. UI And Capability Model

**Why it matters:** Each new track type multiplies switch-case branches across ~15-20 files.

**Evidence to gather:**
- Count all `case Track::TrackMode::` branches in the codebase.
- Check which pages are track-type specific vs generic.
- Evaluate whether a capability matrix would reduce branching.

**What a good architecture would clarify:**
- Whether `TrackMode` should be replaced by a capability bitmask for UI dispatch.
- Whether page classes should be capability-selected rather than mode-selected.
- Whether this reduces real maintenance cost or just shuffles it.

**What not to change yet:**
- Do not over-abstract. 7 track types is manageable with switch cases. A capability matrix would add complexity unless the number of types grows beyond ~8-10.

---

## Direction Ranking

| Direction | Architectural Value | Quirk Reduction | Future Feature Leverage | RAM Side Benefit | Risk | Timing |
|---|---|---|---|---|---|---|
| 3. Routing as Modulation | High | Medium | High | High (baseline ~7.4 KB; recoverable depends on design) | Medium | Next (highest ROI) |
| 1. Track Lifecycle | High | Low | High | Medium (~1.8 KB engine; more if model split viable) | Medium | After routing |
| 2. Pattern Semantics | Medium | Low | Medium | Low (Teletype triple-store is designed) | Medium | Research when needed |
| 4. Output Ownership | Medium | Medium | Medium | None | Low (documentation) | Parallel with routing |
| 5. State Lifecycle | Medium | Low | Medium | Low (ttSlot ~1.2 KB) | Low | Parallel with lifecycle |
| 6. Teletype Boundary | Low | Low | Low | Medium | Low | When needed |
| 7. UI Capability Model | Low | None | Medium | None | Low | Only if types grow |

---

## Keep / Change / Research Later

### Keep
- **Note/Curve model Container at NoteTrack size.** Both types genuinely need 17 full sequences.
- **Teletype dual-path output.** Verify whether it exists for latency reasons before changing.
- **Teletype PatternSlot swap-buffer mechanism.** It serves atomic slot switching. Simplify backups, don't eliminate the mechanism.
- **LITE mode configuration.** Correctly constrains scripts, delays, and grid.
- **Route model per-track configuration (`biasPct`, `depthPct`, `creaseEnabled`, `shaper`).** These are user-facing parameters that correctly live in the persistent model.

### Change
- **RoutingEngine `TrackState[8]` storage layout should be conditional.** Routes targeting global targets don't need per-track state. Stateful shapers should only allocate TrackState entries when the shaper and target type require it. This is the highest-ROI architecture change. The `RouteState[16]` array must change its storage layout — runtime branching alone saves nothing. See `ram-recovery-experiments.md`.
- **ttSlot backup buffers should be consolidated.** 2 backups into 1 shared backup saves ~1,226 B. See `ram-recovery-experiments.md`.
- **`mutes[8]` should be `uint8_t` bits.** Saves 56 B per track, trivial change. Already merged.

### Research Later
- **Model Container split into type-pooled storage.** Requires redesigning Track copy/clear/snapshot and project serialization.
- **Engine Container extraction of TeletypeTrackEngine.** Only saves RAM if maximum live Teletype track count is capped. Research whether users actually use >2 Teletype tracks.
- **Capability-matrix UI dispatch.** Only worth it if track type count grows significantly.
- **Pattern diff/sparse storage for Note/Curve.** Requires complex serialization, breaks value-copy semantics. Research needed.

---

## Relationship To RAM Recovery

**Findings that feed `ram-recovery-experiments.md`:**
- RoutingEngine conditional state (Direction 3)
- ttSlot backup consolidation (Direction 6 / Mismatch 4)
- Container size measurement (Direction 1)
- Symbol/section measurement (Direction 6)

**Findings that do NOT feed RAM recovery:**
- Output ownership (Direction 4) — documentation only
- State lifecycle taxonomy (Direction 5) — conceptual clarity
- UI capability model (Direction 7) — maintenance only

**Key distinction:** "Routing evolved from sidecar to modulation engine" is the architecture finding. "Conditional TrackState storage layout may recover CCMRAM" is the RAM experiment. They serve different purposes. The architecture finding explains why the current design is structurally wrong; the RAM experiment measures what a fix would save.
