# T9type Track Tutorial for Performer

## Introduction

This tutorial is designed for intermediate teletype users who want to learn how to use the T9type Track functionality within the Performer firmware. The T9type Track in Performer provides a powerful way to integrate teletype scripts with your sequencer, allowing for complex musical patterns and interactions.

## Prerequisites

Before starting this tutorial, you should have:
- Basic understanding of teletype operations and syntax
- Familiarity with the Performer sequencer interface
- Understanding of CV/Gate concepts in modular synthesis

---

# Part 1: Overview

## Overview of Teletype in Performer

The Teletype Track in Performer provides:

- NO GRID NO USB KEYBOARD SUPPORT,
- 4 Scripts + Metro + 4 TT-Patterns.
- 4 CV outputs (CV 1-4) freely mapped to Performer's CV outputs
- 4 Gate outputs (TR A-D) freely mapped to Performer's gate outputs
- 4 trigger inputs (TI-TR1-4) to trigger scripts
- 2 CV inputs (TI-IN and TI-PARAM) from Performer's CV inputs, X/Y/Z/T variables mapped from Performers inputs.
- Integration with Performer's clock system
- Pattern and variable storage
- Two slots (P1/P2) for S4 + Metro + I/O mapping + TT patterns
  - Slot = Performer pattern index % 2
  - Script/Pattern View shows the active slot label before the script name (or top right in Pattern View)
- Performer-only CV modulation ops: per-CV envelopes (`E.*`) and per-CV LFOs (`LFO.*`), G.* modulator

---


# Part 2: Keyboard Shortcuts

## Script View Shortcuts

### Function Keys (F1-F5)
- **F1**: Select S0
- **F2**: Select S1
- **F3**: Select S2
- **F4**: Toggle S3 / Metro
- **F5**: Toggle Pattern View - Live View - Script View
- **Shift + F1-F4**: Fire script S0-S3 (TI-TR1..4)

### Step Keys (Steps 1-16) - Default Mode
- **Step 1**: 1 → A → B (cycles)
- **Step 2**: 2 → C → D (cycles)
- **Step 3**: 3 → E → F (cycles)
- **Step 4**: 4 → G → H (cycles)
- **Step 5**: 5 → I → J (cycles)
- **Step 6**: 6 → K → L (cycles)
- **Step 7**: 7 → M → N (cycles)
- **Step 8**: 8 → O → P (cycles)
- **Step 9**: 9 → Q → R (cycles)
- **Step 10**: 0 → S → T (cycles)
- **Step 11**: . → U → V (cycles)
- **Step 12**: : → W → X (cycles)
- **Step 13**: ; → Y → Z (cycles)
- **Step 14**: Backspace (delete character before cursor)
- **Step 15**: Insert space
- **Step 16**: Commit line and advance

### Shift + Step Keys (Steps 1-16) - Symbols & Commands
- **Shift + Step 1**: + → - (cycles)
- **Shift + Step 2**: * → / (cycles)
- **Shift + Step 3**: = → ! (cycles)
- **Shift + Step 4**: < → > (cycles)
- **Shift + Step 5**: % → ^ (cycles)
- **Shift + Step 6**: & → | (cycles)
- **Shift + Step 7**: $ → @ (cycles)
- **Shift + Step 8**: ? → ; (cycles)
- **Shift + Step 9**: CV → CV.SLEW (cycles)
- **Shift + Step 10**: TR. → TR.TIME (cycles)
- **Shift + Step 11**: PARAM → SCL (cycles)
- **Shift + Step 12**: P.NEXT → P.HERE (cycles)
- **Shift + Step 13**: ELIF → OTHER (cycles)
- **Shift + Step 14**: RRAND → RND.P (cycles)
- **Shift + Step 15**: DRUNK → WRAP (cycles)
- **Shift + Step 16**: M.ACT → M.RESET (cycles)

### Page + Step Keys (Steps 9-13) - Clipboard & Edit
- **Page + Step 9**: Copy current line
- **Page + Step 10**: Paste line
- **Page + Step 11**: Duplicate line
- **Page + Step 12**: Comment/uncomment line
- **Page + Step 13**: Delete line

### Navigation and Editing
- **Left Arrow**: Backspace (delete character to the left)
- **Shift + Left Arrow**: Move cursor left
- **Right Arrow**: Insert space
- **Shift + Right Arrow**: Move cursor right
- **Encoder Press**: Load selected line into edit buffer
- **Shift + Encoder Press**: Commit line (save changes)
- **Encoder Turn**: Move cursor left/right (normal) or select line (with Shift)
- **Page + Left/Right**: History prev/next (live/edit)

## Pattern View Shortcuts

### Function Keys
- **F1-F4**: Select pattern 0-3
- **F5**: Toggle back to Script View

### Step Keys (Steps 1-16)
- **Steps 1-9**: Insert digits 1-9
- **Step 10**: Insert digit 0
- **Step 14**: Backspace digit
- **Step 16**: Commit edited value

### Page + Step Keys (Steps 4-6)
- **Page + Step 4**: Set pattern length to current row
- **Page + Step 5**: Set pattern start to current row
- **Page + Step 6**: Set pattern end to current row

### Navigation and Editing
- **Left Arrow**: Backspace digit; **Shift + Left** deletes row
- **Right Arrow**: Insert row; **Shift + Right** toggles turtle display
- **Encoder Turn**: Move up/down through pattern rows
- **Encoder Press**: Negate current value
- **Shift + Encoder Press**: Commit edited value

---

# Part 3: I/O Setup

## Teletype Track Setup (I/O Mapping)

If your Teletype track is new, set up its I/O first so scripts can see the correct inputs/outputs.

1. **Select the Teletype track**
   Press the Track button for the track slot where Teletype is loaded.

2. **Open the Track page (I/O list)**
   Press **Page + Track** to open the track list model for the selected track.

3. **Map Teletype inputs**
   - **TI-TR1..TI-TR4**: assign trigger sources (CV inputs or gate outputs).
   - **TI-IN**: select the CV source for `IN`.
   - **TI-PARAM**: select the CV source for `PARAM`.
   - **Note**: CV inputs here are direct hardware inputs (no routing needed).

4. **Map Teletype outputs**
   - **TO-TRA..TO-TRD**: map Teletype trigger outputs A-D to Performer gate outputs.
   - **TO-CV1..TO-CV4**: map Teletype CV outputs 1-4 to Performer CV outputs.

   - **Tip**: If a TO entry shows `!`, it is mapped to a physical output not owned by this track in **Layout**.
   - **Sync**: **F5 (SYNC OUTS)** on the Track page fills TO-CV/TO-TR from the Layout-owned outputs (does not overwrite unmatched slots).

5. **(Optional) Adjust ranges/offsets**
   - **CV1 RNG/OFF .. CV4 RNG/OFF**: set per-output voltage range and offset.

6. **Trigger input presets**
   - **F1 (TI PRESET)** cycles TI-TR presets: CV1-4, G1-4, G5-8, L-G1-4, L-G5-8, None.

After this, proceed to Script View and start editing scripts.

## Layout Prompt for Teletype Tracks

When you commit a track mode change to Teletype from the Layout page, a prompt asks:
`ASSIGN OUTS i-i+3?`
Answering **Yes** assigns both gate and CV outputs in that range to the Teletype track (no wrap beyond output 8).

Track LED note: when a track is in Teletype mode, its TR activity captures the track LEDs regardless of CV/gate output mapping. TR1 uses the assigned track LED; TR2-TR4 use the next track LEDs (no wrap).

## Script View - I/O Grid

The Script View features a visualization grid in the top-right corner, providing real-time feedback on Teletype I/O states.

**Grid Layout (5 Sections):**

### BUS Block (Left Side)
- Four CV bus meters (A-D) showing current bus voltages.
- Top row: BUS 1-2
- Bottom row: BUS 3-4

### TI (Trigger Inputs) - Top Row
- Displays the state of `TI-TR1` through `TI-TR4`.
- **Dim (Low):** Input is unassigned (None).
- **Medium:** Input is assigned but inactive.
- **Bright:** Input is assigned and receiving a high signal (Gate High).
- Filled block indicates TI-TR assigned; brighter when active.

### TO (Trigger Outputs) - Middle Row
- Displays the state of `TO-TRA` through `TO-TRD` (mapped to Performer Gate outputs).
- **Solid block:** Destination owned by this track in Layout (bright when active).
- **Outline block:** Destination owned by a different track (Layout mismatch).
- **Dim (Low):** The target Gate Output is assigned to a *different* track in the global Layout.
- **Medium:** Output is owned by this track but inactive.
- **Bright:** Output is owned and pulsing (Active).

### CV (CV Outputs) - Bottom Section (Double Height)
- Displays the state of `TO-CV1` through `TO-CV4` (mapped to Performer CV outputs).
- **Visualization:** A vertical bar graph growing from the center (Bipolar).
- **Center Line:** Represents 0V.
- **Upward Bar:** Positive voltage (> 0V).
- **Downward Bar:** Negative voltage (< 0V).
- **Bright bar:** Destination owned by this track in Layout.
- **Dim bar:** Destination owned by a different track (Layout mismatch).
- If the target CV Output is assigned to another track, the bar is **hidden** (only the dim container frame is visible).

### IN and PARAM Values (Right Side)
- Top: IN value
- Bottom: PARAM value
- Brighter when assigned; dim when unassigned.

---


## Teletype Panic (Page + Track 6)

Need to instantly silence all Teletype activity? Use the global panic:

**Page + Track 6** → `TT PANIC`

This stops **all Teletype tracks** by:
- disabling metro (M.ACT = 0)
- clearing pending DEL commands
- clearing TR pulses and forcing gates low
- zeroing all Teletype CV outputs
- stopping E.*, LFO.*, and G.* modulation
- clearing Geode routing and phase

## Default Scripts on New T9type Track

When a Teletype track has no scripts, it seeds two defaults:

- **S0**: `M.ACT 1` (enables metro)
- **M (Metro)**: `CV 1 N 48 ; TR.P 1` (C3 on CV1 + trigger pulse)

You can overwrite these immediately in Script View.

---

# Part 5: Performer-only Ops Reference

## LFO Modulation (LFO.*)

Per-CV low-frequency oscillator with waveform, amplitude, fold, and offset control.

**Quick Start (CV 1):**

```
LFO.R 1 2000   # 2s cycle
LFO.W 1 15     # triangle → saw
LFO.A 1 80     # amplitude
LFO.F 1 0      # no fold
LFO.O 1 8192   # offset/bias
LFO.S 1 1      # start
```

**Defaults:**

`LFO.R`=1000 ms, `LFO.W`=0, `LFO.A`=100, `LFO.F`=0, `LFO.O`=8192, `LFO.S`=0.

## Envelope Modulation (E.*)

Per-CV attack/decay envelope with target/offset.

**Quick Start (CV 1):**

```
E.O 1 0        # offset/baseline
E 1 12000      # target (raw 0-16383)
E.A 1 10       # attack ms
E.D 1 80       # decay ms
E.T 1          # trigger envelope
```

**Defaults:**

`E`=16383, `E.A`=50 ms, `E.D`=300 ms, `E.O`=0, `E.L`=1.

**Optional triggers and looping:**

```
E.R 1 1       # pulse TR1 at end of rise
E.C 1 2       # pulse TR2 at end of cycle
E.L 1 4       # loop 4 cycles (0 = infinite)
```

## Geode Modulation (G.*)

Polymetric rhythm generator with per-voice timing control.

**Quick Start (CV 1):**

```
G.R 1 0       # mix to CV1
G.V 1 7 8     # voice 1: 7-div, 8 repeats (immediate trigger)
```

**Defaults:**

`G.TIME`=8192, `G.TONE`=8192, `G.RAMP`=8192, `G.CURV`=8192, `G.RUN`=8192, `G.MODE`=0, `G.O`=0, `G.BAR`=4, tune ratios 1/1..6/1.

When transport is stopped, Geode free-runs using the last known bar duration (defaults to 120 BPM if none).

**Aliases (short ops):**

`G.T`=G.TIME, `G.I`=G.TONE, `G.RA`=G.RAMP, `G.C`=G.CURV, `G.N`=G.RUN, `G.M`=G.MODE, `G.B`=G.BAR, `G.L`=G.VAL.

**Compound ops:**

`G.S t i n` sets TIME, TONE, RUN (no trigger).

**Parameter behavior:**

- `G.TIME`: 0 fast (~166.7 ms / 6 Hz), 16383 slow (~60 s)
- `G.TONE`: 8192 no spread; <8192 undertones; >8192 overtones
- `G.RAMP`: 0 percussive, 8192 triangle, 16383 reverse
- `G.CURV`: low step/log, 8192 linear, high smooth
- `G.RUN`: 8192 neutral; positive emphasizes; negative reverses/slow
- `G.MODE`: 0 Transient, 1 Sustain, 2 Cycle
- `G.V v divs reps`: immediate trigger; `divs` events per bar; `reps` additional hits (0 single, -1 infinite)

## BUS Routing


BUS is a shared CV bus (4 slots) that lets Teletype modulate other tracks without using hardware CV outputs.

**Example: Teletype drives NoteTrack transpose via BUS 1.**

1. In Teletype Script View (LIVE or S0), set BUS 1:

```
BUS 1 X
```

2. In the Routing page, create a route:

   - **Source**: BUS 1
   - **Target**: Transpose (or any track target)
   - **Tracks**: select the target track(s)

3. Now changing X in Teletype updates BUS 1 and modulates the route target.

Note: BUS is global. Multiple Teletype tracks can read/write the same BUS slot directly without routing.

## BAR Tempo-Locked Modulation

BAR is a read-only opcode that returns the position within the current musical bar (0-16383).
It provides zero-overhead tempo-synced modulation that automatically adjusts to your project's time signature.

### Basic Usage

**Simple sawtooth LFO synced to bar:**

```
CV 1 V BAR
```

This outputs a ramp from 0V to 10V over each bar. At bar start, BAR=0 (0V). At bar end, BAR=16383 (~10V).

**Multi-bar phase (1-128 bars):**

```
CV 1 V BAR 4
```
This outputs a ramp that spans 4 full bars before wrapping.

**Bipolar centered output (-5V to +5V):**

```
CV 2 V SUB BAR 8192
```
Subtracting 8192 centers the output around 0V, creating a -5V to +5V ramp.

### Conditional Logic Based on Bar Position

**Pulse gate only in 2nd half of bar:**

```
IF GT BAR 8192: TR.P A
```

**Divide bar into sections:**

```
X DIV BAR 4096
IF EQ X 0: SCRIPT 2
IF EQ X 1: SCRIPT 3
IF EQ X 2: SCRIPT 4
IF EQ X 3: SCRIPT 5
```

### Time Signature Awareness

BAR automatically adjusts to your project's time signature setting:

- **4/4 time**: BAR completes one cycle (0→16383) every 4 beats
- **3/4 time**: BAR completes one cycle every 3 beats
- **5/8 time**: BAR completes one cycle every 5 eighth-notes

### Musical Examples

**Bar-synced triangle wave:**

```
IF LT BAR 8192: CV 1 V BAR
ELSE: CV 1 V SUB 16383 BAR
```

**Bar-locked random sample & hold:**

```
IF LT BAR 100: X RAND 16383
CV 1 V X
```

**Cross-fade between two values over a bar:**

```
START 2000
END 8000
PROGRESS DIV BAR 16384
CV 1 V ADD START MUL SUB END START PROGRESS
```

### Performance Notes

- **Zero overhead**: BAR reads Engine's `measureFraction()` which is already computed every tick
- **High resolution**: Updates at 192 PPQN (192 times per quarter note)
- **Synchronized**: All Teletype tracks see the same BAR value (global project timing)
- **Read-only**: `BAR` has no setter—it always reflects current bar position

## WR Transport Flag

WR is a read-only opcode that returns whether the Performer transport is running.

**Gate only when transport is running:**

```
IF WR: TR.P A
```

**Hold a value when stopped:**

```
IF WR: X RAND 16383
CV 1 V X
```

WR returns `1` when the sequencer is playing and `0` when stopped.

**Set transport (run/stop):**

```
W.ACT 1   # start
W.ACT 0   # stop
W.ACT 2   # pause
```

## WNG / WNN Note Step Ops

Per-step gate and note access for Note tracks (track index 1..8, step index 1..64).

- `WNG t s` -> get gate (0/1) at step s
- `WNG t s v` -> set gate (v != 0 => on)
- `WNN t s` -> get note at step s (0..127)
- `WNN t s v` -> set note at step s (0..127, clamped)
- `WNG.H t` -> get gate at current gate step
- `WNN.H t` -> get note at current note step

Notes:
- These operate on the playing pattern for that track.
- If the track is not a Note track, getters return 0 and setters do nothing.

## WBPM Tempo Control

WBPM reads the current project tempo in BPM. WBPM.S sets the tempo.

**Read tempo:**

```
X WBPM     # X = current BPM
```

**Set tempo:**

```
WBPM.S 120
WBPM.S 87
```

Notes:
- Valid range: 1-1000 BPM.
- WBPM is a direct BPM value (no scaling to 0-16383).

## WMS / WTU Timing Helpers

WMS and WTU return tempo-derived durations in **milliseconds**.

### WMS (1/16 note in ms)

```
X WMS       # 1/16 note duration in ms
X WMS 4     # one beat (4x 1/16)
X WMS 16    # one bar in 4/4
```

### WTU (beat divided by n)

```
X WTU 3     # beat / 3 (triplet timing)
X WTU 5     # beat / 5 (quintuplet timing)
X WTU 7 2   # 2 * (beat / 7)
```

Notes:
- `WMS n` and `WTU n m` clamp `n` and `m` to 1-128.
- Use these values directly with `M` in MS timebase.

## RT Route Source Readback

RT reads the current source value for a routing slot (pre-shaper), scaled to Teletype's 0-16383 range.

**Mirror Route 1 source to a CV output:**

```
CV 1 V RT 1
```

**Use Route 2 source as a condition:**

```
IF GT RT 2 8192: TR.P 1
```

RT returns the normalized source value (0-1) of the route before shapers and per-track processing.

## WP Pattern-Aware Scripting

The `WP` (Westlicht Pattern) opcode allows Teletype scripts to query the current pattern index (0-15) of any track in the Performer project.

**Syntax:** 

`WP i` where `i` = track number (1-8)

**Returns:** 

Pattern index (1-16) for the specified track (matches Performer UI)


```
CV 1 N SCALE WP 1 1 16 60 75    # Map pattern 1-16 to notes C4-Eb5
IF EQ WP 1 5: TR.PULSE 1        # Trigger when track 1 on pattern 5
X WP 2                          # Store track 2's pattern in X
WP.SET 1 2                      # Force track 1 to pattern 2 (1-based)
```

### Cross-Track Pattern Detection

```
IF EQ WP 1 WP 2: TR 1 1         # Trigger when tracks 1 and 2 are on same pattern
```

### Pattern as Modulation Source

```
CV 1 N SCALE WP 1 1 16 48 72    # Linear mapping: pattern 1→C3, pattern 16→C5
CV 2 N ADD 60 WP 2              # C4 + pattern offset
```

### Pattern Change Detection

```
# In Init script (S0):
A WP 1                          # Store initial pattern in A

# In Metro script:
B WP 1                          # Get current pattern
IF NE A B: TR.PULSE 1           # Pulse on pattern change
A B                             # Update stored pattern
```

### Performance Characteristics

- **Zero overhead**: WP reads cached pattern state from TrackEngine
- **Read-only getter**: Cannot change patterns via WP (use Performer UI or MIDI)
- **Settable via WP.SET**: Use `WP.SET track pattern` (both 1-based)
- **Boundary safe**: Invalid track numbers return 0

## Pattern Math Ops (P.PA / P.PS / P.PM / P.PD / P.PMOD)

These ops process the active pattern range between `P.START` and `P.END`.
Use `PN.*` variants to target a specific pattern number.

**Shift all steps up by 100:**

```
P.PA 100
```

**Scale down by 2:**

```
P.PD 2
```

**Wrap steps into a 0-11 range:**

```
P.PMOD 12
```

**Remap pattern values to MIDI note range:**

```
P.SCALE 0 16383 48 72
```

## Pattern Analysis Ops (P.SUM / P.AVG / P.MINV / P.MAXV / P.FND)

These return values computed over `P.START`..`P.END`.

**Density check (sum of 0/1 gates):**

```
X P.SUM
```

**Average step value:**

```
Y P.AVG
```

**Find first zero in the active range:**

```
I P.FND 0
```

**Read min/max values:**

```
A P.MINV
B P.MAXV
```

## Pattern Randomize (RND.P / RND.PN)

Randomizes all steps in the active range `P.START..P.END`. Defaults to full 0-16383 range, or pass min/max bounds.

**Randomize working pattern:**

```
RND.P
RND.P 0 4095
```

**Randomize pattern 2:**

```
RND.PN 2
RND.PN 2 0 4095
```

## Trigger Divider & Width (TR.D / TR.W)

Divide or shape trigger pulses per output:

```
TR.D 1 2     # every other pulse on TR1
TR.W 1 50    # 50% width based on post-div interval
TR.P 1       # emit (subject to divider/width)
TR.W 1 0     # disable width mode (back to TR.TIME)
```

---

# Part 6: Tutorial

## Section 1: Basic Script Setup

### Step 1: Creating Your First Script

Let's start with a simple script that toggles a gate output when triggered:

```
TR.TOG A
```

This script will toggle gate output A each time the trigger input receives a signal.

### Step 2: Adding CV Output

Now let's add a CV output to the script:

```
TR.TOG A
CV 1 V 5
```

This will toggle gate A and set CV 1 to 5 volts when triggered.

### Step 3: Using Variables

Let's make the CV voltage dynamic using variables:

```
X ADD X 1
CV 1 V X
TR.TOG A
```

This script increments variable X each time it's triggered, sets CV 1 to that voltage, and toggles gate A.

## Section 2: Working with Patterns

### Step 4: Basic Pattern Usage

Patterns allow you to create sequences of values. Let's create a simple melody pattern:

```
CV 1 N P.NEXT
```

This script sets CV 1 to the next note in the pattern each time it's triggered.

### Step 5: Loading Pattern Values

To load values into the pattern, you can use the following commands in LIVE mode:

```
P.PUSH 0
P.PUSH 12
P.PUSH 7
P.PUSH 5
P.L 4
```

This creates a 4-step pattern with semitone values (0, 12, 7, 5) representing a simple chord progression.

### Step 6: Multiple Patterns

You can use multiple pattern banks (0-3):

```
P.N 1        // Switch to pattern 1
P.PUSH 0
P.PUSH 2
P.PUSH 4
P.PUSH 5
P.L 4
```

Then in your script:

```
CV 1 N PN.NEXT 1    // Get next value from pattern 1
```

## Section 3: Advanced Control Flow

### Step 7: Using EVERY for Clock Division

```
EVERY 2: TR.P A      // Pulse gate A every 2 triggers
EVERY 4: TR.P B      // Pulse gate B every 4 triggers
CV 1 V RAND 5        // Set CV 1 to random voltage 0-5V
```

### Step 8: Conditional Logic with IF

```
IF GT X 5: CV 1 V 5
IF LTE X 5: CV 1 V 0
X ADD X 1
TR.TOG A
```

This script checks if X is greater than 5, sets CV 1 accordingly, increments X, and toggles gate A.

### Step 9: Loops with L

```
L 1 4: TR.P I        // Pulse gates 1-4 in sequence
CV 1 N RAND 24       // Random note in 2-octave range
```

## Section 4: Integration with Performer Clock

### Step 10: Clock-Based Timing

In Performer, you can set the Teletype track to use clock timing instead of ms timing. In Clock mode, `M`/`M!`/`M.A` are derived from the current tempo/div/mult and are read-only (edits show "Clock Mode").

```
M.ACT 1              // Enable metronome
M 500                // Set metronome to 500ms (120 BPM) [MS timebase only]
M.A 500              // Set all Teletype track metros to 500ms
M.ACT.A 1            // Enable metros on all Teletype tracks
M.RESET.A            // Reset all Teletype metro timers
```

### Step 11: Using Metro Script

The metro script runs at the metronome interval:

```
// In M script:
CV 2 V DRUNK         // Set CV 2 to drunk-walk voltage
DRUNK.WRAP 1         // Enable wrapping
```

## Section 5: CV Inputs and Parameter Control

### Step 12: Using CV Inputs

You can use the IN and PARAM variables to read from CV inputs:

```
CV 1 V IN            // Set CV 1 to value of IN input
CV 2 V PARAM         // Set CV 2 to value of PARAM input
```

### Step 13: Scaling CV Inputs

```
X SCALE 0 16383 0 10 IN    // Scale IN from 0-16383 to 0-10V
CV 1 V X
```

## Section 6: Advanced Pattern Operations

### Step 14: Pattern Manipulation

```
P.NEXT              // Get next value in pattern
P.I ADD P.I 2       // Skip 2 steps in pattern
CV 1 N P.HERE       // Set CV to current pattern value
```

### Step 15: Pattern Ranges

```
P.START 2           // Set pattern start to index 2
P.END 6             // Set pattern end to index 6
P.WRAP 1            // Enable wrapping within range
```

## Section 7: Randomness and Probability

### Step 16: Random Operations

```
PROB 50: TR.P A      // 50% chance to pulse gate A
CV 1 V RRAND 2 8    // Random voltage between 2 and 8V
```

### Step 17: Drunk Walk

```
CV 1 V DRUNK         // Use drunk walk for CV
DRUNK.MIN 0          // Set minimum value
DRUNK.MAX 16383      // Set maximum value
DRUNK.WRAP 0         // Don't wrap at boundaries
```

## Section 8: MIDI Integration

### Step 18: MIDI Operations (Performer-specific)

```
MI.NV                // Get latest MIDI note as voltage
MI.CCV               // Get latest MIDI CC as voltage
CV 1 V MI.LNV        // Set CV 1 to latest MIDI note voltage
```

## Section 9: Delay and Stack Operations

### Step 19: Using Delays

```
TR.P A
DEL 100: TR.P B     // Trigger B 100ms after A
DEL 200: TR.P C     // Trigger C 200ms after A
```

### Step 20: Using Stacks

```
S: CV 1 V 5         // Stack this command
S: TR.P A           // Stack this command
S.ALL               // Execute all stacked commands
```

## Section 10: Complex Example

### Step 21: Complete Pattern Sequencer

Here's a complete example that combines multiple concepts:

```
// Script 1 (triggered by input):
CV 1 N P.NEXT        // Next note in main pattern
TR.P A               // Pulse gate A
X ADD X 1            // Increment step counter
MOD X 8              // Wrap at 8 steps
EQ X 0               // Check if at step 0
PROB 75: SCRIPT 2    // 75% chance to run script 2 on step 0

// Script 2 (called conditionally):
CV 2 V RAND 10       // Random CV on output 2
TR.P B               // Pulse gate B
```

## Section 11: Troubleshooting and Tips

### Common Issues:

1. **Scripts not running**: Check trigger input mapping in the UI
2. **CV outputs not working**: Verify CV output mapping in the UI
3. **Pattern not advancing**: Make sure you're using P.NEXT or manually incrementing P.I

### Performance Tips:

1. **Keep scripts efficient**: Complex scripts can impact performance
2. **Use appropriate timing**: Use clock timing for synced sequences
3. **Organize patterns**: Use different pattern banks for different purposes

## Section 12: Advanced Techniques

### Step 22: Function Scripts

Use SCRIPT to call other scripts as functions:

```
// Script 1: Main logic
CV 1 V RAND 5
SCRIPT 3             // Call script 3

// Script 3: Function
CV 2 V ADD CV 2 1    // Increment CV 2
```

### Step 23: Script Function Operations ($F, $F1, $F2, $L, $L1, $L2, $S, $S1, $S2)

Advanced function operations allow you to treat scripts as functions that return values:

```
// $F: Execute script as function
# Script 1: Define a function that returns the square of X
* X X

# In another script: Call script 1 as a function and use its return value

A $F 1    // A gets the square of X

// $F1: Execute script as function with 1 parameter
# Script 1: Function that returns the square of the input parameter
FR * I1 I1

# In another script: Call script 1 with parameter 5
A $F1 1 5    // A gets 25 (5 squared)

// $F2: Execute script as function with 2 parameters
# Script 1: Function that returns the sum of two input parameters
FR + I1 I2

# In another script: Call script 1 with parameters 3 and 4
A $F2 1 3 4    // A gets 7 (3 + 4)
```

### Step 24: Queue Operations

Use queues for smoothing or averaging:

```
Q CV 1               // Add current CV 1 value to queue
Q.N 4                // Set queue length to 4
CV 2 V Q.AVG         // Set CV 2 to average of queue
```

 qq qq

