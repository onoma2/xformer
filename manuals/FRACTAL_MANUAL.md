# Fractal Track Manual

## Introduction

The Fractal track is a **section-sampled CV/gate looper**. It does not generate notes
of its own — it watches one or two other tracks (or routing channels), summarizes
what they emit over each loop section into a compact buffer (the **trunk**), and
replays that captured loop. On top of the trunk you stack two composable layers:
**branches** (chained transforms of the trunk that play after it) and **ornaments**
(scale-snapped flourish notes injected inside a section). Capture, transform, and
flourish are independent — you record once, then reshape forever without touching
the source.

This manual is grounded in the firmware as built. Defaults, ranges, enum members,
key bindings, and step-grid maps are taken from the model, engine, and UI code.

## Prerequisites

Before working with the Fractal track you should have:

- Familiarity with the xformer track/sequence/edit page model (Page+S2 / Page+S1 / Page+S0).
- Understanding of CV/gate concepts in modular synthesis.
- At least one other track (Note, Curve, Stochastic, …) running that the Fractal track can listen to, or a routing channel worth sampling.

---

# Part 1: Overview

## What the Fractal track is

A Fractal track has **no sequence data of its own** in the usual sense. Instead it:

1. **Listens** to source A and/or source B — another track's emitted gate+CV, or a single routing channel.
2. **Observes over each section**: every tick while armed it accumulates how long the source gate was high and where in the section it first fired, then commits one summarized **cell** into the trunk buffer.
3. **Replays** the captured loop through a loop window, optionally re-ordered, transposed, and ornamented.

The three layers stack like this:

- **Trunk** — the literal captured loop. Pitch + proportional gate length per cell.
- **Branches** — chained transforms (transpose, reverse, rotate, …) concatenated *after* the trunk, so the playhead walks trunk → branch 1 → branch 2 → … building a longer phrase from one recording.
- **Ornaments** — probabilistic flourish notes (turns, runs, trills, …) snapped to the scale and injected *inside* eligible sections.

It is a sampler/looper, not a stochastic or algorithmic note source. Everything it plays traces back to what it captured.

## The three pages of state

- **Track-wide params** (sources, logic mix, buffer length, octave/transpose, quantize, delay, lock, ghost) live on the **track** — shared across all patterns.
- **Per-pattern params** (loop/record/orn zones, order mode, divisor, branch/ornament settings, the live scale) live on the **sequence** — one set per w-pattern.
- The **scale group** (scale / root / rotate) lives on the **sequence**, not the track.

---

# Part 2: Sources & Capture

## Source A and Source B

Each source slot (`SrcA`, `SrcB`) selects one of three kinds:

- **None** — slot empty.
- **Track** — a parent track 1..N. Gate and CV are taken natively from that track's output.
- **Channel** — a single routing channel. The ordered channel list after the tracks is: CvIn1-4, CvOut1-8, BusCv1-4, GateOut1-8, Mod1-8.

A channel derives a **gate** (a GateOut bit, or any other channel reading non-zero) and a **CV** (raw volts, 1V/oct; Mod 0..127 maps to ±5V).

> **Self-reference is forbidden.** A Track-kind slot pointing at the Fractal track's own index would be output feedback. Setting it directly snaps to None; turning the encoder past it skips over it in the travel direction.

## The two-source mix

With both slots set, the parents combine through two independent logic stages.

**Gate logic** (`GATE`) — 6 modes:

| Mode | Result |
|---|---|
| A | source A gate |
| B | source B gate |
| And | A and B |
| Or | A or B |
| Xor | A xor B |
| Nand | not (A and B) |

**CV logic** (`CV`) — 7 modes:

| Mode | Result |
|---|---|
| A | source A CV |
| B | source B CV |
| Min | min(A, B) |
| Max | max(A, B) |
| Sum | A + B |
| Avg | (A + B) / 2 |
| Gated (Gat) | whichever slot just fired, A-priority |

With only one slot set, that slot passes through alone (no mix). The classic "A=CV, B=Gate" tailoring is just `CV=A` + `GATE=B`.

## Record-muted (ghost source)

`Rec Muted` (track param) taps the parent's **pre-mute** intended gate. With it on, the Fractal track keeps capturing a source even while that source is muted — a silent rehearsal you record from without hearing the parent.

## Capture cadence and fidelity

Two per-sequence switches shape *how* a section is summarized:

| Param | Members | Default | Effect |
|---|---|---|---|
| Capture (cadence) | Section, Event | **Event** | Section commits one cell per loop section boundary; Event commits on the parent's note-change (hysteretic) |
| Fidelity | Quantized, Feel | **Feel** | Quantized holds the last CV sample and discards onset timing; Feel sample-and-holds CV at the first rising edge and keeps the onset phase within the section |

Gate length is always proportional: the count of gate-high ticks over the section's total ticks becomes a 4-bit gate length (0 = rest, 15 = tie). Feel additionally stores the onset nibble — *when* in the section the gate first fired — so playback reproduces the original feel.

## Record arm, Replace vs Latch, R.Skip

- **Record** (`recordTrigger`, per sequence) is the arm. Capture runs only when armed, the track is **not locked**, and a source is set. It is routable.
- **Rec Mode** — **Replace** (default) overwrites every section; **Latch** only writes sections that gated, so silent sections keep their prior content (overdub).
- **R.Skip** (`recordSkip`, 0-15) packs the capture: it skips that many sections between writes, and written cells pack consecutively into the buffer (so R.Skip 1 writes half as many cells over the same span).

Capture writes into the **record zone** (`recF..recL`) and the record cursor wraps within it.

---

# Part 3: The Trunk & Zones

## Three nested zones

The trunk buffer holds up to `bufferLength` cells (default 16, max 128). Three brackets carve it up, and they must nest:

```
recordFirst ≤ loopFirst ≤ ornFirst ≤ ornLast ≤ loopLast ≤ recordLast
```

- **Record zone** (rec) — outermost. Where capture writes.
- **Loop zone** (loop) — the window that actually replays.
- **Ornament zone** (orn) — innermost. Cells inside it are eligible for ornament flourishes.

Moving an outer edge pushes the inner edges it would cross; moving an inner edge clamps against its outer neighbour. The invariant is always enforced.

> **Buffer Length shrinks pull edges in.** Lowering `bufferLength` clamps every pattern's loop/record/orn edges down to fit the new last cell.

## The tape

The Trunk page draws the buffer as a horizontal **tape**. Each captured cell is a filled bar: **height = pitch** (±5 octaves mapped into the tape), **width = gate length** (0..15 of the cell width). Cells inside the loop window are brighter; the cell currently sounding is brightest. A vertical **playhead** marks the read position. Above the tape, three thin lines show the rec / loop / orn zones stacked top-to-bottom, the focused bracket drawn bright.

---

# Part 4: Branches

A branch is **one chained transform** of the trunk, derived deterministically from a seed. `branchCount` (0-7) branches play one after another, concatenated after the trunk, so the global walk is trunk → B1 → B2 → … Each branch reads the *previous* segment and applies its transform on read; the trunk buffer is never mutated.

## The 8 transforms

| # | Label | Op | Param |
|---|---|---|---|
| 0 | Tra | Transpose | ± a fixed interval: P4 (5), P5 (7), or octave (12) |
| 1 | Rev | Reverse | reads the segment backwards |
| 2 | Inv | Inverse | mirrors pitch around the loop-start cell |
| 3 | RtI | Retrograde Inverse | reverse + invert |
| 4 | Rot | Rotate | shifts read position by an offset (1..loopLen-1) |
| 5 | Cmp | Compress | scales intervals around centre by ×0.5 or ×0.67 |
| 6 | Exp | Expand | scales intervals around centre by ×1.5 or ×2.0 |
| 7 | Thn | Gate-thin | periodically rests every Nth cell (N = 2..4), CV untouched |

## Count, Path, Pool, Seed

- **CNT** (`branchCount`, 0-7, routable) — how many branches play.
- **PATH** (`path`, 0-255 bit-word, routable) — read order of the branches. Each path bit flips a branch between **outward** (bit 0, walked ascending after the trunk) and **held** (bit 1, walked descending after the outward ones). `routeOf` resolves the concrete order; the Branch page prints it as `T>B1>B2…`.
- **POOL** (`branchPool`, 8-bit mask) — which of the 8 transforms the seed may draw from. An empty pool falls back to all-Transpose.
- **SEED** (`branchSeed`) — the deterministic draw. The same seed + pool + loop length always yields the same chain. Pressing **F4 (SEED)** reseeds to a fresh random value.

## Jumping segments live

On the Branch page the top step row queues a **beat-synced jump** to a segment: the playback jump applies at the next section boundary. The currently playing block shows a bright border; a queued target shows a `Q`.

---

# Part 5: Ornaments

Ornaments inject short, scale-snapped flourish notes inside a section. They are **probabilistic and live** — rolled per eligible cell from a free-running PRNG. The trunk stays raw; ornament notes snap to the sequence scale on the way out.

## The 15 ornaments

| # | Name | Tier |
|---|---|---|
| 0 | Anticipation | 2-step |
| 1 | Suspension | 2-step |
| 2 | Syncopation | 2-step |
| 3 | Octave-Up | 2-step |
| 4 | Fifth-Up | 2-step |
| 5 | Half-Turn Toward | 2-step |
| 6 | Half-Turn Away | 2-step |
| 7 | Run Toward | 4-step |
| 8 | Run Away | 4-step |
| 9 | Turn | 4-step |
| 10 | Arp Toward | 4-step |
| 11 | Arp Away | 4-step |
| 12 | Mordent Up | 4-step |
| 13 | Mordent Down | 4-step |
| 14 | Trill | trill |

## Rate and Intensity

- **RATE** (`ornamentRate`, 0-100%, routable) — probability that an eligible cell fires an ornament at all. 0 = never.
- **INT** (`ornamentIntensity`, 0-100%, routable) — which tiers are in the draw pool:
  - 0-39%: the 7 **2-step** ornaments only.
  - ≥40%: add the 7 **4-step** ornaments.
  - ≥75%: add **Trill**.

The Ornament page labels the intensity slider with a tier readout (`off` / `2-step` / `4-step` / `8-trill`); the underlying eligibility uses the thresholds above.

## Lock

**LOCK** (track param) freezes the ornament roll. While locked, every firing repeats the **last-fired** ornament *and its realized shape* (direction + lookahead) instead of re-adapting per cell. Capture is also blocked while locked. Toggle it with **F4 (LOCK)** on the Ornament page, or **Shift+F1** on any hero page.

## Queued punch-in

The Ornament page step row queues a **forced ornament**: it ignores rate, zone, and lock, and fires on the next gated cell. A queued ornament reads `Q <name>`; the last fired reads `Last <name>`. Consecutive ornament notes leave a one-tick retrigger gap so each note re-articulates.

---

# Part 6: Playback

## Order mode

`orderMode` (per sequence) sets how the loop window is read each section:

| Mode | Reads |
|---|---|
| Fwd (Forward) | 0, 1, 2, … |
| Rev (Reverse) | last, …, 1, 0 |
| Pend (Pendulum) | forward then back |
| Rand (Random) | random within the window |
| Conv (Converge) | 0, last, 1, last-1, … inward |
| Div (Diverge) | centre outward |
| Page | 4-cell pages, interleaved |

Forward/Reverse/Pendulum/Random map to the shared sequence run-mode state; Converge/Diverge/Page advance linearly and remap the read index.

## Quantize

`Quantize` (track param) is a playback-only snap: **Raw** (default) plays the captured pitch literally; **On** snaps the main note to the sequence scale. Trunk storage stays raw either way — only the played note is shaped. Ornament flourishes always snap to their own scale regardless.

## Track delay

`Delay` (`trackDelay`, 0-16 sections) shifts the resolved main note that many sections later through a fixed ring — a canon/echo against the source. Only the note is delayed; ornaments are re-rolled fresh when the delayed entry surfaces.

Other per-sequence timing: **Divisor** (default 12), **Clock Mult** (50-150%, default 1.00x), **Reset Measure** (off / N bars, default off), **Rotate** (-64..64), **Loop Mode** (Loop / Once).

---

# Part 7: The Hero Pages

Page+S0 opens the **hero ring**: one page object cycling **Trunk → Branch → Ornament → Source** via **F5 (NEXT)**. **Shift+F1** toggles Lock on any of them. The context menu (long-press) offers **INIT** (clear sequence params), **CLEAR BUF** (erase the recorded loop), **COPY**, **PASTE**.

Holding **Page** + **S9-S16** opens a **quick-edit** overlay for, in order: Rec First, Rec Last, Loop First, Loop Last, Orn First, Orn Last, Orn Rate, Orn Intensity. **Page + any other step** exits the hero page.

## Trunk page

The tape, the zone lines, and a step keypad. F-keys focus the bracket editor; the encoder nudges the focused edge by ±1 (Shift = the bracket's *last* edge).

- **F1 REC** / **F2 LOOP** / **F3 ORN** — focus that bracket for the encoder.
- **F4 R.Skip** — focus R.Skip for the encoder.

Step keypad (top row S1-8 adds, bottom row S9-16 subtracts, edges by ±8):

| Col | S1-8 (top, +) | S9-16 (bottom, −) |
|---|---|---|
| 1 | Rec First +8 | Rec First −8 |
| 2 | Rec Last +8 | Rec Last −8 |
| 3 | Loop First +8 | Loop First −8 |
| 4 | Loop Last +8 | Loop Last −8 |
| 5 | Orn First +8 | Orn First −8 |
| 6 | Orn Last +8 | Orn Last −8 |
| 7 | R.Skip +1 | R.Skip −1 |
| 8 | Divisor (faster) | Divisor (slower) |

All edge moves are nesting-clamped. On the Divisor column **top = faster** (lower divisor value).

**LEDs:** top row dim green (+), bottom row dim red (−).

## Branch page

The route readout, the chain blocks (T + B1..BN), and the transform pool toggles.

- **F1 CNT** / **F2 PATH** / **F4 SEED** — focus for the encoder (SEED also reseeds on press).
- **F3 POOL** — select the pool layer; an **encoder click toggles the focused pool bit**.

Steps: **top row S1..S(N+1)** queues a beat-synced jump to that segment; **bottom row S9..S16** toggles the 8 transform-pool bits.

**LEDs:** top row — present segment dim green, playing bright amber, queued bright red. Bottom row — pool bit set = bright green.

## Ornament page

Rate and Intensity bars, scale + zone readout, last/queued ornament names.

- **F1 RATE** / **F2 INT** / **F3 SCALE** — focus for the encoder.
- **F4 LOCK** — toggle Lock.

Steps **S1-S15** queue a forced ornament (id 0-14), ignoring rate and zone. **S16 is inert.**

**LEDs:** S1-S15 — slot dim green, last-fired dim red, queued bright amber.

## Source page

Source A / B refs, the gate-logic row, the cv-logic row, and a result readout.

- **F1 SrcA** / **F2 SrcB** — focus the slot for the encoder.
- **F3 GATE** / **F4 CV** — focus the logic row for the encoder.

Steps: **top row S1-S6** selects the gate-logic mode; **bottom row S9-S15** selects the cv-logic mode; **S16 randomizes both** logics.

**LEDs:** selected mode bright green, other modes in the row dim green; S16 bright amber.

---

# Part 8: Setup & Routing

## Track-scope list (Page+S2)

Track-wide params, hosted by the generic Track page:

- Buffer Length (1-128, default 16)
- Octave (-10..+10)
- Transpose (-100..+100)
- Slide Time (0-100%)
- CV Update Mode (Gate / Always / Last)
- Quantize (Raw / On)
- Delay (0-16 sections)
- Lock (yes/no)
- Rec Muted (yes/no)

## Sequence-scope list (Page+S1)

Per-pattern params:

Loop First, Loop Last, Order Mode, Divisor, Clock Mult, Reset Measure, Rec First, Rec Last, Rec Mode, Rec Skip, Record (arm), Orn First, Orn Last, Orn Rate, Orn Intens, Capture (cadence), Fidelity, Scale, Root Note, Scale Rotate.

## What is routable

Only these Fractal params appear as routing targets — all per-sequence:

- `branchCount`
- `path`
- `ornamentRate`
- `ornamentIntensity`
- `recordTrigger` (record arm)

(Scale and Root Note also route through the shared scale targets.) No track-scope Fractal param is routable.

## The four live controls

The hero-page step grids give four hands-on, beat-synced performance controls without leaving the edit page:

1. **Branch jump** — queue a playback jump to any segment (Branch top row).
2. **Pool toggles** — flip transforms in/out of the branch draw (Branch bottom row).
3. **Ornament punch-in** — force any ornament on the next gated cell (Ornament S1-S15).
4. **Logic randomize** — roll both gate and cv logic at once (Source S16).
