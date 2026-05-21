# Resource Optimization — RAM & Flash Budget Recovery

## Overview

XFORMER firmware is at 97% RAM capacity (127,420 bytes out of 128 KB) with only ~4 KB headroom. This is insufficient margin for any feature additions and likely causes hard faults on boot when stack frames or dynamic allocations exceed available RAM. The 62-94 KB larger flash binary (vs Vinx baseline) reflects XFORMER-unique track types but also suggests optimization opportunities.

**Goal:** Recover at least 15-20 KB of RAM to provide safe headroom for future features, and reduce flash usage where possible.

## Current Implementation State (2026-05-21)

- Active branch in this workspace: `refactor/resouce-optimization` (resource-optimization is paused; current work happens on `feat/stochastic`).
- **2026-05-21 measurement on `feat/stochastic` post-bar-normalize build:** `.text=848,412`, `.data=6,332`, `.bss=112,616`, `.ccmram_bss=55,028`. Total SRAM `.data + .bss = 118,948 (90.7%)`. CCMRAM `55,028 (84.0%)`. UPDATE.DAT binary `875,076 B`. Compared to the post-P15b baseline (`.text=763,436`, SRAM=119,960 / 91.4%, CCMRAM=54,096 / 84.5%) — `.text` grew **+84,976 B (+11%)** from Phase 2 stochastic engine work (StochasticTrackEngine + StochasticGenerator new logic, `LockedParentEvent` buffer plumbing, Container destructor-pointer infra in `src/core/utils/Container.h`, `Engine::update()` reorder, bar rendering polish in `StochasticSequenceEditPage.cpp`). SRAM is actually **-1,012 B better** than P15b baseline; CCMRAM **+932 B worse**. Net headroom: SRAM ~12 KB, CCMRAM ~10 KB, Flash ~199 KB. Today's bar-normalize commit alone: `.text` +1,104 B, no RAM cost. Build verified rebuild picked up `StochasticSequenceEditPage.cpp.obj` and `StochasticTrackEngine.cpp.obj` at 2026-05-21 09:22.
- Hero-page UI draft (CORE-RAIN / MARBLES-BELL / DIRECT-POND / LOOP-TAPE) in `.tasks/stochastic-track-port/UI-CONTROL-DRAFT.md` — estimated cost ~10-15 KB flash, 0 B SRAM, 0 B CCMRAM. Framebuffer is reused (16,384 B CCMRAM + 8,192 B SRAM already allocated). Safe to land without touching this resource-optimization queue.
- **Phase 1 complete and hardware-verified.** Teletype mute bit-packing, `DELAY_SIZE=8`, Teletype font `const`, and `tele_ops` flash residency all confirmed working on hardware.
- Phase 1 measured saving: `.data` 9,020→6,320 = 2,700 B. Runtime SRAM: 124,720 B (95.2% of 128 KB).
- Current build after P5 + creaseEnabled fix: `.data=6,320`, `.bss=118,272`, `.ccmram_bss=54,132`; total SRAM `.data + .bss = 124,592` (94.9%).
- **Phase 2 measurement gate: ARM sizeof probes recorded ✓.** Values captured from hardware on 2026-05-14 (see "ARM sizeof Probe Results" section below). Probes are still in MonitorPage and can be removed after follow-up size checks are no longer needed.
- **P5 complete and hardware-verified.** Union-based RoutingEngine TrackState compaction saved 4,096 B CCMRAM; creaseEnabled cleanup + serialization fix saved 128 B `.bss` and fixed shaper save/load.
- **P15 complete.** CurveSequence parameter narrowing flipped the model container max from CurveTrack to NoteTrack and reduced `.bss` by 4,760 B total across P15a/P15b. Current MonitorPage values: `Track=9560`, `NoteTrack=9544`, `CurveTrack=9480`, `Engine=11492`, `Model=88072`, `Project=78012`.
- Current build after P15b: `.text=763,436`, `.data=6,320`, `.bss=113,640`, `.ccmram_bss=54,096`; total SRAM `.data + .bss = 119,960` (91.4%).
- P6 sparse/capped shaper-state allocation is deferred. The only now-clear medium-size version is a product constraint: only the first N routing lanes have stateful shaper memory. Estimated savings after P5: first 4 lanes => ~2.3 KB CCMRAM, first 8 lanes => ~1.5 KB CCMRAM.
- P15 follow-on NoteSequence packing is low ROI now: `NoteTrack=9544` and `CurveTrack=9480`, so only 64 B per Track slot is available before CurveTrack becomes the max again. Treat model/sequence packing as done for now.
- P4b Teletype backup consolidation is deferred to research because read failure rollback currently restores both slots and is not a simple one-buffer cleanup.
- P13 file task stack trimming is deprioritized to future research. It requires trustworthy watermark evidence and is not the next RAM recovery step.
- **USB/FS symbol audit complete.** Combined USB+FS SRAM residents are about 6.2 KB, but nearly all are functional buffers. The original `DirBuf` headline win is already absent from the ELF (`FF_USE_LFN=0`); remaining safe cleanup is only minor struct packing, likely ~100-300 B after ARM verification.

## Restructured Implementation Queue

1. ~~**Phase 2 measurement gate:**~~ ARM sizeof probes recorded from hardware; all five MonitorPage pages are recorded.
2. ~~**P5 RoutingEngine union compaction:**~~ complete, committed, and hardware-verified; `TrackStateUnion` reduced CCMRAM by 4,096 B.
3. ~~**P15 CurveSequence-first header packing:**~~ complete; `Track=9560`, `NoteTrack=9544`, `CurveTrack=9480`, `Model=88072`.
4. **Next active research/spec:** Teletype slot backup consolidation (`ttSlot1Backup`/`ttSlot2Backup`) needs a rollback transaction design before implementation.
5. ~~**Post-P15 USB/FS symbol audit:**~~ complete; `DirBuf` already not compiled (`FF_USE_LFN=0`), remaining safe win is ~100-300 B struct packing only (~700-1,000 B original estimate reduced after confirming DirBuf is dead code).
6. **Maybe later:** P6 capped stateful RoutingEngine lanes. If the product accepts "only routing lanes 1-N have stateful shapers", a static `TrackStateUnion[N][8]` recovers ~2.3 KB CCMRAM for N=4 or ~1.5 KB for N=8.
7. **Deferred research:** TeletypeTrackEngine container extraction targets CCMRAM and needs live-Teletype-engine/product-cap decisions.
8. **Future research:** P13 file task stack trim only after hardware watermark evidence; do not keep it in the active implementation queue.
9. **NO-GO:** LCD packed Canvas (D-A) — 16,384 B CCMRAM is the biggest real target but touches FrameBuffer/Canvas/blitting with visual regression risk; not casual. LCD D-B (`Lcd::_frameBuffer`, 8,192 B SRAM) remains conditional no-go: saves SRAM but trades away asynchronous full-frame DMA.

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

### Recovery Progress (current build vs original baseline)

| Resource | Original (pre-P1) | After P1 | After P5 + crease | After P15a | After P15b | **2026-05-21 (feat/stochastic post-Phase 2)** | Total Δ from origin | % of limit |
|---|---|---|---|---|---|---|---|---|
| **.data** | 9,020 | 6,320 | 6,320 | 6,320 | 6,320 | **6,332** | -2,688 | 4.8% |
| **.bss** | 118,400 | 118,400 | 118,272 | 114,760 | 113,640 | **112,616** | **-5,784** | 85.9% |
| **Total SRAM** (.data+.bss) | 127,420 **(97.4%)** | 124,720 (95.2%) | 124,592 (94.9%) | 121,080 (92.4%) | 119,960 (91.4%) | **118,948 (90.7%)** | **-8,472** | 90.7% |
| **CCMRAM** (.ccmram_bss) | 58,236 | 58,236 | 54,132 | 54,108 | 54,096 | **55,028** | -3,208 | 84.0% |
| **Flash .text** | 760,732 | ~764,300 | 762,956 | 762,636 | 763,436 | **848,412** | **+87,680** | 80.9% |
| **Binary (UPDATE.DAT)** | 789,252 | — | 791,472 | 788,448 | 789,260 | **875,076** | **+85,824** | — |

> **90.7% SRAM utilization — target of 85% requires ~7.5 KB more.** Current SRAM recovery: 8,472 B. Current CCMRAM recovery: 3,208 B. Flash grew +85 KB since P15b baseline from Phase 2 Stochastic engine work + Container destructor infra + UI bar-render polish — flash is well under the 1 MB limit (~199 KB headroom) so this is acceptable.

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

## ARM sizeof Probe Results (hardware, 2026-05-14)

Measured on ARM Cortex-M4F (STM32F405, `-O2`, hard float). Corrects host-x86_64 estimates.

### Page 1: Track & Container types (initial pre-P15 recording)

| Type | ARM sizeof | Host estimate | Delta | Notes |
|---|---|---|---|---|
| Track | 10,108 | 10,120 | -12 | ARM packs tighter than x86_64; superseded by post-P15 `Track=9560` |
| NoteTrack | 9,544 | 9,612 | -68 | 17×560 B sequences + ~24 B header |
| **CurveTrack** | **10,092** | 10,092 | 0 | **Pre-P15 container-sizing model type; post-P15 `CurveTrack=9480`** |
| MidiCvTrack | 24 | — | — | Minimal stub type |
| TuesdayTrack | 718 | 500 | +218 | |
| DiscreteMapTrack | 2,196 | ~1,800 | +396 | |
| IndexedTrack | 7,496 | ~4,000 | +3,496 | Far larger than estimated — 3rd largest |
| TeletypeTrack | 7,104 | ~2,800 | +4,304 | Recorded later on Teletype SIZES page; below post-P15 model gate |
| Container<7 types> | (not in first batch) | — | — | Sized by CurveTrack |
| TrackEngineContainer | 912 (per slot) | — | Sized by TeletypeTrackEngine |

### Page 2: Sequence types

| Type | ARM sizeof | Host estimate | Notes |
|---|---|---|---|
| NoteSequence | 560 | 564 | Close to host; 48 B header over 512 B step array |
| NoteSequence::Step | 8 | 8 | Confirmed packed |
| NoteSequence Step array | 512 (8×64) | 512 | Exactly 64×8 B |
| CurveSequence | 592 | 592 | Matches host; 80 B header over 512 B step array |
| CurveSequence::Step | 8 | 8 | Confirmed packed |
| CurveSequence Step array | 512 (8×64) | 512 | Exactly 64×8 B |

**Historical key finding: CurveTrack (10,092 B) > NoteTrack (9,544 B). CurveTrack was the pre-P15 container-sizing model type.** P15 later changed the gate to `NoteTrack=9544`, `CurveTrack=9480`, `Track=9560`. TeletypeTrack is now measured at `7104`, so it is not a current model-container gate.

### Page 3: RoutingEngine

| Type | ARM sizeof | Notes |
|---|---|---|
| RoutingEngine | 7,484 | Matches analysis exactly |
| RouteState | 460 | Confirmed: 56 B × 8 TrackStates + 2 B base + 8 B Shaper array + 2 B padding |
| RouteState::TrackState | (implied 56 from 7168/128) | 7,168 / (16×8) = 56 B each, confirmed |
| Routing::Shaper | 1 | Enum, no storage |
| RouteStates[] (×16) | 7,360 | 460 × 16 |
| TrackStates[] (×16×8=128) | 7,168 | 56 × 128 — **the single largest waste target** |

### P5 Results: Union-Based TrackState Compaction (hardware-verified)

| Type | Before | After | Delta |
|---|---|---|---|
| RouteState::TrackStateUnion | 56 B (old TrackState) | **24 B** | **-32 B (-57%)** |
| shaperState per route (×8) | 448 B | 192 B | -256 B |
| TrackStateUnion total (×128) | 7,168 B | **3,072 B** | **-4,096 B** |
| `.ccmram_bss` total | 58,236 B | **54,140 B** | **-4,096 B** |
| `.text` | 760,732 B | 764,300 B | +3,568 B (new helpers) |
| `.data` | 6,320 B | 6,320 B | 0 |
| `.bss` | 118,400 B | 118,400 B | 0 |

**Per-shaper state sizes (ARM-verified):**

| Shaper state struct | Estimated | ARM actual |
|---|---|---|
| LocationState | 4 B | (awaiting hardware) |
| EnvelopeState | 4 B | (awaiting hardware) |
| FreqFollowState | ~8 B | (awaiting hardware) |
| ActivityState | ~12 B | (awaiting hardware) |
| ProgDivState | ~24 B | (awaiting hardware) |
| **TrackStateUnion** (max variant) | ~24-28 B | **24 B** |

**Total runtime RAM: .data + .bss = 124,720 B (95.2% → still 95.2%). CCMRAM: 54,140 B (84.6% of 64 KB, was 91.0%).**

**Confirmed: RouteState::TrackState is exactly 56 B, and RouteState is 460 B.** The 7,168 B of TrackState storage across all 16 routes × 8 tracks is the primary compaction target for P5/P6.

### Page 4: TrackEngine types

| Type | ARM sizeof | Host estimate | Notes |
|---|---|---|---|
| NoteTrackEngine | 588 | ~590 | Close to host |
| CurveTrackEngine | 392 | — | |
| MidiCvTrackEngine | 416 | — | |
| TuesdayTrackEngine | 416 | — | |
| DiscreteMapTrackEngine | 416 | — | |
| IndexedTrackEngine | 120 | — | Very small — just pointer forwarding |
| TeletypeTrackEngine | 912 | ~904 | **Container max type at 912 B** |
| TrackEngine (base) | 32 | 24 (host) | vtable + refs |

**Key finding: TeletypeTrackEngine (912 B) is the container-sizing type.** The TrackEngineContainer is 912 B per slot, yielding 7,296 B for the 8-slot array. If TeletypeTrackEngine were moved out of the container, the next-largest engine (NoteTrackEngine at 588 B) would size it, saving (912-588)×8 = 2,592 B from the container array alone, plus likely 4+ KB from alignment cascades in the Engine singleton.

### Page 5: Global singletons

| Type | ARM sizeof | Notes |
|---|---|---|
| Engine | 15,588 | Matches earlier nm analysis exactly |
| Model | 92,836 | Matches nm analysis |
| Project | 82,524 | |
| Clock | 18* | *Display showed 18 — likely truncation; earlier nm analysis shows 164 B. Needs verification. |
| RoutingEngine | 7,484 | Confirmed |
| TrackEngineContainer (per slot) | 912 | Sized by TeletypeTrackEngine |
| TrackEngineContainerArray (×8) | 7,296 | 912 × 8 |
| TrackEngineArray (×8 ptrs) | 32 | 8 × 4 B pointers |

### Pages 4-6: Hardware recording complete
- Page 4 recorded TrackEngine sizes.
- Page 5 recorded global singleton sizes.
- Page 6 recorded Teletype-focused sizes.

## ARM sizeof Summary — Critical Corrections from Host Estimates

| Item | Host estimate | ARM actual | Delta | Impact |
|---|---|---|---|---|
| Track | 10,120 | 10,108 pre-P15 / 9,560 post-P15 | -12 / -560 | P15 reduced the model container gate |
| **CurveTrack** | **10,092** | **10,092 pre-P15 / 9,480 post-P15** | 0 / -612 | Pre-P15 max; no longer container-sizing after P15 |
| NoteTrack | 9,612 | 9,544 | -68 | Current post-P15 model container max |
| IndexedTrack | ~4,000 | **7,496** | **+3,496** | Far larger than estimated — 3rd largest |
| TeletypeTrack | ~2,800 | **7,104** | **+4,304** | Below current model gate, not a container-sizing type |
| CurveSequence | 592 | 592 | 0 | 80 B header × 17 = 1,360 B overhead per track |
| NoteSequence | 564 | 560 | -4 | 48 B header × 17 = 816 B overhead per track |
| RoutingEngine | 7,484 | 7,484 | 0 | Confirmed |
| RouteState | 460 | 460 | 0 | Confirmed |
| RouteState::TrackState | 56 | 56 | 0 | Confirmed |
| TeletypeTrackEngine | ~904 | **912** | +8 | **Container-sizing engine type** |
| NoteTrackEngine | ~590 | **588** | -2 | Next-largest engine if TT extracted |
| TrackEngineContainer | — | 912 (×8 = 7,296) | — | Sized by TeletypeTrackEngine |
| Engine | 15,588 | 15,588 | 0 | Confirmed from nm |
| Model | 92,836 | 92,836 | 0 | Confirmed from nm |


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

### Track::_container — Not the Current Bottleneck

Earlier analysis incorrectly claimed the `Track::_container` variant was a ~12 KB fork gap. In reality, all forks are dominated by large step-sequence track models, so the container explains little of the XFORMER-vs-Vinx delta. P15 then moved the local max from CurveTrack to NoteTrack.

```cpp
// XFORMER Track::_container — current max is NoteTrack (9544 B)
Container<NoteTrack, CurveTrack, MidiCvTrack,
          TuesdayTrack, DiscreteMapTrack, IndexedTrack, TeletypeTrack>

// Vinx Track::_container — large Note/Curve-style sequence tracks also dominate;
// exact max requires fork-specific sizeof verification.
Container<NoteTrack, CurveTrack, MidiCvTrack,
          StochasticTrack, LogicTrack, ArpTrack>
```

| Track Type | Current ARM Size | Why |
|---|---|---|
| **NoteTrack** | **9,544 B** | Current `Track::_container` max after P15. |
| CurveTrack | 9,480 B | P15 narrowed CurveSequence parameters; now 64 B below NoteTrack. |
| IndexedTrack | 7,496 B | Larger than early estimates, but still below Note/Curve. |
| TeletypeTrack | 7,104 B | `scene_state_t` (4,640 B) + 2× PatternSlot (2,452 B) + small fields/padding. |
| DiscreteMapTrack | 2,196 B | 32 stages + 17 sequence metadata. |
| TuesdayTrack | 718 B | 17 TuesdaySequence parameter objects + track fields. |
| MidiCvTrack | 24 B | Minimal stub type. |

Since both forks' containers are sized by large step-sequence tracks, the Track::_container produces **virtually zero fork gap**. Current MonitorPage values:

```text
NoteTrack      = 9,544 B
CurveTrack     = 9,480 B
TeletypeTrack  = 7,104 B
Track          = 9,560 B
Model          = 88,072 B
```

P15 was the right model/container experiment and is complete. Further NoteSequence packing has low current ROI: only 64 B per Track slot is available before CurveTrack becomes the max again.

### Total Gap Accounting

| Source | RAM | Location |
|---|---|---|
| **RoutingEngine** (per-track shaping state × 16 routes) | **~7.4 KB** | .ccmram |
| Teletype subsystem (ops + slots + backups + glyphs + line buffer) | ~6.9 KB | .bss + .data |
| Model singleton (scattered: HarmonyEngine, ClipBoard, UserSettings, sequence metadata diffs) | ~4.8 KB | .bss |
| Engine container (TeletypeTrackEngine 912 B vs NoteTrackEngine 588 B × 8) | 2,592 B conditional | .ccmram |
| USB HID device struct | 0.5 KB | .bss |
| **Total explained** | **~21.4 KB** | |
| **Actual measured gap** | **~14 KB** | XFORMER 127,420 vs Vinx 113,308 |

> The 7 KB over-count is from Vinx having extra features XFORMER lacks (Stochastic/Logic/Arp track types, chaos/entropy subsystems, knobPad16, extra settings, larger UI). The RoutingEngine alone accounts for more than the entire measured gap when Vinx's extras are subtracted.

### Key Takeaway

**The RoutingEngine is the single biggest RAM problem — not the Engine container.** The RouteState in XFORMER has per-track shaping state (56 B × 8 tracks = 448 B per route × 16 routes = 7,168 B) that Vinx doesn't have at all. That's over half the total gap.

Optimization priority after P5 (ARM-verified):
1. **P15 CurveSequence header packing** (.bss/model, next active): CurveTrack (10,092 B) is the container-sizing model type. CurveSequence header = 80 B × 17 = 1,360 B per track. Reducing CurveSequence should shrink CurveTrack and therefore `Track::_container` across all 8 tracks.
2. **NoteSequence header packing** (.bss/model, follow-up only): NoteSequence header = 48 B × 17 = 816 B per NoteTrack, but NoteTrack is not the max container arm. Do this after CurveSequence only if it yields additional model reduction or keeps the sequence API/storage coherent.
3. **P6 sparse/capped RoutingEngine state** (.ccmram, deferred): P5 already recovered 4,096 B CCMRAM. Further savings require sparse allocation or a cap on stateful shaper-track pairs, so this needs a separate design.
4. **Teletype standalone backups** (.bss, deferred research): `ttSlot1Backup`/`ttSlot2Backup` are tempting, but read failure rollback currently restores both slots. Consolidation is not a simple one-buffer cleanup.
5. **TeletypeTrackEngine container extraction** (.ccmram, deferred research): direct container saving is 2,592 B, but it needs lifecycle and live-Teletype-engine cap decisions and no longer addresses the most urgent region after P5.
6. **Model scatter** (.bss, later): many micro-tweaks needed.

### Track Engine Container Contents (Per Fork)

The Engine singleton's `_trackEngineContainers` is a `std::array<Container<Types...>, CONFIG_TRACK_COUNT>` — a union-like variant where each slot is as large as the **biggest** type in the template list. This is the single largest RAM driver within the Engine.

| Fork | Types in Container | Max Type | Per-Track Slot | Array Total |
|---|---|---|---|---|
| **XFORMER** | Note, Curve, MidiCv, Tuesday, DiscreteMap, Indexed, **Teletype** | TeletypeTrackEngine (**912 B** ARM) | **912 B** | **7,296 B (7.1 KB)** |
| **Vinx** | Note, Curve, MidiCv, Stochastic, LogicTrack, Arp | ArpTrackEngine (~675 B) | **~675 B** | **~5,400 B (5.3 KB)** |
| **Mebitek** | Note, Curve, MidiCv, Stochastic, LogicTrack, Arp | ArpTrackEngine (~675 B) | **~675 B** | **~5,400 B (5.3 KB)** |
| **Modulove** | Note (±Curve ±MidiCv via `#ifdef`) | NoteTrackEngine or CurveTrackEngine | **~590 B** | **~9,440 B (9.2 KB) × 16 tracks** |

**ARM-verified TrackEngine sizes:**

| TrackEngine Type | ARM sizeof |
|---|---|
| TeletypeTrackEngine | **912 B** (container-sizing max) |
| NoteTrackEngine | 588 B |
| MidiCvTrackEngine | 416 B |
| TuesdayTrackEngine | 416 B |
| DiscreteMapTrackEngine | 416 B |
| CurveTrackEngine | 392 B |
| IndexedTrackEngine | 120 B |
| TrackEngine (base) | 32 B |

**Removing TeletypeTrackEngine from container** → new max = NoteTrackEngine (588 B), container slot drops from 912 → 588, saving (912-588)×8 = **2,592 B** directly from TrackEngineContainerArray, plus likely 4+ KB from alignment cascade in Engine (15,588 B singleton). Total Engine reduction estimate: **2,592 + ~4,000 = ~6.6 KB**.

**Container gap is now 2,592 B (ARM-verified)**: (912 - 588) × 8. The remaining ~8,000 B of XFORMER's Engine gap (15,588 - 5,400 = 10,188 B) comes from non-container members + alignment cascade.

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

### Strategy B: Teletype standalone footprint (.bss+.data, research)
Teletype subsystem at ~6.9 KB of unique symbols. Options:
- Share the slot backup buffers only after redesigning read failure rollback. Current `readTeletypeTrack()` snapshots both PatternSlots before clearing and restores both on error, so backup consolidation is not low-risk or one-file/simple.
- Reduce tele_ops table (1,664 B) if some operations can share code paths
- Move tele_glyphs to flash (already .data, but the glyph bitmaps in .data are 760+280 B that could be computed at runtime)

### Strategy C: Engine container — Remove TeletypeTrackEngine from variant (deferred research)
TeletypeTrackEngine at 912 B inflates all 8 container slots to 912 B. Current MonitorPage values show `TeletypeTrackEngine=912`, `Engine::TrackEngineContainer=912`, and `NoteTrackEngine=588`. The direct conditional gap is `(912 - 588) * 8 = 2,592 B` CCMRAM; do not reuse the old 904-vs-675 estimate.

After P5, this is not the proper next step: it mainly targets CCMRAM, while current pressure is still `.data + .bss`. It also requires explicit lifecycle and product constraints around maximum live Teletype engines.

**Files:** `src/apps/sequencer/engine/Engine.h`, `src/apps/sequencer/engine/Engine.cpp`, `src/apps/sequencer/engine/TeletypeTrackEngine.h`

### Strategy D: Model singleton scatter (.bss, target: -2 KB)
The 4.8 KB Model gap is distributed across HarmonyEngine, ClipBoard::_container (Pattern with Teletype::PatternSlot), UserSettings vector backing, and Tuesday/Discrete/Indexed sequence metadata. No single big target. Lowest priority.

### Strategy D2: P15 CurveSequence-first header packing (.bss/model, complete)
NoteSequence and CurveSequence steps are already packed into 8 B per step, but sequence-level parameters are still stored as multiple `Routable<T>` and byte/enum fields across 17 patterns/snapshots.

P15a/P15b narrowed CurveSequence parameter storage behind existing accessors while preserving serialized field order and routed/base value semantics. This was the correct model/SRAM target: ARM probes originally showed CurveTrack as the `Track::_container` max.

Post-P15 hardware/build values:
- `Track=9560`
- `NoteTrack=9544`
- `CurveTrack=9480`
- `TeletypeTrack=7104`
- `Model=88072`
- `.bss=113,640`

Implications:
- P15 succeeded and flipped the container max from CurveTrack to NoteTrack.
- Model storage is now effectively at the Vinx reference level (`Model=88072` vs earlier Vinx comparison around 88056); stop treating the model container as the main RAM problem under current semantics.
- NoteSequence follow-up packing is low ROI: with `NoteTrack=9544` and `CurveTrack=9480`, only 64 B per Track slot is available before CurveTrack becomes max again. That is at most 512 B from the main `Track::_container`, so it is not the next active target.
- TeletypeTrack cleanup is not a current model-container win: `TeletypeTrack=7104` is far below the `NoteTrack=9544` gate. Slot/VM cleanup may still be important for correctness and future architecture.

Files: `src/apps/sequencer/model/NoteSequence.h/.cpp`, `src/apps/sequencer/model/CurveSequence.h/.cpp`, `src/apps/sequencer/model/Track.h`.

### Strategy E: Task stack trimming (.ccmram, future research)
Task stack sizes are mostly in line with other forks, but `_ZL6fsTask` (4,256 B vs Modulove's 2,208 B) and `_ZL10driverTask` (1,184 B vs Vinx's 1,148 B) are slightly larger.

P13 is intentionally deprioritized. Stack trimming is not an implementation target until hardware watermark instrumentation proves worst-case file-flow headroom. The `ttSlot*` globals were previously moved out of stack into `.bss` because file-task stack pressure was real, so this must remain a research/watermark task.

### Strategy E1: USB/FS symbol audit (complete; small safe win only)

USB/FS SRAM residents total about 6.2 KB, but most of that storage is functional:
- `driver_data_fs` is about 2.7-2.8 KB: USB host controller state, 15 `usbh_device_t` slots, and a 2,048 B enumeration buffer.
- HID/MIDI/hub driver globals are about 1.1 KB combined: endpoint buffers, device slots, previous-key state, and event rings.
- FatFs/file globals are about 1.7 KB: `filePool` is about 1.1 KB for two `FIL` handles with sector buffers; `DirBuf` is about 608 B.
- UsbH wrapper queues/buffers are about 300 B.

Verdict:
- No hidden 8 KB SRAM target exists in USB/FS.
- `DirBuf` (~608 B) is **already not compiled** — `FF_USE_LFN == 0` and `FF_FS_EXFAT == 0` cause the `#if FF_USE_LFN == 0` branch to be taken, which defines empty macros and never declares `DirBuf` or `LfnBuf`. Confirmed absent from ELF symbols via `nm`. The bootloader FatFs copy also has `FF_USE_LFN == 0`. No cleanup needed.
- Current measured SRAM residents: `driver_data_fs` 2,800 B (.data), `filePool` 1,116 B (.bss), `hid_device` 560 B (.bss), `channels_fs` 288 B (.bss), `_ZL4usbh` 188 B (.bss), `FatFs` 4 B (.bss), `usbh_data` 16 B (.bss).
- Minor struct packing (`usbh_device_t`, `channel_t`, `hid_device_t`) may recover ~100-300 B, but must be verified on ARM and should not change USB behavior.
- Product-level config reductions can save more, but they change behavior: reducing `USBH_MAX_DEVICES`, HID/MIDI device counts, or `BUFFER_ONE_BYTES` may break hubs, multiple MIDI/HID devices, or composite descriptors.

Realistic no-behavior-change SRAM win: ~100-400 B (DirBuf is already gone; remaining wins are struct packing only).

### Strategy E2: Display buffer architecture (confirmed no-go)

Current display memory:
- `Ui::_frameBufferData[256*64]` = 16,384 B in CCMRAM. This is the 8-bit Canvas working buffer.
- `Lcd::_frameBuffer[256*64/2]` = 8,192 B in normal SRAM. This is the packed 4-bit DMA buffer; DMA cannot read CCMRAM on STM32F4.

Verdict:
- **D-A / packed Canvas framebuffer: NO-GO.** The 16,384 B 8-bit UI buffer is the biggest CCMRAM target, but it touches FrameBuffer/Canvas/blitting and requires visual regression testing across every page. The savings are in CCMRAM (not main SRAM), and the risk of subtle rendering bugs is high. Marked as no-go for now.
- **D-B / remove `Lcd::_frameBuffer`: conditional no-go.** CPU-SPI streaming or line-buffered DMA can recover 8,192 B SRAM, but it replaces the current asynchronous full-frame DMA transfer with blocking transfer/restart work. At default 50 Hz, complex pages can exceed the UI frame budget. Keep this only as a last-resort `CONFIG_LCD_LOW_RAM_MODE` idea, likely paired with a 30 Hz display cap and hardware validation.

Do not propose LCD buffer removal as an easy 8 KB win in the active RAM plan. Prefer USB/FS symbol audit and Teletype file transaction-buffer research first.

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
Active — Phase 1, P5, and P15 complete. Current work is target selection for the remaining SRAM gap, with P13 moved to future research.

## Phase 1 Results (verified by build, hardware-tested ✓)
| Proposal | Savings | Status |
|----------|---------|--------|
| P2 — mutes bool[8]→uint8_t | 7 B/TT track internal cleanup; no current top-level .bss reduction | ✅ Merged |
| P4 — DELAY_SIZE 16→8 | 488 B/TT track internal cleanup; no current top-level .bss reduction | ✅ Merged |
| P14 — tele_glyphs/bitmap to flash | 1,040 B from .data | ✅ Merged |
| P14b — tele_ops[] to flash | 1,664 B from .data | ✅ Merged |
| **Total .data reduction** | **9,020 → 6,320 = 2,700 B** | **SRAM: 97.4% → 95.2%** |

**Key finding**: P2+P4 are valid Teletype footprint cleanup, but they do not currently count toward the 15-20 KB RAM-headroom goal. The owning storage is inside `Track::_container`, whose size is still dominated by large Note/Curve sequence-track models. The .bss total (118,400) stayed unchanged. The immediate headroom win came entirely from P14/P14b moving read-only data to flash.

## Phase 2 Measurement Gate (hardware-recorded)
ARM `sizeof` probes added to MonitorPage SIZES mode and recorded from hardware. 6 pages, encoder-scrollable:
- **Page 1**: Track container types (Track, NoteTrack, CurveTrack, MidiCvTrack, TuesdayTrack, DiscreteMapTrack, IndexedTrack, TeletypeTrack, Container sizes)
- **Page 2**: Sequence types (NoteSequence, NoteSequence::Step, CurveSequence, CurveSequence::Step, step array totals)
- **Page 3**: RoutingEngine (RoutingEngine total, RouteState, RouteState::TrackState, Shaper, array totals)
- **Page 4**: TrackEngine types (all 7 engine sizes + base TrackEngine)
- **Page 5**: Global singletons (Engine, Model, Project, Clock, RoutingEngine, TrackEngineContainer array, MidiOutputEngine)
- **Page 6**: Teletype-focused sizes (`TTTrack`, `TTSlot`, `scene_state`, `tele_cmd`, `scene_pat`, `TTTE`, `TECont`)

Teletype-focused hardware measurements:
- `TTTrack=7104`
- `TTSlot=1226`
- `scene_state=4640`
- `tele_cmd=52`
- `scene_pat=138`
- `TTTE=912`
- `TECont=912`

Implications:
- `TTSlot=1226` vs `scene_state=4640` confirms PatternSlot is not a full VM scene snapshot; it is partial/shadow storage around scripts, metro, patterns, and routing config.
- `TTTrack=7104` is below the `Track::_container` gate (`NoteTrack=9544`), so Teletype model shrinkage does not currently reduce top-level model RAM.
- `TTTE=912` equals `TECont=912`, so TeletypeTrackEngine is the current engine-container gate. Teletype engine extraction/compaction remains the structurally relevant Teletype RAM target, after slot/VM ownership is clarified.

Build after P5 + creaseEnabled fix: `.data=6,320 .bss=118,272 .ccmram_bss=54,132`.

## P5 Results (hardware-tested ✓)

| Proposal | Savings | Status |
|----------|---------|--------|
| P5 — union-based TrackState by shaper type | 4,096 B `.ccmram_bss` | ✅ Committed and hardware-verified |
| creaseEnabled cleanup + serialization fix | 128 B `.bss`; fixes shaper save/load | ✅ Committed and hardware-verified |

All five stateful shapers were checked on hardware. Shaper configuration now saves/loads correctly.

## Next action
Do **not** go straight to NoteSequence or P13.

1. Research/spec the Teletype file-load backup transaction before touching `ttSlot1Backup` / `ttSlot2Backup`: define what gets snapshotted, when clearing is allowed, and what must be restored after malformed multi-slot files.
2. Do not pursue USB/FS as active RAM recovery; `DirBuf` is already absent and remaining safe struct-packing cleanup is only ~100-300 B after ARM verification.
3. Only then choose whether to pursue Teletype transaction-buffer changes or defer more RAM recovery.

## Future research
- P4b — Teletype slot backup consolidation. Deferred because `readTeletypeTrack()` currently restores both PatternSlots on parse/read failure; one shared backup would change rollback semantics unless the load transaction is redesigned.
- P6 — Capped stateful RoutingEngine lanes. Maybe later, if the product accepts that only the first N routing lanes can use stateful shapers; stateless shapers still work on all 16 lanes. After P5, estimated savings are ~2.3 KB CCMRAM for N=4 or ~1.5 KB for N=8. This is the only remaining medium-level RAM option with a clear static layout.
- P7 — Extract TeletypeTrackEngine from Container variant. Deferred because it targets CCMRAM and requires a capped live Teletype engine pool or equivalent lifecycle contract. Preserving "any/all 8 tracks can be Teletype" requires separate storage for all 8 TeletypeTrackEngines and does not recover RAM.
- P13 — File task stack trim. Deprioritized to future research; requires reliable hardware watermark data across worst-case save/load/malformed-file flows before any stack size reduction.
- NoteSequence header packing. Low ROI after P15 because the Track container max is now NoteTrack by only 64 B over CurveTrack.
- LCD D-A — packed Canvas framebuffer. Confirmed NO-GO. The 16,384 B 8-bit UI CCMRAM buffer is the biggest real memory target, but it touches FrameBuffer/Canvas/blitting and needs visual regression testing. Not casual; marked as no-go for now.
- LCD D-B — remove `Lcd::_frameBuffer` / line-buffered DMA. Conditional no-go: saves 8 KB SRAM but sacrifices asynchronous full-frame DMA, likely requiring 30 Hz cap and hardware validation. Last-resort only.
- USB/FS config reductions — reducing USB device counts or `BUFFER_ONE_BYTES` is deferred because it changes supported hub/composite/MIDI/HID behavior. Safe cleanup is limited to struct packing (~100-300 B after ARM verification). `DirBuf` is already not compiled (`FF_USE_LFN=0`).

## Remaining medium-level RAM conclusion

As of 2026-05-16, the medium-level no-drama RAM queue is effectively exhausted:

- P5 and P15 are done.
- Teletype Phase 4 is done and verified.
- USB/FS has no hidden safe target; `DirBuf` is absent from the ELF.
- P13 needs stack watermark proof.
- LCD/display work is large-risk UI architecture.
- Model/container pooling is no-go under current "any track can be any type" semantics.

The only meaningful medium option left is the explicit P6 product constraint above: cap stateful shaper memory to the first N routing lanes. Everything else is either micro-cleanup, future research, or major architecture work.

## Branch
refactor/resouce-optimization
