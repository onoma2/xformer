# Discrete Track Manual

## Introduction

The Discrete track is a **threshold map**. It does not step through a sequence
of notes. Instead it watches one moving input — an internal ramp or an external
CV — and fires one of up to 32 **stages** whenever that input crosses the stage's
threshold in the stage's direction. The stage that fires emits a note (CV) and a
gate. Sweep the input and the map plays back as a melody; drive it from outside
and the same map becomes a quantizer / CV shaper.

It is a **stateless ramp** engine. Nothing is "remembered" between ticks: each
tick the engine recomputes the ramp phase from the clock position, compares the
current input against the previous input, and lets any threshold that was
crossed claim the active stage. Pause, reset, or change pattern and the whole
picture is rebuilt from the clock.

This manual is grounded in the firmware as built. Defaults, ranges, enum members,
key bindings, and step-grid maps are taken from the model, engine, and UI code.

## Prerequisites

Before working with the Discrete track you should have:

- Familiarity with the xformer track/sequence/edit page model (Page+S2 / Page+S1 / Page+S0).
- Understanding of CV/gate concepts in modular synthesis, in particular 1V/oct pitch and ±5V CV.
- For External mode, a routing channel set up to feed the track's input.

---

# Part 1: Overview

## What the Discrete track is

The track owns **32 stages**. Each stage holds three things:

- a **threshold** (−100..+100), the position in the voltage window where it watches,
- a **direction** (Rise / Fall / Off / Both), the edge it watches for,
- a **note index** (−63..+63), the scale degree it emits when it fires.

A single moving **input** scans the window. When the input crosses a stage's
threshold in that stage's direction, that stage becomes the **active stage**: its
note converts to output CV and a gate fires. The active stage stays active until
another stage's threshold is crossed (an Off active stage releases to silence).

The input comes from one of three **clock sources**:

- **Internal Saw** — a −5V→+5V sawtooth ramp, period set by the divisor.
- **Internal Tri** — a −5V→+5V triangle (up then down), same period.
- **External** — a routed CV input (the *DMap Input* route).

## The two pages of state

- **Track-wide params** (Play Mode, CV Update) live on the **track** — shared across all patterns.
- **Per-pattern params** (clock source, sync, divisor, gate length, loop, range, scale group, slew, pluck, octave/transpose/offset, and all 32 stages) live on the **sequence** — one set per w-pattern.
- The **scale group** (scale / root / scale-rotate) lives on the **sequence**, not the track. Root note clamps to 0..11 (C..B).

---

# Part 2: Input & Stages

## Clock source and the ramp

The internal ramp always spans the **full ±5V**, regardless of the Above/Below
range — the range only moves where the thresholds sit, not where the ramp goes.

- **Internal Saw** rises linearly from −5V to +5V across one period, then snaps back.
- **Internal Tri** rises −5V→+5V over the first half of the period and falls +5V→−5V over the second half.

Period in ticks is `divisor ÷ (clock multiplier as a fraction)`: a higher clock
multiplier shortens the period (faster sweeps), a higher divisor lengthens it.

**External** ignores the internal ramp entirely and reads the *DMap Input* route
value (clamped −5V..+5V) as the live input each tick.

## Loop vs Once

- **Loop** (default) — the ramp cycles forever.
- **Once** — the ramp sweeps a single period, then **holds its final value** (+5V, phase 1.0) and stops until a reset or a Loop↔Once toggle restarts it.

For External + Once the engine tracks an external sweep: it arms when the input
first enters the range window (with a 5% tolerance), then declares the sweep
**done** once the input has covered ≥90% of the range span. While done, the
active stage is frozen — no further re-scanning until reset/restart/pattern
change. Direction-agnostic, so an LFO or envelope that doesn't hit the exact
endpoints still completes.

## Stage direction

Each stage watches one edge of its threshold:

| Direction | Fires when | OLED / LED |
|---|---|---|
| Rise | input crosses from below to at/above the threshold | `^` / green |
| Fall | input crosses from above to at/below the threshold | `v` / red |
| Both | either edge crosses the threshold | `x` / yellow |
| Off | never; stage disabled | `-` / unlit |

Stages are scanned in index order and the **first** crossing found wins. A stage
stays active until some other stage is crossed. The init/default direction for
every stage is **Off**.

## Threshold modes: Position vs Length

The Above (`rangeHigh`) and Below (`rangeLow`) params define the voltage window
the thresholds live in. The threshold mode decides how each stage's −100..+100
value maps into that window:

| Mode | Default | Mapping |
|---|---|---|
| Position | **yes** | Each stage's −100..+100 maps **linearly** to Below..Above. 0 lands at the midpoint; each threshold is an absolute trigger voltage. |
| Length | — | Each stage's value is treated as a **proportional length**. The values become cumulative interval ends across the window, so the stages partition the span by weight. (All stages at −100 → even distribution.) |

Changing the range or the mode marks the threshold cache dirty and recomputes on
the next tick.

## Note index → CV

A stage's note index (−63..+63) is a scale degree. Output volts come from the
sequence's selected scale: `noteToVolts(noteIndex + octave·notesPerOctave +
transpose)`. On a chromatic scale the root note adds `root × (1/12)` volts. The
**Offset** param adds a flat CV bias (Offset is in centivolts, ×0.01 = volts) on
top of the resolved note.

The **CV Update mode** (track-scope) decides when pitch params are read:

- **Gate** (default) — octave / transpose / root are **sampled at the gate trigger** (sample-and-hold). Changing them mid-note does not retune the held note until the next gate. When muted in Gate mode the CV output drops to 0V.
- **Always** — pitch params are read live every update; CV updates continuously regardless of stage changes.

---

# Part 3: Gate, Slew & Pluck

## Gate length

When a new stage becomes active, a gate fires for a length derived from the step:

- **Gate Len 0% = "T"** — a fixed short pulse (a few ticks), independent of step length.
- **Gate Len 1..100%** — that percentage of the step's tick length.

## Slew

**Slew** (0 = Off, 1..100%) smooths CV between stages by limiting the pitch rate.
Internally the max rate is 48 semitones/sec at Slew=1 (fast) scaling down to a
slow glide near Slew=100. With Slew off the CV jumps instantly to each new note.
Slew enabled also flags the outgoing MIDI note as a slide.

## Pluck

**Pluck** (−100..+100, 0 = Off) adds a pitch transient at note onset. The note
starts bent away from its target and decays linearly back to pitch:

- magnitude up to ~200 cents (2 semitones), and onset glide time 10..450ms, both scaled by a square curve of |Pluck|,
- **sign sets direction**: Pluck > 0 bends *up* into the note, Pluck < 0 bends *down*,
- higher |Pluck| also adds more random jitter (10%..50%) to depth and time, so repeated notes vary.

Pluck is per-sequence and is **not** routable.

---

# Part 4: Playback Timing

Per-sequence timing params:

| Param | Default | Range / members |
|---|---|---|
| Clock | Internal Saw | Internal Saw / Internal Tri / External |
| Divisor | 192 | 1..768 |
| Clock Mult | 1.00x | 0.50x..1.50x (50..150%) |
| Gate Len | T (0%) | 0..100% |
| Loop | Loop | Loop / Once |
| Reset Measure | 8 bars | off / 1..128 bars |
| Sync | Off | Off / Reset (External retired → behaves as Off) |

**Sync = Reset** realigns the ramp to a bar boundary every *Reset Measure* bars
(a reset re-zeros the ramp to −5V/phase 0 and clears stage/CV/gate/pluck state).
The legacy *External* sync member is inert — re-anchor from outside via a Reset
route instead.

**Play Mode** (track-scope) is Aligned (ramp phase locks to the bar grid) or Free
(ramp runs from its own free clock position).

---

# Part 5: The Sequence Page

Page+S0 opens the Discrete edit page (header **DMAP**). It draws a threshold bar
across the top, a live input cursor, and a three-row info block for the eight
stages of the current section. There are four sections of eight stages each;
LEFT / RIGHT step through them.

## The footer (F1-F5), normal mode

| Key | Shows | Press |
|---|---|---|
| F1 | clock: `SAW` / `TRI` / `EXT` | cycle clock source |
| F2 | range-macro name (e.g. `-5/+5`) | advance to next range macro |
| F3 | `POS` / `LEN` | toggle Position / Length threshold mode |
| F4 | `LOOP` / `ONCE` | toggle loop mode |
| F5 | sync short label (`OFF` / `RM`) | cycle sync mode |

The F2 **range macros** snap Above/Below to a preset window:

| Macro | Above / Below |
|---|---|
| Full | −5 / +5 |
| Inv | +5 / −5 |
| Pos | 0 / +5 |
| Neg | −5 / 0 |
| Top | +4 / +5 |
| Btm | −5 / −4 |
| Ass | −1 / +4 |
| BAss | +3 / −2 |

## The step keypad (S1-S16)

The 16 step buttons are two rows of 8. They act on the eight stages of the
current section:

- **Bottom row (S1-S8) = stage SELECT.** Plain press selects that stage exclusively; holding one and pressing another multi-selects. Holding a bottom key also drives the engine **monitor** (auditions that stage's note while the sequencer is stopped). Shift + bottom = toggle that stage in/out of the selection without changing edit mode.
- **Top row (S9-S16) = direction EDIT.** Plain press selects that stage and cycles its direction (Off→Rise→Fall→Both). Shift + top = select every stage sharing the pressed stage's direction.

**Double-click a bottom-row step** (plain) auto-places its threshold at the
**midpoint between its nearest active neighbours** — range ends (−100 / +100)
substitute when a neighbour is missing (message `THR BETWEEN`). **Shift +
double-click** a bottom-row step selects all stages with the same value (note
value in note-edit mode, otherwise threshold).

## The encoder

Pressing the encoder toggles the edit mode between **Threshold** and **Note**.
The encoder then edits the selected stage(s):

- **Threshold** — ±1 fine, **±10 with Shift** (clamped −99..99).
- **Note** — ±1; **Shift jumps an octave** on chromatic scales only.
- **All 32 selected** (full mask) switches to a rotate of all values: thresholds rotate (Shift fine / no-Shift ×8 coarse), note indices rotate one step.

## Stage edit LEDs

- Top row (direction): Rise green, Fall red, Both yellow, Off unlit.
- Bottom row (select/active): selected yellow, active-but-unselected green, else unlit.

The OLED threshold bar marks each non-Off stage as an upward tick (tallest =
active, mid = selected) and draws the live input as a bright cursor. The info
block shows direction char / threshold / note name per stage, the active stage
brightest.

---

# Part 6: Generators, Voicings & Macros

## The context menu

The context-menu key opens: **INIT**, **COPY**, **PASTE**, **GEN**.

- **INIT** enters a target submenu in the footer: `ALL` / `THR` / `NOTE` / (blank) / `X`. ALL clears the whole sequence (re-seeds the fret-pattern thresholds, all stages Off, notes 0); THR re-seeds thresholds only; NOTE zeroes notes.
- **COPY** / **PASTE** copy and paste the whole sequence (paste only enabled when the clipboard holds a Discrete sequence).
- **GEN** enters the generator (below).

## GEN (generator)

GEN runs in two footer steps. First choose a **kind**: `RAND` / `LIN` / `LOG` /
`EXP` / `X`. Then choose a **target**:

- RAND target row: `ALL` / `THR` / `NOTE` / `TOG` / `X` (TOG randomizes directions).
- LIN/LOG/EXP target row: `ALL` / `THR` / `NOTE5` / `NOTE2` / `X` — NOTE5 spreads notes wide (−63..+64), NOTE2 narrow (0..+32). Threshold shaping is linear / √ / ^1.3 across −99..99.

## Page+Step quick edits

Holding **Page** + a step button:

| Combo | Action |
|---|---|
| Page+S9 | Range High (Above) |
| Page+S10 | Range Low (Below) |
| Page+S11 | Divisor |
| Page+S12 | Piano voicing (held; encoder scrolls bank, release applies) |
| Page+S13 | Guitar voicing (held; encoder scrolls bank, release applies) |

Range High / Range Low / Divisor open the standard quick-edit overlay. The
voicing quick-edits apply a chord shape across the active stages (limited to the
selection if more than one stage is selected); the selected stage's note is the
root, `C0` resets from the track/project root, `ROOT` is a safe no-op.

- Piano banks: ROOT, C0, MAJ13, MAJ6/9, MIN13, MIN6/9, MINMAJ9, DOM13, M9B5, DIM7, AUG9, AUGMAJ9, SUS2(9), SUS4(11).
- Guitar banks: ROOT, C0, MAJ, MIN, 7, MAJ7, MIN7, 6, MIN6, 9, 13, SUS2, SUS4, ADD9, AUG, M7B5, DIM7.

## Page+Step macros

Four step combos open threshold/note macro menus (yellow LEDs):

- **Page+S5 — Distribution:** `I-8`, `I-5(2)`, `I-7(3)`, `I-9(4)`, `I-FRET`. Even-distribute thresholds across stage groups; I-FRET round-robin interleaves across the four sections.
- **Page+S6 — Cluster:** `M-CLUSTER`, `M-AR4`, `M-SWELL`, `M-ISWELL`, `M-STRUM`. Operate on active stages (selection-limited if more than one selected).
- **Page+S7 — Distribute Active:** `E-ACT`, `E-RISE`, `E-FALL`, `E-BOTH`, `NORM`. Even-spread thresholds filtered by direction; NORM expands the current spread to fill −100..+100.
- **Page+S15 — Transform:** `FLIP`, `T-REV`, `T-INV`, `N-MIRR`, `N-REV`. FLIP cycles all directions; T-* transform thresholds; N-* transform note indices.

---

# Part 7: The Scanner

The **DMap Scan** route lets an external CV reshape the map live. Route a CV
source to *DMap Scan* (value 0..34):

- `0` = bottom dead zone, `1..32` = stages 0..31, `33`/`34` = top dead zone.
- Each time the scanned value **enters a new stage's segment**, that stage's direction is cycled (Off→Rise→Fall→Both).

Sweep a fader or slow LFO across the scan input to "draw" directions across the
map in real time. The dead zones at the extremes toggle nothing.

---

# Part 8: Setup & Routing

## Track-scope list (Page+S2)

Track-wide params, hosted by the generic Track page:

- Play Mode (Aligned / Free)
- CV Update (Gate / Always)

## Sequence-scope list (Page+S1)

Per-pattern params, in order:

Clock, Sync, Divisor, Clock Mult, Gate Len, Loop, Reset Measure, Threshold (mode),
Above, Below, Scale, Root, Scale Rotate, Slew, Pluck, Octave, Transpose, Offset.

| Param | Default | Range |
|---|---|---|
| Divisor | 192 | 1..768 |
| Clock Mult | 1.00x | 50..150% |
| Gate Len | T (0%) | 0..100% |
| Reset Measure | 8 bars | off / 1..128 |
| Above (rangeHigh) | +5.0V | −5.0..+5.0V |
| Below (rangeLow) | −5.0V | −5.0..+5.0V |
| Scale | Project | Project / track scales |
| Root | C (0) | 0..11 (C..B) |
| Scale Rotate | Default | Default / 0..31 |
| Slew | Off | 0..100% |
| Pluck | Off | −100..+100 |
| Octave | 0 | −10..+10 |
| Transpose | 0 | −60..+60 |
| Offset | +0.00V | −5.00..+5.00V (−500..+500 cV) |

## What is routable

Routing targets exposed for a Discrete track:

- `DMap Input` — the External-mode input CV (−5..+5V).
- `DMap Scan` — the scanner (0..34, cycles stage directions).
- `DMap Above` (rangeHigh) and `DMap Below` (rangeLow) (−5..+5V).
- Shared sequence/track params: Divisor, Clock Mult, Scale, Root Note, Slew (Slide Time), Octave, Transpose, Offset.

`DMap Sync` is retired (folds into the universal Align/Reset). **Pluck and Gate
Length are not routable.**

## Initialization (the fret pattern)

INIT (ALL) re-seeds every stage with an **interleaved "fret-pattern" threshold
distribution** — round-robin across the four sections, ~6.5 units apart overall —
with all directions Off and all notes 0. Enabling a stage (changing its
direction) lights up a pre-spaced threshold without re-placing it.
