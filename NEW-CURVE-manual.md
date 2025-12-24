# Curve Track New Features Manual

## 1. Introduction

The Curve Track has been significantly enhanced with advanced modulation and signal processing capabilities. This manual covers the new "Phase", "Wavefolder", and "Chaos" features, as well as new workflow shortcuts.

These features transform the Curve Track from a simple LFO sequencer into a complex modulation source capable of generating evolving, chaotic, and harmonically rich control voltages.

## 2. Global Phase

The Global Phase feature allows you to offset the playback position of your curve sequence relative to the actual step counter. This effectively "rotates" the sequence in floating-point precision without changing the underlying step data.

### 2.1 Overview
- **Function**: Adds an offset (0-100%) to the read position of the sequence.
- **Visuals**: A dimmer vertical line indicates the *phased* playback position, while the bright line shows the *actual* step position.
- **Use Case**: Phase-shifting LFOs against each other, creating "canon" effects with multiple tracks, or fine-tuning modulation timing.

### 2.2 Controls
- **Access**: Press **F5** (Function key 5) to cycle the edit mode to **PHASE**.
- **Edit**: Turn the encoder to adjust phase from 0.00 to 1.00.
- **Recording**: When recording CV into a Curve Track with Global Phase active, the recorded value is passed through; the phase offset is applied to playback.

## 3. Signal Processor (Wavefolder & Filter)

The signal path now includes a post-processing stage with a Wavefolder and a DJ-style Filter. This allows you to take a simple LFO shape (like a triangle) and fold it into complex harmonics or filter it.

### 3.1 Signal Flow
`Step Shape` -> `Chaos` -> `Wavefolder` -> `Filter` -> `Crossfade` -> `Limiter` -> `Output`

### 3.2 Wavefolder
- **Access**: Press **F5** to cycle to **WAVEFOLDER**.
- **FOLD**: Controls the amount of wavefolding (sine-based folding).
  - range: 0.00 to 1.00
  - Increases the number of folds (harmonics) as you increase the value.
- **GAIN**: Input gain before the folder.
  - range: 0.00 to 2.00 (0% to 200%)
  - Driving the gain harder into the folder creates more aggressive timbres.

### 3.3 DJ Filter
- **FILTER**: One-knob filter control.
  - **Center (0.00)**: Filter off.
  - **Negative (-1.00 to -0.01)**: Low Pass Filter (LPF). Removes highs.
  - **Positive (0.01 to 1.00)**: High Pass Filter (HPF). Removes lows.

### 3.4 Crossfade (XFADE)
- **XFADE**: Blends between the original (dry) signal and the processed (folded/filtered) signal.
  - **0.00**: Dry signal only.
  - **1.00**: Wet signal only.

## 4. Chaos Engine

A Chaos Generator has been added to the signal path *before* the wavefolder. This allows for generative, non-repetitive modulation that can still be constrained by the sequence structure.

### 4.1 Algorithms
- **Latoocarfian**: A hyperchaotic map that generates stepped, unpredictable values. Great for sample-and-hold style chaos.
- **Lorenz**: A fluid, strange attractor system. Generates smooth, orbiting values.

### 4.2 Controls
- **Access**: Press **F5** to cycle to **CHAOS**.
- **Toggle Algorithm**: Hold **Shift + F1 (AMT)** to switch between Latoocarfian and Lorenz.
- **AMT (Amount)**: Depth of chaotic modulation added to the curve signal.
- **HZ (Rate)**: Speed of the chaos evolution.
- **P1 / P2**: Shape parameters specific to the algorithm.
  - *Latoocarfian*: Affects the "a/b" and "c/d" coefficients (attractor shape).
  - *Lorenz*: Affects "Rho" and "Beta" (orbit size and geometry).

## 5. Workflow Shortcuts

### 5.1 LFO Context Menu
Quickly populate a sequence (or selected steps) with standard LFO shapes.

- **Action**: Press **Page + Step 6** (while in Sequence Page).
- **Options**:
  - **TRI**: Triangle waves.
  - **SINE**: Sine wave approximation (SmoothUp/Down).
  - **SAW**: Sawtooth waves (RampUp).
  - **SQUA**: Square waves (StepUp/Down).
- **Selection**: If steps are selected, only those steps are filled. Otherwise, the entire sequence is filled.

### 5.2 Multi-Step Gradient Editing
Create smooth transitions for Shape, Min, and Max values across multiple steps.

- **Action**: 
  1. Select multiple steps (Hold first step, press last step).
  2. Hold **Shift** and turn the Encoder on **MIN** or **MAX**.
- **Result**: The values will form a linear gradient from the first selected step to the last.

### 5.3 Settings Management
You can Copy/Paste/Init the Wavefolder and Chaos settings independently of the sequence steps.

- **Access**: Long-press **Context Menu** button while in Wavefolder or Chaos page.
- **Options**:
  - **INIT**: Reset settings to default.
  - **RAND**: Randomize current page parameters.
  - **COPY**: Copy settings to clipboard.
  - **PASTE**: Paste settings from clipboard.
