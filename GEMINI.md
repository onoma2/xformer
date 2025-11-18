# GEMINI.md
You are amazing STM32 coder with advanced musical acumen, aware of EURORACK CV
conventions and OLED ui design constraints. You work only in TEST DRIVEN
DEVELOPMENT methodology.
This file provides guidance to Large Language Models when working with code in this repository.

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

## Implemented Features

This section details major features that have been implemented and are ready for use.

### Accumulator Feature

The PEW|FORMER firmware includes an advanced accumulator feature that provides powerful real-time parameter modulation capabilities.

**Overview**

An accumulator is a stateful counter that increments/decrements based on configurable parameters and updates when specific sequence steps are triggered. It modulates musical parameters (currently pitch, with potential for expansion to gate length, probability, and CV curves).

**Core Parameters**

- **Enable**: On/off control
- **Mode**: Stage or Track level operation
- **Direction**: Up, Down, or Freeze
- **Order**: Boundary behavior modes (Wrap, Pendulum, Random, Hold)
- **Polarity**: Unipolar or Bipolar range
- **Value Range**: Min/Max constraints (-100 to 100)
- **Step Size**: Amount to increment/decrement per trigger (1-100)
- **Current Value**: The current accumulated value (read-only)

**UI Integration**

- **AccumulatorPage ("ACCUM"):** Main parameter editing interface.
- **AccumulatorStepsPage ("ACCST"):** Per-step trigger configuration.
- **NoteSequenceEditPage Integration:** Press Note button (F3) to cycle to the AccumulatorTrigger layer.
- **TopPage Navigation:** Sequence key cycles through NoteSequence, Accumulator, and AccumulatorSteps pages.

**Implementation Architecture**

- **Model:** `src/apps/sequencer/model/Accumulator.h/cpp`
- **Engine:** `NoteTrackEngine.cpp` integrates accumulator logic in `triggerStep()` and `evalStepNote()`.
- **UI:** `AccumulatorPage.h/cpp` and `AccumulatorStepsPage.h/cpp` provide the user interface.

**Testing Status**

- ✅ **Fully tested and verified:** Unit tests (`TestAccumulator.cpp`), integration tests, and hardware tests all pass.

### Pulse Count Feature

A Metropolix-style pulse count feature that allows each step to repeat for a configurable number of clock pulses (1-8) before advancing.

**Core Parameters**

- **Pulse Count**: Per-step value from 0-7 (representing 1-8 clock pulses).

**UI Integration**

- Accessed by pressing the Retrigger button (F2) on the STEPS page until the "PULSE COUNT" layer appears.

**Implementation Architecture**

- **Model:** `NoteSequence.h`'s `Step` class includes a `pulseCount` field.
- **Engine:** `NoteTrackEngine.h/cpp` manages a `_pulseCounter` to track repetitions.

**Testing Status**

- ✅ **Fully tested and verified:** Unit tests (`TestPulseCount.cpp`), simulator, and hardware tests all pass.

### Gate Mode Feature

Controls how gates are fired during pulse count repetitions, providing fine-grained rhythmic control.

**Core Parameters**

- **Gate Mode**: Per-step value from 0-3, representing four modes:
  - **A (ALL, 0)**: Fires gates on every pulse.
  - **1 (FIRST, 1)**: Single gate on the first pulse only.
  - **H (HOLD, 2)**: One long gate held for the entire step duration.
  - **1L (FIRSTLAST, 3)**: Gates on the first and last pulse only.

**UI Integration**

- Accessed by pressing the Gate button (F1) on the STEPS page until the "GATE MODE" layer appears.

**Implementation Architecture**

- **Model:** `NoteSequence.h`'s `Step` class includes a `gateMode` field.
- **Engine:** `NoteTrackEngine.cpp`'s `triggerStep()` contains the logic for the different gate modes.

**Testing Status**

- ✅ **Fully tested and verified:** Unit tests (`TestGateMode.cpp`), simulator, and hardware tests all pass. Two critical bugs were found and fixed during development.

## Simulator Interface

The simulator provides a complete virtual hardware interface, allowing for accurate development and testing of the firmware. See `doc/simulator-interface.png` for a visual diagram. All physical hardware interactions (encoders, buttons, I/O) are simulated.

## Development Workflow

**Simulator-first development is recommended:**
1. Develop and test features in the simulator (`build/sim/debug`).
2. Use native debugging tools for a better experience.
3. Port to hardware once the feature is stable.

**When modifying timing-critical code:**
- Always test on actual hardware as simulator timing can differ.
- Profile `Engine::update()` execution time.

**When adding UI features:**
- Be mindful of the noise impact of display changes.
- Use the simulator's screenshot feature to document UI changes.

## File Generation Notes

Building the sequencer generates multiple artifacts in `build/{platform}/{type}/src/apps/sequencer/`, including `.hex`, `.bin`, and `UPDATE.DAT` files for flashing, as well as `.map` and `.list` files for debugging.

## Third-Party Libraries

- **FreeRTOS**: Real-time operating system
- **libopencm3**: ARM Cortex-M microcontroller library
- **FatFs**: FAT filesystem module
- And others, see `CLAUDE.md` for a complete list.

## Documentation References

- **GEMINI.md** (this file): Main development reference.
- **QWEN.md**: Detailed feature development reports.
- **CLAUDE.md**: Alternative detailed development reference.
- **TODO.md**: Development task tracking.
- **README.md**: Project overview and build instructions.
- **doc/improvements/**: Documentation on major improvements.

## Optional TDD Plan: Per-Retrigger Accumulator Ticking

**Disclaimer:** This is a high-risk feature that complicates the engine's timing logic. The analysis in `RTRIG-Timing-Research.md` recommends against it due to pointer invalidation risks and complexity. Proceed with caution. This plan outlines a Test-Driven Development approach for the "Sequence ID" (weak reference) method.

### Phase 1: Model Layer (GateQueue)

1.  **Test: `Gate` struct stores `shouldTickAccumulator` flag.**
    *   **RED:** In a new test file (e.g., `TestGateQueue.cpp`), write a test that pushes a `Gate` event with `shouldTickAccumulator = true` to the `GateQueue` and asserts the flag is `true` when the event is popped. The test will fail to compile.
    *   **GREEN:** Add `bool shouldTickAccumulator;` to the `Gate` struct in `GateQueue.h`. Initialize it to `false` in constructors.
    *   **REFACTOR:** N/A.

2.  **Test: `Gate` struct stores `sequenceId`.**
    *   **RED:** Write a test to push a `Gate` event with a specific `sequenceId` (e.g., `1` for a fill sequence) and assert the ID is correct when popped. The test will fail to compile.
    *   **GREEN:** Add `uint8_t sequenceId;` to the `Gate` struct. Define constants for IDs (e.g., `MainSequenceId = 0`, `FillSequenceId = 1`). Initialize it in constructors.
    *   **REFACTOR:** Ensure struct memory alignment is reasonable.

### Phase 2: Engine Layer (Scheduling)

*This requires a test setup for `NoteTrackEngine` where the `GateQueue` can be inspected.*

1.  **Test: `triggerStep` schedules gates with `shouldTickAccumulator = true`.**
    *   **RED:** In `TestNoteTrackEngine.cpp`, configure a step with an accumulator trigger and a retrigger count > 1. Set the accumulator's trigger mode to `Retrigger`. Call `triggerStep`. Inspect the events pushed to the `GateQueue`. Assert that the `shouldTickAccumulator` flag is `true` for the gate-on events. This test will fail.
    *   **GREEN:** In `NoteTrackEngine::triggerStep`, within the retrigger loop, add logic to set `shouldTickAccumulator = true` on the `Gate` event if the step is an accumulator trigger and the accumulator is configured for retrigger mode.
    *   **REFACTOR:** Encapsulate the flag-setting logic in a clear, readable conditional block.

2.  **Test: `triggerStep` schedules gates with the correct `sequenceId`.**
    *   **RED:** Write a test where the `NoteTrackEngine` is processing a fill sequence. Call `triggerStep` and assert that the scheduled gate events have `sequenceId = FillSequenceId`. This will likely require modifying the `triggerStep` signature or engine state for testing.
    *   **GREEN:** Pass the sequence context (or its ID) into `triggerStep` and use it to set the `sequenceId` on the scheduled `Gate` events.
    *   **REFACTOR:** Clean up the `triggerStep` signature and call sites.

### Phase 3: Engine Layer (Processing)

*This is the most critical phase and requires testing the main `Engine`'s processing loop.*

1.  **Test: `Engine` ticks the correct accumulator on a tagged gate event.**
    *   **RED:** In a new test file (e.g., `TestEngineGating.cpp`), get a full `Engine` instance. Manually push a `Gate` event to its queue with `shouldTickAccumulator = true` and `sequenceId = MainSequenceId`. Use a mock or spy on the main sequence's `Accumulator` to track calls to its `tick()` method. Run the `Engine` for one cycle. Assert that `tick()` was called exactly once.
    *   **GREEN:** In the `Engine`'s main processing loop where it pops from the `_gateQueue`, add the new logic:
        1.  Check if `event.shouldTickAccumulator` is true.
        2.  If so, get the `NoteSequence*` based on `event.sequenceId`.
        3.  Perform a validity check on the pointer.
        4.  If valid, call `sequence->accumulator().tick()`.
    *   **REFACTOR:** Move the sequence lookup and ticking logic into a new private helper method within the `Engine`.

2.  **Test: `Engine` does NOT tick accumulator if sequence is invalid.**
    *   **RED:** Similar to the above test, but schedule an event for the `FillSequenceId`. Before running the engine cycle, deactivate the fill sequence in the model, invalidating it. Run the engine cycle. Assert that the accumulator's `tick()` method was **never** called and the firmware did not crash.
    *   **GREEN:** Implement the pointer validity check from the previous step. Ensure it correctly identifies the sequence as invalid and skips the `tick()` call.
    *   **REFACTOR:** N/A.

### Phase 4: Manual and Stress Testing

After all unit tests pass, this feature requires extensive manual testing on hardware due to its timing-sensitive nature.

1.  **Queue Capacity:** Test with maximum retrigger and pulse counts to ensure the `GateQueue` (16 entries) does not overflow.
2.  **Pattern Switching:** While a retriggering sequence is playing, rapidly switch patterns.
3.  **Fill Mode:** Trigger and release fill mode while a retriggering sequence is active.
4.  **Project Loading:** Load a new project while a sequence with this feature is playing.
5.  **UI Responsiveness:** Ensure the UI remains responsive under heavy retrigger/accumulator load.
