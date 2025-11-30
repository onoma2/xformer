# Curve Analysis Tool

A visualization tool for analyzing the signal processing pipeline in the CurveTrackEngine of the PEW|FORMER Eurorack sequencer firmware.

## Purpose

This tool provides real-time visualization of how parameters affect the signal path in the CurveTrackEngine, allowing developers to:

- Visually analyze how parameters affect the signal at each stage
- See the intermediate processing steps (original, after phase skew, after wavefolding, after filtering, etc.)
- Adjust sample rates to see how they affect the processing
- View real-time parameter values and their effects
- Verify that changes to the signal processing code produce expected results
- Debug audio artifacts or unexpected behavior

## Signal Processing Pipeline

The tool simulates the complete signal path from phased shape evaluation to CV output, implementing the exact same functions used in the CurveTrackEngine:

1.  **Phase Generation**: Linear phase ramp 0->1 based on Frequency.
2.  **Phase Skew (NEW)**: Warps the time domain logarithmically/exponentially (Rushing/Dragging feel).
    *   Feedback: Shape -> Skew, Filter -> Skew.
3.  **Phase Mirror (NEW)**: Reflects the phase (Ping-Pong / Through-Zero).
    *   Feedback: Shape -> Mirror.
4.  **Shape Evaluation**: `evalStepShape` generates the base waveform (Sine, Tri, etc.).
5.  **Wavefolding**: `applyWavefolder` with fold, gain, and symmetry.
    *   Feedback: Shape -> Fold, Filter -> Fold.
6.  **DJ Filter**: `applyDjFilter` with LPF/HPF modes, resonance, and adjustable slope (6/12/24 dB/oct).
    *   Feedback: Fold -> Filter Frequency.
7.  **Amplitude Compensation**: `calculateAmplitudeCompensation` (Toggleable).
8.  **Crossfading**: Between original phased shape and processed signal.
9.  **LFO Limiting**: `applyLfoLimiting` (Soft limiting).
10. **Hardware Output**: Simulation of DAC quantization and update rate (Sample & Hold).

## Usage

- Run `./curve_analysis` from the build directory
- **Controls:**
    - Click headers (`[+]`/`[-]`) to expand/collapse sections.
    - Drag sliders to adjust values.
    - **Shift + Click** a control to reset it to default.
    - **Checkbox** toggles enable/disable entire processing blocks to save CPU or bypass effects.
- **Audio Engine:**
    - Press **'P'** to toggle Audio ON/OFF.
    - Adjust "Audio Vol" and "Audio Mod Amt" in the bottom section.
    - Hear the LFO modulating a 220Hz sine wave in real-time.

## Key Features

### Visualization (4x2 Grid)
- **Row 1:** Input Signal, Skewed Phase (Time Warp), Mirrored Phase, Post Wavefolder.
- **Row 2:** Post Filter, Final Output, Hardware Limited Output, Post Compensation.
- **Spectrum:** Real-time FFT analysis of the selected stage.

### Signal Chain Features
- **Phase Skew:** -1.0 (Rush) to +1.0 (Drag).
- **Phase Mirror:** 0.0 (Saw) to 0.5 (Tri) to 1.0 (Inv Saw).
- **Wavefolder:** Sine-based folding with Gain and Symmetry.
- **DJ Filter:** 1-pole to 4-pole (6/12/24 dB) resonant filter.
- **Feedback Matrix:** 6 distinct feedback paths (Shape->Fold, Fold->Filter, Filter->Fold, Shape->Skew, Filter->Skew, Shape->Mirror) with bipolar controls.

### Hardware Simulation
- **LFO Frequency:** 0.01Hz to 10Hz (Non-linear control).
- **DAC Resolution:** 12-16 bits.
- **DAC Update Rate:** 0.1ms - 5.0ms.
- **Safety Analysis Panel:**
    - **Max Step Jump:** Detects slew rate violations (>0.5V/ms).
    - **Algo Cost:** Estimates CPU complexity score.
    - **Clipping %:** Detects signal loss at ±5V rails.

### Audio Engine
- Real-time sonification of the LFO.
- Phase-accurate playback (you hear the steps if they exist).
- Safety limiter on audio output.

## Hardware Constraints Analysis

This tool accurately models the real hardware constraints found in the PEW|FORMER:

### CV Update Rate
- Real hardware updates CV outputs every 1ms (1000 Hz) regardless of musical timing
- The "Hardware Limited Output" plot simulates this 1ms update rate correctly.
- **Stair-Stepping Detection:** Visual (Red Border) and text alert when the signal changes too fast for the hardware to track (>50mV jump).

### DAC Specifications
- Uses 16-bit DAC (DAC8568) for output range ±5.0V.

### Processing Constraints
- **Algo Cost** metric helps you budget your CPU usage.
- Keep "Algo Cost" below 10-15 for safety on STM32F4.

## UI Improvements (v2.0)
- **Collapsible Sections:** "Signal Chain", "Advanced Shaping", "Hardware Simulation", "Fine Tuning", "Audio Engine".
- **Color-Coded Alerts:** Green/Orange/Red indicators for safety metrics.
- **Smart Interaction:** Dragging works only inside controls; Shift+Click resets.
- **Dense Layout:** Optimized to show 8 graphs and 40+ controls on a standard screen.