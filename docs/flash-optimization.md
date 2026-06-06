# Flash Optimization Notes

Snapshot: 2026-06-05 13:21:03 MSK.

## Current Constraint

The sequencer app partition is defined by `src/apps/sequencer/sequencer.ld`:

- App origin: `0x08010000`
- App length: `960K` = 983,040 B

The measured STM32 release build before the latest Asteroids build-list removal was:

- ELF: `build/stm32/release/src/apps/sequencer/sequencer`
- `.text=952,068`
- `.preinit_array=4`
- `.init_array=84`
- `.ARM.extab=8,416`
- `.ARM.exidx=13,128`
- `.data=6,600`
- Total loaded flash: 980,300 B
- Headroom to 960K partition: 2,740 B

Post-Asteroids-removal STM32 release measurement:

- `.text=935,252`
- `.preinit_array=4`
- `.init_array=80`
- `.ARM.extab=8,200`
- `.ARM.exidx=12,768`
- `.data=6,572`
- Total loaded flash: 962,876 B
- Headroom to 960K partition: 20,164 B
- Asteroids removal recovered roughly 17-18 KB loaded flash, depending on the exact build snapshot compared.

Loaded flash is not only `.text`. Count `.text`, init arrays, unwind tables, and the ROM load image for `.data`.

## Measurement Commands

Use STM32 release as the source of truth:

```bash
cd build/stm32/release
make sequencer
/opt/homebrew/bin/arm-none-eabi-size -A src/apps/sequencer/sequencer
/opt/homebrew/bin/arm-none-eabi-objdump -h src/apps/sequencer/sequencer
```

Largest final symbols:

```bash
/opt/homebrew/bin/arm-none-eabi-nm --print-size --size-sort --radix=d src/apps/sequencer/sequencer \
  | /opt/homebrew/bin/arm-none-eabi-c++filt \
  | rg ' [TtRrVvWw] ' \
  | tail -80
```

Largest archive/object contributors:

```bash
/opt/homebrew/bin/arm-none-eabi-size \
  src/apps/sequencer/CMakeFiles/sequencer.dir/Sequencer.cpp.obj \
  src/apps/sequencer/libsequencer_shared.a \
  src/libcore.a \
  | awk 'NR==1 {print "flash", $0; next} {print $1+$2, $0}' \
  | sort -n \
  | tail -50
```

## Current Hotspots

Largest final symbols in the pre-Asteroids-removal build:

- `match_token`: 20,280 B
- `stbsp_vsprintfcb`: 16,128 B
- `.ARM.exidx`: 13,128 B
- `Routing::writeTarget(...)`: 11,008 B
- `PhaseFluxEditPage::editSlot(...)`: 9,636 B

Largest flash-bearing object files in the same build:

- `PhaseFluxEditPage.cpp.obj`: ~42.3 KB
- `IndexedSequenceEditPage.cpp.obj`: ~32.8 KB
- `DiscreteMapSequencePage.cpp.obj`: ~31.0 KB
- `StochasticSequenceEditPage.cpp.obj`: ~30.4 KB
- `CurveSequenceEditPage.cpp.obj`: ~30.2 KB
- `TrackPage.cpp.obj`: ~26.9 KB
- `FileManager.cpp.obj`: ~22.8 KB
- `Engine.cpp.obj`: ~22.4 KB
- `stb_sprintf.c.obj`: ~21.3 KB
- `match_token.c.obj`: 20.3 KB
- `TuesdayTrackEngine.cpp.obj`: ~19.7 KB
- `Routing.cpp.obj`: ~19.6 KB

Asteroids was previously a clear removable contributor:

- `Asteroids.cpp.obj`: ~14.5 KB total object footprint in the pre-removal build.
- Current source list comments out `asteroids/Asteroids.cpp` and `ui/pages/AsteroidsPage.cpp`; rerun the STM32 release build before treating that as recovered flash.

Post-Asteroids broad candidates:

```text
20.3K  match_token
16.1K  stbsp_vsprintfcb
12.8K  .ARM.exidx
 8.2K  .ARM.extab
 5.3K  FileManager::readTeletypeTrack
 5.3K  DiscreteMapSequencePage::clusterContextAction
 4.2K  Engine::update
 4.2K  RoutingEngine::updateSinks
 3.8K  NoteTrackEngine::triggerStep
 3.8K  StochasticTrackEngine::triggerStep
 3.7K  PhaseFluxTrackEngine::rebuildSchedule
```

## Reference Performer Flags

Checked `temp-ref` variants:

- `temp-ref/mebitek-performer`
- `temp-ref/modulove-performer`
- `temp-ref/original-performer`
- `temp-ref/performer-nx`
- `temp-ref/phazer-dev-performer`
- `temp-ref/vinx-performer`

All use the same STM32 release optimization shape:

```text
C release:   -O2
C++ release: -O2

common C:
-g -Wall -fdata-sections -ffunction-sections
-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
-mthumb-interwork -funroll-loops -fshort-enums

common C++:
-g -Wall -Wno-unused-function -fdata-sections -ffunction-sections
-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
-mthumb-interwork -funroll-loops -fshort-enums

link:
-nostartfiles --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections
```

Generated STM32 release `link.txt` exists for `mebitek`, `modulove`, and `vinx`; all confirm:

```text
-g -Wall -Wno-unused-function -fdata-sections -ffunction-sections
-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
-mthumb-interwork -funroll-loops -fshort-enums
-O2
-nostartfiles --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections
```

`vinx` skips `-g` only for Emscripten builds. STM32 still has `-g`.

## Candidate Fixes

First low-risk experiment:

- Replace STM32 release `-O2 -funroll-loops` with `-Os`.
- Rebuild.
- Compare loaded flash sections.
- Verify `Engine::update()` timing and hardware feel before accepting.

Reason:

- `-O2` optimizes for speed and can grow firmware.
- `-funroll-loops` intentionally duplicates loop bodies to reduce branch/counter overhead.
- `-Os` optimizes for size while keeping most useful optimizations.

Second experiment:

- Try `-fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables`.
- Verify the firmware still links.
- Current unwind metadata costs about 21.5 KB of flash: `.ARM.extab + .ARM.exidx`.

Third path:

- Refactor monolithic functions with repeated switch/edit/UI logic.
- Start with `Routing::writeTarget(...)` and `PhaseFluxEditPage::editSlot(...)`.
- Use tables/helpers only where they reduce duplicated generated code.

Higher-risk path:

- Reduce Teletype parser/token footprint.
- `match_token` is large, but generated/parser code is more fragile than compiler flag experiments or obvious UI deadweight.

Additional candidate areas:

1. Teletype parser and op code:

   ```text
   20.3K  match_token
   14.4K  patterns.c.obj
   10.6K  maths.c.obj
    7.1K  queue.c.obj
    4.2K  table.c.obj
   ```

   This is a large source of flash, but risk is high. Generated/parser behavior is correctness-sensitive.

2. Formatting:

   ```text
   16.1K  stbsp_vsprintfcb
    2.9K  stbsp__real_to_str
   ```

   Local reductions help:

   - avoid `%f`
   - avoid large `FixedStringBuilder` format paths where lookup strings are enough
   - replace repeated formatted labels with shared helpers/tables

3. Unwind tables:

   ```text
   12.8K  .ARM.exidx
    8.2K  .ARM.extab
   ```

   Experiment with:

   ```text
   -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables
   ```

   Potential win is about 21 KB if the firmware still links and behaves correctly.

4. FileManager Teletype IO:

   ```text
   5.3K  FileManager::readTeletypeTrack
   2.2K  FileManager::writeTeletypeTrack
   22.8K FileManager.cpp.obj
   ```

   Correctness-sensitive. Do not start here.

5. DiscreteMapSequencePage:

   ```text
   31.0K object
    5.3K clusterContextAction
    2.8K distributeActiveStagesEvenly
    2.1K transformContextAction
    1.5K keyPress
    1.4K encoder
   ```

   Good later candidate. Similar context-action bloat to other UI pages.

6. Launchpad:

   ```text
   12.3K LaunchpadController.cpp.obj
   ~9K  Launchpad device variants combined
   ```

   If external controller support is optional, feature-gating Launchpad is a clean flash lever. This is a product decision, not a refactor.

7. Teletype UI:

   ```text
   17.1K TeletypeScriptViewPage.cpp.obj
    5.7K TeletypePatternViewPage.cpp.obj
   ```

   Leave alone if Teletype is core.

8. Engine large functions:

   ```text
   4.2K Engine::update
   4.2K RoutingEngine::updateSinks
   3.8K NoteTrackEngine::triggerStep
   3.8K StochasticTrackEngine::triggerStep
   3.7K PhaseFluxTrackEngine::rebuildSchedule
   3.5K NoteTrackEngine::tick
   3.3K TuesdayTrackEngine::tick
   ```

   Do not optimize first. Timing-sensitive and higher regression risk.

Recommended order:

1. Compiler experiment: `-Os` without `-funroll-loops`.
2. Unwind-table experiment.
3. PhaseFlux `editSlot()` helper refactor. See `docs/pf-flash.md`.
4. Stochastic slot descriptor table. See `docs/editpages-flash.md`.
5. Routing ownership split. See `docs/routing-flash.md`.
6. DiscreteMapSequencePage context-action cleanup.
7. Formatting reductions.
8. Feature gates: Launchpad / non-core UI.
9. Teletype parser only if still flash-constrained.

## Triage Tools

Use binary tools first. They answer what actually costs flash.

- `arm-none-eabi-nm`: per-symbol byte size. Best first pass for custom functions.
- `arm-none-eabi-size`: section totals and per-object/archive-member totals.
- `arm-none-eabi-objdump`: disassembly/source view for one bloated function.
- Linker `.map`: ownership, archive-member pull-in, address layout.

Useful commands:

```bash
# Biggest final functions/data symbols.
/opt/homebrew/bin/arm-none-eabi-nm --print-size --size-sort --radix=d \
  build/stm32/release/src/apps/sequencer/sequencer \
  | /opt/homebrew/bin/arm-none-eabi-c++filt \
  | rg ' [TtRrVvWw] ' \
  | tail -100
```

```bash
# Biggest object files. Approx flash cost = text + data.
/opt/homebrew/bin/arm-none-eabi-size \
  build/stm32/release/src/apps/sequencer/libsequencer_shared.a \
  build/stm32/release/src/libcore.a \
  | awk 'NR==1 {next} {print $1+$2, $0}' \
  | sort -n \
  | tail -80
```

```bash
# Inspect one function at asm/source level.
/opt/homebrew/bin/arm-none-eabi-objdump -Cd --source \
  build/stm32/release/src/apps/sequencer/sequencer \
  | rg -n 'Routing::writeTarget|PhaseFluxEditPage::editSlot'
```

`ast-grep` is useful as a second pass. Use it after `nm` / `size` identify fat files or functions.

Good `ast-grep` targets:

- huge `switch` statements
- repeated `case` bodies
- lambdas in UI edit paths
- `std::function`
- float math calls
- template-heavy helpers
- repeated `StringBuilder` formatting
- large `contextAction`, `editValue`, `draw`, `encoder`, and `keyPress` methods

Installed locally:

```text
/opt/homebrew/bin/ast-grep
/opt/homebrew/bin/sg
```

Graphify is useful for topology, not byte attribution.

Good Graphify questions:

- Why is this function coupled to half the UI?
- What can be deleted without breaking reachability?
- Which page/model/engine clusters are god nodes?

Do not use Graphify as the first source for flash size. Use it after byte triage to understand coupling and deletion/refactor blast radius.

Other useful options:

- `bloaty`: excellent for binary size diffing, but not currently installed here.
- `-Wl,--print-gc-sections`: shows discarded sections. Useful for dead-code questions.
- `-Wl,--cref`: linker cross-reference. Useful to explain why something is pulled in.
- `objdump -drC`: shows literal pools, calls to libm/libstdc++, and duplicated instruction blocks.
- `compile_commands.json` plus clang tools: useful for structured source queries when `ast-grep` is too shallow.

Recommended workflow:

1. Rank final symbols with `nm`.
2. Rank objects/archive members with `size`.
3. Pick one fat function.
4. Inspect it with `objdump`.
5. Use `ast-grep` to find repeated source shape.
6. Refactor one target.
7. Rebuild and compare loaded flash sections.

Current custom-function triage targets:

- `Routing::writeTarget(...)`
- `PhaseFluxEditPage::editSlot(...)`
- `DiscreteMapSequencePage::clusterContextAction(...)`
- `StochasticSequenceEditPage` draw/context methods
- `IndexedSequenceEditPage` draw/encoder methods
