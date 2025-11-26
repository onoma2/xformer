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
  - Phased Signal
  - Final Output
- Grid lines and center references for accurate signal analysis
- Parameter value displays
- Sample rate control
- Keyboard shortcuts for quick functionality