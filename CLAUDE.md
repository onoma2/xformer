# CLAUDE.md
System Prompt:
You are amazing STM32 coder with advanced musical acumen, aware of EURORACK CV
conventions and OLED ui design constraints.                                                              │
│                                                                                                              │
│  You are an elite Test-Driven Development (TDD) specialist and software architect with deep expertise in     │
│  implementing features through rigorous test-first methodologies. Your approach involves systematically      │
│  decomposing tasks, writing comprehensive tests, and implementing functionality through iterative            │
│  red-green-refactor cycles.                                                                                  │
│                                                                                                              │
│  **Core Methodology:**                                                                                       │
│  - Always begin by analyzing and decomposing the requested task into smaller, testable units                 │
│  - For each feature increment, write tests before implementing the corresponding code                        │
│  - Follow the classic TDD red-green-refactor cycle: write failing test (red) → implement minimal code to     │
│  pass test (green) → refactor while maintaining passing tests                                                │
│  - Write tests that are specific, comprehensive, and cover edge cases and error conditions  .
- dont build and run tests if not working on users local machine.
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

**Visual Indicators**:
- **Counter display**: Current value shown in header (x=176, aligned with step 12)
  - Format: "+5", "-12", "+0" using Font::Tiny, Color::Medium
  - Visible on all Note track pages when accumulator is enabled
  - Implemented via `WindowPainter::drawAccumulatorValue()`
- **Corner dot indicator**: Top-right dot on steps with accumulator trigger enabled
  - Position: `(x + stepWidth - 5, y + 4)`
  - Color: `Color::None` (black) when gate on, `Color::Bright` when gate off
  - Visible across all pages and layers
  - AccumulatorTrigger layer inherits gate squares, only adds corner dots

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
- `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` - Corner dot and counter display
- `src/apps/sequencer/ui/painters/WindowPainter.h/cpp` - drawAccumulatorValue() function
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

## Harmony Feature

The PEW|FORMER firmware includes Harmonàig-style harmonic sequencing that allows tracks to automatically harmonize a master track's melody. This enables instant chord creation, complex harmonic arrangements, and modal exploration. See `HARMONY-DONE.md` for complete implementation summary and `QWEN.md` Part 5 for technical details.

### Overview

The harmony feature provides master/follower track relationships where follower tracks automatically play harmonized chord tones (root, 3rd, 5th, 7th) based on a master track's melody. Any track can be a master or follower, allowing flexible harmonic arrangements.

**Core capabilities:**
- Master/follower track relationships
- 7 modal scales (Ionian modes)
- 4-voice chord generation
- Per-track harmony configuration
- Synchronized step playback

### Core Parameters

**HarmonyRole** (per NoteSequence):
- `Off` (0): No harmony (default)
- `Master` (1): Defines the harmony (plays melody)
- `FollowerRoot` (2): Plays root note of harmonized chord
- `Follower3rd` (3): Plays 3rd note of harmonized chord
- `Follower5th` (4): Plays 5th note of harmonized chord
- `Follower7th` (5): Plays 7th note of harmonized chord

**MasterTrackIndex** (per NoteSequence):
- Range: 0-7 (tracks 1-8)
- Specifies which track to follow when in follower role
- Only relevant for follower tracks

**HarmonyScale** (per NoteSequence):
- Range: 0-6 (7 Ionian modes)
- **0 - Ionian** (Major): Bright, happy
- **1 - Dorian** (Minor with raised 6th): Jazz minor
- **2 - Phrygian** (Minor with b2): Spanish/dark
- **3 - Lydian** (Major with #4): Dreamy
- **4 - Mixolydian** (Major with b7): Dominant/bluesy
- **5 - Aeolian** (Natural Minor): Melancholic
- **6 - Locrian** (Diminished): Unstable/tense

### UI Integration

**Accessing Harmony Page:**
1. Navigate to a Note track (press T1-T8)
2. Press **Sequence** button (S2) to cycle views:
   - 1st press: NoteSequence page
   - 2nd press: Accumulator page (ACCUM)
   - 3rd press: **Harmony page** (HARMONY) ← Feature page
   - 4th press: Cycles back to NoteSequence

**Editing Harmony Parameters:**
- Turn encoder to navigate between parameters
- Turn encoder to edit selected parameter value
- **ROLE**: Cycles Off → Master → Root → 3rd → 5th → 7th
- **MASTER**: Cycles T1 → T2 → ... → T8 (which track to follow)
- **SCALE**: Cycles through 7 modes (Ionian through Locrian)

### Implementation Architecture

**Model Layer:**
- `HarmonyEngine.h/cpp` - Core harmonization logic for all 7 Ionian modes
- `NoteSequence.h/cpp` - Harmony properties (harmonyRole, masterTrackIndex, harmonyScale)
- `Model.h` - Central HarmonyEngine instance accessible to all tracks

**Engine Layer:**
- `NoteTrackEngine.cpp::evalStepNote()` - Direct integration
- Checks if sequence is harmony follower
- Gets master track's note at same step index (synchronized playback)
- Harmonizes using HarmonyEngine
- Extracts appropriate chord tone based on follower role
- Replaces follower's note with harmonized pitch

**Modulation Order** (important):
1. Base note + transpose/octave
2. **Harmony modulation** (if follower)
3. Accumulator modulation (if enabled)
4. Note variation (if enabled)

**UI Layer:**
- `HarmonyListModel.h` - Parameter editing model
- `HarmonyPage.h/cpp` - Dedicated harmony configuration page
- `Pages.h` - HarmonyPage integration
- `TopPage.h/cpp` - Navigation integration (Sequence button cycling)

### Use Cases

**Instant Chord Pads:**
- Set Track 1 as Master, program melody
- Set Tracks 2-4 as Followers (Root/3rd/5th)
- Result: Instant 3-voice chord harmonization

**Dual Chord Progressions:**
- Tracks 1-4: Bass harmony group
- Tracks 5-8: Lead harmony group
- Independent harmonic progressions

**Modal Exploration:**
- Same master melody across multiple tracks
- Different harmonyScale settings per follower
- Hear how modes color the melodic material

**Jazz Voicings:**
- Use all 4 follower voices (Root/3rd/5th/7th)
- Set scale to Dorian or Mixolydian
- Instant jazz chord progressions

### Compatibility

Harmony works seamlessly with all existing features:
- ✅ Accumulator (harmony first, then accumulator offset)
- ✅ Note variation (harmony first, then random variation)
- ✅ Transpose/Octave (applied after harmony)
- ✅ Gate modes, Pulse count, Retrigger
- ✅ Fill modes
- ✅ Slide/portamento

### Testing Status

✅ **Fully tested and verified:**
- 19 passing unit tests (HarmonyEngine, NoteSequence, Model integration)
- Contract tests verify coordination
- Hardware build successful
- Compatible with existing features
- Production ready

### Per-Step Inversion/Voicing Overrides (Master Tracks Only)

Master tracks can define per-step inversion and voicing overrides that affect how follower tracks harmonize.

**Per-Step Inversion Override:**
- **SEQ (0)**: Use sequence-level inversion setting (default)
- **ROOT (1)**: Override to root position
- **1ST (2)**: Override to 1st inversion
- **2ND (3)**: Override to 2nd inversion
- **3RD (4)**: Override to 3rd inversion

**Per-Step Voicing Override:**
- **SEQ (0)**: Use sequence-level voicing setting (default)
- **CLOSE (1)**: Override to close voicing
- **DROP2 (2)**: Override to drop-2 voicing
- **DROP3 (3)**: Override to drop-3 voicing
- **SPREAD (4)**: Override to spread voicing

**UI Access:**
- Available only for Master tracks via F3 (Note) button cycle
- Master track cycle: Note → Range → Prob → Accum → **INVERSION** → **VOICING** → Note
- Compact display abbreviations: S/R/1/2/3 (inversion), S/C/2/3/W (voicing)

**Implementation:**
- Stored in NoteSequence::Step bitfields (bits 25-27, 28-30)
- Read in `evalStepNote()` from master step when harmonizing followers
- Values passed to local HarmonyEngine with per-step overrides

### Inversion Algorithm (applyInversion in HarmonyEngine.cpp)

All 4 inversions are **fully implemented** (as of 2025-11-20):
- **Root position (0)**: No change - root is bass
- **1st inversion (1)**: Root moves up octave (+12) - third becomes bass
- **2nd inversion (2)**: Root and third move up (+12) - fifth becomes bass
- **3rd inversion (3)**: Root, third, fifth move up (+12) - seventh becomes bass

### Voicing Algorithm (applyVoicing in HarmonyEngine.cpp)

All 4 voicings are **fully implemented** using pitch-order ranking:
- **Close (0)**: No change - notes stay in close position
- **Drop2 (1)**: 2nd highest note drops down an octave (-12)
- **Drop3 (2)**: 3rd highest note drops down an octave (-12)
- **Spread (3)**: 3rd, 5th, 7th all move up an octave (+12), root stays as bass

### What's NOT Implemented (Optional)

These features from the original plan are not yet implemented but could be added:
- ❌ Manual chord quality selection (currently auto-diatonic)
- ❌ Additional scales (Harmonic Minor, Melodic Minor, etc.)

### Key Files

**Model Layer:**
- `src/apps/sequencer/model/HarmonyEngine.h/cpp` - Core harmonization
- `src/apps/sequencer/model/NoteSequence.h/cpp` - Harmony properties
- `src/apps/sequencer/model/Model.h` - HarmonyEngine integration

**Engine Layer:**
- `src/apps/sequencer/engine/NoteTrackEngine.cpp` - Direct integration in evalStepNote()

**UI Layer:**
- `src/apps/sequencer/ui/model/HarmonyListModel.h` - Parameter editing model
- `src/apps/sequencer/ui/pages/HarmonyPage.h/cpp` - Main configuration page
- `src/apps/sequencer/ui/pages/Pages.h` - Page registration
- `src/apps/sequencer/ui/pages/TopPage.h/cpp` - Navigation integration

**Tests:**
- `src/tests/unit/sequencer/TestHarmonyEngine.cpp` - HarmonyEngine unit tests
- `src/tests/unit/sequencer/TestHarmonyIntegration.cpp` - Integration tests
- `src/tests/unit/sequencer/TestHarmonyVoicing.cpp` - Voicing algorithm tests (17 tests)
- `src/tests/unit/sequencer/TestHarmonyInversionIssue.cpp` - Inversion investigation tests (24 tests)
- `src/tests/unit/sequencer/TestHarmonyInversionBug.cpp` - Bug demonstration tests (10 tests)
- `src/tests/unit/sequencer/TestModel.cpp` - Model coordination tests

**Documentation:**
- `HARMONY-DONE.md` - Complete implementation summary
- `HARMONY-HARDWARE-TESTS.md` - 8 comprehensive test cases
- `WORKING-TDD-HARMONY-PLAN.md` - Planning document with status
- `QWEN.md` Part 5 - Technical implementation details
- `PHASE-1-COMPLETE.md` - Phase 1 Days 1-6 summary

## Tuesday Track (Generative Algorithmic Sequencing)

The PEW|FORMER firmware includes a Tuesday track type that provides generative/algorithmic sequencing. Instead of manually programming steps, users set high-level parameters and algorithms generate musical patterns in real-time.

### Overview

**Inspired by:**
- Mutable Instruments Marbles
- Noise Engineering Mimetic Digitalis
- Intellijel Metropolix
- Tuesday Eurorack module

**Key difference from other track types:** No steps to edit - patterns are generated algorithmically based on parameter settings.

### Core Parameters

| Parameter | Range | Musical Meaning |
|-----------|-------|-----------------|
| **Algorithm** | 0-12 | Pattern personality (MARKOV, STOMPER, TRITRANCE, etc.) |
| **Flow** | 0-16 | Sequence movement/variation (seeds RNG) |
| **Ornament** | 0-16 | Embellishments and fills (seeds extra RNG) |
| **Power** | 0-16 | Number of notes in loop (0=silent, 16=all steps) |
| **LoopLength** | Inf, 1-64 | Pattern length |
| **Glide** | 0-100% | Slide probability |
| **Scale** | Free/Project | Note quantization mode |
| **Skew** | -8 to +8 | Density curve across loop |

### Key Features

**Power: Linear Note Count**
- Power = number of notes that play in the loop
- Power 16 = 16 notes, Power 8 = 8 notes, Power 1 = 1 note
- Gap between notes = LoopLength / Power

**Flow & Ornament: Dual RNG Seeding**
- Same values always produce identical patterns (deterministic)
- Different combinations create unique pattern variations

**Gate Lengths with Long Gates**
- Short gates (40%): 25-100%
- Medium gates (30%): 100-175%
- Long gates (30%): 200-400% (multi-beat sustains)

**Glide Parameter**
- 0% = No slides (default)
- 100% = Always slide between notes

**Scale Quantization**
- Free = All 12 semitones
- Project = Quantize to project scale

**Skew Parameter**
Creates density curves across the loop:
- Skew 8: Last 50% at power 16 (build-up)
- Skew -8: First 50% at power 16 (fade-out)
- Skew 0: Use Power setting throughout
- **Note:** Skew has no effect when Loop is Inf (infinite)

**Reseed Functionality**
- **Shift+F5**: Generates new random pattern
- **Context Menu**: RESEED option for Tuesday tracks
- Produces new pattern while preserving Flow/Ornament values

### Implementation Architecture

**Model Layer** (`src/apps/sequencer/model/`):
- `TuesdayTrack.h/cpp`: Data model with all parameters, serialization

**Engine Layer** (`src/apps/sequencer/engine/`):
- `TuesdayTrackEngine.h/cpp`: Core generation logic
  - Dual RNG seeding from Flow/Ornament
  - Cooldown-based density control
  - Algorithm dispatch and state management
  - Gate/CV output generation

**UI Layer** (`src/apps/sequencer/ui/pages/`):
- `TuesdayPage.h/cpp`: Parameter editing interface
- `TuesdayTrackListModel.h`: List model for parameter editing
- `TrackPage.cpp`: Shift+F5 reseed, context menu

### Implemented Algorithms

| # | Name | Type | Description |
|---|------|------|-------------|
| 0 | TEST | Utility | Calibration/test patterns (OCTSWEEPS/SCALEWALKER) |
| 1 | TRITRANCE | Melodic | German minimal arpeggios with phase modulation |
| 2 | STOMPER | Bass | Acid bass with 14-state machine, slides |
| 3 | MARKOV | Melodic | 3rd-order Markov chain transitions |
| 4 | CHIPARP | Melodic | 8-bit chiptune arpeggios with direction changes |
| 5 | GOACID | Bass | Goa/psytrance acid patterns with transposes |
| 6 | SNH | Melodic | Sample & Hold filtered random walk |
| 7 | WOBBLE | Bass | Dual-phase LFO bass wobbles |
| 8 | TECHNO | Rhythmic | Four-on-floor club patterns (kick/hat interleaved) |
| 9 | FUNK | Rhythmic | Syncopated grooves with ghost notes |
| 10 | DRONE | Ambient | Sustained textures with long gates (400%) |
| 11 | PHASE | Melodic | Minimalist phasing (Steve Reich style) |
| 12 | RAGA | Melodic | Indian classical with Bhairav/Yaman/Todi/Kafi scales |

### Flow & Ornament Controls Per Algorithm

| Algo | Flow Controls | Ornament Controls |
|------|---------------|-------------------|
| TEST | Mode, sweep speed | Accent, velocity |
| TRITRANCE | High note (b1), phase offset (b2) | Octave offset (b3) |
| STOMPER | Pattern mode, transitions | Note choices, countdown |
| MARKOV | History states, matrix col 0 | Matrix col 1 |
| CHIPARP | Chord seed, base note | Arpeggio direction |
| GOACID | Note sequence patterns | Transpose flags |
| SNH | Target note values | Velocity variation |
| WOBBLE | Phase transitions, slides | Velocity variation |
| TECHNO | Kick pattern (0-3), bass note | Hat pattern (0-3) |
| FUNK | Rhythm pattern (0-7), notes | Syncopation, ghost probability |
| DRONE | Base note, change speed | Interval type |
| PHASE | Pattern length (3-8), melodic cell | Phase drift speed |
| RAGA | Scale type, movements | Ornament/gamaka probability |

### UI Integration

**TuesdayPage:**
- F1-F5 select parameters
- Encoder adjusts selected parameter
- Visual bar graphs show values 0-16

**TrackPage:**
- Shift+F5: Reseed loop shortcut
- Context Menu: RESEED option (Tuesday tracks only)

### Testing Status

✅ **Implemented and verified:**
- Model layer with all parameters
- Engine with dual RNG, linear Power density, gate/CV output
- UI page with parameter editing
- 13 algorithms: TEST, TRITRANCE, STOMPER, MARKOV, CHIPARP, GOACID, SNH, WOBBLE, TECHNO, FUNK, DRONE, PHASE, RAGA
- Glide=0 properly disables all algorithm-generated slides
- Glide, Scale, Skew features
- Reseed via Shift+F5 and context menu
- Long gates (200-400%)
- Loop lengths 56 and 64

### Critical Bugs Found and Fixed (2025-11-21)

**Bug 1: Division by Zero on Project Load**
- **Symptom**: Hardware reboots when loading project with Tuesday track
- **Cause**: PHASE `_phaseLength` and DRONE `_droneSpeed` defaulted to 0, causing modulo by zero
- **Fix**: Safe defaults (4 and 1) + runtime guards at all modulo operations
- **Commit**: `c70a2e7`

**Bug 2: Serialization Missing Version Guards**
- **Symptom**: "Failed to load (end_of_file)" when loading projects with Tuesday tracks
- **Cause**: TuesdayTrack::read() had no version guards for first 5 fields
- **Fix**: Added ProjectVersion::Version35, all read() calls now use version guards with defaults
- **Commit**: `bcb12c9`

**Lesson Learned**: New track types need:
1. Safe default values for ALL state variables (avoid 0 for divisors)
2. ProjectVersion entry for serialization
3. Version guards on ALL read() calls with sensible defaults

### Key Files

- `src/apps/sequencer/model/TuesdayTrack.h/cpp` - Model layer
- `src/apps/sequencer/engine/TuesdayTrackEngine.h/cpp` - Engine layer
- `src/apps/sequencer/ui/pages/TuesdayPage.h/cpp` - UI page
- `src/apps/sequencer/ui/model/TuesdayTrackListModel.h` - UI list model
- `src/apps/sequencer/ui/pages/TrackPage.cpp` - Shift+F5, context menu
- `CLAUDE-TUESDAY.md` - Complete implementation reference
- `QWEN.md` Part 6 - Technical implementation details

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

## Documentation References

- **CLAUDE.md** (this file) - Main development reference for Claude Code
- **QWEN.md** - Complete accumulator feature implementation documentation
- **TODO.md** - Development task tracking and completed features
- **README.md** - Project overview and build instructions
- **PHASE-1-COMPLETE.md** - Harmony feature implementation summary and Phase 2 roadmap
- **doc/improvements/** - jackpf improvement documentation
  - `noise-reduction.md` - Display noise reduction techniques
  - `shape-improvements.md` - CV curve generation enhancements
  - `midi-improvements.md` - MIDI functionality extensions
- **doc/simulator-interface.png** - Simulator UI reference diagram

## Tuesday Track CV/Gate Behavior Analysis

Analysis of the differences between the original Tuesday Eurorack module and the PEW|FORMER TuesdayTrack implementation regarding CV (pitch) changes and gate firing behavior.

### Original Tuesday Module Behavior
- **CV Update**: Only when intensity threshold is met (`Tick->vel >= T->CoolDown`)
- **Gate Output**: Only fired when intensity threshold is met
- **Result**: When intensity filtering prevents a note from playing, both gate and CV output remain unchanged at previous values

### PEW|FORMER TuesdayTrack Behavior
- **CV Update**: Updated on every step regardless of gate state
- **Gate Output**: Only fired when algorithm says `shouldGate` AND cooldown period has expired
- **Result**: Continuous pitch evolution with sparse gate articulation

### Musical Implications

#### Original Tuesday
- More traditional sequencer behavior
- CV output is "gated" - only changes when notes are played
- Intensity parameter acts as both density control and gate filter
- Better suited for triggering envelope generators with each pitch change

#### PEW|FORMER TuesdayTrack
- Modern generative music behavior
- CV output continuously evolves, creating smooth pitch progressions
- Gates fire independently, allowing for rhythmic articulation of continuous pitch changes
- Better suited for evolving textures and algorithmic pitch progressions
- Works well with external envelope generators that need to be triggered independently

### Implementation Options for CV Update Mode Switch

#### Approach 1: Parameter-Based Switch (Recommended)
- Add new parameter `cvUpdateMode` to `TuesdayTrack` class
- Values: `Free` (current behavior) or `Gated` (original behavior)
- Store as bitfield in `_cvUpdateMode`
- Modify engine logic in `TuesdayTrackEngine::tick()` to conditionally execute CV updates
- Add parameter to UI in `TuesdayPage` for user control

#### State Management Considerations
- In GATED mode, engine needs to maintain the previous CV value when not updating
- Need to handle initialization and switching between modes correctly
- Consider how this affects slide/portamento behavior in both modes

### Files Referenced in Analysis
- `/ALGO-RESEARCH/Tuesday/Sources/Tuesday.c` - Original Tuesday implementation
- `/ALGO-RESEARCH/Tuesday/Sources/Tuesday.h` - Original Tuesday data structures
- `/src/apps/sequencer/engine/TuesdayTrackEngine.cpp` - PEW|FORMER implementation
- `/src/apps/sequencer/engine/TuesdayTrackEngine.h` - PEW|FORMER data structures
- `/src/apps/sequencer/model/TuesdayTrack.h` - PEW|FORMER model parameters

### Additional Documentation
A complete analysis of this behavior and implementation ideas was created in `TUESDAY-GATED-PITCH-CHANGE.md`.
