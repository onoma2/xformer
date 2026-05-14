# Resource Optimization Hardware/RAM Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Recover 15-20 KB of RAM while proving each optimization on STM32 hardware before moving to the next risk tier.

**Architecture:** Treat `build/stm32/release` as the source of truth for footprint. Each phase is a small, bootable firmware slice with before/after `.size`, `.map`, `.list`, and symbol evidence. Risk rises from read-only data placement, to Teletype state compaction, to RoutingEngine state layout, to task-stack and higher-risk semantic changes.

**Tech Stack:** STM32F405 firmware, C/C++, CMake, `arm-none-eabi-size`, `arm-none-eabi-nm`, generated `sequencer.map`, hardware flashing via `UPDATE.DAT`.

---

## Current Baseline

Use this baseline before the first code change and refresh it after every phase.

Known preparatory baseline:

| Metric | Current |
|---|---:|
| `.text` | 760,732 B |
| `.data` | 9,020 B |
| `.bss` | 118,400 B |
| `.ccmram_bss` | 58,232 B |
| Total runtime RAM (`.data + .bss`) | 127,420 B |
| Binary | 789,252 B |

The active savings plan ranks the work as:

| Phase | Proposals | Target savings | Risk |
|---|---|---:|---|
| 1 | P2/P4 internal cleanup + P14 + P14b | 2,700 B measured .data | none-low |
| 2B | P5 union-based TrackState compaction | 4,096 B measured .ccmram_bss | medium; complete |
| 2C | P15 CurveSequence-first header packing | measurement-first; count only if `CurveSequence` shrinks `Track` / `_ZL5model` | low-medium |
| 2R | P4b, P6, P7 | deferred research only | medium-high |
| 3 | P8 + P13, maybe P5a only by explicit decision | ~2,080 B safe subset; +7,488 B risky | medium-high |

Current implementation state:

- Phase 1 is hardware-verified: `.data` 9,020→6,320 = 2,700 B saved.
- P5 is hardware-verified: `TrackStateUnion` reduced `.ccmram_bss` by 4,096 B.
- creaseEnabled removal + serialization fix is hardware-verified: `.bss` reduced by 128 B and shaper save/load now works.
- Current pressure is SRAM (`.data + .bss`), not CCMRAM; next active work is P15 CurveSequence-first header packing.
- Do not start model-pool/container-pool architecture work under current "any track can be any type" semantics; `model-pool-decision-table.md` records this as no-go.
- Defer Teletype backup consolidation. Current read failure rollback restores both PatternSlots, so one-buffer consolidation is research, not a simple Phase 2 cleanup.

## Measurement Protocol

Run before every phase and after every phase:

```bash
cmake --build build/stm32/release --target sequencer
sed -n '1,24p' build/stm32/release/src/apps/sequencer/sequencer.size
arm-none-eabi-nm --print-size --size-sort build/stm32/release/src/apps/sequencer/sequencer | tail -80
```

Record these fields in the phase notes:

| Field | Source |
|---|---|
| `.data` | `sequencer.size` |
| `.bss` | `sequencer.size` |
| `.ccmram_bss` | `sequencer.size` |
| `.text` | `sequencer.size` |
| Total RAM | `.data + .bss` |
| CCMRAM headroom | `65536 - .ccmram_bss` |
| Binary size | `wc -c build/stm32/release/src/apps/sequencer/sequencer.bin` |
| Top changed symbols | `arm-none-eabi-nm --print-size --size-sort ...` and `build/stm32/release/sequencer.map` |

Pass criteria for every phase:

- Firmware builds with the STM32 `sequencer` target.
- Runtime RAM does not increase unless the phase explicitly trades RAM regions and the trade is documented.
- The measured saving is within 20% of the expected saving, or the variance is explained by alignment/section placement.
- Firmware boots on hardware.
- No new crash, black screen, frozen boot, missing display, or obvious input lockup.

## Hardware Smoke Checklist

Run this after flashing each phase's `UPDATE.DAT`:

1. Boot: module reaches the normal sequencer UI within the expected boot time.
2. Display: OLED draws the current page without corruption.
3. Transport: Play/stop responds; clock advances visibly.
4. Note track: select a Note track, toggle gates, confirm gate/CV output.
5. Curve track: select a Curve track, confirm output still updates.
6. MidiCv track: enter the page and verify no boot/page crash.
7. Tuesday track: enter the page and run one pattern.
8. DiscreteMap track: enter the page and verify stage/output activity.
9. Indexed track: enter the page and verify sequence output still advances.
10. Teletype track: enter script/pattern UI, run at least one simple script that touches TR/CV.
11. File path: save and reload a project containing a Teletype track.
12. USB: keyboard or Launchpad still enumerates if connected.

If any check fails, stop the phase, keep the measurement output, and bisect within that phase before continuing.

## Phase 0: Baseline Capture

**Files:**
- Read: `build/stm32/release/src/apps/sequencer/sequencer.size`
- Read: `build/stm32/release/src/apps/sequencer/sequencer.list`
- Read: `build/stm32/release/sequencer.map`
- Modify: none

- [ ] Run the measurement protocol on the current branch.
- [ ] Copy the `.data`, `.bss`, `.ccmram_bss`, `.text`, binary size, and top relevant symbols into the implementation notes.
- [ ] Flash current `UPDATE.DAT`.
- [ ] Run the hardware smoke checklist.
- [ ] If baseline hardware fails, stop. RAM optimization must not start from an unbootable baseline.

## Phase 1: Safe Quick Wins

**Proposals:** P2, P4, P14, P14b

**Files:**
- Modify: `teletype/src/state.h`
- Modify: `teletype/src/state.c`
- Modify: `teletype/src/teletype_config.h`
- Modify: `src/core/gfx/fonts/tele.h`
- Modify: `teletype/src/ops/op.c`

**Expected savings:** updated after Phase 1 measurement. P14/P14b recover 2,704 B from `.data`; P2/P4 are internal Teletype footprint cleanup and do not currently reduce top-level `.bss` because `Track::_container` remains sized by large Note/Curve sequence-track models.

- [x] P2: pack `scene_state_t::mutes[8]` into one `uint8_t`; keep access through `ss_get_mute()` / `ss_set_mute()`.
- [x] P4: reduce `DELAY_SIZE` from 16 to 8 in the active Performer Teletype config only.
- [x] P14: add `const` to `tele_glyphs` and `tele_bitmap`; verify they leave `.data`.
- [x] P14b: make `tele_ops[]` flash-resident only if the table and pointed-to op records are genuinely immutable.
- [x] Build STM32 `sequencer`.
- [x] Run the measurement protocol and confirm expected symbol movement: `tele_glyphs`, `tele_bitmap`, and ideally `tele_ops` are no longer RAM residents.
- [ ] Flash hardware and run the hardware smoke checklist.
- [ ] Teletype-specific check: run a script using `DEL` or queued delay behavior enough to prove delay queue still works at size 8.
- [ ] Commit only after hardware passes and the RAM delta is recorded.

**Stop conditions:**

- Any write attempt to a newly-const table.
- Teletype delay behavior breaks under a normal small script.
- Saving or loading a Teletype project corrupts slot data.

## Phase 2R: Deferred Research — Teletype File Backup Consolidation

**Proposal:** P4b

**Files:**
- Modify: `src/apps/sequencer/model/FileManager.cpp`

**Expected savings:** about 1,226 B from `.bss` only after rollback semantics are redesigned.

**Status:** deferred. This is not the next implementation step. Current `readTeletypeTrack()` snapshots both PatternSlots before clearing and restores both on failure, so replacing two backups with one shared backup changes transaction semantics unless the load flow is redesigned.

- [ ] Read the current `readTeletypeTrack()` slot parse flow and identify where `ttSlot1Backup` and `ttSlot2Backup` are copied/restored.
- [ ] Replace the two backup globals with one shared backup buffer only if restore semantics stay single-slot and explicit.
- [ ] Build STM32 `sequencer`.
- [ ] Run the measurement protocol and confirm one `ttSlot*Backup`-sized symbol is gone.
- [ ] Flash hardware and run the hardware smoke checklist.
- [ ] File failure check: load a deliberately malformed Teletype file after saving a known-good project; verify the active slot does not become partially corrupted.
- [ ] Multi-slot check: load a file with both `#SLOT 1` and `#SLOT 2`; verify both land correctly on success.
- [ ] Commit only after hardware passes and rollback behavior is understood.

**Stop conditions:**

- Multi-slot parse failure cannot be made honest with one backup.
- Rollback semantics become ambiguous.
- Any save/load behavior regresses.

## Phase 2B: RoutingEngine State Reduction

**Proposals:** P5 complete; P6 deferred

**Files:**
- Modify: `src/apps/sequencer/engine/RoutingEngine.h`
- Modify: `src/apps/sequencer/engine/RoutingEngine.cpp`

**Measured savings:** P5 saved 4,096 B from `.ccmram_bss`. P6 is deferred because sparse/capped state allocation needs a separate design around stateful-shaper caps or sparse state lifetime.

- [x] Before editing, catalog every `TrackState` field use in `RoutingEngine.cpp` by shaper.
- [x] Add a focused test or debug-only size assertion for `sizeof(RoutingEngine::RouteState)` if the local test framework can cover this cheaply.
- [x] Convert `TrackState` to a per-track union/discriminant layout; do not use a per-route discriminant because tracks in one route can use different shapers.
- [x] Keep accumulated state for ProgressiveDivider, Location, Envelope, Activity, and FrequencyFollower.
- [ ] Avoid P5a in this phase; do not strip state entirely.
- [ ] Skip state storage for route targets where `Routing::isPerTrackTarget()` proves no per-track shaping can apply. Deferred as P6 research; runtime branching alone is not a RAM win.
- [x] Build STM32 `sequencer`.
- [x] Run the measurement protocol and confirm `_ZL6engine` / routing-related CCMRAM drops by the expected range.
- [x] Flash hardware and run the hardware smoke checklist.
- [x] Routing hardware check: create or load routes using each stateful shaper and verify output changes over time, not just instantly.
- [x] Regression check: verify Tempo/Swing/Play/Record/Mute/Fill/Pattern routes still behave as non-per-track routes.
- [x] Commit only after hardware passes and routing semantics are proven.

**Stop conditions:**

- Stateful shapers become stateless or audibly/visibly stuck.
- Route target classification is unclear.
- CCMRAM saving is much smaller than expected and cannot be explained.

## Phase 2C: P15 CurveSequence-First Header Packing

**Proposal:** P15

**Files:**
- Modify: `src/apps/sequencer/model/CurveSequence.h`
- Modify: `src/apps/sequencer/model/CurveSequence.cpp`
- Read/follow-up only: `src/apps/sequencer/model/NoteSequence.h`
- Read/follow-up only: `src/apps/sequencer/model/NoteSequence.cpp`
- Read/measure: `src/apps/sequencer/model/Track.h`

**Expected savings:** measurement-first. ARM probes show `CurveSequence=592 B`, `CurveTrack=10,092 B`, and `Track=10,108 B`. CurveTrack is the `Track::_container` sizing arm, so CurveSequence header packing is the correct first model/SRAM experiment. Count this phase only from ARM `sizeof(CurveSequence)`, `sizeof(CurveTrack)`, `sizeof(Track)`, `_ZL5model`, and `.bss` deltas.

- [x] Add temporary ARM build size probes for `sizeof(NoteSequence::Step)`, `sizeof(NoteSequence)`, `sizeof(CurveSequence::Step)`, `sizeof(CurveSequence)`, `sizeof(NoteTrack)`, `sizeof(CurveTrack)`, and `sizeof(Track)`.
- [x] Record whether NoteTrack or CurveTrack is the current `Track::_container` max on target: CurveTrack is max.
- [ ] Design a packed CurveSequence header behind current accessors.
- [ ] Preserve the current public getters/setters and the current serialized `write()` / `read()` field order.
- [ ] Preserve `Routable<T>` semantics for routed sequence params: base and routed values must remain independently representable.
- [ ] Build STM32 `sequencer`.
- [ ] Run the measurement protocol and confirm `sizeof(Track)`, `_ZL5model`, and `.bss` drop; otherwise mark as local cleanup only, not RAM recovery.
- [ ] Flash hardware and run the hardware smoke checklist.
- [ ] Pattern independence check: create Note and Curve patterns with different divisor/run mode/first/last/scale/root-style settings, switch patterns, save, reboot, reload, and verify settings survive.
- [ ] Routing check: route sequence params that support routing and verify routed values still override base values only while the route is active.
- [ ] Commit only after hardware passes and the real RAM delta is recorded.

**Stop conditions:**

- ARM size probes show no plausible `Track` / `_ZL5model` reduction path.
- Routed base/routed value ownership becomes ambiguous.
- Serialization would require a project-version migration for this storage-only change.
- Pattern-specific sequence params collapse into shared track-level behavior.

## Phase 2D: Future Research: Extract TeletypeTrackEngine From Engine Container

**Proposal:** P7

**Files:**
- Modify: `src/apps/sequencer/engine/Engine.h`
- Modify: `src/apps/sequencer/engine/Engine.cpp`
- Read: `src/apps/sequencer/engine/TeletypeTrackEngine.h`

**Expected savings:** conditional. About 1,832 B direct from `.ccmram_bss` only if the product accepts a capped live Teletype engine pool. If any/all 8 tracks must remain valid Teletype tracks, separate storage for 8 TeletypeTrackEngines cancels the container saving.

- [ ] Map the current track-engine construction/destruction path in `Engine::updateTrackSetups()`.
- [ ] Decide the product rule for maximum live Teletype tracks before implementation.
- [ ] Define load-time behavior for projects with more Teletype tracks than the cap.
- [ ] Design separate static storage for Teletype engines without heap allocation unless the project already has an accepted allocator pattern for this subsystem.
- [ ] Remove `TeletypeTrackEngine` from the general track-engine `Container<>` only after lifecycle ordering is clear.
- [ ] Build STM32 `sequencer`.
- [ ] Run the measurement protocol and confirm the engine singleton/container symbol shrinks.
- [ ] Flash hardware and run the hardware smoke checklist.
- [ ] Mode-switch check: repeatedly switch a track between Teletype and non-Teletype modes, then reboot and verify the selected modes persist.
- [ ] Teletype runtime check: run clocked scripts, CV output, TR output, metro, and panic/reset behavior.
- [ ] Commit only after hardware passes mode-switch and runtime checks.

**Stop conditions:**

- Teletype engine lifetime becomes unclear.
- Track mode switching leaves stale engine pointers.
- Any use-after-destroy or boot freeze appears.

## Phase 3A: Pattern Slot Mirror Cleanup

**Proposal:** P8

**Files:**
- Modify: `src/apps/sequencer/model/TeletypeTrack.h`
- Modify: `src/apps/sequencer/model/TeletypeTrack.cpp`

**Expected savings:** about 544 B.

- [ ] Re-read `applyPatternSlot()`, `syncToActiveSlot()`, and all copy helpers before editing.
- [ ] Remove only one redundant pattern mirror; preserve the active scene state as the runtime truth.
- [ ] Build STM32 `sequencer`.
- [ ] Run the measurement protocol and confirm the Teletype track/model symbol shrinks.
- [ ] Flash hardware and run the hardware smoke checklist.
- [ ] Teletype slot check: edit slot 1 and slot 2 independently, switch between them, save, reboot, reload, and confirm both slots survive.
- [ ] Commit only after hardware passes.

**Stop conditions:**

- Slot switching loses edits.
- Save/load no longer preserves both slots.
- Runtime scene state and stored slot state diverge.

## Phase 3B: File Task Stack Trim With Watermark

**Proposal:** P13

**Files:**
- Modify: `src/apps/sequencer/Config.h`
- Modify only if needed for temporary instrumentation: `src/apps/sequencer/Sequencer.cpp`

**Expected savings:** about 1,536 B in `.ccmram_bss` if trimming from 4096 to 2560.

- [ ] Add temporary stack watermark instrumentation for `fsTask`, or identify existing FreeRTOS/task watermark support.
- [ ] Build and flash the instrumented firmware.
- [ ] Exercise worst-case file flows: project save, project load, Teletype file parse, malformed Teletype file, SD card removal/reinsert if safe.
- [ ] Record minimum observed file-task stack headroom.
- [ ] Reduce `CONFIG_FILE_TASK_STACK_SIZE` only if measured headroom remains comfortably above the proposed reduction.
- [ ] Remove temporary instrumentation unless the user wants it kept.
- [ ] Build STM32 `sequencer`.
- [ ] Run the measurement protocol and confirm `.ccmram_bss` drops by the stack delta.
- [ ] Flash hardware and repeat the file-flow checks.
- [ ] Commit only after hardware passes.

**Stop conditions:**

- No trustworthy watermark measurement is available.
- File task headroom is marginal.
- Any file operation freezes, corrupts, or silently fails.

## Phase 3C: Explicitly Deferred High-Risk Options

Do not implement these without a fresh decision:

- P5a: stripping RoutingEngine state entirely. This breaks accumulated behavior for ProgressiveDivider, Location, Envelope, Activity, and FrequencyFollower.
- P9: packing `tele_command_t` tags. This touches parser, op handlers, serialization, and runtime command access.
- P10/P11/P12: lazy page construction, 4bpp framebuffer, flash-backed model. These are architecture changes, not preparatory RAM cleanups.

If one is revived, write a separate plan with its own baseline, test cases, and hardware acceptance criteria.

## Completion Gate

The resource-optimization task can unblock `performer-improvements` when all are true:

- Total runtime RAM is at or below about 111-113 KB, or at least 15 KB below the current 127,420 B baseline.
- `.ccmram_bss` has enough headroom that boot does not depend on fragile stack margins.
- Firmware passes the full hardware smoke checklist after a cold boot.
- All seven track types can be entered and produce plausible output.
- Teletype save/load and runtime script behavior pass the phase checks.
- The final RAM table is recorded in the task notes before any follow-on feature work starts.
