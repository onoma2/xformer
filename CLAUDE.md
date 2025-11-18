# CLAUDE.md

You are amazing STM32 coder with advanced musical acumen, aware of EURORACK CV
conventions and OLED ui design constraints. You work only in TEST DRIVEN
DEVELOPMENT methodology.

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**PEW|FORMER** is a fork of the original PER|FORMER eurorack sequencer firmware. This fork maintains the known-good master branch while carefully integrating improvements from other forks, notably jackpf's noise reduction, shape improvements, and MIDI improvements. The firmware has been updated to gcc 14.2 and libopencm3 (October 2024).

The project is a dual-platform embedded system running on both STM32 hardware and a desktop simulator, using FreeRTOS for task management.

## Build System

This project uses CMake with platform-specific toolchains. Build directories are organized by platform (stm32/sim) and build type (debug/release).

### Initial Setup

**First-time setup requires cloning with submodules:**
```bash
git clone --recursive https://github.com/djphazer/performer.git
```

**For STM32 hardware development:**
```bash
make tools_install    # Installs ARM toolchain (gcc 14.2) and OpenOCD
make setup_stm32      # Creates build/stm32/{debug,release} directories
```

**For simulator development:**
```bash
make setup_sim        # Creates build/sim/{debug,release} directories
```

### Building

**Build for STM32 hardware (release):**
```bash
cd build/stm32/release
make -j sequencer                # Main sequencer app
make -j sequencer_standalone     # Sequencer without bootloader
make -j bootloader               # Bootloader
make -j tester                   # Hardware tester
```

**Build for simulator (debug):**
```bash
cd build/sim/debug
make -j
./src/apps/sequencer/sequencer   # Run from build directory
```

**Flash to hardware:**
```bash
cd build/stm32/release
make -j flash_bootloader
make -j flash_sequencer
```

Note: Flashing uses OpenOCD with Olimex ARM-USB-OCD-H JTAG by default. To use a different JTAG, edit `OPENOCD_INTERFACE` in `src/platform/stm32/CMakeLists.txt`.

### Testing

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

## Accumulator Feature

The PEW|FORMER firmware includes an advanced accumulator feature that provides powerful real-time parameter modulation capabilities. See `QWEN.md` for complete implementation documentation.

### Overview

An accumulator is a stateful counter that increments/decrements based on configurable parameters and updates when specific sequence steps are triggered. It modulates musical parameters (currently pitch, with potential for expansion to gate length, probability, and CV curves).

### Core Parameters

- **Enable**: On/off control
- **Mode**: Stage or Track level operation
- **Direction**: Up, Down, or Freeze
- **Order**: Boundary behavior modes
  - **Wrap**: Values wrap from max to min and vice versa
  - **Pendulum**: Bidirectional counting with direction reversal at boundaries
  - **Random**: Generates random values within min/max range when triggered
  - **Hold**: Clamps at min/max boundaries without wrapping
- **Polarity**: Unipolar or Bipolar range
- **Value Range**: Min/Max constraints (-100 to 100)
- **Step Size**: Amount to increment/decrement per trigger (1-100)
- **Current Value**: The current accumulated value (read-only)
- **Trigger Mode**: Controls when accumulator increments (NEW in 2025-11-17)
  - **STEP**: Increment once per step (default, backward compatible)
  - **GATE**: Increment per gate pulse (respects pulse count and gate mode)
  - **RTRIG**: Increment per retrigger subdivision (note: all ticks fire immediately at step start, not spread over time)

### Trigger Mode Details

The trigger mode parameter controls WHEN the accumulator increments during step playback:

**STEP Mode (default):**
- Ticks once per step when accumulator trigger is enabled on that step
- Independent of pulse count, gate mode, and retrigger settings
- Example: 4 steps with triggers → 4 increments total

**GATE Mode:**
- Ticks once per gate pulse
- Respects pulse count and gate mode settings:
  - `pulseCount=3, gateMode=ALL` → 4 ticks (pulses 1, 2, 3, 4)
  - `pulseCount=3, gateMode=FIRST` → 1 tick (pulse 1 only)
  - `pulseCount=3, gateMode=HOLD` → 1 tick (pulse 1 only)
  - `pulseCount=3, gateMode=FIRSTLAST` → 2 ticks (pulses 1 and 4)
- Useful for rhythmic accumulation patterns tied to pulse density

**RTRIG Mode (Retrigger):**
- Ticks N times for N retriggers
- `retrig=1` → 1 tick
- `retrig=3` → 3 ticks
- `retrig=7` → 7 ticks
- **Known behavior**: All N ticks fire immediately when step starts (not spread over time)
  - Retrigger gates fire spread over time (you hear ratchets)
  - But accumulator increments happen upfront, all at once
  - This is an architectural limitation
  - See `RTRIG-Timing-Research.md` for technical investigation and workaround analysis
  - See `Queue-BasedAccumTicks.md` for detailed implementation plan if future enhancement needed
- Useful for step-based bursts of accumulation

**Delayed First Tick:**
All modes respect the delayed first tick feature (Improvement 3):
- First tick after PLAY is skipped (prevents jump on start)
- Subsequent steps tick normally
- Reset via STOP button or `accumulator.reset()`

### UI Integration

**AccumulatorPage ("ACCUM"):**
- Parameter editing interface using list-based layout
- All configurable parameters accessible via encoder
- TRIG parameter: Turn encoder to cycle STEP → GATE → RTRIG
- Real-time value updates

**AccumulatorStepsPage ("ACCST"):**
- Per-step trigger configuration
- 16-step toggle interface (STP1-STP16)
- Visual feedback for active triggers

**NoteSequenceEditPage Integration:**
- Press Note button (F3) to cycle through: Note → NoteVariationRange → NoteVariationProbability → AccumulatorTrigger → Note
- When in AccumulatorTrigger layer, use S1-S16 buttons to toggle accumulator triggers for each step

**TopPage Navigation:**
- Sequence key cycles: NoteSequence → Accumulator → AccumulatorSteps → NoteSequence
- Maintains current view state

### Implementation Architecture

**Model Layer** (`src/apps/sequencer/model/`):
- `Accumulator.h/cpp`: Core accumulator logic with all parameters and tick() method
- `NoteSequence.h`: Integration with sequence steps via `AccumulatorTrigger` layer and `_accumulator` instance
- Thread-safe with mutable state management for multi-threaded access
- Memory-efficient bitfield parameter packing

**Engine Layer** (`src/apps/sequencer/engine/`):
- `NoteTrackEngine.cpp`: Integration in `triggerStep()` and `evalStepNote()`
  - STEP mode: Ticks once per step (line ~353)
  - GATE mode: Ticks per gate pulse (line ~392)
  - RTRIG mode: Ticks N times for N retriggers (line ~410, immediate loop)
  - Applies accumulator value to note pitch in real-time during `evalStepNote()`

**UI Layer** (`src/apps/sequencer/ui/pages/`):
- `AccumulatorPage.h/cpp`: Main parameter editing page
- `AccumulatorStepsPage.h/cpp`: Step trigger configuration page
- `ui/model/AccumulatorListModel.h`: List model for parameter editing with indexed value support

### Performance Impact

- Minimal CPU overhead (single conditional check per step)
- No additional memory per sequence beyond accumulator object
- UI updates only during manual interaction
- Compatible with existing timing constraints

### Testing Status

✅ **Fully tested and verified:**
- All unit tests pass (`TestAccumulator.cpp`)
- Integration tests confirm real-time modulation
- Successfully deployed and tested on actual hardware
- Full compatibility with existing sequencer features

### Future Extensions

Planned enhancements documented in `QWEN.md`:
- Apply accumulator to gate length and probability
- Cross-track accumulator influence
- Integration with arpeggiator
- CV input tracking
- Scene recall functionality
- Extension to curve sequences

**RTRIG Mode Timing Enhancement:**
- Currently: All N ticks fire immediately at step start
- Potential: Queue-based ticks spread over time (one per retrigger as it fires)
- See `RTRIG-Timing-Research.md` for gate queue architecture investigation and workaround analysis
- See `Queue-BasedAccumTicks.md` for detailed implementation plan and risk analysis
- Status: Investigated and documented - recommendation is to accept current behavior (high effort, high risk, pointer invalidation concerns)

### Key Files

- `src/apps/sequencer/model/Accumulator.h/cpp` - Core implementation
- `src/apps/sequencer/engine/NoteTrackEngine.cpp` - Engine integration
- `src/apps/sequencer/ui/pages/AccumulatorPage.h/cpp` - ACCUM page UI
- `src/apps/sequencer/ui/pages/AccumulatorStepsPage.h/cpp` - ACCST page UI
- `src/apps/sequencer/ui/model/AccumulatorListModel.h` - UI list model
- `src/tests/unit/sequencer/TestAccumulator.cpp` - Unit tests

## Pulse Count Feature

The PEW|FORMER firmware includes a Metropolix-style pulse count feature that allows each step to repeat for a configurable number of clock pulses before advancing to the next step. This enables variable step lengths without changing the global divisor.

### Overview

Pulse count is a per-step parameter that determines how many clock pulses a step will play before the sequencer advances to the next step. Unlike retrigger (which subdivides a step), pulse count extends the step duration by repeating it for multiple clock pulses.

### Core Parameters

- **Pulse Count**: Per-step value from 0-7 (representing 1-8 clock pulses)
  - 0 = 1 pulse (normal/default behavior)
  - 1 = 2 pulses
  - 2 = 3 pulses
  - ...
  - 7 = 8 pulses (maximum)

### UI Integration

**Accessing Pulse Count Layer:**
1. Navigate to STEPS page (track editing view)
2. Press Retrigger button (F2) to cycle through layers:
   - First press: RETRIG (retrigger count)
   - Second press: RETRIG PROB (retrigger probability)
   - Third press: **PULSE COUNT** ← Feature layer
   - Fourth press: cycles back to RETRIG

**Editing Pulse Count:**
1. Select steps using S1-S16 buttons
2. Turn encoder to adjust pulse count (displays 1-8)
3. Detail overlay shows current value when adjusting
4. Visual display shows number above each step

### Implementation Architecture

**Model Layer** (`src/apps/sequencer/model/`):
- `NoteSequence.h`: Added `pulseCount` field to Step class
  - 3-bit bitfield in `_data1` union (bits 17-19)
  - Type: `using PulseCount = UnsignedValue<3>;`
  - Automatic clamping (0-7)
  - Integrated with Layer enum for UI access
  - Serialization automatic via `_data1.raw`

**Engine Layer** (`src/apps/sequencer/engine/`):
- `NoteTrackEngine.h/cpp`: Pulse counter state management
  - Added `_pulseCounter` member variable
  - Tracks current pulse within step
  - Increments on each clock pulse
  - Only advances step when `_pulseCounter > step.pulseCount()`
  - Resets counter when advancing to next step
  - Works with both Aligned and Free play modes

**UI Layer** (`src/apps/sequencer/ui/pages/`):
- `NoteSequenceEditPage.cpp`: Full UI integration
  - Added to Retrigger button cycling
  - Encoder support for value adjustment
  - Visual display showing pulse count (1-8)
  - Detail overlay showing current value

### Use Cases

- **Variable rhythm patterns**: Create polyrhythmic sequences by varying step durations
- **Step emphasis**: Make important steps longer by increasing their pulse count
- **Complex timing**: Combine with retrigger for intricate rhythmic structures
- **Pattern variation**: Change pulse counts to create pattern variations without altering notes

### Compatibility

- ✅ Works with all play modes (Aligned, Free)
- ✅ Compatible with retrigger feature
- ✅ Works with fill modes
- ✅ Integrates with all existing sequencer features
- ✅ Serialization supported (saved with projects)

### Testing Status

✅ **Fully tested and verified:**
- All unit tests pass (7 test cases covering model layer)
- Engine logic verified in simulator
- UI integration complete and functional
- Compatible with existing features

### Key Files

- `src/apps/sequencer/model/NoteSequence.h/cpp` - Model layer implementation
- `src/apps/sequencer/engine/NoteTrackEngine.h/cpp` - Engine timing logic
- `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` - UI integration
- `src/tests/unit/sequencer/TestPulseCount.cpp` - Unit tests

## Gate Mode Feature

The PEW|FORMER firmware includes a gate mode feature that controls how gates are fired during pulse count repetitions. This provides fine-grained control over gate timing patterns when combined with the pulse count feature.

### Overview

Gate mode is a per-step parameter that determines the gate firing behavior when a step repeats for multiple pulses. It allows creative rhythmic patterns by controlling which pulses within a step produce gates.

### Core Parameters

- **Gate Mode**: Per-step value from 0-3 (representing 4 different modes)
  - **A (ALL, 0)**: Fires gates on every pulse (default, backward compatible)
  - **1 (FIRST, 1)**: Single gate on first pulse only, silent for remaining pulses
  - **H (HOLD, 2)**: One long gate held high for entire step duration
  - **1L (FIRSTLAST, 3)**: Gates on first and last pulse only

### UI Integration

**Accessing Gate Mode Layer:**
1. Navigate to STEPS page (track editing view)
2. Press Gate button (F1) to cycle through layers:
   - First press: GATE (gate on/off)
   - Second press: GATE PROB (gate probability)
   - Third press: GATE OFFSET (gate timing offset)
   - Fourth press: SLIDE (slide/portamento)
   - Fifth press: **GATE MODE** ← Feature layer
   - Sixth press: cycles back to GATE

**Editing Gate Mode:**
1. Select steps using S1-S16 buttons
2. Turn encoder to adjust gate mode (0-3)
3. Detail overlay shows mode abbreviation when adjusting
4. Visual display shows compact abbreviation above each step:
   - A = gates on every pulse
   - 1 = gate on first pulse only
   - H = one long continuous gate
   - 1L = gates on first and last pulse

### Implementation Architecture

**Model Layer** (`src/apps/sequencer/model/`):
- `NoteSequence.h`: Added `gateMode` field to Step class
  - 2-bit bitfield in `_data1` union (bits 20-21)
  - Type: `using GateMode = UnsignedValue<2>;`
  - Automatic clamping (0-3)
  - GateModeType enum: All=0, First=1, Hold=2, FirstLast=3
  - Integrated with Layer enum for UI access
  - Serialization automatic via `_data1.raw`
  - 10 bits remaining in `_data1` for future features

**Engine Layer** (`src/apps/sequencer/engine/`):
- `NoteTrackEngine.cpp`: Gate firing logic in `triggerStep()`
  - Switch statement controls gate generation based on mode
  - Uses `_pulseCounter` to determine current pulse (1 to pulseCount+1)
  - ALL mode: `shouldFireGate = true` (every pulse)
  - FIRST mode: `shouldFireGate = (_pulseCounter == 1)` (first only)
  - HOLD mode: Single gate on first pulse with extended length `divisor * (pulseCount + 1)`
  - FIRSTLAST mode: `shouldFireGate = (_pulseCounter == 1 || _pulseCounter == pulseCount + 1)`
  - Works with both Aligned and Free play modes

**UI Layer** (`src/apps/sequencer/ui/pages/`):
- `NoteSequenceEditPage.cpp`: Full UI integration
  - Added to Gate button (F1) cycling mechanism
  - Encoder support for value adjustment (0-3 with clamping)
  - Visual display showing compact abbreviations (A/1/H/1L)
  - Detail overlay showing mode abbreviation when adjusting

### Use Cases

- **Accent patterns**: Use FIRST mode to create accents on downbeats while step repeats
- **Long notes**: Use HOLD mode to sustain notes across multiple pulses
- **Rhythmic variations**: Use FIRSTLAST mode to create "bouncing" rhythms
- **Silent repeats**: Combine FIRST mode with high pulse counts for rhythmic gaps
- **Dynamic patterns**: Mix different gate modes across steps for complex gate patterns

### Compatibility

- ✅ Works with all play modes (Aligned, Free)
- ✅ Integrates seamlessly with pulse count feature
- ✅ Compatible with retrigger feature
- ✅ Works with gate offset and gate probability
- ✅ Compatible with slide/portamento
- ✅ Works with fill modes
- ✅ Backward compatible (default mode 0 = ALL maintains existing behavior)
- ✅ Serialization supported (saved with projects)

### Testing Status

✅ **Fully tested and verified:**
- All unit tests pass (6 test cases covering model layer)
- Engine logic verified in simulator with all 4 modes
- UI integration complete and functional
- Two critical bugs discovered and fixed during testing:
  - Pulse counter timing bug (triggerStep called before counter reset)
  - Step index lookup bug (stale _currentStep instead of _sequenceState.step())
- Compatible with existing features
- Production ready

### Key Files

- `src/apps/sequencer/model/NoteSequence.h/cpp` - Model layer implementation
- `src/apps/sequencer/engine/NoteTrackEngine.cpp` - Engine gate firing logic
- `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` - UI integration
- `src/tests/unit/sequencer/TestGateMode.cpp` - Unit tests
- `GATE_MODE_TDD_PLAN.md` - Complete technical specification
- `GATE_MODE_ENGINE_DESIGN.md` - Engine implementation design

## Simulator Interface

The simulator provides a complete virtual hardware interface (see `doc/simulator-interface.png`):

### Top Panel - I/O Ports
**Left side:**
- USB, MICRO SD indicators
- MIDI IN/OUT ports (with activity LEDs)
- CLK IN/OUT, RST IN/OUT (clock/reset jacks)

**Right side:**
- CV1-CV4 IN (4 CV input jacks with activity indicators)
- GATE1-GATE8 (8 gate outputs)
- CV1-CV8 (8 CV outputs)

### Center Panel - Display & Controls
**Large encoder** (left): Main rotary encoder with tick marks
**OLED Display** (center):
- Shows sequencer info (tempo, pattern, mode)
- 8-track visualization (T1-T8)
- Function key labels (F1-F5)
- Status indicators (LATCH, SYNC, UNMUTE, FILL, PERFORM)

**Function buttons** (F1-F5): Context-sensitive function keys below display

### Control Matrix - 4 rows of buttons
**Row 1 (Playback):**
- PLAY/REC, TEMP/CLOCK
- PATT, PERF (Pattern/Performance modes)
- PREV, NEXT (navigation)
- SHIFT, PAGE (modifiers)

**Row 2 (Track selection - T1-T8):**
- T1/PROJECT, T2/LAYOUT, T3/ROUTING, T4/MIDIOUT
- T5/USCALE, T6, T7/SYSTEM, T8
- Each button has dual function (track selection + system page access)

**Row 3 (Step buttons - S1-S8):**
- S1/STEPS, S2/SEQ, S3/TRACK, S4/SONG
- S5, S6, S7/MONITOR, S8
- Step input/editing + mode switching

**Row 4 (Utility - S9-S16):**
- S9/FIRST, S10/LAST, S11/MODE, S12/DIVISOR
- S13/RESET, S14/SCALE, S15/ROOT, S16
- Additional step controls + parameter access

### Bottom Panel - Inputs
**CV Inputs:** CV1-CV4 IN (simulated control voltage inputs)
**Clock/Reset:** CLK IN, RST IN (external sync inputs)
**Screenshot button:** For capturing simulator state

### Simulator Controls
All physical hardware interactions are simulated:
- Click encoder to turn
- Click buttons to press
- CV/Gate I/O is visualized with activity indicators
- MIDI I/O shows activity LEDs
- Display updates in real-time

The simulator runs the exact same firmware code as the hardware, making it an accurate development and testing environment.

## Development Workflow

**Simulator-first development is recommended:**
1. Develop and test features in simulator (`build/sim/debug`)
2. Better debugging experience with native tools
3. Faster iteration cycle
4. Port to hardware once stable

**When modifying timing-critical code:**
- Test on actual hardware - simulator timing differs
- Check `Engine::update()` execution time
- Verify against task priorities and stack sizes

**When adding UI features:**
- Consider noise reduction impact (pixel count affects audio noise)
- Test with different brightness/screensaver settings
- Verify screensaver wake mode behavior
- Use simulator screenshot feature to document UI changes

## File Generation Notes

Building the sequencer generates multiple artifacts in `build/{platform}/{type}/src/apps/sequencer/`:
- `.bin`: Raw binary
- `.hex`: Intel HEX (flashing)
- `.srec`: Motorola SREC (flashing)
- `UPDATE.DAT`: Bootloader-based firmware update file
- `.list`: Full disassembly
- `.map`: Symbol offset information
- `.size`: Section sizes

The `compile_commands.json` symlink at `src/compile_commands.json` points to the STM32 release build for IDE integration.

## Third-Party Libraries

The following third-party libraries are used in this project:

- [FreeRTOS](http://www.freertos.org) - Real-time operating system
- [libopencm3](https://github.com/libopencm3/libopencm3) - Open-source ARM Cortex-M microcontroller library (updated to October 2024)
- [libusbhost](https://github.com/libusbhost/libusbhost) - USB host library
- [NanoVG](https://github.com/memononen/nanovg) - Vector graphics rendering
- [FatFs](http://elm-chan.org/fsw/ff/00index_e.html) - FAT filesystem module
- [stb_sprintf](https://github.com/nothings/stb/blob/master/stb_sprintf.h) - Fast sprintf implementation
- [stb_image_write](https://github.com/nothings/stb/blob/master/stb_image_write.h) - Image writing
- [soloud](https://sol.gfxile.net/soloud/) - Audio engine (simulator)
- [RtMidi](https://www.music.mcgill.ca/~gary/rtmidi/) - MIDI I/O library (simulator)
- [pybind11](https://github.com/pybind/pybind11) - Python bindings
- [tinyformat](https://github.com/c42f/tinyformat) - Type-safe printf
- [args](https://github.com/Taywee/args) - Command-line argument parsing

## Documentation References

- **CLAUDE.md** (this file) - Main development reference for Claude Code
- **QWEN.md** - Complete accumulator feature implementation documentation
- **TODO.md** - Development task tracking and completed features
- **README.md** - Project overview and build instructions
- **doc/improvements/** - jackpf improvement documentation
  - `noise-reduction.md` - Display noise reduction techniques
  - `shape-improvements.md` - CV curve generation enhancements
  - `midi-improvements.md` - MIDI functionality extensions
- **doc/simulator-interface.png** - Simulator UI reference diagram
