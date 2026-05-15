# Savings Plan — RAM Optimization for Teletype-Performer Ecosystem

## Context

XFORMER firmware at **97% RAM** (127,420 / 128 KB). ~4 KB headroom blocks all feature additions. This plan catalogs every identified redundancy, conflict, and wasted allocation across the architecture, with ranked proposals to recover 15-25 KB.

## Verified Size Reference

All measurements verified by `arm-none-eabi-nm` and manual struct analysis. These are inputs to every proposal below.

| Object | Verified Size | File:Line |
|--------|---------------|-----------|
| `TrackState` | **56 bytes** (15 fields + padding) | `RoutingEngine.h:34-50` |
| `RouteState` (full) | **468 bytes** (Target + tracks + shaper[8] + TrackState[8]) | `RoutingEngine.h:30-51` |
| `tele_command_t` | **52 bytes** | `command.h:25-32` |
| `scene_pattern_t` | **138 bytes** | MonitorPage SIZES page 6 |
| `PatternSlot` | **1,226 bytes** | `TeletypeTrack.h:127-152` |
| `scene_state_t` | **4,640 bytes** | MonitorPage SIZES page 6 |
| `TeletypeTrack` | **7,104 bytes** | MonitorPage SIZES page 6 |
| `tele_ops[]` | **1,664 bytes** (416 × 4 byte ptrs) | `op.c / op_enum.h` |
| `ttSlot` globals (×4) | **4,904 bytes** (4 × 1,226) | `FileManager.cpp:628-635` |
| `TeletypeTrackEngine` | **912 B** | MonitorPage SIZES page 6 |
| `Engine::TrackEngineContainer` | **912 B** | MonitorPage SIZES page 6 |
| Next largest engine | **588 B** (`NoteTrackEngine`) | MonitorPage SIZES page 4 |

### Configuration Constants

| Constant | Value | File:Line |
|----------|-------|-----------|
| `CONFIG_TRACK_COUNT` | 8 | `Config.h:54` |
| `CONFIG_ROUTE_COUNT` | 16 | `Config.h:56` |
| `PATTERN_COUNT` | 4 | `state.h:28` |
| `PATTERN_LENGTH` | 64 | `state.h:29` |
| `SCRIPT_MAX_COMMANDS` | 6 | `state.h:30` |

---

## Cross-Reference: resource-optimization/TASK.md

The parallel resource-optimization task (`../../resource-optimization/TASK.md`) provides symbol-level `arm-none-eabi-nm` measurements against Vinx, Mebitek, and Modulove forks.

| Finding | resource-optimization source | Impact on this plan |
|---|---|---|
| RoutingEngine is **7.4 KB** gap — 90% of Engine singleton delta | lines 88, 233-241 | Confirms P5/P6 as #1 priority, adds aggressive P5a variant |
| `ttSlot1-4` are **.bss globals** (4,904 B), not stack locals | lines 104-108, `FileManager.cpp:628-635` | Adds P4b (backup consolidation only) |
| Engine container gate is **TeletypeTrackEngine=912 B** | MonitorPage SIZES page 6 | P7 direct conditional gap is `(912 - 588) × 8 = 2,592 B` CCMRAM; only recoverable with capped/separate Teletype engine residency or TeletypeTrackEngine shrink |
| `tele_ops` table in **.data** = 1,664 B | line 104 | Adds to P14 scope if const-able |
| Stack: file task 4,096 B vs Modulove 2,208 B (~1,888 B gap) | line 258 | Tracks P13 as future research; requires stack watermark measurement before implementation |
| `tele_glyphs/bitmap` in .data = 1,040 B | lines 109, 111 | Adds P14 |
| Model scatter = 4.8 KB gap (no single target) | line 120 | Not addressed here |
| UI is **bigger** in Vinx (30,068 B) than XFORMER (27,244 B) | line 228 | P10/P11 are not XFORMER's bottleneck |
| Top 3 directions after P15/P5: Teletype engine container and transaction/slot semantics remain relevant; Teletype model shrink is not a current container win | current MonitorPage SIZES | `TeletypeTrack=7104` is below `NoteTrack=9544`; `TeletypeTrackEngine=912` equals engine container |
| Model/container pool architecture | `.tasks/core-architecture-optimization/model-pool-decision-table.md` | No-go under current "any track can be any type" semantics; only re-open with explicit simultaneous type caps |

---

## Problem Catalog

### Redundancies

| ID | Pattern | Location | Cost |
|---|---|---|---|
| R1 | RouteState mirrors Route's per-track shaper config | `RoutingEngine.h:31-51` vs `Routing.h:822` | _shaper[8] copied (16 B × 16 routes = 256 B) |
| R2 | Pattern data triple-stored | `TeletypeTrack` — `_state.patterns[4]` + `_patternSlots[0].patterns[4]` + `_patternSlots[1].patterns[4]` | 544 B × 3 = 1,632 B per Teletype track |
| R3 | Script data in both PatternSlot & scene_state_t | `PatternSlot.slotScript[6]` + `metro[6]` mirrored in `_state.scripts[S3/M]` | 312 B × 2 = 624 B per Teletype track |
| R4 | `#ifndef DELAY_SIZE` double-guard | `state.h:22-26` | Copy-paste artifact, zero runtime cost |
| R5 | Static PatternSlot globals in FileManager | `FileManager.cpp:628-635` — anonymous namespace: `ttSlot1` / `ttSlot2` / `ttSlot1Backup` / `ttSlot2Backup` | 4 × 1,226 B = **4,904 B .bss** |

### Conflicts

| ID | Pattern | Location | Cost |
|---|---|---|---|
| C1 | Engine Container inflated by TeletypeTrackEngine | `Engine.h:41` — `Container<..., TeletypeTrackEngine>` sizes all 8 slots to ~904 B | (904 - 675) × 8 = **1,832 B direct** (verified 904 is 8-byte aligned; no alignment cascade mechanism identified) |
| C2 | RouteState::TrackState[8] allocated unconditionally for all 16 routes | `RoutingEngine.h:51` — full union-of-all-shaper state per track × 16 routes | 56 B × 8 × 16 = **7,168 B**, mostly zero for non-shaper routes |
| C3 | Pattern data in both Track union AND PatternSlot | `TeletypeTrack` carries _state.patterns + slot.patterns simultaneously | 1,088 B per Teletype track (one slot mirror) |

### Weird Patterns

| ID | Pattern | Location | Notes |
|---|---|---|---|
| W1 | 38+ page objects eagerly constructed in CCMRAM | `Ui.h:75` → `Pages.h:58-173` | ~2-3 KB of ~27 KB Ui singleton (not a bottleneck — Vinx is larger) |
| W2 | 8bpp framebuffer for 4bpp OLED | `Ui.h:61` — `_frameBufferData[256*64]` = 16,384 B | LCD compresses to 8 KB for SPI |
| W3 | `const_cast` in const methods | `TeletypeTrack.cpp:331,612` | Hidden mutation inside const methods |
| W4 | `mutes` as `bool[8]` not `uint8_t` bits | `state.h:104` | 7 wasted bytes per track × 8 tracks = 56 B |
| W5 | `update(0.f)` called with dt=0 | `Engine.cpp:174` | No-op for state, triggers `updateTrackOutputs()` routing |

### Stale/Dead

| ID | Pattern | Location | Notes |
|---|---|---|---|
| S1 | 22 teletype ops files compiled but never linked | `teletype/src/ops/*.c` | Source clutter, no RAM cost |
| S2 | Grid structures allocated in LITE mode | `scene_grid_t` inside `scene_state_t` | Already minimal via dimension defines (~10-20 B) |

---

## Proposal Definitions

Each proposal: **Savings**, **Effort** (Low/Medium/High/Very High), **Risk** (None/Low/Medium/High/Very High), **Scope**.

### Tier 1 — Quick Wins

#### P2 — Pack `mutes` into `uint8_t` bitfield

**What**: Replace `bool mutes[8]` (8 bytes) with `uint8_t mutes` (1 byte). Update `ss_get_mute()` / `ss_set_mute()` to use bit ops. Only accessed through these accessor functions — no address-of taken.

**Savings**: 7 B per track × 8 tracks = **56 B**  
**Effort**: Low — grep `mutes[i]` in `state.c`, replace with bit ops  
**Risk**: None  

**Files**: `teletype/src/state.h`, `teletype/src/state.c`

#### P4 — Reduce DELAY_SIZE from 16 to 8

**What**: Change `#define DELAY_SIZE 16` to `#define DELAY_SIZE 8` in `teletype_config.h`. Each delay slot = 52 B (command) + 2 B (time) + 1 B (origin_script) + 2 B (origin_i) + 2 B (fparam1) + 2 B (fparam2) ≈ 61 B. 8 fewer slots.

**Savings**: 8 × 61 B = 488 B per track × 8 = **~3,904 B**  
**Effort**: Low — one `#define` change  
**Risk**: Low — delay scripts rarely exceed 4-6 pending commands in practice  

**Files**: `teletype/src/teletype_config.h`

#### P4b — Consolidate ttSlot BACKUP buffers only

**What**: `FileManager.cpp:628-635` has 4 static `PatternSlot` globals. The two primaries (`ttSlot1` / `ttSlot2`) are accessed **concurrently** during file parsing — `#SLOT 1` / `#SLOT 2` directives can switch between them. The two backup slots (`ttSlot1Backup` / `ttSlot2Backup`) are used for atomic rollback on parse failure.

Status: **deferred research**. Do not treat this as a low-risk one-file cleanup. Current `readTeletypeTrack()` snapshots both slots before clearing and restores both slots on failure. Replacing the two backups with one shared buffer changes transaction semantics unless the read/apply flow is redesigned around a clear single-slot rollback contract.

Simpler alternative: keep both backups but acknowledge this savings is currently unreachable.

**Savings**: 1 × 1,226 B = **~1,226 B** (consolidate 2 backups → 1)  
**Effort**: Medium — requires rollback semantics redesign, not just changing the globals  
**Risk**: Medium — multi-slot parse failure can corrupt or partially restore state if the transaction boundary is wrong  

**Files**: `src/apps/sequencer/model/FileManager.cpp`

#### P14 — Move font data to flash

**What**: `tele_glyphs` (760 B) and `tele_bitmap` (280 B) in `tele.h` are declared `static` but not `const`. Adding `const` moves them from `.data` to `.rodata` (flash on STM32F4). Not mutating them to `.rodata` confirms flash residency.

**Savings**: 760 + 280 = **~1,040 B** from .data  
**Effort**: Low — add `const` qualifier  
**Risk**: None — read-only at runtime; flash access is zero-wait at 168 MHz with ART accelerator  

**Files**: `src/core/gfx/fonts/tele.h`

#### P14b — Optionally move tele_ops[] to flash

**What**: `tele_ops[]` is a `const tele_op_t *` array (1,664 B). If it's currently mutable, mark `const` to move to flash.

**Savings**: **~1,664 B** from .data  
**Effort**: Low — verify const-ability, add `const` qualifier  
**Risk**: None — ops table is fixed at compile time  

**Files**: `teletype/src/ops/op.c`

### Tier 2 — Medium Effort, High Savings

#### P5 — Union-based TrackState by shaper type

**What**: Replace monolithic `TrackState` (~56 B per instance, 128 instances = 16 routes × 8 tracks) with a per-TrackState union keyed by the active shaper. **Key detail**: each TrackState within the same RouteState can have a **different** active shaper (track 0 = ProgressiveDivider, track 1 = Location, etc.). The union + discriminant must be per-TrackState, not per-RouteState.

Each variant ~16-24 B vs current ~56 B. Non-shaper tracks (None, Crease, TriangleFold, VcaNext) store nothing.

**Savings**: ~32 B × 128 = **~4,096 B**  
**Effort**: Medium — per-TrackState union with discriminant; update `updateSinks()` to access through union members  
**Risk**: Low — functions already branch on `route.shaper(trackIndex)`

**Files**: `src/apps/sequencer/engine/RoutingEngine.h`, `RoutingEngine.cpp`

#### P5a — Aggressive: Strip RouteState TrackState entirely

**What**: Eliminate `TrackState[8]` from RouteState entirely. Match Vinx: `RouteState = { Target target, uint8_t tracks }` ≈ 4 B. Per-track shaping becomes stateless — computed each frame from source, not accumulated.

**Breaks 5 of 9 shapers** (verified):
| Shaper | State fields used | Purpose |
|--------|-------------------|---------|
| ProgressiveDivider | `progCount`, `progThreshold`, `progSign`, `progOut`, `progOutSlewed`, `progHold` | Count-up oscillator/divider |
| Location | `location`, `freqAcc`, `freqSign`, `ffHold` | Slew-based positioning |
| Envelope | `envelope` | Attack/decay accumulator |
| Activity | `activityPrev`, `activityLevel`, `activitySign`, `actHold` | Hysteresis with hold timers |
| FrequencyFollower | `freqAcc`, `freqSign`, `ffHold` | Frequency tracking |
| **Stateless (unaffected)**: Crease, TriangleFold, VcaNext, None | | |

**Savings**: **~7,488 B** (16 routes × 468 B → ~4 B)  
**Effort**: Low-Medium (code removal)  
**Risk**: **HIGH** — modifies shaper accumulator semantics. Only safe if these 5 shapers are not actively used.

**Files**: `src/apps/sequencer/engine/RoutingEngine.h`, `RoutingEngine.cpp`

#### P6 — Skip TrackState for non-per-track routes

**What**: Routes targeting global targets (Tempo, Swing, Play, Record, CvRouteScan, CvRouteRoute, Mute, Fill, Pattern) don't need per-track shaping. Identified by `Routing::isPerTrackTarget()`. Conditional allocation: only per-track routes carry TrackState[8].

**Savings**: ~56 B × 8 × 6 (non-per-track routes) = **~2,688 B**  
**Effort**: Medium — conditional TrackState allocation in RouteState, branching in `updateSinks()`  
**Risk**: Low — `isPerTrackTarget()` already classifies targets; routes are type-checked at model level

**Files**: `src/apps/sequencer/engine/RoutingEngine.h`, `RoutingEngine.cpp`

#### P7 — Extract TeletypeTrackEngine from Container variant

**What**: TeletypeTrackEngine (912 B) inflates all 8 container slots. Remove it from the `Container<...>` type only if the next-largest engine is smaller and Teletype engine residency can be capped.

**Savings**: Conditional. Current ARM probes show `TeletypeTrackEngine=912 B`, `Engine::TrackEngineContainer=912 B`, and `NoteTrackEngine=588 B`; direct container gap is `(912 - 588) × 8 = 2,592 B` CCMRAM. If Performer preserves the current behavior where any/all 8 tracks can be Teletype, separate storage for 8 TeletypeTrackEngines cancels the container saving.
**Note**: No alignment cascade mechanism identified in Engine.h member ordering. The old 904-vs-675 estimate is superseded by the MonitorPage measurements.
**Effort**: Medium — separate TTE storage array, update `updateTrackSetups()`  
**Risk**: Medium-High — track mode switch lifecycle plus product semantics around maximum live Teletype tracks. Future research only until the cap/semantics question is decided.

**Files**: `src/apps/sequencer/engine/Engine.h`, `Engine.cpp`

#### P8 — Share pattern backup between slots

**What**: Each PatternSlot carries `patterns[4]` (544 B) AND `_state.patterns[4]` (544 B) — 3 copies (1,632 B). Replace one slot's patterns with a shared backup.

**Savings**: ~544 B per Teletype track × 1 = **~544 B**  
**Effort**: Medium — changes `applyPatternSlot()` / `syncToActiveSlot()` in `TeletypeTrack.cpp`  
**Risk**: Medium — save/restore ordering correctness. Verify `copyPatternSlot()` concurrency.

**Files**: `src/apps/sequencer/model/TeletypeTrack.h`, `TeletypeTrack.cpp`

#### P13 — Trim file task stack (future research)

**What**: `CONFIG_FILE_TASK_STACK_SIZE` is 4,096 B. Modulove runs at 2,208 B with the same FatFs + SDIO driver.

**Must verify**: The `ttSlot*` globals were **moved out of stack** into .bss at `FileManager.cpp:628-635` specifically because large PatternSlot objects caused stack overflow on the file task. This means stack was already tight before the move. Need stack watermark measurement in debug build before reducing.

**Status**: deprioritized to future research. Do not keep this in the active resource-optimization implementation queue until hardware watermark data exists for worst-case save/load, Teletype file parse, malformed Teletype file, and relevant SD failure flows.

**Savings**: 4,096 → 2,560 = **~1,536 B in CCMRAM** (conservative)  
**Effort**: Low — one `#define` change  
**Risk**: **Medium** — possible stack overflow. Requires functional verification.

**Files**: `src/apps/sequencer/Config.h`

### Tier 3 — Major Refactors

#### P9 — Pack `tele_command_t` tags into bitfield

**What**: `tele_command_t` = 52 B. Tags stored as `uint8_t[16]` (16 B) where `tele_word_t` has only 8 values (3 bits). Pack into `uint32_t` (4 B, 2 bits per tag). Saves 12 B × N instances per track.

**Savings**: 12 B × ~84 instances × 8 = **~8,064 B**  
**Effort**: High — every op handler, Ragel parser, serialization function  
**Risk**: High — fundamental representation change; trades RAM for compute (bit extraction per access)

**Files**: `teletype/src/command.h`, `state.h`, `ops/*.c`, Ragel parser, `TeletypeTrack.h`

#### P10 — Lazy page construction

**What**: Replace `Pages` aggregate (38+ objects eager-constructed) with `Page*` lazily initialized. Rarely-used pages never allocate.

**Savings**: ~2-3 KB (not a bottleneck — XFORMER UI at 27,244 B is smaller than Vinx at 30,068 B)  
**Effort**: High — factory pattern, PageManager lifecycle changes  
**Risk**: Medium — navigation latency; fragmentation with dynamic alloc on bare metal

**Files**: `src/apps/sequencer/ui/Ui.h`, `Ui.cpp`, `pages/Pages.h`, `PageManager.h`, `PageManager.cpp`

#### P11 — Native 4bpp framebuffer

**What**: OLED supports 4 shades (4bpp). Current 8bpp buffer (16,384 B CCMRAM) → 4bpp (8,192 B).

**Savings**: **8,192 B** CCMRAM  
**Effort**: High — FrameBuffer, Canvas, font rasterizer, blit operations  
**Risk**: High — pixel addressing changes everywhere; visual regression possible

**Files**: `src/core/gfx/FrameBuffer.h`, `FrameBuffer.cpp`, `Canvas.h`, `Canvas.cpp`, `src/platform/stm32/drivers/Lcd.cpp`

#### P12 — Flash-backed model with RAM cache

**What**: Page inactive sequences from Model to SD card/flash, loading on demand.

**Savings**: Potentially 20-60 KB  
**Effort**: Very High  
**Risk**: Very High — flash latency, real-time guarantees, write amplification  

**Files**: Entire model layer, Engine, file I/O, UI page reads

---

## Adversarial Findings Summary

Key corrections from adversarial review of v1:

1. **P1/P3 REMOVED (compile-blocking)**: `ss_init()` at `state.c:24` calls `ss_grid_init()` which accesses `ss->grid`. Cannot `#if` out `scene_grid_t` from `scene_state_t` without compilation failure. Grid is already minimal in LITE mode (dimension defines reduce it).

2. **P4b CORRECTED (backups only)**: `ttSlot1`/`ttSlot2` primitives accessed concurrently by `parseSlot()` lambda at `FileManager.cpp:791` — `SLOT 1`/`SLOT 2` directives can switch mid-file. Only the backup slots can be consolidated. Savings halved from 2,452 B to 1,226 B.

3. **P7 REVISED (no cascade)**: TeletypeTrackEngine = 904 B, which is 8-byte aligned (904 % 8 = 0). No alignment cascade mechanism identified in Engine.h member ordering (lines 255-257). The 4,304 B "cascade" figure from resource-optimization task requires source verification.

4. **P5 REVISED (per-TrackState union)**: Within a single RouteState, TrackState[0] and TrackState[1] can have different shaper types (track 0 = ProgressiveDivider, track 1 = Location). Union + discriminant must be per-TrackState, not per-RouteState.

5. **P5a RISK UPGRADED (breaks 5/9 shapers)**: Verified ProgressiveDivider, Location, Envelope, Activity, FrequencyFollower all depend on accumulated state. Making them stateless changes behavior.

6. **P13 TEMPERED (needs measurement)**: `ttSlot*` globals were moved OFF stack (into .bss) because large PatternSlot objects caused overflow. Stack reduction requires watermark measurement before attempting.

7. **P15 ADDED (missed sequence-header candidate)**: NoteSequence and CurveSequence steps are already packed, but sequence-level parameters/header fields are still repeated across 17 patterns/snapshots. Host sizeof probe: `NoteSequence=564 B`, `CurveSequence=592 B`, `NoteTrack=9,612 B`, `CurveTrack=10,092 B`, `Track=10,120 B`. Because CurveTrack appears to size `Track::_container`, note-only packing may not reduce top-level RAM; measure ARM sizes first and pack Note/Curve headers together if pursuing this as RAM recovery.

---

## Recommended First Pass

| Phase | Proposals | Verified Savings | Cumulative | Risk |
|---|---|---|---|---|
| **Phase 1 (Safe)** | P2/P4 internal Teletype cleanup + P14 (1,040 B) + P14b (1,664 B) | **2,704 B measured .data** | ~2.7 KB | None-Low |
| **Phase 2 (Medium)** | P5 complete (4,096 B CCMRAM), P15 complete (4,760 B `.bss`) | **8,856 B measured across SRAM/CCMRAM** | ~11.6 KB total with Phase 1 | Low-Medium |
| **Phase 3 (Risk)** | P5a (7,488 B, only by explicit decision) + P8 (544 B) | **~8,032 B** | conditional | Medium-High |

**Phase 1 alone**: 2,704 B recovers from .data → RAM ~124.7 KB (~95.2%).  
**P5 status**: complete, hardware-verified, saves 4,096 B CCMRAM.  
**P15 status**: complete, saves 4,760 B `.bss`; post-P15 MonitorPage shows `Track=9560`, `NoteTrack=9544`, `CurveTrack=9480`, `Model=88072`. Model storage is done for now.

Recommended order:
1. **P14+P14b** (2.7 KB measured) plus P2/P4 internal cleanup — completed  
2. **P5** — completed and hardware-verified  
3. **P15 CurveSequence-first** — completed; model container max is now NoteTrack
4. **Post-P15 SRAM symbol audit** — required before choosing another implementation target
5. **P4b** — research/spec next; rollback semantics are not simple
6. **P6** — deferred research; sparse/capped shaper state needs a separate design
7. **P7** — future research only; useful only with a capped live Teletype engine pool
8. **P8** (0.5 KB, 2 files, ~2 days) — possible final pattern savings after transaction semantics are understood
9. **P13** — future research only; requires hardware stack watermark evidence before implementation

---

## Reference

- `TASK.md` — Task definition for this workstream
- `OVERVIEW.md` — Full 6-layer pipeline architecture
- `../../resource-optimization/TASK.md` — Symbol-level RAM budget analysis with fork comparisons
- `build/stm32/release/src/apps/sequencer/sequencer.size` — Section size measurements
- `build/stm32/release/src/apps/sequencer/sequencer.list` — Symbol-level RAM mapping
