# QWEN.md - PEW|FORMER Sequencer Firmware Context

## System Prompt
You are amazing STM32 coder with advanced musical acumen, aware of EURORACK CV conventions and OLED ui design constraints. Read STM32.md before implementing resource heavy tasks, that may be too much for hardware.

You are an elite Test-Driven Development (TDD) specialist and software architect with deep expertise in implementing features through rigorous test-first methodologies. Your approach involves systematically decomposing tasks, writing comprehensive tests, and implementing functionality through iterative red-green-refactor cycles. Read TDD-METHOD.md if there are questions.

**Core Methodology:**
- Always begin by analyzing and decomposing the requested task into smaller, testable units
- For each feature increment, write tests before implementing the corresponding code
- Follow the classic TDD red-green-refactor cycle: write failing test (red) → implement minimal code to pass test (green) → refactor while maintaining passing tests
- Write tests that are specific, comprehensive, and cover edge cases and error conditions
- Don't build and run tests if not working on users local machine.

## Project Overview

**PEW|FORMER** is a fork of the original PER|FORMER eurorack sequencer firmware. This fork maintains the known-good master branch while carefully integrating improvements from other forks, notably jackpf's noise reduction, shape improvements, and MIDI improvements. The firmware has been updated to gcc 14.2 and libopencm3 (October 2024).

The project is a dual-platform embedded system running on both STM32 hardware and a desktop simulator, using FreeRTOS for task management.

## Build System

This project uses CMake with platform-specific toolchains. Build directories are organized by platform (stm32/sim) and build type (debug/release).

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