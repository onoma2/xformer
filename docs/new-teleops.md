# New Teletype Ops from Live-Sequencer Reference

Operations from `temp-ref/live-sequencers/` that are genuinely absent from the
current teletype VM (425 ops) and cannot be expressed through existing
combinations. Source references point to the reference implementation across
13 surveyed languages: Tidal, Mercury, FoxDot, Cane, Ixi Lang, Alda, Melrose,
Subsequence, Topos, Gibber, Gibberwocky, TimeLines-hs, and Strudel.

---

## Signal Generation

### PERLIN — Smooth Perlin noise

One-shot Perlin noise value (0–16383). Like `CHAOS` but produces smoothly
interpolated pseudo-random output. Useful for natural-sounding modulation
(filter cutoff, pitch vibrato).

```
CV 1 V PERLIN
```

- **`temp-ref/live-sequencers/tidal/tidal-core/src/Sound/Tidal/UI.hs`** — `perlin` (194), `perlinWith` (178), `perlin2` (234), `perlin2With` (212)
- **`temp-ref/live-sequencers/subsequence/subsequence/sequence_utils.py`** — `perlin_1d` (382), `perlin_1d_sequence` (431), `perlin_2d` (498), `perlin_2d_grid` (535)

### NORMAL — Gaussian distribution

Box-Muller transform returning normally distributed values in 0–16383
(clamped to ±3σ). Unlike `RAND`/`RRAND` which are uniform. Useful for
natural pitch distributions, velocity curves.

```
CV 1 V NORMAL
```

- **`temp-ref/live-sequencers/tidal/tidal-core/src/Sound/Tidal/UI.hs`** — `normal` (248-255), uses Box-Muller on two uniform randoms

### PINK — 1/f pink noise (Voss-McCartney)

Energy equal per octave: slow drift + fast jitter in one signal. Matches
natural parameter variation statistics. Unlike `RAND` (white, uniform) or
`PERLIN` (smooth interpolation), pink noise has a specific spectral slope.

```
CV 1 V PINK
```

- **`temp-ref/live-sequencers/subsequence/subsequence/sequence_utils.py`** — `pink_noise` (630), Voss-McCartney algorithm with configurable source count

### WALK — Bounded random walk (brownian CV)

Standalone random walk with step size, bounds, and wrap control
(functionally similar to `P.WALK` but as a continuous CV source).

```
CV 1 V WALK 100 8192    # step 100, start at 8192
```

- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Patterns/Generators.py`** — `PWalk` (256), random walk with `max`, `step`, `start`
- **`temp-ref/live-sequencers/subsequence/subsequence/sequence_utils.py`** — `random_walk` (260), bounded ±step
- **`temp-ref/live-sequencers/topos/src/API.ts`** — `drunk()` (978), bounded walk with configurable min/max/wrap

### LFO→Script readback — Read LFO output into variables

LFO currently drives CV only and can't be read into script variables.
An op like `LFO.V n` → var would allow LFO to modulate pattern params,
gate probability, slew rates, etc.

```
X LFO.V 1    # read LFO1 into X
CV 2 N ADD 60 X   # use it as transposition
```

- No equivalent in any surveyed language (all treat LFOs as opaque signal paths)

---

## Algorithmic Generators (new category)

### P.LSYS — L-system string rewriting

Self-similar fractal sequences from rewrite rules. E.g. Fibonacci word:
axiom `A`, rule `A→AB, B→A`. A 3-character rule produces hundreds of notes
with hierarchical structure.

```
P.LSYS A   # A→AB, B→A  (Fibonacci word)
P.LSYS B   # A→B, B→AB  (variant)
```

- **`temp-ref/live-sequencers/subsequence/subsequence/pattern_algorithmic.py`** — `lsystem()` (790), supports deterministic and stochastic (weighted) rules

### P.THUE — Thue-Morse aperiodic sequence

Binary parity sequence: popcount(n) mod 2. Never periodic. Produces balanced
rhythms that avoid repetition at any scale.

```
P.THUE 16    # 16 steps: 0,1,1,0,1,0,0,1,...
```

- **`temp-ref/live-sequencers/subsequence/subsequence/pattern_algorithmic.py`** — `thue_morse()` (892), optionally maps to two pitches

### P.CA1D — 1D cellular automaton

Wolfram rule-based binary sequence. Rule 30 = structured chaos, Rule 90 =
fractal, Rule 110 = Turing-complete. Evolves per cycle.

```
P.CA1D 30 16   # Wolfram rule 30, 16 steps
```

- **`temp-ref/live-sequencers/subsequence/subsequence/pattern_algorithmic.py`** — `cellular_1d()` (503), rules 0-255

### P.CA2D — 2D Life-like cellular automaton

Rows = voices, columns = time steps. Birth/Survival notation
(`B3/S23` for Conway, `B368/S245` for Morley).

```
P.CA2D B3/S23 8 8   # Conway's Life, 8x8 grid
```

- **`temp-ref/live-sequencers/subsequence/subsequence/pattern_algorithmic.py`** — `cellular_2d()` (562)

### P.MARKOV — Markov chain generator

First-order Markov: state-to-state transition weights mapped to values.
Generates sequences with stylistic coherence.

```
P.MARKOV (0 60 0.7)(1 64 0.3)(0 67 0.5)(2 72 0.5)
```

- **`temp-ref/live-sequencers/subsequence/subsequence/pattern_algorithmic.py`** — `markov()` (642)
- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Patterns/Generators.py`** — `PChain` (169), Markov chain with dict mapping of `state: [next_states]`

### P.LORENZ — Strange attractor mapping

Lorenz attractor → pitch/velocity/duration triplets. Extremely sensitive to
initial conditions. One seed change produces divergent trajectory.

```
P.LORENZ 10 28 2.667   # sigma, rho, beta (classic params)
```

- **`temp-ref/live-sequencers/subsequence/subsequence/pattern_algorithmic.py`** — `lorenz()` (1066), maps xyz→pitch/velocity/duration

### P.DBRU — De Bruijn sequence

Generates all possible N-gram value subsequences exactly once before
repeating. Exhaustive melodic exploration.

```
P.DBRU 2   # window=2: every 2-step transition appears exactly once
```

- **`temp-ref/live-sequencers/subsequence/subsequence/pattern_algorithmic.py`** — `de_bruijn()` (958)

### P.REAC — Reaction-diffusion (Gray-Scott)

1D PDE simulation → binary rhythm via concentration thresholding. Produces
spots, stripes, traveling waves — organic, biological pattern character.

```
P.REAC 0.037 0.06   # feed rate, kill rate
```

- **`temp-ref/live-sequencers/subsequence/subsequence/pattern_algorithmic.py`** — `reaction_diffusion()` (1154)

### P.FIB — Fibonacci/golden angle spacing

Place N events using the golden angle method: `(i * phi) mod bar_length`.
Irrational, off-grid, organic-feeling timing.

```
P.FIB 8 16   # 8 events distributed across 16 steps
```

- **`temp-ref/live-sequencers/subsequence/subsequence/pattern_algorithmic.py`** — `fibonacci()` (1025)

### FIB — Fibonacci number sequence

Standalone Fibonacci numbers as CV source.

```
CV 1 V FIB 10   # 0,1,1,2,3,5,8,13,21,34
```

- **`temp-ref/live-sequencers/mercury/grammar/totalSerialismIR.js`** — `fibonacci` (81), `pell` (90), `lucas` (94), `threeFibonacci` (98)
- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Patterns/Generators.py`** — `PFibMod` (313), generators returning fibonacci mod

### BIN — Binary pattern from integer

Convert any integer to binary and cycle through bits as boolean triggers.
`bin(13)` → binary `1101` → 4-step gate pattern `[1,1,0,1]`.

```
BIN 13 16    # binary 1101 cycled across 16 steps
```

- **`temp-ref/live-sequencers/topos/src/API.ts`** — `bin(iterator, n)` (1642)

---

## Rhythm Logic (new category)

### CANE.TR.AND / CANE.TR.OR / CANE.TR.XOR / CANE.TR.NOT

Bitwise rhythm combinators: combine two trigger patterns with boolean logic.
Similar to the Gate/trigger pattern combination section below but operates
on pattern arrays, not live trigger inputs.

```
P.TR.AND P1 P2     # gate pattern = P1 AND P2 (stepwise)
P.TR.OR P1 P2      # gate pattern = P1 OR P2
P.TR.XOR P1 P2     # gate pattern = P1 XOR P2
P.TR.NOT P1        # gate pattern = NOT P1 (invert)
```

- **`temp-ref/live-sequencers/cane/doc/ideas.md`** — rhythm bitwise combinators (71), rhythm invert `~` (66)

### P.BRES — Bresenham line rhythm

Alternative to Euclidean rhythm using Bresenham's line algorithm. Slightly
different even spacing. Poly variant supports weighted interlocking voices:
one voice per step, density controlled per voice.

```
P.BRES 3 8     # 3 pulses, 8 steps (Bresenham variant)
P.BRES.POLY 64 30 20 10   # 3 voices with weights, 64 steps
```

- **`temp-ref/live-sequencers/subsequence/subsequence/pattern_algorithmic.py`** — `bresenham()` (118), `bresenham_poly()` (145)

### P.GHOST — Rhythm-aware probability fill

Fill steps with probability weighted by rhythmic position bias. 8 named bias
shapes: `uniform`, `offbeat`, `sixteenths`, `before`, `after`, `downbeat`,
`upbeat`, `e_and_a`. Bias curve is a mutable float list — surgically editable.

```
P.GHOST 0.3 offbeat   # 30% fill, bias toward offbeat positions
```

- **`temp-ref/live-sequencers/subsequence/subsequence/pattern_algorithmic.py`** — `ghost_fill()` (382), `build_ghost_bias()` (277)

### P.THIN — Rhythm-aware note removal

Remove notes based on rhythmic position. Same vocabulary as P.GHOST but
inverted: removes notes rather than placing them.

```
P.THIN sixteenths 0.2   # remove 20% of notes at 16th-note positions
```

- **`temp-ref/live-sequencers/subsequence/subsequence/pattern_algorithmic.py`** — `thin()` (1297)

### LCM / GCD — Polyrhythm math helpers

Lowest common multiple and greatest common divisor. Useful for aligning
polyrhythmic sequences.

```
LCM 3 4    # 12
GCD 6 10   # 2
```

- **`temp-ref/live-sequencers/cane/doc/ideas.md`** — `lcm`, `gcd` (49)
- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Utils/__init__.py`** — `LCM()` (48)

---

## Pattern Transforms (P.* namespace)

### P.PAL — Palindrome (alternate forward/reverse)

Alternates pattern forward/reverse each cycle. Like `P.REV` but
auto-toggles. Triple-confirmed across Tidal, Mercury, and FoxDot.

```
P.PAL
```

- **`temp-ref/live-sequencers/tidal/tidal-core/src/Sound/Tidal/UI.hs`** — `palindrome` (572): `palindrome p = slowAppend p (rev p)`
- **`temp-ref/live-sequencers/mercury/grammar/totalSerialismIR.js`** — `palindrome` (246), `palin` (249), `mirror` (252) → `Mod.palindrome`
- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Patterns/Main.py`** — `.palindrome()` (734), supports trim parameters

### P.WTH — Sub-range transform

Apply a transform (rev, shuffle, add, etc.) to only a portion of the
pattern defined by start/end indices, leaving the rest untouched.

```
P.WTH 2 6 REV      # reverse only steps 2-6
P.WTH 0 3 SHUF    # reshuffle first 4 steps
```

- **`temp-ref/live-sequencers/tidal/tidal-core/src/Sound/Tidal/UI.hs`** — `within` (807-812), `within'` (838-843), `withinArc` (814)

### P.STR — Stretch/resample to length

Resample the pattern to a target length with interpolation.

```
P.STR 16      # stretch active pattern to 16 steps
```

- **`temp-ref/live-sequencers/mercury/grammar/totalSerialismIR.js`** — `stretch` (284), `stretchFloat` (290) → `Mod.stretch`
- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Patterns/Main.py`** — `.stretch(size)` (659), repeats content until length equals size

### P.URN — Sample without replacement

Pick N distinct values from the pattern. Like `P.RND` but guarantees no
repeats until the pool is exhausted.

```
P.URN 4       # fill 4 steps with random non-repeating values from pattern
```

- **`temp-ref/live-sequencers/mercury/grammar/totalSerialismIR.js`** — `urn` (137) → `Rand.urn`
- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Patterns/Main.py`** — `.sample(n)` (729), random N without replacement

### P.INV — Invert/mirror around center

Mirror all values around a center point. E.g. center=8192: 0→16383,
16383→0, 4096→12288.

```
P.INV 8192    # invert around midpoint
```

- **`temp-ref/live-sequencers/mercury/grammar/totalSerialismIR.js`** — `invert` (208), `inverse` (211), `flip` (214), `inv` (221) → `Mod.invert`

### P.WALK — Random walk fill

Fill pattern values using a brownian/random walk starting from a seed
value. Step size adjustable.

```
P.WALK 4096 0   # start at 0, max step change 4096
```

- **`temp-ref/live-sequencers/mercury/grammar/totalSerialismIR.js`** — `drunk` (127), `drunkF` (130), `drunkFloat` (133) → `Rand.drunk`
- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Patterns/Generators.py`** — `PWalk` (256), bounded walk with configurable step
- Note: teletype has `DRUNK` as a single global var; this is a pattern-fill variant

### P.SAWALK — Self-avoiding walk fill

Guarantees no pitch/parameter repeats within a phrase. When all neighbours
are visited, visited set resets → natural phrase boundaries.

```
P.SAWALK 0 127   # traverse range 0-127 without immediate repeats
```

- **`temp-ref/live-sequencers/subsequence/subsequence/pattern_algorithmic.py`** — `self_avoiding_walk()` (1234)

### P.COMP — Compress/select mask

Keep values only where a mask pattern is 1. Non-destructive: output length
matches original, masked positions are 0.

```
P.COMP 1 0 1 0   # keep only positions 1 and 3
```

- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Patterns/Main.py`** — `.compress(selector)` (823), `.select(selector)` (830)

### P.UNDUP — Remove consecutive duplicates

Filters out repeated consecutive values. Useful for preventing the same CV
value on successive steps.

```
P.UNDUP   # 0 0 5 5 5 3 3 → 0 5 3
```

- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Patterns/Main.py`** — `.undup()` (768)

### P.ACCUM — Cumulative sum

Running total: `[v0, v0+v1, v0+v1+v2, ...]`. Builds envelope shapes and
integrator effects.

```
P.ACCUM   # 100 200 50 → 100 300 350
```

- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Patterns/Main.py`** — `.accum()` (648)

### P.STUTT — Ratchet (stutter)

Repeat each value N times. N can be a pattern for variable ratchets.

```
P.STUTT 3    # 1 0 1 → 1 1 1 0 0 0 1 1 1
P.STUTT 2 3 1   # variable ratchet per step
```

- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Patterns/Main.py`** — `.stutter(n)` (562), supports per-element pattern for `n`
- **`temp-ref/live-sequencers/gibber/playground/examples/pattern.js`** — `repeat(value, count)` (ratchet via phase-freeze), also in `gibberwocky.midi/js/pattern.js` (209)

### P.OFF — Delayed copy with transform

Create a delayed, transformed copy of the pattern and combine (add/mul).
Ghost notes, canons, delayed transpositions.

```
P.OFF 7 0.5    # add 7 to a 50%-delayed copy of values
P.OFF.M 1.5 0.25   # multiply by 1.5 at 25% delay
```

- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Patterns/PGroups.py`** — `.offadd(value, dur)` (190), `.offmul(value, dur)` (194), `.offlayer(method, dur)` (199)

### Wave-fill pattern gen — P.SINE / P.COS / P.SQR

Fill a pattern with N periods of a wave function. Takes length, periods,
amplitude range.

```
P.SINE 16 1 0 16383   # 16 steps, 1 period, full range
```

- **`temp-ref/live-sequencers/mercury/grammar/totalSerialismIR.js`** — `sine` (43), `cosine` (53)
- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Patterns/Sequences.py`** — `PSine(n=16)` (246), `PTri` (237), one cycle of sine/triangle

---

## Timing / Phase (new category)

### FLIP — A/B time-split

True for the first ratio% of each time chunk, false for the remainder.
Creates alternating A/B pattern sections in a single expression.

```
EVERY 4: IF FLIP 50: TR.P 1    # first half: TR1, second half: rest
```

- **`temp-ref/live-sequencers/topos/src/API.ts`** — `flip(chunk, ratio)` (1438)

### ODDS — Time-windowed probability

Probability normalized by a time window. Unlike `PROB` (per-event), ODDS
adjusts for clock speed so the effective rate stays constant at any PPQN.

```
ODDS 0.5 1: TR.P 1    # 50% probability per beat
```

- **`temp-ref/live-sequencers/topos/src/API.ts`** — `odds(n, beats)` (1144)

### INTOX — Timing jitter (beer/hash/coffee/LSD/detox)

Named jitter operations with specific time-distortion semantics:

- **BEER**: Adds per-step duration jitter then renormalizes to keep total loop length constant. "Drunk" timing with preserved meter.
- **HASH**: Random positive timing offset. Misaligns without renormalization.
- **COFFEE**: Negative timing offset (tightens). Counterpart to HASH.
- **LSD**: Positive-only duration expansion. Last step absorbs remainder.
- **DETOX**: Reset timing to original.

```
EVERY 4: INTOX BEER 5   # 4th cycle: wobble timing
```

- **`temp-ref/live-sequencers/ixilang/source/XiiLang.sc`** — `beer` (2951), `hash` (2962), `coffee` (2967), `LSD` (2971), `detox` (2984)

### FUTURE — Scheduled deferred operations

Schedule any command to execute N bars from now, repeated M times.
Arguments cycle through a wrap list.

```
FUTURE 4:2 SCRIPT 1   # run script 1 every 4 bars, twice
```

- **`temp-ref/live-sequencers/ixilang/source/XiiLang.sc`** — `future` (465-589), supports bar and second timing

---

## CV Mapping (new category)

### LINLIN / LINEXP / EXPLIN / EXPEXP / LINCURVE

SuperCollider-style mapping curves for CV value transformation. Full family
of linear/exponential bidirectional mapping.

```
CV 1 N SCALE 0 127 LINLIN 10 100   # map IN 0-127 to CV 10-100 linearly
CV 2 N SCALE 0 127 LINEXP 10 100   # map IN 0-127 to CV 10-100 exponentially
```

- **`temp-ref/live-sequencers/topos/src/NumberExtensions.ts`** — `linlin` (42), `linexp` (76), `explin` (48), `expexp` (54), `lincurve` (60)

### SMOOTHSTEP — Cubic smooth interpolation

Hermite/Cubic smooth interpolation: input in [0,1] → output in [0,1] with
smooth derivatives at endpoints. Smoother than linear for envelope shaping.

```
CV 1 V SMOOTHSTEP 0.5   # 0.5 at center: returns exactly 0.5
```

- **`temp-ref/live-sequencers/TimeLines-hs/src/Sound/TimeLines/Instruments.hs`** — `smoothstep` (216-218), cubic Hermite interpolation

---

## Control Flow

### CHOOSE / WCHOOSE — Weighted selection

Pick one value from a list with equal or weighted probability. `CHOOSE`
returns one of N args. `WCHOOSE` takes (value, weight) pairs.

```
CHOOSE 60 64 67 72          # equal weight, pick one
WCHOOSE 60 50 64 30 67 20   # C4 50%, E4 30%, G4 20%
```

- **`temp-ref/live-sequencers/tidal/tidal-core/src/Sound/Tidal/UI.hs`** — `choose` (268), `chooseBy` (283), `wchoose` (297), `wchooseBy` (303)
- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Patterns/Generators.py`** — `PwRand` (145), weighted random choice with explicit weight lists
- **`temp-ref/live-sequencers/cane/doc/ideas.md`** — `{a b c}` (53), random choice from alternatives as curly-brace syntax

### SUPERIMPOSE — Layer original + transformed

Apply a transform and sum with the original. In teletype, this means
blending two CV values (add/sub) on a single output rather than write-last.

```
SUPERIMPOSE 2 REV   # output = CV + REV(CV) on CV 2
```

- **`temp-ref/live-sequencers/tidal/tidal-core/src/Sound/Tidal/UI.hs`** — `superimpose` (740): `superimpose f p = stack [p, f p]`

### CONDAPPLY — Conditional transform via crossfade

Conditionally apply a function via smooth crossfade between original and
transformed value. More graceful than `IF` which is a hard switch.

```
CONDAPPLY 0.3 REV CV 1   # 30% crossfade to reversed CV1
```

- **`temp-ref/live-sequencers/TimeLines-hs/src/Sound/TimeLines/Instruments.hs`** — `condApply` (79), uses `lerp(val, f(val), when)`

### MAP — Higher-order pattern transform

Apply any unary function to each pattern element. With `_` as placeholder.

```
MAP INV CV 1   # invert each value of CV1
MAP SHUF CV 2   # shuffle values of CV2 per cycle
```

- **`temp-ref/live-sequencers/melrose/api/op/map.go`** — `map()` (1284), applies transform over each element

---

## Meta-Sequencing (new category)

### MERGE — Polyphonic interleave by duration

Merge parallel sequences into one timeline, aligning by shortest duration at
each step. This is the key to interleaving multiple CV/gate streams into a
single output without explicit timing math.

```
MERGE CV 1 CV 2   # interleave CV1 and CV2 into single output
```

- **`temp-ref/live-sequencers/melrose/api/op/merge.go`** — `merge()` (31), duration-based interleaving with rest gaps

### RESEQUENCE — Index-based reorder with grouping

Reorder pattern values by arbitrary 1-based index pattern. Supports group
syntax (parentheses) for chords/multi-value slots.

```
RESEQUENCE (6 5) 4 3 (2 1)   # group 6-5 as pair, then 4,3, group 2-1
```

- **`temp-ref/live-sequencers/melrose/api/op/resequence.go`** — `resequence()` (17)

### EVERY.N — Periodic mutation

Apply a transform every N cycles. Unlike `EVERY` which gates a single
command, EVERY.N applies a transform to the active pattern on the Nth
cycle.

```
EVERY 4 P.INV 8192   # invert pattern every 4th cycle
```

- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Patterns/Main.py`** — `.every(n, method, *args)` (853)
- **`temp-ref/live-sequencers/FoxDot/FoxDot/lib/Players.py`** — Player-level `.every(n, "method")` (907)

---

## Logic

### Gate/trigger pattern combination

Combine trigger inputs with AND/OR/XOR before routing to a script or
output. Currently each TI maps 1:1 to a script. A `TR.COMBINE` op or
routing-level gate combinator would enable complex trigger logic.

```
TR.COMBINE AND 1 2    # fire when TI1 AND TI2 high
TR.COMBINE NOT 1      # fire when TI1 low
```

- **`temp-ref/live-sequencers/TimeLines-hs/src/Sound/TimeLines/Instruments.hs`** — `andGate` (144), `orGate` (150), logic on continuous CV as signals
- **`temp-ref/live-sequencers/cane/doc/ideas.md`** — rhythm bitwise combinators `and`/`or`/`xor` (71)
- No direct Tidal equivalent (tidal operates on event streams, not gate logic)
