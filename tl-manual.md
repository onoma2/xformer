

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

# Updates

## v5.0.0

- **FIX**: fix off-by-one error in `P.ROT` understanding of pattern
  length
- **FIX**: fix `CROW.Q3` calls `ii.self.query2` instead of
  `ii.self.query3`
- **FIX**: cache currently-running commands to avoid corruption during
  SCENE ops.
- **FIX**: delay when opening docs
- **FIX**: `PROB 100` would execute only 99.01% of the time.
- **FIX**: some `G.FDR` configurations caused incorrect rendering in
  grid visualizer
- **FIX**: fix `EX.LP` not returning correct values
- **FIX**: fix `QT.B` handling of negative voltage input
- **IMP**: scene load/save code refactor, add scene load/save tests
- **IMP**: fader ops now support up to four faderbanks
- **NEW**: new Disting EX ops: dual algorithms, `EX.M.N#`, `EX.M.NO#`,
  `EX.M.CC#`
- **FIX**: reset M timer when changing metro rate
- **NEW**: drum ops: `DR.P`, `DR.V`, `DR.TR`
- **NEW**: <a href="https://github.com/attowatt/i2c2midi"
  target="_blank">I2C2MIDI</a> ops
- **FIX**: fix BPM rounding error
- **FIX**: support all line ending types for USB load
- **FIX**: fix `STATE` not accounting for `DEVICE.FLIP`
- **FIX**: fix MIDI IN ops channel number being off by 1
- **FIX**: improve `TR.P` accuracy
- **FIX**: fix `KILL` not stopping TR pulses in progress
- **NEW**: new op: `SCALE0` / `SCL0`
- **NEW**: new ops: `$F`, `$F1`, `$F2`, `$L`, `$L1`, `$L2`, `$S`, `$S1`,
  `$S2`, `I1`, `I2`, `FR`
- **NEW**: new op: `CV.GET`
- **NEW**: basic menu for reading/writing scenes when a USB stick is
  inserted
- **NEW**: new ops: `CV.CAL` and `CV.CAL.RESET` to calibrate CV outputs
- **FIX**: N.CS scales 7 & 8 were incorrectly swapped; make them
  consistent with N.S and docs
- **FIX**: libavr32 update: support CDC grid size detection (e.g. zero),
  increase HID message buffer
- **NEW**: new Disting EX ops: `EX.CH`, `EX.#`, `EX.N#`, `EX.NO#`
- **NEW**: apply VCV Rack compatibility patches, so branches off main
  can be used in both hardware and software
- **FIX**: update Disting EX looper ops to work with Disting EX firmware
  1.23+
- **NEW**: new dual W/ ops: `W/.SEL`, `W/S.POLY`, `W/S.POLY.RESET`,
  `W/1`, `W/2`
- **NEW**: split cheatsheets into separate PDFs for core ops and i2c

## v4.0.0

- **FIX**: `LAST SCRIPT` in live mode gives time since init script was
  run
- **FIX**: negative pattern values are properly read from USB
- **FIX**: delay when navigating to sections in docs
- **NEW**: generic i2c ops: `IIA`, `IIS..`, `IIQ..`, `IIB..`
- **NEW**: exponential delay operator `DEL.G`
- **NEW**: binary and hex format for numbers: `B...`, `X...`
- **NEW**: Disting EX ops
- **FIX**: `LAST n` is broken for script 1
- **NEW**: bitmasked delay and quantize: `DEL.B..`, `QT.B..`, `QT.BX..`
- **NEW**: scale and chord quantize: `QT.S..`, `QT.CS..`
- **NEW**: bit toggle OP: `BTOG..`
- **NEW**: volts to semitones helper OP: `VN..`
- **IMP**: DELAY\_SIZE increased to 64 from 16
- **FIX**: scale degree arguments 1-indexed: `N.S`, `N.CS`
- **NEW**: Just Friends 4.0 OPs and dual JF OPs
- **NEW**: binary scale ops `N.B` and `N.BX`
- **NEW**: reverse binary for numbers: `R...`
- **NEW**: reverse binary OP: `BREV`
- **NEW**: `ES.CV` read earthsea CV values
- **NEW**: added setter for `R`, sets R.MIN and R.MAX to same value,
  allowing R to be used as variable
- **NEW**: v/oct to hz/v conversion op: `HZ`
- **FIX**: W/2.0 ops added
- **NEW**: W/2.0 ops documentation
- **NEW**: `><`, `<>`, `>=<` and `<=>` OPs, checks if value is within or
  outside of range
- **IMP**: new powerful Q OPs
- **IMP**: Improved line editing movement (forward/backward by word
  skips intervening space).
- **NEW**: Delete to end of word command `alt-d` added.
- **NEW**: new multi-logic OPs `AND3`, `AND4`, `OR3` and `OR4` with
  aliases `&&&`, `&&&&`, `|||` and `||||`
- **NEW**: ops to control live mode: `LIVE.OFF`, `LIVE.VARS`,
  `LIVE.GRID`, `LIVE.DASH`, `PRINT`
- **NEW**: `SCENE.P` OP: load another scene but keep current pattern
  state
- **NEW**: alias: `EV` for `EVERY`
- **NEW**: live mode dashboard
- **NEW**: ops to control live mode: `LIVE.OFF`, `LIVE.VARS`,
  `LIVE.GRID`, `LIVE.DASH`, `PRINT`
- **FIX**: `PN.ROT` parameters are swapped
- **FIX**: better rendering for fine grid faders
- **FIX**: logical operators should treat all non zero values as `true`,
  not just positive values
- **NEW**: crow ops
- **NEW**: `TI.PRM.CALIB` alias added (was already in the docs)
- **FIX**: `SCENE` would crash if parameter was out of bounds

## v3.2.0

- **FIX**: improve DAC latency when using `CV` ops
- **NEW**: call metro / init with `SCRIPT 9` / `SCRIPT 10`
- **NEW**: forward (C-f or C-s) and reverse (C-r) search in help mode
- **NEW**: new ops: `LROT` (alias `<<<`), `RROT` (alias `>>>`)
- **NEW**: `LSH` and `RSH` shift the opposite direction when passed a
  negative shift amount
- **NEW**: new op: `SGN` (sign of argument)
- **NEW**: new kria remote op: `KR.DUR`
- **NEW**: new op: `NR` (binary math pattern generator)
- **NEW**: new ops: `N.S, N.C, N.CS` (use western scales and chords to
  get values from `N` table)
- **NEW**: new ops:
  `FADER.SCALE, FADER.CAL.MIN, FADER.CAL.MAX, FADER.CAL.RESET` for
  scaling 16n Faderbank values (aliases
  `FB.S, FB.C.MIN, FB.C.MAX, FB.C.R`)
- **NEW**: new Tracker mode keybinding `alt-[ ]` semitone up, down
- **NEW**: new Tracker mode keybinding `ctrl-[ ]` fifth up, down
- **NEW**: new Tracker mode keybinding `shift-[ ]` octave up, down
- **NEW**: new Tracker mode keybinding `alt-<0-9>` `<0-9>` semitones up
  (0=10, 1=11)
- **NEW**: new Tracker mode keybinding `shift-alt-<0-9>` `<0-9>`
  semitones down (0=10, 1=11)
- **FIX**: dim M in edit mode when metro inactive
- **NEW**: new pattern ops: `P.SHUF`, `PN.SHUF`, `P.REV`, `PN.REV`,
  `P.ROT`, `PN.ROT`
- **NEW**: new pattern mods: `P.MAP:`, `PN.MAP x:`

## Version 3.1

### New operators

`DEVICE.FLIP` - change how screen is displayed and how I/O are numbered
to let you mount the module upside down

`DEL.X`, `DEL.R` - repeat an action multiple times, separated by a delay

`J` & `K` local script variables

`SEED`, `R.SEED`, `TOSS.SEED`, `DRUNK.SEED`, `P.SEED`, `PROB.SEED` -
get/set seed for different random ops

`SCENE.G` - load another scene but keep the current grid configuration

`SCRIPT.POL` / `$.POL` - get / set script polarity. 1 to fire on rising
edges as usual, 2 for falling edges, 3 for both. indicated on live mode
w/ mutes icon.

#### New Ansible ops

`ANS.G` / `ANS.G.P` - simulate ansible receiving a grid key press

`ANS.A` - simulate ansible receiving an arc encoder turn

`ANS.G.LED` / `ANS.A.LED` - read LED brightness of ansible grid / arc

#### New Kria ops

`KR.CUE` - get / set the cued Kria pattern

`KR.PG` - switch to Kria parameter page

### Changes

DELAY\_SIZE increased to 16 from 8

### Bug fixes

<a href="https://github.com/monome/teletype/issues/156"
target="_blank">some keyboards losing keystrokes</a>

<a href="https://github.com/monome/teletype/issues/174"
target="_blank">metro rate not updated after <code>INIT.SCENE</code></a>

## Version 3.0

### Major new features

#### Grid Integration

Grid integration allows you to use grid to visualize, control and
execute teletype scripts. You can create your own UIs using grid ops, or
control Teletype directly with the Grid Control mode. Built in Grid
Visualizer allows designing and using grid scenes without a grid. For
more information and examples of grid scenes please see the <a
href="https://github.com/scanner-darkly/teletype/wiki/GRID-INTEGRATION"
target="_blank">Grid Studies</a>.

#### Improved script editing

You can now select multiple lines when editing scripts by holding
`shift`. You can move the current selection up and down with `alt-<up>`
and `alt-<down>`. You can copy/cut/paste a multiline selection as well.
To delete selected lines without copying into the clipboard use
`alt-<delete>`.

Three level undo is also now available with `ctrl-z` shortcut.

#### Support for the Orthogonal Devices ER-301 Sound Computer over i2c

You now can connect up to three ER-301s via i2c and address up to 100
virtual CV channels and 100 virtual TR channels per ER-301. (The outputs
range 1-100, 101-200, and 201-300 respectively.) To function, this
requires a slight mod to current in-market ER-301s and a specialized i2c
cable that reorders two of the pins. Find more information <a
href="http://wiki.orthogonaldevices.com/index.php/ER-301/Teletype_Integration"
target="_blank">on the Orthogonal Devices ER-301 Wiki Teletype
Integration Page</a>.

#### Support for the 16n Faderbank via i2c

The 16n Faderbank is an open-source sixteen fader controller with
support for USB MIDI, standard MIDI, and i2c communication with the
Teletype. It operates just like an IN or PARAM (or the TXi for that
matter) in that you read values from the device. You use the operator
FADER (or the alias FB) and the number of the slider you wish to poll
(1-16). Know that longer cables may require that you use a powered bus
board even if you only have one device on your Teletype’s i2c bus. (You
will know that you have a problem if your Teletype randomly hangs on
reads.)

#### Support for the SSSR Labs SM010 Matrixarchate via i2c

The SSSR Labs SM010 Matrixarchate is a 16x8 IO Sequenceable Matrix
Signal Router. Teletype integration allows you to switch programs and
control connections. For a complete list of available ops refer to the
manual. Information on how to connect the module can be found
<a href="https://www.sssrlabs.com/store/sm010/" target="_blank">in the
SM010 manual</a>.

#### Support for W/ via i2c

Support for controlling Whimsical Raps W/ module via i2c. See the
respective section for a complete list of available ops and refer to
https://www.whimsicalraps.com/pages/w-type for more details.

### New operators

`? x y z` is a ternary “if” operator, it will select between `y` and `z`
based on the condition `x`.

#### New pattern ops

`P.MIN` `PN.MIN` `P.MAX` `PN.MAX` return the position for the first
smallest/largest value in a pattern between the `START` and `END`
points.

`P.RND` / `PN.RND` return a randomly selected value in a pattern between
the `START` and `END` points.

`P.+` / `PN.+` / `P.-` / `PN.-` increment/decrement a pattern value by
the specified amount.

`P.+W` / `PN.+W` / `P.-W` / `PN.-W` same as above and wrap to the
specified range.

#### New Telex ops

`TO.CV.CALIB` allows you to lock-in an offset across power cycles to
calibrate your TELEX CV output (`TO.CV.RESET` removes the calibration).

`TO.ENV` now accepts gate values (1/0) to trigger the attack and decay.

#### New Kria ops

`KR.CV x` get the current CV value for channel `x`

`KR.MUTE x` `KR.MUTE x y` get/set mute state for channel `x`

`KR.TMUTE x` toggle mute state for channel `x`

`KR.CLK x` advance the clock for channel `x`

#### Ops for ER-301, 16n Faderbank, SM010, W/

Too many to list, please refer to their respective sections.

### New aliases

`$` for `SCRIPT`

`RND` / `RRND` `RAND` / `RRAND`

`WRP` for `WRAP`

`SCL` for `SCALE`

### New keybindings

Hold `shift` while making line selection in script editing to select
multiple lines. Use `alt-<up>` and `alt-<down>` to move selected lines
up and down. Copy/cut/paste shortcuts work with multiline selection as
well. To delete selected lines without copying into the clipboard use
`alt-<delete>`.

While editing a line you can now use `ctrl-<left>` / `ctrl-<right>` to
move by words.

`ctrl-z` provides three level undo in script editing.

Additional `Alt-H` shortcut is available to view the Help screen.

`Alt-G` in Live mode will turn on the Grid Visualizer, which has its own
shortcuts. Refer to the **Keys** section for a complete list.

The keybindings to insert a scaled knob value in the Tracker mode were
changed from `ctrl` to `ctrl-alt` and from `shift` to `ctrl-shift`.

### Bug fixes

i2c initialization delayed to account for ER-301 bootup

last screen saved to flash

knob jitter when loading/saving scenes reduced

<a href="https://github.com/monome/teletype/issues/99"
target="_blank">duplicate commands not added to history</a>

`SCALE` precision improved

`PARAM` set properly when used in the init script

`PARAM` and `IN` won’t reset to 0 after `INIT.DATA`

<a href="https://github.com/monome/teletype/issues/151"
target="_blank"><code>PN.HERE</code>, <code>P.POP</code>,
<code>PN.POP</code> will update the tracker screen</a>

<a href="https://github.com/monome/teletype/issues/149"
target="_blank"><code>P.RM</code> was 1-based, now 0-based</a>

<a href="https://github.com/monome/teletype/issues/150"
target="_blank"><code>P.RM</code> / <code>PN.RM</code> will not change
pattern length if deleting outside of length range</a>

<a href="https://llllllll.co/t/teletype-the-ji-op/10553"
target="_blank"><code>JI</code> op fixed</a>

<a href="https://github.com/monome/teletype/issues/144"
target="_blank"><code>TIME</code> and <code>LAST</code> are now 1ms
accurate</a>

<a href="https://github.com/monome/teletype/issues/143"
target="_blank"><code>RAND</code> / <code>RRAND</code> will properly
work with large range values</a>

<a href="https://github.com/monome/teletype/issues/148"
target="_blank"><code>L .. 32767</code> won’t freeze</a>

### New behavior

Previously, when pasting the clipboard while in script editing the
pasted line would replace the current line. It will now instead push the
current line down. This might result in some lines being pushed beyond
the script limits - if this happens, use `ctrl-z` to undo the change,
delete some lines and then paste again.

`I` would previously get initialized to 0 when executing a script. If
you called a script from another script’s loop this meant you had to use
a variable to pass the loop’s current `I` value to the called script.
This is not needed anymore - when a script is called from another script
its `I` value will be set to the current `I` value of the calling
script.

## Version 2.2

Teletype version 2.2 introduces Chaos and Bitwise operators, Live mode
view of variables, INIT operator, ability to calibrate CV In and Param
knob and set Min/Max scale values for both, a screensaver, Random Number
Generator, and a number of fixes and improvements.

### Major new features

#### Chaos Operators

The `CHAOS` operator provides a new source of uncertainty to the
Teletype via chaotic yet deterministic systems. This operator relies on
various chaotic maps for the creation of randomized musical events.
Chaotic maps are conducive to creating music because fractals contain a
symmetry of repetition that diverges just enough to create beautiful
visual structures that at times also apply to audio. In mathematics a
map is considered an evolution function that uses polynomials to drive
iterative procedures. The output from these functions can be assigned to
control voltages. This works because chaotic maps tend to repeat with
slight variations offering useful oscillations between uncertainty and
predictability.

#### Bitwise Operators

Bitwise operators have been added to compliment the logic functions and
offer the ability to maximize the use of variables available on the
Teletype.

Typically, when a variable is assigned a value it fully occupies that
variable space; should you want to set another you’ll have to use the
next available variable. In conditions where a state of on, off, or a
bitwise mathematical operation can provide the data required, the
inclusion of these operators give users far more choices. Each variable
normally contains 16 bits and Bitwise allows you to `BSET`, `BGET`, and
`BCLR` a value from a particular bit location among its 16 positions,
thus supplying 16 potential flags in the same variable space.

#### INIT

The new op family `INIT` features operator syntax for clearing various
states from the unforgiving INIT with no parameters that clears ALL
state data (be careful as there is no undo) to the ability to clear CV,
variable data, patterns, scenes, scripts, time, ranges, and triggers.

#### Live Mode Variable Display

This helps the user to quickly check and monitor variables across the
Teletype. Instead of single command line parameter checks the user is
now able to simply press the `~ key` (Tilde) and have a persistent
display of eight system variables.

#### Screensaver

Screen saver engages after 90 minutes of inactivity

#### New Operators

- `IN.SCALE min max` sets the min/max values of the CV Input jack
- `PARAM.SCALE min max` set the min/max scale of the Parameter Knob
- `IN.CAL.MIN` sets the zero point when calibrating the CV Input jack
- `IN.CAL.MAX` sets the max point (16383) when calibrating the CV Input
  jack
- `PARAM.CAL.MIN` sets the zero point when calibrating the Parameter Kob
- `PARAM.CAL.MAX` sets the max point (16383) when calibrating the
  Parameter Kob
- `R` generate a random number
- `R.MIN` set the low end of the random number generator
- `R.MAX` set the upper end of the random number generator

#### Fixes

- Multiply now saturates at limits (-32768 / 32767) while previous
  behavior returned 0 at overflow
- Entered values now saturate at Int16 limits which are -32768 / 32767
- Reduced flash memory consumption by not storing TEMP script
- I now carries across `DEL` commands
- Corrected functionality of `JI` (Just Intonation) op for 1V/Oct tuning
- Reduced latency of `IN` op

#### Improvements

- Profiling code (optional developer feature)
- Screen now redraws only lines that have changed

## Version 2.1

Teletype version 2.1 introduces new operators that mature the syntax and
capability of the Teletype, as well as several bug fixes and enhancement
features.

### Major new features

#### Tracker Data Entry Improvements

Data entry in the tracker screen is now *buffered*, requiring an `ENTER`
keystroke to commit changes, or `SHIFT-ENTER` to insert the value. All
other navigation keystrokes will abandon data entry. The increment /
decrement keystrokes (`]` and `[`), as well as the negate keystroke
(`-`) function immediately if not in data entry mode, but modify the
currently buffered value in edit mode (again, requiring a commit).

#### Turtle Operator

The Turtle operator allows 2-dimensional access to the patterns as
portrayed out in Tracker mode. It uses new operators with the `@`
prefix. You can `@MOVE X Y` the turtle relative to its current position,
or set its direction in degrees with `@DIR` and its speed with `@SPEED`
and then execute a `@STEP`.

To access the value that the turtle operator points to, use `@`, which
can also set the value with an argument.

The turtle can be constrained on the tracker grid by setting its fence
with `@FX1`, `@FY1`, `@FX2`, and `@FY2`, or by using the shortcut
operator `@F x1 y1 x2 y2`. When the turtle reaches the fence, its
behaviour is governed by its *fence mode*, where the turtle can simply
stop (`@BUMP`), wrap around to the other edge (`@WRAP`), or bounce off
the fence and change direction (`@BOUNCE`). Each of these can be set to
`1` to enable that mode.

Setting `@SCRIPT N` will cause script `N` to execute whenever the turtle
crosses the boundary to another cell. This is different from simply
calling `@STEP; @SCRIPT N` because the turtle is not guaranteed to
change cells on every step if it is moving slowly enough.

Finally, the turtle can be displayed on the tracker screen with
`@SHOW 1`, where it will indicate the current cell by pointing to it
from the right side with the `<` symbol.

#### New Mods: EVERY, SKIP, and OTHER, plus SYNC

These mods allow rhythmic division of control flow. EVERY X: executes
the post-command once per X at the Xth time the script is called. SKIP
X: executes it every time but the Xth. OTHER: will execute when the
previous EVERY/SKIP command did not.

Finally, SYNC X will set each EVERY and SKIP counter to X without
modifying its divisor value. Using a negative number will set it to that
number of steps before the step. Using SYNC -1 will cause each EVERY to
execute on its next call, and each SKIP will not execute.

#### Script Line “Commenting”

Individual lines in scripts can now be disabled from execution by
highlighting the line and pressing `ALT-/`. Disabled lines will appear
dim. This status will persist through save/load from flash, but will not
carry over to scenes saved to USB drive.

### New Operators

`W [condition]:` is a new mod that operates as a while loop. The `BREAK`
operator stops executing the current script `BPM [bpm]` returns the
number of milliseconds per beat in a given BPM, great for setting `M`.
`LAST [script]` returns the number of milliseconds since `script` was
last called.

### New Operator Behaviour

`SCRIPT` with no argument now returns the current script number. `I` is
now local to its corresponding `L` statement. `IF/ELSE` is now local to
its script.

### New keybindings

`CTRL-1` through `CTRL-8` toggle the mute status for scripts 1 to 8
respectively. `CTRL-9` toggles the METRO script. `SHIFT-ENTER` now
inserts a line in Scene Write mode.

### Bug fixes

Temporal recursion now possible by fixing delay allocation issue, e.g.:
DEL 250: SCRIPT SCRIPT `KILL` now clears `TR` outputs and stops METRO.
`SCENE` will no longer execute from the INIT script on initial scene
load. `AVG` and `Q.AVG` now round up from offsets of 0.5 and greater.

### Breaking Changes

As `I` is now local to `L` loops, it is no longer usable across scripts
or as a general-purpose variable. As `IF/ELSE` is now local to a script,
scenes that relied on IF in one script and ELSE in another will be
functionally broken.

## Version 2.0

Teletype version 2.0 represents a large rewrite of the Teletype code
base. There are many new language additions, some small breaking changes
and a lot of under the hood enhancements.

### Major new features

#### Sub commands

Several commands on one line, separated by semicolons.

e.g. `CV 1 N 60; TR.PULSE 1`

See the section on “Sub commands” for more information.

#### Aliases

For example, use `TR.P 1` instead of `TR.PULSE 1`, and use `+ 1 1`,
instead of `ADD 1 1`.

See the section on “Aliases” for more information.

#### `PN` versions of every `P` `OP`

There are now `PN` versions of every `P` `OP`. For example, instead of:

    P.I 0
    P.START 0
    P.I 1
    P.START 10

You can use:

    PN.START 0 0
    PN.START 1 10

#### TELEXi and TELEXo `OP`s

Lots of `OP`s have been added for interacting with the wonderful TELEXi
input expander and TELEXo output expander. See their respective sections
in the documentation for more information.

#### New keybindings

The function keys can now directly trigger a script.

The `<tab>` key is now used to cycle between live, edit and pattern
modes, and there are now easy access keys to directly jump to a mode.

Many new text editing keyboard shortcuts have been added.

See the “Modes” documentation for a listing of all the keybindings.

#### USB memory stick support

You can now save you scenes to USB memory stick at any time, and not
just at boot up. Just insert a USB memory stick to start the save and
load process. Your edit scene should not be effected.

It should also be significantly more reliable with a wider ranger of
memory sticks.

**WARNING:** Please backup the contents of your USB stick before
inserting it. Particularly with a freshly flashed Teletype as you will
end up overwriting all the saved scenes with blank ones.

### Other additions

- Limited script recursion now allowed (max recursion depth is 8)
  including self recursion.
- Metro scripts limited to 25ms, but new `M!` op to set it as low as 2ms
  (at your own risk), see “Metronome” `OP` section for more.

### Breaking changes

- **Removed the need for the `II` `OP`.**

  For example, `II MP.PRESET 1` will become just `MP.PRESET 1`.

- **Merge `MUTE` and `UNMUTE` `OP`s to `MUTE x` / `MUTE x y`.**

  See the documentation for `MUTE` for more information.

- **Remove unused Meadowphysics `OP`s.**

  Removed: `MP.SYNC`, `MP.MUTE`, `MP.UNMUTE`, `MP.FREEZE`,
  `MP.UNFREEZE`.

- **Rename Ansible Meadowphysics `OP`s to start with `ME`.**

  This was done to avoid conflicts with the Meadowphysics `OP`s.

**WARNING**: If you restore your scripts from a USB memory stick, please
manually fix any changes first. Alternatively, incorrect commands (due
to the above changes) will be skipped when imported, please re-add them.

### Known issues

#### Visual glitches

The cause of these is well understood, and they are essentially
harmless. Changing modes with the `<tab>` key will force the screen to
redraw. A fix is coming in version 2.1.

# Quickstart

## Panel

<figure>
<img
src="data:image/png;base64,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" />
<figcaption>Panel Overlay</figcaption>
</figure>

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
<tr class="odd">
<td><strong><code>DEVICE.FLIP</code></strong></td>
<td>-</td>
<td>-</td>
<td>Flip the screen, the inputs and the outputs. This op is useful if
you want to mount your Teletype upside down.<br />
The new state will be saved to flash.<br />
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
