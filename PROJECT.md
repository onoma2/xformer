# Project Facts & Conventions

This file contains all project-specific information, architecture details, build instructions, and coding conventions.

**Engine/timing/IO architecture lives in `docs/performer-architecture.md`** — read that for the
1 kHz update loop, PPQN clock, tick-order guarantees, logical-vs-physical output pipeline,
and per-TrackMode timing patterns (Note / Curve / Indexed / DiscreteMap / Tuesday / Teletype).
This file (`PROJECT.md`) covers the higher-level architecture, build, and conventions.

## Project Overview

**PEW|FORMER** is a fork of the original PER|FORMER eurorack sequencer firmware. This fork maintains the known-good master branch while carefully integrating improvements from other forks, notably jackpf's noise reduction, shape improvements, and MIDI improvements. The firmware has been updated to gcc 14.2 and libopencm3 (October 2024).

The project is a dual-platform embedded system running on both STM32 hardware and a desktop simulator, using FreeRTOS for task management.

## Rules
- Never make user-facing out-of-scope additions/changes without asking the user first.
- **No ProjectVersion bumps during feature development.** The project is in active development on `feat/*` branches. Version bumps happen only when a release is prepared and all features for that release are complete. Adding enum values (e.g. `Routing::Source`, `Routing::Target`) without a version bump is acceptable — old projects will read unknown values as default/None, which is safe enough for dev firmware.

## Build System

This project uses CMake with platform-specific toolchains. Build directories are organized by platform (stm32/sim) and build type (debug/release).

## Development Workflow

**Hardware First development is recommended for all compile checks:**
1. Develop and test features in `build/stm32/release` in the first place
2. If needed for debugging or faster iteration, use simulator (`build/sim/debug`)
3. Always verify final implementation on actual hardware

**When modifying hardware drivers (USB, MIDI, DAC, ADC, GPIO):**
- Never commit changes untested on hardware
- Simulator tests are insufficient for hardware-specific code
- Flash to hardware and verify before committing

**When modifying timing-critical code:**
- Test on actual hardware - simulator timing differs
- Check `Engine::update()` execution time
- Verify against task priorities and stack sizes

**When adding UI features:**
- Consider noise reduction impact (pixel count affects audio noise)
- Test with different brightness/screensaver settings
- Verify screensaver wake mode behavior
- Use simulator screenshot feature to document UI changes

**Hardware-confirmed UI lessons:**
- **Track-type page safety is mandatory.** A page that calls type-specific accessors such as `selectedNoteSequence()`, `selectedCurveSequence()`, `Track::noteTrack()`, `Track::curveTrack()`, or `selectedTrackEngine().as<T>()` must prove the selected track is still that type before every draw/LED/input/context callback, or the navigation layer must replace the page immediately when selected track changes. Track-select handling in `TopPage` must remap stale sequence/edit pages through the appropriate `set*Page()` function after `_project.setSelectedTrackIndex(...)`. Never rely on "this page should only be reachable for this mode"; page-stack callbacks can run after the selected track changes and will hit `SANITIZE_TRACK_MODE`.
- Keep simple UI behavior local unless persistence is truly required. Menu list wrapping is hardware-confirmed when implemented only in `ListPage::setSelectedRow()`.
- Do not add a persisted `MenuWrapSetting` or bump `Settings::Version` just to port Vinx menu wrapping. That path black-screened hardware during this workstream and should be treated as known-bad for this feature.
- `ListPage::setListModel()` must not call `setSelectedRow()`. Some derived pages pass member list models during construction, before those members are fully safe to query. Reset `_selectedRow` and `_displayRow` directly there.
- Construction-time UI crashes may appear as simulator `EXC_BAD_ACCESS` at `_listModel->rows()` and as black screen/unresponsive boot on STM32.
- Use the simulator to catch startup/UI construction crashes quickly, but hardware flash remains the final gate for boot/display behavior.

**Unit and integration tests:**
```bash
cd src/tests/unit        # C++ unit tests
cd src/tests/integration # C++ integration tests
```

**Python-based UI tests:**
```bash
cd src/apps/sequencer/tests
python runner.py
```

## Architecture

> The engine, timing, and I/O internals (1 kHz update loop, PPQN clock, TrackEngine virtuals,
> cross-track reads, DAC/GateOutput drivers, per-TrackMode timing patterns) are documented in
> `docs/performer-architecture.md`. This section covers higher-level project structure.

### Multi-Platform Design

The codebase is structured for cross-platform development:
- **stm32**: Production hardware (STM32F4, ARM Cortex-M4)
- **sim**: Desktop simulator for faster development iteration

Platform abstraction layer located in `src/platform/{stm32,sim}/` with common subdirectories:
- `drivers/`: Hardware device drivers (ADC, DAC, MIDI, USB, display)
- `libs/`: Platform-specific third-party libraries
- `os/`: FreeRTOS abstraction layer
- `test/`: Test runners

### Application Architecture

The sequencer application follows a Model-Engine-UI separation:

**Model** (`src/apps/sequencer/model/`):
- `Model.h`: Top-level data container holding Project, Settings, ClipBoard
- `Project.h`: Musical project data (tracks, sequences, patterns)
- `Settings.h`: Global system settings
- `NoteSequence.h`: Note sequence with multiple editable layers:
  - Gate-related: Gate, GateProbability, GateOffset, Slide, GateMode
  - Retrigger: Retrigger, RetriggerProbability, PulseCount
  - Length: Length, LengthVariationRange, LengthVariationProbability
  - Note: Note, NoteVariationRange, NoteVariationProbability
  - Other: Condition, AccumulatorTrigger
- `Accumulator.h`: Accumulator feature for real-time parameter modulation
- Thread-safe access via `WriteLock` and `ConfigLock`

**Engine** (`src/apps/sequencer/engine/`):
- `Engine.h`: Core sequencer engine orchestrating all track engines
- Implements `Clock::Listener` for timing-critical operations
- Contains `TrackEngineContainer` with specialized engines:
  - `NoteTrackEngine`: Note/gate sequencing
  - `CurveTrackEngine`: CV curve generation
  - `MidiCvTrackEngine`: MIDI-to-CV conversion
- `RoutingEngine`: Signal routing and CV/Gate mapping
- `MidiOutputEngine`: MIDI message generation
- Supports locking (short-term) and suspending (long-term, e.g., file I/O)

**UI** (`src/apps/sequencer/ui/`):
- `Ui.h`: Main UI controller managing display, input, and page navigation
- `PageManager`: Page/screen navigation system
- `ControllerManager`: External controller integration
- `Screensaver`: Power management and noise reduction
- Uses `FrameBuffer` and `Canvas` for graphics rendering
- Shared input helpers on `BasePage` (call these, don't re-inline): `keyboard()` folds USB-keyboard handling (USB F1-F5 → `handleFunctionKeys`); `handleContextMenuKey(KeyPressEvent&)` folds the two panel context-menu gestures (`isContextMenu` → `contextShow()`, Page double-press → `contextShow(true)`). The bare `if (key.pageModifier()) return;` catch-all stays inline per page — after that page's Page+Step/QuickEdit/Function handlers, never bundled into the helper (it would swallow them).

### Task Architecture (FreeRTOS)

Five concurrent tasks with defined priorities (see `src/apps/sequencer/Config.h`):
1. Driver Task (priority 5, 1024 stack): Low-level hardware I/O
2. Engine Task (priority 4, 4096 stack): Sequencer engine updates
3. USB Host Task (priority 3, 2048 stack): USB MIDI device handling
4. UI Task (priority 2, 4096 stack): Display rendering and input
5. File Task (priority 1, 2048 stack): SD card operations
6. Profiler Task (priority 0, 2048 stack): Performance monitoring

### Key Configuration Constants

Defined in `src/apps/sequencer/Config.h`:
- `CONFIG_PPQN`: 192 (parts per quarter note for timing)
- `CONFIG_SEQUENCE_PPQN`: 48 (sequence resolution)
- `CONFIG_CHANNEL_COUNT`: 8 (CV/Gate channels)
- `CONFIG_CV_INPUT_CHANNELS`: 4
- `CONFIG_CV_OUTPUT_CHANNELS`: 8
- `CONFIG_DEFAULT_UI_FPS`: 50

### Critical Subsystems

**Clock System:**
- Master clock with external, MIDI, and USB MIDI sync sources
- `TapTempo` and `NudgeTempo` for interactive tempo control
- High-precision timing via `ClockTimer` driver

**MIDI System:**
- Dual MIDI ports (hardware UART + USB)
- `MidiLearn`: MIDI mapping learn functionality
- `CvGateToMidiConverter`: Convert CV/Gate to MIDI messages
- Overflow tracking for diagnostics

**Display/Noise Reduction:**
- OLED display noise affects audio outputs (documented in `doc/improvements/noise-reduction.md`)
- Noise reduction settings: brightness, screensaver, wake mode, dim sequence
- Footer UI uses bold instead of highlight to reduce noise

## jackpf Improvements Integration

Three major improvement categories documented in `doc/improvements/`:
1. **Noise reduction** (`noise-reduction.md`): Display settings to minimize OLED noise
2. **Shape improvements** (`shape-improvements.md`): Enhanced CV curve generation
3. **MIDI improvements** (`midi-improvements.md`): Extended MIDI functionality

## Resource Footprint and Budget

RAM is the tight resource. A feature is acceptable only if it stays within the budget below or identifies the exact RAM tradeoff it consumes.

**Flash ceiling snapshot (2026-06-13).**
- Sequencer app partition: `src/apps/sequencer/sequencer.ld` places the app at `0x08010000` with `LENGTH = 960K` = 983,040 B.
- Current STM32 release build: `build/stm32/release/src/apps/sequencer/sequencer`.
- Loaded flash sections from `arm-none-eabi-size -A`: `.text=934,124`, `.preinit_array=4`, `.init_array=80`, `.ARM.extab=8,228`, `.ARM.exidx=12,704`, `.data=6,572`.
- Total loaded flash = 961,712 B (= the flashed `sequencer.bin`). Headroom to 960K partition = 21,328 B. (Prior 2026-06-05 snapshot was 2,740 B — the routing-matrix reclaims, legacy collapse, and shaper-branch delete freed ~18.6 KB net.)
- Flash measurement command:
  ```bash
  cd build/stm32/release
  make sequencer
  /opt/homebrew/bin/arm-none-eabi-size -A src/apps/sequencer/sequencer
  /opt/homebrew/bin/arm-none-eabi-objdump -h src/apps/sequencer/sequencer
  ```
- Symbol-level culprit command:
  ```bash
  /opt/homebrew/bin/arm-none-eabi-nm --print-size --size-sort --radix=d src/apps/sequencer/sequencer \
    | /opt/homebrew/bin/arm-none-eabi-c++filt \
    | rg ' [TtRrVvWw] ' \
    | tail -80
  ```
- Current largest flash symbols include `match_token` (20,280 B), `stbsp_vsprintfcb` (16,128 B), `.ARM.exidx` (12,704 B), `PhaseFluxEditPage::editSlot(...)` (9,770 B), and `Engine::update()` (7,612 B). (`Routing::writeTarget` is gone — deleted in the routing-matrix legacy collapse.)
- Object/module culprit command:
  ```bash
  /opt/homebrew/bin/arm-none-eabi-size \
    src/apps/sequencer/CMakeFiles/sequencer.dir/Sequencer.cpp.obj \
    src/apps/sequencer/libsequencer_shared.a \
    src/libcore.a \
    | awk 'NR==1 {print "flash", $0; next} {print $1+$2, $0}' \
    | sort -n \
    | tail -50
  ```
- Current largest flash-bearing objects include `PhaseFluxEditPage.cpp.obj` (~42.0 KB), `IndexedSequenceEditPage.cpp.obj` (~32.8 KB), `DiscreteMapSequencePage.cpp.obj` (~31.0 KB), `StochasticSequenceEditPage.cpp.obj` (~30.1 KB), `CurveSequenceEditPage.cpp.obj` (~29.2 KB), `Engine.cpp.obj` (~24.9 KB), `TrackPage.cpp.obj` (~24.8 KB), `RoutingPage.cpp.obj` (~23.1 KB), `FileManager.cpp.obj` (~22.8 KB), `stb_sprintf.c.obj` (~21.3 KB), and `NoteSequenceEditPage.cpp.obj` (~21.0 KB).

**Budget targets:**
- Main SRAM target: `.data + .bss` at or below roughly **111-113 KB**.
- Hard warning zone: `.data + .bss` above **120 KB**. Feature work in this zone needs explicit symbol-level justification.
- CCMRAM target: `.ccmram_bss` below roughly **56 KB** unless hardware testing proves safe margin.
- Flash is secondary right now; still measure `.text`, but do not optimize flash before RAM unless flash is the stated task.

**Current measured baseline (2026-06-13). Refresh this block after resource-optimization work or before major feature work.**
- Flash: `.text` = 934,124 B (≈ 912 KB); total loaded image 961,712 B, 21,328 B under the 960K partition.
- SRAM (128 KB): `.data=6,572` + `.bss=112,720` = 119,292 B, about 91.0% used (still in the hard warning zone, ~708 B under the 120 KB threshold — flat vs the 2026-05-24 baseline). Largest blocks:
  - `Model` = 88,312 B — dominates main SRAM. Inside it, `Project._tracks` = 80,960 B = 8 × 10,120 B track-variant container. The container is sized to the largest track mode (CurveTrack ≈ 10,092 B, NoteTrack ≈ 9,544 B); StochasticTrack at 8,444 B already fits, so stochastic-side shrinks do not free model SRAM unless every variant shrinks below the cap.
  - LCD driver DMA buffer = 8,192 B normal SRAM.
  - Teletype file slot globals = 4 × 1,226 B.
  - USB host / filesystem globals ≈ 6 KB, mostly functional buffers.
- CCM RAM (64 KB): `.ccmram_bss=48,352`, about 73.8% used (~6.7 KB freed since 2026-05-24 — routing-matrix removed `shaperState[8]` + per-route shaper state). Largest blocks:
  - `Ui` = 27,752 B (includes 16,384 B 8-bit Canvas framebuffer).
  - `Engine` = 11,940 B (engine-task slot — Stochastic cache adds 320 B per active stochastic track inside this slot; variant container also sized to the largest engine).
  - FreeRTOS static stacks: engine/fs/ui = 4,256 B each; usbh/profiler = 2,208 B each.
- FreeRTOS heap disabled (static allocation only).
- StochasticSequence = 496 B (64 events × 6 B = 384 B + 112 B knobs/seeds/tickets); StochasticTrack = 8,444 B (17 sequences × 496 B + ~12 B track-global).

## C++ Heap (`new` / `malloc`)

**There is no dedicated heap.** The linker reserves only `.data` + `.bss` in main SRAM and leaves the gap between `end-of-.bss` and `top-of-stack` available for both libc heap and the call stack. The stack pointer starts at `_stack = ORIGIN(RAM) + LENGTH(RAM)` and grows down. The libc `_sbrk` walks `end` upward and **does not check against the stack pointer**, so a successful `new` can return memory that overlaps the live stack region. The fault manifests later as a hardfault when the stack scribbles into the "heap" allocation (or vice versa).

**Numbers, current build:**
- RAM = 128 KB = 131,072 B.
- `.data` + `.bss` (main SRAM portion only — `.ccmram_bss` lives separately) ≈ 119 KB.
- Free space for heap + stack combined ≈ **12 KB**.
- Practical stack ceiling is ~4 KB (worst-case ISR + audio path + UI draw). That leaves ~**8 KB** that any `new`/`malloc` can safely consume — across **all** allocators, for **all** tracks, **for the lifetime of the process**.

**Recorded incident (2026-05-22):** `StochasticTrackEngine::initLockedSteps()` was allocating `new LockedParentEvent[CONFIG_STEP_COUNT]` = 5,888 B per stochastic track at engine construction. Two stochastic tracks → 11,776 B → wiped out the entire 12 KB heap+stack window, leaving 52 B for stack → hardfault on the next deep call after project load. One stochastic track worked because 5,940 B was enough stack headroom; two didn't. The heap-allocated lock cache was in the design from the very first stochastic commit, so this risk was latent since stochastic shipped.

**Resolution (2026-05-22):** The lock-cache heap allocation, the `LockedParentEvent`/`LockedChild` structs, the lock-replay branch in `triggerStep`, the `captureLockedParent` helper, the `lockAvailable()` accessor, and the engine destructor were all deleted in the same commit. `.text` shrunk by ~2,548 B. The user-facing Lock toggle on `StochasticTrack` remains as a UI placeholder; the engine no longer reads it. Hardware-verified with 8 stochastic tracks — no crash. Redesign deferred — see `.tasks/stochastic-track-port/LOCK-DESIGN-DEFERRED.md`. The current preferred implementation is engine-flag (RNG-state snapshot + restore on cycle wrap, ~20 B/track engine state, no heap, no tape) — modeled on `TuesdayTrackEngine::tick:1870-1883` which already ships this pattern. Flat-tape design kept as fallback.

**Rules for new heap users:**
1. **Default to no heap.** Every track-mode struct, every engine subobject, every UI page should live in `.bss` (or in a container with statically-sized variants). Heap is for genuinely dynamic content with no static upper bound.
2. **If you genuinely need dynamic allocation, audit the budget first.** Sum the worst case across all 8 tracks, all UI pages, all generator previews, all transient buffers. Don't reason from "I'll probably only use it once" — products run in arbitrary order.
3. **`new (std::nothrow)` + a nullptr check is not safety.** `_sbrk` may succeed and return stack-overlapping memory. The null check fires only on systems where `_sbrk` enforces a ceiling, which this one doesn't.
4. **When in doubt, use a file-scope BSS pool indexed by track index** (pattern: `gDirectHistory[CONFIG_TRACK_COUNT][N]` in `StochasticTrackEngine.cpp`). Predictable at link time, the size-assert and `.bss` totals catch overflow at build time, and there's no per-track allocation race.
5. **Existing heap users (audit list, refresh when changed):**
   - `SequenceBuilder<T>::_preview` — generator A/B preview clone. Allocated when entering the Generator page, freed on exit. Worst case ~9.5 KB (size of `NoteTrack`). Currently the only runtime heap user in the engine path.
   - Static `std::string` / `std::vector` in `UserSettings` and `TeletypeTrack::getAvailable*` — initialized at static-init time, so charged to "heap usage already in steady state" rather than runtime allocation. Still counts against the 12 KB window.

**Removed heap users (kept for the audit trail):**
- `StochasticTrackEngine::_lockedParents` — `LockedParentEvent[64]` per track, 5,888 B per track. Removed 2026-05-22 after the heap–stack collision was diagnosed. Lock semantics are deferred (see `.tasks/stochastic-track-port/LOCK-DESIGN-DEFERRED.md`). The model-side `_lock` flag remains as a UI placeholder; engine ignores it until the new implementation lands, possibly via the fractal-track trunk substrate.

**Practical heap budget:** Treat heap as ~**6 KB usable** after accounting for stack headroom and the SequenceBuilder preview slot. Any new heap allocator must fit within that or replace an existing user.

## RAM-heavy structures
- `Model`: top-level `Track::_container` is currently sized by `NoteTrack`, not `CurveTrack`. Current MonitorPage values: `Track=9560`, `NoteTrack=9544`, `CurveTrack=9480`, `Model=88072`.
- `Engine`: top-level engine storage is sized by the largest track engine, currently TeletypeTrackEngine-class storage. Current MonitorPage `Engine=11492`.
- `Ui`: framebuffer, `Pages` aggregate of all page instances and list/selection models, MIDI receive ring buffer.
- Display buffers: UI has a 16,384 B 8-bit working framebuffer in CCMRAM; STM32 LCD driver has an 8,192 B packed DMA buffer in normal SRAM.
- Misc: Teletype file PatternSlot globals, FS pools, USB/MIDI state, idle/timer stacks.

**USB/FS note:**
- USB/FS was audited as a possible hidden SRAM target. It is not one: realistic no-behavior-change recovery is only about 700-1,000 B.
- Safe candidate: `DirBuf` (~608 B) appears dead when `FF_USE_LFN=0`; guard/remove it only after confirming the FatFs config path.
- Reducing USB device counts or `BUFFER_ONE_BYTES` is a product/compatibility change, not a transparent optimization.

**Display buffer note:**
- Do not treat `Lcd::_frameBuffer` removal as a near-term easy SRAM win. The 8,192 B normal-SRAM buffer exists because STM32 DMA cannot read the UI framebuffer in CCMRAM.
- CPU-SPI streaming or line-buffered DMA would save SRAM but sacrifice the current full-frame asynchronous DMA path and likely require a lower display FPS on complex pages.
- Keep LCD low-RAM mode as last-resort research only. Packed Canvas rendering is a separate CCMRAM cleanup idea, not a main-SRAM recovery path.

## RAM Gates for Feature Development

Treat RAM as a gate only when a feature changes the relevant container maximum or adds direct RAM residents. Use ARM build/probes, not host estimates.

**Container mental model:**
- `Container<A, B, C>` is static variant storage implemented as an aligned byte buffer sized to `max(sizeof(A), sizeof(B), sizeof(C))`.
- `_container.create<T>()` placement-news one active type into that storage; `_container.as<T>()` reinterprets the same storage as the active type.
- Every owner pays for the largest possible type, not the currently selected type. Eight `Track` objects means eight max-sized `Track::_container` buffers even if some tracks are tiny modes.
- Small track-type savings matter only when they reduce the current largest arm or a direct member outside the container.

**Container gate formula (both Model and Engine):**

Both the model-side `Track::_container` and the engine-side `Engine::TrackEngineContainer` are `Container<...>` variants instantiated **8 times** (`CONFIG_TRACK_COUNT = 8`). The RAM cost is:

```
container_array_total = 8 × max(sizeof(variant_types))
```

- For the **model**: `8 × max(NoteTrack, CurveTrack, MidiCvTrack, …)` lives in normal SRAM as part of `Model`.
- For the **engine**: `8 × max(NoteTrackEngine, CurveTrackEngine, …, StochasticTrackEngine)` lives in CCMRAM as part of `Engine`.

Since the container is always sized to its largest variant, **adding fields to a non-largest variant is free until that variant exceeds the current max** — the wasted bytes in non-largest variant slots already exist. Conversely, growing the largest variant by N bytes costs `8 × N` across the container array.

This is why fold-into-engine often wins over file-scope CCMRAM allocations: existing slot-size waste absorbs the new fields, while a separate global pays the full size × 8.

**Model/track-type gate:**
- Current max: `NoteTrack=9544 B`; `Track=9560 B`.
- A new or modified track model below 9544 B should not increase top-level `Model` RAM through the 8-track `Track::_container`.
- If a track model exceeds 9544 B, the excess is multiplied by 8 in the `Model` singleton.
- Since `CurveTrack=9480 B`, sequence-packing follow-ups have low current ROI: only 64 B per Track slot is available before CurveTrack becomes the max again.
- `TeletypeTrack=7104 B` is below the model container gate. Teletype model cleanup is not a current top-level model RAM win unless the container architecture or gate changes.

**Engine gate:**
- Current measured max on `feat/stochastic-seed-log` (Phase 14B Patch B follow-on, 2026-05-22): `TeletypeTrackEngine=912 B`; `Engine::TrackEngineContainer=912 B`. STM32 release: `.text=889,236`, `.data=6,416`, `.bss=167,996`, `.ccmram_bss=55,084 B`, **CCMRAM free ≈ 10.2 KB**.
- Next-largest measured engine: `NoteTrackEngine=588 B`; conditional direct gap is `(912 - 588) * 8 = 2592 B` CCMRAM.
- A new or modified track engine below 912 B should not inflate the engine container.
- If it exceeds the current largest engine, the excess is multiplied by 8 engine slots in CCMRAM.
- Shrinking or extracting `TeletypeTrackEngine` can save CCMRAM across all 8 engine slots if the current "any/all 8 tracks can be Teletype" behavior is changed or if the engine itself shrinks below 588 B.
- **Worked example (Phase 14B engine cache).** A ~448 B per-track cache was first allocated as a file-scope `CCMRAM_BSS gStochasticCaches[8]` (3,616 B total). Moving the cache into `StochasticTrackEngine` as a member reclaimed the full 3,616 B with no container-array growth, because `StochasticTrackEngine + cache` still fit under the `TeletypeTrackEngine=912 B` slot ceiling. **Pattern:** when adding runtime state for a non-largest engine variant, prefer engine-member placement over file-scope CCMRAM globals — the container's existing slot waste absorbs it.

**Direct RAM still matters:**
- New globals, static buffers, queues, page members, lookup tables, file buffers, task stacks, and DMA buffers count directly even if container gates do not move.
- Immutable tables should be `const`/flash-resident instead of `.data`.
- DMA buffers must generally live in normal SRAM; CCMRAM is not DMA-accessible on STM32F4.

**Feature RAM check:**
- Build with `cd build/stm32/release && make sequencer`.
- Check `.data`, `.bss`, `.ccmram_bss`, `Model`, `Engine`, `Track`, changed track type sizes, and changed engine type sizes.
- If flash grows but RAM sections and container gates stay flat, RAM should not block the feature.
- If RAM grows, identify the exact symbol or container cliff before proposing broader architecture changes.

## Simulator Workflow (Reference Forks)

The project maintains reference forks in `temp-ref/` for comparing features (vinx-performer, mebitek-performer). Each can be built and launched via a shared macOS `.app` bundle.

### Building a Simulator

```bash
cd temp-ref/vinx-performer && mkdir -p build/sim/debug
cd build/sim/debug && cmake -DPLATFORM=sim ../../.. && make -j$(nproc)
```

### macOS App Bundle (Keyboard Focus Fix)

Running the simulator binary directly from the terminal causes macOS to route keyboard events to Terminal.app instead of the SDL window. The fix is a `.app` bundle wrapper:

```
build/sim/Performer.app/
  Contents/Info.plist
  Contents/MacOS/performer    ← shell script wrapper (resolves build dir + assets)
  Contents/MacOS/sequencer    ← symlink → actual binary
```

The wrapper (`performer`) `cd`s to the binary's build directory (where the `assets/` symlink lives), then runs the binary with `| tee /tmp/performer-sim.log` for diagnostics.

**To switch to a different fork/build:** repoint the symlink:

```bash
# Vinx
ln -sf ../../../../../temp-ref/vinx-performer/build/sim/debug/src/apps/sequencer/sequencer \
       build/sim/Performer.app/Contents/MacOS/sequencer

# Xformer
ln -sf ../../../../../build/sim/debug/src/apps/sequencer/sequencer \
       build/sim/Performer.app/Contents/MacOS/sequencer
```

**Launch:**

```bash
open build/sim/Performer.app          # non-blocking (bg process)
open -W build/sim/Performer.app       # blocks until exit
```

(`open -W` may fail with PID errors on macOS 15.x — use `open` without `-W` and check `/tmp/performer-sim.log` for output.)

### Demo Project (Simulator Boot)

The project the simulator boots with is **not a file** — it is assembled in code
in `Project::clear()` inside an `#if PLATFORM_SIM` block
(`src/apps/sequencer/model/Project.cpp`). Edit that block to change what plays on
a fresh simulator launch. Hardware boots the same `clear()` without the block.

Current layout: tempo 80, Minor scale. Track 1 a slow full-range Curve (CV
source); tracks 2–6 NoteTracks with assorted gate patterns; track 8 a PhaseFlux
track that boots straight from `PhaseFluxSequence::clear()` — stages 0..3 active
at 4 pulses each over a 1-bar cycle at the 1/16 divisor, a NoteTrack-equivalent
1/16 metronome in sync with the NoteTracks (octave +1, no per-stage override).

### Simulator Keyboard Mapping

| Physical Button  | Keyboard Key |
|------------------|--------------|
| Step 1-8         | `Z X C V B N M ,` |
| Step 9-16        | `A S D F G H J K` |
| Track 1-8        | `Q W E R T Y U I` |
| 4 round buttons  | `1 2 3 4` |
| Left / Right     | `← →` |
| Shift            | Left Shift |
| Page             | Left Alt |
| Encoder push     | Space |
| Encoder rotate   | Drag mouse on encoder widget |

## Build & test
- **STM32 release build (REQUIRED for all compile checks):** `cd build/stm32/release && make sequencer`
- Simulator build: `cd build/sim/debug && make sequencer` — **DO ONLY IF USER EXPLICITLY ASKS.**
- **Always use the STM32 release build to verify compilation.** The sim build uses a different toolchain (host clang vs arm-none-eabi-gcc) and can mask STM32-specific issues. Only the STM32 build catches cross-compilation errors.

## Fresh-worktree bootstrap

`git worktree add` only checks out tracked files. `build/`, the libopencm3 submodule, and ragel-generated teletype sources do not come along. Bootstrap a new worktree with:

```bash
# 1. Create worktree (branch from master or wherever)
git worktree add .worktrees/<slug> -b feat/<slug> master

# 2. Init the STM32 submodule (skip the sim-only ones unless needed)
git -C .worktrees/<slug> submodule update --init --recursive \
    src/platform/stm32/libs/libopencm3

# 3. Generate ragel sources (gitignored in teletype/.gitignore)
( cd .worktrees/<slug>/teletype/src \
  && ragel -C -G2 match_token.rl -o match_token.c \
  && ragel -C -G2 scanner.rl -o scanner.c )

# 4. Configure cmake (absolute paths — relative breaks if cwd drifts)
cmake -S "$PWD/.worktrees/<slug>" \
      -B "$PWD/.worktrees/<slug>/build/stm32/release" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$PWD/.worktrees/<slug>/cmake/arm.cmake" \
      -DPLATFORM=stm32

# 5. Build
cd .worktrees/<slug>/build/stm32/release && make sequencer -j8
```

Requirements: `ragel` on PATH (`brew install ragel`), `arm-none-eabi-gcc` toolchain installed at the path referenced by `cmake/arm.cmake`. Do not share `build/` between worktrees — cmake caches absolute source paths and branch contents diverge.

`.worktrees/` is gitignored at repo root.

## Considerations
- RAM is the tight constraint; flash has plenty of margin.
- To free RAM: shrink note-step fields/pattern count/snapshots, reduce UI/page caches, or trim task stack sizes; prefer moving non-DMA data to CCM if SRAM pressure rises.
- **Never commit ANY changes without testing on hardware.** Every commit must be flashed and verified on the STM32 module before pushing.
- **HARD RULE: No ProjectVersion bump without explicit user request.** We are in active development consolidating many features; version bumps are done on request only, not automatically when adding model fields. Write new fields unconditionally; no read guards needed during dev. **Old dev-stage project files are accepted to break — there is no requirement to preserve cross-branch file layout, add migration code, or anticipate future version-guarded reads.** Any spec or plan doc that says "must bump", "requires bumping", "before merge", or proposes a `dataVersion() >= VersionN` guard is stale — flag and update it instead of complying. Version bumps and read guards are batched once at release-prep time, not per-feature, and only when the user explicitly says so.

## Testing Conventions and Common Errors

### Test Framework

**CRITICAL**: This project uses a **custom UnitTest.h framework**, NOT Catch2 or Google Test.

**Correct Test Structure:**
```cpp
#include "UnitTest.h"

UNIT_TEST("TestName") {

CASE("test_case_name") {
    // Test code here
    expectEqual(actual, expected, "optional message");
    expectTrue(condition, "optional message");
    expectFalse(condition, "optional message");
}

} // UNIT_TEST("TestName")
```

**Common Test Framework Errors:**

❌ **WRONG** (Catch2 style):
```cpp
#include "catch.hpp"

TEST_CASE("Description", "[tag]") {
    REQUIRE(condition);
}
```

✅ **CORRECT** (UnitTest.h style):
```cpp
#include "UnitTest.h"

UNIT_TEST("TestName") {
CASE("description") {
    expectTrue(condition, "message");
}
}
```

**Assertion Functions:**
- `expectEqual(a, b, msg)` - Compare values (int, float, const char*)
- `expectTrue(condition, msg)` - Assert true
- `expectFalse(condition, msg)` - Assert false
- `expect(condition, msg)` - Generic assertion

**Enum Comparison:**
- Always cast enums to `int` for expectEqual:
```cpp
expectEqual(static_cast<int>(actual), static_cast<int>(expected), "message");
```

### Type System Conventions

**Clamp Function Type Matching:**

The `clamp()` function requires all three arguments to be the **same type**.

❌ **WRONG**:
```cpp
_masterTrackIndex = clamp(index, int8_t(0), int8_t(7));  // Type mismatch!
_harmonyScale = clamp(scale, uint8_t(0), uint8_t(6));   // Type mismatch!
```

✅ **CORRECT**:
```cpp
_masterTrackIndex = clamp(index, 0, 7);  // All int
_harmonyScale = clamp(scale, 0, 6);      // All int
```

The compiler will assign to the correct member variable type automatically.

**Enum Conventions in Model Layer:**

Model enums follow specific patterns:

✅ **CORRECT** (plain enum, no Last):
```cpp
enum HarmonyRole {
    HarmonyOff = 0,
    HarmonyMaster = 1,
    HarmonyFollowerRoot = 2,
    // ... no Last member
};
```

❌ **WRONG** (enum class with Last):
```cpp
enum class HarmonyRole {  // Don't use "class"
    HarmonyOff = 0,
    Last  // Don't add Last in model enums
};
```

**Note**: UI/Pages enums DO use `enum class` and `Last` - this convention is specific to model layer.

### Bitfield Packing

**Available Space Check:**
```cpp
// In NoteSequence::Step::_data1 union
// Check comments for remaining bits:
BitField<uint32_t, 20, GateMode::Bits> gateMode;  // bits 20-21
// 10 bits left  <-- Always documented
```

**Serialization Pattern:**
```cpp
// Write (bit-pack multiple values into single byte)
uint8_t flags = (static_cast<uint8_t>(_role) << 0) |
                (static_cast<uint8_t>(_scale) << 3);
writer.write(flags);

// Read (unpack with masks)
uint8_t flags;
reader.read(flags);
_role = static_cast<Role>((flags >> 0) & 0x7);   // 3 bits
_scale = (flags >> 3) & 0x7;                      // 3 bits
```

### Common Compilation Errors

**Error: "no member named 'X' in 'ClassName'"**
- Cause: Using undefined enum or method
- Fix: Check if you're using plain enum (not enum class) for model enums
- Fix: Ensure you've added the member to the class definition

**Error: "no matching function for call to 'clamp'"**
- Cause: Type mismatch in clamp arguments
- Fix: Use `clamp(value, 0, max)` without type casts

**Error: "undefined symbols for architecture"**
- Cause: Missing CMakeLists.txt registration
- Fix: Add `register_sequencer_test(TestName TestName.cpp)` to CMakeLists.txt

### Test Organization

**Unit Test Location:**
- `src/tests/unit/sequencer/` - Model and small unit tests
- `src/tests/integration/` - Integration tests (less common)

**Test Naming:**
- File: `TestFeatureName.cpp`
- Test: `UNIT_TEST("FeatureName")`
- Cases: `CASE("descriptive_lowercase_name")`

**Example Test Structure:**
```cpp
#include "UnitTest.h"
#include "apps/sequencer/model/YourClass.h"

UNIT_TEST("YourClass") {

CASE("default_values") {
    YourClass obj;
    expectEqual(obj.value(), 0, "default value should be 0");
}

CASE("setter_getter") {
    YourClass obj;
    obj.setValue(42);
    expectEqual(obj.value(), 42, "value should be 42");
}

CASE("clamping") {
    YourClass obj;
    obj.setValue(1000);  // Over max
    expectEqual(obj.value(), 127, "value should clamp to 127");
}

} // UNIT_TEST("YourClass")
```

### Reference Examples

**Good test examples to copy from:**
- `src/tests/unit/sequencer/TestAccumulator.cpp` - Model testing
- `src/tests/unit/sequencer/TestNoteSequence.cpp` - Property testing
- `src/tests/unit/sequencer/TestHarmonyEngine.cpp` - Lookup table testing
- `src/tests/unit/sequencer/TestPulseCount.cpp` - Feature testing

## LFO-Shape Population Enhancement

### Overview
Enhancement to provide quick access to common LFO waveforms for CurveTrack sequences.

### Implemented Features
1. **Core Functions Added to CurveSequence:**
   - `populateWithLfoShape(Curve::Type shape, int firstStep, int lastStep)`
   - `populateWithLfoPattern(Curve::Type shape, int firstStep, int lastStep)`
   - `populateWithLfoWaveform(Curve::Type upShape, Curve::Type downShape, int firstStep, int lastStep)`
   - `populateWithSineWaveLfo(int firstStep, int lastStep)`
   - `populateWithTriangleWaveLfo(int firstStep, int lastStep)`
   - `populateWithSawtoothWaveLfo(int firstStep, int lastStep)`
   - `populateWithSquareWaveLfo(int firstStep, int lastStep)`

2. **UI Integration:**
   - Added to CurveSequencePage context menu:
     - "LFO-TRIANGLE" - Populates with triangle wave pattern
     - "LFO-SINE" - Populates with sine wave approximation
     - "LFO-SAW" - Populates with sawtooth wave pattern
     - "LFO-SQUARE" - Populates with square wave pattern

3. **LFO Shape Mappings:**
   - Sine wave: Uses `Curve::Bell` (0.5 - 0.5 * cos(x * 2π))
   - Triangle wave: Uses `Curve::Triangle`
   - Sawtooth wave: Uses `Curve::RampUp` (ascending) or `Curve::RampDown` (descending)
   - Square wave: Uses `Curve::StepUp` (rising edge) or `Curve::StepDown` (falling edge)

### Resource Requirements
- **RAM Impact**: ~30 bytes for LFO functions
- **Processing Overhead**: ~6,400-9,600 cycles for full sequence population (non real-time)
- **Flash Memory**: ~800-1,300 bytes
- **Audio Performance Impact**: None during normal operation
