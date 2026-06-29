# Tuesday Track Manual

## Introduction

The Tuesday track is a **generative algorithm sequencer**. It stores no fixed step
data — instead it runs one of 16 named algorithms (a port of the "Tuesday"
generator) that produce notes, gates, and timing on every tick from a deterministic
seed. Three knobs — **Flow**, **Ornament**, **Power** — drive that seed and shape the
character; everything else (loop length, rotation, skew, glide, trills, masking,
scale) reshapes the algorithm's output without touching what it generates.

The same Flow + Ornament always produce the same sequence until you reseed. So a
Tuesday track is reproducible: dial in a phrase you like, and it comes back. Reseed
when you want a fresh draw of the same algorithm without changing your parameter
settings.

This manual is grounded in the firmware as built. Defaults, ranges, enum members,
key bindings, and step-grid maps are taken from the model, engine, and UI code.

## Prerequisites

Before working with the Tuesday track you should have:

- Familiarity with the xformer track/sequence/edit page model (Page+S2 / Page+S1 / Page+S0).
- Understanding of CV/gate concepts in modular synthesis.
- A basic grasp of scales and quantization, since Tuesday's output is scale-quantized.

---

# Part 1: Overview

## What the Tuesday track is

A Tuesday track has **no per-step note data** in the usual sense. Instead it:

1. **Seeds** two random generators from Flow and Ornament. Generation is deterministic from those seeds.
2. **Generates** a note, octave, gate, and timing every step by running the selected algorithm against its internal state.
3. **Shapes** that output through density (Power), loop/rotate/skew, glide, trills, masking, and scale quantization on the way to CV/gate out.

It is an algorithmic note source, not a step or sample sequencer. The sequence you
hear is computed live, not stored.

## Where state lives

- **Sequence params** (algorithm, Flow/Ornament/Power, loop, rotate, skew, glide, trills, gate shaping, start, octave/transpose, divisor, mask) live on the **sequence** — one set per pattern.
- The **scale group** (scale / root note / scale rotate) lives on the **sequence**, not the track.
- The **track** holds the play mode (Aligned / Free) and the per-pattern sequence array.

## Seed, reset, reseed

- **Flow** (1-16) seeds the main RNG; **Ornament** (1-16) seeds an auxiliary RNG (with a salt so Flow == Ornament still differs).
- **reset** wipes all engine state and re-seeds both RNGs from the current Flow/Ornament. It runs on track init, loop restart, and the Reset jam button.
- **reseed** (Shift+F5, or the RESEED context action) advances both RNGs to a fresh draw and re-initializes the algorithm, keeping Flow/Ornament unchanged — a new random variation of the same setup.

---

# Part 2: Algorithms

The **Algorithm** param selects one of 16 generators (id 0-15). Each has its own note
selection, rhythm, and character. Flow and Ornament seed the draw and bias structural
decisions; Power gates density.

## The 16 algorithms

| # | Name | What it generates |
|---|---|---|
| 0 | Test | Simple stepped reference sequence — octave sweeps or a scale walk. Constant velocity. |
| 1 | TriTrance | Three overlapping polyrhythmic layers on a 3-beat cycle: tight, swung, then accented with grace-note trills. |
| 2 | Stomper | Alternating high/low octave "stomps" with slides and countdown rest periods. |
| 3 | Markov | First-order Markov chain over 8 scale degrees; next note drawn from a transition matrix. Accent always on. |
| 4 | Chip1 | Beat-synced chord arpeggio; chord stabs trigger a root/third/fifth trill. |
| 5 | Chip2 | Ascending/descending arpeggio with dynamic chord length (3-7 notes) and rhythmic deadtime gaps. |
| 6 | Wobble | Twin phasor (slow + fast) mapped to scale degrees; Ornament controls phase-2 probability, accents get high slide chance. |
| 7 | ScaleWlk | Stateful 7-note scale walker; Flow sets direction (≤7 walks down, >7 walks up). StepTrill fires rapid bursts. |
| 8 | Window | Twin phasor: slow anchor notes/accents over fast texture, with a Markov walk voice and periodic auto-mutation. |
| 9 | Minimal | Burst/silence alternation; burst sections density-gate staccato notes. Conservative glide and trill. |
| 10 | Ganz | Triple phasor (1×/5×/7×) on a 5-tuplet grid with phrase-skip muting and four note-selection modes. |
| 11 | Blake | Stable 4-note motif over an 8-bar breath LFO; real/ghost/whisper articulation and occasional sub-bass drops. |
| 12 | Aphex | Three independent polyrhythms (4/3/5-beat) with a modifier track and a bass-override track; collision stress. |
| 13 | Autech | 8-note pattern that mutates by rotation/mirror/inversion/shift on timer expiry; polyrhythm on off-grid beats. |
| 14 | StepWave | Scale-degree walk with polyrhythm; Flow biases direction, Ornament selects rapid vs spread timing. |
| 15 | Turing | Turing shift register with live drift; Power sets flip probability, Ornament sets range. |

> The Edit page footer truncates algorithm names to 7 characters. The List View shows
> the full name.

## How Flow / Ornament / Power read

- **Flow** (0-16) seeds the main RNG and drives big structural choices: mode select, phrase, walk direction, burst/breath lengths, pattern init.
- **Ornament** (0-16) seeds the auxiliary RNG and drives detail: pitch variation, velocity, micro-timing, phase-selection probability, register range.
- **Power** (0-16) is the **density / cooldown** control. The cooldown between notes is `17 - Power`, so Power 0 means no notes (max cooldown), Power 16 means notes nearly every step. Skew bends Power across the loop (see Part 4).

---

# Part 3: Output Shaping

## Gate length and offset

The algorithm emits a base gate ratio (nominally 75%, up to 200%). Two knobs scale it:

- **Gate Length** (`gateLength`, 0-100%, default 50%) scales the algorithm's gate ratio into actual gate-on ticks. Accented notes extend the gate by 50%.
- **Gate Offset** (`gateOffset`, 0-100%, default 0%) scales the algorithm's intended onset offset. At 50% it applies the algorithm's offset 1×; at 100% it doubles it, pushing the gate later within the step.

## Glide, Trill, StepTrill

Three independent expressive effects:

| Param | List name | Range / default | Effect |
|---|---|---|---|
| Glide | Glide | 0-100%, default 50% | Slide probability. When the algorithm flags a slide and Glide > 0, CV interpolates to the target over a slide window (longer with higher Glide). |
| Trill | Old Trill | 0-100%, default 50% | Re-trigger probability. Combined with the algorithm's articulation indicator; when it fires the gate ratchets (fires twice). List View only. |
| StepTrill | Trill (Edit page) | 0-100%, default 0% | Intra-step subdivision. When the algorithm requests a trill count >1 and StepTrill > 0, the step subdivides into rapid micro-gates with independent pitches. |

> **Two trills, two layers.** The Edit page "TRILL" knob is **StepTrill** (the new,
> intra-step subdivision). The List View exposes both: "Trill" (StepTrill) and "Old
> Trill" (the re-trigger ratchet). They are separate parameters.

## CV update mode

`CvUpdateMode` (List View, **CV Mode**) sets when CV moves:

- **Free** (default) — CV updates every step the algorithm produces a note, even when density gates the gate off. Continuous modulation.
- **Gated** — CV only updates when a gate actually fires. Traditional note-and-rest behavior (the original Tuesday behavior).

## Scale quantization

Output is always quantized. The generated scale degree + octave runs through the
selected scale, then Transpose (in scale degrees), Root Note offset, and the Octave
offset are applied to reach the final CV.

- **Scale** (Default = project scale, Semitones = chromatic, or a named scale).
- **Root Note** (Default = project root, or C..B).
- **Scale Rotate** (Default = project rotate, or 0-31).
- **Octave** (-10..+10) and **Transpose** (-11..+11).

---

# Part 4: Loop, Rotate, Skew, Start

## Loop length

`LoopLength` indexes a fixed table of lengths. Index 0 is **Inf** (infinite/evolving,
the default). Indices 1-29 map to: 1-16, then 19, 21, 24, 32, 35, 42, 48, 56, 64, 95,
96, 127, 128 steps.

- **Inf** — the algorithm runs free, never resetting its RNG. Patterns evolve.
- **Finite** — at the loop boundary the engine re-seeds from the initial seed, so the same N-step phrase repeats exactly.

## Rotate

`Rotate` (default 0) cyclically shifts the playback position within a finite loop. Its
range is clamped to ±(loop length − 1), so it only applies when Loop is finite — the
Edit page shows **N/A** for infinite loops, and rotation is forced to 0.

## Skew

`Skew` (-8..+8, default 0) bends **Power** across the loop:

- Negative — lower density at the start, higher at the end (ramp up).
- Positive — higher density at the start, lower at the end (ramp down).

## Start

`Start` (0-16, default 0) delays note generation by that many steps from track start.
The mask system can extend this further with prime-number delays (Part 5).

---

# Part 5: The Mask System

Masking filters the algorithm's output on and off in time, carving polyrhythms and
rests out of the generated stream. While a tick is masked, the gate stays off.

## Mask Parameter

`MaskParameter` (0-15):

- **0 = ALL** (default) — no masking, every tick allowed.
- **1-14** — prime-number masks. The values, in order, are 2, 3, 5, 11, 19, 31, 43, 61, 89, 131, 197, 277, 409, 599.
- **15 = NONE** — mask everything (silence).

In FREE mode the mask alternates allow/mask in blocks of `maskValue` ticks. In the
grid-synced modes the masked block is `maskValue` ticks and the allowed block is
derived from a quarter-note grid.

## Time Mode

`TimeMode` (List View) sets how the mask aligns to the clock:

| Mode | Behavior |
|---|---|
| FREE (default) | Mask toggles purely by tick count, free-running. |
| QRT | Allowed block aligns to a quarter-note grid. |
| 1.5Q | Allowed block aligns to a 1.5 quarter-note grid. |
| 3QRT | Allowed block aligns to a 3 quarter-note grid. |

## Mask Progression

`MaskProgression` (List View) auto-advances the mask index each cycle:

| Mode | Step |
|---|---|
| NO PROG (default) | Static mask, no advance. |
| PROG+1 | Advance the prime index by 1 per cycle. |
| PROG+5 | Advance by 5. |
| PROG+7 | Advance by 7. |

---

# Part 6: Timing

Per-sequence timing parameters:

| Param | Range | Default | Notes |
|---|---|---|---|
| Divisor | 1-768 | 12 (1/16) | Ticks per step at the hardware clock. Edited by known musical divisors. |
| Clock Mult | 50-150% | 100% (1.00x) | Scales tempo against the divisor; higher = faster steps. |
| Reset Measure | 0-128 bars | 0 (off) | Bar-based reset interval. 0 = off. |
| Play Mode | Aligned / Free | Aligned | Track-level. Aligned steps on the tick grid; Free derives the step from wallclock position. |

The effective step length combines Divisor and Clock Mult: a higher multiplier
shortens the divisor and speeds up stepping.

---

# Part 7: The Edit Page

The Edit page (Page+S0) shows **3 parameter pages** of 4 slots each, plus a status box
and a jam step grid. **F1-F4** select a slot for the encoder; **F5 (NEXT)** advances to
the next parameter page; **Shift+F5** reseeds.

## The three parameter pages

| Page | F1 | F2 | F3 | F4 |
|---|---|---|---|---|
| 1 | ALGO (Algorithm) | FLOW (Flow) | ORN (Ornament) | POWER (Power) |
| 2 | LOOP (Loop Length) | ROT (Rotate) | GLIDE (Glide) | SKEW (Skew) |
| 3 | GATE (Gate Length) | GOFS (Gate Offset) | TRILL (StepTrill) | START (Start) |

The footer shows the four short names plus **NEXT**; a `[page/3]` indicator sits to the
right and `[algo#]` above F1. The status box (top right) shows the current note name +
gate indicator, the CV voltage, and the step/loop position.

## Encoder

The encoder edits the selected slot's parameter. **Shift** accelerates the percentage
params (Glide, Trill, StepTrill, Gate Length, Gate Offset, Clock Mult) by ×10.

## The jam step grid (S1-S16)

The step keypad performs hands-on edits without leaving the page. Top row increases /
speeds up, bottom row decreases / slows down:

| Col | S1-8 (top) | S9-16 (bottom) |
|---|---|---|
| 1 | Octave + | Octave − |
| 2 | Transpose + | Transpose − |
| 3 | Root Note + | Root Note − |
| 4 | Divisor up (straight, faster) | Divisor down (straight, slower) |
| 5 | Divisor up (triplet, faster) | Divisor down (triplet, slower) |
| 6 | Divisor ÷2 (faster) | Divisor ×2 (slower) |
| 7 | Mask + | Mask − |
| 8 | Loop + | Loop − |

**Shift modifiers:**

- **Shift+S8** — Run momentary: while held, the track stops; release restores its prior run state.
- **Shift+S16** — Reset the sequence (shows "RESET").

**LEDs:** top row lights green when the value is above its default (octave/transpose/root
up, divisor faster, mask up); bottom row lights red when below.

## Context menu

Long-press the menu button:

- **INIT** — clear the sequence to defaults.
- **RESEED** — fresh seed draw, same Flow/Ornament.
- **RAND** — randomize all sequence parameters.
- **COPY** — copy sequence params to clipboard slot 1.
- **PASTE** — paste from clipboard slot 1.

## Quick-edit (Page + S9-S16)

Holding **Page** plus a bottom-row step performs clipboard/randomize actions:

| Step | Action |
|---|---|
| S9 | Copy to slot 1 |
| S10 | Copy to slot 2 |
| S11 | Copy to slot 3 |
| S12 | Paste from slot 1 |
| S13 | Paste from slot 2 |
| S14 | Paste from slot 3 |
| S15 | Randomize |
| S16 | (inert) |

**LEDs:** copy and randomize steps light; paste steps light only when their clipboard
slot holds valid data.

> The clipboard carries algorithm, Flow, Ornament, Power, loop, rotate, glide, skew,
> gate length, gate offset, StepTrill, start, octave, transpose, root note, divisor, and
> mask parameter. Other params (scale, time mode, mask progression, CV mode, etc.) are
> not copied.

---

# Part 8: The List View & Routing

## Sequence list (Page+S1)

The List View hosts every sequence parameter in order:

Algorithm, Flow, Ornament, Power, Loop Length, Rotate, Glide, Skew, Gate Length, CV
Mode, Old Trill, Start, Octave, Transpose, Divisor, Clock Mult, Reset Measure, Scale,
Root Note, Scale Rotate, Mask Param, Time Mode, Mask Prog.

The only context action here is **INIT** (clear the sequence).

## Defaults and ranges

| Param | Range | Default |
|---|---|---|
| Algorithm | 0-15 | 0 (Test) |
| Flow | 0-16 | 0 |
| Ornament | 0-16 | 0 |
| Power | 0-16 | 0 |
| Loop Length | Inf, 1-128 (29 indices) | Inf |
| Rotate | ±(loop−1) | 0 |
| Glide | 0-100% | 50% |
| Skew | -8..+8 | 0 |
| Gate Length | 0-100% | 50% |
| CV Mode | Free / Gated | Free |
| Old Trill | 0-100% | 50% |
| StepTrill (Edit "TRILL") | 0-100% | 0% |
| Start | 0-16 | 0 |
| Octave | -10..+10 | 0 |
| Transpose | -11..+11 | 0 |
| Divisor | 1-768 | 12 (1/16) |
| Clock Mult | 50-150% | 100% (1.00x) |
| Reset Measure | 0-128 bars | 0 (off) |
| Gate Offset | 0-100% | 0% |
| Scale | Default / Semitones / named | Default (project) |
| Root Note | Default / C..B | Default (project) |
| Scale Rotate | Default / 0-31 | Default (project) |
| Mask Param | ALL, 1-14 primes, NONE | ALL |
| Time Mode | FREE / QRT / 1.5Q / 3QRT | FREE |
| Mask Prog | NO PROG / +1 / +5 / +7 | NO PROG |

## What is routable

These Tuesday params are routing targets (all per-sequence):

- `Algorithm` (0-15)
- `Flow` (0-16)
- `Ornament` (0-16)
- `Power` (0-16)
- `Glide` (0-100%)
- `Trill` (Old Trill, 0-100%)
- `StepTrill` (0-100%)
- `GateLength` (0-100%)
- `GateOffset` (0-100%)
- `Rotate`
- `Octave`
- `Transpose`
- `Divisor`
- `ClockMult`
- `Scale`, `RootNote` (via the shared scale targets)

**Not routable:** Start, Skew, Loop Length, CV Mode, Reset Measure, Scale Rotate, Mask
Param, Time Mode, Mask Prog.
