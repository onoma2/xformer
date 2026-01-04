# Teletype Track Functionality

This manual covers the Teletype operations available in the **Performer** firmware port.

**Performer Voltage Mapping (important):**

*   The Teletype VM still computes in **0–10V raw units** (0–16383).
*   The Performer bridge maps those raw values to **-5V..+5V**.
*   Result: `V 5` outputs **0V** in Performer (raw ~8192 maps to the midpoint).

**Supported Features:**

*   **Core Logic:** Math, Logic, Random, Patterns, Variables, Stack, Queue.
*   **Sequencing:** Metronome (`M`), Delay (`DEL`), Scripts.
*   **Local I/O:** 4 CV Outputs (`CV`), 4 Gate Outputs (`TR`), 2 Inputs (`IN`, `PARAM`).
*   **BUS (Performer):** Shared CV bus slots (`BUS`), usable in routing.
*   **WBPM (Performer):** Tempo read/write (`WBPM`, `WBPM.S`).
*   **WR (Performer):** Transport running flag (`WR`, `WR.ACT`).
*   **RT (Performer):** Route source readback (`RT`).
*   **MIDI:** Full MIDI input support (`MI.*`).

**Unsupported/Removed:**

*   **Grid:** All Grid operations (`G.*`) and `SCENE.G` are unavailable or no-ops.
*   **External Hardware:** All ops for I2C modules (Ansible, TELEX, Just Friends, etc.) are removed.
*   **Screen Flipping:** `DEVICE.FLIP` is disabled.

---
## BUS (Performer-only)

BUS is a shared CV bus with 4 slots. It stores voltages in the Performer domain (-5V..+5V),
and Teletype reads/writes in raw 0–16383 units.

Syntax:
- `BUS n` → read BUS slot n (1..4)
- `BUS n x` → write raw value x to BUS slot n (1..4)

Notes:
- Slots are 1-based (like `SCRIPT`).
- Use routing to map BUS 1–4 to any track target.
- BUS is global: multiple Teletype tracks can read/write the same slots without routing.

## BAR (Performer-only)

BAR is a tempo-locked phase indicator that returns the position within the current bar (measure).
It provides zero-overhead access to Performer's internal Engine timing for creating tempo-synced modulation.

Syntax:
- `BAR` → read current bar phase (0..16383)

Returns:
- `0` at the start of a bar
- `8192` at the midpoint of a bar (e.g., beat 3 in 4/4 time)
- `16383` near the end of a bar

Voltage mapping (via `V` scale):
- Raw value 0–16383 maps to 0–10V output
- Example: `CV 1 V BAR` creates a sawtooth LFO ramping 0V→10V per bar

Time signature awareness:
- Automatically adjusts to project time signature (4/4, 3/4, 5/8, etc.)
- Bar length changes with time signature setting in Performer

Example uses:
```
# Sawtooth LFO synced to bar
CV 1 V BAR

# Pulse gate only in 2nd half of bar
IF BAR > 8192: TR.P 1

# Divide bar into 4 sections (0, 1, 2, 3)
X DIV BAR 4096
IF EQ X 0: SCRIPT 1

# Bipolar centered output (-5V to +5V)
CV 2 V SUB BAR 8192

# Linear interpolation across bar
START 1000
END 5000
CV 3 V ADD START MUL SUB END START DIV BAR 16384
```

Notes:
- BAR is read-only (no setter)

## WR (Performer-only)

WR returns whether the Performer transport is currently running.

- `WR` → read transport running flag (0/1)

Notes:
- Use `WR.ACT x` to control transport.
- `WR.ACT 1` starts, `WR.ACT 0` stops (reset).
- Any other value pauses (clock stop).

## WBPM (Performer-only)

WBPM reads or sets the project tempo in BPM.

- `WBPM` → read current tempo (1–1000 BPM)
- `WBPM.S x` → set tempo to x BPM (clamped to 1–1000)

## TR.D / TR.W (Performer-only)

Per-output trigger divider and width.

- `TR.D n div` → only let every `div`th pulse through (n = 1–4, div ≥ 1)
- `TR.W n pct` → pulse width as % of post-div interval (1–100)

## E.* (Performer-only)

Per-CV envelope (attack → decay → offset) with optional loop and TR hooks.

- `E n x` → set target (raw 0–16383)
- `E.O n x` → set offset/baseline (raw 0–16383)
- `E.A n ms` → set attack time (ms)
- `E.D n ms` → set decay time (ms)
- `E.T n` → trigger envelope
- `E.L n k` → loop k cycles (0 = infinite)
- `E.R n tr` → pulse TR at end of rise (tr = 1–4, 0 = off)
- `E.C n tr` → pulse TR at end of cycle (tr = 1–4, 0 = off)

## RT (Performer-only)

RT reads the current source value for a routing slot (pre-shaper), scaled to Teletype's 0–16383 range.

- `RT n` → read routing slot n (1–16), returns 0–16383

Notes:
- Read-only (no setter).
- Returns the normalized source value (0–1) for that route before shapers and per-track processing.
- Updates every tick at 192 PPQN resolution
- Synchronized across all tracks in the project
- Uses Engine's `measureFraction()` - zero computational overhead

### WP (Which Pattern) - Read Current Pattern Index

**Syntax:** `WP i` where `i` = track number (1-8)

**Returns:** Pattern index (1-16) currently playing on the specified track

**Description:**
WP queries the current pattern index for any track in the Performer project. Each track can be on a different pattern (1-16), and WP allows scripts to respond to the global pattern state across all 8 tracks.

**Setter:** `WP.SET track pattern`
- Both inputs are 1-based (track 1–8, pattern 1–16).

**Examples:**

```
# Query current track's pattern
WP 1           # Returns 1-16 (pattern on track 1)

# Conditional logic based on pattern
IF EQ WP 1 5: CV 1 V 5    # If track 1 on pattern 5, output 5V

# Cross-track pattern detection
IF EQ WP 1 WP 2: TR.PULSE 1  # Pulse when tracks 1 & 2 on same pattern

# Use pattern as modulation source
CV 2 N SCALE WP 3 1 16 60 72   # Map track 3's pattern to note range

# Pattern-dependent randomization
RAND.SEED WP 1             # Seed based on current pattern
CV 3 N RND 60 72           # Pattern-stable random notes

# Sum all track patterns as variation source
X 0
L 1 8: X ADD X WP I        # Sum patterns from all 8 tracks
CV 4 N SCALE X 0 120 48 84 # Map sum to note range

# Detect when any track hits pattern 1
IF OR4 EQ WP 1 1 EQ WP 2 1 EQ WP 3 1 EQ WP 4 1: TR 1 1  # Gate high
```

**Use Cases:**
- **Pattern-aware scripts**: Change behavior based on which patterns are playing
- **Global state detection**: Scripts that respond to pattern combinations
- **Deterministic randomization**: Use pattern index to seed RAND for repeatable variation
- **Cross-track coordination**: Scripts that detect when tracks are on the same pattern
- **Pattern-driven modulation**: Map pattern indices to CV values for variation

**Technical Notes:**
- WP is read-only, use `WP.SET` to change patterns.
- Returns 0 for invalid track indices (< 1 or > 8)
- Track numbers and pattern indices are **1-indexed** (1-8, 1-16)
- Zero computational overhead - reads cached pattern state
- Snapshots use pattern index 16 (WP returns 16 when snapshot is active)

## Performer Teletype Keyboard Shortcuts

The Performer implementation of Teletype includes specific keyboard shortcuts for editing scripts:

### Function Keys (F1-F8)
- **F1-F8**: Select script 0-7 (F1=Script 0, F2=Script 1, etc.)

### Step Keys (Steps 1-16)
- **Steps 1-16**: Insert common teletype commands/variables (cycles through options)
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
  - **Step 14**: CV → IN (cycles through CV→IN)
  - **Step 15**: TR → PARAM (cycles through TR→PARAM)
  - **Step 16**: EVERY → IF (cycles through EVERY→IF)

### Shift + Step Keys (Steps 1-16)
- **Shift + Steps 1-16**: Insert special characters
  - **Shift + Step 1**: + (Plus)
  - **Shift + Step 2**: - (Minus)
  - **Shift + Step 3**: * (Multiply)
  - **Shift + Step 4**: / (Divide)
  - **Shift + Step 5**: % (Modulo)
  - **Shift + Step 6**: = (Equals)
  - **Shift + Step 7**: < (Less than)
  - **Shift + Step 8**: > (Greater than)
  - **Shift + Step 9**: ! (Logical NOT)
  - **Shift + Step 10**: & (Bitwise AND)
  - **Shift + Step 11**: | (Bitwise OR)
  - **Shift + Step 12**: ^ (Bitwise XOR)
  - **Shift + Step 13**: $ (Dollar sign)
  - **Shift + Step 14**: @ (At symbol)
  - **Shift + Step 15**: ? (Question mark)
  - **Shift + Step 16**: ; (Semicolon)

### Navigation and Editing
- **Left Arrow**: Backspace (delete character to the left)
- **Shift + Left Arrow**: Move cursor left
- **Right Arrow**: Insert space
- **Shift + Right Arrow**: Move cursor right
- **Encoder Press**: Load selected line into edit buffer
- **Shift + Encoder Press**: Commit line (save changes)
- **Encoder Turn**: Move cursor left/right (normal) or select line (with Shift)

### Other Controls
- **Page + F4**: Exit script view
- **Page + Function Keys**: Not handled (reserved for other functions)

## Teletype Pattern View Keyboard Shortcuts

The Performer implementation also includes a pattern view for editing teletype patterns:

### Function Keys
- **F1-F4**: Select pattern 0-3 (F1=Pattern 0, F2=Pattern 1, etc.)

### Step Keys (Steps 1-16)
- **Steps 1-9**: Insert digits 1-9 into the currently selected pattern position
- **Step 10 (0)**: Insert digit 0 into the currently selected pattern position
- **Step 14**: Set pattern length to current row position
- **Step 15**: Set pattern start position to current row
- **Step 16**: Set pattern end position to current row

### Navigation and Editing
- **Left Arrow**: Backspace digit (while editing) or delete current row (with Shift)
- **Right Arrow**: Insert new row at current position (with Shift toggles turtle display)
- **Encoder Turn**: Move up/down through pattern rows
- **Encoder Press**: Negate current value (make positive/negative)
- **Shift + Encoder Press**: Commit edited value
- **Page + F4**: Exit pattern view




# Teletype v5.0.0 Documentation

# Introduction

Teletype is a dynamic, musical event triggering platform.

- <a href="https://monome.org/docs/modular/teletype/studies-1"
  target="_blank">Teletype Studies</a> - guided series of tutorials

- <a href="https://monome.org/docs/teletype/TT_commands_3.0.pdf"
  target="_blank">PDF command reference chart</a> —
  <a href="https://monome.org/docs/teletype/TT_scene_RECALL_sheet.pdf"
  target="_blank">PDF scene recall sheet</a> —
  <a href="http://monome.org/docs/teletype/scenes-10/"
  target="_blank">Default scenes</a>

- Current version: *5.0.0* —
  <a href="https://monome.org/docs/modular/update/"
  target="_blank">Firmware update procedure</a>


# Quickstart

## Panel

```text
           _____________________________________________
          |  ( )   TELETYPE                         ( ) |
          |                                             |
          |          1     3     5     7     in         |   IN & PARAM
 TRIGGER  |         (O)   (O)   (O)   (O)    (O)   /--\ |   CV input
 INPUTS   |                                       |    ||
          |          2     4     6     8           \__/ |
          |         (O)   (O)   (O)   (O)         param |
          |                                             |
          |                   o    o    o    o          |
          |                  (O)  (O)  (O)  (O)   ---------- TR OUTPUTS
          |                   A    B    C    D          |
          |                                             |
          |                   o    o    o    o          |
          |                  (O)  (O)  (O)  (O)   ---------- CV OUTPUTS
          |                   1    2    3    4          |
          |                                             |
          |            +-----------------------+        |
          |            |                       |        |
          |            |                       |        |
          |            |                       |        |
          |            |                       |        |
          |            +-----------------------+        |
          |                                             |
 KEYBOARD |   [_____]                (O)                |   SCENE RECALL
          |                                             |
          |__( )___MONOME___________________________( )_|
```

The keyboard is attached to the front panel, for typing commands. The
commands can be executed immediately in *LIVE mode* or assigned to one
of the eight trigger inputs in *EDIT mode*. The knob and in jack can be
used to set and replace values.

## LIVE mode

Teletype starts up in LIVE mode. You’ll see a friendly **&gt;** prompt,
where commands are entered. The command:

    TR.TOG A

will toggle trigger A after pressing enter. Consider:

    CV 1 V 5
    CV 2 N 7
    CV 1 0

Here the first command sets CV 1 to 5 volts. The second command sets CV
2 to note 7 (which is 7 semitones up). The last command sets CV 1 back
to 0.

Data flows from right to left, so it becomes possible to do this:

    CV 1 N RAND 12

Here a random note between 0 and 12 is set to CV 1.

We can change the behavior of a command with a *PRE* such as `DEL`:

    DEL 500 : TR.TOG A

`TR.TOG A` will be delayed by 500ms upon execution.

A helpful display line appears above the command line in dim font. Here
any entered commands will return their numerical value if they have one.

*SCRIPTS*, or several lines of commands, can be assigned to trigger
inputs. This is when things get musically interesting. To edit each
script, we shift into EDIT mode.

### LIVE mode icons

Four small icons are displayed in LIVE mode to give some important
feedback about the state of Teletype. These icons will be brightly lit
when the above is true, else will remain dim. They are, from left to
right:

- Slew: CV outputs are currently slewing to a new destination.
- Delay: Commands are in the delay queue to be executed in the future.
- Stack: Commands are presently on the stack waiting for execution.
- Metro: Metro is currently active and the Metro script is not empty.

## EDIT mode

Toggle between EDIT and LIVE modes by pushing **TAB**.

The prompt now indicates the script you’re currently editing:

- `1`-`8` indicates the script associated with corresponding trigger
- `M` is for the internal metronome
- `I` is the init script, which is executed upon scene recall

Script 1 will be executed when trigger input 1 (top left jack on the
panel) receives a low-to-high voltage transition (trigger, or front edge
of a gate). Consider the following as script 1:

1:

    TR.TOG A

Now when input 1 receives a trigger, `TR.TOG A` is executed, which
toggles the state of output trigger A.

Scripts can have multiple lines:

1:

    TR.TOG A
    CV 1 V RAND 4

Now each time input 1 receives a trigger, CV 1 is set to a random volt
between 0 and 4, in addition to output trigger A being toggled.

### Metronome

The `M` script is driven by an internal metronome, so no external
trigger is required. By default the metronome interval is 1000ms. You
can change this readily (for example, in LIVE mode):

    M 500

The metronome interval is now 500ms. You can disable/enable the
metronome entirely with `M.ACT`:

    M.ACT 0

Now the metronome is off, and the `M` script will not be executed. Set
`M.ACT` to 1 to re-enable.

## Patterns

Patterns facilitate musical data manipulation– lists of numbers that can
be used as sequences, chord sets, rhythms, or whatever you choose.
Pattern memory consists four banks of 64 steps. Functions are provided
for a variety of pattern creation, transformation, and playback. The
most basic method of creating a pattern is by directly adding numbers to
the sequence:

    P.PUSH 5
    P.PUSH 11
    P.PUSH 9
    P.PUSH 3

`P.PUSH` adds the provided value to the end of the list– patterns keep
track of their length, which can be read or modified with `P.L`. Now the
pattern length is 4, and the list looks something like:

    5, 11, 9, 3

Patterns also have an index `P.I`, which could be considered a playhead.
`P.NEXT` will advance the index by one, and return the value stored at
the new index. If the playhead hits the end of the list, it will either
wrap to the beginning (if `P.WRAP` is set to 1, which it is by default)
or simply continue reading at the final position.

So, this script on input 1 would work well:

1:

    CV 1 N P.NEXT

Each time input 1 is triggered, the pattern moves forward one then CV 1
is set to the note value of the pattern at the new index. This is a
basic looped sequence. We could add further control on script 2:

2:

    P.I 0

Since `P.I` is the playhead, trigger input 2 will reset the playhead
back to zero. It won’t change the CV, as that only happens when script 1
is triggered.

We can change a value within the pattern directly:

    P 0 12

This changes index 0 to 12 (it was previously 5), so now we have *12,
11, 9, 3.*

We’ve been working with pattern `0` up to this point. There are four
pattern banks, and we can switch banks this way:

    P.N 1

Now we’re on pattern bank 1. `P.NEXT`, `P.PUSH`, `P`, (and several more
commands) all reference the current pattern bank. Each pattern maintains
its own play index, wrap parameter, length, etc.

We can directly access and change *any* pattern value with the command
`PN`:

    PN 3 0 22

Here the first argument (3) is the *bank*, second (0) is the *index*,
and last is the new value (22). You could do this by doing `P.N 3` then
`P 0 22` but there are cases where a direct read/write is needed in your
patch.

Check the *Command Set* section below for more pattern commands.

Patterns are stored in flash with each scene!

### TRACKER mode

Editing patterns with scripts or from the command line isn’t always
ergonomic. When you’d like to visually edit patterns, TRACKER mode is
the way.

The `TAB` key cycles between LIVE, EDIT and TRACKER mode. You can also
get directly to TRACKER mode by pressing the `NUM LOCK` key. TRACKER
mode is the one with 4 columns of numbers on the Teletype screen.

The current pattern memory is displayed in these columns. Use the arrow
keys to navigate. Holding ALT will jump by pages.

The edit position is indicated by the brightest number. Very dim numbers
indicate they are outside the pattern length.

Use the square bracket keys `[` and `]` to decrease/increase the values.
Backspace sets the value to 0. Entering numbers will overwrite a new
value. You can cut/copy/paste with ALT-X-C-V.

Check the *Keys* section for a complete list of tracker shortcuts.

## Scenes

A *SCENE* is a complete set of scripts and patterns. Stored in flash,
scenes can be saved between sessions. Many scenes ship as examples. On
startup, the last used scene is loaded by Teletype.

Access the SCENE menu using `ESCAPE`. The bracket keys (`[` and `]`)
navigate between the scenes. Use the up/down arrow keys to read the
scene *text*. This text will/should describe what the scene does
generally along with input/output functions. `ENTER` will load the
selected scene, or `ESCAPE` to abort.

To save a scene, hold `ALT` while pushing `ESCAPE`. Use the brackets to
select the destination save position. Edit the text section as usual–
you can scroll down for many lines. The top line is the name of the
scene. `ALT-ENTER` will save the scene to flash.

### Keyboard-less Scene Recall

To facilitate performance without the need for the keyboard, scenes can
be recalled directly from the module’s front panel.

- Press the `SCENE RECALL` button next to the USB jack on the panel.
- Use the `PARAM` knob to highlight your desired preset.
- Hold the `SCENE RECALL` button for 1 second to load the selected
  scene.

### Init Script

The *INIT* script (represented as `I`) is executed when a preset is
recalled. This is a good place to set initial values of variables if
needed, like metro time `M` or time enable `TIME.ACT` for example.

## USB Backup

Teletype’s scenes can be saved and loaded from a USB flash drive. When a
flash drive is inserted, Teletype will recognize it and go into disk
mode. First, all 32 scenes will be written to text files on the drive
with names of the form `tt##s.txt`. For example, scene 5 will be saved
to `tt05s.txt`. The screen will display `WRITE.......` as this is done.

Once complete, Teletype will attempt to read any files named `tt##.txt`
and load them into memory. For example, a file named `tt13.txt` would be
loaded as scene 13 on Teletype. The screen will display `READ......`
Once this process is complete, Teletype will return to LIVE mode and the
drive can be safely removed.

For best results, use an FAT-formatted USB flash drive. If Teletype does
not recognize a disk that is inserted within a few seconds, it may be
best to try another.

An example of possible scenes to load, as well as the set of factory
default scenes, can be found at the
<a href="https://github.com/monome-community/teletype-codex"
target="_blank">Teletype Codex</a>.

## Commands

### Nomenclature

- SCRIPT – multiple *commands*
- COMMAND – a series (one line) of *words*
- WORD – a text string separated by a space: *value*, *operator*,
  *variable*, *mod*
- VALUE – a number
- OPERATOR – a function, may need value(s) as argument(s), may return
  value
- VARIABLE – named memory storage
- MOD – condition/rule that applies to rest of the *command*, e.g.: del,
  prob, if, s

### Syntax

Teletype uses prefix notation. Evaluation happens from right to left.

The left value gets assignment (*set*). Here, temp variable `X` is
assigned zero:

    X 0

Temp variable `Y` is assigned to the value of `X`:

    Y X

`X` is being *read* (*get* `X`), and this value is being used to *set*
`Y`.

Instead of numbers or variables, we can use operators to perform more
complex behavior:

    X TOSS

`TOSS` returns a random state, either 0 or 1 on each call.

Some operators require several arguments:

    X ADD 1 2

Here `ADD` needs two arguments, and gets 1 and 2. `X` is assigned the
result of `ADD`, so `X` is now 3.

If a value is returned at the end of a command, it is printed as a
MESSAGE. This is visible in LIVE mode just above the command prompt. (In
the examples below ignore the // comments).

    8           // prints 8
    X 4
    X           // prints 4
    ADD 8 32    // prints 40

Many parameters are indexed, such as CV and TR. This means that CV and
TR have multiple values (in this case, each has four.) We pass an extra
argument to specify which index we want to read or write.

    CV 1 0

Here CV 1 is set to 0. You can leave off the 0 to print the value.

    CV 1        // prints value of CV 1

Or, this works too:

    X CV 1      // set X to current value of CV 1

Here is an example of using an operator `RAND` to set a random voltage:

    CV 1 V RAND 4

First a random value between 0 and 3 is generated. The result is turned
into a volt with a table lookup, and the final value is assigned to CV
1.

The order of the arguments is important, of course. Consider:

    CV RRAND 1 4 0

`RRAND` uses two arguments, 1 and 4, returning a value between these
two. This command, then, chooses a random CV output (1-4) to set to 0.
This might seem confusing, so it’s possible to clarify it by pulling it
apart:

    X RRAND 1 4
    CV X 0

Here we use `X` as a temp step before setting the final CV.

With some practice it becomes easier to combine many functions into the
same command.

Furthermore, you can use a semicolon to include multiple commands on the
same line:

    X RRAND 1 4; CV X 0

This is particularly useful in **INIT** scripts where you may want to
initialize several values at once:

    A 66; X 101; TR.TIME 1 20;

## Continuing

Don’t forget to checkout the
<a href="https://monome.org/docs/modular/teletype/studies-1"
target="_blank">Teletype Studies</a> for an example-driven guide to the
language.

# Keys

## Global key bindings

These bindings work everywhere.

<table>
<thead>
<tr class="header">
<th><code>Key</code></th>
<th>Action</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>&lt;tab&gt;</code></strong></td>
<td>change modes, live to edit to pattern and back</td>
</tr>
<tr class="even">
<td><strong><code>&lt;esc&gt;</code></strong></td>
<td>preset read mode, or return to last mode</td>
</tr>
<tr class="odd">
<td><strong><code>alt-&lt;esc&gt;</code></strong></td>
<td>preset write mode</td>
</tr>
<tr class="even">
<td><strong><code>win-&lt;esc&gt;</code></strong></td>
<td>clear delays, stack and slews</td>
</tr>
<tr class="odd">
<td><strong><code>shift-alt-?</code></strong> /
<strong><code>alt-h</code></strong></td>
<td>help text, or return to last mode</td>
</tr>
<tr class="even">
<td><strong><code>&lt;F1&gt;</code></strong> to
<strong><code>&lt;F8&gt;</code></strong></td>
<td>run corresponding script</td>
</tr>
<tr class="odd">
<td><strong><code>&lt;F9&gt;</code></strong></td>
<td>run metro script</td>
</tr>
<tr class="even">
<td><strong><code>&lt;F10&gt;</code></strong></td>
<td>run init script</td>
</tr>
<tr class="odd">
<td><strong><code>alt-&lt;F1&gt;</code></strong> to
<strong><code>alt-&lt;F8&gt;</code></strong></td>
<td>edit corresponding script</td>
</tr>
<tr class="even">
<td><strong><code>alt-&lt;F9&gt;</code></strong></td>
<td>edit metro script</td>
</tr>
<tr class="odd">
<td><strong><code>alt-&lt;F10&gt;</code></strong></td>
<td>edit init script</td>
</tr>
<tr class="even">
<td><strong><code>ctrl-&lt;F1&gt;</code></strong> to
<strong><code>ctrl-&lt;F8&gt;</code></strong></td>
<td>mute/unmute corresponding script</td>
</tr>
<tr class="odd">
<td><strong><code>ctrl-&lt;F9&gt;</code></strong></td>
<td>enable/disable metro script</td>
</tr>
<tr class="even">
<td><strong><code>&lt;numpad-1&gt;</code></strong> to
<strong><code>&lt;numpad-8&gt;</code></strong></td>
<td>run corresponding script</td>
</tr>
<tr class="odd">
<td><strong><code>&lt;num lock&gt;</code></strong> /
<strong><code>&lt;F11&gt;</code></strong></td>
<td>jump to pattern mode</td>
</tr>
<tr class="even">
<td><strong><code>&lt;print screen&gt;</code></strong> /
<strong><code>&lt;F12&gt;</code></strong></td>
<td>jump to live mode</td>
</tr>
</tbody>
</table>

## Text editing

These bindings work when entering text or code.

In most cases, the clipboard is shared between *live*, *edit* and the 2
*preset* modes.

<table>
<thead>
<tr class="header">
<th><code>Key</code></th>
<th>Action</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>&lt;left&gt;</code></strong> /
<strong><code>ctrl-b</code></strong></td>
<td>move cursor left</td>
</tr>
<tr class="even">
<td><strong><code>&lt;right&gt;</code></strong> /
<strong><code>ctrl-f</code></strong></td>
<td>move cursor right</td>
</tr>
<tr class="odd">
<td><strong><code>ctrl-&lt;left&gt;</code></strong> /
<strong><code>alt-b</code></strong></td>
<td>move left by one word</td>
</tr>
<tr class="even">
<td><strong><code>ctrl-&lt;right&gt;</code></strong> /
<strong><code>alt-f</code></strong></td>
<td>move right by one word</td>
</tr>
<tr class="odd">
<td><strong><code>&lt;home&gt;</code></strong> /
<strong><code>ctrl-a</code></strong></td>
<td>move to beginning of line</td>
</tr>
<tr class="even">
<td><strong><code>&lt;end&gt;</code></strong> /
<strong><code>ctrl-e</code></strong></td>
<td>move to end of line</td>
</tr>
<tr class="odd">
<td><strong><code>&lt;backspace&gt;</code></strong> /
<strong><code>ctrl-h</code></strong></td>
<td>backwards delete one character</td>
</tr>
<tr class="even">
<td><strong><code>&lt;delete&gt;</code></strong> /
<strong><code>ctrl-d</code></strong></td>
<td>forwards delete one character</td>
</tr>
<tr class="odd">
<td><strong><code>shift-&lt;backspace&gt;</code></strong> /
<strong><code>ctrl-u</code></strong></td>
<td>delete from cursor to beginning</td>
</tr>
<tr class="even">
<td><strong><code>shift-&lt;delete&gt;</code></strong> /
<strong><code>ctrl-k</code></strong></td>
<td>delete from cursor to end</td>
</tr>
<tr class="odd">
<td><strong><code>alt-&lt;backspace&gt;</code></strong> /
<strong><code>ctrl-w</code></strong></td>
<td>delete from cursor to beginning of word</td>
</tr>
<tr class="even">
<td><strong><code>alt-d</code></strong></td>
<td>delete from cursor to end of word</td>
</tr>
<tr class="odd">
<td><strong><code>ctrl-x</code></strong> /
<strong><code>alt-x</code></strong></td>
<td>cut to clipboard</td>
</tr>
<tr class="even">
<td><strong><code>ctrl-c</code></strong> /
<strong><code>alt-c</code></strong></td>
<td>copy to clipboard</td>
</tr>
<tr class="odd">
<td><strong><code>ctrl-v</code></strong> /
<strong><code>alt-v</code></strong></td>
<td>paste to clipboard</td>
</tr>
</tbody>
</table>

## Live mode

<table>
<thead>
<tr class="header">
<th><code>Key</code></th>
<th>Action</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>&lt;down&gt;</code></strong> /
<strong><code>C-n</code></strong></td>
<td>history next</td>
</tr>
<tr class="even">
<td><strong><code>&lt;up&gt;</code></strong> /
<strong><code>C-p</code></strong></td>
<td>history previous</td>
</tr>
<tr class="odd">
<td><strong><code>&lt;enter&gt;</code></strong></td>
<td>execute command</td>
</tr>
<tr class="even">
<td><strong><code>~</code></strong></td>
<td>toggle variables</td>
</tr>
<tr class="odd">
<td><strong><code>[</code></strong> /
<strong><code>]</code></strong></td>
<td>switch to edit mode</td>
</tr>
<tr class="even">
<td><strong><code>alt-g</code></strong></td>
<td>toggle grid visualizer</td>
</tr>
<tr class="odd">
<td><strong><code>shift-d</code></strong></td>
<td>live dashboard</td>
</tr>
<tr class="even">
<td><strong><code>alt-&lt;arrows&gt;</code></strong></td>
<td>move grid cursor</td>
</tr>
<tr class="odd">
<td><strong><code>alt-shift-&lt;arrows&gt;</code></strong></td>
<td>select grid area</td>
</tr>
<tr class="even">
<td><strong><code>alt-&lt;space&gt;</code></strong></td>
<td>emulate grid press</td>
</tr>
<tr class="odd">
<td><strong><code>alt-/</code></strong></td>
<td>switch grid pages</td>
</tr>
<tr class="even">
<td><strong><code>alt-\</code></strong></td>
<td>toggle grid control view</td>
</tr>
<tr class="odd">
<td><strong><code>alt-&lt;prt sc&gt;</code></strong></td>
<td>insert grid x/y/w/h</td>
</tr>
</tbody>
</table>

In full grid visualizer mode pressing `alt` is not required.

## Edit mode

In *edit* mode multiple lines can be selected and used with the
clipboard.

<table>
<thead>
<tr class="header">
<th><code>Key</code></th>
<th>Action</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>&lt;down&gt;</code></strong> /
<strong><code>C-n</code></strong></td>
<td>line down</td>
</tr>
<tr class="even">
<td><strong><code>&lt;up&gt;</code></strong> /
<strong><code>C-p</code></strong></td>
<td>line up</td>
</tr>
<tr class="odd">
<td><strong><code>[</code></strong></td>
<td>previous script</td>
</tr>
<tr class="even">
<td><strong><code>]</code></strong></td>
<td>next script</td>
</tr>
<tr class="odd">
<td><strong><code>&lt;enter&gt;</code></strong></td>
<td>enter command</td>
</tr>
<tr class="even">
<td><strong><code>shift-&lt;enter&gt;</code></strong></td>
<td>insert command</td>
</tr>
<tr class="odd">
<td><strong><code>alt-/</code></strong></td>
<td>toggle line comment</td>
</tr>
<tr class="even">
<td><strong><code>shift-&lt;up&gt;</code></strong></td>
<td>expand selection up</td>
</tr>
<tr class="odd">
<td><strong><code>shift-&lt;down&gt;</code></strong></td>
<td>expand selection down</td>
</tr>
<tr class="even">
<td><strong><code>alt-&lt;delete&gt;</code></strong></td>
<td>delete selection</td>
</tr>
<tr class="odd">
<td><strong><code>alt-&lt;up&gt;</code></strong></td>
<td>move selection up</td>
</tr>
<tr class="even">
<td><strong><code>alt-&lt;down&gt;</code></strong></td>
<td>move selection down</td>
</tr>
<tr class="odd">
<td><strong><code>ctrl-z</code></strong></td>
<td>undo (3 levels)</td>
</tr>
</tbody>
</table>

## Tracker mode

The tracker mode clipboard is independent of text and code clipboard.

<table style="width:99%;">
<colgroup>
<col style="width: 22%" />
<col style="width: 77%" />
</colgroup>
<thead>
<tr class="header">
<th><code>Key</code></th>
<th>Action</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>&lt;down&gt;</code></strong></td>
<td>move down</td>
</tr>
<tr class="even">
<td><strong><code>alt-&lt;down&gt;</code></strong></td>
<td>move a page down</td>
</tr>
<tr class="odd">
<td><strong><code>&lt;up&gt;</code></strong></td>
<td>move up</td>
</tr>
<tr class="even">
<td><strong><code>alt-&lt;up&gt;</code></strong></td>
<td>move a page up</td>
</tr>
<tr class="odd">
<td><strong><code>&lt;left&gt;</code></strong></td>
<td>move left</td>
</tr>
<tr class="even">
<td><strong><code>alt-&lt;left&gt;</code></strong></td>
<td>move to the very left</td>
</tr>
<tr class="odd">
<td><strong><code>&lt;right&gt;</code></strong></td>
<td>move right</td>
</tr>
<tr class="even">
<td><strong><code>alt-&lt;right&gt;</code></strong></td>
<td>move to the very right</td>
</tr>
<tr class="odd">
<td><strong><code>[</code></strong></td>
<td>decrement by 1</td>
</tr>
<tr class="even">
<td><strong><code>]</code></strong></td>
<td>increment by 1</td>
</tr>
<tr class="odd">
<td><strong><code>alt-[</code></strong></td>
<td>decrement by 1 semitone</td>
</tr>
<tr class="even">
<td><strong><code>alt-]</code></strong></td>
<td>increment by 1 semitone</td>
</tr>
<tr class="odd">
<td><strong><code>ctrl-[</code></strong></td>
<td>decrement by 7 semitones</td>
</tr>
<tr class="even">
<td><strong><code>ctrl-]</code></strong></td>
<td>increment by 7 semitones</td>
</tr>
<tr class="odd">
<td><strong><code>shift-[</code></strong></td>
<td>decrement by 12 semitones</td>
</tr>
<tr class="even">
<td><strong><code>shift-]</code></strong></td>
<td>increment by 12 semitones</td>
</tr>
<tr class="odd">
<td><strong><code>alt-&lt;0-9&gt;</code></strong></td>
<td>increment by <code>&lt;0-9&gt;</code> semitones (0=10, 1=11)</td>
</tr>
<tr class="even">
<td><strong><code>shift-alt-&lt;0-9&gt;</code></strong></td>
<td>decrement by <code>&lt;0-9&gt;</code> semitones (0=10, 1=11)</td>
</tr>
<tr class="odd">
<td><strong><code>&lt;backspace&gt;</code></strong></td>
<td>delete a digit</td>
</tr>
<tr class="even">
<td><strong><code>shift-&lt;backspace&gt;</code></strong></td>
<td>delete an entry, shift numbers up</td>
</tr>
<tr class="odd">
<td><strong><code>&lt;enter&gt;</code></strong></td>
<td>commit edit (increase length if cursor in position after last
entry)</td>
</tr>
<tr class="even">
<td><strong><code>shift-&lt;enter&gt;</code></strong></td>
<td>commit edit, then duplicate entry and shift downwards (increase
length as <code>&lt;enter&gt;</code>)</td>
</tr>
<tr class="odd">
<td><strong><code>alt-x</code></strong></td>
<td>cut value (n.b. <code>ctrl-x</code> not supported)</td>
</tr>
<tr class="even">
<td><strong><code>alt-c</code></strong></td>
<td>copy value (n.b. <code>ctrl-c</code> not supported)</td>
</tr>
<tr class="odd">
<td><strong><code>alt-v</code></strong></td>
<td>paste value (n.b. <code>ctrl-v</code> not supported)</td>
</tr>
<tr class="even">
<td><strong><code>shift-alt-v</code></strong></td>
<td>insert value</td>
</tr>
<tr class="odd">
<td><strong><code>shift-l</code></strong></td>
<td>set length to current position</td>
</tr>
<tr class="even">
<td><strong><code>alt-l</code></strong></td>
<td>go to current length entry</td>
</tr>
<tr class="odd">
<td><strong><code>shift-s</code></strong></td>
<td>set start to current position</td>
</tr>
<tr class="even">
<td><strong><code>alt-s</code></strong></td>
<td>go to start entry</td>
</tr>
<tr class="odd">
<td><strong><code>shift-e</code></strong></td>
<td>set end to current position</td>
</tr>
<tr class="even">
<td><strong><code>alt-e</code></strong></td>
<td>go to end entry</td>
</tr>
<tr class="odd">
<td><strong><code>-</code></strong></td>
<td>negate value</td>
</tr>
<tr class="even">
<td><strong><code>&lt;space&gt;</code></strong></td>
<td>toggle non-zero to zero, and zero to 1</td>
</tr>
<tr class="odd">
<td><strong><code>0</code></strong> to
<strong><code>9</code></strong></td>
<td>numeric entry</td>
</tr>
<tr class="even">
<td><strong><code>shift-2</code></strong> (<code>@</code>)</td>
<td>toggle turtle display marker (<code>&lt;</code>)</td>
</tr>
<tr class="odd">
<td><strong><code>ctrl-alt</code></strong></td>
<td>insert knob value scaled to 0..31</td>
</tr>
<tr class="even">
<td><strong><code>ctrl-shift</code></strong></td>
<td>insert knob value scaled to 0..1023</td>
</tr>
</tbody>
</table>

## Preset read mode

<table>
<thead>
<tr class="header">
<th><code>Key</code></th>
<th>Action</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>&lt;down&gt;</code></strong> /
<strong><code>C-n</code></strong></td>
<td>line down</td>
</tr>
<tr class="even">
<td><strong><code>&lt;up&gt;</code></strong> /
<strong><code>C-p</code></strong></td>
<td>line up</td>
</tr>
<tr class="odd">
<td><strong><code>&lt;left&gt;</code></strong> /
<strong><code>[</code></strong></td>
<td>preset down</td>
</tr>
<tr class="even">
<td><strong><code>&lt;right&gt;</code></strong> /
<strong><code>]</code></strong></td>
<td>preset up</td>
</tr>
<tr class="odd">
<td><strong><code>&lt;enter&gt;</code></strong></td>
<td>load preset</td>
</tr>
</tbody>
</table>

## Preset write mode

<table>
<thead>
<tr class="header">
<th><code>Key</code></th>
<th>Action</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>&lt;down&gt;</code></strong> /
<strong><code>C-n</code></strong></td>
<td>line down</td>
</tr>
<tr class="even">
<td><strong><code>&lt;up&gt;</code></strong> /
<strong><code>C-p</code></strong></td>
<td>line up</td>
</tr>
<tr class="odd">
<td><strong><code>[</code></strong></td>
<td>preset down</td>
</tr>
<tr class="even">
<td><strong><code>]</code></strong></td>
<td>preset up</td>
</tr>
<tr class="odd">
<td><strong><code>&lt;enter&gt;</code></strong></td>
<td>enter text</td>
</tr>
<tr class="even">
<td><strong><code>shift-&lt;enter&gt;</code></strong></td>
<td>insert text</td>
</tr>
<tr class="odd">
<td><strong><code>alt-&lt;enter&gt;</code></strong></td>
<td>save preset</td>
</tr>
</tbody>
</table>

## Help mode

<table>
<thead>
<tr class="header">
<th><code>Key</code></th>
<th>Action</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>&lt;down&gt;</code></strong> /
<strong><code>C-n</code></strong></td>
<td>line down</td>
</tr>
<tr class="even">
<td><strong><code>&lt;up&gt;</code></strong> /
<strong><code>C-p</code></strong></td>
<td>line up</td>
</tr>
<tr class="odd">
<td><strong><code>&lt;left&gt;</code></strong> /
<strong><code>[</code></strong></td>
<td>previous page</td>
</tr>
<tr class="even">
<td><strong><code>&lt;right&gt;</code></strong> /
<strong><code>]</code></strong></td>
<td>next page</td>
</tr>
<tr class="odd">
<td><strong><code>C-f</code></strong> /
<strong><code>C-s</code></strong></td>
<td>search forward</td>
</tr>
<tr class="even">
<td><strong><code>C-r</code></strong></td>
<td>search backward</td>
</tr>
</tbody>
</table>

# OPs and MODs

## Variables

General purpose temp vars: `X`, `Y`, `Z`, and `T`.

`T` typically used for time values, but can be used freely.

`A`-`D` are assigned 1-4 by default (as a convenience for TR labeling,
but TR can be addressed with simply 1-4). All may be overwritten and
used freely.

<table style="width:98%;">
<colgroup>
<col style="width: 25%" />
<col style="width: 23%" />
<col style="width: 10%" />
<col style="width: 40%" />
</colgroup>
<thead>
<tr class="header">
<th><code>OP</code></th>
<th><code>OP (set)</code></th>
<th><code>Alias</code></th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>A</code></strong></td>
<td><strong><code>A x</code></strong></td>
<td>-</td>
<td>get / set the variable <code>A</code>, default <code>1</code></td>
</tr>
<tr class="even">
<td><strong><code>B</code></strong></td>
<td><strong><code>B x</code></strong></td>
<td>-</td>
<td>get / set the variable <code>B</code>, default <code>2</code></td>
</tr>
<tr class="odd">
<td><strong><code>C</code></strong></td>
<td><strong><code>C x</code></strong></td>
<td>-</td>
<td>get / set the variable <code>C</code>, default <code>3</code></td>
</tr>
<tr class="even">
<td><strong><code>D</code></strong></td>
<td><strong><code>D x</code></strong></td>
<td>-</td>
<td>get / set the variable <code>D</code>, default <code>4</code></td>
</tr>
<tr class="odd">
<td><strong><code>FLIP</code></strong></td>
<td><strong><code>FLIP x</code></strong></td>
<td>-</td>
<td>returns the opposite of its previous state (<code>0</code> or
<code>1</code>) on each read (also settable)</td>
</tr>
<tr class="even">
<td><strong><code>I</code></strong></td>
<td><strong><code>I x</code></strong></td>
<td>-</td>
<td>Get / set the variable <code>I</code>. This variable is overwritten
by <code>L</code>, but can be used<br />
freely outside an <code>L</code> loop. Each script gets its own
<code>I</code> variable, so if you call<br />
a script from another script’s loop you can still use and modify
<code>I</code> without<br />
affecting the calling loop. In this scenario the script getting called
will have<br />
its <code>I</code> value initialized with the calling loop’s current
<code>I</code> value.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>J</code></strong></td>
<td><strong><code>J x</code></strong></td>
<td>-</td>
<td>get / set the variable <code>J</code>, each script gets its own
<code>J</code> variable, so if you call<br />
a script from another script you can still use and modify <code>J</code>
without affecting the calling script.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>K</code></strong></td>
<td><strong><code>K x</code></strong></td>
<td>-</td>
<td>get / set the variable <code>K</code>, each script gets its own
<code>K</code> variable, so if you call<br />
a script from another script you can still use and modify <code>K</code>
without affecting the calling script.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>O</code></strong></td>
<td><strong><code>O x</code></strong></td>
<td>-</td>
<td>Auto-increments by <code>O.INC</code> <em>after</em> each access.
The initial value is <code>0</code>. The<br />
lower and upper bounds can be set by <code>O.MIN</code> (default
<code>0</code>) and <code>O.MAX</code><br />
(default <code>63</code>). <code>O.WRAP</code> controls if the value
wraps when it reaches a bound<br />
(default is <code>1</code>).<br />
<br />
Example:<br />
<br />
&#10;<pre><code>O           =&gt; 0
O           =&gt; 1
X O
X           =&gt; 2
O.INC 2
O           =&gt; 3 (O increments after it&#39;s accessed)
O           =&gt; 5
O.INC -2
O 2
O           =&gt; 2
O           =&gt; 0
O           =&gt; 63
O           =&gt; 61</code></pre>
<br />
</td>
</tr>
<tr class="even">
<td><strong><code>O.INC</code></strong></td>
<td><strong><code>O.INC x</code></strong></td>
<td>-</td>
<td>how much to increment <code>O</code> by on each invocation, default
<code>1</code></td>
</tr>
<tr class="odd">
<td><strong><code>O.MIN</code></strong></td>
<td><strong><code>O.MIN x</code></strong></td>
<td>-</td>
<td>the lower bound for <code>O</code>, default <code>0</code></td>
</tr>
<tr class="even">
<td><strong><code>O.MAX</code></strong></td>
<td><strong><code>O.MAX x</code></strong></td>
<td>-</td>
<td>the upper bound for <code>O</code>, default <code>63</code></td>
</tr>
<tr class="odd">
<td><strong><code>O.WRAP</code></strong></td>
<td><strong><code>O.WRAP x</code></strong></td>
<td>-</td>
<td>should <code>O</code> wrap when it reaches its bounds, default
<code>1</code></td>
</tr>
<tr class="even">
<td><strong><code>T</code></strong></td>
<td><strong><code>T x</code></strong></td>
<td>-</td>
<td>get / set the variable <code>T</code>, typically used for time,
default <code>0</code></td>
</tr>
<tr class="odd">
<td><strong><code>TIME</code></strong></td>
<td><strong><code>TIME x</code></strong></td>
<td>-</td>
<td>timer value, counts up in ms., wraps after 32s, can be set</td>
</tr>
<tr class="even">
<td><strong><code>TIME.ACT</code></strong></td>
<td><strong><code>TIME.ACT x</code></strong></td>
<td>-</td>
<td>enable or disable timer counting, default <code>1</code></td>
</tr>
<tr class="odd">
<td><strong><code>LAST x</code></strong></td>
<td>-</td>
<td>-</td>
<td>Gets the number of milliseconds since the given script was run,
where <code>M</code> is script 9 and <code>I</code> is script 10. From
the live mode, <code>LAST SCRIPT</code> gives the time elapsed since
last run of I script.<br />
<br />
For example, one-line tap tempo:<br />
<br />
&#10;<pre><code>M LAST SCRIPT</code></pre>
<br />
<br />
Running this script twice will set the metronome to be the time between
runs.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>X</code></strong></td>
<td><strong><code>X x</code></strong></td>
<td>-</td>
<td>get / set the variable <code>X</code>, default <code>0</code></td>
</tr>
<tr class="odd">
<td><strong><code>Y</code></strong></td>
<td><strong><code>Y x</code></strong></td>
<td>-</td>
<td>get / set the variable <code>Y</code>, default <code>0</code></td>
</tr>
<tr class="even">
<td><strong><code>Z</code></strong></td>
<td><strong><code>Z x</code></strong></td>
<td>-</td>
<td>get / set the variable <code>Z</code>, default <code>0</code></td>
</tr>
</tbody>
</table>

## Hardware

The Teletype trigger inputs are numbered 1-8, the CV and trigger outputs
1-4. See the Ansible documentation for details of the Ansible output
numbering when in Teletype mode.

<table style="width:98%;">
<colgroup>
<col style="width: 25%" />
<col style="width: 23%" />
<col style="width: 10%" />
<col style="width: 40%" />
</colgroup>
<thead>
<tr class="header">
<th><code>OP</code></th>
<th><code>OP (set)</code></th>
<th><code>Alias</code></th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>CV x</code></strong></td>
<td><strong><code>CV x y</code></strong></td>
<td>-</td>
<td>Get the value of CV associated with output <code>x</code>, or set
the CV output of <code>x</code> to<br />
<code>y</code>.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>CV.OFF x</code></strong></td>
<td><strong><code>CV.OFF x y</code></strong></td>
<td>-</td>
<td>Get the value of the offset added to the CV value at output
<code>x</code>. The offset is<br />
added at the final stage. Set the value of the offset added to the CV
value at<br />
output <code>x</code> to <code>y</code>.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>CV.SET x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>Set the CV value at output <code>x</code> bypassing any slew
settings.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>CV.GET x</code></strong></td>
<td>-</td>
<td>-</td>
<td>Get the current CV value at output <code>x</code> with slew and
offset applied.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>CV.SLEW x</code></strong></td>
<td><strong><code>CV.SLEW x y</code></strong></td>
<td>-</td>
<td>Get the slew time in ms associated with CV output <code>x</code>.
Set the slew time<br />
associated with CV output <code>x</code> to <code>y</code> ms.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>V x</code></strong></td>
<td>-</td>
<td>-</td>
<td>converts a voltage to a value usable by the CV outputs
(<code>x</code> between <code>0</code> and <code>10</code>)</td>
</tr>
<tr class="odd">
<td><strong><code>VV x</code></strong></td>
<td>-</td>
<td>-</td>
<td>converts a voltage to a value usable by the CV outputs
(<code>x</code> between <code>0</code> and <code>1000</code>,
<code>100</code> represents 1V)</td>
</tr>
<tr class="even">
<td><strong><code>IN</code></strong></td>
<td>-</td>
<td>-</td>
<td>Get the value of the IN jack. This returns a valuue in the range
0-16383.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>IN.SCALE min max</code></strong></td>
<td>-</td>
<td>-</td>
<td>Set static scaling of the <code>IN</code> CV to between
<code>min</code> and <code>max</code>.</td>
</tr>
<tr class="even">
<td><strong><code>PARAM</code></strong></td>
<td>-</td>
<td><strong><code>PRM</code></strong></td>
<td>Get the value of the PARAM knob. This returns a valuue in the range
0-16383.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>PARAM.SCALE min max</code></strong></td>
<td>-</td>
<td>-</td>
<td>Set static scaling of the PARAM knob to between <code>min</code> and
<code>max</code>.</td>
</tr>
<tr class="even">
<td><strong><code>TR x</code></strong></td>
<td><strong><code>TR x y</code></strong></td>
<td>-</td>
<td>Get the current state of trigger output <code>x</code>. Set the
state of trigger<br />
output <code>x</code> to <code>y</code> (0-1).<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>TR.PULSE x</code></strong></td>
<td>-</td>
<td><strong><code>TR.P</code></strong></td>
<td>Pulse trigger output x.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>TR.TIME x</code></strong></td>
<td><strong><code>TR.TIME x y</code></strong></td>
<td>-</td>
<td>Get the pulse time of trigger output <code>x</code>. Set the pulse
time of trigger<br />
output <code>x</code> to <code>y</code>ms.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>TR.TOG x</code></strong></td>
<td>-</td>
<td>-</td>
<td>Flip the state of trigger output <code>x</code>.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>TR.POL x</code></strong></td>
<td><strong><code>TR.POL x y</code></strong></td>
<td>-</td>
<td>Get the current polarity of trigger output <code>x</code>. Set the
polarity of trigger<br />
output <code>x</code> to <code>y</code> (0-1). When TR.POL = 1, the
pulse is 0 to 1 then back to 0.<br />
When TR.POL = 0, the inverse is true, 1 to 0 to 1.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>MUTE x</code></strong></td>
<td><strong><code>MUTE x y</code></strong></td>
<td>-</td>
<td>Mute the trigger input on <code>x</code> (1-8) when <code>y</code>
is non-zero.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>STATE x</code></strong></td>
<td>-</td>
<td>-</td>
<td>Read the current state of trigger input <code>x</code> (0=low,
1=high).<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>LIVE.OFF</code></strong></td>
<td>-</td>
<td><strong><code>LIVE.O</code></strong></td>
<td>Show the default live mode screen</td>
</tr>
<tr class="even">
<td><strong><code>LIVE.VARS</code></strong></td>
<td>-</td>
<td><strong><code>LIVE.V</code></strong></td>
<td>Show variables in live mode</td>
</tr>
<tr class="odd">
<td><strong><code>LIVE.GRID</code></strong></td>
<td>-</td>
<td><strong><code>LIVE.G</code></strong></td>
<td>Show grid visualizer in live mode</td>
</tr>
<tr class="even">
<td><strong><code>LIVE.DASH x</code></strong></td>
<td>-</td>
<td><strong><code>LIVE.D</code></strong></td>
<td>This allows you to show custom text and print values on the live
mode screen.<br />
To create a dashboard, simply edit the scene description. You can
define<br />
multiple dashboards by separating them with <code>===</code>, and you
can select them<br />
by specifying the dashboard number as the op parameter.<br />
<br />
You can also print up to 16 values using <code>PRINT</code> op. To
create a placeholder<br />
for a value, place <code>%##</code> where you want the number to be,
where <code>##</code> is<br />
a value index between 1 and 16. Please note: if you define multiple
placeholders<br />
for the same value, only the last one will be used, and the rest will
be<br />
treated as plain text. By default, values are printed in decimal
format,<br />
but you can also use hex, binary and reversed binary formats by using
<code>%X##</code>,<br />
<code>%B##</code> and <code>%R##</code> placeholders respectively.<br />
<br />
An example of a dashboard:<br />
<br />
&#10;<pre><code>THIS IS A DASHBOARD
&#10;CURRENT METRO RATE IS: %1</code></pre>
<br />
<br />
You can use this dashboard by entering the above in a scene
description,<br />
placing <code>LIVE.DASH 1</code> in the init script and placing
<code>PRINT 1 M</code> in the metro script.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>PRINT x</code></strong></td>
<td><strong><code>PRINT x y</code></strong></td>
<td><strong><code>PRT</code></strong></td>
<td>This op allows you to display up to 16 values on a live mode
dashboard and<br />
should be used in conjunction with <code>LIVE.DASH</code> op. See
<code>LIVE.DASH</code><br />
description for information on how to use it. You can also use this
op<br />
to store up to 16 additional values.<br />
</td>
</tr>
</tbody>
</table>

## Pitch

Mathematical calcuations and tables helpful for musical pitch.

<table style="width:98%;">
<colgroup>
<col style="width: 25%" />
<col style="width: 23%" />
<col style="width: 10%" />
<col style="width: 40%" />
</colgroup>
<thead>
<tr class="header">
<th><code>OP</code></th>
<th><code>OP (set)</code></th>
<th><code>Alias</code></th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>HZ x</code></strong></td>
<td>-</td>
<td>-</td>
<td>converts 1V/OCT value <code>x</code> to Hz/Volt value, useful for
controlling non-euro synths like Korg MS-20</td>
</tr>
<tr class="even">
<td><strong><code>JI x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>just intonation helper, precision ratio divider normalised to
1V</td>
</tr>
<tr class="odd">
<td><strong><code>N x</code></strong></td>
<td>-</td>
<td>-</td>
<td>The <code>N</code> OP converts an equal temperament note number to a
value usable by the CV outputs.<br />
<br />
Examples:<br />
<br />
&#10;<pre><code>CV 1 N 60        =&gt; set CV 1 to middle C, i.e. 5V
CV 1 N RAND 24   =&gt; set CV 1 to a random note from the lowest 2 octaves</code></pre>
<br />
</td>
</tr>
<tr class="even">
<td><strong><code>N.S r s d</code></strong></td>
<td>-</td>
<td>-</td>
<td>The <code>N.S</code> OP lets you retrieve <code>N</code> table
values according to traditional western scales. <code>s</code> and
<code>d</code> wrap to their ranges automatically and support negative
indexing.<br />
<br />
Scales<br />
- <code>0</code> = Major<br />
- <code>1</code> = Natural Minor<br />
- <code>2</code> = Harmonic Minor<br />
- <code>3</code> = Melodic Minor<br />
- <code>4</code> = Dorian<br />
- <code>5</code> = Phrygian<br />
- <code>6</code> = Lydian<br />
- <code>7</code> = Mixolydian<br />
- <code>8</code> = Locrian<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>N.C r c d</code></strong></td>
<td>-</td>
<td>-</td>
<td>The <code>N.C</code> OP lets you retrieve <code>N</code> table
values according to traditional western chords. <code>c</code> and
<code>d</code> wrap to their ranges automatically and support negative
indexing.<br />
<br />
Chords<br />
- <code>0</code> = Major 7th <code>{0, 4, 7, 11}</code><br />
- <code>1</code> = Minor 7th <code>{0, 3, 7, 10}</code><br />
- <code>2</code> = Dominant 7th <code>{0, 4, 7, 10}</code><br />
- <code>3</code> = Diminished 7th <code>{0, 3, 6, 9}</code><br />
- <code>4</code> = Augmented 7th <code>{0, 4, 8, 10}</code><br />
- <code>5</code> = Dominant 7b5 <code>{0, 4, 6, 10}</code><br />
- <code>6</code> = Minor 7b5 <code>{0, 3, 6, 10}</code><br />
- <code>7</code> = Major 7#5 <code>{0, 4, 8, 11}</code><br />
- <code>8</code> = Minor Major 7th <code>{0, 3, 7, 11}</code><br />
- <code>9</code> = Diminished Major 7th <code>{0, 3, 6, 11}</code><br />
- <code>10</code> = Major 6th <code>{0, 4, 7, 9}</code><br />
- <code>11</code> = Minor 6th <code>{0, 3, 7, 9}</code><br />
- <code>12</code> = 7sus4 <code>{0, 5, 7, 10}</code><br />
</td>
</tr>
<tr class="even">
<td><strong><code>N.CS r s d c</code></strong></td>
<td>-</td>
<td>-</td>
<td>The <code>N.CS</code> OP lets you retrieve <code>N</code> table
values according to traditional western scales and chords.
<code>s</code>, <code>c</code> and <code>d</code> wrap to their ranges
automatically and support negative indexing.<br />
<br />
Chord Scales - Refer to chord indices in <code>N.C</code> OP<br />
- <code>0</code> = Major <code>{0, 1, 1, 0, 2, 1, 6}</code><br />
- <code>1</code> = Natural Minor
<code>{1, 6, 0, 1, 1, 0, 2}</code><br />
- <code>2</code> = Harmonic Minor
<code>{8, 6, 7, 1, 2, 0, 3}</code><br />
- <code>3</code> = Melodic Minor
<code>{8, 1, 7, 2, 2, 6, 6}</code><br />
- <code>4</code> = Dorian <code>{1, 1, 0, 2, 1, 6, 0}</code><br />
- <code>5</code> = Phrygian <code>{1, 0, 2, 1, 6, 0, 1}</code><br />
- <code>6</code> = Lydian <code>{0, 2, 1, 6, 0, 1, 1}</code><br />
- <code>7</code> = Mixolydian <code>{2, 1, 6, 0, 1, 1, 0}</code><br />
- <code>8</code> = Locrian <code>{6, 0, 1, 1, 0, 2, 1}</code><br />
</td>
</tr>
<tr class="odd">
<td><strong><code>N.B d</code></strong></td>
<td><strong><code>N.B r s</code></strong></td>
<td>-</td>
<td>Converts a degree in a user-defined equal temperament scale to a
value usable by the CV outputs. Default values of <code>r</code> and
<code>s</code> are 0 and R101011010101, corresponding to C-major.<br />
To make it easier to generate bit-masks in code, LSB (bit 0) represent
the first note in the octave. To avoid having to mirror scales in our
heads when entering them by hand, we use <code>R...</code> (reverse
binary) instead of <code>B...</code> (binary ).<br />
<br />
The bit-masks uses the 12 lower bits.<br />
<br />
Note that N.B is using scale at index 0 as used by N.BX ,so N.B and N.BX
0 are equivalent.<br />
<br />
Examples:<br />
&#10;<pre><code>CV 1 N.B 1            ==&gt;  set CV 1 to 1st degree of default scale
                           (C, value corresponding to N 0)
N.B 0 R101011010101   ==&gt;  set scale to C-major (default)
CV 1 N.B 1            ==&gt;  set CV 1 get 1st degree of scale
                           (C, value corresponding to N 0)
N.B 2 R101011010101   ==&gt;  set scale to D-major
CV 1 N.B 3            ==&gt;  set CV 1 to 3rd degree of scale
                           (F#, value corresponding to N 6)
N.B 3 R100101010010   ==&gt;  set scale to Eb-minor pentatonic
CV 1 N.B 2            ==&gt;  set CV 1 to 2nd degree of scale 
                           (Gb, value corresponding to N 6)
N.B 5 -3              ==&gt;  set scale to F-lydian using preset</code></pre>
<br />
Values of <code>s</code> less than 1 sets the bit mask to a preset
scale:<br />
&#10;<pre><code>0:   Ionian (major)
-1:  Dorian
-2:  Phrygian
-3:  Lydian
-4:  Mixolydian
-5:  Aeolean (natural minor)
-6:  Locrian
-7:  Melodic minor
-8:  Harmonic minor
-9:  Major pentatonic
-10: Minor pentatonic
-11  Whole note (1st Messiaen mode)
-12  Octatonic (half-whole, 2nd Messiaen mode)
-13  Octatonic (whole-half)
-14  3rd Messiaen mode
-15  4th Messiaen mode
-16  5th Messiaen mode
-17  6th Messiaen mode
-18  7th Messiaen mode
-19  Augmented</code></pre>
<br />
</td>
</tr>
<tr class="even">
<td><strong><code>N.BX i d</code></strong></td>
<td><strong><code>N.BX i r s</code></strong></td>
<td>-</td>
<td>Multi-index version of N.B. Index <code>i</code> in the range 0-15,
allows working with 16 independent scales. Scale at <code>i</code> 0 is
shared with N.B.<br />
<br />
Examples:<br />
&#10;<pre><code>N.BX 0 0 R101011010101   ==&gt;  set scale at index 0 to C-major (default)
CV 1 N.BX 0 1            ==&gt;  set CV 1 to 1st degree of scale
                              (C, value corresponding to N 0)
N.BX 1 3 R100101010010   ==&gt;  set scale at index 1 to Eb-minor pentatonic
CV 1 N.BX 1 2            ==&gt;  set CV 1 to 2nd degree of scale
                              (Gb, value corresponding to N 6)
N.BX 2 5 -3              ==&gt;  set scale at index 2 to F-lydian using preset</code></pre>
<br />
<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>VN x</code></strong></td>
<td>-</td>
<td>-</td>
<td>converts 1V/OCT value <code>x</code> to an equal temperament note
number</td>
</tr>
<tr class="even">
<td><strong><code>QT.B x</code></strong></td>
<td>-</td>
<td>-</td>
<td>quantize 1V/OCT signal <code>x</code> to scale defined by
<code>N.B</code></td>
</tr>
<tr class="odd">
<td><strong><code>QT.BX i x</code></strong></td>
<td>-</td>
<td>-</td>
<td>quantize 1V/OCT signal <code>x</code> to scale defined by
<code>N.BX</code> in scale index <code>i</code></td>
</tr>
<tr class="even">
<td><strong><code>QT.S x r s</code></strong></td>
<td>-</td>
<td>-</td>
<td>quantize 1V/OCT signal <code>x</code> to scale <code>s</code> (0-8,
reference N.S scales) with root 1V/OCT pitch <code>r</code></td>
</tr>
<tr class="odd">
<td><strong><code>QT.CS x r s d c</code></strong></td>
<td>-</td>
<td>-</td>
<td>Quantize 1V/OCT signal <code>x</code> to chord <code>c</code> (1-7)
from scale <code>s</code> (0-8, reference N.S scales) at degree
<code>d</code> (1-7) with root 1V/OCT pitch <code>r</code>.<br />
<br />
Chords (1-7)<br />
- <code>1</code> = Tonic<br />
- <code>2</code> = Third<br />
- <code>3</code> = Triad<br />
- <code>4</code> = Seventh<br />
- etc.<br />
</td>
</tr>
</tbody>
</table>

## Rhythm

Mathematical calculations and tables helpful for rhythmic decisions.

<table style="width:98%;">
<colgroup>
<col style="width: 25%" />
<col style="width: 23%" />
<col style="width: 10%" />
<col style="width: 40%" />
</colgroup>
<thead>
<tr class="header">
<th><code>OP</code></th>
<th><code>OP (set)</code></th>
<th><code>Alias</code></th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>BPM x</code></strong></td>
<td>-</td>
<td>-</td>
<td>milliseconds per beat in BPM <code>x</code></td>
</tr>
<tr class="even">
<td><strong><code>DR.P b p s</code></strong></td>
<td>-</td>
<td>-</td>
<td>The drum helper uses preset drum patterns to give 16-step gate
patterns. Gates wrap after step 16. Bank 0 is a set of pseudo random
gates increasing in density at higher numbered patterns, where pattern 0
is empty,<br />
and pattern 215 is 1s. Bank 1 is bass drum patterns. Bank 2 is snare
drum patterns. Bank 3 is closed hi-hats. Bank 4 is open hi-hits and in
some cases cymbals. Bank 1-4 patterns are related to each other (bank 1
pattern 1’s bass drum pattern fits bank 2 pattern 1’s snare drum
pattern).<br />
The patterns are from <a href="https://shittyrecording.studio/"
target="_blank">Paul Wenzel’s “Pocket Operations” book</a>.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>DR.T b p q l s</code></strong></td>
<td>-</td>
<td>-</td>
<td>The Tresillo helper uses the preset drum patterns described in the
drum pattern help function in a 3, 3, 2 rythmic formation. In the
tresillo, pattern 1 will be repeated twice for a number of steps
determined by the overall length of the pattern. A pattern of length 8
will play the first three steps of your selected pattern 1 twice,
and<br />
the first two steps of pattern 2 once. A pattern length of 16 will play
the first six steps of selected pattern 1 twice, and the first four
steps of pattern 2 once. And so on. The max length is 64. Length will be
rounded down to the nearest multiple of 8. The step number wraps at the
given length.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>DR.V p s</code></strong></td>
<td>-</td>
<td>-</td>
<td>The velocity helper gives velocity values (0-16383) at each step.
The values are intended to be used for drum hit velocities. There are 16
steps, which wrap around. Divide by 129 to convert to midi cc
values.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>ER f l i</code></strong></td>
<td>-</td>
<td>-</td>
<td>Euclidean rhythm helper, as described by Godfried Toussaint in his
2005 paper [“The Euclidean Algorithm Generates Traditional Musical
Rhythms”][euclidean_rhythm_paper][^euclidean_rhythm_citation]. From the
abstract:<br />
<br />
- <code>f</code> is fill (<code>1-32</code>) and should be less then or
equal to length<br />
- <code>l</code> is length (<code>1-32</code>)<br />
- <code>i</code> is the step index, and will work with negative as well
as positive numbers<br />
<br />
If you wish to add rotation as well, use the following form:<br />
<br />
&#10;<pre><code>ER f l SUB i r</code></pre>
<br />
<br />
where <code>r</code> is the number of step of <em>forward</em> rotation
you want.<br />
<br />
For more info, see the post on
[samdoshi.com][samdoshi_com_euclidean]<br />
<br />
[samdoshi_com_euclidean]:
http://samdoshi.com/post/2016/03/teletype-euclidean/<br />
[euclidean_rhythm_paper]:
http://cgm.cs.mcgill.ca/~godfried/publications/banff.pdf<br />
[^euclidean_rhythm_citation]: Toussaint, G. T. (2005, July). The
Euclidean algorithm generates traditional musical rhythms. <em>In
Proceedings of BRIDGES: Mathematical Connections in Art, Music and
Science</em> (pp. 47-56).<br />
</td>
</tr>
<tr class="even">
<td><strong><code>NR p m f s</code></strong></td>
<td>-</td>
<td>-</td>
<td>Numeric Repeater is similar to ER, except it generates patterns
using the binary arithmetic process found in [“Noise Engineering’s
Numeric Repetitor”][numeric_repetitor]. From the description:<br />
<br />
Numeric Repetitor is a rhythmic gate generator based on binary
arithmetic. A core pattern forms the basis and variation is achieved by
treating this pattern as a binary number and multiplying it by another.
NR contains 32 prime rhythms derived by examining all possible rhythms
and weeding out bad ones via heuristic.<br />
<br />
All parameters wrap around their specified ranges automatically and
support negative indexing.<br />
<br />
Masks<br />
- <code>0</code> is no mask<br />
- <code>1</code> is <code>0x0F0F</code><br />
- <code>2</code> is <code>0xF003</code><br />
- <code>3</code> is <code>0x1F0</code><br />
<br />
For further detail [“see the manual”][nr_manual].<br />
<br />
[numeric_repetitor]:
https://www.noiseengineering.us/shop/numeric-repetitor<br />
[nr_manual]:
https://static1.squarespace.com/static/58c709192e69cf2422026fa6/t/5e6041ad4cbc0979d6d793f2/1583366574430/NR_manual.pdf<br />
</td>
</tr>
</tbody>
</table>

## Metronome

An internal metronome executes the M script at a specified rate (in ms).
By default the metronome is enabled (`M.ACT 1`) and set to 1000ms
(`M 1000`). The metro can be set as fast as 25ms (`M 25`). An additional
`M!` op allows for setting the metronome to experimental rates as high
as 2ms (`M! 2`). **WARNING**: when using a large number of i2c commands
in the M script at metro speeds beyond the 25ms teletype stability
issues can occur.

Access the M script directly with `alt-<F10>` or run the script once
using `<F10>`.

<table style="width:98%;">
<colgroup>
<col style="width: 25%" />
<col style="width: 23%" />
<col style="width: 10%" />
<col style="width: 40%" />
</colgroup>
<thead>
<tr class="header">
<th><code>OP</code></th>
<th><code>OP (set)</code></th>
<th><code>Alias</code></th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>M</code></strong></td>
<td><strong><code>M x</code></strong></td>
<td>-</td>
<td>get/set metronome interval to <code>x</code> (in ms), default
<code>1000</code>, minimum value <code>25</code></td>
</tr>
<tr class="even">
<td><strong><code>M!</code></strong></td>
<td><strong><code>M! x</code></strong></td>
<td>-</td>
<td>get/set metronome to experimental interval <code>x</code> (in ms),
minimum value <code>2</code></td>
</tr>
<tr class="odd">
<td><strong><code>M.ACT</code></strong></td>
<td><strong><code>M.ACT x</code></strong></td>
<td>-</td>
<td>get/set metronome activation to <code>x</code> (<code>0/1</code>),
default <code>1</code> (enabled)</td>
</tr>
<tr class="even">
<td><strong><code>M.RESET</code></strong></td>
<td>-</td>
<td>-</td>
<td>hard reset metronome count without triggering</td>
</tr>
</tbody>
</table>

## Randomness

<table style="width:98%;">
<colgroup>
<col style="width: 25%" />
<col style="width: 23%" />
<col style="width: 10%" />
<col style="width: 40%" />
</colgroup>
<thead>
<tr class="header">
<th><code>OP</code></th>
<th><code>OP (set)</code></th>
<th><code>Alias</code></th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>RAND x</code></strong></td>
<td>-</td>
<td><strong><code>RND</code></strong></td>
<td>generate a random number between <code>0</code> and <code>x</code>
inclusive</td>
</tr>
<tr class="even">
<td><strong><code>RRAND x y</code></strong></td>
<td>-</td>
<td><strong><code>RRND</code></strong></td>
<td>generate a random number between <code>x</code> and <code>y</code>
inclusive</td>
</tr>
<tr class="odd">
<td><strong><code>TOSS</code></strong></td>
<td>-</td>
<td>-</td>
<td>randomly return <code>0</code> or <code>1</code></td>
</tr>
<tr class="even">
<td><strong><code>R</code></strong></td>
<td><strong><code>R x</code></strong></td>
<td>-</td>
<td>get a random number/set <code>R.MIN</code> and <code>R.MAX</code> to
same value <code>x</code> (effectively allowing <code>R</code> to be
used as a global variable)</td>
</tr>
<tr class="odd">
<td><strong><code>R.MIN x</code></strong></td>
<td>-</td>
<td>-</td>
<td>set the lower end of the range from -32768 – 32767, default: 0</td>
</tr>
<tr class="even">
<td><strong><code>R.MAX x</code></strong></td>
<td>-</td>
<td>-</td>
<td>set the upper end of the range from -32768 – 32767, default:
16383</td>
</tr>
<tr class="odd">
<td><strong><code>CHAOS x</code></strong></td>
<td>-</td>
<td>-</td>
<td>get next value from chaos generator, or set the current value</td>
</tr>
<tr class="even">
<td><strong><code>CHAOS.R x</code></strong></td>
<td>-</td>
<td>-</td>
<td>get or set the <code>R</code> parameter for the <code>CHAOS</code>
generator</td>
</tr>
<tr class="odd">
<td><strong><code>CHAOS.ALG x</code></strong></td>
<td>-</td>
<td>-</td>
<td>get or set the algorithm for the <code>CHAOS</code> generator. 0 =
LOGISTIC, 1 = CUBIC, 2 = HENON, 3 = CELLULAR</td>
</tr>
<tr class="even">
<td><strong><code>DRUNK</code></strong></td>
<td><strong><code>DRUNK x</code></strong></td>
<td>-</td>
<td>Changes by <code>-1</code>, <code>0</code>, or <code>1</code> upon
each read, saving its state. Setting <code>DRUNK</code><br />
will give it a new value for the next read, and drunkedness will
continue on<br />
from there with subsequent reads.<br />
<br />
Setting <code>DRUNK.MIN</code> and <code>DRUNK.MAX</code> controls the
lower and upper bounds<br />
(inclusive) that <code>DRUNK</code> can reach. <code>DRUNK.WRAP</code>
controls whether the value can<br />
wrap around when it reaches it’s bounds.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>DRUNK.MIN</code></strong></td>
<td><strong><code>DRUNK.MIN x</code></strong></td>
<td>-</td>
<td>set the lower bound for <code>DRUNK</code>, default
<code>0</code></td>
</tr>
<tr class="even">
<td><strong><code>DRUNK.MAX</code></strong></td>
<td><strong><code>DRUNK.MAX x</code></strong></td>
<td>-</td>
<td>set the upper bound for <code>DRUNK</code>, default
<code>255</code></td>
</tr>
<tr class="odd">
<td><strong><code>DRUNK.WRAP</code></strong></td>
<td><strong><code>DRUNK.WRAP x</code></strong></td>
<td>-</td>
<td>should <code>DRUNK</code> wrap around when it reaches it’s bounds,
default <code>0</code></td>
</tr>
<tr class="even">
<td><strong><code>SEED</code></strong></td>
<td><strong><code>SEED x</code></strong></td>
<td>-</td>
<td>get / set the random number generator seed for all <code>SEED</code>
ops</td>
</tr>
<tr class="odd">
<td><strong><code>RAND.SEED</code></strong></td>
<td><strong><code>RAND.SEED x</code></strong></td>
<td><strong><code>RAND.SD</code></strong> ,
<strong><code>R.SD</code></strong></td>
<td>get / set the random number generator seed for <code>R</code>,
<code>RRAND</code>, and <code>RAND</code> ops</td>
</tr>
<tr class="even">
<td><strong><code>TOSS.SEED</code></strong></td>
<td><strong><code>TOSS.SEED x</code></strong></td>
<td><strong><code>TOSS.SD</code></strong></td>
<td>get / set the random number generator seed for the <code>TOSS</code>
op</td>
</tr>
<tr class="odd">
<td><strong><code>PROB.SEED</code></strong></td>
<td><strong><code>PROB.SEED x</code></strong></td>
<td><strong><code>PROB.SD</code></strong></td>
<td>get / set the random number generator seed for the <code>PROB</code>
mod</td>
</tr>
<tr class="even">
<td><strong><code>DRUNK.SEED</code></strong></td>
<td><strong><code>DRUNK.SEED x</code></strong></td>
<td><strong><code>DRUNK.SD</code></strong></td>
<td>get / set the random number generator seed for the
<code>DRUNK</code> op</td>
</tr>
<tr class="odd">
<td><strong><code>P.SEED</code></strong></td>
<td><strong><code>P.SEED x</code></strong></td>
<td><strong><code>P.SD</code></strong></td>
<td>get / set the random number generator seed for the
<code>P.RND</code> and <code>PN.RND</code> ops</td>
</tr>
</tbody>
</table>

## Control flow

<table style="width:98%;">
<colgroup>
<col style="width: 25%" />
<col style="width: 23%" />
<col style="width: 10%" />
<col style="width: 40%" />
</colgroup>
<thead>
<tr class="header">
<th><code>OP</code></th>
<th><code>OP (set)</code></th>
<th><code>Alias</code></th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>IF x: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td>If <code>x</code> is not zero execute command<br />
<br />
#### Advanced <code>IF</code> / <code>ELIF</code> / <code>ELSE</code>
usage<br />
<br />
1. Intermediate statements always run<br />
<br />
&#10;<pre><code>    SCRIPT 1:
    IF 0: 0        =&gt; do nothing
    TR.P 1         =&gt; always happens
    ELSE: TR.P 2   =&gt; else branch runs because of the previous IF</code></pre>
<br />
<br />
2. <code>ELSE</code> without an <code>IF</code><br />
<br />
&#10;<pre><code>    SCRIPT 1:
    ELSE: TR.P 1   =&gt; never runs, as there is no preceding IF</code></pre>
<br />
<br />
<br />
3. <code>ELIF</code> without an <code>IF</code><br />
<br />
&#10;<pre><code>    SCRIPT 1:
    ELIF 1: TR.P 1  =&gt; never runs, as there is no preceding IF</code></pre>
<br />
<br />
4. Independent scripts<br />
<br />
&#10;<pre><code>    SCRIPT 1:
    IF 1: TR.P 1    =&gt; pulse output 1
&#10;    SCRIPT 2:
    ELSE: TR.P 2    =&gt; never runs regardless of what happens in script 1
                       (see example 2)</code></pre>
<br />
<br />
5. Dependent scripts<br />
<br />
&#10;<pre><code>    SCRIPT 1:
    IF 0: TR.P 1    =&gt; do nothing
    SCRIPT 2        =&gt; will pulse output 2
&#10;    SCRIPT 2:
    ELSE: TR.P 2    =&gt; will not pulse output 2 if called directly,
                       but will if called from script 1</code></pre>
<br />
</td>
</tr>
<tr class="even">
<td><strong><code>ELIF x: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td>if all previous <code>IF</code> / <code>ELIF</code> fail, and
<code>x</code> is not zero, execute command</td>
</tr>
<tr class="odd">
<td><strong><code>ELSE: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td>if all previous <code>IF</code> / <code>ELIF</code> fail, excute
command</td>
</tr>
<tr class="even">
<td><strong><code>L x y: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td>Run the command sequentially with <code>I</code> values from
<code>x</code> to <code>y</code>.<br />
<br />
For example:<br />
<br />
&#10;<pre><code>L 1 4: TR.PULSE I   =&gt; pulse outputs 1, 2, 3 and 4
L 4 1: TR.PULSE I   =&gt; pulse outputs 4, 3, 2 and 1</code></pre>
<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>W x: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td>Runs the command while the condition <code>x</code> is true or the
loop iterations exceed 10000.<br />
<br />
For example, to find the first iterated power of 2 greater than
100:<br />
<br />
&#10;<pre><code>A 2
W LT A 100: A * A A</code></pre>
<br />
<br />
A will be 256.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>EVERY x: ...</code></strong></td>
<td>-</td>
<td><strong><code>EV</code></strong></td>
<td>Runs the command every <code>x</code> times the line is executed.
This is tracked on a per-line basis, so each script can have 6 different
“dividers”.<br />
<br />
Here is a 1-script clock divider:<br />
<br />
&#10;<pre><code>EVERY 2: TR.P 1
EVERY 4: TR.P 2
EVERY 8: TR.P 3
EVERY 16: TR.P 4</code></pre>
<br />
<br />
The numbers do <em>not</em> need to be evenly divisible by each other,
so there is no problem with:<br />
<br />
&#10;<pre><code>EVERY 2: TR.P 1
EVERY 3: TR.P 2</code></pre>
<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>SKIP x: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td>This is the corollary function to <code>EVERY</code>, essentially
behaving as its exact opposite.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>OTHER: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td><code>OTHER</code> can be used to do somthing alternately with a
preceding <code>EVERY</code> or <code>SKIP</code> command.<br />
<br />
For example, here is a script that alternates between two triggers to
make a four-on-the-floor beat with hats between the beats:<br />
<br />
&#10;<pre><code>EVERY 4: TR.P 1
OTHER: TR.P 2</code></pre>
<br />
<br />
You could add snares on beats 2 and 4 with:<br />
<br />
&#10;<pre><code>SKIP 2: TR.P 3</code></pre>
<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>SYNC x</code></strong></td>
<td>-</td>
<td>-</td>
<td>Causes all of the <code>EVERY</code> and <code>SYNC</code> counters
to synchronize their offsets, respecting their individual divisor
values.<br />
<br />
Negative numbers will synchronize to to the divisor value, such that
<code>SYNC -1</code> causes all every counters to be 1 number before
their divisor, causing each <code>EVERY</code> to be true on its next
call, and each <code>SKIP</code> to be false.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>PROB x: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td>potentially execute command with probability <code>x</code>
(0-100)</td>
</tr>
<tr class="odd">
<td><strong><code>SCRIPT</code></strong></td>
<td><strong><code>SCRIPT x</code></strong></td>
<td><strong><code>$</code></strong></td>
<td>Execute script <code>x</code> (1-10, 9 = metro, 10 = init),
recursion allowed.<br />
<br />
There is a limit of 8 for the maximum number of nested calls to
<code>SCRIPT</code> to stop infinite loops from locking up the
Teletype.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>SCRIPT.POL x</code></strong></td>
<td><strong><code>SCRIPT.POL x p</code></strong></td>
<td><strong><code>$.POL</code></strong></td>
<td>Get or set the trigger polarity of script <code>x</code>,
determining which trigger edges the script will fire on.<br />
<br />
1: rising edge (default)<br />
2: falling edge<br />
3: either edge<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>$F script</code></strong></td>
<td>-</td>
<td>-</td>
<td>This op will execute a script similarly to <code>SCRIPT</code> op
but it will also return a value,<br />
which means you can define a script that calculates something and then
use it in an expression.<br />
To set the return value, either place an expression at the end of the
script without<br />
assigning it to anything or assign it to the special function return
variable <code>FR</code>. If you do<br />
both, <code>FR</code> will be used, and if you don’t do either, zero
will be returned.<br />
<br />
Let’s say you update script 1 to return the square of <code>X</code>:
<code>* X X</code> (which you could also<br />
write as <code>FR * X X</code>). Then you can use it in an expression
like this: <code>A + A $F 1</code>.<br />
<br />
This op can save space if you have a calculation that is used in
multiple places.<br />
Other than returning a value, a function script isn’t different from a
regular script<br />
and can perform other actions in addition to calculating something,
including calling<br />
other scripts. The same limit of 8 maximum nested calls applies here to
prevent infinite loops.<br />
<br />
If you need to be able to pass parameters into your function, use
<code>$F1</code> or <code>$F2</code> ops.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>$F1 script param</code></strong></td>
<td>-</td>
<td>-</td>
<td>Same as <code>$F</code> but you can also pass a single parameter
into the function. Inside the function<br />
script you can get the parameter using <code>I1</code> op.<br />
<br />
Let’s say you create a script that returns the square of the passed
parameter: <code>FR * I1 I1</code>.<br />
You can then calculate the square of a number by executing
<code>$F1 value</code>.<br />
<br />
See the description of <code>$F</code> op for more details on executing
scripts as functions.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>$F2 script param1 param2</code></strong></td>
<td>-</td>
<td>-</td>
<td>Same as <code>$F</code> but you can also pass two parameters into a
function. Inside the function script<br />
you can get them using <code>I1</code> and <code>I2</code> ops.<br />
<br />
Let’s say you create a script that returns a randomly selected value out
of the two<br />
provided values: <code>FR ? TOSS I1 I2</code>. You can then save space
by using <code>$F2 1 X Y</code> instead of<br />
<code>? TOSS X Y</code>. More importantly, you could use it in multiple
places, and if you later<br />
want to change the calculation to something else, you just need to
update the function script.<br />
<br />
See the description of <code>$F</code> op for more details on executing
scripts as functions.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>$L script line</code></strong></td>
<td>-</td>
<td>-</td>
<td>This op executes the specified script line. This allows you to use a
script as a library of sorts,<br />
where each line does something different, so you can use the same script
for multiple purposes.<br />
It also allows you to use free lines in a script to extend another
script.<br />
<br />
This op behaves similarly to <code>$F</code> op in that it can be used
as a function in an expression<br />
by setting the return value with <code>FR</code>. Let’s say the first
line in script 1 is this: <code>FR * X X</code>.<br />
You can then get the square of <code>X</code> by executing
<code>$L 1 1</code>.<br />
<br />
If you want to use it as a function and you need to pass some parameters
into it, use<br />
<code>$L1</code> / <code>$L2</code> ops.<br />
<br />
This op is also useful if you have a loop that doesn’t fit on one line -
define the line<br />
later in the script and then reference it in the loop:<br />
<br />
&#10;<pre><code>#1
L 1 6: A + A $L 1 3
BREAK
SCALE X Y C D I</code></pre>
<br />
<br />
Don’t forget to add <code>BREAK</code> before the line so that it’s not
executed when the whole script<br />
is executed. If you use this technique, you can also save space by using
<code>$S</code> op which executes<br />
a line within the same script.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>$L1 script line param</code></strong></td>
<td>-</td>
<td>-</td>
<td>Execute the specified script line as a function that takes 1
parameter. See the description of<br />
<code>$L</code> and <code>$F1</code> ops for more details.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>$L2 script line param1 param2</code></strong></td>
<td>-</td>
<td>-</td>
<td>Execute the specified script line as a function that takes 2
parameters. See the description of<br />
<code>$L</code> and <code>$F2</code> ops for more details.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>$S line</code></strong></td>
<td>-</td>
<td>-</td>
<td>This is exactly the same as <code>$L $ line</code> but saves you
space on not having to specify the script<br />
number if the line you want to execute is within the same script.<br />
<br />
See the description of <code>$L</code> for more details.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>$S1 line param</code></strong></td>
<td>-</td>
<td>-</td>
<td>This is exactly the same as <code>$L1 $ line param</code> but saves
you space on not having to specify<br />
the script number if the line you want to execute is within the same
script.<br />
<br />
See the description of <code>$L1</code> for more details.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>$S2 line param1 param2</code></strong></td>
<td>-</td>
<td>-</td>
<td>This is exactly the same as <code>$L2 $ line param1 param2</code>
but saves you space on not having<br />
to specify the script number if the line you want to execute is within
the same script.<br />
<br />
See the description of <code>$L2</code> for more details.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>I1</code></strong></td>
<td>-</td>
<td>-</td>
<td>This op returns the first parameter when a script is called as a
function using<br />
<code>$F1</code> / <code>$F2</code> / <code>$L1</code> /
<code>$L2</code> / <code>$S1</code> / <code>$S2</code> ops. If the
script is called<br />
using other ops, this op will return zero.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>I2</code></strong></td>
<td>-</td>
<td>-</td>
<td>This op returns the second parameter when a script is called as a
function using<br />
<code>$F2</code> / <code>$L2</code> / <code>$S2</code> ops. If the
script is called using other ops, this op will return zero.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>FR</code></strong></td>
<td><strong><code>FR x</code></strong></td>
<td>-</td>
<td>Use this op to get or set the return value in a script that is
called as a function.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>SCENE</code></strong></td>
<td><strong><code>SCENE x</code></strong></td>
<td>-</td>
<td>Load scene <code>x</code> (0-31).<br />
<br />
Does <em>not</em> execute the <code>I</code> script.<br />
Will <em>not</em> execute from the <code>I</code> script on scene load.
Will execute on subsequent calls to the <code>I</code> script.<br />
<br />
<strong>WARNING</strong>: You will lose any unsaved changes to your
scene.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>SCENE.G x</code></strong></td>
<td>-</td>
<td>-</td>
<td>Load scene <code>x</code> (0-31) without loading grid button and
fader states.<br />
<br />
<strong>WARNING</strong>: You will lose any unsaved changes to your
scene.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>SCENE.P x</code></strong></td>
<td>-</td>
<td>-</td>
<td>Load scene <code>x</code> (0-31) without loading pattern data.<br />
<br />
<strong>WARNING</strong>: You will lose any unsaved changes to your
scene.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>KILL</code></strong></td>
<td>-</td>
<td>-</td>
<td>clears stack, clears delays, cancels pulses, cancels slews, disables
metronome</td>
</tr>
<tr class="odd">
<td><strong><code>BREAK</code></strong></td>
<td>-</td>
<td><strong><code>BRK</code></strong></td>
<td>halts execution of the current script</td>
</tr>
<tr class="even">
<td><strong><code>INIT</code></strong></td>
<td>-</td>
<td>-</td>
<td><br />
<strong>WARNING</strong>: You will lose all settings when you initialize
using <code>INIT</code> - there is NO undo!<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>INIT.CV x</code></strong></td>
<td>-</td>
<td>-</td>
<td>clears all parameters on CV associated with output x</td>
</tr>
<tr class="even">
<td><strong><code>INIT.CV.ALL</code></strong></td>
<td>-</td>
<td>-</td>
<td>clears all parameters on all CV’s</td>
</tr>
<tr class="odd">
<td><strong><code>INIT.DATA</code></strong></td>
<td>-</td>
<td>-</td>
<td><br />
Clears the following variables and resets them to default values: A, B,
C, D, CV slew, Drunk min/max, M, O, Q, R, T, TR.<br />
Does not affect the CV input (IN) or the Parameter knob (PARAM)
values.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>INIT.P x</code></strong></td>
<td>-</td>
<td>-</td>
<td>clears pattern number x</td>
</tr>
<tr class="odd">
<td><strong><code>INIT.P.ALL</code></strong></td>
<td>-</td>
<td>-</td>
<td>clears all patterns</td>
</tr>
<tr class="even">
<td><strong><code>INIT.SCENE</code></strong></td>
<td>-</td>
<td>-</td>
<td>loads a blank scene</td>
</tr>
<tr class="odd">
<td><strong><code>INIT.SCRIPT x</code></strong></td>
<td>-</td>
<td>-</td>
<td>clear script number x</td>
</tr>
<tr class="even">
<td><strong><code>INIT.SCRIPT.ALL</code></strong></td>
<td>-</td>
<td>-</td>
<td>clear all scripts</td>
</tr>
<tr class="odd">
<td><strong><code>INIT.TIME x</code></strong></td>
<td>-</td>
<td>-</td>
<td>clear time on trigger x</td>
</tr>
<tr class="even">
<td><strong><code>INIT.TR x</code></strong></td>
<td>-</td>
<td>-</td>
<td>clear all parameters on trigger x</td>
</tr>
<tr class="odd">
<td><strong><code>INIT.TR.ALL</code></strong></td>
<td>-</td>
<td>-</td>
<td>clear all triggers</td>
</tr>
</tbody>
</table>

## Maths

Logical operators such as `EQ`, `OR` and `LT` return `1` for true, and
`0` for false.

<table style="width:98%;">
<colgroup>
<col style="width: 25%" />
<col style="width: 23%" />
<col style="width: 10%" />
<col style="width: 40%" />
</colgroup>
<thead>
<tr class="header">
<th><code>OP</code></th>
<th><code>OP (set)</code></th>
<th><code>Alias</code></th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>ADD x y</code></strong></td>
<td>-</td>
<td><strong><code>+</code></strong></td>
<td>add <code>x</code> and <code>y</code> together</td>
</tr>
<tr class="even">
<td><strong><code>SUB x y</code></strong></td>
<td>-</td>
<td><strong><code>-</code></strong></td>
<td>subtract <code>y</code> from <code>x</code></td>
</tr>
<tr class="odd">
<td><strong><code>MUL x y</code></strong></td>
<td>-</td>
<td><strong><code>*</code></strong></td>
<td>returns <code>x</code> times <code>y</code>, bounded to integer
limits</td>
</tr>
<tr class="even">
<td><strong><code>DIV x y</code></strong></td>
<td>-</td>
<td><strong><code>/</code></strong></td>
<td>divide <code>x</code> by <code>y</code></td>
</tr>
<tr class="odd">
<td><strong><code>MOD x y</code></strong></td>
<td>-</td>
<td><strong><code>%</code></strong></td>
<td>find the remainder after division of <code>x</code> by
<code>y</code></td>
</tr>
<tr class="even">
<td><strong><code>? x y z</code></strong></td>
<td>-</td>
<td>-</td>
<td>if condition <code>x</code> is true return <code>y</code>, otherwise
return <code>z</code></td>
</tr>
<tr class="odd">
<td><strong><code>MIN x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>return the minimum of <code>x</code> and <code>y</code></td>
</tr>
<tr class="even">
<td><strong><code>MAX x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>return the maximum of <code>x</code> and <code>y</code></td>
</tr>
<tr class="odd">
<td><strong><code>LIM x y z</code></strong></td>
<td>-</td>
<td>-</td>
<td>limit the value <code>x</code> to the range <code>y</code> to
<code>z</code> inclusive</td>
</tr>
<tr class="even">
<td><strong><code>WRAP x y z</code></strong></td>
<td>-</td>
<td><strong><code>WRP</code></strong></td>
<td>limit the value <code>x</code> to the range <code>y</code> to
<code>z</code> inclusive, but with wrapping</td>
</tr>
<tr class="odd">
<td><strong><code>QT x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>Round <code>x</code> to the closest multiple of
<code>y</code>.<br />
<em>See also: <code>QT.S</code>, <code>QT.CS</code>, <code>QT.B</code>,
<code>QT.BX</code> in the Pitch section</em>.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>AVG x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>the average of <code>x</code> and <code>y</code></td>
</tr>
<tr class="odd">
<td><strong><code>EQ x y</code></strong></td>
<td>-</td>
<td><strong><code>==</code></strong></td>
<td>does <code>x</code> equal <code>y</code></td>
</tr>
<tr class="even">
<td><strong><code>NE x y</code></strong></td>
<td>-</td>
<td><strong><code>!=</code></strong> ,
<strong><code>XOR</code></strong></td>
<td><code>x</code> is not equal to <code>y</code></td>
</tr>
<tr class="odd">
<td><strong><code>LT x y</code></strong></td>
<td>-</td>
<td><strong><code>&lt;</code></strong></td>
<td><code>x</code> is less than <code>y</code></td>
</tr>
<tr class="even">
<td><strong><code>GT x y</code></strong></td>
<td>-</td>
<td><strong><code>&gt;</code></strong></td>
<td><code>x</code> is greater than <code>y</code></td>
</tr>
<tr class="odd">
<td><strong><code>LTE x y</code></strong></td>
<td>-</td>
<td><strong><code>&lt;=</code></strong></td>
<td><code>x</code> is less than or equal to <code>y</code></td>
</tr>
<tr class="even">
<td><strong><code>GTE x y</code></strong></td>
<td>-</td>
<td><strong><code>&gt;=</code></strong></td>
<td><code>x</code> is greater than or equal to <code>y</code></td>
</tr>
<tr class="odd">
<td><strong><code>INR l x h</code></strong></td>
<td>-</td>
<td><strong><code>&gt;&lt;</code></strong></td>
<td><code>x</code> is greater than <code>l</code> and less than
<code>h</code> (within range)</td>
</tr>
<tr class="even">
<td><strong><code>OUTR l x h</code></strong></td>
<td>-</td>
<td><strong><code>&lt;&gt;</code></strong></td>
<td><code>x</code> is less than <code>l</code> or greater than
<code>h</code> (out of range)</td>
</tr>
<tr class="odd">
<td><strong><code>INRI l x h</code></strong></td>
<td>-</td>
<td><strong><code>&gt;=&lt;</code></strong></td>
<td><code>x</code> is greater than or equal to <code>l</code> and less
than or equal to <code>h</code> (within range, inclusive)</td>
</tr>
<tr class="even">
<td><strong><code>OUTRI l x h</code></strong></td>
<td>-</td>
<td><strong><code>&lt;=&gt;</code></strong></td>
<td><code>x</code> is less than or equal to <code>l</code> or greater
than or equal to <code>h</code> (out of range, inclusive)</td>
</tr>
<tr class="odd">
<td><strong><code>EZ x</code></strong></td>
<td>-</td>
<td><strong><code>!</code></strong></td>
<td><code>x</code> is <code>0</code>, equivalent to logical NOT</td>
</tr>
<tr class="even">
<td><strong><code>NZ x</code></strong></td>
<td>-</td>
<td>-</td>
<td><code>x</code> is not <code>0</code></td>
</tr>
<tr class="odd">
<td><strong><code>LSH x y</code></strong></td>
<td>-</td>
<td><strong><code>&lt;&lt;</code></strong></td>
<td>left shift <code>x</code> by <code>y</code> bits, in effect multiply
<code>x</code> by <code>2</code> to the power of <code>y</code></td>
</tr>
<tr class="even">
<td><strong><code>RSH x y</code></strong></td>
<td>-</td>
<td><strong><code>&gt;&gt;</code></strong></td>
<td>right shift <code>x</code> by <code>y</code> bits, in effect divide
<code>x</code> by <code>2</code> to the power of <code>y</code></td>
</tr>
<tr class="odd">
<td><strong><code>LROT x y</code></strong></td>
<td>-</td>
<td><strong><code>&lt;&lt;&lt;</code></strong></td>
<td>circular left shift <code>x</code> by <code>y</code> bits, wrapping
around when bits fall off the end</td>
</tr>
<tr class="even">
<td><strong><code>RROT x y</code></strong></td>
<td>-</td>
<td><strong><code>&gt;&gt;&gt;</code></strong></td>
<td>circular right shift <code>x</code> by <code>y</code> bits, wrapping
around when bits fall off the end</td>
</tr>
<tr class="odd">
<td><strong><code>| x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>bitwise or <code>x</code></td>
</tr>
<tr class="even">
<td><strong><code>&amp; x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>bitwise and <code>x</code> &amp; <code>y</code></td>
</tr>
<tr class="odd">
<td><strong><code>^ x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>bitwise xor <code>x</code> ^ <code>y</code></td>
</tr>
<tr class="even">
<td><strong><code>~ x</code></strong></td>
<td>-</td>
<td>-</td>
<td>bitwise not, i.e.: inversion of <code>x</code></td>
</tr>
<tr class="odd">
<td><strong><code>BSET x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>set bit <code>y</code> in value <code>x</code></td>
</tr>
<tr class="even">
<td><strong><code>BGET x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>get bit <code>y</code> in value <code>x</code></td>
</tr>
<tr class="odd">
<td><strong><code>BCLR x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>clear bit <code>y</code> in value <code>x</code></td>
</tr>
<tr class="even">
<td><strong><code>BTOG x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>toggle bit <code>y</code> in value <code>x</code></td>
</tr>
<tr class="odd">
<td><strong><code>BREV x</code></strong></td>
<td>-</td>
<td>-</td>
<td>reverse bit order in value <code>x</code></td>
</tr>
<tr class="even">
<td><strong><code>ABS x</code></strong></td>
<td>-</td>
<td>-</td>
<td>absolute value of <code>x</code></td>
</tr>
<tr class="odd">
<td><strong><code>AND x y</code></strong></td>
<td>-</td>
<td><strong><code>&amp;&amp;</code></strong></td>
<td>Logical AND of <code>x</code> and <code>y</code>. Returns
<code>1</code> if both <code>x</code> and <code>y</code> are greater
than <code>0</code>, otherwise it returns <code>0</code>.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>AND3 x y z</code></strong></td>
<td>-</td>
<td><strong><code>&amp;&amp;&amp;</code></strong></td>
<td>Logical AND of <code>x</code>, <code>y</code> and <code>z</code>.
Returns <code>1</code> if both <code>x</code>, <code>y</code> and
<code>z</code> are greater than <code>0</code>, otherwise it returns
<code>0</code>.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>AND4 x y z a</code></strong></td>
<td>-</td>
<td><strong><code>&amp;&amp;&amp;&amp;</code></strong></td>
<td>Logical AND of <code>x</code>, <code>y</code>, <code>z</code> and
<code>a</code>. Returns <code>1</code> if both <code>x</code>,
<code>y</code>, <code>z</code> and <code>a</code> are greater than
<code>0</code>, otherwise it returns <code>0</code>.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>OR x y</code></strong></td>
<td>-</td>
<td><strong><code>||</code></strong></td>
<td>Logical OR of <code>x</code> and <code>y</code>. Returns
<code>1</code> if either <code>x</code> or <code>y</code> are greater
than <code>0</code>, otherwise it returns <code>0</code>.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>OR3 x y z</code></strong></td>
<td>-</td>
<td><strong><code>|||</code></strong></td>
<td>Logical OR of <code>x</code>, <code>y</code> and <code>z</code>.
Returns <code>1</code> if either <code>x</code>, <code>y</code> or
<code>z</code> are greater than <code>0</code>, otherwise it returns
<code>0</code>.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>OR4 x y z a</code></strong></td>
<td>-</td>
<td><strong><code>||||</code></strong></td>
<td>Logical OR of <code>x</code>, <code>y</code>, <code>z</code> and
<code>a</code>. Returns <code>1</code> if either <code>x</code>,
<code>y</code>, <code>z</code> or <code>a</code> are greater than
<code>0</code>, otherwise it returns <code>0</code>.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>SCALE a b x y i</code></strong></td>
<td>-</td>
<td><strong><code>SCL</code></strong></td>
<td>scale <code>i</code> from range <code>a</code> to <code>b</code> to
range <code>x</code> to <code>y</code>,
i.e. <code>i * (y - x) / (b - a)</code></td>
</tr>
<tr class="even">
<td><strong><code>SCALE a b i</code></strong></td>
<td>-</td>
<td><strong><code>SCL0</code></strong></td>
<td>scale <code>i</code> from range <code>0</code> to <code>a</code> to
range <code>0</code> to <code>b</code></td>
</tr>
<tr class="odd">
<td><strong><code>EXP x</code></strong></td>
<td>-</td>
<td>-</td>
<td>exponentiation table lookup. <code>0-16383</code> range (V
<code>0-10</code>)</td>
</tr>
<tr class="even">
<td><strong><code>SGN x</code></strong></td>
<td>-</td>
<td>-</td>
<td>sign function: 1 for positive, -1 for negative, 0 for 0</td>
</tr>
</tbody>
</table>

## Delay

The `DEL` delay op allow commands to be sheduled for execution after a
defined interval by placing them into a buffer which can hold up to 64
commands. Commands can be delayed by up to 16 seconds.

In LIVE mode, the second icon (an upside-down U) will be lit up when
there is a command in the `DEL` buffer.

<table style="width:98%;">
<colgroup>
<col style="width: 25%" />
<col style="width: 23%" />
<col style="width: 10%" />
<col style="width: 40%" />
</colgroup>
<thead>
<tr class="header">
<th><code>OP</code></th>
<th><code>OP (set)</code></th>
<th><code>Alias</code></th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>DEL x: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td>Delay the command following the colon by <code>x</code> ms by
placing it into a buffer.<br />
The buffer can hold up to 16 commands. If the buffer is full, additional
commands<br />
will be discarded.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>DEL.CLR</code></strong></td>
<td>-</td>
<td>-</td>
<td>Clear the delay buffer, cancelling the pending commands.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>DEL.X x delay_time: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td>Delay the command following the colon <code>x</code> times at
intervals of <code>delay_time</code> ms by placing it into a
buffer.<br />
The buffer can hold up to 16 commands. If the buffer is full, additional
commands<br />
will be discarded.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>DEL.R x delay_time: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td>Delay the command following the colon once immediately, and
<code>x - 1</code> times at intervals of <code>delay_time</code> ms by
placing it into a buffer.<br />
The buffer can hold up to 16 commands. If the buffer is full, additional
commands<br />
will be discarded.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>DEL.G x delay_time num denom: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td>Trigger the command once immediately and <code>x - 1</code> times at
ms intervals of <code>delay_time * (num/denom)^n</code> where n ranges
from 0 to <code>x - 1</code> by placing it into a buffer.<br />
The buffer can hold up to 16 commands. If the buffer is full, additional
commands<br />
will be discarded.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>DEL.B delay_time bitmask: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td>Trigger the command up to 16 times at intervals of
<code>delay_time</code> ms. Active intervals set in 16-bit
<code>bitmask</code>, LSB = immediate.</td>
</tr>
</tbody>
</table>

## Stack

These operators manage a last in, first out, stack of commands, allowing
them to be memorised for later execution at an unspecified time. The
stack can hold up to 8 commands. Commands added to a full stack will be
discarded.

<table style="width:98%;">
<colgroup>
<col style="width: 25%" />
<col style="width: 23%" />
<col style="width: 10%" />
<col style="width: 40%" />
</colgroup>
<thead>
<tr class="header">
<th><code>OP</code></th>
<th><code>OP (set)</code></th>
<th><code>Alias</code></th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>S: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td>Add the command following the colon to the top of the stack. If the
stack<br />
is full, the command will be discarded.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>S.CLR</code></strong></td>
<td>-</td>
<td>-</td>
<td>Clear the stack, cancelling all of the commands.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>S.ALL</code></strong></td>
<td>-</td>
<td>-</td>
<td>Execute all entries in the stack (last in, first out), clearing the
stack in<br />
the process.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>S.POP</code></strong></td>
<td>-</td>
<td>-</td>
<td>Pop the most recent command off the stack and execute it.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>S.L</code></strong></td>
<td>-</td>
<td>-</td>
<td>Get the number of entries in the stack.<br />
</td>
</tr>
</tbody>
</table>

## Patterns

Patterns facilitate musical data manipulation– lists of numbers that can
be used as sequences, chord sets, rhythms, or whatever you choose.
Pattern memory consists four banks of 64 steps. Functions are provided
for a variety of pattern creation, transformation, and playback.

New in teletype 2.0, a second version of all Pattern ops have been
added. The original `P` ops (`P`, `P.L`, `P.NEXT`, etc.) act upon the
‘working pattern’ as defined by `P.N`. By default the working pattern is
assigned to pattern 0 (`P.N 0`), in order to execute a command on
pattern 1 using `P` ops you would need to first reassign the working
pattern to pattern 1 (`P.N 1`).

The new set of ops, `PN` (`PN`, `PN.L`, `PN.NEXT`, etc.), include a
variable to designate the pattern number they act upon, and don’t effect
the pattern assignment of the ‘working pattern’ (ex: `PN.NEXT 2` would
increment pattern 2 one index and return the value at the new index).
For simplicity throughout this introduction we will only refer to the
`P` ops, but keep in mind that they now each have a `PN` counterpart
(all of which are detailed below)

Both patterns and their arrays of numbers are indexed from 0. This makes
the first pattern number 0, and the first value of a pattern is index 0.
The pattern index (`P.I`) functions like a playhead which can be moved
throughout the pattern and/or read using ops: `P`, `P.I`, `P.HERE`,
`P.NEXT`, and `P.PREV`. You can contain pattern movements to ranges of a
pattern and define wrapping behavior using ops: `P.START`, `P.END`,
`P.L`, and `P.WRAP`.

Values can be edited, added, and retrieved from the command line using
ops: `P`, `P.INS`, `P.RM`, `P.PUSH`, `P.HERE`, `P.NEXT`, and `P.PREV`.
Some of these ops will additionally impact the pattern length upon their
execution: `P.INS`, `P.RM`, `P.PUSH`, and `P.POP`.

To see your current pattern data use the `<tab>` key to cycle through
live mode, edit mode, and pattern mode. In pattern mode each of the 4
patterns is represented as a column. You can use the arrow keys to
navigate throughout the 4 patterns and their 64 values. For reference a
key of numbers runs the down the lefthand side of the screen in pattern
mode displaying 0-63.

From a blank set of patterns you can enter data by typing into the first
cell in a column. Once you hit `<enter>` you will move to the cell below
and the pattern length will become one step long. You can continue this
process to write out a pattern of desired length. The step you are
editing is always the brightest. As you add steps to a pattern by
editing the value and hitting `<enter>` they become brighter than the
unused cells. This provides a visual indication of the pattern length.

The start and end points of a pattern are represented by the dotted line
next to the column, and the highlighted dot in this line indicates the
current pattern index for each of the patterns. See the key bindings for
an extensive list of editing shortcuts available within pattern mode.

<table style="width:98%;">
<colgroup>
<col style="width: 25%" />
<col style="width: 23%" />
<col style="width: 10%" />
<col style="width: 40%" />
</colgroup>
<thead>
<tr class="header">
<th><code>OP</code></th>
<th><code>OP (set)</code></th>
<th><code>Alias</code></th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>P.N</code></strong></td>
<td><strong><code>P.N x</code></strong></td>
<td>-</td>
<td>get/set the pattern number for the working pattern, default
<code>0</code>. All <code>P</code> ops refer to this pattern.</td>
</tr>
<tr class="even">
<td><strong><code>P x</code></strong></td>
<td><strong><code>P x y</code></strong></td>
<td>-</td>
<td>get/set the value of the working pattern at index <code>x</code>.
All positive values (<code>0-63</code>) can be set or returned while
index values greater than 63 clip to 63. Negative <code>x</code> values
are indexed backwards from the end of the pattern length of the working
pattern.<br />
<br />
Example:<br />
<br />
with a pattern length of 6 for the working pattern:<br />
<br />
<code>P 10</code><br />
retrieves the working pattern value at index 6<br />
<br />
<code>P.I -2</code><br />
retrieves the working pattern value at index 4<br />
<br />
This applies to <code>PN</code> as well, except the pattern number is
the first variable and a second variable specifies the index.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>PN x y</code></strong></td>
<td><strong><code>PN x y z</code></strong></td>
<td>-</td>
<td>get/set the value of pattern <code>x</code> at index
<code>y</code></td>
</tr>
<tr class="even">
<td><strong><code>P.L</code></strong></td>
<td><strong><code>P.L x</code></strong></td>
<td>-</td>
<td>get/set pattern length of the working pattern, non-destructive to
data</td>
</tr>
<tr class="odd">
<td><strong><code>PN.L x</code></strong></td>
<td><strong><code>PN.L x y</code></strong></td>
<td>-</td>
<td>get/set pattern length of pattern x. non-destructive to data</td>
</tr>
<tr class="even">
<td><strong><code>P.WRAP</code></strong></td>
<td><strong><code>P.WRAP x</code></strong></td>
<td>-</td>
<td>when the working pattern reaches its bounds does it wrap
(<code>0/1</code>). With <code>PN.WRAP</code> enabled (<code>1</code>),
when an index reaches its upper or lower bound using <code>P.NEXT</code>
or <code>P.PREV</code> it will wrap to the other end of the pattern and
you can continue advancing. The bounds of P.WRAP are defined through
<code>P.L</code>, <code>P.START</code>, and <code>P.END</code>.<br />
<br />
If wrap is enabled (<code>P.WRAP 1</code>) a pattern will begin at its
start location and advance to the lesser index of either its end
location or the end of its pattern length<br />
<br />
Examples:<br />
<br />
With wrap enabled, a pattern length of 6, a start location of 2 , and an
end location of 8.<br />
<br />
<code>P.WRAP 1; P.L 6; P.START 2; P.END 8</code><br />
<br />
The pattern will wrap between the indexes <code>2</code> and
<code>5</code>.<br />
<br />
With wrap enabled, a pattern length of 10, a start location of 3, and an
end location of 6.<br />
<br />
<code>P.WRAP 1; P.L 10; P.START 3; P.END 6</code><br />
<br />
The pattern will wrap between the indexes <code>3</code> and
<code>6</code>.<br />
<br />
If wrap is disabled (<code>P.WRAP 0</code>) a pattern will run between
its start and end locations and halt at either bound.<br />
<br />
This applies to <code>PN.WRAP</code> as well, except the pattern number
is the first variable and a second variable specifies the wrap behavior
(<code>0/1</code>).<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>PN.WRAP x</code></strong></td>
<td><strong><code>PN.WRAP x y</code></strong></td>
<td>-</td>
<td>when pattern <code>x</code> reaches its bounds does it wrap
(<code>0/1</code>), default <code>1</code> (enabled)</td>
</tr>
<tr class="even">
<td><strong><code>P.START</code></strong></td>
<td><strong><code>P.START x</code></strong></td>
<td>-</td>
<td>get/set the start location of the working pattern, default
<code>0</code></td>
</tr>
<tr class="odd">
<td><strong><code>PN.START x</code></strong></td>
<td><strong><code>PN.START x y</code></strong></td>
<td>-</td>
<td>get/set the start location of pattern <code>x</code>, default
<code>0</code></td>
</tr>
<tr class="even">
<td><strong><code>P.END</code></strong></td>
<td><strong><code>P.END x</code></strong></td>
<td>-</td>
<td>get/set the end location of the working pattern, default
<code>63</code></td>
</tr>
<tr class="odd">
<td><strong><code>PN.END x</code></strong></td>
<td><strong><code>PN.END x y</code></strong></td>
<td>-</td>
<td>get/set the end location of the pattern <code>x</code>, default
<code>63</code></td>
</tr>
<tr class="even">
<td><strong><code>P.I</code></strong></td>
<td><strong><code>P.I x</code></strong></td>
<td>-</td>
<td>get/set index position for the working pattern. all values greater
than pattern length return the first step beyond the pattern length.
negative values are indexed backwards from the end of the pattern
length.<br />
<br />
Example:<br />
<br />
With a pattern length of <code>6</code> (<code>P.L 6</code>), yielding
an index range of <code>0-5</code>:<br />
<br />
<code>P.I 3</code><br />
<br />
moves the index of the working pattern to <code>3</code><br />
<br />
<code>P.I 10</code><br />
<br />
moves the index of the working pattern to <code>6</code><br />
<br />
<code>P.I -2</code><br />
<br />
moves the index of the working pattern to <code>4</code><br />
<br />
This applies to <code>PN.I</code>, except the pattern number is the
first variable and a second variable specifics the index.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>PN.I x</code></strong></td>
<td><strong><code>PN.I x y</code></strong></td>
<td>-</td>
<td>get/set index position for pattern <code>x</code></td>
</tr>
<tr class="even">
<td><strong><code>P.HERE</code></strong></td>
<td><strong><code>P.HERE x</code></strong></td>
<td>-</td>
<td>get/set value at current index of working pattern</td>
</tr>
<tr class="odd">
<td><strong><code>PN.HERE x</code></strong></td>
<td><strong><code>PN.HERE x y</code></strong></td>
<td>-</td>
<td>get/set value at current index of pattern <code>x</code></td>
</tr>
<tr class="even">
<td><strong><code>P.NEXT</code></strong></td>
<td><strong><code>P.NEXT x</code></strong></td>
<td>-</td>
<td>increment index of working pattern then get/set value</td>
</tr>
<tr class="odd">
<td><strong><code>PN.NEXT x</code></strong></td>
<td><strong><code>PN.NEXT x y</code></strong></td>
<td>-</td>
<td>increment index of pattern <code>x</code> then get/set value</td>
</tr>
<tr class="even">
<td><strong><code>P.PREV</code></strong></td>
<td><strong><code>P.PREV x</code></strong></td>
<td>-</td>
<td>decrement index of working pattern then get/set value</td>
</tr>
<tr class="odd">
<td><strong><code>PN.PREV x</code></strong></td>
<td><strong><code>PN.PREV x y</code></strong></td>
<td>-</td>
<td>decrement index of pattern <code>x</code> then get/set value</td>
</tr>
<tr class="even">
<td><strong><code>P.INS x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>insert value <code>y</code> at index <code>x</code> of working
pattern, shift later values down, destructive to loop length</td>
</tr>
<tr class="odd">
<td><strong><code>PN.INS x y z</code></strong></td>
<td>-</td>
<td>-</td>
<td>insert value <code>z</code> at index <code>y</code> of pattern
<code>x</code>, shift later values down, destructive to loop length</td>
</tr>
<tr class="even">
<td><strong><code>P.RM x</code></strong></td>
<td>-</td>
<td>-</td>
<td>delete index <code>x</code> of working pattern, shift later values
up, destructive to loop length</td>
</tr>
<tr class="odd">
<td><strong><code>PN.RM x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>delete index <code>y</code> of pattern <code>x</code>, shift later
values up, destructive to loop length</td>
</tr>
<tr class="even">
<td><strong><code>P.PUSH x</code></strong></td>
<td>-</td>
<td>-</td>
<td>insert value <code>x</code> to the end of the working pattern (like
a stack), destructive to loop length</td>
</tr>
<tr class="odd">
<td><strong><code>PN.PUSH x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>insert value <code>y</code> to the end of pattern <code>x</code>
(like a stack), destructive to loop length</td>
</tr>
<tr class="even">
<td><strong><code>P.POP</code></strong></td>
<td>-</td>
<td>-</td>
<td>return and remove the value from the end of the working pattern
(like a stack), destructive to loop length</td>
</tr>
<tr class="odd">
<td><strong><code>PN.POP x</code></strong></td>
<td>-</td>
<td>-</td>
<td>return and remove the value from the end of pattern <code>x</code>
(like a stack), destructive to loop length</td>
</tr>
<tr class="even">
<td><strong><code>P.MIN</code></strong></td>
<td>-</td>
<td>-</td>
<td>find the first minimum value in the pattern between the START and
END for the working pattern and return its index</td>
</tr>
<tr class="odd">
<td><strong><code>PN.MIN x</code></strong></td>
<td>-</td>
<td>-</td>
<td>find the first minimum value in the pattern between the START and
END for pattern <code>x</code> and return its index</td>
</tr>
<tr class="even">
<td><strong><code>P.MAX</code></strong></td>
<td>-</td>
<td>-</td>
<td>find the first maximum value in the pattern between the START and
END for the working pattern and return its index</td>
</tr>
<tr class="odd">
<td><strong><code>PN.MAX x</code></strong></td>
<td>-</td>
<td>-</td>
<td>find the first maximum value in the pattern between the START and
END for pattern <code>x</code> and return its index</td>
</tr>
<tr class="even">
<td><strong><code>P.SHUF</code></strong></td>
<td>-</td>
<td>-</td>
<td>shuffle the values in active pattern (between its START and
END)</td>
</tr>
<tr class="odd">
<td><strong><code>PN.SHUF x</code></strong></td>
<td>-</td>
<td>-</td>
<td>shuffle the values in pattern <code>x</code> (between its START and
END)</td>
</tr>
<tr class="even">
<td><strong><code>P.ROT n</code></strong></td>
<td>-</td>
<td>-</td>
<td>rotate the values in the active pattern <code>n</code> steps
(between its START and END, negative rotates backward)</td>
</tr>
<tr class="odd">
<td><strong><code>PN.ROT x n</code></strong></td>
<td>-</td>
<td>-</td>
<td>rotate the values in pattern <code>x</code> (between its START and
END, negative rotates backward)</td>
</tr>
<tr class="even">
<td><strong><code>P.REV</code></strong></td>
<td>-</td>
<td>-</td>
<td>reverse the values in the active pattern (between its START and
END)</td>
</tr>
<tr class="odd">
<td><strong><code>PN.REV x</code></strong></td>
<td>-</td>
<td>-</td>
<td>reverse the values in pattern <code>x</code></td>
</tr>
<tr class="even">
<td><strong><code>P.RND</code></strong></td>
<td>-</td>
<td>-</td>
<td>return a value randomly selected between the start and the end
position</td>
</tr>
<tr class="odd">
<td><strong><code>PN.RND x</code></strong></td>
<td>-</td>
<td>-</td>
<td>return a value randomly selected between the start and the end
position of pattern <code>x</code></td>
</tr>
<tr class="even">
<td><strong><code>RND.P</code></strong></td>
<td>-</td>
<td>-</td>
<td>randomize values between START and END of the working pattern
(0..16383). Optional min/max: <code>RND.P min max</code></td>
</tr>
<tr class="odd">
<td><strong><code>RND.PN x</code></strong></td>
<td>-</td>
<td>-</td>
<td>randomize values between START and END of pattern <code>x</code>
(0..16383). Optional min/max: <code>RND.PN x min max</code></td>
</tr>
<tr class="even">
<td><strong><code>P.PA y</code></strong></td>
<td>-</td>
<td>-</td>
<td>add <code>y</code> to each value between START and END of the working
pattern</td>
</tr>
<tr class="odd">
<td><strong><code>PN.PA x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>add <code>y</code> to each value between START and END of pattern
<code>x</code></td>
</tr>
<tr class="even">
<td><strong><code>P.PS y</code></strong></td>
<td>-</td>
<td>-</td>
<td>subtract <code>y</code> from each value between START and END of the
working pattern</td>
</tr>
<tr class="odd">
<td><strong><code>PN.PS x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>subtract <code>y</code> from each value between START and END of
pattern <code>x</code></td>
</tr>
<tr class="even">
<td><strong><code>P.PM y</code></strong></td>
<td>-</td>
<td>-</td>
<td>multiply each value between START and END of the working pattern by
<code>y</code></td>
</tr>
<tr class="odd">
<td><strong><code>PN.PM x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>multiply each value between START and END of pattern <code>x</code> by
<code>y</code></td>
</tr>
<tr class="even">
<td><strong><code>P.PD y</code></strong></td>
<td>-</td>
<td>-</td>
<td>divide each value between START and END of the working pattern by
<code>y</code> (no-op if <code>y</code>=0)</td>
</tr>
<tr class="odd">
<td><strong><code>PN.PD x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>divide each value between START and END of pattern <code>x</code> by
<code>y</code> (no-op if <code>y</code>=0)</td>
</tr>
<tr class="even">
<td><strong><code>P.PMOD y</code></strong></td>
<td>-</td>
<td>-</td>
<td>apply modulo <code>y</code> to each value between START and END of the
working pattern (no-op if <code>y</code>=0)</td>
</tr>
<tr class="odd">
<td><strong><code>PN.PMOD x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>apply modulo <code>y</code> to each value between START and END of
pattern <code>x</code> (no-op if <code>y</code>=0)</td>
</tr>
<tr class="even">
<td><strong><code>P.SCALE a b c d</code></strong></td>
<td>-</td>
<td>-</td>
<td>scale each value between START and END of the working pattern from
range <code>a</code>..<code>b</code> to <code>c</code>..<code>d</code></td>
</tr>
<tr class="odd">
<td><strong><code>PN.SCALE x a b c d</code></strong></td>
<td>-</td>
<td>-</td>
<td>scale each value between START and END of pattern <code>x</code> from
range <code>a</code>..<code>b</code> to <code>c</code>..<code>d</code></td>
</tr>
<tr class="even">
<td><strong><code>P.SUM</code></strong></td>
<td>-</td>
<td>-</td>
<td>sum values between START and END of the working pattern</td>
</tr>
<tr class="odd">
<td><strong><code>PN.SUM x</code></strong></td>
<td>-</td>
<td>-</td>
<td>sum values between START and END of pattern <code>x</code></td>
</tr>
<tr class="even">
<td><strong><code>P.AVG</code></strong></td>
<td>-</td>
<td>-</td>
<td>average values between START and END of the working pattern</td>
</tr>
<tr class="odd">
<td><strong><code>PN.AVG x</code></strong></td>
<td>-</td>
<td>-</td>
<td>average values between START and END of pattern <code>x</code></td>
</tr>
<tr class="even">
<td><strong><code>P.MINV</code></strong></td>
<td>-</td>
<td>-</td>
<td>minimum value between START and END of the working pattern</td>
</tr>
<tr class="odd">
<td><strong><code>PN.MINV x</code></strong></td>
<td>-</td>
<td>-</td>
<td>minimum value between START and END of pattern <code>x</code></td>
</tr>
<tr class="even">
<td><strong><code>P.MAXV</code></strong></td>
<td>-</td>
<td>-</td>
<td>maximum value between START and END of the working pattern</td>
</tr>
<tr class="odd">
<td><strong><code>PN.MAXV x</code></strong></td>
<td>-</td>
<td>-</td>
<td>maximum value between START and END of pattern <code>x</code></td>
</tr>
<tr class="even">
<td><strong><code>P.FND y</code></strong></td>
<td>-</td>
<td>-</td>
<td>return first index between START and END where value equals
<code>y</code> (or -1 if not found)</td>
</tr>
<tr class="odd">
<td><strong><code>PN.FND x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>return first index between START and END where value equals
<code>y</code> in pattern <code>x</code> (or -1 if not found)</td>
</tr>
<tr class="even">
<td><strong><code>P.+ x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>increase the value of the working pattern at index <code>x</code> by
<code>y</code></td>
</tr>
<tr class="odd">
<td><strong><code>PN.+ x y z</code></strong></td>
<td>-</td>
<td>-</td>
<td>increase the value of pattern <code>x</code> at index <code>y</code>
by <code>z</code></td>
</tr>
<tr class="even">
<td><strong><code>P.- x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>decrease the value of the working pattern at index <code>x</code> by
<code>y</code></td>
</tr>
<tr class="odd">
<td><strong><code>PN.- x y z</code></strong></td>
<td>-</td>
<td>-</td>
<td>decrease the value of pattern <code>x</code> at index <code>y</code>
by <code>z</code></td>
</tr>
<tr class="even">
<td><strong><code>P.+W x y a b</code></strong></td>
<td>-</td>
<td>-</td>
<td>increase the value of the working pattern at index <code>x</code> by
<code>y</code> and wrap it to <code>a</code>..<code>b</code> range</td>
</tr>
<tr class="odd">
<td><strong><code>PN.+W x y z a b</code></strong></td>
<td>-</td>
<td>-</td>
<td>increase the value of pattern <code>x</code> at index <code>y</code>
by <code>z</code> and wrap it to <code>a</code>..<code>b</code>
range</td>
</tr>
<tr class="even">
<td><strong><code>P.-W x y a b</code></strong></td>
<td>-</td>
<td>-</td>
<td>decrease the value of the working pattern at index <code>x</code> by
<code>y</code> and wrap it to <code>a</code>..<code>b</code> range</td>
</tr>
<tr class="odd">
<td><strong><code>PN.-W x y z a b</code></strong></td>
<td>-</td>
<td>-</td>
<td>decrease the value of pattern <code>x</code> at index <code>y</code>
by <code>z</code> and wrap it to <code>a</code>..<code>b</code>
range</td>
</tr>
<tr class="even">
<td><strong><code>P.MAP: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td>Replace each cell in the active pattern (between the START and END
of the pattern) by assigning<br />
the variable I to the current value of the cell, evaluating the command
after the mod, and<br />
assigning that pattern cell with the result. The ‘map’ higher-order
function from functional<br />
programming, with the command giving the function of <code>I</code> to
map over the pattern.<br />
<br />
For example:<br />
<br />
&#10;<pre><code>P.MAP: * 2 I  =&gt; double each cell in the active pattern</code></pre>
<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>PN.MAP x: ...</code></strong></td>
<td>-</td>
<td>-</td>
<td>apply the ‘function’ to each value in pattern <code>x</code>,
<code>I</code> takes each pattern value</td>
</tr>
</tbody>
</table>

## Queue

These operators manage a first in, first out, queue of values. The
length of the queue can be dynamically changed up to a maximum size of
64 elements. A fixed length can be set with the `Q.N` operator, or the
queue can grow and shrink automatically by setting `Q.GRW 1`. The queue
contents will be preserved when the length is shortened.

Queues also offer operators that do math on the entire queue (the
`Q.AVG` operator is particularly useful for smoothing input values) or
copy the queue to and from a tracker pattern.

Most operators manipulates the elements up to (and including) length.
Exceptions are `Q.I i x` and `Q.P2`.

Examples, only first 8 elements shown for clarity: By default all
elements of the queue have a value of `0` and the length is set to 1.

    Q.N "length" ->|
    element nb: 1  |  2 3 4 5 6 7 8
    value       0  |  0 0 0 0 0 0 0

Using the Q OP will add values to the beginning of the queue and push
the other elements to the right. `Q 1`

    1  |  0 0 0 0 0 0 0

    Q 2 // add 2 to queue
    Q 3 // add 3 to queue

    3  |  2 1 0 0 0 0 0

Using the `Q` getter OP will return the last element in the queue, but
not modify content or the state of the queue.

    Q // will return 3

    3  |  2 1 0 0 0 0 0

Using the `Q.N` OP will either return the position of the end marker
(1-indexed) or move it:

    Q.N 2 // increace the length to two by moving the end marker:

    3 2  |  1 0 0 0 0 0

    Q // get the value at the end, now `2`

By default grow is disabled, but it can be turned on with `Q.GRW 1`.
With grow enabled, the queue will automatically expand when new elements
are added with `Q x` and likewise shrink when reading with `Q`.

    Q.GRW // enable grow
    3 2  |  1 0 0 0 0 0
    Q 4 // add to to queue
    4 3 2  |  1 0 0 0 0
    Q // read element from queue, will return 2
    4 3  |  2 1 0 0 0 0

<table style="width:98%;">
<colgroup>
<col style="width: 25%" />
<col style="width: 23%" />
<col style="width: 10%" />
<col style="width: 40%" />
</colgroup>
<thead>
<tr class="header">
<th><code>OP</code></th>
<th><code>OP (set)</code></th>
<th><code>Alias</code></th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>Q</code></strong></td>
<td><strong><code>Q x</code></strong></td>
<td>-</td>
<td>Gets the output value from the queue, or places <code>x</code> into
the queue.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>Q.N</code></strong></td>
<td><strong><code>Q.N x</code></strong></td>
<td>-</td>
<td>Gets/sets the length of the queue. The length is 1-indexed.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>Q.AVG</code></strong></td>
<td><strong><code>Q.AVG x</code></strong></td>
<td>-</td>
<td>Getting the value the average of the values in the queue. Setting
<code>x</code> sets the<br />
value of each entry in the queue to <code>x</code>.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>Q.CLR</code></strong></td>
<td><strong><code>Q.CLR x</code></strong></td>
<td>-</td>
<td>Clear queue, set all values to 0, length to 1. If parameter
<code>x</code> is provided, set first elements to <code>x</code>.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>Q.GRW</code></strong></td>
<td><strong><code>Q.GRW x</code></strong></td>
<td>-</td>
<td>If grow is set (value of 1) the queue will automatically grow and
shrink when using <code>Q</code> (popping and pushing).<br />
</td>
</tr>
<tr class="even">
<td><strong><code>Q.SUM</code></strong></td>
<td><strong><code>Q.SUM x</code></strong></td>
<td>-</td>
<td>Get sum of all elements in queue.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>Q.MIN</code></strong></td>
<td><strong><code>Q.MIN x</code></strong></td>
<td>-</td>
<td>Get the minimum value of elements in queue. If <code>x</code> is
provided, set elements with a value less than <code>x</code> to
<code>x</code>.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>Q.MAX</code></strong></td>
<td><strong><code>Q.MAX x</code></strong></td>
<td>-</td>
<td>Get the maximum value of elements in queue. If <code>x</code> is
provided, set elements with a value greater than <code>x</code> to
<code>x</code>.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>Q.RND</code></strong></td>
<td><strong><code>Q.RND x</code></strong></td>
<td>-</td>
<td>Get a random element in queue.<br />
<br />
If <code>x</code> &gt; 0, set all elements to a random value
0-<code>x</code>.<br />
If <code>x</code> &lt; 0, swap two elements <code>-x</code> number of
times.<br />
IF <code>x</code> == 0, do nothing.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>Q.SRT</code></strong></td>
<td><strong><code>Q.SRT</code></strong></td>
<td>-</td>
<td>Sort elements in queue.<br />
With no arguments, entire queue is sorted in accending order.<br />
<br />
If <code>x</code> &gt; 0, sort elements from index <code>i</code> to the
end of queue.<br />
If <code>x</code> &lt; 0, sort elements from beginning of queue to index
<code>-i</code>.<br />
IF <code>x</code> == 0, sort all elements.<br />
<br />
Index <code>i</code> is 0-indexed.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>Q.REV</code></strong></td>
<td>-</td>
<td>-</td>
<td>Reverse order of elements in queue.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>Q.SH</code></strong></td>
<td><strong><code>Q.SH x</code></strong></td>
<td>-</td>
<td>Shift elements <code>x</code> locations to right. Negative values of
<code>x</code> shifts to the left. No value provided is equal to
<code>x</code> = 1. Shifting is wrapped.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>Q.ADD x</code></strong></td>
<td><strong><code>Q.ADD x i</code></strong></td>
<td>-</td>
<td>Add <code>x</code> to all elements in queue. If index <code>i</code>
is provided, only perform addition on element at index
<code>i</code>.<br />
<br />
Index <code>i</code> is 0-indexed.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>Q.SUB x</code></strong></td>
<td><strong><code>Q.SUB x i</code></strong></td>
<td>-</td>
<td>Subtract <code>x</code> from all elements in queue. If index
<code>i</code> is provided, only perform subtraction on element at index
<code>i</code>.<br />
<br />
Index <code>i</code> is 0-indexed.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>Q.MUL x</code></strong></td>
<td><strong><code>Q.MUL x i</code></strong></td>
<td>-</td>
<td>Multiply all elements in queue with <code>x</code>. If index
<code>i</code> is provided, only perform multiplication on element at
index <code>i</code>.<br />
<br />
Index <code>i</code> is 0-indexed.<br />
<br />
</td>
</tr>
<tr class="even">
<td><strong><code>Q.DIV x</code></strong></td>
<td><strong><code>Q.DIV x i</code></strong></td>
<td>-</td>
<td>Divide all elements in queue by <code>x</code>. If index
<code>i</code> is provided, only perform division on element at index
<code>i</code>.<br />
<br />
Index <code>i</code> is 0-indexed.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>Q.MOD x</code></strong></td>
<td><strong><code>Q.MOD x i</code></strong></td>
<td>-</td>
<td>Perform modulo of <code>x</code> (value = value % <code>x</code>) on
all elements in queue. If index <code>i</code> is provided, only perform
modulo operation on element at index <code>i</code>.<br />
<br />
Index <code>i</code> is 0-indexed.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>Q.I i</code></strong></td>
<td><strong><code>Q.I i x</code></strong></td>
<td>-</td>
<td>Get value of element at index <code>i</code> or set value of element
<code>i</code> to value <code>x</code>. Indexing works on entire lenght
of queue, and is not limited to elements below queue end point.<br />
<br />
Index <code>i</code> is 0-indexed.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>Q.2P</code></strong></td>
<td><strong><code>Q.2P i</code></strong></td>
<td>-</td>
<td>Copy entire queue to current pattern or (if <code>i</code> provided)
pattern at index <code>i</code>.<br />
<br />
Index <code>i</code> is 0-indexed.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>Q.P2</code></strong></td>
<td><strong><code>Q.P2 i</code></strong></td>
<td>-</td>
<td>Copy current pattern to queue or (if <code>i</code> provided) copy
pattern at index <code>i</code> to queue.<br />
<br />
Index <code>i</code> is 0-indexed.<br />
</td>
</tr>
</tbody>
</table>

## Turtle

A 2-dimensional, movable index into the pattern values as displayed on
the TRACKER screen.

<table style="width:98%;">
<colgroup>
<col style="width: 25%" />
<col style="width: 23%" />
<col style="width: 10%" />
<col style="width: 40%" />
</colgroup>
<thead>
<tr class="header">
<th><code>OP</code></th>
<th><code>OP (set)</code></th>
<th><code>Alias</code></th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>@</code></strong></td>
<td><strong><code>@ x</code></strong></td>
<td>-</td>
<td>get or set the current pattern value under the turtle</td>
</tr>
<tr class="even">
<td><strong><code>@X</code></strong></td>
<td><strong><code>@X x</code></strong></td>
<td>-</td>
<td>get the turtle X coordinate, or set it to x</td>
</tr>
<tr class="odd">
<td><strong><code>@Y</code></strong></td>
<td><strong><code>@Y x</code></strong></td>
<td>-</td>
<td>get the turtle Y coordinate, or set it to x</td>
</tr>
<tr class="even">
<td><strong><code>@MOVE x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>move the turtle x cells in the X axis and y cells in the Y axis</td>
</tr>
<tr class="odd">
<td><strong><code>@F x1 y1 x2 y2</code></strong></td>
<td>-</td>
<td>-</td>
<td>set the turtle’s fence to corners x1,y1 and x2,y2</td>
</tr>
<tr class="even">
<td><strong><code>@FX1</code></strong></td>
<td><strong><code>@FX1 x</code></strong></td>
<td>-</td>
<td>get the left fence line or set it to x</td>
</tr>
<tr class="odd">
<td><strong><code>@FX2</code></strong></td>
<td><strong><code>@FX2 x</code></strong></td>
<td>-</td>
<td>get the right fence line or set it to x</td>
</tr>
<tr class="even">
<td><strong><code>@FY1</code></strong></td>
<td><strong><code>@FY1 x</code></strong></td>
<td>-</td>
<td>get the top fence line or set it to x</td>
</tr>
<tr class="odd">
<td><strong><code>@FY2</code></strong></td>
<td><strong><code>@FY2 x</code></strong></td>
<td>-</td>
<td>get the bottom fence line or set it to x</td>
</tr>
<tr class="even">
<td><strong><code>@SPEED</code></strong></td>
<td><strong><code>@SPEED x</code></strong></td>
<td>-</td>
<td>get the speed of the turtle’s <span class="citation"
data-cites="STEP">@STEP</span> in cells per step or set it to x</td>
</tr>
<tr class="odd">
<td><strong><code>@DIR</code></strong></td>
<td><strong><code>@DIR x</code></strong></td>
<td>-</td>
<td>get the direction of the turtle’s <span class="citation"
data-cites="STEP">@STEP</span> in degrees or set it to x</td>
</tr>
<tr class="even">
<td><strong><code>@STEP</code></strong></td>
<td>-</td>
<td>-</td>
<td>move <code>@SPEED</code>/100 cells forward in <code>@DIR</code>,
triggering <code>@SCRIPT</code> on cell change</td>
</tr>
<tr class="odd">
<td><strong><code>@BUMP</code></strong></td>
<td><strong><code>@BUMP 1</code></strong></td>
<td>-</td>
<td>get whether the turtle fence mode is BUMP, or set it to BUMP with
1</td>
</tr>
<tr class="even">
<td><strong><code>@WRAP</code></strong></td>
<td><strong><code>@WRAP 1</code></strong></td>
<td>-</td>
<td>get whether the turtle fence mode is WRAP, or set it to WRAP with
1</td>
</tr>
<tr class="odd">
<td><strong><code>@BOUNCE</code></strong></td>
<td><strong><code>@BOUNCE 1</code></strong></td>
<td>-</td>
<td>get whether the turtle fence mode is BOUNCE, or set it to BOUNCE
with 1</td>
</tr>
<tr class="even">
<td><strong><code>@SCRIPT</code></strong></td>
<td><strong><code>@SCRIPT x</code></strong></td>
<td>-</td>
<td>get which script runs when the turtle changes cells, or set it to
x</td>
</tr>
<tr class="odd">
<td><strong><code>@SHOW</code></strong></td>
<td><strong><code>@SHOW 0/1</code></strong></td>
<td>-</td>
<td>get whether the turtle is displayed on the TRACKER screen, or turn
it on or off</td>
</tr>
</tbody>
</table>

## Grid

Grid operators allow creating scenes that can interact with grid
connected to teletype (important: grid must be powered externally, do
not connect it directly to teletype!). You can light up individual LEDs,
draw shapes and create controls (such as buttons and faders) that can be
used to trigger and control scripts. You can take advantage of grid
operators even without an actual grid by using the built in Grid
Visualizer.

For more information on grid integration see Advanced section and <a
href="https://github.com/scanner-darkly/teletype/wiki/GRID-INTEGRATION"
target="_blank">Grid Studies</a>.

As there are many operators let’s review some naming conventions that
apply to the majority of them. All grid ops start with `G.`. For control
related ops this is followed by 3 letters specifying the control:
`G.BTN` for buttons, `G.FDR` for faders. To define a control you use the
main ops `G.BTN` and `G.FDR`. To define multiple controls replace the
last letter with `X`: `G.BTX`, `G.FDX`.

All ops that initialize controls use the same list of parameters: id,
coordinates, width, height, type, level, script. When creating multiple
controls there are two extra parameters: the number of columns and the
number of rows. Controls are created in the current group (set with
`G.GRP`). To specify a different group use the group versions of the 4
above ops - `G.GBT`, `G.GFD`, `G.GBX`, `G.GFX` and specify the desired
group as the first parameter.

All controls share some common properties, referenced by adding a `.`
and:

- `EN`: `G.BTN.EN`, `G.FDR.EN` - enables or disables a control
- `V`: `G.BTN.V`, `G.FDR.V` - value, 1/0 for buttons, range value for
  faders
- `L`: `G.BTN.L`, `G.FDR.L` - level (brightness level for buttons and
  coarse faders, max value level for fine faders)
- `X`: `G.BTN.X`, `G.FDR.X` - the X coordinate
- `Y`: `G.BTN.Y`, `G.FDR.Y` - the Y coordinate

To get/set properties for individual controls you normally specify the
control id as the first parameter: `G.FDR.V 5` will return the value of
fader 5. Quite often the actual id is not important, you just want to
work with the latest control pressed. As these are likely the ops to be
used most often they are offered as shortcuts without a `.`: `G.BTNV`
returns the value of the last button pressed, `G.FDRL 4` will set the
level of the last fader pressed etc etc.

<table style="width:98%;">
<colgroup>
<col style="width: 25%" />
<col style="width: 23%" />
<col style="width: 10%" />
<col style="width: 40%" />
</colgroup>
<thead>
<tr class="header">
<th><code>OP</code></th>
<th><code>OP (set)</code></th>
<th><code>Alias</code></th>
<th>Description</th>
</tr>
</thead>
<tbody>
<tr class="odd">
<td><strong><code>G.RST</code></strong></td>
<td>-</td>
<td>-</td>
<td>Full grid reset - hide all controls and reset their properties to
the default<br />
values, clear all LEDs, reset the dim level and the grid rotation.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.CLR</code></strong></td>
<td>-</td>
<td>-</td>
<td>Clear all LEDs set with <code>G.LED</code>, <code>G.REC</code> or
<code>G.RCT</code>.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.DIM level</code></strong></td>
<td>-</td>
<td>-</td>
<td>Set the dim level (0..14, higher values dim more). To remove set to
0.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.ROTATE x</code></strong></td>
<td>-</td>
<td>-</td>
<td>Set the grid rotation (0 - no rotation, 1 - rotate by 180
degrees).<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.KEY x y action</code></strong></td>
<td>-</td>
<td>-</td>
<td>Emulate a grid key press at the specified coordinates (0-based). Set
<code>action</code><br />
to 1 to emulate a press, 0 to emulate a release. You can also emulate a
button<br />
press with <code>G.BTN.PR</code> and a fader press with
<code>G.FDR.PR</code>.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.GRP</code></strong></td>
<td><strong><code>G.GRP id</code></strong></td>
<td>-</td>
<td>Get or set the current group. Grid controls created without
specifying a group<br />
will be assigned to the current group. This op doesn’t enable/disable
groups -<br />
use <code>G.GRP.EN</code> for that. The default current group is 0. 64
groups are<br />
available.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.GRP.EN id</code></strong></td>
<td><strong><code>G.GRP.EN id x</code></strong></td>
<td>-</td>
<td>Enable or disable the specified group or check if it’s currently
enabled.<br />
1 means enabled, 0 means disabled. Enabling or disabling a group enables
/<br />
disables all controls assigned to that group (disabled controls are not
shown<br />
and receive no input). This allows groups to be used as pages -
initialize<br />
controls in different groups, and then simply enable one group at a
time.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.GRP.RST id</code></strong></td>
<td>-</td>
<td>-</td>
<td>Reset all controls associated with the specified group. This will
disable<br />
the controls and reset their properties to the default values. This will
also<br />
reset the fader scale range to 0..16383.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.GRP.SW id</code></strong></td>
<td>-</td>
<td>-</td>
<td>Switch groups. Enables the specified group, disables all
others.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.GRP.SC id</code></strong></td>
<td><strong><code>G.GRP.SC id script</code></strong></td>
<td>-</td>
<td>Assign a script to the specified group, or get the currently
assigned script.<br />
The script gets executed whenever a control associated with the group
receives<br />
input. It is possible to have different scripts assigned to a control
and<br />
the group it belongs to. Use 9 for Metro and 10 for Init. To unassign,
set it<br />
to 0.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.GRPI</code></strong></td>
<td>-</td>
<td>-</td>
<td>Get the id of the last group that received input. This is useful
when sharing<br />
a script between multiple groups.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.LED x y</code></strong></td>
<td><strong><code>G.LED x y level</code></strong></td>
<td>-</td>
<td>Set the LED level or get the current level at the specified
coordinates.<br />
Possible level range is 0..15 (on non varibright grids anything below 8
is<br />
‘off’, 8 or above is ‘on’).<br />
<br />
Grid controls get rendered first, and LEDs are rendered last. This means
you can<br />
use LEDs to accentuate certain areas of the UI. This also means that any
LEDs<br />
that are set will block whatever is underneath them, even with the level
of 0.<br />
In order to completely clear an LED set its level to -3. There are two
other<br />
special values for brightness: -1 will dim, and -2 will brighten
what’s<br />
underneath. They can be useful to highlight the current sequence step,
for<br />
instance.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.LED.C x y</code></strong></td>
<td>-</td>
<td>-</td>
<td>Clear the LED at the specified coordinates. This is the same as
setting<br />
the brightness level to -3. To clear all LEDs use
<code>G.CLR</code>.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.REC x y w h fill border</code></strong></td>
<td>-</td>
<td>-</td>
<td>Draw a rectangle with the specified width and height. <code>x</code>
and <code>y</code> are<br />
the coordinates of the top left corner. Coordinates are 0-based, with
the 0,0<br />
point located at the top left corner of the grid. You can draw
rectangles that<br />
are partially outside of the visible area, and they will be properly
cropped.<br />
<br />
<code>fill</code> and <code>border</code> specify the brightness levels
for the inner area and<br />
the one-LED-wide border respectively, 0..15 range. You can use the three
special<br />
brightness levels: -1 to dim, -2 to brighten and -3 for transparency
(you could<br />
draw just a frame by setting <code>fill</code> to -3, for
instance).<br />
<br />
To draw lines, set the width or the height to 1. In this case only
<code>border</code><br />
brightness level is used.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.RCT x1 y1 x2 y2 fill border</code></strong></td>
<td>-</td>
<td>-</td>
<td>Same as <code>G.REC</code> but instead of specifying the width and
height you specify<br />
the coordinates of the top left corner and the bottom right
corner.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.BTN id x y w h type level script</code></strong></td>
<td>-</td>
<td>-</td>
<td>Initializes and enables a button with the specified id. 256 buttons
are<br />
available (ids are 0-based so the possible id range is 0..255. The
button will<br />
be assigned to the current group (set with <code>G.GRP</code>). Buttons
can be<br />
reinitialized at any point.<br />
<br />
<code>x</code> and <code>y</code> specify the coordinates of the top
left corner, and <code>w</code> and <code>h</code><br />
specify width and height respectively. <code>type</code> determines
whether the button is<br />
latching (1) or momentary (0). <code>level</code> sets the “off”
brightness level, possible<br />
rand is -3..15 (the brightness level for pressed buttons is fixed at
13).<br />
<br />
<code>script</code> specifies the script to be executed when the button
is pressed or<br />
released (the latter only for momentary buttons). Use 9 for Metro and 10
for<br />
Init. Use 0 if you don’t need a script assigned.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.GBT group id x y w h type level script</code></strong></td>
<td>-</td>
<td>-</td>
<td>Initialize and enable a button. Same as <code>G.BTN</code> but you
can also choose which<br />
group to assign the button too.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.BTX id x y w h type level script columns rows</code></strong></td>
<td>-</td>
<td>-</td>
<td>Initialize and enable a block of buttons in the current group with
the specified<br />
number of columns and rows . Ids are incremented sequentially by columns
and<br />
then by rows.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.GBX group id x y w h type level script columns rows</code></strong></td>
<td>-</td>
<td>-</td>
<td>Initialize and enable a block of buttons. Same as <code>G.BTX</code>
but you can also<br />
choose which group to assign the buttons too.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.BTN.EN id</code></strong></td>
<td><strong><code>G.BTN.EN id x</code></strong></td>
<td>-</td>
<td>Enable (set <code>x</code> to 1) or disable (set <code>x</code> to
0) a button with the specified id,<br />
or check if it’s currently enabled. Disabling a button hides it and
stops it<br />
from receiving input but keeps all the other properties (size/location
etc)<br />
intact.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.BTN.X id</code></strong></td>
<td><strong><code>G.BTN.X id x</code></strong></td>
<td>-</td>
<td>Get or set <code>x</code> coordinate for the specified button’s top
left corner.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.BTN.Y id</code></strong></td>
<td><strong><code>G.BTN.Y id y</code></strong></td>
<td>-</td>
<td>Get or set <code>y</code> coordinate for the specified button’s top
left corner.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.BTN.V id</code></strong></td>
<td><strong><code>G.BTN.V id value</code></strong></td>
<td>-</td>
<td>Get or set the specified button’s value. For buttons the value of 1
means<br />
the button is pressed and 0 means it’s not. If there is a script
assigned<br />
to the button it will not be triggered if you change the value -
use<br />
<code>G.BTN.PR</code> for that.<br />
<br />
Button values don’t change when a button is disabled. Button values are
stored<br />
with the scene (both to flash and to USB sticks).<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.BTN.L id</code></strong></td>
<td><strong><code>G.BTN.L id level</code></strong></td>
<td>-</td>
<td>Get or set the specified button’s brightness level (-3..15). Please
note you<br />
can only set the level for unpressed buttons, the level for pressed
buttons is<br />
fixed at 13.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.BTNI</code></strong></td>
<td>-</td>
<td>-</td>
<td>Get the id of the last pressed button. This is useful when multiple
buttons are<br />
assigned to the same script.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.BTNX</code></strong></td>
<td><strong><code>G.BTNX x</code></strong></td>
<td>-</td>
<td>Get or set <code>x</code> coordinate of the last pressed button’s
top left corner. This is<br />
the same as <code>G.BTN.X G.BTNI</code>.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.BTNY</code></strong></td>
<td><strong><code>G.BTNY y</code></strong></td>
<td>-</td>
<td>Get or set <code>y</code> coordinate of the last pressed button’s
top left corner. This is<br />
the same as <code>G.BTN.Y G.BTNI</code>.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.BTNV</code></strong></td>
<td><strong><code>G.BTNV value</code></strong></td>
<td>-</td>
<td>Get or set the value of the last pressed button. This is the same
as<br />
<code>G.BTN.V G.BTNI</code>. This op is especially useful with momentary
buttons when you<br />
want to react to presses or releases only - just put
<code>IF EZ G.BTNV: BREAK</code> in<br />
the beginning of the assigned script (this will ignore releases, to
ignore<br />
presses replace <code>NZ</code> with <code>EZ</code>).<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.BTNL</code></strong></td>
<td><strong><code>G.BTNL level</code></strong></td>
<td>-</td>
<td>Get or set the brightness level of the last pressed button. This is
the same as<br />
<code>G.BTN.L G.BTNI</code>.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.BTN.SW id</code></strong></td>
<td>-</td>
<td>-</td>
<td>Set the value of the specified button to 1 (pressed), set it to 0
(not pressed)<br />
for all other buttons within the same group (useful for creating radio
buttons).<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.BTN.PR id action</code></strong></td>
<td>-</td>
<td>-</td>
<td>Emulate pressing/releasing the specified button. Set
<code>action</code> to <code>1</code> for press,<br />
<code>0</code> for release (<code>action</code> is ignored for latching
buttons).<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.GBTN.V group value</code></strong></td>
<td>-</td>
<td>-</td>
<td>Set the value for all buttons in the specified group.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.GBTN.L group odd_level even_level</code></strong></td>
<td>-</td>
<td>-</td>
<td>Set the brightness level (0..15) for all buttons in the specified
group. You can<br />
use different values for odd and even buttons (based on their index
within the<br />
group, not their id) - this can be a good way to provide some visual
guidance.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.GBTN.C group</code></strong></td>
<td>-</td>
<td>-</td>
<td>Get the total count of all the buttons in the specified group that
are currently<br />
pressed.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.GBTN.I group index</code></strong></td>
<td>-</td>
<td>-</td>
<td>Get the id of a currently pressed button within the specified group
by its index<br />
(0-based). The index should be between 0 and C-1 where C is the total
count of<br />
all pressed buttons (you can get it using <code>G.GBTN.C</code>).<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.GBTN.W group</code></strong></td>
<td>-</td>
<td>-</td>
<td>Get the width of the rectangle formed by pressed buttons within the
specified<br />
group. This is basically the distance between the leftmost and the
rightmost<br />
pressed buttons, inclusive. This op is useful for things like setting a
loop’s<br />
length, for instance. To do so, check if there is more than one button
pressed<br />
(using <code>G.GBTN.C</code>) and if there is, use <code>G.GBTN.W</code>
to set the length.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.GBTN.H group</code></strong></td>
<td>-</td>
<td>-</td>
<td>Get the height of the rectangle formed by pressed buttons within the
specified<br />
group (see <code>G.GBTN.W</code> for more details).<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.GBTN.X1 group</code></strong></td>
<td>-</td>
<td>-</td>
<td>Get the X coordinate of the leftmost pressed button in the specified
group. If<br />
no buttons are currently pressed it will return -1.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.GBTN.X2 group</code></strong></td>
<td>-</td>
<td>-</td>
<td>Get the X coordinate of the rightmost pressed button in the
specified group. If<br />
no buttons are currently pressed it will return -1.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.GBTN.Y1 group</code></strong></td>
<td>-</td>
<td>-</td>
<td>Get the Y coordinate of the highest pressed button in the specified
group. If no<br />
buttons are currently pressed it will return -1.<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>G.GBTN.Y2 group</code></strong></td>
<td>-</td>
<td>-</td>
<td>Get the Y coordinate of the lowest pressed button in the specified
group. If no<br />
buttons are currently pressed it will return -1.<br />
</td>
</tr>
<tr class="even">
<td><strong><code>G.FDR id x y w h type level script</code></strong></td>
<td>-</td>
<td>-</td>
<td>Initializes and enables a fader with the specified id. 64 faders are
available<br />
(ids are 0-based so the possible id range is 0..63). The fader will be
assigned<br />
to the current group (set with <code>G.GRP</code>). Faders can be
reinitialized at any<br />
point.<br />
<br />
<code>x</code> and <code>y</code> specify the coordinates of the top
left corner, and <code>w</code> and <code>h</code><br />
specify width and height respectively.<br />
<br />
<code>type</code> determines the fader type and orientation. Possible
values are:<br />
<br />
* 0 - coarse, horizontal bar<br />
* 1 - coarse, vertical bar<br />
* 2 - coarse, horizontal dot<br />
* 3 - coarse, vertical dot<br />
* 4 - fine, horizontal bar<br />
* 5 - fine, vertical bar<br />
* 6 - fine, horizontal dot<br />
* 7 - fine, vertical dot<br />
<br />
Coarse faders have the possible range of 0..N-1 where N is width for
horizontal<br />
faders or height for vertical faders. Pressing anywhere within the fader
area<br />
sets the fader value accordingly. Fine faders allow selecting a bigger
range<br />
of values by mapping the range to the fader’s height or width and
dedicating<br />
the edge buttons for incrementing/decrementing. Fine faders employ<br />
varibrightness to reflect the current value.<br />
<br />
<code>level</code> has a different meaning for coarse and fine faders.
For coarse faders<br />
it selects the background brightness level (similar to buttons). For
fine faders<br />
this is the maximum value level (the minimum level being 0). In order to
show<br />
each value distinctly using varibright the maximum level possible is the
number<br />
of available buttons multiplied by 16 minus 1 (since range is 0-based).
Remember<br />
that 2 buttons are always reserved for increment/decrement. Using a
larger<br />
number is allowed - it will be automatically adjusted to what’s
possible.<br />
<br />
<code>script</code> specifies the script to be executed when the fader
value is changed.<br />
Use 9 for Metro and 10 for Init. Use 0 if you don’t need a script
assigned.<br />
</tr>
<tr class="odd">
<td><strong><code>MI.LNV</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the latest Note On scaled to teletype range (shortcut for
<code>N MI.LN</code>)</td>
</tr>
<tr class="even">
<td><strong><code>MI.LV</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the latest velocity (0..127)</td>
</tr>
<tr class="odd">
<td><strong><code>MI.LVV</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the latest velocity scaled to 0..16383 range (0..+10V)</td>
</tr>
<tr class="even">
<td><strong><code>MI.LO</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the latest Note Off (0..127)</td>
</tr>
<tr class="odd">
<td><strong><code>MI.LC</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the latest controller number (0..127)</td>
</tr>
<tr class="even">
<td><strong><code>MI.LCC</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the latest controller value (0..127)</td>
</tr>
<tr class="odd">
<td><strong><code>MI.LCCV</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the latest controller value scaled to 0..16383 range
(0..+10V)</td>
</tr>
<tr class="even">
<td><strong><code>MI.NL</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the number of Note On events</td>
</tr>
<tr class="odd">
<td><strong><code>MI.NCH</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the Note On event channel (1..16) at index specified by variable
<code>I</code></td>
</tr>
<tr class="even">
<td><strong><code>MI.N</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the Note On (0..127) at index specified by variable
<code>I</code></td>
</tr>
<tr class="odd">
<td><strong><code>MI.NV</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the Note On scaled to 0..+10V range at index specified by
variable <code>I</code></td>
</tr>
<tr class="even">
<td><strong><code>MI.V</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the velocity (0..127) at index specified by variable
<code>I</code></td>
</tr>
<tr class="odd">
<td><strong><code>MI.VV</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the velocity scaled to 0..10V range at index specified by
variable <code>I</code></td>
</tr>
<tr class="even">
<td><strong><code>MI.OL</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the number of Note Off events</td>
</tr>
<tr class="odd">
<td><strong><code>MI.OCH</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the Note Off event channel (1..16) at index specified by
variable <code>I</code></td>
</tr>
<tr class="even">
<td><strong><code>MI.O</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the Note Off (0..127) at index specified by variable
<code>I</code></td>
</tr>
<tr class="odd">
<td><strong><code>MI.CL</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the number of controller events</td>
</tr>
<tr class="even">
<td><strong><code>MI.CCH</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the controller event channel (1..16) at index specified by
variable <code>I</code></td>
</tr>
<tr class="odd">
<td><strong><code>MI.C</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the controller number (0..127) at index specified by variable
<code>I</code></td>
</tr>
<tr class="even">
<td><strong><code>MI.CC</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the controller value (0..127) at index specified by variable
<code>I</code></td>
</tr>
<tr class="odd">
<td><strong><code>MI.CCV</code></strong></td>
<td>-</td>
<td>-</td>
<td>get the controller value scaled to 0..+10V range at index specified
by variable <code>I</code></td>
</tr>
<tr class="even">
<td><strong><code>MI.CLKD</code></strong></td>
<td><strong><code>MI.CLKD x</code></strong></td>
<td>-</td>
<td>set clock divider to <code>x</code> (1-24) or get the current
divider</td>
</tr>
<tr class="odd">
<td><strong><code>MI.CLKR</code></strong></td>
<td>-</td>
<td>-</td>
<td>reset clock counter</td>
</tr>
</tbody>
</table>

## Calibration

<table style="width:98%;">
<colgroup>
<col style="width: 25%" />
<col style="width: 23%" />
<col style="width: 10%" />
<col style="width: 40%" />
</colgroup>
<thead>
<tr class="header">
<th><code>OP</code></th>
<th><code>OP (set)</code></th>
<th><code>Alias</code></th>
<th>Description</th>
</tr>
</thead>
<tbody>
</td>
</tr>
<tr class="even">
<td><strong><code>IN.CAL.MIN</code></strong></td>
<td>-</td>
<td>-</td>
<td>1. Connect a patch cable from a calibrated voltage source<br />
2. Set the voltage source to 0 volts<br />
3. Execute IN.CAL.MIN from the live terminal<br />
4. Call IN and confirm the 0 result<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>IN.CAL.MAX</code></strong></td>
<td>-</td>
<td>-</td>
<td>5. Set the voltage source to target maximum voltage (10V)<br />
6. Execute IN.CAL.MAX from the live terminal<br />
7. Call IN and confirm that the result is 16383<br />
</td>
</tr>
<tr class="even">
<td><strong><code>IN.CAL.RESET</code></strong></td>
<td>-</td>
<td>-</td>
<td>Resets the input CV calibration</td>
</tr>
<tr class="odd">
<td><strong><code>PARAM.CAL.MIN</code></strong></td>
<td>-</td>
<td>-</td>
<td>1. Turn the PARAM knob all the way to the left<br />
2. Execute PARAM.CAL.MIN from the live terminal<br />
3. Call PARAM and confirm the 0 result<br />
</td>
</tr>
<tr class="even">
<td><strong><code>PARAM.CAL.MAX</code></strong></td>
<td>-</td>
<td>-</td>
<td>4. Turn the knob all the way to the right<br />
5. Execute PARAM.CAL.MAX from the live terminal<br />
6. Call PARAM and verify that the result is 16383<br />
</td>
</tr>
<tr class="odd">
<td><strong><code>PARAM.CAL.RESET</code></strong></td>
<td>-</td>
<td>-</td>
<td>Resets the Parameter Knob calibration</td>
</tr>
<tr class="even">
<td><strong><code>CV.CAL n mv1v mv3v</code></strong></td>
<td>-</td>
<td>-</td>
<td>Following a short calibration procedure, you can use
<code>CV.CAL</code> to more<br />
precisely match your CV outputs to each other or to an external
reference. A<br />
digital multimeter (or other voltage measuring device) is
required.<br />
<br />
To calibrate CV 1, first set it to output one volt with
<code>CV 1 V 1</code>. Using<br />
a digital multimeter with at least millivolt precision (three digits
after<br />
the decimal point), record the measured output of CV 1 between tip and
sleeve<br />
on a patch cable. Then set CV 1 to three volts with
<code>CV 1 V 3</code> and measure<br />
again.<br />
<br />
Once you have both measurements, use the observed 1V and 3V values
in<br />
millivolts as the second and third arguments to <code>CV.CAL</code>. For
example, if you<br />
measured 0.990V and 2.984V, enter <code>CV.CAL 1 990 2984</code>. (If
both your<br />
measurements are within 1 or 2 millivolts already, there’s no need to
run<br />
<code>CV.CAL</code>.)<br />
<br />
Measure the output with <code>CV 1 V 1</code> and <code>CV 1 V 3</code>
again and confirm the values<br />
are closer to the expected 1.000V and 3.000V.<br />
<br />
Repeat the above steps for CV 2-4, if desired. The calibration data is
stored<br />
in flash memory so you only need to go through this process once.<br />
