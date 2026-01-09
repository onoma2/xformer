# Teletype Track Learning Tutorial for Performer

## Introduction

This tutorial is designed for intermediate teletype users who want to learn how to use the Teletype Track functionality within the Performer firmware. The Teletype Track in Performer provides a powerful way to integrate teletype scripts with your sequencer, allowing for complex musical patterns and interactions.

## Prerequisites

Before starting this tutorial, you should have:
- Basic understanding of teletype operations and syntax
- Familiarity with the Performer sequencer interface
- Understanding of CV/Gate concepts in modular synthesis

## Overview of Teletype in Performer

The Teletype Track in Performer provides:
- 4 CV outputs (CV 1-4) mapped to Performer's CV outputs
- 4 Gate outputs (TR A-D) mapped to Performer's gate outputs
- 4 trigger inputs (TI-TR1-4) to trigger scripts
- 2 CV inputs (TI-IN and TI-PARAM) from Performer's CV inputs
- Integration with Performer's clock system
- Pattern and variable storage
- Two pattern slots (P1/P2) for S0 + Metro + I/O mapping + TT patterns
  - Slot = Performer pattern index % 2
  - Script/Pattern View shows the active slot label before the script name (or top right in Pattern View)

## The T9 Type (Reference)

### 1. Script View Page (The Editor)

| Action | Hardware Control | Performer Function |
| :--- | :--- | :--- |
| **Active Slot** | **P1/P2 label (top right)** | Shows the current pattern slot (pattern % 2). |
| **Select Line** | **Shift + Encoder Turn** | Moves the vertical selection (Lines 1-6). |
| **Edit Line** | **Encoder Press** | Loads selected line into the edit buffer. |
| **Commit Line** | **Shift + Encoder Press** | Compiles buffer and saves it to the script. |
| **Select Script** | **Fn 1 - Fn 9** | Switches between Scripts (S1-S8, Metro). |
| **Move Cursor** | **Encoder Turn** | Moves edit cursor horizontally. |
| **Cursor Left** | **Shift + Page Left** | Moves cursor left in edit buffer. |
| **Cursor Right** | **Shift + Page Right** | Moves cursor right in edit buffer. |
| **Backspace** | **Page Left** | Deletes character before cursor. |
| **Space** | **Page Right** | Inserts a space. |
| **Insert Text** | **Step Keys 1-16** | Cycles through characters (0-9, A-Z, symbols). |

Notes:
- S0 and Metro are per-slot (P1/P2). S1–S3 are global across patterns.

### Script View I/O Grid Highlights

The right-side I/O grid shows live Teletype input/output activity and ownership:
- **BUS block**: four CV bus meters (A–D) showing current bus voltages.
- **TI row**: filled block indicates TI-TR assigned; brighter when active.
- **TO (gates) row**:
  - Solid block = destination owned by this track in Layout (bright when active).
  - Outline block = destination owned by a different track (Layout mismatch).
- **CV row**:
  - Bright bar = destination owned by this track in Layout.
  - Dim bar = destination owned by a different track (Layout mismatch).
- **IN/PARAM bars**: brighter when assigned; dim when unassigned.

### 2. Pattern View Page (The Tracker)

| Action | Hardware Control | Performer Function |
| :--- | :--- | :--- |
| **Select Row** | **Encoder Turn** | Moves the vertical selection (Index 0-63). |
| **Select Col** | **Fn 1 - Fn 4** | Switches between Pattern 0 to 3. |
| **Negate Value** | **Encoder Press** | Toggles sign of value or buffer. |
| **Commit Value** | **Shift + Encoder Press** | Writes buffer to pattern and advances length. |
| **Insert Digit** | **Step Keys 1-10** | Enables numeric entry (digits 0-9). |
| **Backspace** | **Page Left** | Deletes last digit from buffer. |
| **Delete Row** | **Shift + Page Left** | Deletes current row (shifts subsequent rows up). |
| **Insert Row** | **Page Right** | Duplicates current row (shifts subsequent rows down). |
| **Toggle Turtle**| **Shift + Page Right** | Toggles Turtle visibility on the grid. |
| **Set Length** | **Step Key 14** | Sets Pattern Length to current row + 1. |
| **Set Start** | **Step Key 15** | Sets Loop Start to current row. |
| **Set End** | **Step Key 16** | Sets Loop End to current row. |


## Performer Teletype Keyboard Shortcuts

The Performer implementation of Teletype includes specific keyboard shortcuts for editing scripts:

### Function Keys (F0-F4)
- **F1**: Select S0
- **F2**: Select S1
- **F3**: Select S2
- **F4**: Toggle S3 ↔ Metro
- **F5**: Toggle Pattern View - Live view - Script View

- **Shift + F1-F4**: Fire script S0-S3 (TI-TR1..4)

### Step Keys (Steps 1-16)
- **Steps 1-16**: Insert common teletype commands/variables (cycles through options)
-
  - **Step 1**: 1 → A → B (cycles through 1→A→B)
  - **Step 2**: 2 → C → D (cycles through 2→C→D)
  - **Step 3**: 3 → E → F (cycles through 3→E→F)
  - **Step 4**: 4 → G → H (cycles through 4→G→H)
  - **Step 5**: 5 → I → J (cycles through 5→I→J)
  - **Step 6**: 6 → K → L (cycles through 6→K→L)
  - **Step 7**: 7 → M → N (cycles through 7→M→N)
  - **Step 8**: 8 → O → P (cycles through 8→O→P)
  - **Step 9**: 9 → Q → R (cycles through 9→Q→R)
  - **Step 10**: 0 → S → T (cycles through 0→S→T)
  - **Step 11**: . → U → V (cycles through .→U→V)
  - **Step 12**: : → W → X (cycles through :→W→X)
  - **Step 13**: ; → Y → Z (cycles through ;→Y→Z)
  - **Step 14**: Backspace (delete character before cursor) - DUPLICATED FROM LEFT ARROW
  - **Step 15**: Insert space (insert space character) - DUPLICATED FROM RIGHT ARROW
  - **Step 16**: Commit line and advance (commit current line and move to next line) - REPLACES ORIGINAL FUNCTION


### Page + Step Keys (Steps 1-16)

- **Page + Step 9**: Copy current line
- **Page + Step 10**: Paste line
- **Page + Step 11**: Duplicate line
- **Page + Step 12**: Comment/uncomment line
- **Page + Step 13**: Delete line

### Shift + Step Keys (Steps 1-16)

- **Shift + Steps 1-16**: Insert special characters with rotation

  - **Shift + Step 1**: + → - (Plus → Minus, cycles)
  - **Shift + Step 2**: * → / (Multiply → Divide, cycles)
  - **Shift + Step 3**: = → ! (Equals → Logical NOT, cycles)
  - **Shift + Step 4**: < → > (Less than → Greater than, cycles)
  - **Shift + Step 5**: % → ^ (Modulo → Bitwise XOR, cycles)
  - **Shift + Step 6**: & → | (Bitwise AND → Bitwise OR, cycles)
  - **Shift + Step 7**: $ → @ (Dollar → At symbol, cycles)
  - **Shift + Step 8**: ? → ; (Question mark → Semicolon, cycles)
  - **Shift + Step 9**: CV → CV.SLEW (CV vs Slewed CV, cycles)
  - **Shift + Step 10**: TR. → TR.TIME (Trigger vs Trigger timing, cycles)
  - **Shift + Step 11**: PARAM → SCL (Parameter vs Scale, cycles)
  - **Shift + Step 12**: P.NEXT → P.HERE (Next pattern vs Current pattern, cycles)
  - **Shift + Step 13**: ELIF → OTHER (Else-if vs Other, cycles)
  - **Shift + Step 14**: RRAND → RND.P (Random range vs Random pattern, cycles)
  - **Shift + Step 15**: DRUNK → WRAP (Drunk walk vs Wrap, cycles)
  - **Shift + Step 16**: M.ACT → M.RESET (Metro activate vs Metro reset, cycles)

### Navigation and Editing

- **Left Arrow**: **Backspace** (delete character to the left)
- **Shift + Left Arrow**: Move cursor left
- **Right Arrow**: Insert space (**Spacebar**)
- **Shift + Right Arrow**: Move cursor right
- **Encoder Press**: Load selected line into edit buffer
- **Shift + Encoder Press**: Commit line (save changes - **Enter**)
- **Encoder Turn**: Move cursor left/right (normal) or select line (with Shift)

### Other Controls

- **Page + Left/Right**: History prev/next (live/edit)

## Teletype Pattern View Keyboard Shortcuts

The Performer implementation also includes a pattern view for editing teletype patterns:

### Function Keys
- **F1-F4**: Select pattern 0-3
- **F4**: Toggle back to Script View

### Step Keys (Steps 1-16)
- **Steps 1-9**: Insert digits 1-9 into the currently selected pattern position
- **Step 10 (0)**: Insert digit 0 into the currently selected pattern position
- **Step 14**: Backspace digit (delete last digit from buffer) - DUPLICATED FROM LEFT ARROW
- **Step 16**: Commit edited value - DUPLICATED FROM SHIFT+ENCODER

### Page + Step Keys (Steps 4-6)
- **Page + Step 4**: Set pattern length to current row position (moved from Step 14)
- **Page + Step 5**: Set pattern start position to current row (moved from Step 15)
- **Page + Step 6**: Set pattern end position to current row (moved from Step 16)

### Navigation and Editing
- **Left Arrow**: Backspace digit (while editing); **Shift + Left** deletes row
- **Right Arrow**: Insert row; **Shift + Right** toggles turtle display
- **Encoder Turn**: Move up/down through pattern rows
- **Encoder Press**: Negate current value (make positive/negative)
- **Shift + Encoder Press**: Commit edited value

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
   - **TO-TRA..TO-TRD**: map Teletype trigger outputs A–D to Performer gate outputs.  
   - **TO-CV1..TO-CV4**: map Teletype CV outputs 1–4 to Performer CV outputs.
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

## Teletype Panic (Page + Track 6)

Need to instantly silence all Teletype activity? Use the global panic:

**Page + Track 6** → `TT PANIC`

This stops **all Teletype tracks** by:

- disabling metro (M.ACT = 0)
- clearing pending DEL commands
- clearing TR pulses and forcing gates low
- zeroing all Teletype CV outputs

## Default Scripts on New T9type Track

When a Teletype track has no scripts, it seeds two defaults:

- **S0**: `M.ACT 1` (enables metro)
- **M (Metro)**: `CV 1 N 48 ; TR.P 1` (C3 on CV1 + trigger pulse)

You can overwrite these immediately in Script View.


### 3. Teletype Script View - I/O Grid

The Script View now features a 4x4 visualization grid in the top-right corner, providing real-time feedback on Teletype I/O states.

**Grid Layout (3 Rows):**

1.  **TI (Trigger Inputs)** - Top Row
    -   Displays the state of `TI-TR1` through `TI-TR4`.
    -   **Dim (Low):** Input is unassigned (None).
    -   **Medium:** Input is assigned but inactive.
    -   **Bright:** Input is assigned and receiving a high signal (Gate High).

2.  **TO (Trigger Outputs)** - Middle Row
    -   Displays the state of `TO-TRA` through `TO-TRD` (mapped to Performer Gate outputs).
    -   **Dim (Low):** The target Gate Output is assigned to a *different* track in the global Layout. (Signal is generated but effectively disconnected from this track).
    -   **Medium:** Output is owned by this track but inactive.
    -   **Bright:** Output is owned and pulsing (Active).

3.  **CV (CV Outputs)** - Bottom Section (Double Height)
    -   Displays the state of `TO-CV1` through `TO-CV4` (mapped to Performer CV outputs).
    -   **Visualization:** A vertical bar graph growing from the center (Bipolar).
    -   **Center Line:** Represents 0V.
    -   **Upward Bar:** Positive voltage (> 0V).
    -   **Downward Bar:** Negative voltage (< 0V).
    -   **Ownership:** If the target CV Output is assigned to another track, the bar is **hidden** (only the dim container frame is visible), indicating the signal is disconnected.

4. **In and Param values** 

	- Top is In
	- Bottom is param

5. **Bus 1-4 values** 

To the left of track specific params there is a display of bus values.

	- Top is BUS 1-2
	- Bottom is BUS 2-4

## BUS Routing Example (Performer)

BUS is a shared CV bus (4 slots) that lets Teletype modulate other tracks without using hardware CV outputs.

Example: Teletype drives NoteTrack transpose via BUS 1.

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

## BAR Tempo-Locked Modulation (Performer)

BAR is a read-only opcode that returns the position within the current musical bar (0–16383).
It provides zero-overhead tempo-synced modulation that automatically adjusts to your project's time signature.

### Basic Usage

**Simple sawtooth LFO synced to bar:**
```
CV 1 V BAR
```
This outputs a ramp from 0V to 10V over each bar. At bar start, BAR=0 (0V). At bar end, BAR=16383 (~10V).

**Multi-bar phase (1–128 bars):**
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
When BAR exceeds 8192 (midpoint), pulse gate A.

**Divide bar into sections:**
```
X DIV BAR 4096
IF EQ X 0: SCRIPT 2
IF EQ X 1: SCRIPT 3
IF EQ X 2: SCRIPT 4
IF EQ X 3: SCRIPT 5
```
This divides each bar into 4 sections (0, 1, 2, 3) and runs different scripts based on which section you're in.

### Time Signature Awareness

BAR automatically adjusts to your project's time signature setting:

- **4/4 time**: BAR completes one cycle (0→16383) every 4 beats
- **3/4 time**: BAR completes one cycle every 3 beats
- **5/8 time**: BAR completes one cycle every 5 eighth-notes

No configuration needed—BAR always represents one complete bar regardless of time signature.

### Musical Examples

**Create a bar-synced triangle wave:**

```
# S0: Triggered each tick
IF LT BAR 8192: CV 1 V BAR
ELSE: CV 1 V SUB 16383 BAR
```


**Bar-locked random sample & hold:**

```
# S0: Runs on bar start
IF LT BAR 100: X RAND 16383
CV 1 V X
```
When BAR resets to 0 (start of bar), generate new random value and hold it for entire bar.


**Cross-fade between two values over a bar:**

```
START 2000
END 8000
PROGRESS DIV BAR 16384
CV 1 V ADD START MUL SUB END START PROGRESS
```

Linear interpolation from START voltage to END voltage across the bar.

### Combining BAR with Patterns

**Bar-synced pattern playback:**

```
# M script (metro)
X DIV BAR 2048       # Divide bar into 8 sections
P.I X                # Set pattern index to section
CV 2 N P.HERE        # Output current pattern value
```

### Integration with Routing

BAR is a Teletype-internal value and cannot be directly routed to other tracks.
To modulate other tracks with BAR, write it to a CV output or BUS slot:

```
# Write BAR to CV output
CV 1 V BAR

# Write BAR to BUS for routing
BUS 1 BAR
```

Then use Performer's Routing page to route CV 1 or BUS 1 to any track parameter.

### Performance Notes

- **Zero overhead**: BAR reads Engine's `measureFraction()` which is already computed every tick
- **High resolution**: Updates at 192 PPQN (192 times per quarter note)
- **Synchronized**: All Teletype tracks see the same BAR value (global project timing)
- **Read-only**: `BAR` has no setter—it always reflects current bar position

## WR Transport Flag (Performer)

WR is a read-only opcode that returns whether the Performer transport is running.

Use WR.ACT to start/stop the transport.

### Basic Usage

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
WR.ACT 1   # start
WR.ACT 0   # stop
WR.ACT 2   # pause
```

## WBPM Tempo Control (Performer)

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

## WMS / WTU Timing Helpers (Performer)

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

- `WMS n` and `WTU n m` clamp `n` and `m` to 1–128.
- Use these values directly with `M` in MS timebase.

Notes:

- Valid range: 1–1000 BPM.
- WBPM is a direct BPM value (no scaling to 0–16383).
- WBPM.S clamps to 1–1000 BPM.

## RT Route Source Readback (Performer)

RT reads the current source value for a routing slot (pre-shaper), scaled to Teletype's 0–16383 range.

### Basic Usage

**Mirror Route 1 source to a CV output:**

```
CV 1 V RT 1
```

**Use Route 2 source as a condition:**

```
IF GT RT 2 8192: TR.P 1
```

RT returns the normalized source value (0–1) of the route before shapers and per-track processing.

## Pattern Math Ops (P.PA / P.PS / P.PM / P.PD / P.PMOD)

These ops process the active pattern range between `P.START` and `P.END`.
Use `PN.*` variants to target a specific pattern number.

**Shift all steps up by 100 - Pattern.PatternAdd:**

```
P.PA 100
```

**Scale down by 2 - Pattern.PatternDivide :**

```
P.PD 2
```

**Wrap steps into a 0–11 range - Pattern.PatternModulo:**

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

Randomizes all steps in the active range `P.START..P.END`. Defaults to full
0–16383 range, or pass min/max bounds.

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

## Envelope (E.*)

Per-CV attack/decay envelope with target/offset:

```
E 1 12000     # target (raw 0–16383)
E.O 1 4000    # offset/baseline
E.A 1 200     # attack ms
E.D 1 400     # decay ms
E.T 1         # trigger envelope
```

Optional triggers and looping:


```
E.R 1 1       # pulse TR1 at end of rise
E.C 1 2       # pulse TR2 at end of cycle
E.L 1 4       # loop 4 cycles (0 = infinite)
```

## WP - Pattern-Aware Scripting (Performer)


The `WP` (Westlicht Pattern) opcode allows Teletype scripts to query the current pattern index (0-15) of any track in the Performer project. This enables pattern-aware scripting where behavior changes based on which patterns are playing across all 8 tracks.

### Basic Usage

**Syntax:** 

`WP i` where `i` = track number (1-8)

**Returns:** 

Pattern index (1-16) for the specified track (matches Performer UI)


```
# Query pattern on track 1
CV 1 N SCALE WP 1 1 16 60 75    # Map pattern 1-16 to notes C4-Eb5

# Conditional based on pattern
IF EQ WP 1 5: TR.PULSE 1        # Trigger when track 1 on pattern 5

# Store pattern for later use
X WP 2                          # Store track 2's pattern in X

WP.SET 1 2                      # Force track 1 to pattern 2 (1-based)
```


### Pattern-Dependent Behavior

Use WP to create scripts that change based on which pattern is playing:

```
# Different CV output per pattern on track 1
X WP 1
IF EQ X 0: CV 1 V 2
IF EQ X 1: CV 1 V 4
IF EQ X 2: CV 1 V 6
IF EQ X 3: CV 1 V 8

# Pattern-stable randomization
RAND.SEED WP 1                  # Same seed = same random sequence
CV 2 N RND 60 84                # Pattern 0 always gives same notes
TR.PULSE 2
```

### Cross-Track Pattern Detection

Detect when multiple tracks are on specific patterns:

```
# Trigger when tracks 1 and 2 are on the same pattern
IF EQ WP 1 WP 2: TR 1 1
IF NE WP 1 WP 2: TR 1 0

# Detect when all drum tracks on pattern 0 (intro)
IF AND4 EQ WP 1 1 EQ WP 2 1 EQ WP 3 1 EQ WP 4 1: CV 1 V 10

# Complex pattern combinations
IF AND EQ WP 1 5 EQ WP 3 7: CV 2 V 7   # Tracks 1&3 on specific patterns
```

### Pattern as Modulation Source

Map pattern indices to CV values for dynamic variation:


```
# Linear mapping: pattern 0→C3, pattern 15→C5
CV 1 N SCALE WP 1 1 16 48 72

# Quantized steps: each pattern = semitone
CV 2 N ADD 60 WP 2              # C4 + pattern offset

# Exponential-ish: square the pattern index
X WP 3
CV 3 N ADD 60 DIV MUL X X 4     # C4 + (pattern²/4)

# Use sum of all patterns
X 0
L 1 8: X ADD X WP I             # Sum all 8 track patterns
CV 4 N SCALE X 0 120 36 96      # Map to 5-octave range
```

### Pattern-Driven Rhythm

Create rhythmic variations based on pattern state:


```
# Metro script with pattern-dependent tempo
M DIV 1000 ADD 4 WP 1           # Faster on higher patterns
M!                              # Trigger metro

# Pattern-dependent probability
PROB SCALE WP 2 1 16 10 90      # 10% on pattern 1, 90% on pattern 16
IF TOSS: TR.PULSE 3
```


### Global Pattern State Hash

Create a fingerprint of all patterns playing across all tracks:

```

# Simple sum (0-120 range)
X 0
L 1 8: X ADD X WP I
CV 1 N SCALE X 0 120 48 84

# Weighted combination
X 0
L 1 8: X ADD X MUL WP I I       # Weight by track number
CV 2 N SCALE X 0 252 36 96      # Max = 1*15 + 2*15 + ... + 8*15 = 540

# XOR combination for variation
X WP 1
L 2 8: X XOR X WP I
CV 3 N SCALE X 1 16 60 75
```

### Pattern Change Detection

Store previous pattern and trigger on changes:


```
# In Init script (S0):
A WP 1                          # Store initial pattern in A

# In Metro script:
B WP 1                          # Get current pattern
IF NE A B: TR.PULSE 1           # Pulse on pattern change
A B                             # Update stored pattern
```

### Use Cases

**Song sections:**

```
# Different behavior for verse/chorus/bridge
IF LT WP 1 4: CV 1 V 3          # Patterns 0-3 = verse
IF INRI WP 1 4 7: CV 1 V 5      # Patterns 4-7 = chorus
IF GT WP 1 7: CV 1 V 8          # Patterns 8-15 = bridge
```

**Euclidean density control:**

```
# Use pattern as Euclidean fill parameter
ER 16 WP 1                      # Pattern 0 = 0/16, pattern 15 = 15/16
IF NZ: TR.PULSE 1
```

**Deterministic generative:**

```
# Pattern-stable generative sequences
SEED ADD WP 1 MUL WP 2 16       # Combine 2 track patterns for seed
L 1 8: CV I N RND 48 84         # Same patterns = same notes every time
```

### Routing Integration

Combine WP with Performer's routing system:

```
# Route pattern-dependent CV
CV 1 N SCALE WP 1 1 16 60 75
# In Performer routing: CV 1 → Track 5 Transpose

# Gate based on pattern match
IF EQ WP 2 WP 3: TR 1 1         # Gate when patterns match
# In Performer routing: Gate 1 → Track 6 Gate

# Dynamic sequencer control
CV 2 V MUL WP 4 200             # 0-3V based on pattern
# In Performer routing: CV 2 → Track 7 Clock Divisor CV
```

### Performance Characteristics

- **Zero overhead**: WP reads cached pattern state from TrackEngine
- **Instant**: No pattern lookup or computation required
- **Global view**: All 8 tracks accessible from any Teletype track
- **Read-only**: Cannot change patterns via WP (use Performer UI or MIDI Program Change)
- **Settable via WP.SET**: Use `WP.SET track pattern` (both 1-based)
- **Boundary safe**: Invalid track numbers return 0

### Notes

- Track numbers are **1-indexed** (1-8 for users)
- Pattern indices are **1-indexed** (1-16 returned by WP)
- WP.SET uses **1-indexed** inputs (1-16)
- Snapshots internally use pattern index 16, but WP returns the active pattern index
- Out-of-bounds track numbers (< 1 or > 8) return 0
- WP works even when called from a Teletype track that isn't the selected track





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

// $L: Execute specific line as function
# Script 1:
X + A B    // Line 1: X = A + B
Y * C D    // Line 2: Y = C * D

# In another script: Execute line 1 of script 1 and get its result
Z $L 1 1   // Z gets the result of A + B

// $L1: Execute specific line as function with 1 parameter
# Script 1:
X + I1 10  // Line 1: X = parameter + 10

# In another script: Execute line 1 of script 1 with parameter 5
Z $L1 1 1 5   // Z gets 15 (5 + 10)

// $L2: Execute specific line as function with 2 parameters
# Script 1:
X + I1 I2  // Line 1: X = parameter1 + parameter2

# In another script: Execute line 1 of script 1 with parameters 3 and 4
Z $L2 1 1 3 4   // Z gets 7 (3 + 4)

// $S: Execute line in same script as function
# In any script:
X + A B    // Line 1: X = A + B
Y $S 1     // Line 2: Y gets the result of line 1 (A + B)

// $S1: Execute line in same script as function with 1 parameter
# In any script:
X + I1 10  // Line 1: X = parameter + 10
Y $S1 1 5  // Line 2: Y gets 15 (5 + 10)

// $S2: Execute line in same script as function with 2 parameters
# In any script:
X + I1 I2  // Line 1: X = parameter1 + parameter2
Y $S2 1 3 4  // Line 2: Y gets 7 (3 + 4)

// Practical example combining all concepts:
# Script 1: A function to calculate a note value based on scale degree
# Input: I1 = scale degree (0-7), I2 = root note
FR N.S I2 0 I1    // Return note from major scale at root I2, degree I1

# Script 2: A function to calculate probability
# Input: I1 = base probability, I2 = modifier
X + I1 I2
FR MIN X 100        // Return probability capped at 100%

# Script 3: Main logic using the functions
ROOT_NOTE 60        // C4 as root
DEGREE RAND 7       // Random scale degree

# Get note from scale
NOTE $F1 1 DEGREE ROOT_NOTE

# Output the note
CV 1 N NOTE

# Calculate probability based on pattern position
PROBABILITY $F2 2 50 WP 1    // Base 50% + pattern position

# Use probability to conditionally trigger
PROB PROBABILITY: TR.P 1
```

### Step 23: Queue Operations

Use queues for smoothing or averaging:

```
Q CV 1               // Add current CV 1 value to queue
Q.N 4                // Set queue length to 4
CV 2 V Q.AVG         // Set CV 2 to average of queue
```

## Example Scripts from Teletype Codex

Here are some practical example scripts from the Teletype Codex that demonstrate advanced techniques:

### Example 1: Tetra Sequencer
This script creates 4 independent CV and trigger sequencers:

```
#1 (Main sequencer)
SCRIPT 2; SCRIPT 3; SCRIPT 4

#2 (Sequencer 1 & 2)
P.N 0; A P.HERE
IF NZ A: TR.P 1; CV 1 N A
P.NEXT
P.N 1; B P.HERE
IF NZ B: TR.P 2; CV 2 N B
P.NEXT

#3 (Sequencer 3 & 4)
P.N 2; C P.HERE
IF NZ C: TR.P 3; CV 3 N C
P.NEXT
P.N 3; D P.HERE
IF NZ D: TR.P 4; CV 4 N D
P.NEXT

#4 (Parameter control)
Y DIV PARAM 2
IF NZ PARAM: P.HERE Y

#5-8 (Randomize sequencers)
#5: P.N 0; P.INS 0 RRAND 0 1; PN.POP 0
#6: P.N 1; P.INS 0 RRAND 0 200; PN.POP 1
#7: P.N 2; P.INS 0 RRAND 0 200; PN.POP 2
#8: P.N 3; P.INS 0 RRAND 1 200; PN.POP 3

#I (Init script)
P.N 0; P.START 0; P.END 16
P.N 1; P.START 0; P.END 16
P.N 2; P.START 0; P.END 16
P.N 3; P.START 0; P.END 16
PN.I 0 0; PN.I 1 0; PN.I 2 0; PN.I 3 0
```

**What it does:** This creates a 4-channel sequencer where each pattern bank (0-3) controls a different CV and trigger output. When triggered, it advances all 4 patterns simultaneously, sending CV values to outputs 1-4 and triggering gates when the pattern value is non-zero.

**What to expect:** You'll get 4 synchronized sequencers that can play different patterns. Pattern 0 controls CV 1 and TR A, Pattern 1 controls CV 2 and TR B, and so on. The randomize scripts (5-8) will add random values to each pattern.

### Example 2: Turing Machine
A generative sequencer using probability:

```
#5 (Copy pattern 0 to 1)
L 0 7 : PN 1 I PN 0 I

#6 (Clock out pattern 1)
P.N 1
P.INS 0 P.POP
X 0
L 0 4 : X ADD X LSH P I I
CV 2 N PN 4 X
TR.PULSE 2

#7 (Parameter control)
P.N 0
I SCALE 100 16000 0 100 PARAM
PROB I : P.PUSH EZ P.POP

#8 (Clock trigger)
P.N 0
P.INS 0 P.POP
X 0
L 0 4 : X ADD X LSH P I I
CV 1 N PN 4 X
TR.PULSE 1

#I (Init script)
P.N 0
L 0 63 : P.POP
L 0 7 : P.PUSH TOSS
P.N 1
L 0 63 : P.POP
L 0 7 : P.PUSH PN 0 I
```

**What it does:** This creates a Turing machine-style generative sequencer that uses probability to determine whether to add new values to the pattern. It uses the PARAM knob to control the probability of adding new values.

**What to expect:** The sequencer will generate evolving patterns based on probability. When you press script 8, it will clock out a value from pattern 0 and send it to CV 1. Script 7 uses the PARAM knob to control the probability of adding new values to the pattern. The higher the PARAM value, the more likely it is to add new values.

### Example 3: Triangle Mountain
A melodic sequencer with scale control:

```
#1 (Reset position)
P.I 0

#2 (Next scale)
P.N WRAP ADD P.N 1 0 3
P.I P.END

#3 (Shorten loop)
P.END WRAP SUB P.END 1 1 7

#4 (Toggle forward/reverse)
X EZ X

#5 (Throw CV 2 a new note)
S : CV 2 ADD N P.HERE V 1
S : TR.PULSE B

#6 (Randomize loop length)
P.END RRAND 1 7

#7 (Set root to CV in)
Z IN

#8 (Add 1 semi to CV 4)
CV 4 N WRAP 0 0 11

#M (Metro script)
IF X : P.PREV
ELSE : P.NEXT
CV 1 ADD N P.HERE Z
M SUB 320 RSH PARAM 6
S.ALL
TR.PULSE A

#I (Init script)
L A B : TR.TIME I 40
```

**What it does:** This creates a melodic sequencer that can play forward or backward through a pattern, with tempo controlled by the PARAM knob. It allows transposition via CV input and can play in different scales.

**What to expect:** The sequencer will play through your pattern at a tempo set by the PARAM knob (higher PARAM = faster). Script 1 resets the position to 0, script 2 cycles through different pattern banks, script 3 shortens the loop, script 4 toggles between forward and reverse playback, script 5 sends a new note to CV 2, script 6 randomizes the loop length, script 7 sets the root note based on CV input, and script 8 adds 1 semitone to CV 4. The metro script runs automatically and sends the current pattern value to CV 1, transposed by the root note.

## Conclusion

This tutorial has covered the fundamental concepts of using Teletype in the Performer firmware. From basic scripts to complex pattern operations, you now have the tools to create intricate musical sequences and interactions.

Remember to experiment with different combinations of operations, and don't hesitate to refer back to the teletype manual for more detailed information about specific operations.

The key to mastering Teletype in Performer is practice and experimentation. Try modifying the examples in this tutorial to create your own unique patterns and behaviors.


