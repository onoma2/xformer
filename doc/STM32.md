# STM32 Development Guide for EURORACK Firmware

## Hardware Architecture

### Platform-Specific Architecture
- **stm32**: Production hardware (STM32F4, ARM Cortex-M4)
- Hardware-specific drivers in `src/platform/stm32/drivers/`:
  - ADC: Analog-to-Digital conversion for CV inputs
  - DAC: Digital-to-Analog conversion for CV outputs
  - MIDI: Hardware UART and USB MIDI
  - USB: USB host/device functionality
  - Display: OLED display driver

### Task Architecture (FreeRTOS on STM32)
Five concurrent tasks with defined priorities for STM32 hardware:
1. Driver Task (priority 5, 1024 stack): Low-level hardware I/O
2. Engine Task (priority 4, 4096 stack): Sequencer engine updates
3. USB Host Task (priority 3, 2048 stack): USB MIDI device handling
4. UI Task (priority 2, 4096 stack): Display rendering and input
5. File Task (priority 1, 2048 stack): SD card operations
6. Profiler Task (priority 0, 2048 stack): Performance monitoring

### Critical Hardware Subsystems

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

## Build System for STM32

### Initial Setup for STM32 Hardware Development
**For STM32 hardware development:**
```bash
make tools_install    # Installs ARM toolchain (gcc 14.2) and OpenOCD
make setup_stm32      # Creates build/stm32/{debug,release} directories
```

### Building for STM32 Hardware
**Build for STM32 hardware (release):**
```bash
cd build/stm32/release
make -j sequencer                # Main sequencer app
make -j sequencer_standalone     # Sequencer without bootloader
make -j bootloader               # Bootloader
make -j tester                   # Hardware tester
```

### Flashing to Hardware
**Flash to hardware:**
```bash
cd build/stm32/release
make -j flash_bootloader
make -j flash_sequencer
```

Note: Flashing uses OpenOCD with Olimex ARM-USB-OCD-H JTAG by default. To use a different JTAG, edit `OPENOCD_INTERFACE` in `src/platform/stm32/CMakeLists.txt`.

## Hardware Constraints and Considerations

### Performance and Memory Constraints
- STM32F4 microcontroller with ARM Cortex-M4 core
- Memory constraints require efficient data structures
- Real-time processing requires careful timing optimization
- Interrupt handlers must be minimal and efficient

### Timing Constraints for Real-Time Audio
- Engine runs at high priority to maintain audio timing
- `CONFIG_PPQN`: 192 (parts per quarter note for timing)
- `CONFIG_SEQUENCE_PPQN`: 48 (sequence resolution)
- Critical timing paths must be optimized for deterministic behavior

### CV and Gate Conventions for Eurorack
- CV outputs: 0-10V range for Eurorack compatibility
- 8 CV outputs and 8 gate outputs
- Gate inputs for clock/reset synchronization
- Timing-accurate gate generation for musical precision

### OLED UI Design Constraints
- Display updates can cause audio noise due to digital switching
- Consider pixel count and display brightness settings
- Screensaver functionality to reduce noise when not in use
- Visual feedback must be balanced with audio quality

## Hardware-Specific Testing Considerations

### Timing Differences Between Simulator and Hardware
- Simulator timing differs from actual STM32 hardware
- Timing-critical features should be validated on hardware
- Engine update execution time may vary on actual hardware
- FreeRTOS task scheduling may behave differently

### Hardware-Specific Debugging
- Use JTAG interface for low-level debugging
- Check FreeRTOS task priorities and stack usage
- Monitor interrupt timing and duration
- Validate audio output quality and noise levels

### Memory Management for STM32
- Memory-constrained environment (limited RAM and flash)
- Efficient data structures for sequence storage
- Consider memory alignment for optimal performance
- Stack size management for different task priorities

## Eurorack-Specific Hardware Constraints

### Power Specifications
- Eurorack modules require +12V, -12V, and sometimes +5V power rails
- Power consumption must be minimized to avoid overloading power supplies
- Current draw should be specified and minimized where possible

### CV/Gate Conventions
- Standard Eurorack CV range is 0-10V
- 1V/Octave standard for pitch CV (0V = C0, 1V = C1, etc.)
- Gate signals are typically 0V (off) and +5V/+10V (on)
- Trigger signals are short positive pulses (typically 1-100ms)

### Hardware Design Considerations
- Module width measured in HP (horizontal pitch) - typically 4HP, 8HP, 10HP, etc.
- Panel height is standardized at 3U (128.5mm)
- Use of standard Eurorack jacks (3.5mm TRS)
- Attention to physical layout and component placement for panel mounting

### Audio Quality Requirements
- Low-noise design essential for audio applications
- Careful attention to ground planes and signal routing
- Digital switching noise must be minimized
- High-quality ADC/DAC components for audio path

### Real-Time Performance
- Deterministic timing for musical applications
- No dropped samples or timing variations
- Low-latency responses to user input
- Consistent processing across all tasks

## Feature Implementation for STM32

### Accumulator Feature (STM32 Implementation)
The accumulator feature provides real-time parameter modulation on STM32 hardware:
- Memory-efficient bitfield parameter packing in NoteSequence::Step
- Thread-safe access with mutable state management for multi-threaded FreeRTOS environment
- Performance-optimized tick() method with minimal overhead per step
- Real-time modulation applied during evalStepNote() calls in NoteTrackEngine

### Pulse Count Feature (STM32 Implementation)
Pulse count feature for variable step durations on STM32:
- Hardware-optimized pulse counter state management in NoteTrackEngine
- Efficient conditional checks to maintain timing accuracy
- Memory-efficient 3-bit bitfield storage in NoteSequence::Step
- Compatible with Aligned and Free play modes on STM32 platform

### Gate Mode Feature (STM32 Implementation)
Gate mode feature for creative rhythmic patterns on STM32:
- Engine-level gate firing logic with minimal CPU overhead
- 2-bit bitfield storage in NoteSequence::Step for memory efficiency
- Optimized switch statement for gate generation based on mode
- Real-time operation compatible with STM32's timing constraints

### Harmony Feature (STM32 Implementation)
Harmonàig-style harmonic sequencing on STM32:
- Optimized HarmonyEngine with <1µs per note evaluation on ARM Cortex-M4
- Direct integration in NoteTrackEngine::evalStepNote() for real-time performance
- Memory-efficient bitfield storage for harmony parameters in NoteSequence
- Per-step inversion/voicing overrides with bitfield packing (bits 25-30)

### Tuesday Track (STM32 Implementation)
Generative algorithmic sequencing for STM32 platform:
- Dual RNG seeding optimized for ARM Cortex-M4 performance
- Cooldown-based density control for linear note count
- Algorithm implementations optimized for STM32's processing capabilities
- Gate length support with long gates (up to 400%) for evolving textures

## Third-Party Libraries for STM32

The following STM32-specific libraries are used in this project:
- [libopencm3](https://github.com/libopencm3/libopencm3) - Open-source ARM Cortex-M microcontroller library (updated to October 2024)
- FreeRTOS - Real-time operating system for task management
- [libusbhost](https://github.com/libusbhost/libusbhost) - USB host library
- [FatFs](http://elm-chan.org/fsw/ff/00index_e.html) - FAT filesystem module