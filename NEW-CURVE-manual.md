# Curve Studio Manual

## 1. Introduction

**Curve Studio** represents a major overhaul of the Curve Track engine, transforming it from a simple LFO sequencer into a complex, "wavetable-style" modulation source. This manual covers the new signal processing chain, generative tools, and advanced playback features.

## 2. Signal Processor

The signal path has been expanded to include Chaos, Wavefolding, and Filtering.

### 2.1 Signal Flow
`Step Shape` -> `Chaos` -> `Wavefolder` -> `Filter` -> `Crossfade` -> `Limiter` -> `Output`

### 2.2 Wavefolder
- **Access**: Press **F5** to cycle to **WAVEFOLDER**.
- **FOLD**: Controls the amount of wavefolding (sine-based folding). Range: 0.00-1.00.
- **GAIN**: Input gain before the folder. Range: 0.00-2.00 (0% to 200%).

### 2.3 DJ Filter
- **FILTER**: One-knob filter control.
  - **Center (0.00)**: Filter off.
  - **Negative**: Low Pass Filter (LPF).
  - **Positive**: High Pass Filter (HPF).

### 2.4 Crossfade (XFADE)
- **XFADE**: Blends between the dry signal and the processed (folded/filtered) signal.

## 3. Chaos Engine

A generative engine inserted *before* the wavefolder.

- **Access**: Press **F5** to cycle to **CHAOS**.
- **Toggle Algorithm**: Hold **Shift + F1** to switch between **Latoocarfian** (Stepped/Sample&Hold) and **Lorenz** (Smooth/Attractor).
- **Controls**:
  - **AMT**: Modulation depth.
  - **HZ**: Evolution speed.
  - **P1/P2**: Algorithm-specific shape parameters.

## 4. Advanced Playback

### 4.1 Global Phase
- **Function**: Offsets the playback position of the sequence (0-100%).
- **Access**: Press **F5** to cycle to **PHASE**.
- **Use Case**: Phase-shifting LFOs, canon effects, fine-tuning timing.

### 4.2 True Reverse Playback
The engine now supports "True Reverse" playback for shapes.
- **Backward Mode**: Steps play in reverse order (4, 3, 2...), AND the internal shape plays backwards (100% -> 0%).
- **Pendulum/PingPong**: Shapes play forward on the "up" phase and backward on the "down" phase, creating perfectly smooth loops.

### 4.3 Min/Max Inversion
You can now create inverted signal windows by setting **Min > Max**.
- **Normal**: Min=0, Max=255 (Signal goes UP).
- **Inverted**: Min=255, Max=0 (Signal goes DOWN).
- **Step**: The step shape scales from "Start" (Min) to "End" (Max).

## 5. Curve Studio Workflow

Curve Studio introduces powerful context menus for populating sequences.

### 5.1 LFO Context Menu
Populate steps with single-cycle waveforms. Defaults to the active loop range (First Step to Last Step) unless steps are selected.

- **Access**: Press **Page + Step 6** (Button 6).
- **Options**:
  - **TRI**: Triangle (1 cycle/step).
  - **SINE**: Sine / Bell (1 cycle/step).
  - **SAW**: Sawtooth / Ramp (1 cycle/step).
  - **SQUA**: Square / Pulse (1 cycle/step).
  - **MM-RND**: Randomize Min/Max values for each step (supports inversion).

### 5.2 Macro Shape Menu
"Draw" complex shapes that span across multiple steps. The system calculates the precise start/end points for each step to create a seamless high-resolution curve.

**Target Range Logic:**
- **No Selection**: Applies to the entire active loop.
- **Multiple Steps**: Applies to the selected range.
- **Single Step**: Applies from the selected step to the end of the loop (perfect for "Rasterize to Right").

- **Access**: Press **Page + Step 5** (Button 5).
- **Options**:
  - **MM-INIT**: **Min/Max Reset**. Resets the range to default (Min=0, Max=255, Shape=Triangle).
  - **M-FM**: **Chirp / FM**. A sine wave that accelerates in frequency over the range.
  - **M-DAMP**: **Damped Oscillation** (Decaying Sine, 4 cycles). Great for percussive envelopes.
  - **M-BOUNCE**: **Bouncing Ball** physics (Decaying absolute sines).
  - **M-RSTR**: **Rasterize**. Takes the shape and range of the *first step* in the range and stretches it across the entire range.

### 5.3 Transform Menu
Manipulate existing sequence data. Defaults to the active loop range unless steps are selected.

- **Access**: Press **Page + Step 7** (Button 7).
- **Options**:
  - **T-INV**: **Invert**. Swaps Min and Max values (Flips slope direction).
  - **T-REV**: **Reverse**. Reverses step order and internal shape direction (True Audio Reverse).
  - **T-MIRR**: **Mirror**. Reflects voltages across the centerline (High becomes Low, Low becomes High).
  - **T-ALGN**: **Align**. Ensures continuity by setting each step's Start (Min) to the previous step's End (Max).
  - **T-WALK**: **Smooth Walk**. Generates a continuous random path ("Drunken Sine").

### 5.4 Multi-Step Gradient Editing
Create smooth transitions for Shape, Min, and Max values.

- **Action**: 
  1. Select multiple steps (Hold first step, press last step).
  2. Hold **Shift** and turn the Encoder on **MIN** or **MAX**.
- **Result**: The values will form a linear gradient from the first selected step to the last.

### 5.4 Settings Management
Copy/Paste/Init the Wavefolder and Chaos settings independently of the sequence steps.

- **Access**: Long-press **Context Menu** button while in Wavefolder or Chaos page.
- **Options**: **INIT**, **RAND**, **COPY**, **PASTE**.