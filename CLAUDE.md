# CLAUDE.md

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
