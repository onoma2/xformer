# Curve Analysis Tool

A visualization tool for analyzing the signal processing pipeline in the CurveTrackEngine of the PEW|FORMER Eurorack sequencer firmware.

## Purpose

This tool provides real-time visualization of how parameters affect the signal path in the CurveTrackEngine, allowing developers to:

- Visually analyze how parameters affect the signal at each stage
- See the intermediate processing steps (original, after wavefolding, after filtering, etc.)
- Adjust sample rates to see how they affect the processing
- View real-time parameter values and their effects
- Verify that changes to the signal processing code produce expected results
- Debug audio artifacts or unexpected behavior

## Signal Processing Pipeline

The tool simulates the complete signal path from phased shape evaluation to CV output, implementing the exact same functions used in the CurveTrackEngine:

1. **Phased Step Evaluation**: Using `evalStepShape` function with variation and invert capabilities
2. **Wavefolding**: Using `applyWavefolder` function with fold, gain, and symmetry parameters
3. **DJ Filter**: Using `applyDjFilter` function with LPF/HPF modes and resonance
4. **Amplitude Compensation**: Using `calculateAmplitudeCompensation` function
5. **Crossfading**: Between original phased shape and processed signal
6. **LFO-appropriate Limiting**: Using `applyLfoLimiting` function
7. **Feedback Processing**: Using feedback state for iterative processing

## Usage

- Run `./curve_analysis` from the build directory
- Control parameters using the sliders on the left
- Press number keys 1-8 to change curve shapes
- Press S for shape variation, I for invert
- Press R to reset internal states
- Press F1-F6 to change sample rates (8kHz to 96kHz)

## Key Features

- Real-time visualization of 6 signal stages:
  - Original Signal
  - Post Wavefolder
  - Post Filter
  - Post Compensation
  - Final Output
  - Hardware Limited Output (with real hardware constraints applied)
- Grid lines and center references for accurate signal analysis
- Parameter value displays
- Sample rate control
- Keyboard shortcuts for quick functionality
- Performance monitoring with CPU usage and time budget analysis
- Realistic hardware simulation:
  - 16-bit DAC resolution
  - 1ms CV update rate (1000Hz) matching PEW|FORMER hardware
  - Sample-and-hold behavior between updates
  - Quantization effects
- Adjustable hardware limitation parameters:
  - DAC Resolution (8-24 bits)
  - DAC Update Rate (0.001-10ms interval)
  - Timing Jitter (0-1ms)
- Performance visualization showing estimated processing load variation across the signal cycle
- Responsive UI that adapts to window size changes
- Resizable window support for better workflow
- High-DPI display support for Retina and other high-resolution screens

## Hardware Constraints Analysis

This tool now accurately models the real hardware constraints found in the PEW|FORMER:

### CV Update Rate
- Real hardware updates CV outputs every 1ms (1000 Hz) regardless of musical timing
- This is implemented in the engine task which runs with a 1ms period
- The "Hardware Limited Output" plot now simulates this 1ms update rate correctly

### DAC Specifications
- Uses 16-bit DAC (DAC8568) for output
- SPI communication with ~200ns per channel update time
- Output range limited to ±5.0V as in the real hardware

### Processing Constraints
- Engine runs with priority 4 and 1ms period
- Maximum 1ms processing time available per update cycle
- 4096-byte stack for the engine task
- All track processing and CV calculations must complete within this time

### Memory and CPU Usage
- The tool includes performance monitoring to show CPU usage as percentage of available time
- Time budget calculation based on sample rate and buffer size
- Visual indicators alert when processing approaches hardware limits