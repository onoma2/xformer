# XFORMER User Manual

## 0. WARININGS

The update is based on Phazerville fork of the original firmware, so no
Mebitek or other features. No new modulation ROUTES are implemented (yet).
MIDI and Launchpad functionalities WERE NOT tested at all.

It is important to note that 99,5% of the code was written by musician littel
helpers, that you can by for couple of dollars a month.

I have tested it on my unit and it seems that everything that I care about works. But no backwards compartibility, no guaranties, no sincere or not condolecenses are planned to happen.

Use at your own risk. Make a backup of your projects on a separate SD.




## 1. Introduction

This manual covers the XFORMER-specific features:

   - **Algo Track**: Generative sequencing based on TINRS Tuesday algorithms.
   - **DiscreteMap Track**: Threshold-based sequencing mapping inputs to stages.
   - **Indexed Track**: Duration-based sequencing with independent step lengths.
   - **Note Track Enhancements**:
   	- Harmony Engine (Inspired by Instruo Harmonaig)
	- Accumulator (Inspired by Intellijel Metropolix)
	- Pulse Count and Gate Modes
   - **Curve Track Enhancements**:
		- Global Phase
		- Wavefolder
   - **Routing**:
        - CV/Gate Output Rotation
        - VCA Next Shaper
        - Per-Track Reset

It assumes you are already familiar with the standard functionality of the Westlicht PER|FORMER.

## 2. Algo Track

### 2.1 What is Algo Track?

Algo Track is a special track type that generates musical content algorithmically. Most of the algos are deterministic, so same parameters produce same results.

To enable Algo Track:
1. Navigate to the track settings page for a selected track
2. Change the track type to "Algo" (formerly Tuesday)

### 2.2 UI Overview

- **Shift-S1**: Main UI
    - **Page 1:** Algorithm, Flow, Ornament, Power
    - **Page 2:** Loop, Rotate, Glide, Skew
    - **Page 3:** Gate Length, Gate Offset, Trill, Start
    - **Status Box:** Visual feedback showing current note, gate status, voltage, and step position.

- **Shift-S2**: Divisor, Root, Octave
- **Shift-S3**: Menu with same controls as main UI

**Shortcuts:**
- **F1-F4**: Select parameter
- **F5**: Next Page
- **Shift+F5**: **RESEED** (Generate new random seed for current algorithm)
- **Context Menu (Long Press)**: INIT, RESEED
- **Quick Edit (Step 15)**: **Randomize** all parameters

### 2.3 The Algorithms

(See full list in TUESDAY_MANUAL.md)
Includes 21 algorithms like **Stomper** (Acid bass), **Markov** (Chains), **ChipArp**, **Wobble** (LFO bass), **Techno**, **Drone**, etc.

### 2.4 Common Parameters

- **Algorithm (0-14)**: Selects the generation logic.
- **Flow/Ornament (0-16)**: Algorithm-specific macro controls.
- **Power (0-16)**: Note density and cooldown. 0 = Silence.
- **Loop (0-29)**: 0=Infinite, >0=Finite loop length.
- **Rotate**: Shifts start point of finite loops.
- **Scan**: Offsets the window into the infinite pattern buffer.
- **Skew**: Density bias across the loop (front/back loaded).

## 3. DiscreteMap Track

### 3.1 Overview
DiscreteMap maps an input signal (internal ramp or external CV) to 32 discrete output "Stages". Each stage triggers when the input crosses a specific voltage threshold.

**Key Features:**
- **Thresholds**: Voltage levels (-100 to +100) that trigger stages.
- **Directions**: **Rise** (↑), **Fall** (↓), or **Off**. A stage only triggers if crossed in the specified direction.
- **Notes**: Output note/voltage for each stage.

### 3.2 Input Sources
- **Internal Saw/Tri**: LFO-like ramps (synced to Divisor).
- **External**: Uses routed CV Input. Great for wave-shaping or envelope following.

### 3.3 Advanced Features
- **Scanner (Routing Target)**: Route CV to `DMap Scan`. As CV changes, it toggles the direction of stages it passes over.
- **Generator (Long Press GEN)**:
    - **Types**: Random, Linear, Log, Exp.
    - **Targets**: Thresholds (THR), Notes (NOTE2/NOTE5), Toggles (TOG).
- **Quick Edit**:
    - **Step 12**: Evenly distribute active stages.
    - **Step 13**: Flip all stage directions.

## 4. Indexed Track

### 4.1 Overview
Indexed Track is a duration-based sequencer. Unlike standard steps that are locked to a grid, each Indexed step has its own **Duration** in clock ticks (0-65535).

**Use Cases**: Polyrhythms, unquantized timing, complex envelopes.

### 4.2 Step Parameters
- **Duration**: Length of step in ticks.
- **Gate**: Gate length % or Trigger mode.
- **Note**: Output voltage index.

### 4.3 Math Operations (F5)
Apply batch operations to selected steps:
- **Arithmetic**: Add, Sub, Mul, Div, Set.
- **Ramp**: Linear interpolation between values.
- **Quant**: Quantize values to nearest multiple.
- **Rand/Jitter**: Randomization.

### 4.4 Quick Edit
- **Step 12**: Set First Step (Rotate sequence).
- **Step 13**: Active Length.
- **Step 14**: Run Mode.
- **Step 15**: Reset Measure.

## 5. Note Track Enhancements

### 5.1 Harmony Engine

The Harmony Engine creates complex harmonic relationships between tracks by generating chords based on scale degrees and applying various harmonic transformations. It works by taking a root note from a "Master" track and generating harmonized chord tones that can be assigned to "Follower" tracks.

The system uses a combination of:

- **Scale intervals:** Based on the selected mode (Ionian, Dorian, Phrygian, etc.)
- **Chord qualities:** Determined by the mode (Major7, Minor7, Dominant7, HalfDim7)
- **Inversions:** Root, 1st, 2nd, and 3rd inversions
- **Voicings:** Close, Drop2, Drop3, and Spread voicings
- **Transpose:** Global transposition of all chord tones

#### Modes and Settings

**Harmony Modes:**

- **Ionian (0):** Major scale - I∆7, ii-7, iii-7, IV∆7, V7, vi-7, viiø
- **Dorian (1):** Dorian mode - i-7, ii-7, ♭III∆7, IV7, v-7, viø, ♭VII∆7
- **Phrygian (2):** Phrygian mode - i-7, ♭II∆7, ♭III7, iv-7, vø, ♭VI∆7, ♭vii-7
- **Lydian (3):** Lydian mode - I∆7, II7, iii-7, #ivø, V∆7, vi-7, vii-7
- **Mixolydian (4):** Mixolydian mode - I7, ii-7, iiiø, IV∆7, v-7, vi-7, ♭VII∆7
- **Aeolian (5):** Natural minor - i-7, iiø, ♭III∆7, iv-7, v-7, ♭VI∆7, ♭VII7
- **Locrian (6):** Diminished scale - iø, ♭II∆7, ♭iii-7, iv-7, ♭V∆7, ♭VI7, ♭vii-7

**Voicing Options:**

- **Close (0):** All chord tones in closest possible position
- **Drop2 (1):** Second highest note dropped down an octave
- **Drop3 (2):** Third highest note dropped down an octave
- **Spread (3):** Third and fifth above root, seventh an octave higher

#### Harmony Roles (Master/Follower)

**Setting up Master/Follower Relationships:**
1. Configure one track as the "Master" track (typically a Note track playing scale degrees)
2. Enable Harmony Engine on the Master track with desired settings
3. Set Following type on ANOTHER note track
4. Each Master track can generate up to 4 harmonized outputs (root, third, fifth, seventh)
5. By default Performer settings tracks designated as Followers will update their CV output on Gate events. So enable gates where you want to hear a chord.

**Master Track Configuration:**
- The root note of the chord is determined by the Master track's current note
- The Harmony Engine converts these scale degrees to chord tones based on the selected mode
- Per step overrides of Inversion and Voicing are available on the NOTE(F4) button layer for Master tracks

**Follower Track Configuration:**
- Follower tracks receive the harmonized chord tones from the Master
- Each Follower can be assigned to a specific chord tone (root, third, fifth, or seventh)
- Multiple Followers can be assigned to the same chord tone for thickening
- **Note**: Followers DON'T change their Harmony Scale automatically if you change master!

#### Shortcuts Related to Harmony

- **SECOND PRESS of Shift-S3**: Harmony Engine
- **Voicing Quick Switch**: Use encoder to cycle through voicing options
- **Inversion Control**: Use parameter to cycle through inversions
- **Transpose Adjust**: Use encoder with shift to adjust transpose in larger steps

### 5.2 Accumulator

The Accumulator functions as a counter that can count up, down, or in complex patterns based on trigger inputs. The accumulator maintains a current value that can be used to modulate various parameters in real-time. Currently it is used ONLY to move tracks pitch every playhead pass.

**Features:**
- Counts trigger events and maintains a running value
- Supports multiple counting modes (Wrap, Pendulum, Random, Hold)
- Provides unipolar or bipolar output ranges
- Can be triggered by step events, gate events, or retrigger events

Accumulator is enabled by default. Push NOTE(F4) several times to get to Accumulator page. Push step and rotate encoder to change accumulator increments for the step. Small persistent dot will appear in top right corner of the square.

#### Modes and Settings

**Polarity Options:**
- **Unipolar (0):** Output range from 0 to maxValue
- **Bipolar (1):** Output range from minValue to maxValue (centered around 0)

**Activation Options:**
- **TRACK:** All steps will be transposed in following accumulator counter increase.
- **STAGE:** Only steps with accumulator triggers will be updated.

**Order (Counting Pattern):**
- **Wrap (0):** Wraps around when reaching bounds
- **Pendulum (1):** Bounces between min and max values
- **Random (2):** Jumps to random values
- **Hold (3):** Moves toward bounds and holds when reached

**Trigger Modes:**
- **Step (0):** Responds to step advancement
- **Gate (1):** Responds to gate events
- **Retrigger (2):** Responds to retrigger events (adds number of retriggers)

#### Shortcuts Related to Accumulator

- **SECOND PRESS of Shift-S3**: Navigate to ACCUM page
- **THIRD PRESS of Shift-S3**: Access detailed parameter controls (ACCST)
- **Shift+F4**: Resets accum to 0

### 5.3 Pulse Count and Gate Mode

#### Pulse Count
Pulse Count allows a single step in a sequence to play for multiple clock pulses before advancing to the next step.
- Values 0-7 represent 1-8 pulses.
- 0 = Normal behavior (1 pulse).
- 7 = Step holds for 8 pulses.

#### Gate Mode
Determines which pulses fire a gate signal when Pulse Count is active.
1. **All (0)**: Fires gates on every pulse (default).
2. **First (1)**: Single gate on first pulse only.
3. **Hold (2)**: One long gate for entire duration.
4. **FirstLast (3)**: Gates on first and last pulse only.

## 6. Routing & Output Rotation

### 6.1 Global Output Rotation (CV & Gate)

This feature allows you to dynamically rotate which tracks are assigned to physical CV and Gate outputs. It acts like a virtual 8-channel sequential switch.

**How it Works:**
The rotation applies to the physical output's lookup of its assigned track. Only tracks explicitly enabled for rotation will participate.

**Setup Instructions:**
1.  **Configure Track Layout:** Map Tracks to CV/Gate Outputs in Layout Page (Shift+S2 -> LAYOUT).
2.  **Create a Rotation Route:**
    *   **Target**: `CV Out Rot` or `Gate Out Rot`.
    *   **Source**: Modulation source (e.g., `CV In 1`).
    *   **Tracks**: Select the tracks to participate in the rotation pool.
    *   **Range**: 0-8 (unipolar) or -8 to +8 (bipolar).

**Example:**
Rotate 3 LFOs (Track 1, 2, 3) across 3 outputs (CV Out 1, 2, 3). Modulating the route shifts the assignment: T1->Out2, T2->Out3, T3->Out1.

### 6.2 VCA Next Shaper (VC)

The **VCA Next** (VC) shaper allows one route to amplitude-modulate another.

- **Concept**: Route A is the "Carrier". Route B is the "Modulator".
- **Setup**:
    1.  **Route A**: Source = LFO, Target = Filter, Shaper = **VC**.
    2.  **Route B**: Source = Envelope, Target = None (Dummy).
- **Result**: The LFO from Route A is multiplied by the Envelope from Route B.
- **Formula**: `Output = Center + (Input - Center) * NextRouteSource`.

### 6.3 Per-Track Reset

The **Reset** routing target allows external triggers to hard-reset a track.

- **Target**: `Reset`
- **Source**: Gate input, MIDI note, etc.
- **Action**: On a rising edge, the targeted track(s) immediately reset to their initial state (Step 0, Ramp 0, etc.), overriding internal loop counters.

## 7. New Curve Track Features

### Global Phase

Offsets the playback position of the curve sequence without changing step data.
- **Access**: Press F5 to cycle edit modes -> **Phase**.
- **Range**: 0.00 to 1.00.
- **Visual**: Dimmer vertical line shows phased position.

### Wavefolding

Advanced wavefolding accessible through the Phase button (cycle F5).
- **FOLD**: Folding amount (harmonics).
- **GAIN**: Input gain.
- **FILTER**: DJ-style Low/High pass.
- **XFADE**: Dry/Wet mix.

### Chaos

Chaotic modulation source added to the Curve track signal path (Pre-fold).
- **Access**: Cycle F5 to **CHAOS** page.
- **Algorithms**: **Latoocarfian** (Hyperchaotic, stepped) or **Lorenz** (Fluid, smooth).
    - **Toggle Algo**: Hold **Shift + F1** (AMT).
- **Parameters**:
    - **AMT**: Modulation depth.
    - **HZ**: Speed/Rate.
    - **P1/P2**: Algorithm-specific parameters (e.g., Rho/Beta for Lorenz).

### LFO Context Menu

Quickly fill sequences with LFO shapes.
- **Access**: Hold **STEP 5** button on CurveSequence page.
- **Options**: TRI, SINE, SAW, SQUA.

### Shortcuts
- **Shift+Encoder (Multi-step)**: Creates smooth transitions/gradients across selected steps for Shape, Min, and Max values.
