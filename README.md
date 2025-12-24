## XFORMER Summary

XFORMER is a fork of the Phazerville PER|FORMER firmware with several algorithmic and musical enhancement features:

### Warnings
- The update is based on Phazerville fork of the original firmware, so no
  Mebitek or other features. No new modulation ROUTES are implemented for new
track features (yet).
  MIDI and Launchpad functionalities WERE NOT tested at all.
- It is important to note that 99,5% of the code was written by musician little
  helpers aka LLMs, that you can by for couple of dollars a month.
- I have tested it on my unit and it seems that everything that I care about works. But no backwards compatibility, no guarantees, no sincere or not condolences are planned to happen.
- Use at your own risk. Make a backup of your projects on a separate SD.

### Key Features

#### Algo Track
- A special track type that generates musical content algorithmically
- 21 built-in algorithms that produce deterministic results (same parameters = same outputs)
- Parameters: Algorithm (0-14), Flow (0-16), Ornament (0-16), Power (0-16)
- Additional controls: Loop Length, Scan, Rotate, CV Mode, Glide, Gate Offset, Trill, Skew

#### DiscreteMap Track
- Threshold-based sequencer mapping input voltages (Internal Ramp or External CV) to 32 discrete output stages
- Each stage has independent Threshold, Direction (Rise/Fall/Off), and Note value
- **Scanner**: `DMap Scan` routing target allows CV-controlled stage flipping
- **Generators**: Random, Linear, Logarithmic, Exponential generation of thresholds/notes

#### Indexed Track
- Duration-based sequencer with independent step lengths (0-65535 ticks)
- Ideal for polyrhythms, unquantized timing, and complex envelopes
- **Math Operations**: Batch processing (Add, Sub, Ramp, Quantize, Random, Jitter)
- **Quick Edit**: Set First Step (Rotate), Active Length, Run Mode

#### NOTE TRACK Harmony Engine
- Creates harmonic relationships between tracks by generating chords based on scale degrees
- Supports 7 harmony modes: Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian
- Voicing options: Close, Drop2, Drop3, and Spread
- Master/Follower system for harmonizing tracks

#### NOTE TRACK Accumulator
- Counter that can count up, down, or in complex patterns based on trigger inputs
- Multiple counting modes: Wrap, Pendulum, Random, Hold
- Can be triggered by step events, gate events, or retrigger events

#### NOTE TRACK Pulse Count and Gate Mode
- Pulse Count: Allows steps to play for multiple clock pulses before advancing
- Values range from 0 to 7 (1 to 8 pulses total)
- Gate Mode: Controls which of multiple pulses fire a gate signal
- Four modes: All, First, Hold, FirstLast

#### Global Output Rotation
- Dynamically rotate which tracks are assigned to physical CV and Gate outputs
- Acts like a virtual 8-channel sequential switch
- Allows modulation patterns to "spin" around connected modules

#### Routing Enhancements
- **VCA Next Shaper**: Amplitude modulation using the next route's raw source
- **Per-Track Reset**: Hard reset of track engine via routing target (Rising Edge)

#### Curve Track Features
- **Global Phase**: Offset the playback position of curve sequences (0.00 to 1.00 range)
- **Wavefolding**: Advanced signal processing with FOLD, GAIN, FILTER, and XFADE parameters
- **Shift+Step Shortcut**: Create smooth transitions across multiple selected steps
- **LFO Context Menu**: Quick fill options for common LFO waveforms (Triangle, Sine, Sawtooth, Square)
- **Phase Visualization**: Visual feedback showing both actual and phased step positions


# PEW|FORMER

_From [Phazerville](https://phazerville.com) with Love <3_

I'm working to bring this firmware up-to-date, and make myself at home with the UI controls. I've managed to update the toolchain to gcc 14.2, libopencm3 to the latest from October 2024, and the GitHub Actions CI workflow will produce an artifact file with the binaries for every push.

After encountering instability on the mebitek fork, I've decided to start from the known-good master branch of the original firmware, and carefully work in some of the new features from other forks.

## jackpf Improvements

| Change             	| Documentation                                    	|
|--------------------	|--------------------------------------------------	|
| Noise reduction    	| [docs](./doc/improvements/noise-reduction.md)    	|
| Shape improvements 	| [docs](./doc/improvements/shape-improvements.md) 	|
| MIDI improvements 	| [docs](./doc/improvements/midi-improvements.md) 	|

--- original documentation below ---

<a href="doc/sequencer.jpg"><img src="doc/sequencer.jpg"/></a>

## Overview

This repository contains the firmware for the **PER|FORMER** eurorack sequencer.

For more information on the project go [here](https://westlicht.github.io/performer).

The hardware design files are hosted in a separate repository [here](https://github.com/westlicht/performer-hardware).

## Development

If you want to do development on the firmware, the following is a quick guide on how to setup the development environment to get you going.

### Setup on macOS and Linux

First you have to clone this repository (make sure to add the `--recursive` option to also clone all the submodules):

```
git clone --recursive https://github.com/westlicht/performer.git
```

After cloning, enter the performer directory:

```
cd performer
```

Make sure you have a recent version of CMake installed. If you are on Linux, you might also want to install a few other packages. For Debian based systems, use:

```
sudo apt-get install libtool autoconf cmake libusb-1.0.0-dev libftdi-dev pkg-config
```

To compile for the hardware and allow flashing firmware you have to install the ARM toolchain and build OpenOCD:

```
make tools_install
```

Next, you have to setup the build directories:

```
make setup_stm32
```

If you also want to compile/run the simulator use:

```
make setup_sim
```

The simulator is great when developing new features. It allows for a faster development cycle and a better debugging experience.

### Setup on Windows

Currently, there is no native support for compiling the firmware on Windows. As a workaround, there is a Vagrantfile to allow setting up a Vagrant virtual machine running Linux for compiling the application.

First you have to clone this repository (make sure to add the `--recursive` option to also clone all the submodules):

```
git clone --recursive https://github.com/westlicht/performer.git
```

Next, go to https://www.vagrantup.com/downloads.html and download the latest Vagrant release. Once installed, use the following to setup the Vagrant machine:

```
cd performer
vagrant up
```

This will take a while. When finished, you have a virtual machine ready to go. To open a shell, use the following:

```
vagrant ssh
```

When logged in, you can follow the development instructions below, everything is now just the same as with a native development environment on macOS or Linux. The only difference is that while you have access to all the source code on your local machine, you use the virtual machine for compiling the source.

To stop the virtual machine, log out of the shell and use:

```
vagrant halt
```

You can also remove the virtual machine using:

```
vagrant destroy
```

### Build directories

After successfully setting up the development environment you should now have a list of build directories found under `build/[stm32|sim]/[release|debug]`. The `release` targets are used for compiling releases (more code optimization, smaller binaries) whereas the `debug` targets are used for compiling debug releases (less code optimization, larger binaries, better debugging support).

### Developing for the hardware

You will typically use the `release` target when building for the hardware. So you first have to enter the release build directory:

```
cd build/stm32/release
```

To compile everything, simply use:

```
make -j
```

Using the `-j` option is generally a good idea as it enables parallel building for faster build times.

To compile individual applications, use the following make targets:

- `make -j sequencer` - Main sequencer application
- `make -j sequencer_standalone` - Main sequencer application running without bootloader
- `make -j bootloader` - Bootloader
- `make -j tester` - Hardware tester application
- `make -j tester_standalone` - Hardware tester application running without bootloader

Building a target generates a list of files. For example, after building the sequencer application you should find the following files in the `src/apps/sequencer` directory relative to the build directory:

- `sequencer` - ELF binary (containing debug symbols)
- `sequencer.bin` - Raw binary
- `sequencer.hex` - Intel HEX file (for flashing)
- `sequencer.srec` - Motorola SREC file (for flashing)
- `sequencer.list` - List file containing full disassembly
- `sequencer.map` - Map file containing section/offset information of each symbol
- `sequencer.size` - Size file containing size of each section

If compiling the sequencer, an additional `UPDATE.DAT` file is generated which can be used for flashing the firmware using the bootloader.

To simplify flashing an application to the hardware during development, each application has an associated `flash` target. For example, to flash the bootloader followed by the sequencer application use:

```
make -j flash_bootloader
make -j flash_sequencer
```

## New Features

### Accumulator

The PEW|FORMER firmware now includes an advanced accumulator feature that provides powerful parameter modulation capabilities:

#### Core Features:
- **Stateful Counter**: Increments/decrements based on configurable parameters
- **Step-Triggered**: Updates when specific sequence steps are triggered
- **Multi-Mode Operation**: Supports four distinct operational modes
- **Real-Time Modulation**: Applies modulation to note pitch in real-time

#### Operational Modes:
- **Wrap**: Values wrap from max to min and vice versa
- **Pendulum**: Bidirectional counting with direction reversal at boundaries
- **Random**: Generates random values within min/max range when triggered
- **Hold**: Clamps at min/max boundaries without wrapping

#### UI Controls:
- **ACCUM Page**: Dedicated parameter editing interface with "ACCUM" header
- **ACCST Page**: Per-step trigger configuration with "ACCST" header
- **Integration**: Accessible via existing sequence navigation (Sequence key cycles: NoteSequence → Accumulator → AccumulatorSteps → NoteSequence)

#### Configuration Parameters:
- Enable/Disable control
- Direction (Up, Down, Freeze)
- Order (Wrap, Pendulum, Random, Hold)
- Polarity (Unipolar, Bipolar)
- Value Range (-100 to 100)
- Step Size (1-100)
- Trigger mapping per step

For complete implementation details, see `QWEN.md`.

### Pulse Count

A Metropolix-style pulse count feature that allows each step to repeat for a configurable number of clock pulses (1-8) before advancing to the next step.

#### Features:
- **Variable Step Duration**: Each step can play for 1-8 clock pulses
- **Independent Control**: Per-step configuration without changing global divisor
- **Seamless Integration**: Works with all play modes and existing features

#### UI Access:
1. Navigate to STEPS page
2. Press Retrigger button (F2) twice to reach "PULSE COUNT" layer
3. Select steps with S1-S16 buttons
4. Adjust pulse count (1-8) with encoder

#### Use Cases:
- Create polyrhythmic patterns
- Add emphasis to specific steps
- Generate complex timing without changing tempo
- Combine with retrigger for intricate rhythmic structures

### Gate Mode

Gate mode controls how gates are fired during pulse count repetitions, providing fine-grained rhythmic control.

#### Four Gate Modes:
- **A (ALL)**: Fires gates on every pulse (default)
- **1 (FIRST)**: Single gate on first pulse only
- **H (HOLD)**: One long continuous gate for entire step duration
- **1L (FIRSTLAST)**: Gates on first and last pulse only

#### UI Access:
1. Navigate to STEPS page
2. Press Gate button (F1) five times to reach "GATE MODE" layer
3. Select steps with S1-S16 buttons
4. Adjust gate mode with encoder (displays A/1/H/1L)

#### Use Cases:
- Create accent patterns with FIRST mode
- Sustain notes across pulses with HOLD mode
- Generate bouncing rhythms with FIRSTLAST mode
- Mix different gate modes across steps for complex patterns

#### Compatibility:
- Works seamlessly with pulse count feature
- Compatible with retrigger, gate offset, and gate probability
- Backward compatible (default mode maintains existing behavior)
- Fully integrated with all play modes

For complete technical documentation, see `CLAUDE.md`.

Flashing to the hardware is done using OpenOCD. By default, this expects an Olimex ARM-USB-OCD-H JTAG to be attached to the USB port. You can easily reconfigure this to use a different JTAG by editing the `OPENOCD_INTERFACE` variable in the `src/platform/stm32/CMakeLists.txt` file. Make sure to change both occurrences. A list of available interfaces can be found in the `tools/openocd/share/openocd/scripts/interface` directory (or `/home/vagrant/tools/openocd/share/openocd/scripts/interface` when running the virtual machine).

### Developing for the simulator

Note that the simulator is only supported on macOS and Linux and does not currently run in the virtual machine required on Windows.

You will typically use the `debug` target when building for the simulator. So you first have to enter the debug build directory:

```
cd build/sim/debug
```

To compile everything, simply use:

```
make -j
```

To run the simulator, use the following:

```
./src/apps/sequencer/sequencer
```

Note that you have to start the simulator from the build directory in order for it to find all the assets.

### Source code directory structure

The following is a quick overview of the source code directory structure:

- `src` - Top level source directory
- `src/apps` - Applications
- `src/apps/bootloader` - Bootloader application
- `src/apps/hwconfig` - Hardware configuration files
- `src/apps/sequencer` - Main sequencer application
- `src/apps/tester` - Hardware tester application
- `src/core` - Core library used by both the sequencer and hardware tester application
- `src/libs` - Third party libraries
- `src/os` - Shared OS helpers
- `src/platform` - Platform abstractions
- `src/platform/sim` - Simulator platform
- `src/platform/stm32` - STM32 platform
- `src/test` - Test infrastructure
- `src/tests` - Unit and integration tests

The two platforms both have a common subdirectories:

- `drivers` - Device drivers
- `libs` - Third party libraries
- `os` - OS abstraction layer
- `test` - Test runners

The main sequencer application has the following structure:

- `asteroids` - Asteroids game
- `engine` - Engine responsible for running the sequencer core
- `model` - Data model storing the live state of the sequencer and many methods to change that state
- `python` - Python bindings for running tests using python
- `tests` - Python based tests
- `ui` - User interface

## Third Party Libraries

The following third party libraries are used in this project.

- [FreeRTOS](http://www.freertos.org)
- [libopencm3](https://github.com/libopencm3/libopencm3)
- [libusbhost](https://github.com/libusbhost/libusbhost)
- [NanoVG](https://github.com/memononen/nanovg)
- [FatFs](http://elm-chan.org/fsw/ff/00index_e.html)
- [stb_sprintf](https://github.com/nothings/stb/blob/master/stb_sprintf.h)
- [stb_image_write](https://github.com/nothings/stb/blob/master/stb_image_write.h)
- [soloud](https://sol.gfxile.net/soloud/)
- [RtMidi](https://www.music.mcgill.ca/~gary/rtmidi/)
- [pybind11](https://github.com/pybind/pybind11)
- [tinyformat](https://github.com/c42f/tinyformat)
- [args](https://github.com/Taywee/args)

## License

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

This work is licensed under a [MIT License](https://opensource.org/licenses/MIT).
