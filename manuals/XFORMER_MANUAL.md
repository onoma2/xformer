# XFORMER User Manual

## 0. WARININGS

The update is based on Phazerville fork of the original firmware. Launchpad functionalities for new addutiobs WERE NOT tested at all.

It is important to note that 99,5% of the code was written by musician little
helpers, that you can by for couple of dollars a month.

I have tested it on my unit and it seems that everything that I care about works. The firmware is provided AS IS, I'd appreciate feedback, but not sure if will be able to fix many things.
.



## 1. Introduction

This manual covers the XFORMER-specific features:

   - **Algo Track**: Generative sequencing based on TINRS Tuesday algorithms.
   - **Discrete Track**: Threshold-based sequencing mapping inputs to stages.
   - **Indexed Track**: Duration-based sequencing with independent step lengths.
   - **Note Track Enhancements**:
   	- Harmony Engine (Inspired by Instruo Harmonaig)
	- Accumulator (Inspired by Intellijel Metropolix)
	- Pulse Count and Gate Modes
   - **Curve Track Enhancements**:
		- Global Phase
		- Wavefolder
		- Chaos Engine
		- Advanced Playback Features
   - **Routing**:
        - CV/Gate Output Rotation
        - VCA Next Shaper
        - Per-Track Reset
		- Advanced Bias/Depth/Shaper System

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

- **Shift-S2**: Divisor, Clock Mult, Root, Octave
- **Shift-S3**: Menu with same controls as main UI

**Shortcuts:**
- **F1-F4**: Select parameter
- **F5**: Next Page
- **Shift+F5**: **RESEED** (Generate new random seed for current algorithm)
- **Context Menu (Long Press)**: INIT, RESEED
- **Quick Edit (Steps 9-14)**: **Copy/Paste** to 3 clipboard slots
- **Quick Edit (Step 15)**: **Randomize** all parameters

### 2.3 The Algorithms

(See full list in TUESDAY_MANUAL.md)
Includes 21 algorithms like **Stomper** (Acid bass), **Markov** (Chains), **ChipArp**, **Wobble** (LFO bass), **Techno**, **Drone**, etc.

### 2.4 Common Parameters

- **Algorithm (0-20)**: Selects the generation logic.
- **Flow/Ornament (0-16)**: Algorithm-specific macro controls.
- **Power (0-16)**: Note density and cooldown. 0 = Silence.
- **Loop (0-29)**: 0=Infinite, >0=Finite loop length.
- **Rotate**: Shifts start point of finite loops.
- **Scan**: Offsets the window into the infinite pattern buffer.
- **Skew**: Density bias across the loop (front/back loaded).

### 2.5 Jam Surface (Step Buttons)

| Step | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Top | Octave + | Transpose + | Root Note + | Div up (straight) | Div up (triplet) | Div /2 (faster) | Mask + | Loop +<br>Shift: Run momentary |
| Bottom | Octave - | Transpose - | Root Note - | Div down (straight) | Div down (triplet) | Div x2 (slower) | Mask - | Loop -<br>Shift: Reset |

Shift modifiers:
- Shift+Step 8: Run momentary (press = stop)
- Shift+Step 16: Reset (restart sequence from start)

Behavior notes:
- Top row moves "up" (faster/greater), bottom row moves "down" (slower/less); for divisors, smaller = faster.
- Divisor up/down only walks known divisors for straight or triplet types.

## 3. Discrete Track

### 3.1 Overview
Discrete maps an input signal (internal ramp or external CV) to 32 discrete output "Stages". Each stage triggers when the input crosses a specific voltage threshold.

**Key Features:**
- **Thresholds**: Voltage levels (-100 to +100) that trigger stages.
- **Directions**: **Rise** (↑), **Fall** (↓), or **Off**. A stage only triggers if crossed in the specified direction.
- **Notes**: Output note/voltage for each stage.

### 3.2 Input Sources
- **Internal Saw/Tri**: LFO-like ramps (synced to Divisor × Clock Mult).
- **External**: Uses routed CV Input. Great for wave-shaping or envelope following.

### 3.3 Advanced Features
- **Scanner (Routing Target)**: Route CV to `DMap Scan`. As CV changes, it toggles the direction of stages it passes over.
- **Generator (Long Press GEN)**:
    - **Types**: Random, Linear, Log, Exp.
    - **Targets**: Thresholds (THR), Notes (NOTE2/NOTE5), Toggles (TOG).
- **Voicing Quick Edit (hold + encoder)**:
    - **Step 12**: Piano voicings
    - **Step 13**: Guitar voicings
- **Transform Menu (Page + Step 14)**:
    - **FLIP**: Toggle stage directions
    - **T-REV**: Reverse threshold order
    - **T-INV**: Invert thresholds around 0
    - **N-MIRR**: Mirror note indices (copy first half into second half, reversed)
    - **N-REV**: Reverse note index order
- **Fret-Style Initialization**: When clearing or initializing the sequence, stages are distributed using an interleaved "fretboard" pattern across the 4 pages of 8 steps each, creating an even distribution of threshold values from -100 to +100 across all 32 stages.

### 3.4 Fret-Style Initialization Pattern

The Discrete track uses a unique "fretboard" or interleaved initialization pattern that distributes threshold values across all 32 stages in a round-robin fashion:

**Initialization Algorithm:**
- The 32 stages are organized in 4 pages of 8 steps each
- Threshold values are distributed from -100 to +100 using round-robin interleaving
- Each step position (0-7) gets evenly spaced threshold values across all 4 pages
- This creates a "fretboard" pattern where similar threshold values align vertically across pages

**Technical Details:**
- Page calculation: `page = (i / 8) + 1` (where i is stage index 0-31)
- Button calculation: `button = i % 8` (where i is stage index 0-31)
- Global index: `global_index = button * 4 + (page - 1)`
- Threshold value: `threshold = -100 + (global_index * 200 / 31)`

This pattern ensures that when you're using multiple pages of the Discrete track, similar threshold values are aligned across the pages, making it easier to create coordinated mappings across different sections of your sequence.

### 3.5 All-Stages Selection Behavior

When all 32 stages are selected (selection mask includes all stages):

**Threshold Editing:**
- **Shift + Encoder**: Rotate all 32 threshold values by 1 position
- **Encoder only**: Rotate all 32 threshold values by 8 positions
- Values cyclically shift to the left/right (wrapping around)

**Note Editing:**
- **Encoder**: Rotate all 32 note indices by 1 position (no shift distinction)
- Values cyclically shift to the left/right (wrapping around)

This behavior allows for bulk manipulation of all stage parameters simultaneously, enabling complex pattern transformations across the entire Discrete sequence.

## 4. Indexed Track

### 4.1 Overview
Indexed Track is a duration-based sequencer. Unlike standard steps that are locked to a grid, each Indexed step has its own **Duration** in clock ticks (0-65535).

**Use Cases**: Polyrhythms, unquantized timing, complex envelopes.

### 4.2 Step Parameters
- **Duration**: Length of step in ticks (0-65535).
- **Gate**: Gate length in ticks (OFF or 4..32767). FULL equals step duration.
- **Note**: Output voltage index (-63 to +63).

### 4.3 Math Operations (F5)
Apply batch operations to selected steps:
- **Arithmetic**: Add, Sub, Mul, Div, Set.
- **Ramp**: Linear interpolation between values.
- **Quant**: Quantize values to nearest multiple.
- **Rand/Jitter**: Randomization.

### 4.4 Quick Edit (Page+Steps 8-15)
- **Step 8**: Split (requires step selection)
- **Step 9**: Merge with next (first selected step only)
- **Step 10**: Swap (hold + encoder to choose offset, release to apply)
- **Step 11**: Run Mode (quick edit list)
- **Step 12**: Piano Voicings (hold + encoder)
- **Step 13**: Guitar Voicings (hold + encoder)

### 4.5 Macro Shortcuts (Page+Steps 4, 5, 6, 14)
Macros provide powerful generative and transformative operations on sequences. All macros operate on:
- **Selected steps** (if any are selected)
- **Full active length** (if no selection)

**Page+Step 4 - Rhythm Generators** (YELLOW LED):
- **3/9**: Cycles even subdivisions with 3 steps, then 9 steps (fills 768 ticks)
- **5/20**: Cycles even subdivisions with 5 steps, then 20 steps (fills 768 ticks)
- **7/28**: Cycles even subdivisions with 7 steps, then 28 steps (fills 768 ticks)

## 5. Monitor Scope

The Monitor page includes a full-screen scope view for the selected track.

**Access:**
- Press **Page + Step 7** to open Monitor.
- Press **Page + Step 7** again to toggle Scope.

**Scope View:**
- Full-screen (no header/footer).
- **CV trace** shows the selected track output.
- **Gate band** appears only while gate is high (3px tall, near the bottom).
- CV scale is fixed to **±6V**.
- **Top right**: current CV value (low intensity, fixed width to avoid jitter).
- **Top left**: secondary track label (low intensity).

**Secondary Trace (optional):**
- **Encoder** cycles through **OFF + 7 other tracks** (never the selected track).
- When enabled, a second CV trace is drawn at low intensity.
- Label shows **OFF** or **Tn** for the secondary track.
- **3-5/5-7**: Cycles grouped patterns (3+5) and (5+7), each fit to 768 ticks
- **M-TALA**: Cycles Khand (5+7), Tihai (2-1-2 x3), Dhamar (5-2-3-4)

**Page+Step 5 - Waveforms** (YELLOW LED):
- **TRI**: Single triangle dip (shorter in the middle, longer at the ends)
- **2TRI**: Two triangle dips across the range
- **3TRI**: Three triangle dips across the range
- **2SAW**: Two saw ramps (short at the start of each segment)
- **3SAW**: Three saw ramps (short at the start of each segment)

**Page+Step 6 - Melodic Generators** (YELLOW LED):
- **SCALE**: Scale fill generator
- **ARP**: Arpeggio pattern generator
- **CHORD**: Chord progression generator
- **MODAL**: Modal melody generator
- **M-MEL**: Random melody generator

**Page+Step 14 - Duration & Transform** (YELLOW LED):
- **D-LOG**: Duration logarithmic curve
- **D-EXP**: Duration exponential curve
- **D-TRI**: Duration triangle curve
- **REV**: Reverse step order
- **MIRR**: Mirror steps around midpoint

## 5. Note Track Enhancements

### 5.1 Harmony Engine

The Harmony Engine creates complex harmonic relationships between tracks by generating chords based on scale degrees and applying various harmonic transformations. It works by taking a root note from a "Master" track and generating harmonized chord tones that can be assigned to "Follower" tracks.

The system uses a combination of:

- **Scale intervals:** Based on the selected **Harmony Mode** (Ionian, Dorian, Phrygian, etc.)
- **Chord qualities:** Determined by the mode (Major7, Minor7, Dominant7, HalfDim7)
- **Inversions:** Root, 1st, 2nd, and 3rd inversions (Sequence-level or Per-step)
- **Voicings:** Close, Drop2, Drop3, and Spread voicings (Sequence-level or Per-step)
- **Transpose:** Global transposition of all chord tones (CH-TRNSP)

#### Modes and Settings

**Harmony Modes:** (UI Label: **MODE**)

- **Ionian (0):** Major scale - I∆7, ii-7, iii-7, IV∆7, V7, vi-7, viiø
- **Dorian (1):** Dorian mode - i-7, ii-7, ♭III∆7, IV7, v-7, viø, ♭VII∆7
- **Phrygian (2):** Phrygian mode - i-7, ♭II∆7, ♭III7, iv-7, vø, ♭VI∆7, ♭vii-7
- **Lydian (3):** Lydian mode - I∆7, II7, iii-7, #ivø, V∆7, vi-7, vii-7
- **Mixolydian (4):** Mixolydian mode - I7, ii-7, iiiø, IV∆7, v-7, vi-7, ♭VII∆7
- **Aeolian (5):** Natural minor - i-7, iiø, ♭III∆7, iv-7, v-7, ♭VI∆7, ♭VII7
- **Locrian (6):** Diminished scale - iø, ♭II∆7, ♭iii-7, iv-7, ♭V∆7, ♭VI7, ♭vii-7

**Voicing Options:**

- **Close (0):** All chord tones in closest possible position within an octave
- **Drop2 (1):** Second highest note dropped down an octave
- **Drop3 (2):** Third highest note dropped down an octave
- **Spread (3):** Open voicing - Third and fifth above root, seventh an octave higher

**Global Transpose:**
- **CH-TRNSP:** ±24 semitones. Transposes the entire chord structure after harmonization.

#### Harmony Roles (Master/Follower)

**Setting up Master/Follower Relationships:**
1. Configure ANY note track as the "Master" track (typically playing scale degrees)
2. Enable Harmony Engine on the Master track with desired settings (Mode, Inversion, etc.)
3. Set Following type on ANOTHER note track
4. Each Master track can generate up to 4 harmonized outputs (root, third, fifth, seventh)
5. By default Performer settings tracks designated as Followers will update their CV output on Gate events. So enable gates where you want to hear a chord.

**Master Track Configuration:**
- The root note of the chord is determined by the Master track's current note
- The Harmony Engine converts these scale degrees to chord tones based on the selected Mode
- **Per-step Overrides**:
    - **Inversion**: Override sequence inversion per step. Access via **F4** (Length) cycle: `Length -> Range -> Prob -> INVERSION -> VOICING`.
    - **Voicing**: Override sequence voicing per step. Access via **F4** cycle.
    - Abbreviations:
        - **Inversion**: S (Seq), R (Root), 1 (1st), 2 (2nd), 3 (3rd)
        - **Voicing**: S (Seq), C (Close), 2 (Drop2), 3 (Drop3), W (Wide/Spread)

**Follower Track Configuration:**
- Follower tracks receive the harmonized chord tones from the Master
- Each Follower can be assigned to a specific chord tone (root, third, fifth, or seventh)
- **Per-step Role Override**:
    - You can override the follower role (e.g., change from 3rd to 5th) on a per-step basis.
    - Access via **F3** (Note) cycle: `Note -> Range -> Prob -> Accum -> HARMONY ROLE`.
    - Options: SEQ (Default), ROOT, 3RD, 5TH, 7TH, OFF.
    - This allows for arpeggiated chord lines within a single track.
- **Note**: When you switch a track to a Follower role, it automatically copies the Harmony Mode from the Master track as a starting point.

#### Setting up 2 Chord Progressions in 2 Patterns

This example demonstrates how to create two distinct chord progressions that switch when you change patterns.

**1. Track Configuration:**
   - **Track 1 (Master):** Set to Note Track. This will play the root notes (the progression).
   - **Track 2 (Follower):** Set to Note Track.
   - **Track 3 (Follower):** Set to Note Track.

**2. Pattern 1 Setup (Progression A):**
   - Select **Track 1**, **Pattern 1**.
   - Go to Harmony Page (Shift+S3 -> Harmony).
   - Set **Mode** to **Ionian (0)**.
   - Enter root notes for your first progression (e.g., C, F, G, C) on steps 1, 5, 9, 13.
   - Select **Track 2**. Set Harmony Role to **3rd**.
   - Select **Track 3**. Set Harmony Role to **5th**.
   - Enable gates on Tracks 2 & 3 where you want the chords to play (steps 1, 5, 9, 13).

**3. Pattern 2 Setup (Progression B):**
   - Switch to **Pattern 2**.
   - Select **Track 1**.
   - Go to Harmony Page. Set **Mode** to **Aeolian (5)** (Natural Minor).
   - Enter root notes for your second progression (e.g., A, F, G, E).
   - Select **Track 2**. Set Harmony Role to **3rd**.
   - Select **Track 3**. Set Harmony Role to **7th** (for a jazzier feel).
   - Enable gates on Tracks 2 & 3.

**4. Performance:**
   - Switch between Pattern 1 and Pattern 2.
   - Track 1 changes the root notes *and* the Harmony Mode (Major vs Minor).
   - Tracks 2 & 3 follow the new roots and scale intervals automatically.
   - Track 3 changes its interval from 5th to 7th automatically because the Harmony Role is stored per-pattern.

#### Shortcuts Related to Harmony

- **SECOND PRESS of Shift-S3**: Harmony Engine Page
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

**Condition Handling:**
- **Trigger Mode: Step** respects step **Conditions** only.
- **Trigger Mode: Step** ignores gate on/off and gate probability.
- **Trigger Modes: Gate/Retrigger** follow gate logic (so Conditions and gate probability apply).

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

### 5.4 Re:Rene Mode (Note Track)

Re:Rene turns the Note sequence into a cartesian 8x8 grid of locations. X and Y advance independently using two divisors, and the active loop range acts like an ACCESS window that allows or denies locations.

**Setup:**
- Open the Note Sequence list.
- **Mode**: Linear or Re:Rene.
- **Div X**: X-axis clock divisor (uses the existing Divisor).
- **Div Y**: Y-axis clock divisor (new).

**Behavior (Free mode only):**
- The grid is always 8x8 (64 locations).
- X steps on **Div X**, Y steps on **Div Y**.
- If a location is denied (outside First/Last), the engine **seeks forward** within the same row/column until it finds the next allowed location.
- Timing does not pause on denied locations; they are skipped instantly (fast-forward seek).
- Gate timing uses the faster of the two clocks: `min(Div X, Div Y)`.

**ACCESS window:**
- **First Step/Last Step** define which locations are allowed.
- This window does not resize the grid; it only allows/denies locations.

## 6. Curve Track Enhancements

### 6.1 Signal Processor

The signal path has been expanded to include Chaos, Wavefolding, and Filtering.

#### 6.1.1 Signal Flow
`Step Shape` -> `Chaos` -> `Wavefolder` -> `Filter` -> `Crossfade` -> `Limiter` -> `Output`

#### 6.1.2 Wavefolder
- **Access**: Press **F5** to cycle to **WAVEFOLDER**.
- **FOLD**: Controls the amount of wavefolding (sine-based folding). Range: 0.00-1.00.
- **GAIN**: Input gain before the folder. Range: 0.00-2.00 (0% to 200%).

#### 6.1.3 DJ Filter
- **FILTER**: One-knob filter control.
  - **Center (0.00)**: Filter off.
  - **Negative**: Low Pass Filter (LPF).
  - **Positive**: High Pass Filter (HPF).

#### 6.1.4 Crossfade (XFADE)
- **XFADE**: Blends between the dry signal and the processed (folded/filtered) signal.

### 6.2 Chaos Engine

A generative engine inserted *before* the wavefolder.

- **Access**: Press **F5** to cycle to **CHAOS**.
- **Controls**:
  - **AMT**: Modulation depth (0-100%).
  - **HZ**: Evolution speed (0.01Hz-50Hz).
  - **P1**: Algorithm-specific parameter 1 (0-100, default: 50).
  - **P2**: Algorithm-specific parameter 2 (0-100, default: 50).

#### 6.2.1 Chaos Range Offset
- **Function**: Shifts the centerpoint of chaos wiggling within the voltage range.
- **Access**: Hold **Shift + F1** to cycle through range modes.
- **Modes**:
  - **Mid**: Wiggles around centerpoint (0V for bipolar ranges, +2.5V for unipolar 5V).
  - **Below**: Wiggles around bottom quarter (e.g., -2.5V for bipolar 5V, peaks at 0V, bottoms at -5V).
  - **Above**: Wiggles around top quarter (e.g., +2.5V for bipolar 5V, dips to 0V, peaks at +5V).
- **Use Case**: Asymmetric chaos modulation - chaos affects only positive or negative swings when mixed with bipolar shapes.

#### 6.2.2 Chaos Algorithms
- **Toggle**: Hold **Shift + F2** to switch between algorithms.
- **Latoocarfian**: Stepped/Sample&Hold character, chaotic jumps.
- **Lorenz**: Smooth/Attractor character, flowing modulation.

### 6.3 Advanced Playback

#### 6.3.1 Global Phase
- **Function**: Offsets the playback position of the sequence (0-100%).
- **Access**: Press **F5** to cycle to **PHASE**.
- **Use Case**: Phase-shifting LFOs, canon effects, fine-tuning timing.

#### 6.3.2 True Reverse Playback
The engine now supports "True Reverse" playback for shapes.
- **Backward Mode**: Steps play in reverse order (4, 3, 2...), AND the internal shape plays backwards (100% -> 0%).
- **Pendulum/PingPong**: Shapes play forward on the "up" phase and backward on the "down" phase, creating perfectly smooth loops.

#### 6.3.3 Min/Max Inversion
You can now create inverted signal windows by setting **Min > Max**.
- **Normal**: Min=0, Max=255 (Signal goes UP).
- **Inverted**: Min=255, Max=0 (Signal goes DOWN).
- **Step**: The step shape scales from "Start" (Min) to "End" (Max).

#### 6.3.4 Free vs Aligned Play Modes
The Curve track now supports different play modes:

**Free Mode**:
- Uses phase accumulator system for smooth FM modulation
- Allows for speed modulation via Curve Rate parameter
- Provides continuous, non-grid-locked timing
- Best for audio-rate FM and smooth modulation

**Aligned Mode**:
- Grid-locked to sequencer clock
- Steps advance at divisor boundaries
- Quantized timing perfect for rhythmic LFOs
- No speed modulation possible (Curve Rate has no effect)

### 6.4 Curve Studio Workflow

Curve Studio introduces powerful context menus for populating sequences.

#### 6.4.1 LFO Menu (Step 6)
Populate steps with single-cycle waveforms. Defaults to active loop range or selection.

- **Access**: Press **Page + Step 6** (Button 6).
- **Options**:
  - **TRI**: Triangle (1 cycle/step).
  - **SINE**: Sine / Bell (1 cycle/step).
  - **SAW**: Sawtooth / Ramp (1 cycle/step).
  - **SQUA**: Square / Pulse (1 cycle/step).
  - **MM-RND**: Randomize Min/Max values for each step (supports inversion).

#### 6.4.2 Macro Shape Menu (Step 5)
"Draw" complex shapes that span multiple steps. Automatically handles selection logic (Single step -> to end of loop).

- **Access**: Press **Page + Step 5** (Button 5).
- **Options**:
  - **MM-INIT**: **Min/Max Reset**. Resets the range to default (Min=0, Max=255) without changing shapes.
  - **M-FM**: **Chirp / FM**. A triangle wave that accelerates in frequency over the range.
  - **M-DAMP**: **Damped Oscillation** (Decaying Sine, 4 cycles).
  - **M-BOUNCE**: **Bouncing Ball** physics (Decaying absolute sines).
  - **M-RSTR**: **Rasterize**. Stretches the shape of the *first step* across the entire range.

#### 6.4.3 Transform Menu (Step 7)
Manipulate existing sequence data.

- **Access**: Press **Page + Step 7** (Button 7).
- **Options**:
  - **T-INV**: **Invert**. Swaps Min and Max values (Flips slope direction).
  - **T-REV**: **Reverse**. Reverses step order and internal shape direction.
  - **T-MIRR**: **Mirror**. Reflects voltages across the centerline.
  - **T-ALGN**: **Align**. Ensures continuity by setting `Min = previous Max`.
  - **T-WALK**: **Smooth Walk**. Generates a continuous random path ("Drunken Sine").

#### 6.4.4 Gate Presets Menu (Step 15)
Quickly assign dynamic gate behaviors based on the curve slope or levels.

- **Access**: Press **Page + Step 15** (Button 15).
- **Options**:
  - **ZC+**: **Zero Cross**. Trigger at every zero crossing (rising + falling).
  - **EOC/EOR**: **Peaks/Troughs**. Trigger at every local maximum and minimum.
  - **RISING**: Gate HIGH while voltage is increasing.
  - **FALLING**: Gate HIGH while voltage is decreasing.
  - **>50%**: **Comparator**. Gate HIGH when voltage is in the top half of the range.

### 6.5 Shortcuts & Gestures

#### 6.5.1 Double-Click Toggles
- **Note Track**: Double-click a step button to toggle the Gate (on any non-gate layer).
- **Curve Track**: Double-click a step button to toggle the **Peak+Trough** gate preset.

#### 6.5.2 Multi-Step Gradient Editing
- **Action**: Select multiple steps, hold **Shift** and turn the Encoder on **MIN** or **MAX**.
- **Result**: Creates a linear ramp of values from the first selected step to the last.

#### 6.5.3 Shaper Gates
Curve tracks now support advanced gate generation based on curve analysis:

**Available Shaper Gate Types**:
- **Zero Cross (ZC+)**: Trigger at every zero crossing (rising + falling)
- **End of Cycle/End of Rise (EOC/EOR)**: Trigger at every local maximum and minimum (peaks/troughs)
- **Rising**: Gate HIGH while voltage is increasing
- **Falling**: Gate HIGH while voltage is decreasing
- **>50%**: Comparator - gate HIGH when voltage is in the top half of the range

**Advanced Gate Modes**:
The Gate Probability parameter in Curve tracks has been repurposed as an Advanced Gate Mode selector:
- Range: 0 to 7 with different behaviors:
  - 0: OFF (no gate output)
  - 1: RISE (gate triggers on rising slope)
  - 2: FALL (gate triggers on falling slope)
  - 3: MOVE (gate triggers on any slope movement)
  - 4: > 25% (gate HIGH when voltage is above 25% of range)
  - 5: > 50% (gate HIGH when voltage is above 50% of range)
  - 6: > 75% (gate HIGH when voltage is above 75% of range)
  - 7: WIND (Window mode - gate HIGH when voltage is within a specific window)

#### 6.5.4 Curve Rate Routing
The Curve Rate parameter can be modulated via CV routing for dynamic FM effects:

**Routing Range:** 0 to 400
- **0** → 0.0x (freeze)
- **100** → 1.0x (normal speed, default)
- **200** → 2.0x (double speed)
- **400** → 4.0x (quadruple speed)

**Setup**:
- Target: Curve Rate (available in routing menu)
- Source: CV Input, MIDI CC, or other modulation source
- Min/Max: Set range for modulation depth (e.g., 100-200 for 1x to 2x speed)
- Track Selection: Apply to specific curve tracks only
- Bias/Depth: Use per-track bias/depth for individual control
- Allows for FM modulation in Free mode

## 7. Routing & Output Rotation

### 7.1 Global Output Rotation (CV & Gate)

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

### 7.2 VCA Next Shaper (VC)

The **VCA Next** (VC) shaper allows one route to amplitude-modulate another.

- **Concept**: Route A is the "Carrier". Route B is the "Modulator".
- **Setup**:
    1.  **Route A**: Source = LFO, Target = Filter, Shaper = **VC**.
    2.  **Route B**: Source = Envelope, Target = None (Dummy).
- **Result**: The LFO from Route A is multiplied by the Envelope from Route B.
- **Formula**: `Output = Center + (Input - Center) * NextRouteSource`.

### 7.3 Per-Track Reset

The **Reset** routing target allows external triggers to hard-reset a track.

- **Target**: `Reset`
- **Source**: Gate input, MIDI note, etc.
- **Action**: On a rising edge, the targeted track(s) immediately reset to their initial state (Step 0, Ramp 0, etc.), overriding internal loop counters.

### 7.4 Advanced Bias/Depth/Shaper System

The routing system provides sophisticated per-track modulation capabilities using Bias, Depth, and Shaper parameters.

#### 7.4.1 Signal Flow Overview

```
Raw Source Value
      ↓
[Normalization] → Converts to 0-100% range
      ↓
[Bias + (Raw × Depth/100)] → Per-track adjustment
      ↓
[Shaper (if active)] → Non-linear transformation
      ↓
[Target Parameter] → Final modulated value
```

#### 7.4.2 Per-Track Parameters

Each track has independent bias, depth, and shaper settings:

**Bias (-100% to +100%)**:
- Adds offset to normalized source value
- Positive bias increases source signal
- Negative bias decreases source signal

**Depth (0% to 100%)**:
- Controls amount of source modulation applied
- 0% = no modulation (use target base value)
- 100% = full source range applied to target range

**Shapers**:
- Apply non-linear transformations to source
- Each shaper modifies how source affects target
- Available shapers: None, Crease, Location, Envelope, Triangle Fold, Frequency Follower, Activity, Progressive Divider, VcaNext

#### 7.4.3 Shaper Functions

**None**: Direct linear mapping
**Crease**: Creates "crease" or fold in the middle of the range
**Location**: Shifts where the modulation is most sensitive
**Envelope**: Applies envelope-like transformation
**Triangle Fold**: Folds the range like a triangle wave
**Frequency Follower**: Responds to rate of change in source
**Activity**: Responds to activity and movement in source
**Progressive Divider**: Applies different divisions to the source
**VcaNext (VC)**: Performs amplitude modulation using the next route's raw source
