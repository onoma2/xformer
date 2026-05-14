# Resource Optimization — RAM & Flash Budget Recovery

## Overview

XFORMER firmware is at 97% RAM capacity (127,420 bytes out of 128 KB) with only ~4 KB headroom. This is insufficient margin for any feature additions and likely causes hard faults on boot when stack frames or dynamic allocations exceed available RAM. The 62-94 KB larger flash binary (vs Vinx baseline) reflects XFORMER-unique track types but also suggests optimization opportunities.

**Goal:** Recover at least 15-20 KB of RAM to provide safe headroom for future features, and reduce flash usage where possible.

## Current Implementation State (2026-05-14)

- Active branch in this workspace: `refactor/resouce-optimization`.
- Phase 1 code is present: Teletype mute bit-packing, `DELAY_SIZE=8`, Teletype font `const`, and `tele_ops` flash residency.
- Current measured build artifact: `.data=6,316 B`, `.bss=118,400 B`, `.ccmram_bss=58,232 B`, total runtime RAM `.data + .bss = 124,716 B`.
- Phase 1 measured saving: `.data` 9,020→6,316 = 2,704 B.
- Hardware proof is not recorded here yet; treat Phase 1 as build-size verified, not fully hardware-accepted.
- No Phase 2 code exists yet. RoutingEngine P5/P6, P4b backup consolidation, P15 sequence-header packing, stack watermark/trim, and container safety fixes are still analysis/plans only.

## Restructured Implementation Queue

1. **Phase 2 measurement gate:** add temporary ARM `sizeof` probes for Track, TrackEngine, NoteSequence, CurveSequence, and RoutingEngine state; remove probes after recording results.
2. **Phase 2B first implementation:** RoutingEngine P5/P6 state compaction. This is the highest-value current implementation target because it attacks actual CCMRAM waste without product caps or save-format changes.
3. **Phase 2A low-medium risk:** Teletype file backup consolidation (`ttSlot1Backup`/`ttSlot2Backup`) after rollback semantics are rechecked.
4. **Phase 2C measurement-first:** Note/Curve sequence-header packing only if ARM probes show `Track`, `_ZL5model`, and `.bss` can shrink; note-only packing is not a claimed RAM win while CurveTrack remains the max container arm.
5. **Phase 3B:** file task stack trim only after hardware watermark evidence.

Do not start a model-pool/container-pool implementation under current semantics. `.tasks/core-architecture-optimization/model-pool-decision-table.md` records the decision: the current `Container<T...>` is the best static representation for "any track can be any type"; per-type pools save RAM only with explicit simultaneous track-type caps.

## Current Resource Budget

### Measured from `build/stm32/release` (fresh build)

| Resource | XFORMER | Vinx | Mebitek | Modulove | Diff (Xf vs V) | Chip Limit |
|---|---|---|---|---|---|---|---|
| **Total RAM** (.data+.bss) | 127,420 (97%) | 113,308 (86%) | 113,736 (87%) | 110,560 (85%) | **+14,112** | 128 KB |
| **CCMRAM** (.ccmram_bss) | 58,232 | 48,516 | 49,844 | 49,528 | **+9,716** | 64 KB |
| **SRAM** (.data+.bss - ccmram) | 69,188 | 64,792 | 61,032 | **+4,396** | — |
| **Flash .text** | 760,732 | 665,776 | 589,232 | 351,140 | **+94,956** | 1 MB |
| **Flash .ARM.extab** | 7,936 | 10,248 | 5,364 | **-2,312** | — |
| **Flash .ARM.exidx** | 11,488 | 12,008 | 6,856 | **-520** | — |
| **Binary size** | 789,252 | 694,528 | 370,064 | **+94,724** | — |

> - **Vinx**: "closest fork" — shares Note, Curve, Arp, Stochastic, Logic tracks; adds Chaos/Entropy/Generator features. 86% RAM.
> - **Mebitek**: adds Logic, Stochastic, Arp tracks plus UI shortcuts. 87% RAM, 615 KB flash.
> - **Modulove**: lightweight — 16 tracks (Note+Curve+MIDI/CV only), adds LFOs. 85% RAM, 370 KB flash.

### Memory Map (STM32F405RGTx)

```
Region    Base        Size      Usage
SRAM      0x20000000  128 KB    .data + .bss = ~67 KB
CCMRAM    0x10000000   64 KB    .ccmram_bss = ~57 KB
FLASH     0x08000000    1 MB    .text + rodata = ~760 KB
```

### RAM Breakdown Comparison

| .bss Component | XFORMER | Vinx | Mebitek | Modulove | Delta (Xf-V) |
|---|---|---|---|---|---|---|
| .bss | 118,400 | 106,880 | 107,472 | 103,856 | **+11,520** |
| .data | 9,020 | 6,428 | 6,264 | 6,656 | **+2,592** |
| .ccmram_bss | 58,232 | 48,516 | 49,844 | 49,528 | **+9,716** |

The 11,520 byte .bss gap likely comes from:
- **Unique track engines:** TuesdayTrackEngine, DiscreteMapTrackEngine, IndexedTrackEngine, TeletypeTrackEngine — each has model state arrays (sequences, buffers)
- **Unique model data:** TuesdaySequence, DiscreteMapSequence, IndexedSequence — step arrays per track
- **Harmony system:** HarmonyEngine with voicing/inversion state

The 9,716 byte CCMRAM gap likely comes from:
- **Larger audio/gate buffers:** Teletype bridge or additional DMA buffers
- **Additional FreeRTOS task stacks** for unique subsystems

### Key Insight: Modulove Comparison

Modulove has **16 tracks** (double XFORMER's 8) and additional features (LFO modulators, microtiming) yet uses **less total RAM** (110,560 bytes vs 127,420 bytes) and **half the flash** (370 KB vs 789 KB) than XFORMER. This confirms the RAM pressure is not from track count but from XFORMER-unique subsystems — particularly the **Teletype, Tuesday, Discrete, Indexed track types and Harmony engine**, which each carry their own model/engine/sequence state per track regardless of how many tracks are in use.

## Reference File Locations

### XFORMER binaries
- `build/stm32/release/src/apps/sequencer/sequencer.size` — section sizes
- `build/stm32/release/src/apps/sequencer/sequencer.bin` — 789,252 bytes
- `build/stm32/release/src/apps/sequencer/sequencer.list` — full symbol map
- `src/platform/stm32/stm32f405.ld` — linker script (FLASH=1MB, RAM=128K, CCMRAM=64K)

### Vinx baseline
- `temp-ref/vinx-performer/build/stm32/release/src/apps/sequencer/` — pre-built UPDATE.DAT (726,356 bytes)
- Fresh build: `temp-ref/vinx-performer/build/stm32/release/src/apps/sequencer/sequencer.size` (665,776 .text, 694,528 binary)
- `temp-ref/vinx-performer/build/stm32/release/src/apps/sequencer/sequencer.list` — full symbol map

### Modulove baseline
- `temp-ref/modulove-performer/build/stm32/release/src/apps/sequencer/` — fresh build
- `temp-ref/modulove-performer/build/stm32/release/src/apps/sequencer/sequencer.size` (351,140 .text, 370,064 binary)
- `temp-ref/modulove-performer/build/stm32/release/src/apps/sequencer/sequencer.list` — full symbol map

### Mebitek baseline
- `temp-ref/mebitek-performer/build/stm32/release/src/apps/sequencer/` — fresh build
- `temp-ref/mebitek-performer/build/stm32/release/src/apps/sequencer/sequencer.size` (589,232 .text, 615,056 binary)
- `temp-ref/mebitek-performer/build/stm32/release/src/apps/sequencer/sequencer.list` — full symbol map

## Symbol-Level RAM Analysis

### Three Broad Optimization Directions

Every byte of the ~14 KB RAM gap between XFORMER and Vinx falls into one of three buckets:

| # | Direction | RAM | Section | Root Cause |
|---|---|---|---|---|
| **1** | **RoutingEngine** | **~7.4 KB** | .ccmram | XFORMER's `RouteState` has per-track shaping state (56 B × 8 tracks = 448 B per route × 16 routes = 7,168 B) plus cvRotate/lastReset arrays (48 B). Vinx's RouteState is just `{Target, tracks}` = 2 B. This is 90% of the Engine gap. |
| **2** | **Teletype standalone** | ~6.9 KB | .bss+.data | Teletype subsystem: ops table (1,664 B), slot state + backups (4 × 1,226 B = 4,904 B), font data (760+280 B), line buffer (256 B). |
| **3** | **Model singleton** | ~4.8 KB | .bss | Distributed across HarmonyEngine, ClipBoard::_container (Pattern with Teletype::PatternSlot), UserSettings vector overhead, Tuesday/Discrete/Indexed sequence metadata. |

> Note: Earlier analysis incorrectly attributed the Engine gap to the container. The actual container gap is only **~1,832 B** (TeletypeTrackEngine 904 B vs ArpTrackEngine 675 B × 8). The RoutingEngine is **7.4× larger**.

Everything else (UI, task stacks, clock/tempo state, Engine sub-objects) is comparable or smaller across all four forks. The three directions above fully account for the measured gap.

`arm-none-eabi-nm --print-size --size-sort` run on all 4 built ELF binaries. Symbols categorized as XFORMER-only (not present in any other fork) or over-sized in XFORMER (>=128 B bigger than the max of all other forks).

### XFORMER-Only Symbols (>= 256 bytes each)

These account for the unique subsystem cost:

| Size | Section | Symbol | Identity |
|---|---|---|---|
| 1,664 B | .data | `tele_ops` | Teletype operations table |
| 1,226 B | .bss | `ttSlot1` | Teletype slot 1 state |
| 1,226 B | .bss | `ttSlot2` | Teletype slot 2 state |
| 1,226 B | .bss | `ttSlot1Backup` | Teletype slot 1 undo backup |
| 1,226 B | .bss | `ttSlot2Backup` | Teletype slot 2 undo backup |
| 760 B | .data | `tele_glyphs` | Teletype font glyphs |
| 560 B | .bss | `hid_device` | USB HID device struct |
| 280 B | .data | `tele_bitmap` | Teletype font bitmap |
| 256 B | .bss | `ttLineBuffer` | Teletype line buffer |
| **~6.9 KB** | | **Total unique XFORMER** | |

### Symbols Over-Sized in XFORMER (>= 128 B bigger than any other fork)

| Diff | Section | Symbol | XFORMER | Max Other | Identity |
|---|---|---|---|---|---|
| +6,136 B | .ccmram | `_ZL6engine` | 15,588 B | 9,452 B (Modulove) | Engine singleton — 4 extra track engines (Teletype, Tuesday, Discrete, Indexed) |
| +4,780 B | .bss | `_ZL5model` | 92,836 B | 88,056 B (Vinx) | Model singleton — Harmony voicing + extra track model data |
| +180 B | .bss | `_ZL4usbh` | 188 B | 8 B | USB host struct (minor, likely from USB keyboard support) |

**Engine breakdown**: Vinx/Mebitek = 5,400 B, Modulove = 9,452 B, XFORMER = 15,588 B. Vinx has the same core track count minus the 4 XFORMER-unique tracks. The 10 KB gap (15,588 - 5,400 = 10,188 B) maps directly to Teletype + Tuesday + Discrete + Indexed engines.

**Model breakdown**: XFORMER = 92,836 B, Vinx = 88,056 B, Mebitek = 88,052 B, Modulove = 87,204 B. The 4.8 KB XFORMER gap is model scatter plus container-sized sequence tracks; exact attribution needs ARM `sizeof` probes before ranking model changes.

### Track::_container — Not a Bottleneck (Correction, With New Follow-Up)

Earlier analysis incorrectly claimed the `Track::_container` variant was a ~12 KB fork gap. In reality, all forks are dominated by large step-sequence track models, so the container explains little of the XFORMER-vs-Vinx delta. A follow-up host size probe also corrected the local max-type assumption: current XFORMER appears CurveTrack-sized, not NoteTrack-sized.

```cpp
// XFORMER Track::_container — host probe suggests max is CurveTrack (~10,092 B)
Container<NoteTrack, CurveTrack, MidiCvTrack,
          TuesdayTrack, DiscreteMapTrack, IndexedTrack, TeletypeTrack>

// Vinx Track::_container — large Note/Curve-style sequence tracks also dominate;
// exact max requires fork-specific sizeof verification.
Container<NoteTrack, CurveTrack, MidiCvTrack,
          StochasticTrack, LogicTrack, ArpTrack>
```

| Track Type | Estimated Size | Why |
|---|---|---|
| NoteTrack | 9,612 B host probe | 17 NoteSequence = 17 × (Step[64]×8 B + ~52 B header/state) + track-level members. ARM verification still required. |
| TeletypeTrack | ~2,800 B | scene_state_t (1,226 B) + 2× PatternSlot (~1,500 B) + misc |
| TuesdayTrack | ~500 B | 17 TuesdaySequence (params only, no step array) + playMode |
| **CurveTrack** | **10,092 B host probe** | 17 CurveSequence = 17 × (Step[64]×8 B + ~80 B header/state) + track-level members. Current likely `Track::_container` max. |
| DiscreteMapTrack | ~1,800 B | 32 stages × ~50 B + 17 sequence metadata |
| IndexedTrack | ~4,000 B | 17 IndexedSequence × ~232 B each (Step[48] × 4 B + params) |

Since both forks' containers are sized by large step-sequence tracks, the Track::_container produces **virtually zero fork gap**. Follow-up local size probes found that the current XFORMER model container is probably sized by CurveTrack, not NoteTrack:

```text
NoteSequence::Step = 8 B
NoteSequence       = 564 B
CurveSequence::Step = 8 B
CurveSequence       = 592 B
NoteTrack          = 9,612 B
CurveTrack         = 10,092 B
Track              = 10,120 B
```

This corrects the earlier missed RAM candidate: NoteSequence step data is packed, but the per-pattern NoteSequence header is still about 52 B beyond its 512 B step array. Packing that header is locally legitimate, but **note-only packing may not reduce top-level model RAM while CurveTrack remains the largest container arm**. The immediate experiment should therefore measure and, if worthwhile, pack NoteSequence and CurveSequence sequence-parameter/header storage together.

### Total Gap Accounting

| Source | RAM | Location |
|---|---|---|
| **RoutingEngine** (per-track shaping state × 16 routes) | **~7.4 KB** | .ccmram |
| Teletype subsystem (ops + slots + backups + glyphs + line buffer) | ~6.9 KB | .bss + .data |
| Model singleton (scattered: HarmonyEngine, ClipBoard, UserSettings, sequence metadata diffs) | ~4.8 KB | .bss |
| Engine container (TeletypeTrackEngine 904 B vs ArpTrackEngine 675 B × 8) | ~1.8 KB | .ccmram |
| USB HID device struct | 0.5 KB | .bss |
| **Total explained** | **~21.4 KB** | |
| **Actual measured gap** | **~14 KB** | XFORMER 127,420 vs Vinx 113,308 |

> The 7 KB over-count is from Vinx having extra features XFORMER lacks (Stochastic/Logic/Arp track types, chaos/entropy subsystems, knobPad16, extra settings, larger UI). The RoutingEngine alone accounts for more than the entire measured gap when Vinx's extras are subtracted.

### Key Takeaway

**The RoutingEngine is the single biggest RAM problem — not the Engine container.** The RouteState in XFORMER has per-track shaping state (56 B × 8 tracks = 448 B per route × 16 routes = 7,168 B) that Vinx doesn't have at all. That's over half the total gap.

Optimization priority:
1. **RoutingEngine** (.ccmram, ~7.4 KB): Reduce RouteState to Vinx's minimalist version (`{Target, tracks}` = 2 B), or make per-track shaping state conditional/allocated. Single highest-ROI change.
2. **Teletype standalone** (.bss+.data, ~6.9 KB): Slot backups could be shared (saving 2,452 B).
3. **Sequence header packing** (.bss/model, measurement-first): NoteSequence and CurveSequence already pack steps, but their per-pattern headers are still byte/routable-heavy and repeated 17 times per track type. Note-only packing may save zero top-level RAM if CurveTrack remains the max container arm; pack/measure both.
4. **Engine container** (.ccmram, ~1.8 KB): TeletypeTrackEngine inflates container slots — worth fixing but smaller impact.
5. **Model scatter** (.bss, ~4.8 KB): Lowest priority — many micro-tweaks needed.

### Track Engine Container Contents (Per Fork)

The Engine singleton's `_trackEngineContainers` is a `std::array<Container<Types...>, CONFIG_TRACK_COUNT>` — a union-like variant where each slot is as large as the **biggest** type in the template list. This is the single largest RAM driver within the Engine.

| Fork | Types in Container | Max Type | Per-Track Slot | Array Total |
|---|---|---|---|---|
| **XFORMER** | Note, Curve, MidiCv, Tuesday, DiscreteMap, Indexed, **Teletype** | TeletypeTrackEngine (~904 B with vtable) | **~904 B** | **~7,232 B (7.1 KB)** |
| **Vinx** | Note, Curve, MidiCv, Stochastic, LogicTrack, Arp | ArpTrackEngine (~675 B) | **~675 B** | **~5,400 B (5.3 KB)** |
| **Mebitek** | Note, Curve, MidiCv, Stochastic, LogicTrack, Arp | ArpTrackEngine (~675 B) | **~675 B** | **~5,400 B (5.3 KB)** |
| **Modulove** | Note (±Curve ±MidiCv via `#ifdef`) | NoteTrackEngine or CurveTrackEngine | **~590 B** | **~9,440 B (9.2 KB) × 16 tracks** |

**TeletypeTrackEngine byte-accurate breakdown (~904 B):**

| Component | Size | Items |
|---|---|---|
| TrackEngine base (vtable + 5 refs) | 24 B | vtable ptr, &_engine, &_model, &_track, &_trackState, _linkedTrackEngine |
| _teletypeTrack ref | 4 B | Reference to TeletypeTrack model |
| 7 bools + _manualScriptIndex | 8 B | boot/activity/clock/metro/init/geode flags |
| _metroPeriodMs + _cachedPattern | 6 B | int16 + int |
| 8 floats + _clockTickCounter | 36 B | _tickDurationMs, _pulseClockMs, _metroRemainingMs, _activityCountdownMs, _tickRemainderMs, _geodeFreePhase, _geodePhaseOffset, _geodeBarSeconds, _clockTickCounter |
| 2 doubles (8-byte aligned) | 16 B | _clockRemainder, _lastClockPos |
| 6× bool[4] + 1× bool[8] | 32 B | teletypeInputState/Prev, trWidthEnabled, cvSlewActive, lfoActive, cvInterpolationEnabled, performerGateOutput[8] |
| 6× uint8[4] | 24 B | trDiv, trDivCounter, trWidthPct, envState, lfoAmp, lfoFold |
| 3× int8[4] | 12 B | envEorTr, envEocTr, geodeOutRouting |
| 13× int16[4] | 104 B | teletypeCvRaw/Offset, cvSlewTime, envTargetRaw/Offset/Attack/Decay/Loop/LoopsRemaining/SlewMs, lfoCycleMs/Wave/OffsetRaw |
| 10× float[4] | 160 B | trActivityCountdown, PulseRemainingMs, trLastPulse/Interval, cvOutputTarget, cvRealTarget, prevCvTarget, cvTickPhase, lfoPhase, envStageRemaining |
| 1× float[8] | 32 B | _performerCvOutput |
| GeodeParams | 40 B | 8 int16 (time/intone/ramp/curve/run/offset/bars + mode) + tuneNum[6] + tuneDen[6] |
| GeodeEngine | ~394 B | 6× Voice[48] + _prevMeasureFraction + tune arrays + cached values + _cachedMixLevel |
| **Total** | **~904 B** | |

**Why ArpTrackEngine is smaller (~675 B)**: It handles arpeggiation with minimal state — a few arrays of note/cv values and a sequencer state, no LFO/envelope/Geode/CV slew subsystems.

**Modulove anomaly**: Despite having **16 tracks** (double everyone else), its engine is only 9,452 B because its container only holds Note (+ optional Curve/MidiCv) — each slot is ~590 B instead of ~904 B. The 16×590 = 9,440 B matches the measured engine size.

**Container gap is only ~1,832 B**: The container difference (904 vs 675 B per slot × 8 = 1,832 B) is a minority of the 6,136 B Engine gap. The remaining **~4,304 B** comes from non-container Engine members unique to XFORMER:
- `UsbH &_usbH` (4 B) + 3 std::function handlers (~24 B each for Keyboard/HidConnect/HidDisconnect) ≈ 76 B
- `_cvRouteOutputs[CvRoute::LaneCount=4]` (16 B) + `_busCv[4]` (16 B) + `_busCvWritten[4]` (4 B) + `_busCvSafeMode` (1 B) + BusCvSlewPerTick/Decay constants ≈ 48 B
- 3 teletype control methods (`panicTeletype`, `setTeletypeMetroAll`, `setTeletypeMetroActiveAll`, `resetTeletypeMetroAll`) — their thunks and supporting state
- Alignment cascade: the larger container pushes subsequent members further apart, creating ~4 KB of cascading overhead in the 15,588 B singleton
- **Key insight**: fixing the container recovers 1,832 B directly, but the alignment cascade means fixing it might recover more (~3-4 KB total) as the remaining members pack tighter

**The key leverage point**: Removing TeletypeTrackEngine from the variant would drop the container max to whichever is next largest (TuesdayTrackEngine or NoteTrackEngine). This directly saves ~1,832 B, plus indirectly recovers some of the ~4,304 B in alignment cascade overhead. Moving TeletypeTrackEngine to heap allocation is the single highest-ROI change in the entire codebase.

### Reference: Per-Fork Main Singleton Sizes

| Singleton | XFORMER | Vinx | Mebitek | Modulove |
|---|---|---|---|---|
| `_ZL5model` (Model) | 92,836 B | 88,056 B | 88,052 B | 87,204 B |
| `_ZL6engine` (Engine) | 15,588 B | 5,400 B | 5,400 B | 9,452 B |
| `_ZL2ui` (UI) | 27,244 B | 30,068 B | 29,044 B | 24,676 B |

> Note: UI is actually *larger* in Vinx (30,068 B) and Mebitek (29,044 B) than XFORMER (27,244 B) — not a pressure point.

## Optimization Strategies (Updated Priority)

### Strategy A: RoutingEngine — REDUCE RouteState (target: -7.4 KB)
**The single highest-ROI change.** XFORMER's `RouteState` has per-track shaping state (56 B × 8 tracks = 448 B) per route × 16 routes = 7,168 B that Vinx doesn't have at all. The RouteState in Vinx is just `{Target target, uint8_t tracks}` = 2 B.

Options:
- **Aggressive**: Shrink RouteState to match Vinx — strip all per-track shaping state (`TrackState[8]` with location, envelope, activity, progressive gates) and per-track cvRotate/Reset arrays. Recovers full 7.4 KB.
- **Conservative**: Make per-track shaping state conditional (allocate only when shaping is configured per route). Most routes don't use shaping — saves proportional to routes with shaping enabled.
- **Moderate**: Reduce TrackState from 56 B to essential fields, or reduce per-track arrays from 8 to actual track count.

**Files:** `src/apps/sequencer/engine/RoutingEngine.h`, `src/apps/sequencer/engine/RoutingEngine.cpp`

### Strategy B: Teletype standalone footprint (.bss+.data, target: -7 KB)
Teletype subsystem at ~6.9 KB of unique symbols. Options:
- Share the slot backup buffers (currently 2,452 B for backups when undo could reuse a single backup)
- Reduce tele_ops table (1,664 B) if some operations can share code paths
- Move tele_glyphs to flash (already .data, but the glyph bitmaps in .data are 760+280 B that could be computed at runtime)

### Strategy C: Engine container — Remove TeletypeTrackEngine from variant (target: -1.8 KB)
TeletypeTrackEngine at ~904 B inflates all 8 container slots to 904 B vs Vinx's 675 B. Removing it from the variant (heap allocation per Teletype-mode track) saves 1,832 B.

**Files:** `src/apps/sequencer/engine/Engine.h`, `src/apps/sequencer/engine/Engine.cpp`, `src/apps/sequencer/engine/TeletypeTrackEngine.h`

### Strategy D: Model singleton scatter (.bss, target: -2 KB)
The 4.8 KB Model gap is distributed across HarmonyEngine, ClipBoard::_container (Pattern with Teletype::PatternSlot), UserSettings vector backing, and Tuesday/Discrete/Indexed sequence metadata. No single big target. Lowest priority.

### Strategy D2: Note/Curve sequence header packing (.bss/model, measurement-first)
NoteSequence and CurveSequence steps are already packed into 8 B per step, but sequence-level parameters are still stored as multiple `Routable<T>` and byte/enum fields across 17 patterns/snapshots.

Measured host probe:
- `NoteSequence::Step = 8 B`, `NoteSequence = 564 B`; step array is 512 B, leaving about 52 B of sequence header/state.
- `CurveSequence::Step = 8 B`, `CurveSequence = 592 B`; step array is 512 B, leaving about 80 B of sequence header/state.
- `NoteTrack = 9,612 B`, `CurveTrack = 10,092 B`, `Track = 10,120 B`.

Implications:
- Packing NoteSequence alone is a real local simplification but may not reduce top-level `Model` RAM while CurveTrack remains the largest `Track::_container` arm.
- A useful RAM experiment should pack NoteSequence and CurveSequence sequence-parameter/header storage together, or first prove with ARM `sizeof` assertions that NoteTrack is currently the max on target.
- Serialization must remain byte-compatible: keep the current `write()`/`read()` field order and only change in-RAM representation behind existing accessors.
- Routed parameters still need base+routed slots, or an equivalent replacement, because `Routable<T>` carries live routed feedback separately from serialized base values.

Estimated savings: measurement-first. Rough local upper bound is tens of bytes per sequence × 17 patterns; top-level savings only appears if the largest model container arm shrinks.

Files: `src/apps/sequencer/model/NoteSequence.h/.cpp`, `src/apps/sequencer/model/CurveSequence.h/.cpp`, `src/apps/sequencer/model/Track.h`.

### Strategy E: Task stack trimming (.ccmram, target: -1 KB)
Task stack sizes are mostly in line with other forks, but `_ZL6fsTask` (4,256 B vs Modulove's 2,208 B) and `_ZL10driverTask` (1,184 B vs Vinx's 1,148 B) are slightly larger.

### Strategy F: Compiler flag tuning
- Verify `-Os` (optimize for size) is used — XFORMER uses `-O2` in Release
- Check `-fdata-sections -ffunction-sections` + `--gc-sections`
- Consider `-fno-rtti` (saves ~10 KB flash, but breaks existing `dynamic_cast`)

## Success Criteria

- **Minimum:** Recover 15-20 KB of RAM to reach ~85% utilization (like Vinx baseline)
- **Stretch:** Recover 30+ KB RAM for comfortable feature headroom
- **Measure:** `arm-none-eabi-size sequencer` before/after each change
- **Verify:** Firmware boots on hardware and all 7 track types function

## Dependencies

| Task | Relationship |
|---|---|
| `performer-improvements` | **Blocks.** Cannot add new settings/features until RAM headroom is recovered. |

## Priority

High — blocks all further feature development.

## Key documents
- `../../docs/superpowers/plans/2026-05-14-resource-optimization-hardware-ram-plan.md` — Go-to-market RAM recovery specification (Phase 1 measured: 2.7 KB .data recovered; P2/P4 are internal Teletype cleanup only)
- `../../teletype-performer-ecosystem-redesign/OVERVIEW.md` — 6-layer pipeline architecture map (absorbed from merged teletype-performer-ecosystem-redesign task)
- `../../teletype-performer-ecosystem-redesign/savings-plan.md` — 12 verified RAM optimization proposals with phased implementation plan
- `../../engine-subobject-size-comparison.md` — superseded preliminary analysis (now fully covered in symbol section above)

## Status
Active — Phase 1 complete. Phase 2 prep.

## Phase 1 Results (verified by build)
| Proposal | Savings | Status |
|----------|---------|--------|
| P2 — mutes bool[8]→uint8_t | 7 B/TT track internal cleanup; no current top-level .bss reduction | ✅ Merged |
| P4 — DELAY_SIZE 16→8 | 488 B/TT track internal cleanup; no current top-level .bss reduction | ✅ Merged |
| P14 — tele_glyphs/bitmap to flash | 1,040 B from .data | ✅ Merged |
| P14b — tele_ops[] to flash | 1,664 B from .data | ✅ Merged |
| **Total .data reduction** | **9,020 → 6,316 = 2,704 B** | **SRAM: 97.4% → 95.2%** |

**Key finding**: P2+P4 are valid Teletype footprint cleanup, but they do not currently count toward the 15-20 KB RAM-headroom goal. The owning storage is inside `Track::_container`, whose size is still dominated by large Note/Curve sequence-track models. The .bss total (118,400) stayed unchanged. The immediate headroom win came entirely from P14/P14b moving read-only data to flash.

## Next action (Phase 2 prep)
Plan and implement Phase 2 proposals:
- P5 — Union-based TrackState by shaper type (~4,096 B CCMRAM)
- P6 — Skip TrackState for non-per-track routes (~2,688 B CCMRAM)
- P15 — Measurement-first Note/Curve sequence header packing: verify ARM `sizeof(NoteSequence)`, `sizeof(CurveSequence)`, `sizeof(NoteTrack)`, `sizeof(CurveTrack)`, and `sizeof(Track)` before ranking; do not count note-only packing as a RAM win unless the container max actually shrinks.

## Future research
- P7 — Extract TeletypeTrackEngine from Container variant. Graphify/source review shows this only saves RAM if the firmware accepts a capped live Teletype engine pool. Preserving "any/all 8 tracks can be Teletype" requires separate storage for all 8 TeletypeTrackEngines and does not recover RAM.

## Branch
refactor/resouce-optimization
