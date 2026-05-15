# RAM Recovery Experiments

## Scope

This document is for measured RAM/flash recovery only. It is **not** the full architecture research answer. For architecture findings, mismatches, and research directions, see `architecture-research-directions.md`.

Current triage: RoutingEngine state compaction is the next implementable RAM experiment. Model/container pool architecture is not an active experiment under current "any track can be any type" semantics; see `model-pool-decision-table.md` for the no-go decision. Note/Curve sequence-header packing remains measurement-first because it can reduce the dominant container arms directly without changing project semantics.

## Baseline (post Phase 1, from `sequencer.size` + `nm`)

| Metric | Value | Source |
|---|---|---|
| `.data` | 6,316 B | `sequencer.size` (post P14/P14b) |
| `.bss` | 118,400 B | `sequencer.size` |
| `.ccmram_bss` | 58,232 B | `sequencer.size` |
| Total runtime RAM | 124,716 B | `.data + .bss` |
| `engine` singleton (CCMRAM) | 15,588 B | `nm --size-sort` |
| `model` (BSS) | 92,836 B | `nm --size-sort` |
| `ui` (BSS) | 27,244 B | `nm --size-sort` |
| `fsTask` stack (BSS) | 4,256 B | `nm --size-sort` |
| `ttSlot` globals × 4 (BSS) | 4,904 B | `nm --size-sort` (4 × 1,226 B) |

**Verified struct sizes (from resource-optimization TASK.md and savings-plan):**

| Object | Size | Source |
|---|---|---|
| `RouteState::TrackState` | 56 B | savings-plan ARM struct analysis |
| `RouteState` (full) | 468 B (ARM) / 460 B (standalone) | savings-plan; on-target verification needed |
| `RouteState × 16` | 7,488 B (ARM) | 468 × 16 |
| `TeletypeTrack` | 7,104 B | MonitorPage SIZES page 6 |
| `TeletypeTrackEngine` | 912 B | MonitorPage SIZES page 6 |
| `Engine::TrackEngineContainer` | 912 B | MonitorPage SIZES page 6 |
| `scene_state_t` | 4,640 B | MonitorPage SIZES page 6 |
| `PatternSlot` | 1,226 B | MonitorPage SIZES page 6 |
| `tele_command_t` | 52 B | MonitorPage SIZES page 6 |
| `scene_pattern_t` | 138 B | MonitorPage SIZES page 6 |
| `NoteSequence::Step` | 8 B | host sizeof probe; ARM verification needed |
| `NoteSequence` | 564 B | host sizeof probe; 512 B step array + ~52 B header/state |
| `CurveSequence::Step` | 8 B | host sizeof probe; ARM verification needed |
| `CurveSequence` | 592 B | host sizeof probe; 512 B step array + ~80 B header/state |
| `NoteTrack` (model) | 9,544 B | MonitorPage after P15 |
| `CurveTrack` (model) | 9,480 B | MonitorPage after P15 |
| `Track` (model container) | 9,560 B | MonitorPage after P15 |
| Engine container direct gap | TBD | `TeletypeTrackEngine=912` is the gate; measure next-largest engine before estimating |

---

## Experiments

### Experiment A: RoutingEngine Conditional State

**Architecture finding it came from:** Routing evolved from a lightweight sidecar into a modulation engine with accumulated state, but still allocates that state unconditionally for all 16 routes × 8 tracks. See `architecture-research-directions.md` Mismatch 1.

**Hypothesis:** Changing the `RouteState` storage layout so that shaper state is only allocated for routes+tracks that use stateful shapers on per-track targets saves measurable CCMRAM without changing any musical behavior.

**Important:** Runtime branching alone (skipping TrackState access in `updateSinks()`) saves zero RAM because the `RouteState[16]` array is statically allocated with full `TrackState[8]` in every entry. Savings require changing the storage layout — replacing the unconditional `std::array<TrackState, 8>` with a conditional or union-based structure that has smaller actual allocation. This is a struct layout change, not just a logic change.

**Two sub-experiments:**

#### A1: Skip TrackState storage for non-per-track routes

**Rationale:** `Routing::isPerTrackTarget()` already classifies targets. Routes targeting global targets (Tempo, Swing, Play, Record, CvRouteScan, CvRouteRoute, Mute, Fill, Pattern) do not need per-track shaping. Currently every route gets `TrackState[8]` in the `RouteState` struct regardless of target type.

**Required change:** Replace `std::array<TrackState, CONFIG_TRACK_COUNT> shaperState` in `RouteState` with a conditional structure. Options: (a) use `etl::optional<TrackState[8]>` or similar, (b) use a pointer to dynamically-allocated TrackState arrays only for per-track routes, or (c) use a separate array indexed by route type. The key point: `RouteState` must shrink in memory for global-target routes.

**Files:**
- `src/apps/sequencer/engine/RoutingEngine.h` — change `RouteState` struct layout to make TrackState conditional
- `src/apps/sequencer/engine/RoutingEngine.cpp` — update `updateSinks()` and state access to handle conditional TrackState
- `src/apps/sequencer/model/Routing.h` — no change needed (model arrays are per-track config, not per-track state)

**Measurement command:**
```sh
arm-none-eabi-nm --print-size --size-sort build/stm32/release/src/apps/sequencer/sequencer.elf | grep -E "routing|Rout" | head -20
arm-none-eabi-size build/stm32/release/src/apps/sequencer/sequencer.elf
```

**Expected RAM/flash delta:** ~2,688 B CCMRAM saved (8 non-per-track targets × 56 B × 8 tracks = 3,584 B gross, minus per-route overhead). Conservative estimate: ~2,688 B. **This requires a storage layout change, not just conditional logic.**

**Behavior risk:** Low. `isPerTrackTarget()` already exists. Non-per-track routes never read TrackState. The storage layout change is safe because global-target routes have no per-track shaper state to preserve.

**Hardware check:** Boot and verify all 9 shaper types still produce correct output for per-track targets.

**Exit criteria:** Project with routes targeting both per-track and global targets loads and runs correctly. Non-per-track routes modulate Tempo/Swing/etc. correctly.

**Rollback:** Revert RoutingEngine.h/cpp changes.

#### A2: Union-based TrackState by shaper type

**Rationale:** Even for per-track routes, only 5 of 9 shapers use TrackState fields. None, Crease, TriangleFold, and VcaNext are stateless or use only `location` (which Location also uses). A per-TrackState union keyed by shaper type would reduce from 56 B to ~16-24 B for most shaper types, and 0 B for None/Crease/TriangleFold/VcaNext.

**Required change:** Replace the monolithic `TrackState` struct (56 B per entry, 8 entries per route, 16 routes = 7,168 B) with a per-TrackState union + discriminant. Each TrackState within a single RouteState can have a different active shaper (track 0 = ProgressiveDivider, track 1 = Location, etc.), so the union + discriminant must be per-TrackState, not per-RouteState. This is a struct layout change that reduces the actual memory allocation, not just a logic change.

**Files:**
- `src/apps/sequencer/engine/RoutingEngine.h` — Replace `TrackState` struct with per-TrackState union + discriminant
- `src/apps/sequencer/engine/RoutingEngine.cpp` — Update `updateSinks()` to access state through union members, create/destroy state on shaper configuration changes

**Measurement command:** Same as A1.

**Expected RAM/flash delta:** ~4,096 B CCMRAM saved (conservative: ~32 B per TrackState × 128 entries, though stateful shapers need more).

**Behavior risk:** Medium. Must verify all 5 stateful shapers produce identical output before and after. Shaper state lifecycle must be preserved: state created when shaper configured, accumulated during runtime, destroyed on shaper change.

**Hardware check:** Test envelope follower, frequency follower, activity detector, progressive divider, and location shaper with continuous CV input over 30+ seconds. Verify smooth ramps, no snaps.

**Exit criteria:** All 9 shaper types produce identical output to baseline. Project files with routing configurations load correctly.

**Rollback:** Revert RoutingEngine.h/cpp changes.

#### A2-aggressive: Strip all TrackState (stateless shapers only)

**Rationale:** Remove TrackState entirely for the 4 stateless shaper types (None, Crease, TriangleFold, VcaNext). Only allocate state for stateful shapers.

**Expected savings:** Additional reduction beyond A2. Combined A1+A2+aggressive ≈ ~7,488 B (full RouteState removal to Vinx-equivalent). But A2-aggressive alone breaks 5/9 shapers if done without union support. Only safe if 5 stateful shapers are confirmed unused.

**Risk:** HIGH. Changes accumulated shaper behavior. Only pursue if user testing confirms stateful shapers are not actively used.

---

### Experiment B: Teletype File Backup Consolidation

**Architecture finding it came from:** Teletype pattern data is triple-stored (scene_state + PatternSlot + file buffer), which is a designed swap/rollback mechanism, not accidental duplication. But the file I/O backup buffers can be consolidated. See `architecture-research-directions.md` Mismatch 4.

**Hypothesis:** Replacing the two separate backup buffers (`ttSlot1Backup` and `ttSlot2Backup`) with one shared backup buffer saves ~1,226 B of `.bss` without affecting normal operation. On multi-slot parse failure, only the most recently dirtied slot is restorable.

**Files:**
- `src/apps/sequencer/model/FileManager.cpp:628-635` — Replace `ttSlot1Backup` and `ttSlot2Backup` with single `ttSlotBackup`
- `src/apps/sequencer/model/FileManager.cpp` — Update `readTeletypeTrack()` backup/restore logic

**Measurement command:**
```sh
arm-none-eabi-nm --print-size --size-sort build/stm32/release/src/apps/sequencer/sequencer.elf | grep ttSlot
arm-none-eabi-size build/stm32/release/src/apps/sequencer/sequencer.elf
```

**Expected RAM/flash delta:** ~1,226 B BSS saved (one PatternSlot removed).

**Behavior risk:** Low-Medium. Rollback becomes single-slot only. If both slots are dirtied during the same file parse, only the most recently dirtied slot is restorable. The primary slots (`ttSlot1`/`ttSlot2`) are still accessible concurrently — only the backup safety net shrinks.

**Hardware check:** Save and load Teletype tracks. Test error recovery: introduce a parse error mid-file and verify partial rollback.

**Exit criteria:** Normal file save/load works. Single-slot rollback works. Multi-slot parse failure correctly reports error and restores the last-dirtied slot.

**Rollback:** Restore two separate backup buffers.

---

### Experiment C: Container Size Measurement

**Architecture finding it came from:** Model and Engine Container variants inflate track storage. Actual waste depends on which track types users configure. See `architecture-research-directions.md` Direction 1.

**Hypothesis:** Not a change experiment — this is a measurement-only experiment to quantify exact per-type waste.

**Files:** None (measurement only — add temporary `static_assert(sizeof(T) == N, "")` to existing build units, e.g., `Track.cpp` for model types and `Engine.cpp` for engine types)

**Measurement commands:**
```sh
# Add static_assert to Track.cpp (or any build unit that includes all track headers):
#   static_assert(sizeof(NoteTrack) == EXPECTED, "NoteTrack size");
#   static_assert(sizeof(CurveTrack) == EXPECTED, "CurveTrack size");
#   ... etc for all 7 model and 7 engine types
# Then build and record the assert failures to get actual sizes.

# Alternative: use linker map symbols
arm-none-eabi-nm --print-size --size-sort build/stm32/release/src/apps/sequencer/sequencer.elf | head -50

# Per-type model sizes (add static_assert in each track model header):
# sizeof(NoteTrack), sizeof(CurveTrack), sizeof(MidiCvTrack),
# sizeof(TuesdayTrack), sizeof(DiscreteMapTrack), sizeof(IndexedTrack), sizeof(TeletypeTrack)

# Per-type engine sizes:
# sizeof(NoteTrackEngine), sizeof(CurveTrackEngine), sizeof(MidiCvTrackEngine),
# sizeof(TuesdayTrackEngine), sizeof(DiscreteMapTrackEngine), sizeof(IndexedTrackEngine),
# sizeof(TeletypeTrackEngine)
```

**Expected RAM/flash delta:** 0 (measurement only)

**Behavior risk:** None

**Hardware check:** None (measurement only)

**Exit criteria:** Exact sizeof() for all 7 model types and 7 engine types documented.

**Rollback:** Remove temporary static_asserts.

---

### Experiment C2: Note/Curve Sequence Header Packing

**Architecture finding it came from:** NoteSequence steps are already packed into two `uint32_t` words per step, but per-pattern sequence parameters remain byte/routable-heavy. This was missed by the earlier RAM plan because the audit did not multiply small repeated sequence headers by 17 patterns/snapshots, and did not verify the current `Track::_container` max before ranking model optimizations.

**Hypothesis:** Packing sequence-level parameters behind existing accessors can shrink `NoteSequence` and `CurveSequence` without changing behavior or project-file compatibility. Top-level RAM savings only counts if the largest `Track::_container` arm shrinks; current host probes suggest CurveTrack, not NoteTrack, is the largest model arm.

**Required measurement first:**
```cpp
static_assert(sizeof(NoteSequence::Step) == 8, "record NoteSequence::Step size");
static_assert(sizeof(NoteSequence) == 564, "record NoteSequence size");
static_assert(sizeof(CurveSequence::Step) == 8, "record CurveSequence::Step size");
static_assert(sizeof(CurveSequence) == 592, "record CurveSequence size");
static_assert(sizeof(NoteTrack) == 9612, "record NoteTrack size");
static_assert(sizeof(CurveTrack) == 10092, "record CurveTrack size");
static_assert(sizeof(Track) == 10120, "record Track size");
```

Use intentionally-wrong static assertions on the ARM build if needed to capture exact target sizes from compiler diagnostics. Host sizes above are evidence, not final ARM proof.

**Files:**
- `src/apps/sequencer/model/NoteSequence.h/.cpp` — pack sequence-level parameters; preserve public accessors
- `src/apps/sequencer/model/CurveSequence.h/.cpp` — same treatment if CurveTrack remains the model-container max
- `src/apps/sequencer/model/Routing.h` — reference only; routed params need base+routed semantics equivalent to `Routable<T>`
- `src/apps/sequencer/model/Track.h` — measure container impact

**Packing candidates:**
- `NoteSequence`: `_scale`, `_rootNote`, `_divisor`, `_divisorY`, `_divisorYSource`, `_divisorYTrack`, `_clockMultiplier`, `_resetMeasure`, `_runMode`, `_mode`, `_firstStep`, `_lastStep`, `_noteFirstStep`, `_noteLastStep`, harmony byte fields, and unused `_edited`.
- `CurveSequence`: same sequence-control family plus curve-specific sequence parameters; inspect live fields before editing.

**Compatibility constraint:** Keep current `write()` / `read()` serialized field order and values. This experiment changes in-RAM representation only. Existing project files must load identically.

**Expected RAM/flash delta:** Measurement-first. Local upper bound is tens of bytes per sequence × 17, but top-level `Model` saving is zero if only NoteSequence shrinks while CurveTrack remains the largest `Track::_container` arm. Count success only from `sequencer.size`, `sizeof(Track)`, and `_ZL5model` deltas.

**Behavior risk:** Low-Medium. Runtime access goes through getters/setters, but routed parameters have two live values (`base` and `routed`) and must not collapse to one.

**Hardware check:** Boot; edit Note and Curve sequence settings; route Scale/Root/Divisor/ClockMult/RunMode/FirstStep/LastStep; save/reload a project with differing pattern settings.

**Exit criteria:** ARM `sizeof` and `_ZL5model` deltas recorded; Note and Curve patterns keep independent params across pattern switch, routing, save, reload, and reboot.

**Rollback:** Revert NoteSequence/CurveSequence storage changes.

---

### Experiment D: Symbol and Section Size Measurement

**Architecture finding it came from:** Understanding where RAM goes requires per-symbol measurement. See `architecture-research-directions.md` Direction 6.

**Hypothesis:** Symbol-level measurement confirms or corrects the estimated per-type RAM contributions from the resource-optimization task.

**Files:** None (measurement only)

**Measurement commands:**
```sh
# Per-section summary
arm-none-eabi-size build/stm32/release/src/apps/sequencer/sequencer.elf

# Top 50 RAM symbols
arm-none-eabi-nm --print-size --size-sort build/stm32/release/src/apps/sequencer/sequencer.elf | grep -E "^[0-9a-f]{8} [0-9a-f]{8} " | sort -k2 -rn | head -50

# Section-specific (.bss, .data, .ccmram_bss)
arm-none-eabi-objdump -h build/stm32/release/src/apps/sequencer/sequencer.elf | grep -E "\.bss|\.data|\.ccmram"

# Teletype-specific symbols
arm-none-eabi-nm --print-size --size-sort build/stm32/release/src/apps/sequencer/sequencer.elf | grep -E "tele_tt|scene_state|PatternSlot"
```

**Expected RAM/flash delta:** 0 (measurement only)

**Behavior risk:** None

**Hardware check:** None

**Exit criteria:** Verified or corrected symbol sizes for RoutingEngine, Teletype subsystem, Engine singleton, Model singleton, and UI singleton. Cross-referenced with resource-optimization task estimates.

**Rollback:** None (measurement only).

---

### Experiment E: Task Stack Watermark Measurement

**Architecture finding it came from:** `CONFIG_FILE_TASK_STACK_SIZE` is 4,096 B. Modulove runs at 2,208 B. The `ttSlot` globals were moved off the stack because PatternSlot objects caused overflow. See `savings-plan.md` P13.

**Hypothesis:** The file task stack has measurable headroom. If watermark measurement shows >1.5 KB free, the stack can safely be reduced, saving CCMRAM.

**Files:**
- `src/apps/sequencer/Config.h` — `CONFIG_FILE_TASK_STACK_SIZE`
- Measurement instrumentation in `os/` task creation

**Measurement commands:**
```sh
# In debug build, fill task stacks with known pattern (0xDEADBEEF),
# run full file save/load sequence, then measure highest stack usage.
# Alternative: use FreeRTOS stack watermark API (uxTaskGetStackHighWaterMark)
```

**Expected RAM/flash delta:** Potentially ~1,536 B CCMRAM if stack can be reduced from 4,096 to 2,560.

**Behavior risk:** Medium. Stack overflow on file operations would cause hard fault. Must verify with worst-case file I/O (large Teletype projects, SD card errors).

**Hardware check:** Run full project save/load cycle with Teletype tracks, file errors, and simultaneous engine operation.

**Exit criteria:** Stack watermark shows >1.5 KB free during worst-case file I/O. Stack reduction boots and completes file operations without hard fault.

**Rollback:** Restore `CONFIG_FILE_TASK_STACK_SIZE` to 4096.

---

## Deferred Architecture Ideas

These are architecture-level ideas that do not meet the criteria for RAM recovery experiments (no measurable RAM savings with current approach, or too speculative to hypothesize savings).

1. **Model Container type pool** — Currently all track types pay NoteTrack's ~9,500 B. A type-pooled approach would save RAM for Tuesday/DiscreteMap/Indexed tracks but requires redesigning project serialization, pattern copy/clear, and UI dispatch. Not measurable until the pool design is specified.

2. **Capability-matrix UI dispatch** — Would reduce maintenance friction per new track type but has negligible RAM impact. Only worth pursuing if track type count grows significantly.

3. **Pattern diff/sparse storage** — Could reduce model RAM for Note/Curve if most patterns are at defaults, but requires a complex serialization format and breaks simple value-copy semantics. Too speculative for a concrete experiment.

4. **Active-pattern-only NoteSequence loading** — Could reduce model RAM by loading only the active pattern's NoteSequence from flash/SD on demand. Requires flash latency guarantees that haven't been verified. Very high risk.

5. **Separate ModulationEngine subsystem** — Would give RoutingEngine modulation state an explicit lifecycle (create/destroy shaper state on configuration change). This is an architecture clarification, not a RAM experiment. The RAM savings come from Experiment A (conditional state), which can be done independently.

6. **Capped stateful routing lanes** — Maybe-later P6 variant. Instead of dynamic sparse allocation, use a static state array only for the first N routing lanes, e.g. `TrackStateUnion statefulShaperState[4][8]`. Lanes above N still support stateless shapers but cannot use stateful shapers. After P5, expected savings are ~2,304 B CCMRAM for N=4 or ~1,536 B for N=8. This is a product constraint, not a transparent optimization.

7. **State lifecycle formalization** — Documenting which state is persistent, rebuildable, transactional, compatibility, and mirror. No direct RAM savings, but reduces conceptual debt. See `architecture-research-directions.md` Direction 5.
